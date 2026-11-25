#include "mainwindow.h"
#include "audio_wizard.h"
#include <QtWidgets>

bool MainWindow::prepareSetup(const QString& name,std::function<void()> reopen){
    if(closing_)return false;
    if(micCheckDialog_&&micCheckDialog_->isVisible()){micCheckDialog_->raise();micCheckDialog_->activateWindow();return false;}
    if(audioWizard_&&audioWizard_->isVisible()){audioWizard_->raise();audioWizard_->activateWindow();return false;}
    const auto busy=[this]{return remoteBusy()||job_.isRunning()||txPreparing_||recording_||playback_.state()!=QProcess::NotRunning||radioTxPhase_!=RadioTxPhase::Idle||radio_->pttActive()||radio_->state().value("ptt",0).toInt()!=0;};
    if(!busy())return true;
    if(setupWaiting_){status_->setText("Waiting for the station to stop before opening setup…");return false;}
    QMessageBox message(QMessageBox::Information,"Open "+name,
        voiceHeld_||voiceHostActive_?"Voice PTT is locked on. Stop transmission to open "+name+".":"The station is transmitting, recording, or completing another operation. Stop it to open "+name+".",QMessageBox::NoButton,this);
    message.setObjectName("setupBusyDialog");auto* stop=message.addButton("Stop and open "+name,QMessageBox::AcceptRole);stop->setObjectName("setupStopAndOpen");message.addButton(QMessageBox::Cancel);message.exec();
    if(message.clickedButton()!=stop)return false;
    setupWaiting_=true;stopRadioTransmission();auto* retry=new QTimer(this);retry->setInterval(100);auto elapsed=std::make_shared<QElapsedTimer>();elapsed->start();
    connect(retry,&QTimer::timeout,this,[this,retry,elapsed,busy,reopen=std::move(reopen)]{
        if(closing_||elapsed->elapsed()>10000){retry->stop();retry->deleteLater();setupWaiting_=false;if(!closing_)QMessageBox::information(this,"Station still busy","PTT release or the running operation has not completed. Check Stop / PTT OFF and the radio connection, then open setup again.");return;}
        if(busy())return;
        retry->stop();retry->deleteLater();setupWaiting_=false;QTimer::singleShot(0,this,reopen);
    });retry->start();return false;
}
void MainWindow::useLocalSetup(bool audio){
    auto config=remoteConfig_;config.role=remote::Role::Local;
    setRemoteConfig(config);
    if(remoteConfig_.role!=remote::Role::Local)return;
    QTimer::singleShot(0,this,[this,audio]{if(audio)showAudioWizard();else showSettingsPage(1);});
}
