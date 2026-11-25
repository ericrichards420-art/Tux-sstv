#include "mainwindow.h"
#include "core/fskid.h"
#include "core/patterns.h"
#include "signalview.h"
#include "imageview.h"
#include "captionpreview.h"
#include "selfie_dialog.h"
#include "radio_frontend.h"
#include "audio_levels_panel.h"
#include "transmit_monitor.h"
#include "voice_audio.h"
#include "core/morse.h"
#include "skins.h"
#include <QtConcurrentRun>
#include <QtWidgets>
#include <QtEndian>
#include <QImageWriter>
#include <QSaveFile>
#include <QScopeGuard>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <stdexcept>
#include <QUndoStack>

namespace {
QString clockText(qint64 milliseconds) {
    const auto seconds=std::max(qint64(0),milliseconds)/1000;
    return QString("%1:%2").arg(seconds/60).arg(seconds%60,2,10,QChar('0'));
}
}
MainWindow::MainWindow(QWidget* parent,std::unique_ptr<radio::Backend> backend):QMainWindow(parent) {
    radio_=new radio::Controller(this,std::move(backend));radio_->setObjectName("radioController");QSettings radioSettings;radioConfig_=radio::Config::load(radioSettings);
    const int implementedCount=int(std::count_if(sstv::modes().begin(),sstv::modes().end(),[](const auto& mode){return mode.implemented;}));
    const int targetCount=int(sstv::modes().size());
    txUndo_=new QUndoStack(this);txUndo_->setObjectName("txUndoStack");txUndo_->setUndoLimit(50);
    buildUi(implementedCount,targetCount);
    setupRadio();
    connect(open_,&QPushButton::clicked,this,&MainWindow::openImage);
    connect(pattern_,&QPushButton::clicked,this,&MainWindow::showTestPatterns);
    connect(txMode_,&QComboBox::currentIndexChanged,this,&MainWindow::updateTx);
    connect(call_,&QLineEdit::textChanged,this,&MainWindow::updateTx);
    connect(rxMode_,&QComboBox::currentIndexChanged,this,[this] {
        stopBackgroundReceive();
        const bool automatic=rxMode_->currentData().toString().isEmpty();
        if(automatic){rxDetected_->setText("Auto · Waiting for mode header");rxDetected_->setProperty("modeId",QString{});}
        else showReceiveMode(rxMode_->currentData().toString(),false);
        requestReceive();
    });
    connect(rxMode_,&QComboBox::activated,this,[this]{requestReceive();});
    connect(export_,&QPushButton::clicked,this,[this]{exportAudio(false);});
    connect(play_,&QPushButton::clicked,this,[this]{exportAudio(true);});
    connect(decode_,&QPushButton::clicked,this,&MainWindow::decodeFile);
    connect(record_,&QPushButton::clicked,this,&MainWindow::toggleRecording);
    connect(saveRx_,&QPushButton::clicked,this,[this]{saveReceived();});
    connect(stop_,&QPushButton::clicked,this,&MainWindow::stopAll);
    connect(&job_,&QFutureWatcher<JobResult>::finished,this,[this]{finishJob();});
    connect(&receiveJob_,&QFutureWatcher<JobResult>::finished,this,[this]{finishJob(true);});
    connect(&capture_,&QProcess::readyReadStandardOutput,this,&MainWindow::readCapture);
    connect(&capture_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus) {
        const bool monitor=backgroundReceiving_;
        readCapture();signal_->endInput(); recording_=false; refresh_.stop(); captureLimit_.stop(); record_->setText("Record audio");
        backgroundReceiving_=false;
        if(closing_) return;
        if(discardCapture_) {discardCapture_=false;updateEnabled();return;}
        if(monitor){receiveInputFailed("Receive input stopped: "+QString::fromLocal8Bit(capture_.readAllStandardError()).left(350));return;}
        if(!stopCaptureRequested_ && code!=0) {receiveInputFailed("Capture failed: "+QString::fromLocal8Bit(capture_.readAllStandardError()).left(350));return;}
        if(job_.isRunning()||receiveJob_.isRunning()) { stopCaptureRequested_=true; }
        else {stopCaptureRequested_=false;decodeCapture(false);}
        updateEnabled();
    });
    connect(&capture_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e) {if(e==QProcess::FailedToStart)receiveInputFailed("Cannot start arecord: "+capture_.errorString());});
    receiveRetry_.setSingleShot(true);connect(&receiveRetry_,&QTimer::timeout,this,&MainWindow::queueReceiveUpdate);
    txProgressTimer_.setInterval(100);
    connect(&txProgressTimer_,&QTimer::timeout,this,&MainWindow::updatePlaybackProgress);
    connect(&playback_,&QProcess::started,this,[this] {
        if(closing_ || playbackStopped_)return;
        outputMeterClock_.start();
        if(remoteHostTx_)startRemoteTransmitMonitor(pendingPlaybackPath_);
        else if(localSharedTx_)startLocalTransmitEcho(pendingPlaybackPath_);
        if(radioTxPhase_==RadioTxPhase::Idle)txPlaybackClock_.start();
        txProgressTimer_.start();updatePlaybackProgress();updateEnabled();
    });
    connect(&playback_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus exitStatus) {
        if(closing_)return;
        audioLevels_->endTransmitMeter();outputMeterClock_.invalidate();
        const bool success=code==0 && exitStatus==QProcess::NormalExit;
        if(localEchoActive_)localEchoPlaybackOk_=success&&!playbackStopped_;
        if(remoteMonitor_&&remoteMonitor_->active())remoteMonitor_->finish(success&&!playbackStopped_);
        finishPlayback(success,"Playback ended: "+QString::fromLocal8Bit(playback_.readAllStandardError()).left(350));
        updateEnabled();
    });
    connect(&playback_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e) {
        if(e==QProcess::FailedToStart) {
            if(remoteMonitor_)remoteMonitor_->stop();
            audioLevels_->endTransmitMeter();outputMeterClock_.invalidate();
            finishPlayback(false,"Cannot start aplay: "+playback_.errorString());updateEnabled();
        }
    });
    refresh_.setInterval(1000); connect(&refresh_,&QTimer::timeout,this,[this]{
        // An ALSA helper can remain alive after the USB stream stalls. No PCM
        // is different from quiet PCM: never restart merely for radio silence.
        if(backgroundReceiving_&&!remoteClientMode()&&capture_.state()==QProcess::Running&&captureDataClock_.isValid()&&captureDataClock_.elapsed()>=8000){
            stopBackgroundReceive();receiveInputFailed("Receive input stopped delivering audio");return;
        }
        if(!pendingReceiveSave_.image.isNull()){const auto pending=pendingReceiveSave_;autoSaveReceived(pending,autoSaveKey_);}
        if((recording_||backgroundReceiving_) && !job_.isRunning() && !receiveJob_.isRunning() && rawCapture_.size()>lastPreviewBytes_) decodeCapture(true);
    });
    captureLimit_.setSingleShot(true); captureLimit_.setInterval(600000); connect(&captureLimit_,&QTimer::timeout,this,[this]{if(recording_) toggleRecording();});
    connect(history_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item) {
        QImage image(item->data(Qt::UserRole).toString()); if(!image.isNull()){remoteMonitorShowing_=false;if(remoteMonitor_&&!localEchoActive_&&!remotePeerMonitor_)remoteMonitor_->stop();received_=image;receivedMode_=image.text("Mode");receivedUtc_=QDateTime::fromString(image.text("ReceivedUTC"),Qt::ISODateWithMs);if(!receivedUtc_.isValid())receivedUtc_=QFileInfo(item->data(Qt::UserRole).toString()).lastModified().toUTC();receivedFrequencyHz_=image.text("FrequencyHz").toDouble();setReceivedCall(image.text("Callsign"),false,image.text("CallsignSource"));rxInfo_->setText(item->text());viewerTabs_->setCurrentWidget(rxPage_);updateImages();updateEnabled();}
    });
    QSettings settings;
    const auto resetReceive=[this]{stopBackgroundReceive();queueReceiveUpdate();};
    for(auto* control:{clock_,offset_,start_})connect(control,&QDoubleSpinBox::valueChanged,this,resetReceive);
    for(auto* control:{autoClock_,autoFrequency_})connect(control,&QCheckBox::toggled,this,resetReceive);
    settings.remove("tx/mcaProfile");
    showCallsignOnImage_->setChecked(settings.value("tx/showCallsignOnImage",true).toBool());
    call_->setText(settings.value("station/call").toString()); inputDevice_->setText(settings.value("audio/input","default").toString()); outputDevice_->setText(settings.value("audio/output","default").toString());
    autoClock_->setChecked(settings.value("rx/autoClock",false).toBool());autoFrequency_->setChecked(settings.value("rx/autoFrequency",false).toBool());
    clock_->setValue(settings.value("rx/clock",0).toDouble()); offset_->setValue(settings.value("rx/offset",0).toDouble());
    morseEnabled_=settings.value("tx/morseEnabled",false).toBool();morseAfterTx_->setChecked(morseEnabled_);
    morseWpm_=std::clamp(settings.value("tx/morseWpm",20).toInt(),5,40);
    morseTone_=std::clamp(settings.value("tx/morseTone",800).toInt(),400,2500);
    settings.remove("tx/caption/text");
    overlay_.color=QColor(settings.value("tx/caption/color","#ffffffff").toString());if(!overlay_.color.isValid())overlay_.color=Qt::white;
    overlay_.sizePercent=std::clamp(settings.value("tx/caption/size",7).toInt(),3,18);overlay_.position=std::clamp(settings.value("tx/caption/position",7).toInt(),0,8);
    overlay_.bold=settings.value("tx/caption/bold",true).toBool();overlay_.outline=settings.value("tx/caption/outline",true).toBool();overlay_.backing=settings.value("tx/caption/backing",false).toBool();
    recentEmojis_=settings.value("tx/recentEmojis").toStringList().mid(0,16);
    emojiTone_=std::clamp(settings.value("tx/emojiTone",0).toInt(),0,6);
    templateFields_=sstv::TemplateFields::load(settings);templateFields_.save(settings);
    historyPath_=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)+"/OpenSSTV";
    autoSaveInfo_->setText("Auto-save on · "+historyPath_);
    stockPaths_=settings.value("pictures/stock").toStringList();
    connect(showCallsignOnImage_,&QCheckBox::toggled,this,[this](bool checked){auto before=txEditState();before.showCallsign=!checked;saveSettings();updateTx();recordTxEdit(before,"Change picture callsign");});
    connect(morseAfterTx_,&QCheckBox::toggled,this,[this](bool enabled){morseEnabled_=enabled;saveSettings();updateTx();});
    loadBuiltInPictures();loadStock();loadHistory();source_=sstv::testPattern();
    if(stock_->count()==0)galleryTabs_->setCurrentWidget(tuxPictures_);
    applySkin(settings.value("ui/skin","classic").toString());updateTx();updateEnabled();
    setupRemote();setupProfiles();settings.remove("audio/liveMonitorAtStartup");queueReceiveUpdate();
}
MainWindow::~MainWindow() { closing_=true;if(remoteSpeakerPlayer_)remoteSpeakerPlayer_->stop();cancelAudioWizard();stopVoice();stopVoiceOutput();if(remoteMonitor_)remoteMonitor_->stop();if(remoteClient_)remoteClient_->disconnectFromHost();if(remoteServer_)remoteServer_->stop();receiveRetry_.stop();disconnect(txView_,nullptr,this,nullptr);disconnect(txFullScreenView_,nullptr,this,nullptr);txUndo_->blockSignals(true);txUndo_->clear();if(audioLevels_)audioLevels_->shutdown();txPointerHide_.stop(); if(cancel_) cancel_->store(true);if(receiveCancel_)receiveCancel_->store(true);radioLead_.stop();radioTail_.stop();radioWatchdog_.stop();playback_.kill();radio_->shutdown();job_.waitForFinished();receiveJob_.waitForFinished();capture_.kill();capture_.waitForFinished(1000);playback_.waitForFinished(1000); }
void MainWindow::saveSettings() {
    saveActiveProfile();
    QSettings s;s.setValue("station/call",call_->text().trimmed());s.setValue("audio/input",inputDevice_->text());s.setValue("audio/output",outputDevice_->text());
    s.setValue("rx/autoClock",autoClock_->isChecked());s.setValue("rx/autoFrequency",autoFrequency_->isChecked());s.setValue("rx/clock",clock_->value());s.setValue("rx/offset",offset_->value());s.setValue("pictures/stock",stockPaths_);
    s.setValue("tx/showCallsignOnImage",showCallsignOnImage_->isChecked());s.setValue("tx/morseEnabled",morseEnabled_);s.setValue("tx/morseWpm",morseWpm_);s.setValue("tx/morseTone",morseTone_);s.setValue("ui/skin",skinId_);
    radioConfig_.save(s);
    templateFields_.save(s);
    s.remove("tx/caption/text");s.setValue("tx/caption/color",overlay_.color.name(QColor::HexArgb));s.setValue("tx/caption/size",overlay_.sizePercent);s.setValue("tx/caption/position",overlay_.position);
    s.setValue("tx/caption/bold",overlay_.bold);s.setValue("tx/caption/outline",overlay_.outline);s.setValue("tx/caption/backing",overlay_.backing);s.setValue("tx/recentEmojis",recentEmojis_);s.setValue("tx/emojiTone",emojiTone_);
}
void MainWindow::applySkin(const QString& id) {
    const auto& skin=ui::skinById(id);skinId_=skin.id;
    const auto colors=ui::skinPalette(skin);setPalette(colors);setStyleSheet(ui::skinStyleSheet(skin));
    radioFrontend_->setSkin(skin);audioLevels_->setSkin(skin);
    for(auto* widget:findChildren<QWidget*>())if(widget->isWindow())widget->setPalette(colors);
    const auto frameStyle=QString("background:#101010;color:#b8b8b8;border:2px %1 %2;").arg(skin.id=="classic"?"inset":"solid",skin.border.name());
    txView_->setStyleSheet(frameStyle);rxView_->setStyleSheet(frameStyle);
    auto grid=skin.trace;grid.setAlpha(45);
    const QColor scopeText=skin.id=="classic"?QColor("#a2b08b"):skin.background.lightness()<128?skin.muted:QColor("#9baec4");
    signal_->setColors(QColor("#0b1118"),grid,scopeText,skin.trace,skin.error);
    setAutoSaveState(!autoSaveInfo_->property("saveError").toBool());
}
void MainWindow::setAutoSaveState(bool success) {
    autoSaveInfo_->setProperty("saveError",!success);const auto& skin=ui::skinById(skinId_);
    autoSaveInfo_->setStyleSheet("color:"+(success?skin.success:skin.error).name()+";");
}
void MainWindow::closeEvent(QCloseEvent* event) { finishTxDrag();saveSettings();closing_=true;if(remoteSpeakerPlayer_)remoteSpeakerPlayer_->stop();if(remoteClient_)remoteClient_->disconnectFromHost();if(remoteServer_)remoteServer_->stop();audioLevels_->shutdown();txPointerHide_.stop();rxFullScreen_->close();txFullScreen_->close();stopAll();const auto error=radio_->shutdown();if(!error.isEmpty())qWarning().noquote()<<error;QMainWindow::closeEvent(event); }
void MainWindow::hideTxPointer(){
    txPointerHide_.stop();txFullScreen_->setCursor(Qt::BlankCursor);txFullScreenView_->setPointerHidden(true);
}
void MainWindow::prepareTxPointerTracking(){
    txFullScreen_->setMouseTracking(true);
    for(auto* widget:txFullScreen_->findChildren<QWidget*>()){widget->setMouseTracking(true);widget->installEventFilter(this);}
    lastTxPointerPosition_=QCursor::pos();hideTxPointer();
}
bool MainWindow::eventFilter(QObject* object,QEvent* event) {
    if(object==txFullScreen_ && (event->type()==QEvent::Show || event->type()==QEvent::Hide)){
        lastTxPointerPosition_=QCursor::pos();hideTxPointer();
    }
    if(event->type()==QEvent::MouseMove && txFullScreen_->isVisible()){
        auto* widget=qobject_cast<QWidget*>(object);
        if(widget && (widget==txFullScreen_ || txFullScreen_->isAncestorOf(widget))){
            const auto position=static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            if(position!=lastTxPointerPosition_){
                lastTxPointerPosition_=position;txFullScreen_->setCursor(Qt::ArrowCursor);txFullScreenView_->setPointerHidden(false);txPointerHide_.start();
            }
        }
    }
    if(object==txFullScreen_ && event->type()==QEvent::Hide && !event->spontaneous()
       && txProgressPanel_ && txProgressPanel_->parentWidget()!=txPage_) {
        auto* layout=qobject_cast<QVBoxLayout*>(txPage_->layout());
        layout->insertWidget(2,txProgressPanel_);txProgressPanel_->show();
    }
    return QMainWindow::eventFilter(object,event);
}
void MainWindow::resizeEvent(QResizeEvent* event) { QMainWindow::resizeEvent(event); }
void MainWindow::saveScreenshot(const QString& path) { grab().save(path); }
void MainWindow::updateImages() {
    if(!source_.isNull()) {const auto composed=transmitComposition();const auto image=composed.image;callsignBounds_=composed.callsignBounds;captionBounds_=composed.captionRegions();insetBounds_=composed.insetBounds;txView_->setImage(image);txFullScreenView_->setImage(image);updateTxDragRegions();}
    if(!received_.isNull()) {rxView_->setImage(received_);rxFullScreenView_->setImage(received_);}
}
QImage MainWindow::transmitImage() const {return composedTransmitImage();}
QString MainWindow::imageCallsign() const {return showCallsignOnImage_->isChecked()?call_->text():QString{};}
sstv::Composition MainWindow::transmitComposition() const {
    const auto* mode=sstv::modeById(txMode_->currentData().toString());QMap<QString,QRect> regions;
    const auto base=templateEnabled_?sstv::renderTemplate(templateFields_,{mode->width,mode->height},call_->text(),source_,&regions,showCallsignOnImage_->isChecked()):source_;
    auto composed=sstv::composeTransmitLayers(base,*mode,templateEnabled_?QString{}:imageCallsign(),captions_,callsignPlacement_,inset_);
    if(templateEnabled_)composed.callsignBounds=regions.value("callsign");
    return composed;
}
QImage MainWindow::composedTransmitImage() const {return transmitComposition().image;}
bool MainWindow::canMoveTxText() const {
    return !remoteBusy()&&!job_.isRunning()&&!txPreparing_&&!recording_&&playback_.state()==QProcess::NotRunning&&radioTxPhase_==RadioTxPhase::Idle&&!radio_->pttActive();
}
void MainWindow::updateTxDragRegions(){
    QMap<QString,QRect> regions;
    if(canMoveTxText()){
        regions=captionBounds_;
        if(!callsignBounds_.isEmpty())regions["callsign"]=callsignBounds_;
        if(!insetBounds_.isEmpty())regions["inset"]=insetBounds_;
    }
    auto order=sstv::captionHitOrder(captions_);if(templateEnabled_)order.append("inset");order.append("callsign");if(!templateEnabled_)order.append("inset");
    txView_->setTextRegions(regions,order);txFullScreenView_->setTextRegions(regions,order);
}
void MainWindow::resetCallsignPosition(){
    if(!canMoveTxText())return;
    finishTxDrag();const auto before=txEditState();callsignPlacement_={};if(templateEnabled_)templateFields_.textPositions.remove("callsign");updateTx();recordTxEdit(before,"Reset callsign position");
}
void MainWindow::updateTx() {
    const auto* m=sstv::modeById(txMode_->currentData().toString());
    if(m) {
        double duration=m->imageMs()+m->headerMs()+200;
        double morseMs=0;bool validId=true;
        if(morseEnabled_)try{morseMs=sstv::morseIdDurationMs({call_->text(),morseWpm_,morseTone_});}catch(const std::exception&){validId=false;}
        if(automaticPtt())duration+=radioConfig_.leadMs+radioConfig_.tailMs;
        txInfo_->setText(QString("%1 × %2 · %3 · %4 s total%5").arg(m->width).arg(m->height).arg(m->name).arg((duration+morseMs)/1000,0,'f',1).arg(morseEnabled_?" · Morse ID":""));
        if(!job_.isRunning() && !txPreparing_ && radioTxPhase_==RadioTxPhase::Idle && playback_.state()==QProcess::NotRunning) {
            txDurationMs_=qint64(std::llround(duration+morseMs));txMorseStartMs_=-1;
            txPlaybackClock_.invalidate();txProgress_->setValue(0);
            txProgress_->setFormat(QString("Ready to send · %p% · %1 s remaining").arg((txDurationMs_+999)/1000));
            txProgressInfo_->setText(QString("Elapsed 0:00 · Remaining %1").arg(clockText(((txDurationMs_+999)/1000)*1000)));
            if(!validId)txProgressInfo_->setText("Morse ID needs a valid station callsign.");
        }
    }
    updateImages();updateEnabled();
}
void MainWindow::openImage() {
    if(!canMoveTxText())return;
    const auto paths=QFileDialog::getOpenFileNames(this,"Load transmit pictures",{},"Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    if(!paths.isEmpty())loadUserImages(paths);
}
QString MainWindow::screenshotDirectory() const{
    const auto pictures=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto standard=QDir(pictures.isEmpty()?QDir::home().filePath("Pictures"):pictures).filePath("Screenshots");
    const auto remembered=QSettings().value("pictures/screenshotDirectory").toString();
    if(!remembered.isEmpty()&&QDir(remembered).exists())return remembered;
    if(QDir(standard).exists())return standard;
    if(!pictures.isEmpty()&&QDir(pictures).exists())return pictures;
    return QDir::homePath();
}
void MainWindow::openScreenshots(){
    if(!canMoveTxText())return;
    // Choosing a picture does not use the radio audio device. Keep live RX,
    // its pending header, and remote listening running through the dialog.
    const auto directory=screenshotDirectory();
    const QStringList filters={"*.png","*.jpg","*.jpeg","*.bmp","*.webp"};
    QFileDialog dialog(this,"Choose screenshots",directory,"Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    dialog.setObjectName("loadScreenshotDialog");dialog.setAcceptMode(QFileDialog::AcceptOpen);dialog.setFileMode(QFileDialog::ExistingFiles);dialog.setViewMode(QFileDialog::List);
    auto locations=dialog.sidebarUrls();const auto folder=QUrl::fromLocalFile(directory);if(!locations.contains(folder))locations<<folder;dialog.setSidebarUrls(locations);
    const auto latest=QDir(directory).entryInfoList(filters,QDir::Files,QDir::Time);
    if(!latest.isEmpty())dialog.selectFile(latest.first().absoluteFilePath());
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto paths=dialog.selectedFiles();if(paths.isEmpty())return;
    QSettings().setValue("pictures/screenshotDirectory",QFileInfo(paths.first()).absolutePath());
    screenshots_->setToolTip("Choose a saved screenshot from\n"+screenshotDirectory());
    loadUserImages(paths);
}
void MainWindow::takeSelfie(){
    if(!canMoveTxText())return;
    // The webcam is video-only; do not interrupt local or remote receive audio.
    SelfieDialog dialog(this);if(dialog.exec()!=QDialog::Accepted)return;
    auto image=dialog.photo();if(image.isNull())return;
    QSettings settings;settings.setValue("camera/device",dialog.device());settings.setValue("camera/mirror",dialog.mirrored());
    const auto captured=QDateTime::currentDateTimeUtc();image.setText("CapturedUTC",captured.toString(Qt::ISODateWithMs));image.setText("Software","Tux-sstv");
    auto pictures=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);if(pictures.isEmpty())pictures=QDir::home().filePath("Pictures");
    const auto folder=QDir(pictures).filePath("OpenSSTV/Selfies"),base=folder+"/Selfie-"+captured.toString("yyyyMMdd-HHmmss-zzz");
    QString path=base+".png";for(int suffix=1;QFileInfo::exists(path);++suffix)path=base+QString("-%1.png").arg(suffix);
    QString error;
    if(!QDir().mkpath(folder))error="Cannot create "+folder;
    else {
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))error=file.errorString();
        else {QImageWriter writer(&file,"png");if(!writer.write(image)){error=writer.errorString();file.cancelWriting();}else if(!file.commit())error=file.errorString();}
    }
    if(error.isEmpty()){loadUserImages({path});status_->setText("Selfie saved and loaded into TX · "+QFileInfo(path).fileName());}
    else {
        resetTxHistory();source_=image;clearImageFields();galleryTabs_->setCurrentIndex(0);viewerTabs_->setCurrentWidget(txPage_);updateTx();updateEnabled();
        status_->setText("Selfie loaded into TX, but could not save it: "+error+" · Use Save image… to keep a copy");
    }
}
void MainWindow::loadUserImages(const QStringList& paths){
    if(!canMoveTxText())return;
    QStringList loaded;QImage first;int failed=0;
    for(const auto& path:paths){
        const auto absolute=QFileInfo(path).absoluteFilePath();if(loaded.contains(absolute))continue;
        QImageReader reader(absolute);reader.setAutoTransform(true);
        if(!first.isNull()&&reader.size().isValid())reader.setScaledSize(reader.size().scaled({160,120},Qt::KeepAspectRatio));
        const auto image=reader.read();if(image.isNull()){++failed;continue;}
        if(first.isNull())first=image;
        loaded<<absolute;
    }
    if(loaded.isEmpty()){status_->setText("Could not load the selected pictures. Check that the files are readable images.");return;}
    resetTxHistory();source_=first;clearImageFields();
    for(auto it=loaded.crbegin();it!=loaded.crend();++it){stockPaths_.removeAll(*it);stockPaths_.prepend(*it);}
    loadStock();galleryTabs_->setCurrentIndex(0);viewerTabs_->setCurrentWidget(txPage_);saveSettings();updateTx();
    status_->setText(QString("Loaded %1 photo%2%3").arg(loaded.size()).arg(loaded.size()==1?"":"s").arg(failed?QString(" · %1 unreadable file(s) skipped").arg(failed):QString{}));
}
void MainWindow::removeSelectedStockImages(){
    if(!canMoveTxText())return;
    QStringList paths;for(auto* item:stock_->selectedItems())paths<<item->data(Qt::UserRole).toString();
    if(paths.isEmpty())return;
    for(const auto& path:paths)stockPaths_.removeAll(path);
    loadStock();saveSettings();
    status_->setText(QString("Removed %1 photo%2 from transmit pictures · Files kept").arg(paths.size()).arg(paths.size()==1?"":"s"));
}

void MainWindow::clearImageFields(){inset_={};callsignPlacement_={};templateFields_.clearFields();templateEnabled_=false;captions_.clear();overlay_.text.clear();overlay_.freePosition=false;overlay_.relativePosition={};}
sstv::DecodeOptions MainWindow::rxOptions(bool partial) const { sstv::DecodeOptions o; o.forceMode=rxMode_->currentData().toString();o.autoClock=autoClock_->isChecked();o.autoFrequency=autoFrequency_->isChecked();o.clockPpm=clock_->value();o.frequencyOffset=offset_->value();o.manualStartMs=o.forceMode.isEmpty()?-1:start_->value();o.allowPartial=partial;return o; }
void MainWindow::showReceiveMode(const QString& id,bool automatic) {
    const auto* mode=sstv::modeById(id);if(!mode)return;
    rxDetected_->setText(QString("%1 · %2").arg(automatic?"Auto detected":"Manual mode",mode->name));
    rxDetected_->setProperty("modeId",id);
}
sstv::ModeDetected MainWindow::modeNotification(bool automatic,bool live) {
    const auto generation=captureGeneration_;
    return [this,automatic,generation,live](const QString& id) {
        QMetaObject::invokeMethod(this,[this,automatic,generation,id,live] {
            const auto token=live?receiveCancel_:cancel_;
            if(closing_ || generation!=captureGeneration_ || (token && token->load()))return;
            showReceiveMode(id,automatic);
            if(recording_||backgroundReceiving_)status_->setText(rxDetected_->text()+" · Receiving image…");
        },Qt::QueuedConnection);
    };
}
void MainWindow::beginJob(std::function<JobResult(sstv::Cancel,sstv::Progress)> work,bool capture) {
    const auto token=std::make_shared<std::atomic_bool>(false);
    if(capture)receiveCancel_=token;else cancel_=token;
    const auto generation=captureGeneration_;if(!capture)progress_->setValue(0);
    auto progress=[this,token,capture,generation,last=-1,clock=QElapsedTimer{}](int value)mutable {
        if(value==last||(value!=100&&clock.isValid()&&clock.elapsed()<16))return;
        last=value;clock.start();
        QMetaObject::invokeMethod(this,[this,token,capture,generation,value] {
            if(closing_||token->load()||(capture&&(generation!=captureGeneration_||job_.isRunning())))return;
            progress_->setValue(value);if(!capture&&txPreparing_)txProgress_->setValue(value);
        },Qt::QueuedConnection);
    };
    auto future=QtConcurrent::run([work=std::move(work),token,progress,capture,generation]{
        JobResult r;try{r=work(token,progress);}catch(const std::exception& e){r.error=QString::fromUtf8(e.what());}
        if(capture){r.captureReceive=true;r.captureGeneration=generation;}
        return r;
    });
    if(capture)receiveJob_.setFuture(future);else job_.setFuture(future);
    updateEnabled();
}
void MainWindow::exportAudio(bool play) {
    if(remoteBusy() || txPreparing_ || job_.isRunning() || recording_ || playback_.state()!=QProcess::NotRunning || radioTxPhase_!=RadioTxPhase::Idle || radio_->pttActive()) return;
    if(play&&remoteClientMode()&&(!remoteClient_||!remoteClient_->connected())){status_->setText("Connect to the shack PC before transmitting.");return;}
    receiveSuspended_=true;const auto resumeReceive=qScopeGuard([this]{receiveSuspended_=false;queueReceiveUpdate();});
    finishTxDrag();audioLevels_->stopChecks();
    stopBackgroundReceive();
    if(play && automaticPtt() && !radio_->connected()){status_->setText("Connect the Hamlib radio before TX (Radio → Connect), or disable automatic PTT in Settings.");return;}
    const sstv::MorseId id{call_->text(),morseWpm_,morseTone_};const bool sendId=morseEnabled_;
    if(sendId)try{sstv::morseIdDurationMs(id);}catch(const std::exception& e){status_->setText(QString("Morse ID: ")+QString::fromUtf8(e.what()));return;}
    QString path;
    if(play) {
        if(!temporary_.isValid()){status_->setText("Cannot create temporary audio directory.");return;}
        path=temporary_.filePath("preview.wav");
    } else {
        path=QFileDialog::getSaveFileName(this,"Export SSTV transmit audio",{},"PCM WAV (*.wav)"); if(path.isEmpty())return;
        if(!path.endsWith(".wav",Qt::CaseInsensitive))path+=".wav";
    }
    const auto* mode=sstv::modeById(txMode_->currentData().toString());
    const auto image=transmitImage();
    const int outputLevel=audioLevels_->transmitPercent();
    if(play&&remoteConfig_.role==remote::Role::Host){const auto error=remoteServer_->reserveLocalTransmit();if(!error.isEmpty()){status_->setText(error);return;}localSharedTx_=true;}
    viewerTabs_->setCurrentWidget(txPage_);
    txPreparing_=true;txProgress_->setValue(0);txProgress_->setFormat("Preparing audio · %p%");
    txProgressInfo_->setText("Generating SSTV audio…");
    status_->setText("Generating "+mode->name+" audio…");
    beginJob([image,mode,path,play,sendId,id,outputLevel](sstv::Cancel cancel,sstv::Progress progress) {
        JobResult r;r.audio=sstv::encode(image,*mode,48000,cancel,progress);
        if(sendId){r.morseStartMs=qint64(std::llround(r.audio.seconds()*1000+500));sstv::appendMorseId(r.audio,id,cancel);}
        sstv::setAudioOutputLevel(r.audio,outputLevel);if(play)r.transmitLevels=sstv::audioEnvelope(r.audio);
        if(cancel->load())throw std::runtime_error("Cancelled.");
        sstv::writeWav(path,r.audio);r.outputPath=path;r.preview=play;return r;
    });
}
void MainWindow::decodeFile() {
    if(job_.isRunning() || recording_)return;
    receiveSuspended_=true;stopBackgroundReceive();const auto resumeReceive=qScopeGuard([this]{receiveSuspended_=false;queueReceiveUpdate();});
    const auto path=QFileDialog::getOpenFileName(this,"Decode received SSTV audio",{},"PCM WAV (*.wav)");if(path.isEmpty())return;
    openReceiveWav(path);
}
void MainWindow::openReceiveWav(const QString& path) {
    if(path.isEmpty() || job_.isRunning() || recording_ || playback_.state()!=QProcess::NotRunning)return;
    stopBackgroundReceive();
    resetAutoSave();receivedFrameId_=0;setReceivedCall({});
    const auto options=rxOptions(true);const int channel=channel_->currentIndex();viewerTabs_->setCurrentWidget(rxPage_);status_->setText("Decoding WAV…");
    rxDetected_->setText(options.forceMode.isEmpty()?"Auto · Searching for mode header…":"Manual · Searching for image start…");
    const auto detected=modeNotification(options.forceMode.isEmpty());
    beginJob([path,options,channel,detected](sstv::Cancel cancel,sstv::Progress progress){JobResult r;r.receive=true;r.audio=sstv::readWav(path,channel);r.decoded=sstv::decode(r.audio,options,cancel,progress,detected);return r;});
}
void MainWindow::finishJob(bool capture) {
    if(closing_)return;
    auto result=capture?receiveJob_.result():job_.result();
    const auto token=capture?receiveCancel_:cancel_;
    if(result.captureReceive&&result.captureGeneration!=captureGeneration_){updateEnabled();return;}
    const auto finishCapture=[this] {
        if(stopCaptureRequested_ && !recording_) {stopCaptureRequested_=false;decodeCapture(false);}
        else updateEnabled();
    };
    // A stop can arrive after the worker has produced its result but before the
    // completion event is delivered. Never start playback from that result.
    if(token && token->load()) {if(!capture&&!recording_)status_->setText("Stopped");updateEnabled();return;}
    if(!result.error.isEmpty()) {
        if(!capture)releaseLocalTransmit();
        if(txPreparing_) {
            txPreparing_=false;txProgress_->setFormat("Preparation failed · %p%");
            txProgressInfo_->setText(result.error);
        }
        if(!recording_)status_->setText(result.error);
        finishCapture();return;
    }
    if(result.captureReceive)showMorse(result.morse);
    if(!result.captureReceive&&!recording_)signal_->setSamples(result.audio.samples,result.audio.sampleRate);
    if(result.captureReceive) {
        const bool automatic=rxMode_->currentData().toString().isEmpty();
        for(const auto& frame:result.frames) {
            if(frame.metadataOnly){if(frame.frameId==receivedFrameId_)setReceivedCall(frame.callsign,true);continue;}
            if(frame.frameId!=receivedFrameId_){receivedFrameId_=frame.frameId;receivedMode_=frame.modeId;receivedUtc_=QDateTime::currentDateTimeUtc();receivedFrequencyHz_=radio_->connected()?radio_->state().value("frequencyHz",0).toDouble():0;setReceivedCall({});}
            if(!frame.image.isNull()){if(remoteMonitorShowing_){remoteMonitorShowing_=false;if(!localEchoActive_&&!remotePeerMonitor_)remoteMonitor_->stop();}received_=frame.image;}
            rxInfo_->setText(frame.message);rxView_->setImage(received_);rxFullScreenView_->setImage(received_);
            autoSaveReceived(frame,QString::number(frame.frameId));if(!frame.callsign.isEmpty())setReceivedCall(frame.callsign,true);
        }
        if(!result.detectedMode.isEmpty()) {
            showReceiveMode(result.detectedMode,automatic);
            if(!recording_ && !result.live && !result.frames.empty() && !result.frames.back().complete)
                status_->setText("Incomplete "+result.frames.back().message);
            else if(!result.frames.empty())status_->setText(result.frames.back().message);
            else status_->setText(rxDetected_->text()+" · Waiting for image data…");
        } else {
            rxDetected_->setText(automatic?"Auto · Waiting for next mode header":"Manual mode · Waiting for next image header");rxDetected_->setProperty("modeId",QString{});
            const bool completed=std::any_of(result.frames.begin(),result.frames.end(),[](const auto& frame){return frame.complete&&!frame.metadataOnly;});
            if(completed&&(recording_||backgroundReceiving_))status_->setText("Image received · Ready for next transmission");
            else if(!backgroundReceiving_||status_->text().startsWith("Receiving automatically")||status_->text().startsWith("Listening"))status_->setText(recording_||backgroundReceiving_?"Listening · Waiting for SSTV header…":"RX finished · No pending image");
        }
    } else if(result.receive) {
        remoteMonitorShowing_=false;if(remoteMonitor_&&!localEchoActive_&&!remotePeerMonitor_)remoteMonitor_->stop();
        receivedMode_=result.decoded.modeId;receivedUtc_=QDateTime::currentDateTimeUtc();receivedFrequencyHz_=0;
        if(!result.decoded.image.isNull())received_=result.decoded.image;
        rxInfo_->setText(result.decoded.message);rxView_->setImage(received_);rxFullScreenView_->setImage(received_);
        status_->setText(result.decoded.message);
        autoSaveReceived(result.decoded,"wav");setReceivedCall(result.decoded.callsign,true);
    } else {
        txPreparing_=false;
        lastAudioPath_=result.outputPath;status_->setText("Audio saved: "+result.outputPath);
        if(result.preview) {
            audioLevels_->setTransmitEnvelope(std::move(result.transmitLevels));
            startPreparedPlayback(result.outputPath,qint64(std::llround(result.audio.seconds()*1000)),result.morseStartMs);
        } else {
            txProgress_->setValue(100);txProgress_->setFormat("WAV saved");
            txProgressInfo_->setText("SSTV audio file ready");
        }
    }
    finishCapture();
}
void MainWindow::updatePlaybackProgress() {
    if((remoteBusy()||playback_.state()==QProcess::Running)&&outputMeterClock_.isValid())audioLevels_->transmitPosition(outputMeterClock_.elapsed());
    if(!txPlaybackClock_.isValid() || txDurationMs_<=0)return;
    const auto elapsed=radioTxPhase_==RadioTxPhase::Releasing?txDurationMs_:radioTxPhase_==RadioTxPhase::Tail?txDurationMs_-radioConfig_.tailMs+std::min(qint64(radioConfig_.tailMs),radioTailClock_.elapsed()):std::min(txDurationMs_,txPlaybackClock_.elapsed());
    txProgress_->setValue(int(std::min(qint64(99),elapsed*100/txDurationMs_)));
    const auto secondsRemaining=(txDurationMs_-elapsed+999)/1000;
    const QString phase=radioTxPhase_==RadioTxPhase::Lead?"Radio lead-in":radioTxPhase_==RadioTxPhase::Tail?"Radio tail delay":radioTxPhase_==RadioTxPhase::Releasing?"Releasing PTT":elapsed==txDurationMs_?"Finishing playback":txMorseStartMs_>=0 && elapsed>=txMorseStartMs_?"Sending Morse ID":"Playing TX audio";
    txProgress_->setFormat(QString("%1 · %p% · %2 s remaining").arg(phase).arg(secondsRemaining));
    const auto remaining=secondsRemaining*1000;
    txProgressInfo_->setText(QString("Elapsed %1 · Remaining %2%3").arg(clockText(elapsed),clockText(remaining),elapsed==txDurationMs_?" · Finishing playback…":""));
}
void MainWindow::endPlaybackProgress(bool success) {
    releaseLocalTransmit();
    if(remoteHostTx_){remoteHostTx_=false;remoteServer_->finishTransmit(success,success?"Shack transmission complete · Ready to receive":"Shack transmission stopped");}
    txProgressTimer_.stop();
    if(!playbackStopped_ && success) {
        txProgress_->setValue(100);txProgress_->setFormat("Playback complete · %p% · 0 s remaining");
        txProgressInfo_->setText(QString("Elapsed %1 · Remaining 0:00").arg(clockText(txDurationMs_)));
        if(txFullScreen_->isVisible()) {
            txFullScreen_->hide();raise();activateWindow();
        }
    } else {
        txProgress_->setFormat(playbackStopped_?"Stopped · %p%":"Playback failed · %p%");
        const auto elapsed=txPlaybackClock_.isValid()?std::min(txDurationMs_,txPlaybackClock_.elapsed()):0;
        txProgressInfo_->setText(QString("%1 · Elapsed %2").arg(playbackStopped_?"Audio playback stopped":"Audio playback failed",clockText(elapsed)));
    }
}
void MainWindow::updateEnabled() {
    updateProfileUi();
    const bool busy=job_.isRunning()||txPreparing_||micCheckActive_,playing=remoteBusy() || (remoteClientMode()&&remoteClient_&&(remoteClient_->snapshot().value("transmitting").toBool()||remoteClient_->snapshot().value("preparing").toBool())) || playback_.state()!=QProcess::NotRunning || radioTxPhase_!=RadioTxPhase::Idle || radio_->pttActive();
    const bool editable=!busy&&!recording_&&!playing;
    const bool remoteAvailable=!remoteClientMode()||(remoteClient_&&remoteClient_->connected());
    for(auto* b:{open_,pattern_,export_,decode_,compose_,templates_})b->setEnabled(editable);
    play_->setEnabled(editable&&remoteAvailable);
    const bool canSaveTx=canMoveTxText()&&!source_.isNull();saveTxImage_->setEnabled(canSaveTx);saveTxImageAction_->setEnabled(canSaveTx);
    pictureInPictureAction_->setEnabled(canSaveTx);addReceivedInset_->setEnabled(canSaveTx&&!received_.isNull());
    record_->setEnabled(remoteAvailable&&(recording_||(!busy&&!playing&&(capture_.state()==QProcess::NotRunning||backgroundReceiving_))));
    saveRx_->setEnabled(!received_.isNull()); stop_->setEnabled(busy||receiveJob_.isRunning()||recording_||playing||backgroundReceiving_||receiveRetry_.isActive());
    audioWizardAction_->setEnabled(!closing_);
    reply_->setEnabled(!remoteMonitorShowing_&&!receivedCall_.isEmpty()&&!busy&&!recording_&&!playing&&radioTxPhase_==RadioTxPhase::Idle&&!radio_->pttActive());
    settingsAction_->setEnabled(!closing_);
    for(auto* button:modeButtons_)button->setEnabled(!busy&&!recording_&&!playing);
    stock_->setEnabled(!busy&&!recording_&&!playing);
    tuxPictures_->setEnabled(canMoveTxText());
    holidayPictures_->setEnabled(canMoveTxText());
    addStock_->setEnabled(canMoveTxText());screenshots_->setEnabled(canMoveTxText());screenshotAction_->setEnabled(canMoveTxText());selfie_->setEnabled(canMoveTxText());selfieAction_->setEnabled(canMoveTxText());removeStock_->setEnabled(canMoveTxText()&&!stock_->selectedItems().isEmpty());
    for(auto* w:std::initializer_list<QWidget*>{txMode_,rxMode_,call_,showCallsignOnImage_,morseAfterTx_,clock_,offset_,channel_,autoClock_,autoFrequency_})w->setEnabled(editable);
    updateTxDragRegions();updateTxUndoActions();
    const auto receiveMode=rxMode_->currentData().toString();
    start_->setEnabled(!busy&&!recording_&&!playing&&!receiveMode.isEmpty());
    const bool editDevices=editable&&!remoteClientMode()&&!remoteHostControlled();
    inputDevice_->setEnabled(editDevices);outputDevice_->setEnabled(editDevices);
    if(radioConnect_){radioConnect_->setEnabled(!remoteBusy()&&displayedRadioConfig().enabled&&!radio_->connected()&&!radio_->busy()&&radioTxPhase_==RadioTxPhase::Idle);radioDisconnect_->setEnabled(radio_->connected()||radio_->pttActive());radioRelease_->setEnabled(radio_->connected()||radio_->pttActive());}
    if(radioFrontend_){radioFrontend_->setLocalControlAllowed(!remoteBusy());radioFrontend_->refresh();}
    if(audioLevels_)audioLevels_->setApplicationState(recording_||backgroundReceiving_,playing,txPreparing_,busy,backgroundReceiving_);
    updateVoiceControls();publishRemoteState();queueReceiveUpdate();
}
void MainWindow::toggleRecording() {
    if(recording_) {
        saveCaptureAudio();
        rawCapture_.remove(0,lastPreviewBytes_);lastPreviewBytes_=0;
        recording_=false;backgroundReceiving_=true;captureLimit_.stop();record_->setText("Record audio");
        if(!job_.isRunning()&&!receiveJob_.isRunning()&&rawCapture_.size()>=6000)decodeCapture(true);
        updateRadioStatus();
        updateEnabled();return;
    }
    if(remoteBusy() || job_.isRunning() || radioTxPhase_!=RadioTxPhase::Idle || playback_.state()!=QProcess::NotRunning || radio_->pttActive())return;
    if(remoteClientMode()&&(!remoteClient_||!remoteClient_->connected()))return;
    const bool promote=backgroundReceiving_&&(remoteClientMode()||(capture_.state()==QProcess::Running&&captureDevice_==inputDevice_->text().trimmed()));
    if(!promote)stopBackgroundReceive();
    finishTxDrag();audioLevels_->stopChecks();if(!promote){captureGain_.reset();signal_->beginInput();}
    viewerTabs_->setCurrentWidget(rxPage_);
    receiveRetry_.stop();receiveRetryDelayMs_=1000;
    if(!promote){++captureGeneration_;rawCapture_.clear();lastPreviewBytes_=0;resetAutoSave();receivedFrameId_=0;setReceivedCall({});receiver_=std::make_shared<sstv::Receiver>(rxOptions(true));}
    backgroundReceiving_=false;stopCaptureRequested_=false;discardCapture_=false;recording_=true;record_->setText("Finish recording");
    if(!promote&&rxMode_->currentData().toString().isEmpty()){rxDetected_->setText("Auto · Waiting for mode header");rxDetected_->setProperty("modeId",QString{});}
    captureDevice_=remoteClientMode()?"Shack PC · "+remoteConfig_.host:inputDevice_->text().trimmed();if(!promote&&!remoteClientMode())capture_.start("arecord",{"-q","-D",captureDevice_,"-t","raw","-f","S16_LE","-r","12000","-c","1"});
    refresh_.start();captureLimit_.start();status_->setText("Recording receiver audio · waiting for mode header · maximum 10 minutes");updateEnabled();
}
void MainWindow::readCapture() {
    if(!recording_&&!backgroundReceiving_){capture_.readAllStandardOutput();return;}
    const auto pcm=capture_.readAllStandardOutput();if(!pcm.isEmpty())captureDataClock_.start();if(remoteServer_&&remoteConfig_.role==remote::Role::Host)remoteServer_->publishPcm(pcm);consumeCapture(pcm);
}
void MainWindow::consumeCapture(const QByteArray& pcm) {
    if(!recording_&&!backgroundReceiving_)return;
    const auto block=captureGain_.push(pcm,audioLevels_->receiveGainDb(),audioLevels_->automaticReceiveLevel());const auto& chunk=block.pcm;audioLevels_->receiveLevels(block);signal_->pushPcm(chunk);
    if(remoteClientMode()&&remoteSpeakerPlayer_&&remoteSpeakerPlayer_->active())remoteSpeakerPlayer_->push(chunk);
    if(!chunk.isEmpty()){receiveRetryDelayMs_=1000;receiveError_.clear();}
    if(backgroundReceiving_){
        if(rawCapture_.size()+chunk.size()>12000*2*600){stopBackgroundReceive();receiveInputFailed("Receive decoder fell behind · restarting input");return;}
        rawCapture_+=chunk;
        // The station summary belongs to updateRadioStatus, including local RX.
        // Replacing it per packet competed with Hamlib polling and resized the form.
        if(!remoteClientMode()){
            receiveState_->setText("Receiving automatically · "+captureDevice_+(block.input.samples&&block.input.peak==0?" · silence":""));
        }
        return;
    }
    if(rawCapture_.size()+chunk.size()>12000*2*600) {if(recording_)toggleRecording();return;}
    rawCapture_+=chunk;
    if(recording_){const auto text=QString("Recording %1 s · mono 12000 Hz\nInput · %2").arg(rawCapture_.size()/24000.,0,'f',1).arg(captureDevice_);recordInfo_->setText(text);recordInfo_->setToolTip(text);}
}
void MainWindow::decodeCapture(bool partial) {
    // Feed new audio promptly; the receiver validates the header and frame length.
    if(job_.isRunning() || receiveJob_.isRunning() || rawCapture_.size()<6000) {if(!partial)status_->setText("Recording is too short to decode.");updateEnabled();return;}
    const int begin=receiver_?lastPreviewBytes_:0;
    const int available=int(rawCapture_.size()/2*2);
    sstv::Audio audio;audio.sampleRate=12000;audio.samples.resize(size_t((available-begin)/2));
    for(size_t i=0;i<audio.samples.size();++i)audio.samples[i]=qFromLittleEndian<qint16>(rawCapture_.constData()+begin+i*2)/32768.f;
    lastPreviewBytes_=available;const auto options=rxOptions(partial);
    // The incremental receiver retains its own header/picture state. Background
    // RX only queues new samples; optional WAV recording retains the full audio.
    if(backgroundReceiving_){rawCapture_.remove(0,available);lastPreviewBytes_=0;}
    if(!partial) {
        saveCaptureAudio();
        status_->setText("Decoding recorded receiver audio…");
    }
    const auto receiver=receiver_;const auto detected=modeNotification(options.forceMode.isEmpty(),true);const auto generation=captureGeneration_;
    beginJob([audio=std::move(audio),options,partial,receiver,detected,generation](sstv::Cancel cancel,sstv::Progress progress)mutable {
        JobResult r;r.receive=true;r.live=partial;r.captureGeneration=generation;
        if(receiver){r.captureReceive=true;r.frames=receiver->push(audio,cancel,progress,detected);r.detectedMode=receiver->modeId();r.morse=receiver->morse();}
        else r.decoded=sstv::decode(audio,options,cancel,progress,detected);
        r.audio=std::move(audio);return r;
    },true);
}
void MainWindow::stopAll() {
    if(micCheckDialog_)micCheckDialog_->reject();
    cancelAudioWizard();
    stopVoice();
    if(remoteServer_)remoteServer_->finishVoice("Voice stopped");
    stopVoiceOutput();
    localEchoPlaybackOk_=false;
    if(remoteMonitor_)remoteMonitor_->stop();
    releaseLocalTransmit();
    if(remoteServer_&&remoteConfig_.role==remote::Role::Host)remoteServer_->abortTransmit("Stopped at shack PC");
    if(remoteClientMode()&&remoteClient_)remoteClient_->stopTransmit();
    remoteManualKey_=false;
    receiveRetry_.stop();receiveRetryDelayMs_=1000;++captureGeneration_;
    audioLevels_->stopChecks();audioLevels_->endTransmitMeter();outputMeterClock_.invalidate();signal_->endInput();
    const bool radioActive=radioTxPhase_!=RadioTxPhase::Idle || radio_->pttActive();++radioTxGeneration_;radioLead_.stop();radioTail_.stop();radioWatchdog_.stop();
    if(cancel_)cancel_->store(true);
    if(receiveCancel_)receiveCancel_->store(true);
    if(txPreparing_) {
        txPreparing_=false;txProgress_->setFormat("Preparation stopped · %p%");
        txProgressInfo_->setText("Audio preparation stopped");
    }
    if(playback_.state()!=QProcess::NotRunning || radioActive) {
        playbackStopped_=true;endPlaybackProgress(false);
    }
    stopCaptureRequested_=false;
    if(recording_ || capture_.state()!=QProcess::NotRunning) {stopCaptureRequested_=false;discardCapture_=true;recording_=false;capture_.kill();refresh_.stop();captureLimit_.stop();record_->setText("Record audio");}
    backgroundReceiving_=false;
    playbackStopped_=true;playback_.kill();status_->setText("Stopped");if(radioActive&&!closing_)releaseRadioTx(false,txStopReason_.isEmpty()?QString("Stopped · Radio PTT released"):txStopReason_);txStopReason_.clear();if(!audioWizardRestoreError_.isEmpty())status_->setText("Stopped · "+audioWizardRestoreError_);updateEnabled();
}
void MainWindow::saveTransmitImage(){
    if(!canMoveTxText()||source_.isNull())return;
    QWidget* owner=txFullScreen_->isVisible()?txFullScreen_:this;
    const auto restoreFocus=qScopeGuard([this,owner]{if(owner==txFullScreen_&&txFullScreen_->isVisible()){txFullScreen_->raise();txFullScreen_->activateWindow();}});
    auto image=transmitImage();if(image.isNull())return;
    const auto mode=txMode_->currentData().toString();const auto savedUtc=QDateTime::currentDateTimeUtc();
    image.setText("Mode",mode);image.setText("Callsign",call_->text().trimmed().toUpper());image.setText("SavedUTC",savedUtc.toString(Qt::ISODateWithMs));image.setText("Software","Tux-sstv");
    QSettings settings;const auto pictures=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    auto directory=settings.value("tx/imageSaveDirectory",pictures+"/OpenSSTV/Transmit").toString();
    if(!QDir(directory).exists()&&!QDir().mkpath(directory))directory=pictures;
    const auto name=QString("TX-%1-%2.png").arg(savedUtc.toString("yyyyMMdd-HHmmss-zzz"),mode);
    QFileDialog dialog(owner,"Save transmit image",QDir(directory).filePath(name),"PNG image (*.png)");dialog.setCursor(Qt::ArrowCursor);
    dialog.setObjectName("saveTransmitImageDialog");
    dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setFileMode(QFileDialog::AnyFile);dialog.setDefaultSuffix("png");
    // Confirm the final PNG path after normalizing its suffix, so a typed name
    // such as picture.jpg cannot silently replace picture.jpg.png.
    dialog.setOption(QFileDialog::DontConfirmOverwrite);
    if(dialog.exec()!=QDialog::Accepted)return;
    auto path=dialog.selectedFiles().value(0);if(path.isEmpty())return;
    if(!path.endsWith(".png",Qt::CaseInsensitive))path+=".png";
    path=QFileInfo(path).absoluteFilePath();
    if(QFileInfo::exists(path)){
        QMessageBox replace(QMessageBox::Question,"Replace image?",QFileInfo(path).fileName()+" already exists. Replace it?",QMessageBox::Yes|QMessageBox::No,owner);replace.setDefaultButton(QMessageBox::No);replace.setCursor(Qt::ArrowCursor);
        if(replace.exec()!=QMessageBox::Yes)return;
    }
    const auto failure=[this,&path](const QString& error){status_->setText("Could not save TX image: "+path+" · "+error);};
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)){failure(file.errorString());return;}
    QImageWriter writer(&file,"png");if(!writer.write(image)){file.cancelWriting();failure(writer.errorString());return;}
    if(!file.commit()){failure(file.errorString());return;}
    settings.setValue("tx/imageSaveDirectory",QFileInfo(path).absolutePath());
    stockPaths_.removeAll(path);stockPaths_.prepend(path);loadStock();saveSettings();galleryTabs_->setCurrentIndex(0);
    status_->setText("TX image saved: "+path);
}
void MainWindow::saveReceived() {
    if(received_.isNull())return;
    auto path=QFileDialog::getSaveFileName(this,remoteMonitorShowing_?"Save remote TX monitor image":"Save received image copy",historyPath_,"PNG image (*.png)");if(path.isEmpty())return;if(!path.endsWith(".png",Qt::CaseInsensitive))path+=".png";
    if(!received_.save(path,"PNG")){status_->setText("Could not save image: "+path);return;}
    status_->setText("Image saved: "+path);
}
void MainWindow::resetAutoSave() {
    autoSaveKey_.clear();autoSavePath_.clear();autoSavedImage_=QImage();autoSavedComplete_=false;pendingReceiveSave_={};
}
void MainWindow::autoSaveReceived(const sstv::DecodeResult& frame,const QString& key) {
    if(frame.rows<=0 || frame.image.isNull())return;
    if(key!=autoSaveKey_) {resetAutoSave();autoSaveKey_=key;}
    if(!autoSavePath_.isEmpty() && frame.image==autoSavedImage_ && frame.complete==autoSavedComplete_)return;
    pendingReceiveSave_=frame;
    const auto failure=[this](const QString& message) {
        autoSaveInfo_->setText("Auto-save failed: "+message);
        setAutoSaveState(false);autoSaveInfo_->setToolTip(historyPath_);
    };
    if(!QDir().mkpath(historyPath_)){failure("Cannot create "+historyPath_);return;}
    if(autoSavePath_.isEmpty()) {
        const auto base=historyPath_+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+frame.modeId;
        autoSavePath_=base+".png";
        for(int suffix=1;QFileInfo::exists(autoSavePath_);++suffix)autoSavePath_=base+QString("-%1.png").arg(suffix);
    }
    QSaveFile file(autoSavePath_);
    if(!file.open(QIODevice::WriteOnly)){failure(file.errorString());return;}
    auto savedImage=frame.image;savedImage.setText("Mode",frame.modeId);savedImage.setText("ReceivedUTC",receivedUtc_.toUTC().toString(Qt::ISODateWithMs));savedImage.setText("FrequencyHz",QString::number(receivedFrequencyHz_,'f',0));
    const auto savedCall=frame.callsign.isEmpty()?receivedCall_:frame.callsign;
    if(!savedCall.isEmpty()){savedImage.setText("Callsign",savedCall);savedImage.setText("CallsignSource",frame.callsign.isEmpty()?receivedCallSource_:QString("FSKID"));}
    if(!savedImage.save(&file,"PNG")){failure("Could not encode "+autoSavePath_);return;}
    if(!file.commit()){failure(file.errorString());return;}
    autoSavedImage_=frame.image;autoSavedComplete_=frame.complete;pendingReceiveSave_={};
    autoSaveInfo_->setText("Saved automatically: "+QFileInfo(autoSavePath_).fileName());
    autoSaveInfo_->setToolTip(autoSavePath_);setAutoSaveState(true);
    for(int i=0;i<history_->count();++i) {
        auto* item=history_->item(i);
        if(item->data(Qt::UserRole).toString()==autoSavePath_) {
            item->setIcon(QIcon(QPixmap::fromImage(frame.image.scaled(112,84,Qt::KeepAspectRatio,Qt::SmoothTransformation))));
            return;
        }
    }
    loadHistory();
}
void MainWindow::loadHistory() {
    history_->clear();
    const auto entries=QDir(historyPath_).entryInfoList({"*.png"},QDir::Files,QDir::Time);
    int count=0;
    for(const auto& entry:entries) {
        if(count++>=256)break;
        QImageReader reader(entry.absoluteFilePath());reader.setScaledSize(QSize(112,84));const auto image=reader.read();
        auto* item=new QListWidgetItem(QIcon(QPixmap::fromImage(image)),entry.fileName(),history_);
        item->setData(Qt::UserRole,entry.absoluteFilePath());item->setToolTip(entry.absoluteFilePath());
    }
    if(entries.isEmpty()){auto* item=new QListWidgetItem("No received images yet",history_);item->setFlags(Qt::NoItemFlags);}
}

void MainWindow::showMorse(const sstv::MorseReading& reading){
    if(reading.text.isEmpty())return;
    if(morseText_->text()!=reading.text){morseText_->setText(reading.text);morseText_->setCursorPosition(reading.text.size());}
    morseCandidate_=reading.candidate;
    useMorseCall_->setEnabled(sstv::validCallsign(morseCandidate_)&&morseCandidate_!=receivedCall_);
    useMorseCall_->setToolTip(morseCandidate_.isEmpty()?"Waiting for a complete Morse callsign":"Confirm "+morseCandidate_+" from Morse for Reply and Log contact. Check the decoded text first.");
    if(reading.wpm>0)morseStatus_->setText(QString("%1 Hz · %2 WPM").arg(reading.toneHz,0,'f',0).arg(reading.wpm,0,'f',0));
}
void MainWindow::setReceivedCall(const QString& call,bool attachToImage,const QString& source) {
    receivedCall_=sstv::validCallsign(call)?call:QString{};
    receivedCallSource_=receivedCall_.isEmpty()?QString{}:source.isEmpty()?QString("FSKID"):source;
    receivedCallLabel_->setText(receivedCall_.isEmpty()?"Received callsign · waiting for FSKID or Morse":"Received callsign · "+receivedCall_+(receivedCallSource_=="Morse (confirmed)"?" · Morse (confirmed)":""));
    // A trailer can arrive after the completed image was saved. Preserve the
    // image and attach the validated callsign to its PNG metadata atomically.
    if(attachToImage && !receivedCall_.isEmpty() && !autoSavePath_.isEmpty()) {
        QImage image(autoSavePath_);
        if(!image.isNull() && (image.text("Callsign")!=receivedCall_||image.text("CallsignSource")!=receivedCallSource_)) {
            image.setText("Callsign",receivedCall_);image.setText("CallsignSource",receivedCallSource_);QSaveFile file(autoSavePath_);
            if(!file.open(QIODevice::WriteOnly) || !image.save(&file,"PNG") || !file.commit()){autoSaveInfo_->setText("Callsign metadata save failed: "+file.errorString());setAutoSaveState(false);}
        }
    }
    updateEnabled();
}
void MainWindow::replyToReceived() {
    if(!reply_->isEnabled() || receivedCall_.isEmpty())return;
    finishTxDrag();const auto before=txEditState();clearImageFields();templateFields_.design="reply";templateFields_.toCall=receivedCall_;templateEnabled_=true;
    viewerTabs_->setCurrentWidget(txPage_);updateTx();status_->setText("Reply ready for "+receivedCall_+" · add a picture or edit Templates");recordTxEdit(before,"Create reply card");
}
