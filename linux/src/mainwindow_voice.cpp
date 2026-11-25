#include "mainwindow.h"
#include "voice_audio.h"
#include "audio_wizard.h"
#include "audio_levels_panel.h"
#include "radio_frontend.h"
#include "mic_check_dialog.h"
#include <QtWidgets>
#include <algorithm>

void MainWindow::setupVoice(){
    voiceCapture_=new VoiceCapture(this);voicePlayer_=new VoicePlayer(this);remoteSpeakerPlayer_=new RemoteSpeaker(this);
    auto* box=new QFrame;box->setObjectName("radioVoiceDeck");auto* layout=new QGridLayout(box);layout->setHorizontalSpacing(10);layout->setVerticalSpacing(5);
    voicePtt_=new QPushButton("VOICE PTT\nClick to transmit");voicePtt_->setObjectName("voicePttButton");voicePtt_->setAutoRepeat(false);voicePtt_->setCheckable(true);voicePtt_->setMinimumSize(180,62);
    voicePtt_->setToolTip("Click once to lock voice transmission on. Click again to return to receive. Space also toggles PTT when this button has focus. SignaLink keys from speech; its DLY control sets the release delay.");
    voiceMicrophone_=new QComboBox;voiceMicrophone_->setObjectName("voiceMicrophone");voiceMicrophone_->setEditable(true);voiceMicrophone_->addItem("default");
    QSettings settings;const auto mic=settings.value("voice/microphone","default").toString();if(mic!="default")voiceMicrophone_->addItem(mic);voiceMicrophone_->setCurrentText(mic);
    voiceMicrophone_->setToolTip("Microphone on this computer. The radio audio output is selected on the shack PC.");
    voiceGain_=new QSpinBox;voiceGain_->setObjectName("voiceGain");voiceGain_->setRange(25,300);voiceGain_->setSuffix(" %");voiceGain_->setValue(settings.value("voice/gain",100).toInt());
    voiceMeter_=new QProgressBar;voiceMeter_->setObjectName("voiceMicrophoneLevel");voiceMeter_->setRange(0,100);voiceMeter_->setValue(0);voiceMeter_->setFormat("MIC %p%");voiceMeter_->setMaximumHeight(20);
    voiceStatus_=new QLabel;voiceStatus_->setObjectName("voiceStatus");voiceStatus_->setWordWrap(true);
    auto* refresh=new QPushButton("Find audio devices");refresh->setObjectName("voiceFindMicrophones");
    layout->addWidget(voicePtt_,0,0,2,1);layout->addWidget(new QLabel("This computer's microphone"),0,1);layout->addWidget(voiceMicrophone_,0,2);layout->addWidget(refresh,0,3);
    layout->addWidget(new QLabel("Mic gain"),1,1);auto* gainRow=new QHBoxLayout;gainRow->addWidget(voiceGain_,1);voiceMeter_->setFixedWidth(100);gainRow->addWidget(voiceMeter_);layout->addLayout(gainRow,1,2);micCheckButton_=new QPushButton("Mic check…");micCheckButton_->setObjectName("voiceMicCheck");micCheckButton_->setToolTip("Check this computer's microphone and adjust voice gain without transmitting.");layout->addWidget(micCheckButton_,1,3);layout->setColumnStretch(2,1);
    remoteSpeakerControls_=new QWidget;remoteSpeakerControls_->setObjectName("radioSpeakerControls");auto* speakers=new QHBoxLayout(remoteSpeakerControls_);speakers->setContentsMargins(0,0,0,0);speakers->setSpacing(10);
    listenToShack_=new QCheckBox("Listen to shack");listenToShack_->setObjectName("listenToShack");listenToShack_->setChecked(settings.value("voice/listenToShack",false).toBool());speakers->addWidget(listenToShack_);
    speakers->addWidget(new QLabel("Laptop speakers"));remoteSpeaker_=new QComboBox;remoteSpeaker_->setObjectName("remoteSpeakerDevice");remoteSpeaker_->setEditable(true);remoteSpeaker_->addItem("default");remoteSpeakerDevice_=settings.value("voice/speaker","default").toString().trimmed();if(remoteSpeakerDevice_.isEmpty())remoteSpeakerDevice_="default";if(remoteSpeakerDevice_!="default")remoteSpeaker_->addItem(remoteSpeakerDevice_);remoteSpeaker_->setCurrentText(remoteSpeakerDevice_);speakers->addWidget(remoteSpeaker_,1);
    remoteSpeakerVolume_=new QSpinBox;remoteSpeakerVolume_->setObjectName("remoteSpeakerVolume");remoteSpeakerVolume_->setRange(0,100);remoteSpeakerVolume_->setSuffix(" %");remoteSpeakerVolume_->setValue(std::clamp(settings.value("voice/speakerVolume",75).toInt(),0,100));remoteSpeakerVolume_->setToolTip("Laptop listening volume. Does not change radio transmit levels or SSTV decoding.");speakers->addWidget(remoteSpeakerVolume_);
    remoteSpeakerStatus_=new QLabel;remoteSpeakerStatus_->setObjectName("remoteSpeakerStatus");remoteSpeakerStatus_->setTextFormat(Qt::PlainText);remoteSpeakerStatus_->setFixedHeight(remoteSpeakerStatus_->fontMetrics().lineSpacing()+2);remoteSpeakerStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    layout->addWidget(remoteSpeakerControls_,2,0,1,4);layout->addWidget(remoteSpeakerStatus_,3,0,1,4);layout->addWidget(voiceStatus_,4,0,1,4);
    radioFrontend_->addVoiceControls(box);
    connect(micCheckButton_,&QPushButton::clicked,this,&MainWindow::showMicCheck);
    connect(voicePtt_,&QPushButton::clicked,this,[this](bool checked){if(checked)startVoice();else stopVoice();updateVoiceControls();});
    connect(voiceMicrophone_,&QComboBox::currentTextChanged,this,[](const QString& text){QSettings().setValue("voice/microphone",text);});
    connect(voiceGain_,&QSpinBox::valueChanged,this,[](int value){QSettings().setValue("voice/gain",value);});
    connect(listenToShack_,&QCheckBox::toggled,this,[this](bool checked){QSettings().setValue("voice/listenToShack",checked);remoteSpeakerError_.clear();updateRemoteListening();});
    connect(remoteSpeakerVolume_,&QSpinBox::valueChanged,this,[this](int volume){QSettings().setValue("voice/speakerVolume",volume);remoteSpeakerPlayer_->setVolume(volume);});
    const auto chooseSpeaker=[this]{auto device=remoteSpeaker_->currentText().trimmed();if(device.isEmpty())device="default";if(device!=remoteSpeakerDevice_){remoteSpeakerPlayer_->stop();remoteSpeakerDevice_=device;QSettings().setValue("voice/speaker",device);}remoteSpeakerError_.clear();updateRemoteListening();};
    connect(remoteSpeaker_,qOverload<int>(&QComboBox::activated),this,[chooseSpeaker](int){chooseSpeaker();});connect(remoteSpeaker_->lineEdit(),&QLineEdit::editingFinished,this,chooseSpeaker);
    connect(remoteSpeakerPlayer_,&RemoteSpeaker::fault,this,[this](const QString& error){listenToShack_->setChecked(false);remoteSpeakerError_=error;updateRemoteListening();status_->setText(error);});
    auto* inputDiscovery=new QProcess(this);auto* outputDiscovery=new QProcess(this);
    const auto installDiscovery=[this,refresh,inputDiscovery,outputDiscovery](QProcess* process,QComboBox* choices){
        auto* limit=new QTimer(process);limit->setSingleShot(true);connect(limit,&QTimer::timeout,process,&QProcess::kill);
        connect(process,&QProcess::started,limit,[limit]{limit->start(3000);});
        const auto found=[process,choices,limit,refresh,inputDiscovery,outputDiscovery]{limit->stop();const QSignalBlocker block(choices);const auto selected=choices->currentText();for(const auto& device:AudioWizard::pcmDevices(process->readAllStandardOutput()))if(choices->findText(device.name)<0)choices->addItem(device.name);choices->setCurrentText(selected);refresh->setEnabled(inputDiscovery->state()==QProcess::NotRunning&&outputDiscovery->state()==QProcess::NotRunning);};
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,found);connect(process,&QProcess::errorOccurred,this,[found](QProcess::ProcessError error){if(error==QProcess::FailedToStart)found();});
    };
    installDiscovery(inputDiscovery,voiceMicrophone_);installDiscovery(outputDiscovery,remoteSpeaker_);
    connect(refresh,&QPushButton::clicked,this,[this,inputDiscovery,outputDiscovery,refresh]{if(inputDiscovery->state()!=QProcess::NotRunning||outputDiscovery->state()!=QProcess::NotRunning||voiceHeld_)return;refresh->setEnabled(false);inputDiscovery->start("arecord",{"-L"});outputDiscovery->start("aplay",{"-L"});});
    connect(voiceCapture_,&VoiceCapture::level,voiceMeter_,&QProgressBar::setValue);
    connect(voiceCapture_,&VoiceCapture::pcm,this,[this](const QByteArray& pcm){if(!voiceHeld_||!voiceReady_)return;if(remoteClientMode())remoteClient_->sendVoicePcm(pcm);else voicePlayer_->push(pcm);});
    connect(voiceCapture_,&VoiceCapture::opened,this,[this]{
        if(!voiceHeld_)return;
        const auto generation=voiceInputGeneration_;
        auto done=[this,generation](QString error){if(generation!=voiceInputGeneration_||!voiceHeld_)return;if(!error.isEmpty()){stopVoice();voiceStatus_->setText(error);return;}voiceReady_=true;updateVoiceControls();};
        if(remoteClientMode())remoteClient_->startVoice(done);else startVoiceOutput(done);
    });
    connect(voiceCapture_,&VoiceCapture::fault,this,[this](const QString& error){stopVoice();voiceStatus_->setText(error);status_->setText(error);});
    connect(voicePlayer_,&VoicePlayer::opened,this,[this]{
        if(!voiceHostActive_)return;
        const auto generation=voiceOutputGeneration_;
        auto keyed=[this,generation](QString error){
            if(generation!=voiceOutputGeneration_||!voiceHostActive_)return;
            if(!error.isEmpty()){auto done=std::move(voiceOutputReady_);stopVoiceOutput();if(done)done("Could not key radio: "+error);return;}
            QTimer::singleShot(voiceUsesPtt_?radioConfig_.leadMs:0,this,[this,generation]{if(generation!=voiceOutputGeneration_||!voiceHostActive_)return;voicePlayer_->allowPlayback();auto done=std::move(voiceOutputReady_);if(done)done({});updateVoiceControls();publishRemoteState();});
        };
        if(voiceUsesPtt_)radio_->key(keyed);else keyed({});
    });
    connect(voicePlayer_,&VoicePlayer::fault,this,[this](const QString& error){remoteServer_->finishVoice(error);stopVoice();stopVoiceOutput();voiceStatus_->setText(error);status_->setText(error);});
    remoteServer_->beginVoice=[this](remote::Server::Ready ready){startVoiceOutput(std::move(ready));};
    remoteServer_->voicePcm=[this](const QByteArray& pcm){voicePlayer_->push(pcm);};
    connect(remoteServer_,&remote::Server::voiceStopped,this,&MainWindow::stopVoiceOutput);
    connect(remoteClient_,&remote::Client::voiceFinished,this,[this](const QString& message){stopVoice();if(!message.isEmpty())voiceStatus_->setText(message);});
    updateVoiceControls();
}
void MainWindow::startVoice(){
    if(!voicePtt_->isEnabled()||voiceHeld_||closing_)return;
    if(!remoteClientMode()&&remoteConfig_.role==remote::Role::Host){const auto error=remoteServer_->reserveLocalTransmit();if(!error.isEmpty()){voiceStatus_->setText(error);return;}voiceReserved_=true;}
    finishTxDrag();audioLevels_->stopChecks();stopBackgroundReceive();voiceHeld_=true;voiceReady_=false;++voiceInputGeneration_;
    voiceCapture_->start(voiceMicrophone_->currentText().trimmed(),voiceGain_->value());updateEnabled();
}
void MainWindow::startVoiceOutput(std::function<void(QString)> ready){
    if(voiceHostActive_||micCheckActive_||closing_){ready("Voice output is busy.");return;}
    stopBackgroundReceive();audioLevels_->stopChecks();voiceHostActive_=true;voiceUsesPtt_=automaticPtt();++voiceOutputGeneration_;voiceOutputReady_=std::move(ready);
    voicePlayer_->start(outputDevice_->text().trimmed());updateEnabled();
}
void MainWindow::stopVoiceOutput(){
    if(!voiceHostActive_)return;
    voiceHostActive_=false;++voiceOutputGeneration_;voiceOutputReady_={};voicePlayer_->stop();
    if(voiceUsesPtt_||radio_->pttActive()){voiceUsesPtt_=false;releaseRadioTx(false,"Voice PTT released · Radio RX");}
    if(voiceReserved_){voiceReserved_=false;remoteServer_->releaseLocalTransmit();}
    updateEnabled();
}
void MainWindow::stopVoice(){
    if(!voiceHeld_)return;
    voiceHeld_=voiceReady_=false;++voiceInputGeneration_;voiceCapture_->stop();voicePtt_->setDown(false);voiceMeter_->setValue(0);
    if(remoteClientMode())remoteClient_->stopVoice();else stopVoiceOutput();
    if(voiceReserved_){voiceReserved_=false;remoteServer_->releaseLocalTransmit();}
    updateEnabled();
}
void MainWindow::updateVoiceControls(){
    if(!voicePtt_)return;
    const auto snapshot=remoteClient_?remoteClient_->snapshot():QJsonObject{};
    const bool paired=remoteClientMode()&&remoteClient_&&remoteClient_->connected();
    const bool available=!remoteClientMode()||(paired&&remoteClient_->voiceSupported());
    const bool busy=remoteBusy()||micCheckActive_||job_.isRunning()||txPreparing_||recording_||playback_.state()!=QProcess::NotRunning||radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive()||radio_->busy()||audioLevels_->checking()||(remoteClientMode()&&(snapshot.value("transmitting").toBool()||snapshot.value("preparing").toBool()));
    micCheckButton_->setEnabled(!closing_&&!busy&&!voiceHeld_&&!voiceHostActive_);
    const bool pttReady=remoteClientMode()?(!snapshot.value("automaticPtt").toBool()||radio_->connected()):(!automaticPtt()||radio_->connected());
    voicePtt_->setEnabled(voiceHeld_||(available&&pttReady&&!busy));voiceMicrophone_->setEnabled(!voiceHeld_);voiceGain_->setEnabled(!voiceHeld_);
    voicePtt_->setChecked(voiceHeld_);
    voicePtt_->setText(voiceHeld_?(voiceReady_?"TRANSMITTING\nClick to stop":"Opening voice…\nClick to cancel"):"VOICE PTT\nClick to transmit");
    const QString style=voiceHeld_?"QPushButton {background: #b3261e; color: white; font-weight: bold;}":QString{};
    if(voicePtt_->styleSheet()!=style)voicePtt_->setStyleSheet(style);
    if(voiceHeld_)voiceStatus_->setText(voiceReady_?"Talk now. Click PTT again to return to receive.":"Opening microphone and waiting for the shack…");
    else if(voiceHostActive_||snapshot.value("voice").toBool())voiceStatus_->setText("Voice transmission active at the shack. Stop / PTT OFF ends it.");
    else if(!available)voiceStatus_->setText(paired?"Update Tux-sstv on the shack PC to use voice PTT.":"Connect to the shack PC to use remote voice PTT.");
    else if(!pttReady)voiceStatus_->setText("Connect the Hamlib radio before using PTT.");
    else if(displayedRadioConfig().model==2039&&displayedRadioConfig().pttValue!=3)voiceStatus_->setText("TS-990S USB voice: choose Data / USB / rear input in Radio setup.");
    else if(displayedRadioConfig().model==2039&&!radio_->state().value("mode").toString().contains("-D"))voiceStatus_->setText("TS-990S USB voice: select USB-D1 / LSB-D1, or configure this mode's DATA SEND source for USB.");
    else voiceStatus_->setText("Voice ready · Click once to transmit, again to return to receive.");
    updateRemoteListening();
}

void MainWindow::updateRemoteListening(){
    if(!remoteSpeakerPlayer_)return;
    const bool client=remoteClientMode(),online=client&&remoteClient_&&remoteClient_->connected();
    const auto snapshot=online?remoteClient_->snapshot():QJsonObject{};
    const bool radioReady=!snapshot.value("radioEnabled").toBool()||snapshot.value("radioConnected").toBool();
    const bool paused=closing_||receiveSuspended_||micCheckActive_||remoteBusy()||txPreparing_||radio_->pttActive()||radio_->state().value("ptt",0).toInt()!=0||snapshot.value("transmitting").toBool()||snapshot.value("preparing").toBool()||snapshot.value("voice").toBool();
    const bool ready=online&&radioReady&&!remoteRadioDisconnectPending_&&!paused&&snapshot.value("receiving").toBool()&&snapshot.value("receiveError").toString().isEmpty();
    const bool wanted=listenToShack_->isChecked()&&ready;
    if(!wanted&&remoteSpeakerPlayer_->active())remoteSpeakerPlayer_->stop();
    else if(wanted&&!remoteSpeakerPlayer_->active())remoteSpeakerPlayer_->start(remoteSpeakerDevice_,remoteSpeakerVolume_->value());
    remoteSpeakerControls_->setVisible(client);remoteSpeakerStatus_->setVisible(client);listenToShack_->setEnabled(client&&!closing_);
    const QString text=!remoteSpeakerError_.isEmpty()?remoteSpeakerError_:!listenToShack_->isChecked()?"Laptop speakers off · SSTV reception continues":!online?"Speakers waiting for shack connection":remoteRadioDisconnectPending_?"Speakers muted · Radio disconnecting":!radioReady?"Speakers muted · Shack radio disconnected":micCheckActive_?"Speakers muted during microphone check":paused?"Speakers muted during transmission or setup":!ready?"Speakers waiting for shack receive audio":"Listening to shack · Laptop speakers";
    if(remoteSpeakerStatus_->text()!=text)remoteSpeakerStatus_->setText(text);
    remoteSpeakerStatus_->setToolTip(text);listenToShack_->setToolTip("Hear the shack receiver on this computer. Mutes during transmission and resumes on receive.");
}

void MainWindow::showMicCheck(){
    if(closing_||micCheckActive_||!micCheckButton_->isEnabled())return;
    const bool suspended=receiveSuspended_;micCheckActive_=true;
    if(!remoteClientMode()){receiveSuspended_=true;stopBackgroundReceive();}
    updateEnabled();QStringList devices;for(int i=0;i<voiceMicrophone_->count();++i)devices<<voiceMicrophone_->itemText(i);
    MicCheckDialog dialog(devices,voiceMicrophone_->currentText().trimmed(),voiceGain_->value(),this);micCheckDialog_=&dialog;
    if(dialog.exec()==QDialog::Accepted&&!closing_){voiceMicrophone_->setCurrentText(dialog.microphone());voiceGain_->setValue(dialog.gainPercent());}
    micCheckDialog_.clear();micCheckActive_=false;receiveSuspended_=suspended;updateEnabled();
}
