#include "mainwindow.h"
#include "audio_levels_panel.h"
#include "signalview.h"
#include "transmit_monitor.h"
#include <QtWidgets>
#include <QtEndian>
#include <algorithm>

void MainWindow::queueReceiveUpdate(){
    if(closing_||receiveUpdateQueued_)return;
    receiveUpdateQueued_=true;QTimer::singleShot(0,this,[this]{receiveUpdateQueued_=false;updateReceive();});
}
void MainWindow::requestReceive(){
    if(closing_)return;
    receiveRetry_.stop();receiveRetryDelayMs_=1000;receiveError_.clear();
    // Mode selection can retry sooner, but reception never depends on it.
    // Preserve a header already captured in live RX.
    if(!backgroundReceiving_&&!recording_){
        const auto mode=rxMode_->currentData().toString();
        if(mode.isEmpty()){rxDetected_->setText("Auto · Waiting for mode header");rxDetected_->setProperty("modeId",QString{});}
        else showReceiveMode(mode,false);
        status_->setText("Receiving automatically · "+inputDevice_->text().trimmed());
    }
    updateEnabled();
}
void MainWindow::receiveInputFailed(const QString& error){
    if(closing_)return;
    const auto text=status_->text();
    const bool report=recording_||text.startsWith("Ready")||text.startsWith("Recording receiver audio")||text.startsWith("Receiving automatically")||text.startsWith("Listening")||(!receiveError_.isEmpty()&&text.startsWith(receiveError_));
    backgroundReceiving_=false;recording_=false;stopCaptureRequested_=false;
    signal_->endInput();refresh_.stop();captureLimit_.stop();record_->setText("Record audio");
    receiveError_=error;if(report)status_->setText(error+" · Retrying automatically");
    receiveRetry_.start(receiveRetryDelayMs_);receiveRetryDelayMs_=std::min(receiveRetryDelayMs_*2,10000);
    publishRemoteState();updateEnabled();
}
void MainWindow::saveCaptureAudio(){
    const auto dir=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recordings";
    if(!QDir().mkpath(dir)){status_->setText("Cannot create the audio recording directory");return;}
    const auto file=dir+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".wav";
    sstv::Audio audio;audio.sampleRate=12000;audio.samples.resize(size_t(rawCapture_.size()/2));
    for(size_t i=0;i<audio.samples.size();++i)audio.samples[i]=qFromLittleEndian<qint16>(rawCapture_.constData()+i*2)/32768.f;
    try{sstv::writeWav(file,audio);status_->setText("Audio recording saved · reception continues");}
    catch(const std::exception& e){status_->setText(QString("Recording save failed: %1").arg(e.what()));}
}
void MainWindow::stopBackgroundReceive(){
    if(!backgroundReceiving_){
        // A capture can have ended while its last decode is still in flight.
        if(!recording_&&receiveCancel_){++captureGeneration_;receiveCancel_->store(true);stopCaptureRequested_=false;}
        return;
    }
    ++captureGeneration_;if(receiveJob_.isRunning()&&receiveCancel_)receiveCancel_->store(true);
    stopCaptureRequested_=false;
    discardCapture_=true;capture_.kill();capture_.waitForFinished(500);backgroundReceiving_=false;refresh_.stop();signal_->endInput();
}
void MainWindow::updateReceive(){
    if(closing_)return;
    if(remoteClientMode()){
        const auto endpoint="\nShack PC · "+remoteConfig_.host;
        if(remotePeerMonitor_){receiveState_->setText("Receiving shack TX"+endpoint);receiveState_->setToolTip("Live audio from the shack transmission");return;}
        const bool online=remoteClient_&&remoteClient_->connected();
        const bool wanted=online&&!receiveSuspended_&&!txPreparing_&&!remoteBusy()&&!radio_->pttActive()&&radioTxPhase_==RadioTxPhase::Idle&&!remoteClient_->snapshot().value("transmitting").toBool()&&!remoteClient_->snapshot().value("preparing").toBool();
        if(backgroundReceiving_&&!wanted)stopBackgroundReceive();
        if(wanted&&!backgroundReceiving_&&!recording_&&!job_.isRunning()&&!receiveJob_.isRunning()){
            captureGain_.reset();rawCapture_.clear();lastPreviewBytes_=0;++captureGeneration_;receiver_=std::make_shared<sstv::Receiver>(rxOptions(true));resetAutoSave();receivedFrameId_=0;
            stopCaptureRequested_=false;backgroundReceiving_=true;captureDevice_="Shack PC · "+remoteConfig_.host;signal_->beginInput();refresh_.start();
        }
        const auto source=remoteClient_?remoteClient_->snapshot():QJsonObject{};
        const auto error=source.value("receiveError").toString();
        const QString message=!online?"Remote RX · Not connected":!wanted?"Remote RX paused":!error.isEmpty()?"Shack audio error":recording_?"Recording shack audio":source.value("receiving").toBool()?"Receiving automatically":"Waiting for shack audio";
        const auto text=message+endpoint;
        if(receiveState_->text()!=text)receiveState_->setText(text);
        receiveState_->setToolTip(!online?"Connect to the shack PC to receive audio":!wanted?"Remote RX is paused during transmission or setup":error);
        return;
    }
    const bool transmitting=voiceHeld_||voiceHostActive_||txPreparing_||playback_.state()!=QProcess::NotRunning||radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive()||radio_->state().value("ptt",0).toInt()!=0;
    const bool wanted=!receiveSuspended_&&!transmitting;
    // An edited device should not wait out the previous device's retry delay.
    if(captureDevice_!=inputDevice_->text().trimmed()&&!inputDevice_->hasFocus()){receiveRetry_.stop();receiveRetryDelayMs_=1000;}
    if(backgroundReceiving_&&(!wanted||(!inputDevice_->hasFocus()&&captureDevice_!=inputDevice_->text().trimmed())))stopBackgroundReceive();
    if(wanted&&!receiveRetry_.isActive()&&!backgroundReceiving_&&!recording_&&!stopCaptureRequested_&&!job_.isRunning()&&!receiveJob_.isRunning()&&capture_.state()==QProcess::NotRunning){
        audioLevels_->stopChecks();captureGain_.reset();rawCapture_.clear();lastPreviewBytes_=0;
        ++captureGeneration_;receiver_=std::make_shared<sstv::Receiver>(rxOptions(true));
        resetAutoSave();receivedFrameId_=0;
        if(rxMode_->currentData().toString().isEmpty()){rxDetected_->setText("Auto · Waiting for mode header");rxDetected_->setProperty("modeId",QString{});}
        discardCapture_=false;stopCaptureRequested_=false;backgroundReceiving_=true;captureDevice_=inputDevice_->text().trimmed();signal_->beginInput();
        if((!receiveError_.isEmpty()&&status_->text().startsWith(receiveError_))||status_->text().startsWith("Ready")||status_->text()=="Stopped")status_->setText("Receiving automatically · "+captureDevice_);
        captureDataClock_.start();capture_.start("arecord",{"-q","-D",captureDevice_,"-t","raw","-f","S16_LE","-r","12000","-c","1"});refresh_.start();updateEnabled();
    }
    receiveState_->setText(recording_?"Receiving and decoding SSTV":backgroundReceiving_?
        QString("Receiving automatically · %1%2").arg(captureDevice_,signal_->property("inputSilent").toBool()?" · silence":""):
        !wanted?(remoteMonitor_&&remoteMonitor_->active()?"Remote TX monitor · Outgoing audio":"Receiver paused"):receiveRetry_.isActive()?receiveError_+" · Retrying automatically":"Receiver restarting automatically");
    receiveState_->setToolTip(receiveError_.isEmpty()?receiveState_->text():receiveError_);
    updateRadioStatus();
}
