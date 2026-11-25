#include "audio_process.h"
#include "mainwindow.h"
#include "radio_ui.h"
#include "radio_frontend.h"
#include <QtWidgets>

bool MainWindow::automaticPtt() const{return !remoteClientMode()&&radioConfig_.enabled&&radioConfig_.automaticPtt;}
void MainWindow::setupRadio(){
    auto* menu=menuBar()->addMenu("&Radio");menu->setObjectName("radioMenu");radioConnect_=menu->addAction("Connect",this,&MainWindow::connectRadio);radioConnect_->setObjectName("radioConnectAction");
    radioDisconnect_=menu->addAction("Disconnect",this,[this]{
        if(remoteClientMode()){remoteRadioDisconnectPending_=true;updateRemoteListening();}
        if(voiceHeld_||voiceHostActive_||radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive())stopAll();
        radio_->disconnectRadio([this](const QString& error){
            remoteRadioDisconnectPending_=false;
            if(remoteClientMode()&&!error.isEmpty()&&listenToShack_)listenToShack_->setChecked(false);
            status_->setText(error.isEmpty()?"Radio disconnected":error);updateEnabled();
        });
    });radioDisconnect_->setObjectName("radioDisconnectAction");
    auto* setup=menu->addAction("Hamlib setup…",this,[this]{showSettingsPage(1);});setup->setObjectName("radioSetupAction");
    auto* controls=menu->addAction("Radio console",this,&MainWindow::showRadioControls);controls->setObjectName("radioControlsAction");auto* advanced=menu->addAction("Advanced controls…",this,&MainWindow::showAdvancedRadioControls);advanced->setObjectName("radioAdvancedControlsAction");
    radioRelease_=menu->addAction("PTT OFF / Stop TX",this,&MainWindow::stopRadioTransmission);radioRelease_->setObjectName("radioReleaseAction");
    connect(radio_,&radio::Controller::changed,this,[this]{updateRadioStatus();updateEnabled();});
    connect(radio_,&radio::Controller::fault,this,[this](const QString& error){if(voiceHostActive_||voiceHeld_||radioTxPhase_!=RadioTxPhase::Idle){txStopReason_="Radio connection error: "+error;stopAll();}status_->setText("Radio connection error: "+error);});
    radioLead_.setSingleShot(true);radioTail_.setSingleShot(true);radioWatchdog_.setSingleShot(true);
    connect(&radioLead_,&QTimer::timeout,this,[this]{if(radioTxPhase_==RadioTxPhase::Lead&&!playbackStopped_&&!closing_)launchPlayback();});
    connect(&radioTail_,&QTimer::timeout,this,[this]{if(radioTxPhase_==RadioTxPhase::Tail)releaseRadioTx(true,"Playback complete · Radio RX");});
    connect(&radioWatchdog_,&QTimer::timeout,this,[this]{
        const bool radioActive=radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive();
        const QString message=radioActive?"Playback exceeded its expected duration · Stopped audio and requested PTT OFF":"Playback exceeded its expected duration · Stopped audio";
        txStopReason_=message;stopAll();if(!radioActive)status_->setText(message);
    });
    updateRadioStatus();
}
void MainWindow::connectRadio(){
    if(remoteBusy())return;
    if(!displayedRadioConfig().enabled){status_->setText("Enable Hamlib in Radio → Hamlib setup before connecting.");return;}
    status_->setText("Connecting to Hamlib radio…");radio_->connectRadio(radioConfig_,[this](const QString& error){status_->setText(error.isEmpty()?"Hamlib radio connected":error);updateRadioStatus();updateEnabled();});
}
void MainWindow::showRadioControls(){
    viewerTabs_->setCurrentWidget(radioPage_);
}
void MainWindow::showAdvancedRadioControls(){
    if(!radioControls_)radioControls_=new RadioControls(radio_,[this]{return !remoteBusy()&&!micCheckActive_&&!job_.isRunning()&&!recording_&&radioTxPhase_==RadioTxPhase::Idle&&playback_.state()==QProcess::NotRunning;},[this]{stopRadioTransmission();},this,displayedRadioConfig().model);
    radioControls_->show();radioControls_->raise();radioControls_->activateWindow();
}
void MainWindow::stopRadioTransmission(){
    const bool pending=radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive();stopAll();
    if(!pending&&radio_->connected())radio_->release([this](const QString& error){status_->setText(error.isEmpty()?"Radio PTT released":error);});
}
void MainWindow::updateRadioStatus(){
    if(radioFrontend_)radioFrontend_->setConfig(displayedRadioConfig());
    if(recording_)return;
    QString text;
    if(remoteClientMode()){
        if(!remoteClient_||!remoteClient_->connected())text="Remote station · Not connected to shack PC";
        else {const auto state=remoteClient_->snapshot();text="Shack RX · "+state.value("input").toString()+"\nShack TX · "+state.value("output").toString();}
    }else {
        if(!radioConfig_.enabled)text=QSettings().value("audio/setupConnection").toString()=="signalink"?"PTT: SignaLink audio · Audio: ALSA":"PTT: Manual · Audio: ALSA";
        else if(!radio_->connected())text="Hamlib: Disconnected · "+(radio_->error().isEmpty()?QString("Radio → Connect"):radio_->error());
        else {const auto state=radio_->state();text=QString("Hamlib: %1 MHz · %2 · PTT %3").arg(state.contains("frequencyHz")?QString::number(state["frequencyHz"].toDouble()/1e6,'f',6):"unknown",state.value("mode").toString(),state.contains("ptt")?(state["ptt"].toInt()?"TX":"RX"):"unknown");}
        if(backgroundReceiving_)text="Automatic RX · Images saved automatically\n"+text;
    }
    text.replace("ALSA",audio::backendName());
    if(recordInfo_->text()!=text)recordInfo_->setText(text);
    recordInfo_->setToolTip(text);
}
void MainWindow::startPreparedPlayback(const QString& path,qint64 audioMs,qint64 morseStart){
    if(remoteClientMode()){
        playbackStopped_=false;remoteTx_=true;txDurationMs_=audioMs;txMorseStartMs_=morseStart;txPlaybackClock_.invalidate();txProgress_->setValue(0);txProgress_->setFormat("Sending audio to shack PC · %p%");txProgressInfo_->setText("The shack PC starts only after the complete audio file arrives.");
        QString error;if(!remoteClient_->sendWave(path,morseStart,&error)){remoteTx_=false;playbackStopped_=true;endPlaybackProgress(false);status_->setText(error);}else status_->setText("Sending SSTV audio to shack PC…");updateEnabled();return;
    }
    playbackStopped_=false;pendingPlaybackPath_=path;++radioTxGeneration_;
    txDurationMs_=std::max(qint64(1),audioMs+(automaticPtt()?radioConfig_.leadMs+radioConfig_.tailMs:0));txMorseStartMs_=morseStart<0?-1:morseStart+(automaticPtt()?radioConfig_.leadMs:0);
    txPlaybackClock_.invalidate();txProgress_->setValue(0);
    if(!automaticPtt()){radioWatchdog_.start(int(txDurationMs_+5000));launchPlayback();return;}
    radioTxPhase_=RadioTxPhase::Keying;txProgress_->setFormat("Keying radio · %p%");txProgressInfo_->setText("Waiting for Hamlib PTT acknowledgment…");const auto generation=radioTxGeneration_;
    radio_->key([this,generation](const QString& error){
        if(generation!=radioTxGeneration_ || closing_)return;
        if(!error.isEmpty()){playbackStopped_=true;releaseRadioTx(false,"Could not key radio: "+error);return;}
        radioTxPhase_=RadioTxPhase::Lead;txPlaybackClock_.start();txProgressTimer_.start();updatePlaybackProgress();
        radioWatchdog_.start(int(txDurationMs_+5000));radioLead_.start(radioConfig_.leadMs);updateEnabled();
    });updateEnabled();
}
void MainWindow::launchPlayback(){
    if(closing_ || playbackStopped_)return;
    if(radioTxPhase_!=RadioTxPhase::Idle)radioTxPhase_=RadioTxPhase::Audio;
    txProgress_->setFormat(QString("Starting playback · %p% · %1 s remaining").arg((txDurationMs_+999)/1000));txProgressInfo_->setText("Opening audio output…");
    audio::startPlayback(playback_,{"-q","-D",outputDevice_->text().trimmed(),pendingPlaybackPath_});
    status_->setText("Playing TX audio · "+outputDevice_->text()+(automaticPtt()?" · Hamlib PTT":QSettings().value("audio/setupConnection").toString()=="signalink"?" · SignaLink audio PTT":" · PTT is manual"));
}
void MainWindow::finishPlayback(bool success,const QString& error){
    if(closing_ || radioTxPhase_==RadioTxPhase::Releasing || (playbackStopped_&&radioTxPhase_==RadioTxPhase::Idle))return;
    if(radioTxPhase_!=RadioTxPhase::Idle){
        if(success&&!playbackStopped_){radioTxPhase_=RadioTxPhase::Tail;radioTailClock_.start();radioTail_.start(radioConfig_.tailMs);updatePlaybackProgress();status_->setText("Audio finished · Returning radio to receive…");}
        else releaseRadioTx(false,error.isEmpty()?"Playback stopped":error);
    }else {radioWatchdog_.stop();endPlaybackProgress(success);status_->setText(playbackStopped_?"Playback stopped":success?(QSettings().value("audio/setupConnection").toString()=="signalink"?"Playback complete · Check SignaLink PTT indicator":"Playback complete · PTT remains manual"):error);}
    updateEnabled();
}
void MainWindow::releaseRadioTx(bool success,const QString& message){
    radioLead_.stop();radioTail_.stop();radioWatchdog_.stop();radioTxPhase_=RadioTxPhase::Releasing;const auto generation=radioTxGeneration_;
    radio_->release([this,generation,success,message](const QString& error){
        if(generation!=radioTxGeneration_ || closing_)return;
        radioTxPhase_=RadioTxPhase::Idle;pendingPlaybackPath_.clear();
        endPlaybackProgress(success&&error.isEmpty());
        status_->setText(error.isEmpty()?message:message+" · PTT OFF not confirmed: "+error);updateRadioStatus();updateEnabled();
    });updateEnabled();
}
