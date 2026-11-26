#include "audio_wizard.h"
#include <QtWidgets>
#include <QJsonArray>
#include <cmath>
#include <memory>
#include <vector>

// Mirror only the wizard's named controls. The shack keeps the actual ALSA
// processes and mixer transaction; the client never opens a local sound device.
QJsonObject AudioWizard::relayState() const{
    QJsonObject controls;
    for(auto* widget:findChildren<QWidget*>()){
        const auto name=widget->objectName();if(!name.startsWith("wizard"))continue;
        QJsonObject state{{"enabled",widget->isEnabled()},{"hidden",widget->isHidden()}};
        if(auto* combo=qobject_cast<QComboBox*>(widget)){
            QJsonArray items;int selected=-1;
            for(int n=0;n<combo->count();++n){if(n>=64&&n!=combo->currentIndex())continue;if(n==combo->currentIndex())selected=int(items.size());items.append(QJsonObject{{"text",combo->itemText(n)},{"data",QJsonValue::fromVariant(combo->itemData(n))},{"index",n}});}
            state["type"]="combo";state["items"]=items;state["index"]=selected;state["text"]=combo->currentText();
        }else if(auto* box=qobject_cast<QCheckBox*>(widget)){state["type"]="check";state["checked"]=box->isChecked();}
        else if(auto* spin=qobject_cast<QSpinBox*>(widget)){state["type"]="spin";state["value"]=spin->value();}
        else if(auto* progress=qobject_cast<QProgressBar*>(widget)){state["type"]="progress";state["value"]=progress->value();}
        else if(auto* label=qobject_cast<QLabel*>(widget)){state["type"]="label";state["text"]=label->text().left(3000);}
        else if(auto* button=qobject_cast<QPushButton*>(widget)){state["type"]="button";state["text"]=button->text();}
        else continue;
        controls[name]=state;
    }
    QJsonObject buttons;for(auto id:{BackButton,NextButton,FinishButton,CancelButton})buttons[QString::number(int(id))]=button(id)->isEnabled();
    return {{"page",std::max(0,currentId())},{"controls",controls},{"buttons",buttons}};
}
void AudioWizard::applyRelayState(const QJsonObject& state){
    if(!relay_)return;
    relayApplying_=true;
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for(auto* widget:findChildren<QWidget*>())if(widget->objectName().startsWith("wizard"))blockers.push_back(std::make_unique<QSignalBlocker>(widget));
    if(currentId()<0)restart();
    const int target=std::clamp(state.value("page").toInt(),0,3);
    for(int n=0;n<4&&currentId()!=target;++n){if(currentId()<target)next();else back();}
    const auto controls=state.value("controls").toObject();
    for(auto it=controls.begin();it!=controls.end();++it){
        const auto value=it.value().toObject();auto* widget=findChild<QWidget*>(it.key());if(!widget)continue;
        if(auto* combo=qobject_cast<QComboBox*>(widget)){
            combo->clear();for(const auto& entry:value.value("items").toArray()){const auto item=entry.toObject();combo->addItem(item.value("text").toString(),item.value("data").toVariant());combo->setItemData(combo->count()-1,item.value("index").toInt(),Qt::UserRole+1);}
            combo->setCurrentIndex(value.value("index").toInt(-1));if(combo->isEditable())combo->setEditText(value.value("text").toString());
        }else if(auto* box=qobject_cast<QCheckBox*>(widget))box->setChecked(value.value("checked").toBool());
        else if(auto* spin=qobject_cast<QSpinBox*>(widget))spin->setValue(value.value("value").toInt());
    }
    updateGuidance();
    auto* advanced=findChild<QWidget*>("wizardAdvancedDevices");advanced->setVisible(advanced_->isChecked());
    for(auto it=controls.begin();it!=controls.end();++it){
        const auto value=it.value().toObject();auto* widget=findChild<QWidget*>(it.key());if(!widget)continue;
        widget->setEnabled(value.value("enabled").toBool());widget->setVisible(!value.value("hidden").toBool());
        if(auto* progress=qobject_cast<QProgressBar*>(widget))progress->setValue(value.value("value").toInt());
        else if(auto* label=qobject_cast<QLabel*>(widget))label->setText(value.value("text").toString());
        else if(auto* push=qobject_cast<QPushButton*>(widget))push->setText(value.value("text").toString());
    }
    const auto buttons=state.value("buttons").toObject();for(auto id:{BackButton,NextButton,FinishButton,CancelButton})button(id)->setEnabled(buttons.value(QString::number(int(id))).toBool());
    relayApplying_=false;
}
QString AudioWizard::relayOperation(const QString& operation,const QJsonObject& args){
    if(relay_)return "Audio actions must run on the shack PC.";
    if(operation=="state")return {};
    if(operation=="stop"){stopChecks();return {};}
    if(operation=="step"){
        if(optimizationPending_)return "Wait for the audio level adjustment to finish.";
        const auto n=args.value("page").toDouble(-1);if(!std::isfinite(n)||n<0||n>3||n!=int(n))return "Invalid wizard page.";
        for(int k=0;k<4&&currentId()!=int(n);++k){if(currentId()<int(n))next();else back();}return {};
    }
    if(operation=="edit"){
        const auto name=args.value("name").toString();if(!name.startsWith("wizard"))return "Invalid wizard control.";
        const auto value=args.value("value");
        if(auto* combo=findChild<QComboBox*>(name)){
            if(!combo->isEnabled())return "Wait for the current audio operation to finish.";
            if(combo->isEditable()){if(!value.isString()||value.toString().size()>500)return "Invalid audio device name.";const int index=combo->findData(value.toString());const int textIndex=combo->findText(value.toString());if(index>=0)combo->setCurrentIndex(index);else if(textIndex>=0)combo->setCurrentIndex(textIndex);else combo->setCurrentText(value.toString());}
            else {const auto n=value.toDouble(-1);if(!value.isDouble()||!std::isfinite(n)||n<0||n>=combo->count()||n!=int(n))return "Invalid audio device choice.";combo->setCurrentIndex(int(n));}return {};
        }
        if(auto* box=findChild<QCheckBox*>(name)){if(!value.isBool())return "Invalid audio option.";box->setChecked(value.toBool());return {};}
        if(auto* spin=findChild<QSpinBox*>(name)){if(!spin->isEnabled())return "Wait for the current audio operation to finish.";const auto n=value.toDouble(-1);if(!value.isDouble()||!std::isfinite(n)||n<spin->minimum()||n>spin->maximum()||n!=int(n))return "Invalid audio level.";spin->setValue(int(n));return {};}
        return "Unknown wizard control.";
    }
    QPushButton* action=operation=="listen"?listen_:operation=="tone"?tone_:operation=="optimize"?optimize_:operation=="refresh"?refresh_:nullptr;
    if(!action||!action->isEnabled())return "Wait for the current audio operation to finish.";
    action->click();return {};
}
