#include "mainwindow.h"
#include "audio_levels_panel.h"
#include "radio_frontend.h"
#include "radio_ui.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QScopeGuard>
#include <algorithm>

namespace {
constexpr auto StateKey="radioProfiles/state";
void writeActiveSettings(QSettings& s,const profiles::Settings& p){
    p.radio.save(s);s.setValue("audio/input",p.input);s.setValue("audio/output",p.output);s.setValue("audio/setupConnection",p.connection);
    s.setValue("audio/txLevelPercent",p.txLevel);s.setValue("audio/rxGainDb",p.rxGain);s.setValue("audio/autoRxLevel",p.autoRx);
    s.setValue("rx/autoClock",p.autoClock);s.setValue("rx/autoFrequency",p.autoFrequency);s.setValue("rx/clock",p.clockPpm);s.setValue("rx/offset",p.offsetHz);
}
}
profiles::Settings MainWindow::currentProfileSettings() const{
    profiles::Settings p;p.radio=radioConfig_;p.input=inputDevice_->text().trimmed();p.output=outputDevice_->text().trimmed();if(p.input.isEmpty())p.input="default";if(p.output.isEmpty())p.output="default";
    QSettings s(profileSettingsFile_,profileSettingsFormat_);p.connection=s.value("audio/setupConnection","other").toString();if(!QStringList{"other","usb","signalink","radio-usb","flex-dax"}.contains(p.connection))p.connection="other";
    p.txLevel=remoteClientMode()?std::clamp(s.value("audio/txLevelPercent",100).toInt(),0,100):audioLevels_->transmitPercent();p.rxGain=audioLevels_->receiveGainDb();p.autoRx=audioLevels_->automaticReceiveLevel();
    p.autoClock=autoClock_->isChecked();p.autoFrequency=autoFrequency_->isChecked();p.clockPpm=clock_->value();p.offsetHz=offset_->value();return p;
}
QString MainWindow::persistProfiles(bool writeActive){
    if(!profilesReady_||!profiles_.current())return {};
    QSettings s(profileSettingsFile_,profileSettingsFormat_);s.setValue(StateKey,QJsonDocument(profiles_.toJson()).toJson(QJsonDocument::Compact));if(writeActive)writeActiveSettings(s,profiles_.current()->settings);s.sync();
    profileSaveFailed_=s.status()!=QSettings::NoError;
    return profileSaveFailed_?QString("Radio profiles could not be saved. Check access to the settings file."):QString{};
}
void MainWindow::saveActiveProfile(){
    if(!profilesReady_||profileApplying_)return;
    profileSaveTimer_.stop();
    if(profiles_.updateCurrent(currentProfileSettings())||profileSaveFailed_){const auto error=persistProfiles(true);if(!error.isEmpty())status_->setText(error);publishRemoteState();}
}
QJsonObject MainWindow::displayedProfiles() const{
    if(remoteClientMode())return remoteClient_&&remoteClient_->connected()?remoteClient_->snapshot().value("profiles").toObject():QJsonObject{};
    return profilesReady_?profiles_.summary():QJsonObject{};
}
void MainWindow::setupProfiles(){
    QSettings settings;profileSettingsFile_=settings.fileName();profileSettingsFormat_=settings.format();const auto raw=settings.value(StateKey).toByteArray();QString error;
    if(raw.isEmpty())profiles_=profiles::Store::initial(currentProfileSettings());
    else if(raw.size()>256*1024||!profiles::Store::fromJson(QJsonDocument::fromJson(raw).object(),profiles_,error)){
        settings.setValue("radioProfiles/recovery/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz"),raw);
        profiles_=profiles::Store::initial(currentProfileSettings());status_->setText("Saved profile data was damaged. Current setup preserved; a recovery copy was kept.");
    }else profiles_.updateCurrent(currentProfileSettings());
    profilesReady_=true;const auto persistence=persistProfiles();if(!persistence.isEmpty())status_->setText(persistence);profileSaveTimer_.setSingleShot(true);profileSaveTimer_.setInterval(500);connect(&profileSaveTimer_,&QTimer::timeout,this,&MainWindow::saveActiveProfile);
    const auto schedule=[this]{if(profilesReady_&&!profileApplying_)profileSaveTimer_.start();};
    for(auto* field:{inputDevice_,outputDevice_})connect(field,&QLineEdit::editingFinished,this,schedule);
    for(auto* field:{clock_,offset_})connect(field,&QDoubleSpinBox::valueChanged,this,[schedule](double){schedule();});
    for(auto* field:{autoClock_,autoFrequency_})connect(field,&QCheckBox::toggled,this,[schedule](bool){schedule();});
    connect(audioLevels_,&AudioLevelsPanel::levelsChanged,this,schedule);
    auto* bar=findChild<QWidget*>("radioStationBar");auto* row=bar?qobject_cast<QHBoxLayout*>(bar->layout()):nullptr;
    if(row){
        profileChoice_=new QComboBox;profileChoice_->setObjectName("radioProfileChoice");profileChoice_->setAccessibleName("Active radio profile");profileChoice_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);profileChoice_->setMinimumContentsLength(12);profileChoice_->setMaximumWidth(200);profileChoice_->setToolTip("Choose the radio's saved audio and CAT/PTT settings. Switching closes CAT; connect explicitly afterward.");
        profilesButton_=new QPushButton("Profiles…");profilesButton_->setObjectName("radioProfilesButton");row->insertWidget(1,profileChoice_);row->insertWidget(2,profilesButton_);
        remoteStatus_->setWordWrap(false);remoteStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        connect(profilesButton_,&QPushButton::clicked,this,&MainWindow::showProfiles);
        connect(profileChoice_,qOverload<int>(&QComboBox::activated),this,[this](int){const auto summary=displayedProfiles();const auto target=profileChoice_->currentData().toString();if(target==summary.value("active").toString())return;const auto done=[this](QString error){if(!error.isEmpty())status_->setText(error);updateProfileUi();};if(remoteClientMode())remoteClient_->command("profiles.change",{{"operation","switch"},{"id",target},{"expected",summary.value("revision")}},done);else changeProfile("switch",target,{},summary.value("revision").toString(),done);});
    }
    if(auto* menu=menuBar()->findChild<QMenu*>("radioMenu")){auto* action=menu->addAction("Radio profiles…",this,&MainWindow::showProfiles);action->setObjectName("radioProfilesAction");}
    connect(remoteClient_,&remote::Client::snapshotChanged,this,&MainWindow::updateProfileUi);connect(remoteClient_,&remote::Client::changed,this,&MainWindow::updateProfileUi);
    updateProfileUi();publishRemoteState();
}
QString MainWindow::profileOperationError() const{
    if(profileApplying_)return "A radio profile change is still in progress.";
    if(micCheckActive_)return "Close Mic check before changing radio profiles.";
    const auto busy=radioSetupError();if(!busy.isEmpty())return "Stop transmission, recording or setup before changing radio profiles.";
    if(auto* modal=QApplication::activeModalWidget();modal&&isAncestorOf(modal)&&modal->objectName()!="radioProfileDialog")return "Close the current setup dialog before changing radio profiles.";
    if(setupWizard_&&setupWizard_->isVisible())return "Close Setup wizard before changing radio profiles.";
    return {};
}
void MainWindow::applyProfileSettings(const profiles::Settings& p){
    const QSignalBlocker input(inputDevice_),output(outputDevice_),clock(clock_),offset(offset_),autoClock(autoClock_),autoFrequency(autoFrequency_);
    radioConfig_=p.radio;inputDevice_->setText(p.input);outputDevice_->setText(p.output);clock_->setValue(p.clockPpm);offset_->setValue(p.offsetHz);autoClock_->setChecked(p.autoClock);autoFrequency_->setChecked(p.autoFrequency);
    audioLevels_->setDevices(p.input,p.output);audioLevels_->restoreLevels(p.txLevel,p.rxGain,p.autoRx);captureGain_.reset();receiveRetry_.stop();receiveRetryDelayMs_=1000;receiveError_.clear();
    if(radioControls_){radioControls_->close();radioControls_.clear();}
}
void MainWindow::changeProfile(const QString& operation,const QString& id,const QString& name,const QString& expected,std::function<void(QString)> done,quint64 session){
    auto error=profileOperationError();if(error.isEmpty()&&remoteClientMode())error="Use the shack's profile controls while remotely connected.";
    if(error.isEmpty()&&expected!=profiles_.revision)error="Radio profiles changed. Refresh the list before trying again.";
    if(!error.isEmpty()){done(error);return;}
    auto previous=profiles_;previous.updateCurrent(currentProfileSettings());auto candidate=previous;error=candidate.change(operation,id,name.trimmed(),currentProfileSettings());if(!error.isEmpty()){done(error);return;}
    const bool switching=candidate.active!=profiles_.active;
    // Saving, renaming and removing an inactive profile do not interrupt RX.
    if(!switching){
        profileSaveTimer_.stop();profiles_=candidate;error=persistProfiles(true);if(!error.isEmpty()){profiles_=previous;persistProfiles(true);}
        updateProfileUi();publishRemoteState();if(error.isEmpty())status_->setText("Radio profile: "+profiles_.current()->name+" · Saved");done(error);return;
    }
    const bool shared=remoteConfig_.role==remote::Role::Host;
    if(shared){error=remoteServer_->reserveLocalTransmit();if(!error.isEmpty()){done(error);return;}}
    profileSaveTimer_.stop();profileApplying_=true;
    const bool wasSuspended=receiveSuspended_;if(switching){receiveSuspended_=true;audioLevels_->stopChecks();stopBackgroundReceive();}
    updateEnabled();
    const auto finish=[this,previous,candidate,switching,shared,wasSuspended,session,done=std::move(done)](QString error){
        if(error.isEmpty()&&(closing_||(session&&(!remoteServer_->hasClient()||remoteServer_->session()!=session))))error="Profile change cancelled because the station connection changed.";
        if(error.isEmpty()){profiles_=candidate;error=persistProfiles(true);if(!error.isEmpty()){profiles_=previous;persistProfiles(true);}else if(switching)applyProfileSettings(profiles_.current()->settings);}
        profileApplying_=false;receiveSuspended_=wasSuspended;if(shared)remoteServer_->releaseLocalTransmit();
        updateProfileUi();updateRadioStatus();updateEnabled();publishRemoteState();
        if(error.isEmpty())status_->setText("Radio profile: "+profiles_.current()->name+(switching?" · Settings loaded · Radio → Connect when ready":" · Saved"));
        done(error);
    };
    if(switching&&(radio_->connected()||radio_->busy()))radio_->disconnectRadio(finish);else finish({});
}
void MainWindow::updateProfileUi(){
    if(!profileChoice_)return;
    auto summary=displayedProfiles();auto catalog=summary;catalog.remove("txLevel");const bool changed=catalog!=profileUiCatalog_;
    if(remoteClientMode()&&txPreparing_&&!profileUiCatalog_.value("active").toString().isEmpty()&&profileUiCatalog_.value("active")!=catalog.value("active")){if(cancel_)cancel_->store(true);txPreparing_=false;status_->setText("Transmission cancelled because the shack radio profile changed.");}
    {const QSignalBlocker block(profileChoice_);if(changed){profileUiCatalog_=catalog;profileChoice_->clear();for(const auto value:summary.value("items").toArray()){const auto entry=value.toObject();profileChoice_->addItem(entry.value("name").toString(),entry.value("id").toString());}if(profileChoice_->count()==0)profileChoice_->addItem(remoteClientMode()?(remoteClient_->connected()?"Shack needs profiles update":"Connect for radio profiles"):"No radio profiles");}const int index=profileChoice_->findData(summary.value("active").toString());if(index>=0&&profileChoice_->currentIndex()!=index)profileChoice_->setCurrentIndex(index);}
    const bool available=!summary.value("items").toArray().isEmpty();const bool transmitting=remoteClientMode()&&(remoteClient_->snapshot().value("transmitting").toBool()||remoteClient_->snapshot().value("preparing").toBool());profileChoice_->setEnabled(available&&!transmitting&&profileOperationError().isEmpty());profilesButton_->setEnabled(!closing_&&!profileApplying_);
    audioLevels_->setRemoteTransmitLevel(remoteClientMode()&&summary.value("txLevel").isDouble()?summary.value("txLevel").toInt():-1);
    if(changed)emit radioProfilesChanged();
}
void MainWindow::showProfiles(){
    QDialog dialog(this);dialog.setObjectName("radioProfileDialog");dialog.setWindowTitle(remoteClientMode()?"Radio profiles · Shack PC":"Radio profiles · This computer");dialog.resize(580,350);auto* layout=new QVBoxLayout(&dialog);
    auto* info=new QLabel(remoteClientMode()?"These profiles belong to the shack PC. Laptop microphone, speakers and Wi-Fi pairing stay on this laptop.":"Each radio keeps its CAT/PTT connection, audio devices, application audio levels and receive calibration. Changes save automatically to the active profile.");info->setWordWrap(true);layout->addWidget(info);
    auto* choice=new QComboBox;choice->setObjectName("profileDialogChoice");layout->addWidget(choice);auto* details=new QLabel;details->setObjectName("profileDetails");details->setWordWrap(true);layout->addWidget(details);
    auto* row=new QHBoxLayout;auto* use=new QPushButton("Use selected");use->setObjectName("profileUse");auto* save=new QPushButton("Save current");save->setObjectName("profileSave");auto* create=new QPushButton("Save as new…");create->setObjectName("profileNew");auto* rename=new QPushButton("Rename…");rename->setObjectName("profileRename");auto* remove=new QPushButton("Delete…");remove->setObjectName("profileDelete");for(auto* button:{use,save,create,rename,remove})row->addWidget(button);layout->addLayout(row);
    auto* setup=new QPushButton("Setup active radio…");setup->setObjectName("profileSetup");layout->addWidget(setup);connect(setup,&QPushButton::clicked,&dialog,[this,&dialog]{dialog.accept();QTimer::singleShot(0,this,&MainWindow::showSetupWizard);});
    auto* hint=new QLabel("Save as new makes a copy of the current setup. Name it for your next radio, then use Setup active radio to choose its audio and CAT devices. Switching closes CAT and never transmits.");hint->setWordWrap(true);layout->addWidget(hint);
    auto* error=new QLabel;error->setObjectName("profileError");error->setWordWrap(true);layout->addWidget(error);auto* close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QJsonArray lastItems;bool pending=false;QString revision,lastActive,lastBusy;
    const auto refresh=[&]{
        const auto summary=displayedProfiles();revision=summary.value("revision").toString();const auto items=summary.value("items").toArray();const auto active=summary.value("active").toString();const auto selected=choice->currentData().toString();
        if(lastItems!=items){const QSignalBlocker block(choice);lastItems=items;choice->clear();for(const auto value:items){const auto p=value.toObject();choice->addItem(p.value("name").toString(),p.value("id").toString());}int index=choice->findData(selected);if(index<0)index=choice->findData(active);choice->setCurrentIndex(index);}
        if(lastActive!=active){const QSignalBlocker block(choice);const int index=choice->findData(active);if(index>=0)choice->setCurrentIndex(index);lastActive=active;}
        const bool online=!items.isEmpty(),selectedActive=choice->currentData().toString()==active;const auto busy=remoteClientMode()?(remoteBusy()||remoteClient_->snapshot().value("transmitting").toBool()||remoteClient_->snapshot().value("preparing").toBool()?QString("Wait for the shack to return to receive."):QString{}):profileOperationError();
        const bool ready=online&&!pending&&busy.isEmpty();choice->setEnabled(!pending);use->setEnabled(ready&&!selectedActive);save->setEnabled(ready);create->setEnabled(ready&&items.size()<profiles::MaxProfiles);rename->setEnabled(ready);remove->setEnabled(ready&&!selectedActive&&items.size()>1);setup->setEnabled(ready);
        QString currentName;for(const auto value:items)if(value.toObject().value("id").toString()==active)currentName=value.toObject().value("name").toString();details->setText(online?"Active radio: "+currentName+(selectedActive?"\nThis profile's settings are in use.":"\nUse selected to load this radio's saved settings."):remoteClientMode()?"Connect to a shack running Tux-sstv 1.13 or newer to manage its profiles.":"Profiles are unavailable.");
        if(!pending){if(!busy.isEmpty())error->setText(busy);else if(error->text()==lastBusy)error->clear();lastBusy=busy;}
    };
    const auto run=[&](const QString& operation,const QString& name,const QString& requestedId=QString{},const QString& expectedRevision=QString{}){const auto id=requestedId.isEmpty()?choice->currentData().toString():requestedId;const auto expected=expectedRevision.isEmpty()?revision:expectedRevision;pending=true;error->setText("Updating radio profiles…");refresh();const QPointer<QDialog> guard(&dialog);const auto done=[&,guard](QString problem){if(!guard)return;pending=false;error->clear();refresh();if(!problem.isEmpty())error->setText(problem);};if(remoteClientMode())remoteClient_->command("profiles.change",{{"operation",operation},{"id",id},{"name",name},{"expected",expected}},done);else changeProfile(operation,id,name,expected,done);};
    connect(choice,&QComboBox::currentIndexChanged,&dialog,[&]{refresh();});connect(use,&QPushButton::clicked,&dialog,[&]{run("switch",{});});connect(save,&QPushButton::clicked,&dialog,[&]{run("save",{});});
    const auto named=[&](bool newProfile){const auto selectedId=choice->currentData().toString(),expected=revision;QInputDialog prompt(&dialog);prompt.setObjectName("radioProfileName");prompt.setWindowTitle(newProfile?"New radio profile":"Rename radio profile");prompt.setLabelText("Radio profile name");prompt.setTextValue(newProfile?QString{}:choice->currentText());if(prompt.exec()==QDialog::Accepted)run(newProfile?"new":"rename",prompt.textValue().trimmed(),selectedId,expected);};
    connect(create,&QPushButton::clicked,&dialog,[&]{named(true);});connect(rename,&QPushButton::clicked,&dialog,[&]{named(false);});connect(remove,&QPushButton::clicked,&dialog,[&]{const auto selectedId=choice->currentData().toString(),expected=revision;if(QMessageBox::question(&dialog,"Delete radio profile","Delete “"+choice->currentText()+"”?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)run("delete",{},selectedId,expected);});
    connect(this,&MainWindow::radioProfilesChanged,&dialog,refresh);connect(radio_,&radio::Controller::changed,&dialog,refresh);connect(remoteClient_,&remote::Client::snapshotChanged,&dialog,refresh);auto* timer=new QTimer(&dialog);timer->setInterval(250);connect(timer,&QTimer::timeout,&dialog,refresh);timer->start();refresh();dialog.exec();
}
