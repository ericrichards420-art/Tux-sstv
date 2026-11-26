#include "mainwindow.h"
#include "radio_ui.h"
#include "audio_levels_panel.h"
#include <QtWidgets>
#include <QJsonArray>

QString MainWindow::radioSetupError() const{
    if(profileApplying_||closing_||audioSetupBusy_||radioSetupBusy_||voiceHeld_||voiceHostActive_||job_.isRunning()||txPreparing_||recording_||
       playback_.state()!=QProcess::NotRunning||radioTxPhase_!=RadioTxPhase::Idle||
       radio_->pttActive()||radio_->state().value("ptt",0).toInt()!=0||audioLevels_->checking()||
       (remoteServer_&&remoteServer_->remoteTransmitBusy()))return "Stop transmission and wait for the radio to be idle before changing Hamlib setup.";
    return {};
}
void MainWindow::applyRadioSetup(const radio::Config& config,std::function<void(QString)> done,quint64 session){
    auto error=radioSetupError();if(error.isEmpty()&&config.enabled)error=config.validate();
    if(!error.isEmpty()){done(error);return;}
    const bool shared=remoteConfig_.role==remote::Role::Host;
    if(shared){error=remoteServer_->reserveLocalTransmit();if(!error.isEmpty()){done(error);return;}}
    radioSetupBusy_=true;updateEnabled();
    const auto apply=[this,config,done=std::move(done),session,shared](QString error){
        if(error.isEmpty()&&(closing_||(session&&(!remoteServer_->hasClient()||remoteServer_->session()!=session))))error="Hamlib setup cancelled because the station connection changed.";
        if(error.isEmpty()){radioConfig_=config;QSettings settings;config.save(settings);settings.sync();saveActiveProfile();}
        radioSetupBusy_=false;if(shared)remoteServer_->releaseLocalTransmit();
        updateRadioStatus();updateEnabled();done(error);
    };
    // A model/device change takes effect only after the old connection is closed.
    if(radio_->connected()||radio_->busy())radio_->disconnectRadio(apply);else apply({});
}
void MainWindow::showRadioSetup(){
    if(!prepareSetup("Hamlib setup",[this]{showRadioSetup();}))return;
    const bool remote=remoteClientMode();
    radio::Config original=radioConfig_;QString decodeError;
    const auto snapshot=remoteClient_?remoteClient_->snapshot():QJsonObject{};
    const auto originalProfile=snapshot.value("profiles").toObject().value("active").toString();
    const bool available=!remote||(remoteClient_->connected()&&radio::Config::fromJson(snapshot.value("radioConfig").toObject(),original,decodeError));
    QDialog dialog(this);dialog.setObjectName("radioSetupDialog");dialog.setWindowTitle(remote?"Hamlib radio setup · Shack PC":"Hamlib radio setup · This shack PC");dialog.resize(720,700);
    auto* layout=new QVBoxLayout(&dialog);auto* hint=new QLabel;hint->setObjectName("radioSetupLocation");hint->setWordWrap(true);
    hint->setText(remote?(available?"These are the shack PC's Hamlib settings. Save updates the shack radio connection; your laptop's local radio settings stay separate.":"Connect to a shack running Tux-sstv or OpenSSTV 1.3.2 or newer to view and change its Hamlib setup. Your laptop's local radio settings are preserved."):"Configure Hamlib here while the laptop remains paired. Save closes the current CAT connection; use Radio → Connect afterward.");layout->addWidget(hint);
    if(remote){auto* local=new QPushButton("Set up a USB radio connected to this computer…");local->setObjectName("radioSetupUseLocal");layout->addWidget(local);connect(local,&QPushButton::clicked,&dialog,[this,&dialog]{dialog.reject();QTimer::singleShot(0,this,[this]{useLocalSetup(false);});});}
    auto* page=new RadioSettingsPage(original,radio_);page->setAudioConnection(remote?snapshot.value("audioConnection").toString():QSettings().value("audio/setupConnection","other").toString());auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(page);layout->addWidget(scroll,1);page->setEnabled(available);scroll->setVisible(available);
    if(remote){QStringList devices;for(const auto& value:snapshot.value("radioDevices").toArray())if(value.isString())devices<<value.toString();page->setDeviceChoices(devices);auto* check=page->findChild<QPushButton*>("radioCheckConnection");check->setEnabled(false);check->setToolTip("Save the shack setup, then use Radio → Connect. Connection checks never request PTT.");}
    auto* error=new QLabel;error->setObjectName("radioSetupError");error->setWordWrap(true);layout->addWidget(error);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    bool saving=false,changed=false;
    const auto validate=[&]{
        QString problem;const auto config=page->config();if(config.enabled)problem=page->validationError();
        const bool online=!remote||(remoteClient_->connected()&&remoteClient_->snapshot().value("radioConfig").isObject());
        const bool occupied=remote?(remoteClient_->snapshot().value("transmitting").toBool()||remoteClient_->snapshot().value("preparing").toBool()):!radioSetupError().isEmpty();
        buttons->button(QDialogButtonBox::Save)->setEnabled(available&&online&&!saving&&!occupied&&problem.isEmpty());
        if(!saving)error->setText(!online?"Shack disconnected. Reconnect and reopen Hamlib setup.":occupied?"Radio is busy. Save becomes available when it returns to receive.":problem);
    };
    connect(page,&RadioSettingsPage::edited,&dialog,validate);connect(radio_,&radio::Controller::changed,&dialog,validate);
    connect(remoteServer_,&remote::Server::activityChanged,&dialog,validate);
    connect(remoteClient_,&remote::Client::snapshotChanged,&dialog,validate);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,[&]{if(!saving)dialog.reject();});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        if(saving)return;
        if(!remote&&original!=radioConfig_){error->setText("Hamlib setup changed. Reopen this dialog before saving.");return;}
        const auto config=page->config();saving=true;page->setEnabled(false);buttons->setEnabled(false);error->setText("Saving Hamlib setup…");
        const QPointer<QDialog> guard(&dialog);
        const auto done=[&,guard](QString message){if(!guard)return;saving=false;buttons->setEnabled(true);page->setEnabled(available);if(remote)page->findChild<QPushButton*>("radioCheckConnection")->setEnabled(false);if(message.isEmpty()){changed=true;dialog.accept();}else{validate();error->setText(message);}};
        if(remote)remoteClient_->command("radio.configure",{{"config",config.toJson()},{"expected",original.toJson()},{"expectedProfile",originalProfile}},done);
        else applyRadioSetup(config,done);
    });
    validate();dialog.exec();
    if(changed)status_->setText("Hamlib setup saved · Radio → Connect opens the configured shack radio.");
}
