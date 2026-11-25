#include "mainwindow.h"
#include "audio_wizard.h"
#include "audio_levels_panel.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QtEndian>

namespace {
QJsonObject packedState(AudioWizard* wizard,qint64 id,QString& error){
    const auto packed=qCompress(QJsonDocument(wizard->relayState()).toJson(QJsonDocument::Compact),9);
    if(packed.size()>4096){error="The audio device list is too large to display remotely. Use the wizard on the shack PC.";return {};}
    return {{"wizard",double(id)},{"state",QString::fromLatin1(packed.toBase64())}};
}
QJsonObject unpackState(const QJsonObject& result,QString& error){
    const auto packed=QByteArray::fromBase64(result.value("state").toString().toLatin1());
    if(packed.size()<4||packed.size()>4096||qFromBigEndian<quint32>(packed.constData())>256*1024){error="Invalid shack audio setup response.";return {};}
    QJsonParseError parse;const auto json=QJsonDocument::fromJson(qUncompress(packed),&parse);
    if(parse.error!=QJsonParseError::NoError||!json.isObject()){error="Invalid shack audio setup controls.";return {};}return json.object();
}
}
void MainWindow::saveAudioWizard(AudioWizard* wizard){
    receiveRetry_.stop();receiveRetryDelayMs_=1000;inputDevice_->setText(wizard->inputDevice());outputDevice_->setText(wizard->outputDevice());
    audioLevels_->setTransmitPercent(wizard->transmitPercent());if(wizard->useInterfacePtt())radioConfig_.automaticPtt=false;
    QSettings().setValue("audio/setupConnection",wizard->connectionType());saveSettings();updateRadioStatus();status_->setText("Audio devices and levels saved");
}
void MainWindow::showAudioWizard(){
    if(!prepareSetup("Audio setup wizard",[this]{showAudioWizard();}))return;
    if(remoteClientMode()){showRemoteAudioWizard();return;}
    audioLevels_->stopChecks();const bool shared=remoteConfig_.role==remote::Role::Host;
    if(shared){const auto error=remoteServer_->reserveLocalTransmit();if(!error.isEmpty()){status_->setText(error);return;}}
    audioSetupBusy_=true;audioWizardRestoreError_.clear();receiveSuspended_=true;stopBackgroundReceive();updateEnabled();
    AudioWizard wizard(inputDevice_->text(),outputDevice_->text(),this);audioWizard_=&wizard;
    wizard.setWindowTitle(shared?"Tux-sstv audio setup · Shack PC":"Tux-sstv audio setup");
    const bool accepted=wizard.exec()==QDialog::Accepted;
    if(accepted)saveAudioWizard(&wizard);
    if(!wizard.mixerError().isEmpty())status_->setText("Could not restore previous audio levels: "+wizard.mixerError());
    audioWizard_.clear();audioSetupBusy_=false;receiveSuspended_=false;if(shared)remoteServer_->releaseLocalTransmit();updateEnabled();
    if(accepted&&wizard.openRadioControls())showRadioControls();
}
void MainWindow::cancelAudioWizard(){
    if(!audioWizard_)return;
    if(remoteClientMode()){audioWizard_->finishRelay(QDialog::Rejected);return;}
    audioWizard_->reject();
    if(!remoteAudioLease_)return;
    const auto restoreError=audioWizard_->mixerError();audioWizardRestoreError_=restoreError.isEmpty()?QString{}:"Could not restore previous audio levels: "+restoreError;audioWizard_->deleteLater();audioWizard_.clear();
    remoteAudioLease_=false;audioSetupBusy_=false;receiveSuspended_=false;remoteServer_->releaseLocalTransmit();updateEnabled();
    if(!restoreError.isEmpty())status_->setText("Could not restore previous audio levels: "+restoreError);
}
void MainWindow::handleAudioWizardRequest(qint64 id,const QJsonObject& args){
    const auto session=remoteServer_->session();QString error;QJsonObject result;
    const auto operation=args.value("operation").toString();
    if(operation=="open"){
        audioLevels_->stopChecks();error=radioSetupError();
        if(error.isEmpty()&&audioSetupBusy_)error="The shack audio wizard is already open.";
        if(error.isEmpty())error=remoteServer_->reserveLocalTransmit();
        if(error.isEmpty()){
            remoteAudioLease_=audioSetupBusy_=true;audioWizardRestoreError_.clear();audioWizardSession_=session;++audioWizardId_;receiveSuspended_=true;stopBackgroundReceive();updateEnabled();
            audioWizard_=new AudioWizard(inputDevice_->text(),outputDevice_->text(),this);audioWizard_->restart();
            result=packedState(audioWizard_,audioWizardId_,error);if(!error.isEmpty())cancelAudioWizard();
        }
    }else if(!remoteAudioLease_||!audioWizard_||session!=audioWizardSession_||args.value("wizard").toDouble(-1)!=audioWizardId_){
        error="This shack audio setup session has ended. Open the wizard again.";
    }else if(operation=="cancel"){
        cancelAudioWizard();error=audioWizardRestoreError_;
    }else if(operation=="finish"){
        if(audioWizard_->inputDevice().isEmpty()||audioWizard_->outputDevice().isEmpty())error="Enter both receive and transmit device names.";
        else if(!audioWizard_->button(QWizard::FinishButton)->isEnabled())error="Wait for audio setup to finish and open the Review page before saving.";
        else {
            audioWizard_->accept();saveAudioWizard(audioWizard_);result={{"txLevel",audioWizard_->transmitPercent()}};
            audioWizard_->deleteLater();audioWizard_.clear();remoteAudioLease_=audioSetupBusy_=false;receiveSuspended_=false;remoteServer_->releaseLocalTransmit();updateEnabled();
        }
    }else {
        error=audioWizard_->relayOperation(operation,args);if(error.isEmpty())result=packedState(audioWizard_,audioWizardId_,error);
    }
    remoteServer_->reply(id,error,result,session,false);
}
void MainWindow::showRemoteAudioWizard(){
    if(!remoteClient_->connected()||!remoteClient_->snapshot().value("audioWizard").toBool()){
        QMessageBox choice(QMessageBox::Information,"Tux-sstv audio setup wizard","This computer is in remote-laptop mode. Connect to a shack running Tux-sstv or OpenSSTV 1.3.3 or newer to configure its audio over Wi-Fi, or choose local USB setup for a radio connected to this computer.",QMessageBox::NoButton,this);choice.setObjectName("audioSetupConnectionDialog");
        auto* local=choice.addButton("Set up this computer's USB audio",QMessageBox::AcceptRole);local->setObjectName("audioSetupUseLocal");auto* connection=choice.addButton("Remote connection…",QMessageBox::ActionRole);choice.addButton(QMessageBox::Cancel);choice.exec();
        if(choice.clickedButton()==local)useLocalSetup(true);else if(choice.clickedButton()==connection)showRemoteSetup();return;
    }
    audioSetupBusy_=true;updateEnabled();
    remoteClient_->commandResult("audio.wizard",{{"operation","open"}},[this](QString error,QJsonObject result){
        if(closing_)return;
        const auto initial=error.isEmpty()?unpackState(result,error):QJsonObject{};
        if(!error.isEmpty()){if(result.value("wizard").toInteger()>0)remoteClient_->commandResult("audio.wizard",{{"operation","cancel"},{"wizard",result.value("wizard")}},{});audioSetupBusy_=false;updateEnabled();status_->setText(error);return;}
        const auto id=result.value("wizard").toInteger();
        AudioWizard wizard({}, {},this,true);audioWizard_=&wizard;wizard.setWindowTitle("Tux-sstv audio setup · Shack PC over Wi-Fi");wizard.applyRelayState(initial);
        QTimer poll;poll.setInterval(250);bool polling=false,finishing=false,cancelled=false;quint64 revision=0;
        const QPointer<AudioWizard> guard(&wizard);
        std::function<void(QString,QJsonObject)> send=[&,guard,id](QString operation,QJsonObject args){
            if(!guard)return;
            if(operation=="finish"){finishing=true;poll.stop();wizard.setEnabled(false);}
            args["operation"]=operation;args["wizard"]=double(id);
            if(operation=="cancel"){
                if(cancelled)return;
                cancelled=true;poll.stop();const QPointer<MainWindow> owner(this);
                remoteClient_->commandResult("audio.wizard",args,[owner](QString message,QJsonObject){if(owner&&!message.isEmpty()&&!message.contains("session has ended"))owner->status_->setText(message);});return;
            }
            const auto sent=++revision;
            remoteClient_->commandResult("audio.wizard",args,[&,guard,sent,operation](QString error,QJsonObject response){
                if(!guard)return;
                if(operation=="state")polling=false;
                if(operation=="cancel")return;
                if(!error.isEmpty()){
                    if(!remoteClient_->connected()||error.contains("session has ended")){const auto warning=remoteClient_->snapshot().value("audioSetupError").toString();status_->setText(warning.isEmpty()?error:warning);wizard.finishRelay(QDialog::Rejected);return;}
                    wizard.setEnabled(true);if(wizard.currentPage())wizard.currentPage()->setSubTitle(error);status_->setText(error);
                    if(operation=="finish"){finishing=false;poll.start();}return;
                }
                if(operation=="finish"){audioLevels_->setTransmitPercent(response.value("txLevel").toInt(100));wizard.finishRelay(QDialog::Accepted);return;}
                if(sent!=revision||finishing)return;
                const auto state=unpackState(response,error);if(error.isEmpty())wizard.applyRelayState(state);else {status_->setText(error);wizard.reject();}
            });
        };
        connect(&wizard,&AudioWizard::relayRequest,&wizard,send);
        connect(&poll,&QTimer::timeout,&wizard,[&]{if(!polling&&!finishing){polling=true;send("state",{});}});poll.start();
        const bool accepted=wizard.exec()==QDialog::Accepted;poll.stop();
        // Cleanup also covers Stop, which closes the view without emitting Cancel.
        if(!accepted)send("cancel",{});
        audioWizard_.clear();audioSetupBusy_=false;updateEnabled();
        if(accepted){status_->setText("Shack audio devices and levels saved");if(wizard.openRadioControls())showRadioControls();}
    });
}
