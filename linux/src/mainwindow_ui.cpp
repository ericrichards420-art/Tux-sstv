#include "mainwindow.h"
#include "branding.h"
#include "imageview.h"
#include "signalview.h"
#include "core/morse.h"
#include "core/fskid.h"
#include "skins.h"
#include "composer.h"
#include "radio_ui.h"
#include "radio_frontend.h"
#include "audio_levels_panel.h"
#include "templatesdialog.h"
#include "audio_wizard.h"
#include "contactlog_ui.h"
#include "core/patterns.h"
#include "patternsdialog.h"
#include "captionpreview.h"
#include "insetdialog.h"
#include <QScopeGuard>
#include <QUndoStack>
#include <QJsonArray>
#include <QJsonDocument>
#include <QResource>
#include <QtWidgets>

static void initTuxPictureResources(){
    static const bool initialized=[]{Q_INIT_RESOURCE(tux_pictures);return true;}();
    Q_UNUSED(initialized);
}

namespace {
void thumbnailList(QListWidget* list) {
    list->setViewMode(QListView::IconMode);
    list->setFlow(QListView::LeftToRight);
    list->setWrapping(false);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setIconSize(QSize(112,84));
    list->setGridSize(QSize(136,110));
    list->setSpacing(3);
    list->setUniformItemSizes(true);
    list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setMinimumHeight(122);
}
}
void MainWindow::buildUi(int implementedCount,int targetCount) {
    setWindowTitle("Tux-sstv — SSTV station");resize(1000,720);setMinimumSize(900,680);
    setStyleSheet(ui::skinStyleSheet(ui::skinById("classic")));
    setWindowIcon(branding::icon());
    auto* root=new QWidget;
    auto* layout=new QVBoxLayout(root);layout->setContentsMargins(6,4,6,5);layout->setSpacing(5);
    auto* split=new QSplitter(Qt::Horizontal);split->setObjectName("operatingSplitter");split->setChildrenCollapsible(false);
    viewerTabs_=new QTabWidget;viewerTabs_->setObjectName("imageTabs");
    rxPage_=new QWidget;rxPage_->setObjectName("receivePage");auto* rx=new QVBoxLayout(rxPage_);rx->setContentsMargins(6,6,6,6);rx->setSpacing(5);
    rxView_=new ImageView("Waiting for an SSTV image\n\nOpen an RX WAV or record receiver audio");rxView_->setObjectName("receiveImage");rx->addWidget(rxView_,1);
    rxDetected_=new QLabel("Auto · Waiting for mode header");rxDetected_->setObjectName("detectedReceiveMode");
    rx->addWidget(rxDetected_);
    rxInfo_=new QLabel("RX · Waiting for audio");rx->addWidget(rxInfo_);
    autoSaveInfo_=new QLabel;autoSaveInfo_->setObjectName("receiveAutoSaveInfo");autoSaveInfo_->setTextFormat(Qt::PlainText);autoSaveInfo_->setWordWrap(true);rx->addWidget(autoSaveInfo_);
    auto* idRow=new QHBoxLayout;receivedCallLabel_=new QLabel("Received callsign · waiting for FSKID or Morse");receivedCallLabel_->setTextFormat(Qt::PlainText);receivedCallLabel_->setObjectName("receivedCallsign");
    reply_=new QPushButton("Reply");reply_->setObjectName("replyReceived");connect(reply_,&QPushButton::clicked,this,&MainWindow::replyToReceived);idRow->addWidget(receivedCallLabel_,1);idRow->addWidget(reply_);rx->addLayout(idRow);
    addReceivedInset_=new QPushButton("Add to TX");addReceivedInset_->setObjectName("addReceivedInsetButton");addReceivedInset_->setToolTip("Place this received picture inside your TX image to show how it came through.");idRow->addWidget(addReceivedInset_);connect(addReceivedInset_,&QPushButton::clicked,this,[this]{showPictureInPicture(received_);});
    auto* morseRow=new QHBoxLayout;morseRow->addWidget(new QLabel("Morse"));
    morseText_=new QLineEdit;morseText_->setObjectName("receivedMorseText");morseText_->setReadOnly(true);morseText_->setPlaceholderText("Listening automatically for a Morse callsign…");morseText_->setToolTip("Decoded CW from the receive audio. Select text to copy it. Best with a clear, single Morse tone; check the text before using a callsign.");morseRow->addWidget(morseText_,1);
    morseStatus_=new QLabel("Auto tone / speed");morseStatus_->setObjectName("receivedMorseStatus");morseRow->addWidget(morseStatus_);
    useMorseCall_=new QPushButton("Use callsign");useMorseCall_->setObjectName("useMorseCallsign");useMorseCall_->setEnabled(false);useMorseCall_->setToolTip("Confirm the decoded Morse callsign for Reply and Log contact. It is not a checksum-validated FSKID.");morseRow->addWidget(useMorseCall_);rx->addLayout(morseRow);
    connect(useMorseCall_,&QPushButton::clicked,this,[this]{if(!sstv::validCallsign(morseCandidate_))return;setReceivedCall(morseCandidate_,true,"Morse (confirmed)");useMorseCall_->setEnabled(false);});
    auto* rxControls=new QHBoxLayout;decode_=new QPushButton("Open RX WAV…");decode_->setObjectName("decodeFileButton");saveRx_=new QPushButton("Save copy…");
    auto* fullScreen=new QPushButton("Full screen");fullScreen->setObjectName("receiveFullScreenButton");
    fullScreen->setToolTip("Show the receiving image full screen. Press Esc to return.");
    auto* logContactButton=new QPushButton("Log contact…");logContactButton->setObjectName("logContactButton");connect(logContactButton,&QPushButton::clicked,this,&MainWindow::logContact);rxControls->addWidget(logContactButton);rxControls->addWidget(decode_);rxControls->addWidget(saveRx_);rxControls->addStretch();rxControls->addWidget(fullScreen);rx->addLayout(rxControls);
    // This viewer persists across completed images and idle periods. Only its
    // Escape shortcut hides it; receive updates never change window visibility.
    rxFullScreen_=new QWidget(this,Qt::Window);rxFullScreen_->setObjectName("receiveFullScreenWindow");
    rxFullScreen_->setWindowTitle("Tux-sstv · Received image");
    rxFullScreen_->setCursor(Qt::BlankCursor);
    rxFullScreen_->setStyleSheet("QWidget#receiveFullScreenWindow {background:#101010;}");
    auto* exitFullScreen=new QShortcut(QKeySequence(Qt::Key_Escape),rxFullScreen_);
    exitFullScreen->setAutoRepeat(false);
    connect(exitFullScreen,&QShortcut::activated,rxFullScreen_,&QWidget::hide);
    auto* fullLayout=new QVBoxLayout(rxFullScreen_);fullLayout->setContentsMargins(0,0,0,0);fullLayout->setSpacing(0);
    rxFullScreenView_=new ImageView("Waiting for an SSTV image",rxFullScreen_);rxFullScreenView_->setObjectName("receiveFullScreenImage");
    rxFullScreenView_->setStyleSheet("background:#101010;color:#b8b8b8;border:0;");fullLayout->addWidget(rxFullScreenView_,1);
    connect(fullScreen,&QPushButton::clicked,this,[this] {
        txFullScreen_->hide();
        if(!received_.isNull())rxFullScreenView_->setImage(received_);
        rxFullScreen_->showFullScreen();rxFullScreen_->raise();rxFullScreen_->activateWindow();
    });
    viewerTabs_->addTab(rxPage_,"RX");

    txPage_=new QWidget;txPage_->setObjectName("transmitPage");auto* tx=new QVBoxLayout(txPage_);tx->setContentsMargins(6,6,6,6);tx->setSpacing(5);
    txView_=new CaptionPreview;txView_->setObjectName("transmitImage");tx->addWidget(txView_,1);
    auto* txInfoRow=new QWidget;auto* txInfoRows=new QVBoxLayout(txInfoRow);txInfoRows->setContentsMargins(0,0,0,0);txInfoRows->setSpacing(5);
    auto* txInfoLayout=new QHBoxLayout;txInfo_=new QLabel;txInfo_->setObjectName("transmitInfo");txInfo_->setWordWrap(true);txInfoLayout->addWidget(txInfo_,1);txInfoRows->addLayout(txInfoLayout);
    saveTxImage_=new QPushButton("Save image…");saveTxImage_->setObjectName("saveTransmitImageButton");saveTxImage_->setAccessibleName("Save transmit image");saveTxImage_->setToolTip("Save the finished TX picture as a PNG, including callsign, text and emoji.");txInfoLayout->addWidget(saveTxImage_);connect(saveTxImage_,&QPushButton::clicked,this,&MainWindow::saveTransmitImage);
    templates_=new QPushButton("Templates…");templates_->setObjectName("templatesButton");txInfoLayout->addWidget(templates_);connect(templates_,&QPushButton::clicked,this,&MainWindow::showTemplates);
    compose_=new QPushButton("Text & emoji…");compose_->setObjectName("compositionButton");txInfoLayout->addWidget(compose_);tx->addWidget(txInfoRow);
    connect(compose_,&QPushButton::clicked,this,&MainWindow::showComposer);
    txProgressPanel_=new QWidget;txProgressPanel_->setObjectName("transmitProgressPanel");
    auto* txProgressLayout=new QVBoxLayout(txProgressPanel_);txProgressLayout->setContentsMargins(0,0,0,0);txProgressLayout->setSpacing(5);
    auto* undoTools=new QHBoxLayout;undoTools->setSpacing(4);undoTxButton_=new QPushButton("Undo");undoTxButton_->setObjectName("undoTransmitButton");redoTxButton_=new QPushButton("Redo");redoTxButton_->setObjectName("redoTransmitButton");undoTools->addWidget(undoTxButton_);undoTools->addWidget(redoTxButton_);undoTools->addStretch();txProgressLayout->addLayout(undoTools);
    connect(undoTxButton_,&QPushButton::clicked,this,&MainWindow::undoTx);connect(redoTxButton_,&QPushButton::clicked,this,&MainWindow::redoTx);
    txProgress_=new QProgressBar;txProgress_->setObjectName("transmitProgress");txProgress_->setRange(0,100);txProgress_->setValue(0);
    txProgress_->setFormat("Ready to send · %p%");txProgress_->setAlignment(Qt::AlignCenter);txProgress_->setFixedHeight(28);
    txProgress_->setTextVisible(true);txProgress_->setAccessibleName("Transmit image progress");
    txProgress_->setToolTip("The seconds countdown estimates remaining playback time, including Morse ID. The bar completes after successful audio playback.");txProgressLayout->addWidget(txProgress_);
    txProgressInfo_=new QLabel("Elapsed 0:00 · Remaining —");txProgressInfo_->setObjectName("transmitProgressInfo");txProgressLayout->addWidget(txProgressInfo_);
    tx->addWidget(txProgressPanel_);
    auto* txControls=new QHBoxLayout;open_=new QPushButton("Load picture…");pattern_=new QPushButton("Test patterns…");pattern_->setObjectName("testPatternsButton");export_=new QPushButton("Save TX WAV…");play_=new QPushButton("TX audio");
    for(auto* button:{open_,pattern_,export_,play_})txControls->addWidget(button);
    auto* txFullScreenButton=new QPushButton("Full screen");txFullScreenButton->setObjectName("transmitFullScreenButton");
    txFullScreenButton->setToolTip("Show the sending image and progress full screen. Space starts TX; Esc returns to the normal layout.");txControls->addWidget(txFullScreenButton);
    tx->addLayout(txControls);
    txFullScreen_=new QWidget(this,Qt::Window);txFullScreen_->setObjectName("transmitFullScreenWindow");
    txFullScreen_->setWindowTitle("Tux-sstv · Sending image");txFullScreen_->setCursor(Qt::BlankCursor);
    txFullScreen_->setStyleSheet("QWidget#transmitFullScreenWindow {background:#101010;}");
    auto* txFullLayout=new QVBoxLayout(txFullScreen_);txFullLayout->setContentsMargins(0,0,0,0);txFullLayout->setSpacing(0);
    txFullScreenView_=new CaptionPreview(txFullScreen_);txFullScreenView_->setPointerHidden(true);txFullScreenView_->setObjectName("transmitFullScreenImage");
    txFullScreenView_->setStyleSheet("background:#101010;border:0;");txFullLayout->addWidget(txFullScreenView_,1);
    const auto moveText=[this](const QString& key,QPointF position){
        if(!canMoveTxText())return;
        const auto before=txEditState();const bool standalone=!txDragBefore_;
        const int index=sstv::captionIndex(key);
        if(index>=0&&index<captions_.size()){captions_[index].freePosition=true;captions_[index].relativePosition=position;sstv::bringCaptionToFront(captions_,index);}
        else if(key=="inset")inset_.relativePosition=position;
        else if(key=="callsign"){
            if(templateEnabled_)templateFields_.textPositions["callsign"]=position;
            else {callsignPlacement_.freePosition=true;callsignPlacement_.relativePosition=position;}
        }else return;
        updateImages();if(standalone)recordTxEdit(before,key=="inset"?"Move received picture":key=="callsign"?"Move callsign":"Move text");
    };
    txView_->textMoved=moveText;txFullScreenView_->textMoved=moveText;
    for(auto* preview:{txView_,txFullScreenView_}){
        connect(preview,&CaptionPreview::editStarted,this,&MainWindow::beginTxDrag);connect(preview,&CaptionPreview::editFinished,this,&MainWindow::finishTxDrag);
        preview->setToolTip({});preview->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(preview,&QWidget::customContextMenuRequested,this,[this,preview](QPoint point){
            if(!canMoveTxText())return;
            QMenu menu(preview);menu.addAction(undoTxAction_);menu.addAction(redoTxAction_);menu.addSeparator();menu.setMouseTracking(true);menu.installEventFilter(this);const int index=sstv::captionIndex(preview->textKeyAt(point));
            if(index>=0&&index<captions_.size()){
                menu.addAction("Edit text & emoji…",this,[this,index]{showCaptionEditor(index);});
                menu.addAction("Remove text block",this,[this,index]{if(canMoveTxText()&&index<captions_.size()){const auto before=txEditState();captions_.removeAt(index);updateTx();recordTxEdit(before,"Remove text");}});
            }
            if(preview->textKeyAt(point)=="inset")menu.addAction("Remove received inset",this,[this]{if(canMoveTxText()){const auto before=txEditState();inset_={};updateTx();recordTxEdit(before,"Remove received picture");}});
            if(!callsignBounds_.isEmpty()){
                if(!menu.isEmpty())menu.addSeparator();
                menu.addAction("Reset callsign position",this,&MainWindow::resetCallsignPosition);
            }
            if(!menu.isEmpty())menu.addSeparator();
            auto* showCall=menu.addAction("Callsign on image");showCall->setObjectName("txMenuShowCallsign");showCall->setCheckable(true);showCall->setChecked(showCallsignOnImage_->isChecked());connect(showCall,&QAction::toggled,showCallsignOnImage_,&QCheckBox::setChecked);
            auto* morse=menu.addAction("Morse ID after TX");morse->setObjectName("txMenuMorseId");morse->setCheckable(true);morse->setChecked(morseAfterTx_->isChecked());connect(morse,&QAction::toggled,morseAfterTx_,&QCheckBox::setChecked);menu.addSeparator();
            menu.addAction("Picture in picture…",this,[this]{showPictureInPicture();});
            menu.addAction("Save TX image…",this,&MainWindow::saveTransmitImage);
            if(!menu.isEmpty())menu.exec(preview->mapToGlobal(point));
        });
    }
    auto* exitTxFullScreen=new QShortcut(QKeySequence(Qt::Key_Escape),txFullScreen_);exitTxFullScreen->setAutoRepeat(false);
    connect(exitTxFullScreen,&QShortcut::activated,txFullScreen_,&QWidget::hide);
    auto* startTxFullScreen=new QShortcut(QKeySequence(Qt::Key_Space),txFullScreen_);
    startTxFullScreen->setContext(Qt::WindowShortcut);startTxFullScreen->setAutoRepeat(false);
    connect(startTxFullScreen,&QShortcut::activated,this,[this]{if(play_->isEnabled())play_->click();});
    auto* saveTxFullScreen=new QShortcut(QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_S),txFullScreen_);saveTxFullScreen->setAutoRepeat(false);connect(saveTxFullScreen,&QShortcut::activated,this,&MainWindow::saveTransmitImage);
    txFullScreen_->installEventFilter(this);
    txPointerHide_.setSingleShot(true);txPointerHide_.setInterval(3000);txPointerHide_.setTimerType(Qt::PreciseTimer);
    connect(&txPointerHide_,&QTimer::timeout,this,&MainWindow::hideTxPointer);
    connect(txFullScreenButton,&QPushButton::clicked,this,[this,txFullLayout] {
        rxFullScreen_->hide();txFullScreenView_->setImage(transmitImage());
        // Move the actual progress display, retaining every playback state.
        txFullLayout->addWidget(txProgressPanel_);txProgressPanel_->show();prepareTxPointerTracking();
        txFullScreen_->showFullScreen();txFullScreen_->raise();txFullScreen_->activateWindow();
    });
    viewerTabs_->addTab(txPage_,"TX");

    syncPage_=new QWidget;syncPage_->setObjectName("syncPage");auto* sync=new QFormLayout(syncPage_);sync->setContentsMargins(14,16,14,12);
    auto* syncTitle=new QLabel("Receive synchronization and audio corrections");syncTitle->setStyleSheet("font-weight:bold;");sync->addRow(syncTitle);
    channel_=new QComboBox;channel_->addItems({"Left / mono","Right (stereo WAV)","Average (stereo WAV)"});sync->addRow("WAV channel",channel_);
    autoClock_=new QCheckBox("Automatically straighten received images");autoClock_->setObjectName("autoClock");sync->addRow(autoClock_);
    autoFrequency_=new QCheckBox("Automatically fine tune received tones (AFC)");autoFrequency_->setObjectName("autoFrequency");sync->addRow(autoFrequency_);
    clock_=new QDoubleSpinBox;clock_->setRange(-10000,10000);clock_->setDecimals(1);clock_->setSuffix(" ppm");sync->addRow("Sample clock",clock_);
    offset_=new QDoubleSpinBox;offset_->setRange(-200,200);offset_->setDecimals(1);offset_->setSuffix(" Hz");sync->addRow("Tone offset",offset_);
    start_=new QDoubleSpinBox;start_->setObjectName("receiveStart");start_->setRange(-1,600000);start_->setDecimals(2);start_->setSpecialValueText("Auto · VIS header");start_->setValue(-1);start_->setSuffix(" ms");sync->addRow("Image start",start_);
    auto* syncHelp=new QLabel("For a manual start, select a legacy RX mode and enter the picture start after VIS and any AVT countdown. Positive sample clock correction means more recorded samples per second; positive tone offset means higher received tones. Nearby line sync pulses are tracked in legacy modes. Enable automatic correction to measure clock drift from line syncs and tone offset from a valid VIS leader. Until a reliable measurement is available, the manual values apply. AVT uses the manual clock. Automatic correction is opt-in while on-air validation continues.");syncHelp->setWordWrap(true);sync->addRow(syncHelp);
    viewerTabs_->addTab(syncPage_,"Sync");viewerTabs_->setCurrentWidget(txPage_);split->addWidget(viewerTabs_);
    radioPage_=new QWidget;radioPage_->setObjectName("radioPage");auto* radioLayout=new QVBoxLayout(radioPage_);radioLayout->setContentsMargins(0,0,0,0);
    radioFrontend_=new RadioFrontend(radio_,radioConfig_,[this]{return !remoteBusy()&&!micCheckActive_&&!job_.isRunning()&&!txPreparing_&&!recording_&&playback_.state()==QProcess::NotRunning&&radioTxPhase_==RadioTxPhase::Idle;},radioPage_);radioLayout->addWidget(radioFrontend_);viewerTabs_->addTab(radioPage_,"Radio");
    connect(radioFrontend_,&RadioFrontend::connectRequested,this,&MainWindow::connectRadio);connect(radioFrontend_,&RadioFrontend::disconnectRequested,this,[this]{radioDisconnect_->trigger();});connect(radioFrontend_,&RadioFrontend::stopRequested,this,&MainWindow::stopRadioTransmission);connect(radioFrontend_,&RadioFrontend::settingsRequested,this,[this]{showSettingsPage(1);});connect(radioFrontend_,&RadioFrontend::advancedRequested,this,&MainWindow::showAdvancedRadioControls);

    auto* right=new QWidget;right->setMinimumWidth(340);right->setMaximumWidth(430);
    operatingControls_=right;right->setObjectName("operatingControls");
    auto* rightLayout=new QVBoxLayout(right);rightLayout->setContentsMargins(5,0,0,0);rightLayout->setSpacing(4);
    auto* controlsScroll=new QScrollArea;controlsScroll->setObjectName("operatingControlsScroll");controlsScroll->setWidgetResizable(true);controlsScroll->setFrameShape(QFrame::NoFrame);rightLayout->addWidget(controlsScroll,1);
    auto* controlsBody=new QWidget;controlsBody->setObjectName("operatingControlsBody");controlsScroll->setWidget(controlsBody);auto* controls=new QVBoxLayout(controlsBody);controls->setContentsMargins(0,0,0,0);controls->setSpacing(4);controls->setSizeConstraint(QLayout::SetMinimumSize);
    auto* spectrumBox=new QGroupBox("Audio monitor · Hz");auto* spectrumLayout=new QVBoxLayout(spectrumBox);spectrumLayout->setContentsMargins(4,13,4,4);
    receiveState_=new QLabel("Starting receiver…");receiveState_->setObjectName("liveAudioState");receiveState_->setTextFormat(Qt::PlainText);receiveState_->setWordWrap(true);receiveState_->setFixedHeight(receiveState_->fontMetrics().lineSpacing()*2+4);receiveState_->setMinimumWidth(0);receiveState_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);spectrumLayout->addWidget(receiveState_);
    spectrumBox->setObjectName("audioMonitor");auto* monitorTools=new QHBoxLayout;monitorTools->setSpacing(4);auto* monitorView=new QComboBox;monitorView->setObjectName("audioMonitorView");monitorView->addItems({"Waterfall","Spectrum"});monitorView->setCurrentIndex(QSettings().value("ui/audioView","waterfall").toString()=="spectrum"?1:0);auto* freeze=new QPushButton("Freeze");freeze->setObjectName("audioMonitorFreeze");freeze->setCheckable(true);auto* clear=new QPushButton("Clear");clear->setObjectName("audioMonitorClear");monitorTools->addWidget(monitorView,1);monitorTools->addWidget(freeze);monitorTools->addWidget(clear);spectrumLayout->addLayout(monitorTools);
    signal_=new SignalView;signal_->setObjectName("signalScope");signal_->setMinimumHeight(110);signal_->setMaximumHeight(150);signal_->setWaterfall(monitorView->currentIndex()==0);spectrumLayout->addWidget(signal_);controls->addWidget(spectrumBox);
    connect(monitorView,&QComboBox::currentIndexChanged,this,[this,monitorView](int index){signal_->setWaterfall(index==0);QSettings().setValue("ui/audioView",index==0?"waterfall":"spectrum");});connect(freeze,&QPushButton::toggled,signal_,&SignalView::setFrozen);connect(signal_,&SignalView::freezeChanged,freeze,&QPushButton::setChecked);connect(clear,&QPushButton::clicked,signal_,&SignalView::clearHistory);freeze->setToolTip("Hold the display while reception continues. Uncheck to return to live audio.");clear->setToolTip("Clear the display history without stopping reception.");

    rxMode_=new QComboBox;rxMode_->addItem("Auto detect · All modes",QString{});txMode_=new QComboBox;
    rxMode_->setToolTip("Automatically detects supported VIS, extended VIS and N-VIS headers.");
    for(const auto& mode:sstv::modes())if(mode.implemented){const auto name=mode.name;rxMode_->addItem(name,mode.id);txMode_->addItem(name,mode.id);}
    txMode_->setCurrentIndex(txMode_->findData("scottie1"));
    modeTabs_=new QTabWidget;modeTabs_->setObjectName("modeTabs");
    auto modePanel=[this](QComboBox* combo,bool receive) {
        auto* panel=new QWidget;auto* panelLayout=new QVBoxLayout(panel);panelLayout->setContentsMargins(5,5,5,5);panelLayout->setSpacing(4);
        auto* grid=new QGridLayout;grid->setSpacing(3);auto* group=new QButtonGroup(panel);
        QStringList ids=receive?QStringList{"","robot36","robot72","scottie1","scottie2","scottiedx","martin1","martin2","sc2180","pd50"}
                               :QStringList{"robot36","robot72","robot24","scottie1","scottie2","scottiedx","martin1","martin2","pd50","pd120"};
        for(int i=0;i<ids.size();++i) {
            const int index=combo->findData(ids[i]);auto* button=new QPushButton(ids[i].isEmpty()?"Auto":combo->itemText(index));
            button->setObjectName(QString("%1Mode_%2").arg(receive?"rx":"tx",ids[i].isEmpty()?"auto":ids[i]));
            button->setCheckable(true);button->setChecked(combo->currentIndex()==index);button->setMinimumHeight(21);
            group->addButton(button,index);modeButtons_.push_back(button);grid->addWidget(button,i/2,i%2);
            connect(button,&QPushButton::clicked,this,[this,combo,index,receive]{
                if(combo->currentIndex()!=index)combo->setCurrentIndex(index);
                else if(receive)requestReceive();
            });
        }
        connect(combo,&QComboBox::currentIndexChanged,this,[group](int index){
            group->setExclusive(false);for(auto* button:group->buttons())button->setChecked(group->id(button)==index);group->setExclusive(true);
        });
        panelLayout->addLayout(grid);auto* more=new QHBoxLayout;more->addWidget(new QLabel("All modes"));more->addWidget(combo,1);panelLayout->addLayout(more);
        return panel;
    };
    modeTabs_->addTab(modePanel(rxMode_,true),"RX mode");modeTabs_->addTab(modePanel(txMode_,false),"TX mode");modeTabs_->setCurrentIndex(1);controls->addWidget(modeTabs_);
    connect(viewerTabs_,&QTabWidget::currentChanged,this,[this](int index){
        if(index<2)modeTabs_->setCurrentIndex(index);
        else if(index==2){QSignalBlocker block(modeTabs_);modeTabs_->setCurrentIndex(0);}
    });
    connect(modeTabs_,&QTabWidget::currentChanged,this,[this](int index){viewerTabs_->setCurrentIndex(index);});
    connect(modeTabs_,&QTabWidget::tabBarClicked,this,[this](int index){if(index>=0&&index<2)viewerTabs_->setCurrentIndex(index);});

    auto* station=new QGroupBox("Station / audio");auto* stationForm=new QFormLayout(station);stationForm->setContentsMargins(8,15,8,6);stationForm->setSpacing(4);
    call_=new QLineEdit;call_->setPlaceholderText("Your callsign");call_->setMaxLength(20);stationForm->addRow("Call",call_);
    showCallsignOnImage_=new QCheckBox("Callsign on image");showCallsignOnImage_->setObjectName("showCallsignOnImage");showCallsignOnImage_->setChecked(true);showCallsignOnImage_->setToolTip("Show your stored station callsign on TX pictures and templates.");morseAfterTx_=new QCheckBox("Morse ID");morseAfterTx_->setObjectName("morseAfterTx");morseAfterTx_->setToolTip("Send your stored callsign in Morse after each image. Set speed and tone in Settings.");auto* identification=new QHBoxLayout;identification->addWidget(showCallsignOnImage_);identification->addWidget(morseAfterTx_);identification->addStretch();stationForm->addRow(identification);
    inputDevice_=new QLineEdit("default");inputDevice_->setToolTip("ALSA input PCM name. See arecord -L.");stationForm->addRow("Input",inputDevice_);
    outputDevice_=new QLineEdit("default");outputDevice_->setToolTip("ALSA output PCM name. SignaLink keys from audio; other PTT methods are configured in Radio setup.");stationForm->addRow("Output",outputDevice_);
    recordInfo_=new QLabel("PTT: Manual · Audio: ALSA");recordInfo_->setObjectName("recordingInfo");recordInfo_->setTextFormat(Qt::PlainText);recordInfo_->setWordWrap(false);recordInfo_->setFixedHeight(recordInfo_->fontMetrics().lineSpacing()*2+4);recordInfo_->setMinimumWidth(0);recordInfo_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);stationForm->addRow(recordInfo_);controls->addWidget(station);
    auto* transport=new QHBoxLayout;record_=new QPushButton("Record audio");stop_=new QPushButton("Stop");transport->addWidget(record_,1);transport->addWidget(stop_,1);
    record_->setToolTip("Reception and image decoding run automatically. Optionally record receiver audio to a WAV file; Finish recording saves it.");
    stop_->setToolTip("Cancel the current operation and release PTT. Automatic reception resumes when idle.");
    auto* radioButton=new QPushButton("Radio…");radioButton->setObjectName("radioControlsButton");connect(radioButton,&QPushButton::clicked,this,&MainWindow::showRadioControls);transport->addWidget(radioButton,1);rightLayout->addLayout(transport);controls->addStretch();
    audioLevels_=new AudioLevelsPanel([this]{return !remoteHostControlled()&&!remoteClientMode()&&canMoveTxText();});radioFrontend_->addAudioControls(audioLevels_);connect(audioLevels_,&AudioLevelsPanel::devicesRequested,this,&MainWindow::showSetupWizard);connect(audioLevels_,&AudioLevelsPanel::tuningRequested,radioFrontend_,&RadioFrontend::showTuning);connect(audioLevels_,&AudioLevelsPanel::activityChanged,this,&MainWindow::updateEnabled);
    connect(audioLevels_,&AudioLevelsPanel::inputMonitorStarted,signal_,&SignalView::beginInput);connect(audioLevels_,&AudioLevelsPanel::inputMonitorStopped,signal_,&SignalView::endInput);connect(audioLevels_,&AudioLevelsPanel::inputMonitorPcm,signal_,[this](const QByteArray& pcm){signal_->pushPcm(pcm);});
    const auto audioDevices=[this]{audioLevels_->setDevices(inputDevice_->text().trimmed(),outputDevice_->text().trimmed());};connect(inputDevice_,&QLineEdit::textChanged,this,audioDevices);connect(outputDevice_,&QLineEdit::textChanged,this,audioDevices);
    connect(inputDevice_,&QLineEdit::editingFinished,this,[this]{requestReceive();});
    split->addWidget(right);split->setStretchFactor(0,1);split->setStretchFactor(1,0);split->setSizes({640,370});layout->addWidget(split,1);

    galleryTabs_=new QTabWidget;galleryTabs_->setObjectName("galleryTabs");galleryTabs_->setMaximumHeight(160);
    stock_=new QListWidget;stock_->setObjectName("stockPictures");thumbnailList(stock_);stock_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    stock_->setAccessibleName("Your transmit pictures");
    history_=new QListWidget;history_->setObjectName("receiveHistory");thumbnailList(history_);
    history_->setContextMenuPolicy(Qt::CustomContextMenu);connect(history_,&QWidget::customContextMenuRequested,this,[this](QPoint point){
        if(!canMoveTxText())return;
        auto* item=history_->itemAt(point);if(!item)return;const auto path=item->data(Qt::UserRole).toString();if(path.isEmpty())return;
        QMenu menu(history_);menu.addAction("Add to TX as inset…",this,[this,path]{QImage image(path);if(image.isNull()){status_->setText("Cannot load received image: "+path);return;}showPictureInPicture(image);});menu.exec(history_->viewport()->mapToGlobal(point));
    });
    tuxPictures_=new QListWidget;tuxPictures_->setObjectName("tuxPictures");thumbnailList(tuxPictures_);
    tuxPictures_->setAccessibleName("Built-in Tux radio pictures");
    tuxPictures_->setToolTip("Full-size Tux artwork · Choose a picture, then select any SSTV mode.");
    holidayPictures_=new QListWidget;holidayPictures_->setObjectName("holidayPictures");thumbnailList(holidayPictures_);
    holidayPictures_->setAccessibleName("Holiday SSTV pictures");
    holidayPictures_->setToolTip("Full-size holiday pictures · Add your greeting or callsign and choose any SSTV mode.");
    galleryTabs_->addTab(stock_,"Transmit pictures");galleryTabs_->addTab(history_,"Receive history");galleryTabs_->addTab(tuxPictures_,"Tux pictures");galleryTabs_->addTab(holidayPictures_,"Holidays");layout->addWidget(galleryTabs_);
    connect(viewerTabs_,&QTabWidget::currentChanged,this,[this,controls,spectrumBox](int){const bool radio=viewerTabs_->currentWidget()==radioPage_;if(radio)radioFrontend_->addAudioMonitor(spectrumBox);else controls->insertWidget(0,spectrumBox);spectrumBox->show();operatingControls_->setVisible(!radio);galleryTabs_->setVisible(!radio);});
    auto* stockTools=new QWidget;auto* stockToolsLayout=new QHBoxLayout(stockTools);stockToolsLayout->setContentsMargins(0,0,0,0);stockToolsLayout->setSpacing(4);
    addStock_=new QPushButton("Add photos…");addStock_->setObjectName("addStockPhotosButton");stockToolsLayout->addWidget(addStock_);
    screenshots_=new QPushButton("Screenshots…");screenshots_->setObjectName("loadScreenshotButton");screenshots_->setToolTip("Choose a saved screenshot from\n"+screenshotDirectory());stockToolsLayout->addWidget(screenshots_);
    selfie_=new QPushButton("Take selfie…");selfie_->setObjectName("takeSelfieButton");selfie_->setToolTip("Preview your webcam and take a photo for TX.");stockToolsLayout->addWidget(selfie_);
    removeStock_=new QPushButton("Remove selected");removeStock_->setObjectName("removeStockPhotosButton");removeStock_->setToolTip("Remove selected photos from this list; keep the files and current TX image.");stockToolsLayout->addWidget(removeStock_);galleryTabs_->setCornerWidget(stockTools,Qt::TopRightCorner);
    connect(galleryTabs_,&QTabWidget::currentChanged,this,[this,stockTools](int index){
        stockTools->setVisible(index==0||galleryTabs_->widget(index)==tuxPictures_||galleryTabs_->widget(index)==holidayPictures_);
        removeStock_->setVisible(index==0);
    });
    connect(addStock_,&QPushButton::clicked,this,&MainWindow::openImage);connect(removeStock_,&QPushButton::clicked,this,&MainWindow::removeSelectedStockImages);
    connect(screenshots_,&QPushButton::clicked,this,&MainWindow::openScreenshots);
    connect(selfie_,&QPushButton::clicked,this,&MainWindow::takeSelfie);
    connect(stock_,&QListWidget::itemSelectionChanged,this,&MainWindow::updateEnabled);
    stock_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(stock_,&QWidget::customContextMenuRequested,this,[this](QPoint point){
        if(!canMoveTxText())return;
        auto* item=stock_->itemAt(point);if(!item)return;
        if(!item->isSelected())stock_->setCurrentItem(item);
        QMenu menu(stock_);menu.addAction("Remove selected photos",this,&MainWindow::removeSelectedStockImages);menu.exec(stock_->viewport()->mapToGlobal(point));
    });
    auto* removeShortcut=new QShortcut(QKeySequence(Qt::Key_Delete),stock_);removeShortcut->setContext(Qt::WidgetWithChildrenShortcut);removeShortcut->setAutoRepeat(false);connect(removeShortcut,&QShortcut::activated,this,&MainWindow::removeSelectedStockImages);
    const auto selectPicture=[this](QListWidgetItem* item) {
        if(!item || !canMoveTxText() || (QApplication::keyboardModifiers()&(Qt::ControlModifier|Qt::ShiftModifier)))return;
        const auto path=item->data(Qt::UserRole).toString();QImageReader reader(path);reader.setAutoTransform(true);const auto image=reader.read();
        if(image.isNull()){status_->setText("Cannot load picture: "+path);return;}
        resetTxHistory();source_=image;clearImageFields();viewerTabs_->setCurrentWidget(txPage_);updateTx();
    };
    connect(stock_,&QListWidget::itemClicked,this,selectPicture);
    connect(tuxPictures_,&QListWidget::itemClicked,this,selectPicture);
    connect(tuxPictures_,&QListWidget::itemActivated,this,selectPicture);
    connect(holidayPictures_,&QListWidget::itemClicked,this,selectPicture);
    connect(holidayPictures_,&QListWidget::itemActivated,this,selectPicture);

    auto* bottom=new QHBoxLayout;status_=new QLabel("Ready · Sound devices closed");status_->setObjectName("statusLabel");status_->setWordWrap(true);status_->setFixedHeight(status_->fontMetrics().lineSpacing()*2+4);bottom->addWidget(status_,1);
    progress_=new QProgressBar;progress_->setRange(0,100);progress_->setValue(0);progress_->setTextVisible(false);progress_->setFixedWidth(90);progress_->setMaximumHeight(14);bottom->addWidget(progress_);
    layout->addLayout(bottom);
    setCentralWidget(root);
    record_->setObjectName("recordButton");stop_->setObjectName("stopButton");inputDevice_->setObjectName("inputDevice");outputDevice_->setObjectName("outputDevice");rxInfo_->setObjectName("receiveInfo");
    play_->setObjectName("playButton");export_->setObjectName("exportButton");txMode_->setObjectName("transmitMode");rxMode_->setObjectName("receiveMode");call_->setObjectName("stationCall");

    auto* compatibilityDialog=new QDialog(this);compatibilityDialog->setWindowTitle("Mode compatibility");compatibilityDialog->resize(960,600);
    auto* compatibilityLayout=new QVBoxLayout(compatibilityDialog);auto* evidence=new QLabel("43 MMSSTV target modes. External interoperability verification is pending; see the compatibility notes for the current evidence.");evidence->setWordWrap(true);compatibilityLayout->addWidget(evidence);
    auto* table=new QTableWidget(targetCount,5);table->setHorizontalHeaderLabels({"Mode","Family","Image","Implementation","Compatibility evidence"});table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->verticalHeader()->hide();
    int row=0;for(const auto& mode:sstv::modes()) {
        const QString evidence=mode.implemented?(mode.note.isEmpty()?"Internal tests; external verification pending":mode.note):mode.note;
        const QStringList cells={mode.name,sstv::familyName(mode.family),QString("%1 × %2").arg(mode.width).arg(mode.height),mode.implemented?"WAV TX / RX":"Pending",evidence};
        for(int column=0;column<cells.size();++column)table->setItem(row,column,new QTableWidgetItem(cells[column]));
        ++row;
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Stretch);compatibilityLayout->addWidget(table);
    auto* closeCompatibility=new QDialogButtonBox(QDialogButtonBox::Close);connect(closeCompatibility,&QDialogButtonBox::rejected,compatibilityDialog,&QDialog::reject);compatibilityLayout->addWidget(closeCompatibility);

    auto* file=menuBar()->addMenu("&File");auto* openAction=file->addAction("Load TX picture…",this,&MainWindow::openImage);openAction->setShortcut(QKeySequence::Open);
    screenshotAction_=file->addAction("Load screenshot…",this,&MainWindow::openScreenshots);screenshotAction_->setObjectName("loadScreenshotAction");
    selfieAction_=file->addAction("Take selfie…",this,&MainWindow::takeSelfie);selfieAction_->setObjectName("takeSelfieAction");
    saveTxImageAction_=file->addAction("Save TX image…",this,&MainWindow::saveTransmitImage);saveTxImageAction_->setObjectName("saveTransmitImageAction");saveTxImageAction_->setShortcut(QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_S));
    pictureInPictureAction_=file->addAction("Picture in picture…",this,[this]{showPictureInPicture();});pictureInPictureAction_->setObjectName("pictureInPictureAction");
    auto* patternsAction=file->addAction("Test patterns…",this,&MainWindow::showTestPatterns);patternsAction->setObjectName("testPatternsAction");
    file->addAction("Open RX WAV…",this,&MainWindow::decodeFile);file->addAction("Save RX copy…",this,[this]{saveReceived();});file->addSeparator();auto* quit=file->addAction("Quit",this,&QWidget::close);quit->setShortcut(QKeySequence::Quit);
    auto* edit=menuBar()->addMenu("&Edit");undoTxAction_=edit->addAction("Undo",this,&MainWindow::undoTx);undoTxAction_->setObjectName("undoTransmitAction");undoTxAction_->setShortcut(QKeySequence(Qt::CTRL|Qt::Key_Z));redoTxAction_=edit->addAction("Redo",this,&MainWindow::redoTx);redoTxAction_->setObjectName("redoTransmitAction");redoTxAction_->setShortcuts({QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_Z),QKeySequence(Qt::CTRL|Qt::Key_Y)});
    for(auto* action:{undoTxAction_,redoTxAction_}){action->setShortcutContext(Qt::WindowShortcut);txFullScreen_->addAction(action);}
    connect(txUndo_,&QUndoStack::indexChanged,this,&MainWindow::updateTxUndoActions);updateTxUndoActions();
    auto* view=menuBar()->addMenu("&View");view->addAction("RX picture",this,[this]{viewerTabs_->setCurrentWidget(rxPage_);});view->addAction("TX picture",this,[this]{viewerTabs_->setCurrentWidget(txPage_);});
    auto* windowFullScreen=view->addAction("Full screen",this,[this,previousState=Qt::WindowStates{}]() mutable {
        if(isFullScreen())setWindowState(previousState);
        else {previousState=windowState();showFullScreen();}
    });
    windowFullScreen->setObjectName("windowFullScreenAction");windowFullScreen->setShortcut(QKeySequence(Qt::Key_F11));
    windowFullScreen->setShortcutContext(Qt::WindowShortcut);windowFullScreen->setAutoRepeat(false);
    view->addAction("Sync corrections",this,[this]{viewerTabs_->setCurrentWidget(syncPage_);});auto* radioTab=view->addAction("Radio console",this,&MainWindow::showRadioControls);radioTab->setObjectName("radioTabAction");view->addAction("Receive history",this,[this]{viewerTabs_->setCurrentWidget(rxPage_);galleryTabs_->setCurrentWidget(history_);});
    auto* contactLogAction=view->addAction("Contact log…",this,&MainWindow::showContactLog);contactLogAction->setObjectName("contactLogAction");
    view->addAction("Mode compatibility",compatibilityDialog,[compatibilityDialog]{compatibilityDialog->show();compatibilityDialog->raise();});
    auto* option=menuBar()->addMenu("&Option");settingsAction_=option->addAction("Settings…",this,&MainWindow::showSettings);settingsAction_->setObjectName("settingsAction");
    audioWizardAction_=option->addAction("Setup wizard…",this,&MainWindow::showSetupWizard);audioWizardAction_->setObjectName("audioWizardAction");
    auto* help=menuBar()->addMenu("&Help");auto* about=help->addAction("About Tux-sstv",this,[this,implementedCount] {
        QMessageBox box(this);box.setObjectName("aboutTuxSstv");box.setWindowTitle("About Tux-sstv");box.setIconPixmap(branding::tux(88));
        box.setText(QString("Tux-sstv %1 — Final Release\nDeveloped by Eric Richards — KI5BZO").arg(QCoreApplication::applicationVersion()));
        box.setInformativeText(QString("Send and receive SSTV pictures over amateur radio.\n\n%1 SSTV modes, automatic reception, guided audio, CAT and remote setup and a built-in picture editor.\n\nTux is on picture duty. 73!\n\nFormerly OpenSSTV. Hamlib radio and PTT controls depend on the radio backend.").arg(implementedCount));box.setStandardButtons(QMessageBox::Ok);box.exec();
    });about->setObjectName("aboutAction");
}
void MainWindow::showSettings() {
    showSettingsPage(0);
}
void MainWindow::showSettingsPage(int index){
    if(index==1&&(remoteClientMode()||remoteConfig_.role==remote::Role::Host)){showRadioSetup();return;}
    if(!prepareSetup("Settings",[this,index]{showSettingsPage(index);}))return;
    const bool shared=remoteConfig_.role==remote::Role::Host;
    if(shared){const auto error=remoteServer_->reserveLocalTransmit();if(!error.isEmpty()){status_->setText(error);return;}}
    const bool linked=remoteClientMode()||shared;radioSetupBusy_=true;updateEnabled();
    const auto releaseSetup=qScopeGuard([this,shared]{radioSetupBusy_=false;if(shared)remoteServer_->releaseLocalTransmit();updateEnabled();});
    finishTxDrag();const auto beforePicture=txEditState();
    receiveSuspended_=true;stopBackgroundReceive();const auto monitorResume=qScopeGuard([this]{receiveSuspended_=false;queueReceiveUpdate();});
    const auto originalSkin=skinId_;
    QDialog dialog(this);dialog.setObjectName("settingsDialog");dialog.setWindowTitle("Tux-sstv settings");dialog.resize(700,650);
    auto* outer=new QVBoxLayout(&dialog);
    auto* station=new QLabel(remoteClientMode()?"Station connection: Remote laptop · Hardware setup uses the shack PC":shared?"Station connection: This shack PC · Shared over Wi-Fi":"Station connection: Local USB / sound card · Hardware setup uses this computer");station->setObjectName("settingsStationConnection");station->setWordWrap(true);outer->addWidget(station);
    auto* connection=new QPushButton("Station connection…");connection->setObjectName("settingsStationConnectionButton");outer->addWidget(connection);connect(connection,&QPushButton::clicked,&dialog,[this,&dialog]{dialog.reject();QTimer::singleShot(0,this,&MainWindow::showRemoteSetup);});
    if(remoteClientMode()){auto* local=new QPushButton("Set up this computer's USB radio/audio…");local->setObjectName("settingsUseLocal");outer->addWidget(local);connect(local,&QPushButton::clicked,&dialog,[this,&dialog]{dialog.reject();QTimer::singleShot(0,this,[this]{useLocalSetup(false);});});}
    auto* audioSetup=new QPushButton("Setup wizard…");audioSetup->setObjectName("settingsOpenAudioWizard");outer->addWidget(audioSetup);connect(audioSetup,&QPushButton::clicked,&dialog,[this,&dialog]{dialog.reject();QTimer::singleShot(0,this,&MainWindow::showSetupWizard);});
    auto* tabs=new QTabWidget;tabs->setObjectName("settingsTabs");outer->addWidget(tabs);
    auto* general=new QWidget;auto* layout=new QVBoxLayout(general);auto* generalScroll=new QScrollArea;generalScroll->setObjectName("settingsGeneralScroll");generalScroll->setWidgetResizable(true);generalScroll->setFrameShape(QFrame::NoFrame);generalScroll->setWidget(general);tabs->addTab(generalScroll,"General");auto* form=new QFormLayout;
    auto* radioPage=new RadioSettingsPage(radioConfig_,radio_);radioPage->setEnabled(!linked);auto* radioScroll=new QScrollArea;radioScroll->setWidgetResizable(true);radioScroll->setWidget(radioPage);
    if(linked){
        radioScroll->setParent(&dialog);radioScroll->hide();auto* remoteRadio=new QWidget;auto* remoteLayout=new QVBoxLayout(remoteRadio);
        auto* note=new QLabel("Open Hamlib setup below to select the radio model, CAT device and PTT method for the shack. Pairing stays connected while you configure the idle station.");note->setWordWrap(true);remoteLayout->addWidget(note);
        auto* setup=new QPushButton("Open shack Hamlib setup…");setup->setObjectName("settingsShackRadioSetup");remoteLayout->addWidget(setup);remoteLayout->addStretch();
        connect(setup,&QPushButton::clicked,&dialog,[this,&dialog]{dialog.reject();QTimer::singleShot(0,this,&MainWindow::showRadioSetup);});tabs->addTab(remoteRadio,"Radio (Hamlib)");
    }else tabs->addTab(radioScroll,"Radio (Hamlib)");
    tabs->setCurrentIndex(index==1?1:0);
    auto* appearance=new QGroupBox("Appearance");auto* appearanceForm=new QFormLayout(appearance);
    auto* skinChoice=new QComboBox;skinChoice->setObjectName("settingsSkin");
    for(const auto& skin:ui::skins()){QPixmap swatch(42,18);QLinearGradient gradient(0,0,42,18);gradient.setColorAt(0,skin.accent);gradient.setColorAt(1,skin.secondary.isValid()?skin.secondary:skin.surface);QPainter painter(&swatch);painter.fillRect(swatch.rect(),gradient);painter.end();skinChoice->addItem(QIcon(swatch),skin.name,skin.id);}
    skinChoice->setIconSize({42,18});skinChoice->setCurrentIndex(skinChoice->findData(skinId_));
    skinChoice->setToolTip("Preview a skin immediately. Save keeps your choice; Cancel restores the previous skin.");
    auto* skinDescription=new QLabel;skinDescription->setObjectName("skinDescription");skinDescription->setWordWrap(true);
    auto* skinPreview=new QLabel;skinPreview->setObjectName("skinPreview");skinPreview->setFixedHeight(72);skinPreview->setAccessibleName("Selected skin material and color preview");
    appearanceForm->addRow("Skin",skinChoice);appearanceForm->addRow(skinPreview);appearanceForm->addRow(skinDescription);layout->addWidget(appearance);
    const auto previewSkin=[&] {const auto selectedId=skinChoice->currentData().toString();const auto& skin=ui::skinById(selectedId);skinDescription->setText(skin.description);
        QPixmap preview(560,72);preview.fill(skin.field);QPainter painter(&preview);painter.setRenderHint(QPainter::Antialiasing);QLinearGradient material(0,0,560,72);material.setColorAt(0,skin.surface.lighter(125));material.setColorAt(1,skin.background);painter.fillRect(preview.rect(),material);
        painter.fillRect(QRect(0,0,4,72),skin.accent);painter.setPen(skin.text);QFont title("Sans Serif",12,QFont::Bold);painter.setFont(title);painter.drawText(QRect(16,7,330,25),Qt::AlignLeft|Qt::AlignVCenter,skin.name);painter.setPen(skin.accent);painter.setFont(QFont("Monospace",15,QFont::Bold));painter.drawText(QRect(16,35,270,28),Qt::AlignLeft|Qt::AlignVCenter,"145.500000 MHz");
        painter.setPen(Qt::NoPen);for(int n=0;n<16;++n){painter.setBrush(n<10?skin.accent:n<13?(skin.secondary.isValid()?skin.secondary:skin.success):skin.border);painter.drawRoundedRect(QRectF(350+n*11,40,7,18),2,2);}painter.end();skinPreview->setPixmap(preview);applySkin(skin.id);};
    connect(skinChoice,&QComboBox::currentIndexChanged,&dialog,previewSkin);previewSkin();
    auto* call=new QLineEdit(call_->text());auto* input=new QLineEdit(inputDevice_->text());auto* output=new QLineEdit(outputDevice_->text());call->setMaxLength(20);
    call->setObjectName("settingsCall");input->setObjectName("settingsInput");output->setObjectName("settingsOutput");
    input->setEnabled(!linked);output->setEnabled(!linked);
    form->addRow("Station callsign",call);form->addRow("ALSA input",input);form->addRow("ALSA output",output);layout->addLayout(form);
    auto* morseGroup=new QGroupBox("Callsign identification");auto* morseForm=new QFormLayout(morseGroup);
    auto* showCall=new QCheckBox("Show callsign on image");showCall->setObjectName("settingsShowCallsign");showCall->setChecked(showCallsignOnImage_->isChecked());morseForm->addRow(showCall);
    auto* enabled=new QCheckBox("Send callsign in Morse after TX");enabled->setObjectName("settingsMorseEnabled");enabled->setChecked(morseEnabled_);
    auto* speed=new QSpinBox;speed->setObjectName("settingsMorseWpm");speed->setRange(5,40);speed->setSuffix(" WPM");speed->setValue(morseWpm_);
    auto* tone=new QSpinBox;tone->setObjectName("settingsMorseTone");tone->setRange(400,2500);tone->setSingleStep(50);tone->setSuffix(" Hz");tone->setValue(morseTone_);
    morseForm->addRow(enabled);morseForm->addRow("Speed",speed);morseForm->addRow("Audio tone",tone);
    auto* morseHint=new QLabel("Uses the stored callsign once after each image, including saved TX WAVs. Turn off Show callsign on image for Morse-only identification.");morseHint->setWordWrap(true);morseForm->addRow(morseHint);
    auto* morseError=new QLabel;morseError->setObjectName("settingsMorseError");morseError->setWordWrap(true);morseForm->addRow(morseError);layout->addWidget(morseGroup);
    auto* hint=new QLabel("Use ALSA PCM names such as default or plughw:CARD=USB,DEV=0. Option → Setup wizard guides audio, CAT / Hamlib and remote station setup. Receiver clock, tone and channel corrections are on the Sync tab.");hint->setWordWrap(true);layout->addWidget(hint);
    layout->addStretch();auto* radioError=new QLabel;radioError->setObjectName("settingsRadioError");radioError->setWordWrap(true);outer->addWidget(radioError);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);outer->addWidget(buttons);
    const auto validate=[&] {
        speed->setEnabled(enabled->isChecked());tone->setEnabled(enabled->isChecked());QString error;
        if(enabled->isChecked())try{sstv::morseIdDurationMs({call->text(),speed->value(),tone->value()});}catch(const std::exception& e){error=QString::fromUtf8(e.what());}
        const auto config=radioPage->config();const auto connectionError=config.enabled?config.validate():QString{};
        radioError->setText(connectionError);morseError->setText(error);buttons->button(QDialogButtonBox::Save)->setEnabled(error.isEmpty()&&connectionError.isEmpty()&&!radio_->busy());
    };
    connect(enabled,&QCheckBox::toggled,&dialog,validate);connect(call,&QLineEdit::textChanged,&dialog,validate);
    connect(speed,&QSpinBox::valueChanged,&dialog,validate);connect(tone,&QSpinBox::valueChanged,&dialog,validate);connect(radioPage,&RadioSettingsPage::edited,&dialog,validate);connect(radio_,&radio::Controller::changed,&dialog,validate);validate();
    if(dialog.exec()==QDialog::Accepted){
        const auto configuration=linked?radioConfig_:radioPage->config();
        if(configuration!=radioConfig_ && (radio_->connected() || radio_->pttActive()))radio_->disconnectRadio();
        receiveRetry_.stop();receiveRetryDelayMs_=1000;radioConfig_=configuration;{const QSignalBlocker imageBlock(showCallsignOnImage_),morseBlock(morseAfterTx_);showCallsignOnImage_->setChecked(showCall->isChecked());morseAfterTx_->setChecked(enabled->isChecked());}morseEnabled_=enabled->isChecked();morseWpm_=speed->value();morseTone_=tone->value();call_->setText(call->text().trimmed());inputDevice_->setText(input->text().trimmed());outputDevice_->setText(output->text().trimmed());saveSettings();updateTx();updateRadioStatus();recordTxEdit(beforePicture,"Change picture callsign");
    }
    else applySkin(originalSkin);
}
void MainWindow::showComposer() {
    showCaptionEditor(-1);
}
void MainWindow::showCaptionEditor(int index){
    if(!canMoveTxText() || index>=captions_.size())return;
    finishTxDrag();const auto before=txEditState();
    const auto* mode=sstv::modeById(txMode_->currentData().toString());
    const auto base=templateEnabled_?sstv::renderTemplate(templateFields_,{mode->width,mode->height},call_->text(),source_,nullptr,showCallsignOnImage_->isChecked()):source_;
    auto placed=captions_;const auto draft=index<0?overlay_:placed.takeAt(index);
    QWidget* editorParent=txFullScreen_->isVisible()?txFullScreen_:this;
    CompositionDialog dialog(base,*mode,templateEnabled_?QString{}:imageCallsign(),draft,recentEmojis_,emojiTone_,editorParent,callsignPlacement_,placed,index,inset_);
    dialog.setWindowTitle(QString(index<0?"Add text & emoji · %1":"Edit text & emoji · %1").arg(mode->name));dialog.setCursor(Qt::ArrowCursor);
    const auto result=dialog.exec();
    if(txFullScreen_->isVisible()){txFullScreen_->raise();txFullScreen_->activateWindow();}
    if(result!=QDialog::Accepted)return;
    const auto added=dialog.overlay();
    if(index<0){if(!added.text.trimmed().isEmpty())captions_.append(added);}
    else if(added.text.trimmed().isEmpty())captions_.removeAt(index);
    else captions_[index]=added;
    overlay_=added;overlay_.text.clear();overlay_.freePosition=false;overlay_.relativePosition={};
    callsignPlacement_=dialog.callsignPlacement();recentEmojis_=dialog.recentEmojis();emojiTone_=dialog.emojiTone();saveSettings();updateTx();recordTxEdit(before,index<0?"Add text":"Edit text");
}
void MainWindow::showTemplates() {
    if(job_.isRunning() || recording_ || playback_.state()!=QProcess::NotRunning || radioTxPhase_!=RadioTxPhase::Idle || radio_->pttActive())return;
    const auto* mode=sstv::modeById(txMode_->currentData().toString());
    finishTxDrag();const auto before=txEditState();
    auto fields=templateFields_;auto captions=captions_;
    if(!templateEnabled_){fields.clearFields();captions.clear();}
    TemplatesDialog dialog(fields,templateEnabled_,*mode,call_->text(),overlay_,this,source_,captions,inset_,showCallsignOnImage_->isChecked());
    if(dialog.exec()==QDialog::Accepted){
        templateFields_=dialog.fields();templateEnabled_=dialog.enabled();captions_=dialog.captionOverlays();overlay_=dialog.captionOverlay();inset_=dialog.imageInset();
        overlay_.text.clear();overlay_.freePosition=false;overlay_.relativePosition={};
        if(!dialog.picture().isNull())source_=dialog.picture();
        if(!templateEnabled_)clearImageFields();
        saveSettings();viewerTabs_->setCurrentWidget(txPage_);updateTx();
        QString name;for(const auto& design:sstv::imageTemplates())if(design.id==templateFields_.design)name=design.name;
        status_->setText(templateEnabled_?"Template ready · "+name:"Picture layout restored");recordTxEdit(before,templateEnabled_?"Change template":"Clear template");
    }
}
void MainWindow::showPictureInPicture(const QImage& received){
    if(!canMoveTxText()||source_.isNull())return;
    finishTxDrag();const auto before=txEditState();
    QWidget* owner=txFullScreen_->isVisible()?txFullScreen_:this;
    const auto focus=qScopeGuard([this,owner]{if(owner==txFullScreen_&&txFullScreen_->isVisible()){txFullScreen_->raise();txFullScreen_->activateWindow();}});
    auto draft=inset_;if(!received.isNull())draft.image=received.copy();else if(draft.image.isNull()&&!received_.isNull())draft.image=received_.copy();
    const auto* mode=sstv::modeById(txMode_->currentData().toString());const auto base=templateEnabled_?sstv::renderTemplate(templateFields_,{mode->width,mode->height},call_->text(),source_,nullptr,showCallsignOnImage_->isChecked()):source_;
    InsetDialog dialog(draft,base,*mode,templateEnabled_?QString{}:imageCallsign(),captions_,callsignPlacement_,received_,historyPath_,owner);
    if(dialog.exec()!=QDialog::Accepted)return;
    inset_=dialog.inset();viewerTabs_->setCurrentWidget(txPage_);updateTx();status_->setText(inset_.image.isNull()?"Received inset removed":"Received picture added to TX");recordTxEdit(before,"Change received picture");
}
void MainWindow::loadStock() {
    const QSignalBlocker block(stock_);stock_->clear();
    for(const auto& path:stockPaths_) {
        QImageReader reader(path);reader.setAutoTransform(true);if(reader.size().isValid())reader.setScaledSize(reader.size().scaled({160,120},Qt::KeepAspectRatio));const auto image=reader.read();if(image.isNull())continue;
        auto* item=new QListWidgetItem(QIcon(QPixmap::fromImage(image)),QFileInfo(path).fileName(),stock_);item->setData(Qt::UserRole,path);item->setToolTip(path);
    }
    updateEnabled();
}
void MainWindow::loadBuiltInPictures(){
    initTuxPictureResources();
    struct Picture{QString title,path;QSize size;QImage thumbnail;bool holiday;};
    // Decode thumbnails once per process. Selecting one still reads the original.
    static const QList<Picture> pictures=[]{
        QList<Picture> result;
        for(const auto& prefix:QStringList{":/pictures/tux/",":/pictures/holidays/"}){
            QFile catalog(prefix+"catalog.json");if(!catalog.open(QIODevice::ReadOnly))continue;
            for(const auto& entry:QJsonDocument::fromJson(catalog.readAll()).array()){
                const auto info=entry.toObject();const auto path=prefix+info.value("file").toString();
                QImageReader reader(path);const auto size=reader.size();
                if(size.isValid())reader.setScaledSize(size.scaled({160,120},Qt::KeepAspectRatio));
                const auto thumbnail=reader.read();if(thumbnail.isNull())continue;
                result.append({info.value("title").toString(),path,size,thumbnail,prefix.contains("holidays")});
            }
        }
        return result;
    }();
    for(const auto& picture:pictures){
        auto* item=new QListWidgetItem(QIcon(QPixmap::fromImage(picture.thumbnail)),picture.title,picture.holiday?holidayPictures_:tuxPictures_);
        item->setData(Qt::UserRole,picture.path);
        item->setToolTip(QString("%1\nFull-size artwork · %2 × %3\nChoose any SSTV mode; add your own callsign or text.").arg(picture.title).arg(picture.size.width()).arg(picture.size.height()));
    }
}


void MainWindow::logContact() {
    sstv::Contact seed;seed.call=receivedCall_;seed.mode=receivedMode_.isEmpty()?txMode_->currentData().toString():receivedMode_;
    if(receivedUtc_.isValid())seed.utc=receivedUtc_;
    seed.frequencyHz=received_.isNull()?(radio_->connected()?radio_->state().value("frequencyHz",0).toDouble():0):receivedFrequencyHz_;
    if(remoteMonitorShowing_)seed.notes="Remote laptop transmission (TX monitor)";
    ContactEditor editor(sstv::ContactLog(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/contacts"),seed,remoteMonitorShowing_?QImage{}:received_,remoteMonitorShowing_?received_:transmitImage(),this);
    if(editor.exec()==QDialog::Accepted)status_->setText("Contact and selected pictures saved · View → Contact log");
}
void MainWindow::showContactLog() {
    ContactLogDialog dialog(sstv::ContactLog(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/contacts"),this);dialog.exec();
}

void MainWindow::showTestPatterns(){
    if(job_.isRunning() || recording_ || playback_.state()!=QProcess::NotRunning || radioTxPhase_!=RadioTxPhase::Idle || radio_->pttActive())return;
    PatternsDialog dialog(selectedPattern_,this);if(dialog.exec()==QDialog::Accepted)selectTestPattern(dialog.selectedId());
}
void MainWindow::selectTestPattern(const QString& id){
    if(job_.isRunning() || recording_ || playback_.state()!=QProcess::NotRunning || radioTxPhase_!=RadioTxPhase::Idle || radio_->pttActive())return;
    const auto* pattern=sstv::testPatternById(id);if(!pattern)return;const auto image=sstv::renderTestPattern(id);if(image.isNull())return;
    resetTxHistory();source_=image;selectedPattern_=id;clearImageFields();viewerTabs_->setCurrentWidget(txPage_);updateTx();status_->setText("Test pattern ready · "+pattern->name);
}
