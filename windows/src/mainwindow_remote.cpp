#include "mainwindow.h"
#include "audio_levels_panel.h"
#include "radio_frontend.h"
#include "signalview.h"
#include "imageview.h"
#include "transmit_monitor.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonArray>
#include <cmath>
#include <limits>

radio::Config MainWindow::displayedRadioConfig() const{
    auto config=radioConfig_;
    if(remoteClientMode()&&remoteClient_){const auto state=remoteClient_->snapshot();radio::Config hostConfig;QString error;if(radio::Config::fromJson(state.value("radioConfig").toObject(),hostConfig,error))config=hostConfig;config.enabled=state.value("radioEnabled").toBool();config.network=true;config.host=remoteConfig_.host;config.port=remoteConfig_.port;config.automaticPtt=state.value("automaticPtt").toBool();}
    return config;
}
void MainWindow::setupRemote(){
    remoteServer_=new remote::Server(this);remoteClient_=new remote::Client(this);
    connect(remoteServer_,&remote::Server::activityChanged,this,&MainWindow::updateEnabled);
    remoteMonitor_=new TransmitMonitor(this);
    connect(remoteMonitor_,&TransmitMonitor::audio,this,[this](const QByteArray& pcm){
        if(closing_)return;
        if(localEchoActive_)remoteServer_->publishMonitorPcm(pcm);
        if((remoteMonitorShowing_||localEchoActive_||remotePeerMonitor_)&&(remotePeerMonitor_||playback_.state()!=QProcess::NotRunning))signal_->pushPcm(pcm.right(24000));
    });
    connect(remoteMonitor_,&TransmitMonitor::picture,this,[this](const sstv::DecodeResult& frame){
        if(closing_||!remoteMonitorShowing_||localEchoActive_)return;
        received_=frame.image;received_.setText("Direction","TX monitor");received_.setText("Mode",frame.modeId);receivedMode_=frame.modeId;
        rxView_->setImage(received_);rxFullScreenView_->setImage(received_);
        rxView_->setProperty("remoteTxMonitorRows",frame.rows);rxView_->setProperty("remoteTxMonitorComplete",frame.complete);
        const auto* mode=sstv::modeById(frame.modeId);const auto name=mode?mode->name:frame.modeId;
        const auto title=remotePeerMonitor_?QString("Shack TX monitor"):QString("Remote TX monitor");
        rxInfo_->setText(QString("%1 · %2 · %3 / %4 rows%5").arg(title,name).arg(frame.rows).arg(frame.image.height()).arg(frame.complete?" · Image complete":""));
        rxDetected_->setText(title+" · "+name);rxDetected_->setProperty("modeId",frame.modeId);updateEnabled();
    });
    connect(remoteMonitor_,&TransmitMonitor::finished,this,[this](bool complete,const QString& message){
        if(localEchoActive_){localEchoActive_=false;remoteServer_->finishLocalMonitor(localEchoPlaybackOk_);return;}
        const bool peer=remotePeerMonitor_;remotePeerMonitor_=false;
        if(closing_)return;
        if(!remoteMonitorShowing_){if(peer)updateEnabled();return;}
        if(!backgroundReceiving_&&!recording_)signal_->endInput();
        auto text=message;if(peer)text.replace("Remote TX monitor","Shack TX monitor");rxInfo_->setText(text);rxView_->setProperty("remoteTxMonitorComplete",complete);updateEnabled();
    });
    connect(remoteServer_,&remote::Server::request,this,&MainWindow::handleRemoteRequest);
    connect(remoteServer_,&remote::Server::emergencyStop,this,[this]{remoteManualKey_=false;stopAll();});
    connect(remoteServer_,&remote::Server::clientChanged,this,[this](bool connected){audioLevels_->stopChecks();status_->setText(connected?"Laptop connected · shack audio and radio are under remote control":"Laptop disconnected · stopping remote TX");updateRemoteStatus();updateEnabled();});
    remoteServer_->canTransmit=[this]{return remoteTransmitError();};
    remoteServer_->transmit=[this](const QString& path,qint64 duration,qint64 morse){
        const auto error=remoteTransmitError();if(!error.isEmpty())return error;
        finishTxDrag();stopBackgroundReceive();remoteHostTx_=true;
        startPreparedPlayback(path,duration,morse);publishRemoteState();return QString{};
    };
    connect(radio_,&radio::Controller::changed,this,&MainWindow::publishRemoteState);
    connect(&capture_,&QProcess::stateChanged,this,[this]{publishRemoteState();});
    connect(&playback_,&QProcess::stateChanged,this,[this]{publishRemoteState();});
    connect(remoteClient_,&remote::Client::changed,this,[this]{
        if(!remoteClientMode())return;
        if(!remoteClient_->connected()){
            stopVoice();cancelAudioWizard();
            if(txPreparing_){if(cancel_)cancel_->store(true);txPreparing_=false;txProgress_->setFormat("Remote TX cancelled · connection lost");txProgressInfo_->setText("Reconnect, then press Play TX again to resend.");}
            ++captureGeneration_;if(receiveCancel_)receiveCancel_->store(true);backgroundReceiving_=recording_=false;refresh_.stop();captureLimit_.stop();signal_->endInput();record_->setText("Record audio");
        }
        updateRemoteStatus();updateRadioStatus();updateEnabled();
    });
    connect(remoteClient_,&remote::Client::snapshotChanged,this,[this]{
        if(!remoteClientMode())return;
        const auto state=remoteClient_->snapshot();
        if(state.value("transmitOrigin")=="shack"&&txPreparing_){if(cancel_)cancel_->store(true);txPreparing_=false;status_->setText("Shack PC is preparing a transmission · Laptop ready to receive");}
        radio_->setRemoteState(remoteClient_->connected()&&state.value("radioConnected").toBool(),state.value("radio").toObject().toVariantMap(),state.value("radioError").toString(),state.value("radioPttActive").toBool(),state.value("radioPttActivation").toString());
        updateRemoteStatus();updateVoiceControls();
    });
    connect(remoteClient_,&remote::Client::pcm,this,[this](const QByteArray& pcm){if(!remoteClientMode()||!remoteClient_->connected())return;updateReceive();if(backgroundReceiving_||recording_)consumeCapture(pcm);});
    connect(remoteClient_,&remote::Client::monitorStarted,this,[this]{
        if(!remoteClientMode())return;
        if(recording_){saveCaptureAudio();recording_=false;captureLimit_.stop();record_->setText("Record audio");}
        stopBackgroundReceive();refresh_.stop();remoteMonitor_->startStream();remotePeerMonitor_=true;showRemoteTransmitMonitor(true);
    });
    connect(remoteClient_,&remote::Client::monitorPcm,this,[this](const QByteArray& pcm){if(remotePeerMonitor_)remoteMonitor_->pushPcm(pcm);});
    connect(remoteClient_,&remote::Client::monitorFinished,this,[this](bool success){if(remotePeerMonitor_)remoteMonitor_->finish(success);});
    connect(remoteClient_,&remote::Client::transmitStarted,this,[this](qint64 duration,qint64 morse){
        if(!remoteClientMode()||!remoteTx_)return;
        txDurationMs_=std::max(qint64(1),duration);txMorseStartMs_=morse;txPlaybackClock_.start();outputMeterClock_.start();txProgressTimer_.start();status_->setText("Shack PC is transmitting · "+remoteConfig_.host);updateEnabled();
    });
    connect(remoteClient_,&remote::Client::transmitFinished,this,[this](bool success,const QString& message){
        if(!remoteClientMode())return;
        remoteTx_=false;playbackStopped_=!success;audioLevels_->endTransmitMeter();outputMeterClock_.invalidate();endPlaybackProgress(success);status_->setText(message);updateEnabled();
    });
    auto* action=new QAction("Remote station…",this);action->setObjectName("remoteStationAction");connect(action,&QAction::triggered,this,&MainWindow::showRemoteSetup);
    auto* menu=menuBar()->addMenu("&Remote");menu->addAction(action);
    auto* row=new QFrame;row->setObjectName("radioStationBar");auto* layout=new QHBoxLayout(row);layout->setContentsMargins(10,5,10,5);layout->setSpacing(8);
    auto* caption=new QLabel("STATION");caption->setObjectName("radioStationCaption");layout->addWidget(caption);
    auto* button=new QPushButton("Remote station…");button->setObjectName("remoteStationButton");connect(button,&QPushButton::clicked,this,&MainWindow::showRemoteSetup);layout->addWidget(button);
    auto* audioSetup=new QToolButton;audioSetup->setObjectName("radioAudioSetupButton");audioSetup->setDefaultAction(audioWizardAction_);layout->addWidget(audioSetup);
    if(auto* radioMenu=menuBar()->findChild<QMenu*>("radioMenu"))radioMenu->addAction(audioWizardAction_);
    remoteStatus_=new QLabel;remoteStatus_->setObjectName("remoteStationStatus");remoteStatus_->setWordWrap(true);layout->addWidget(remoteStatus_,1);
    radioFrontend_->addStationControls(row);
    setupVoice();
    QSettings settings;applyRemoteConfig(remote::Config::load(settings));
}
void MainWindow::startRemoteTransmitMonitor(const QString& path){
    remoteMonitor_->start(path);showRemoteTransmitMonitor();
}
void MainWindow::startLocalTransmitEcho(const QString& path){
    remoteMonitor_->stop();localEchoActive_=true;localEchoPlaybackOk_=false;remoteMonitor_->start(path);remoteServer_->startLocalMonitor();
}
void MainWindow::releaseLocalTransmit(){if(!localSharedTx_)return;localSharedTx_=false;remoteServer_->releaseLocalTransmit();publishRemoteState();}
void MainWindow::showRemoteTransmitMonitor(bool fromShack){
    remoteMonitorShowing_=true;received_={};receivedMode_.clear();receivedCall_.clear();resetAutoSave();
    receivedUtc_=QDateTime::currentDateTimeUtc();receivedFrequencyHz_=radio_->connected()?radio_->state().value("frequencyHz",0).toDouble():0;
    rxView_->setImage({});rxFullScreenView_->setImage({});rxView_->setProperty("remoteTxMonitorRows",0);rxView_->setProperty("remoteTxMonitorComplete",false);
    const auto title=fromShack?QString("Shack TX monitor"):QString("Remote TX monitor");
    rxInfo_->setText(title+" · Waiting for SSTV image header…");receivedCallLabel_->setText(fromShack?"Outgoing image · Shack PC transmission":"Outgoing image · Remote laptop transmission");
    autoSaveInfo_->setText(title+" · Outgoing audio, not an over-air reception");autoSaveInfo_->setToolTip({});
    rxDetected_->setText(title+" · Detecting mode…");txFullScreen_->hide();viewerTabs_->setCurrentWidget(rxPage_);signal_->beginInput();updateEnabled();
}
QString MainWindow::remoteTransmitError() const{
    if(closing_||remoteConfig_.role!=remote::Role::Host)return "Shack sharing is not active.";
    if(profileApplying_||audioSetupBusy_||radioSetupBusy_||micCheckActive_||voiceHeld_||voiceHostActive_||job_.isRunning()||txPreparing_||recording_||playback_.state()!=QProcess::NotRunning||radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive()||radio_->state().value("ptt",0).toInt()!=0||audioLevels_->checking())return "Shack PC is busy or transmitting.";
    if(automaticPtt()&&!radio_->connected())return "Connect the Hamlib radio using Radio → Connect before transmitting.";
    return {};
}
void MainWindow::handleRemoteRequest(qint64 id,const QString& operation,const QJsonObject& arguments){
    const auto session=remoteServer_->session();
    const auto done=[this,id,session](QString error){publishRemoteState();remoteServer_->reply(id,error,{},session);};
    if(closing_||remoteConfig_.role!=remote::Role::Host||!remoteServer_->hasClient()){done("Shack sharing is not active.");return;}
    if(operation=="profiles.change"){
        if(!arguments.value("operation").isString()||!arguments.value("expected").isString()||arguments.value("id").toString().size()>36||arguments.value("name").toString().toUtf8().size()>64){done("Invalid radio profile request.");return;}
        changeProfile(arguments.value("operation").toString(),arguments.value("id").toString(),arguments.value("name").toString(),arguments.value("expected").toString(),done,session);return;
    }
    if(operation=="audio.wizard"){handleAudioWizardRequest(id,arguments);return;}
    if(operation=="radio.release"){
        // Timed/dialog-owned releases identify the activation acknowledged by
        // their key. Check here even if the laptop has not seen newer snapshots.
        // Emergency Stop intentionally omits this condition.
        const auto expected=arguments.value("expectedPttActivation");
        if(!expected.isUndefined()){
            if(!expected.isString()||expected.toString().isEmpty()){done("Invalid owned PTT release.");return;}
            const auto current=QString::number(remoteServer_->session())+":"+QString::number(radio_->pttGeneration());
            if(expected.toString()!=current){done({});return;}
        }
        remoteManualKey_=false;stopAll();if(radio_->connected()||radio_->pttActive())radio_->release(done);else done({});return;
    }
    if(operation=="radio.disconnect"){remoteManualKey_=false;stopAll();radio_->disconnectRadio(done);return;}
    if(operation=="radio.configure"){
        const auto expectedProfile=arguments.value("expectedProfile").toString();
        if((!expectedProfile.isEmpty()&&expectedProfile!=profiles_.active)||(expectedProfile.isEmpty()&&profiles_.entries.size()>1)){done("Radio profile changed or the laptop needs an update. Reopen Hamlib setup using Tux-sstv 1.13 or newer.");return;}

        radio::Config configuration;QString error;
        if(!radio::Config::fromJson(arguments.value("config").toObject(),configuration,error)){done(error);return;}
        if(arguments.value("expected").toObject()!=radioConfig_.toJson()){done("Hamlib setup changed on the shack. Reopen Radio setup before saving.");return;}
        applyRadioSetup(configuration,done,session);return;
    }
    if(operation=="radio.connect"||operation=="radio.apply"||operation=="radio.key"){
        const auto expectedProfile=arguments.value("expectedProfile").toString();
        if((!expectedProfile.isEmpty()&&expectedProfile!=profiles_.active)||(expectedProfile.isEmpty()&&profiles_.entries.size()>1)){done("Radio profile changed. Review the selected radio and try again using Tux-sstv 1.13 or newer.");return;}
    }
    const auto busy=remoteTransmitError();
    // Connecting is allowed while automatic PTT is waiting for that connection.
    if(!busy.isEmpty()&&!(operation=="radio.connect"&&busy.startsWith("Connect the Hamlib"))){done(busy);return;}
    if(operation=="radio.connect"){
        if(!radioConfig_.enabled){done("Enable Hamlib in Radio → Hamlib setup before connecting.");return;}
        if(radio_->connected()){done({});return;}radio_->connectRadio(radioConfig_,done);return;
    }
    if(operation=="radio.key"){
        const auto session=remoteServer_->session(),activation=radio_->pttGeneration()+1;remoteManualKey_=true;
        radio_->key([this,done,session,activation](QString error){
            if(error.isEmpty()&&(remoteServer_->session()!=session||radio_->pttGeneration()!=activation))error="PTT activation was cancelled.";
            done(error);
        });
        if(radio_->pttGeneration()==activation){
            QTimer::singleShot(10000,this,[this,session,activation]{if(remoteManualKey_&&remoteServer_->session()==session&&radio_->pttGeneration()==activation){remoteManualKey_=false;stopRadioTransmission();}});
        }
        return;
    }
    if(operation=="radio.apply"){
        const auto control=arguments.value("control").toInt(-1);const auto value=arguments.value("value").toDouble(std::numeric_limits<double>::quiet_NaN());const auto text=arguments.value("text").toString();
        if(control<0||control>int(radio::Control::Tone)||!std::isfinite(value)||text.size()>128){done("Invalid radio command.");return;}
        radio_->apply({radio::Control(control),value,text,arguments.value("option").toInt()},done);return;
    }
    done("Unsupported remote radio command.");
}
void MainWindow::publishRemoteState(){
    if(!remoteServer_||remoteConfig_.role!=remote::Role::Host)return;
    const bool playing=voiceHostActive_||playback_.state()!=QProcess::NotRunning||radioTxPhase_!=RadioTxPhase::Idle;
    QStringList devices;int deviceBytes=0;for(const auto& path:radio::serialDevices()){deviceBytes+=path.toUtf8().size();if(deviceBytes>1024)break;devices<<path;}
    remoteServer_->setSnapshot({{"profiles",profilesReady_?profiles_.summary():QJsonObject{}},{"radioEnabled",radioConfig_.enabled},{"automaticPtt",automaticPtt()},
        {"audioWizard",true},{"audioSetupError",audioWizardRestoreError_.left(1500)},{"radioConfig",radioConfig_.toJson()},{"radioDevices",QJsonArray::fromStringList(devices)},{"radioConnected",radio_->connected()},{"radioError",radio_->error()},{"radioPttActive",radio_->pttActive()},{"radioPttActivation",QString::number(remoteServer_->session())+":"+QString::number(radio_->pttGeneration())},{"radio",QJsonObject::fromVariantMap(radio_->state())},
        {"voice",voiceHostActive_},{"transmitting",playing||radio_->pttActive()},{"preparing",profileApplying_||txPreparing_||radioSetupBusy_||audioSetupBusy_||micCheckActive_},{"transmitOrigin",localSharedTx_?"shack":remoteHostTx_||remoteServer_->remoteTransmitBusy()?"laptop":""},{"txDurationMs",double(txDurationMs_)},
        {"receiving",backgroundReceiving_&&capture_.state()==QProcess::Running},{"receiveError",receiveError_},
        {"input",inputDevice_->text()},{"output",outputDevice_->text()},{"audioConnection",QSettings().value("audio/setupConnection","other").toString()}});
}
void MainWindow::updateRemoteStatus(){
    if(!remoteStatus_)return;
    if(remoteConfig_.role==remote::Role::Local){audioLevels_->setDevices(inputDevice_->text(),outputDevice_->text());remoteStatus_->setText("Local USB / sound card");return;}
    if(remoteConfig_.role==remote::Role::Host){
        const auto addresses=remote::localAddresses().join(", ");remoteStatus_->setText(remoteServer_->listening()?QString("Shack PC · %1 · port %2 · %3").arg(addresses.isEmpty()?"No Wi-Fi/LAN address":addresses).arg(remoteConfig_.port).arg(remoteServer_->hasClient()?"Laptop connected":"Waiting for laptop"):"Cannot share shack PC: "+remoteServer_->error());return;
    }
    const auto snapshot=remoteClient_->snapshot();audioLevels_->setDevices("Shack PC: "+snapshot.value("input").toString(),"Shack PC: "+snapshot.value("output").toString());remoteStatus_->setText(remoteClient_->status()+" · "+remoteConfig_.host+(snapshot.value("receiving").toBool()?" · Live shack audio":snapshot.value("transmitting").toBool()?" · Shack transmitting":snapshot.value("receiveError").toString().isEmpty()?QString{}:" · "+snapshot.value("receiveError").toString()));
}
void MainWindow::setRemoteConfig(const remote::Config& config){
    const auto error=config.validate();if(!error.isEmpty()){status_->setText(error);return;}
    if(profileApplying_||audioSetupBusy_||radioSetupBusy_||job_.isRunning()||txPreparing_){status_->setText("Stop the current job before changing station connections.");return;}
    const auto generation=++remoteConfigGeneration_;receiveSuspended_=true;stopAll();remoteClient_->disconnectFromHost();remoteServer_->stop();
    const auto apply=[this,config,generation](QString error){
        if(generation!=remoteConfigGeneration_||closing_)return;
        if(!error.isEmpty()){receiveSuspended_=false;status_->setText("Connection unchanged · PTT release/disconnect failed: "+error);updateEnabled();return;}
        applyRemoteConfig(config);QSettings settings;config.save(settings);settings.sync();receiveSuspended_=false;updateEnabled();
    };
    if(config.role==remote::Role::Client&&!remoteClientMode()&&(radio_->connected()||radio_->pttActive()||radio_->busy()))radio_->disconnectRadio(apply);else apply({});
}
void MainWindow::applyRemoteConfig(const remote::Config& config){
    const bool wasClient=remoteClientMode();remoteConfig_=config;remoteTx_=remoteHostTx_=remoteManualKey_=false;remoteRadioDisconnectPending_=false;audioLevels_->setRemoteAudio(remoteClientMode());
    if(remoteClientMode()){
        radio_->setRemote([this](const QString& operation,const QVariantMap& arguments,radio::Controller::Done done){auto request=QJsonObject::fromVariantMap(arguments);if(operation=="radio.connect"||operation=="radio.apply"||operation=="radio.key")request["expectedProfile"]=remoteClient_->snapshot().value("profiles").toObject().value("active");remoteClient_->command(operation,request,std::move(done));});
        remoteClient_->connectToHost(config);
    }else {if(wasClient)radio_->setRemote({});if(config.role==remote::Role::Host){remoteServer_->start(config.port,config.key);publishRemoteState();}}
    updateRemoteStatus();updateRadioStatus();updateEnabled();updateProfileUi();
}
void MainWindow::showRemoteSetup(){
    QDialog dialog(this);dialog.setObjectName("remoteStationDialog");dialog.setWindowTitle("Tux-sstv remote station");auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);
    auto* role=new QComboBox;role->setObjectName("remoteRole");role->addItems({"Local USB / sound card","Share this shack PC","Connect to shack PC"});role->setCurrentIndex(int(remoteConfig_.role));form->addRow("Use this computer as",role);
    auto* host=new QLineEdit(remoteConfig_.host);host->setObjectName("remoteHost");host->setPlaceholderText("Shack PC's Wi-Fi/LAN address");form->addRow("Shack address",host);
    auto* port=new QSpinBox;port->setObjectName("remotePort");port->setRange(1,65535);port->setValue(remoteConfig_.port);form->addRow("Connection port",port);
    auto* key=new QLineEdit(remoteConfig_.key.isEmpty()?remote::newKey():remoteConfig_.key);key->setObjectName("remoteKey");key->setMaxLength(128);key->setMinimumWidth(340);form->addRow("Connection key",key);
    auto* generate=new QPushButton("Generate new key");generate->setObjectName("remoteNewKey");form->addRow("",generate);connect(generate,&QPushButton::clicked,&dialog,[key]{key->setText(remote::newKey());});
    auto* retry=new QCheckBox("Reconnect automatically when Wi-Fi returns");retry->setObjectName("remoteReconnect");retry->setChecked(remoteConfig_.reconnect);layout->addWidget(retry);
    auto* info=new QLabel;info->setObjectName("remoteSetupHelp");info->setWordWrap(true);info->setMinimumWidth(480);layout->addWidget(info);
    auto* current=new QLabel(remoteStatus_->text());current->setWordWrap(true);layout->addWidget(current);
    auto* error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const auto configuration=[&]{remote::Config c;c.role=remote::Role(role->currentIndex());c.host=host->text().trimmed();c.key=key->text().trimmed();c.port=quint16(port->value());c.reconnect=retry->isChecked();return c;};
    const auto refresh=[&]{const auto mode=remote::Role(role->currentIndex());host->setEnabled(mode==remote::Role::Client);port->setEnabled(mode!=remote::Role::Local);key->setEnabled(mode!=remote::Role::Local);generate->setEnabled(mode==remote::Role::Host);retry->setEnabled(mode==remote::Role::Client);
        info->setText(mode==remote::Role::Host?"Leave Tux-sstv open on the shack PC with its usual USB audio and radio settings. On the laptop, choose Connect to shack PC and copy this address, port and key. Shack address: "+remote::localAddresses().join(", "):mode==remote::Role::Client?"Use your normal picture editor, RX waterfall, Play TX and Radio controls. Audio and PTT use the shack PC. Use Listen to shack in the Radio tab to hear received audio on your laptop speakers, and Voice PTT to talk through your laptop microphone. Use your home Wi-Fi/LAN; do not forward this port through your router.":"Use the audio interface and radio connected to this computer.");
        const auto problem=configuration().validate();error->setText(problem);buttons->button(QDialogButtonBox::Save)->setEnabled(problem.isEmpty()&&!audioSetupBusy_&&!radioSetupBusy_&&!job_.isRunning()&&!txPreparing_);
    };
    connect(role,&QComboBox::currentIndexChanged,&dialog,refresh);connect(host,&QLineEdit::textChanged,&dialog,refresh);connect(key,&QLineEdit::textChanged,&dialog,refresh);connect(port,&QSpinBox::valueChanged,&dialog,refresh);refresh();
    if(dialog.exec()==QDialog::Accepted)setRemoteConfig(configuration());
}
