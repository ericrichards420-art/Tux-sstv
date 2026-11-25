#include "radio_frontend.h"
#include "branding.h"
#include "skins.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>

class TuningDial:public QDial {
public:
    using QDial::QDial;
    void setColors(const QPalette& colors){colors_=colors;update();}
private:
    QPalette colors_=ui::skinPalette(ui::radioSkin(ui::skinById("classic")));
protected:
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.translate(width()/2.,height()/2.);
        const double radius=std::min(width(),height())/2.-5;const auto accent=colors_.color(isEnabled()?QPalette::Highlight:QPalette::Mid);
        p.setBrush(Qt::NoBrush);p.setPen(QPen(colors_.color(QPalette::Dark),8));p.drawEllipse(QPointF(0,0),radius-1,radius-1);
        for(int i=0;i<60;++i){const double angle=i*6.283185307/60.;p.setPen(QPen(colors_.color(isEnabled()?QPalette::Mid:QPalette::Dark),i%5?1:2));p.drawLine(QPointF(std::cos(angle)*(radius-4),std::sin(angle)*(radius-4)),QPointF(std::cos(angle)*radius,std::sin(angle)*radius));}
        const double edge=radius-10;QRadialGradient metal(QPointF(-edge*.35,-edge*.45),edge*1.7);metal.setColorAt(0,colors_.color(QPalette::Button).lighter(isEnabled()?170:115));metal.setColorAt(.5,colors_.color(QPalette::Button));metal.setColorAt(1,colors_.color(QPalette::Button).darker(185));
        p.setBrush(metal);p.setPen(QPen(colors_.color(QPalette::Mid).lighter(150),1));p.drawEllipse(QPointF(0,0),edge,edge);
        p.setBrush(colors_.color(QPalette::Base));p.setPen(QPen(colors_.color(QPalette::Dark),2));p.drawEllipse(QPointF(0,0),edge-12,edge-12);
        const double angle=value()*6.283185307/100.-1.570796327;p.setPen(QPen(accent,5,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(std::cos(angle)*(edge-21),std::sin(angle)*(edge-21)),QPointF(std::cos(angle)*(edge-5),std::sin(angle)*(edge-5)));
        QFont label("DejaVu Sans");label.setPixelSize(13);label.setBold(true);p.setFont(label);p.setPen(colors_.color(isEnabled()?QPalette::Text:QPalette::PlaceholderText));p.drawText(QRectF(-32,-9,64,22),Qt::AlignCenter,"VFO");
        if(hasFocus()){p.setBrush(Qt::NoBrush);p.setPen(QPen(accent,1,Qt::DashLine));p.drawEllipse(QPointF(0,0),radius+2,radius+2);}
    }
};

class RadioSignalMeter:public QWidget {
public:
    explicit RadioSignalMeter(QWidget* parent=nullptr):QWidget(parent){setObjectName("radioFrontendMeter");setMinimumHeight(65);setAccessibleName("Radio signal strength");}
    void setColors(const QPalette& colors){colors_=colors;update();}
    void setReading(std::optional<int> db){if(readingSet_&&reading_==db)return;readingSet_=true;reading_=db;setProperty("hasReading",db.has_value());setProperty("readingDb",db?QVariant(*db):QVariant{});setAccessibleDescription(db?QString("%1 dB relative to S9").arg(*db):"Signal strength not available");update();}
protected:
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);p.fillRect(rect(),colors_.color(QPalette::Base));p.setRenderHint(QPainter::Antialiasing);
        const QRectF bezel(1,1,std::max(1,width()-2),34);p.setBrush(colors_.color(QPalette::Dark));p.setPen(QPen(colors_.color(QPalette::Mid),1));p.drawRoundedRect(bezel,5,5);
        const QRectF track(7,7,std::max(1,width()-14),22);const double segment=track.width()/38.;const bool light=colors_.color(QPalette::Base).lightness()>128;
        for(int i=0;i<38;++i){
            const int level=-54+(i+1)*3;const bool lit=reading_&&*reading_>=level;
            const auto color=lit?(level<=0?QColor(light?"#087751":"#50e2b8"):level<=24?QColor(light?"#8a5a05":"#ffd37a"):QColor(light?"#b7223f":"#ff7e87")):colors_.color(QPalette::AlternateBase);
            QLinearGradient led(0,track.top(),0,track.bottom());led.setColorAt(0,color.lighter(lit?130:105));led.setColorAt(1,color.darker(lit?115:120));p.setBrush(led);p.setPen(Qt::NoPen);p.drawRoundedRect(QRectF(track.x()+i*segment,track.y(),std::max(1.,segment-2),track.height()),1.5,1.5);
        }
        p.setPen(colors_.color(QPalette::Text));QFont font("DejaVu Sans");font.setPixelSize(10);p.setFont(font);
        for(const auto& tick:QList<QPair<int,QString>>{{-48,"S1"},{-36,"3"},{-24,"5"},{-12,"7"},{0,"S9"},{20,"+20"},{40,"+40"},{60,"+60"}}){const double x=track.x()+(tick.first+54)/114.*track.width();p.drawLine(QPointF(x,39),QPointF(x,43));p.drawText(QRectF(std::clamp(x-15.,0.,std::max(0.,width()-30.)),45,30,16),Qt::AlignCenter,tick.second);}
    }
private:
    QPalette colors_=ui::skinPalette(ui::radioSkin(ui::skinById("classic")));
    std::optional<int> reading_;
    bool readingSet_=false;
};

RadioFrontend::RadioFrontend(radio::Controller* controller,const radio::Config& config,std::function<bool()> idle,QWidget* parent)
    :QWidget(parent),controller_(controller),config_(config),idle_(std::move(idle)){
    setObjectName("radioFrontend");setAttribute(Qt::WA_StyledBackground,true);
    setStyleSheet(ui::radioStyleSheet(ui::radioSkin(ui::skinById("classic"))));
    auto* root=new QVBoxLayout(this);rootLayout_=root;root->setContentsMargins(12,10,12,10);root->setSpacing(5);
    auto* header=new QHBoxLayout;header->setSpacing(7);
    auto* tux=new QLabel;tux->setObjectName("radioTuxMascot");tux->setAccessibleName("Tux penguin mascot");tux->setToolTip("Tux is standing by for your next SSTV picture.");tux->setPixmap(branding::tux(36));header->addWidget(tux);
    auto* brand=new QVBoxLayout;brand->setSpacing(0);auto* title=new QLabel("Tux-sstv Radio");title->setObjectName("radioConsoleTitle");brand->addWidget(title);auto* subtitle=new QLabel("SSTV  /  VOICE  /  STATION CONTROL");subtitle->setObjectName("radioConsoleSubtitle");brand->addWidget(subtitle);header->addLayout(brand);header->addStretch();
    monitorButton_=new QPushButton("Waterfall");monitorButton_->setObjectName("radioFrontendWaterfall");monitorButton_->setEnabled(false);header->addWidget(monitorButton_);
    audioButton_=new QPushButton("Audio levels");audioButton_->setObjectName("radioFrontendAudio");audioButton_->setEnabled(false);header->addWidget(audioButton_);
    settings_=new QPushButton("Radio setup…");settings_->setObjectName("radioFrontendSettings");connect_=new QPushButton("Connect");connect_->setObjectName("radioFrontendConnect");disconnect_=new QPushButton("Disconnect");disconnect_->setObjectName("radioFrontendDisconnect");stop_=new QPushButton("Stop / PTT OFF");stop_->setObjectName("radioFrontendStop");
    for(auto* button:{settings_,connect_,disconnect_,stop_}){button->setMinimumHeight(30);header->addWidget(button);}root->addLayout(header);
    connection_=new QLabel;connection_->setObjectName("radioFrontendConnection");connection_->setWordWrap(true);root->addWidget(connection_);

    auto* display=new QFrame;display->setObjectName("radioConsoleDisplay");auto* displayLayout=new QVBoxLayout(display);displayLayout->setContentsMargins(18,10,18,10);displayLayout->setSpacing(6);
    auto* badges=new QHBoxLayout;badges->setSpacing(9);stateBadge_=new QLabel("OFFLINE");stateBadge_->setObjectName("radioFrontendBadge");badges->addWidget(stateBadge_);
    auto* frequencyCaption=new QLabel("RADIO FREQUENCY");frequencyCaption->setObjectName("radioConsoleSection");badges->addWidget(frequencyCaption);badges->addStretch();
    vfoDisplay_=new QLabel("VFO —");vfoDisplay_->setObjectName("radioFrontendVfoReadout");badges->addWidget(vfoDisplay_);modeDisplay_=new QLabel("MODE —");modeDisplay_->setObjectName("radioFrontendModeReadout");badges->addWidget(modeDisplay_);displayLayout->addLayout(badges);
    auto* frequencyRow=new QHBoxLayout;frequencyDisplay_=new QLabel("—.——————");frequencyDisplay_->setObjectName("radioFrontendFrequency");frequencyDisplay_->setTextInteractionFlags(Qt::TextSelectableByMouse);frequencyDisplay_->setAccessibleName("Radio frequency in megahertz");frequencyRow->addWidget(frequencyDisplay_);
    auto* mhz=new QLabel("MHz");mhz->setObjectName("radioFrequencyUnit");frequencyRow->addWidget(mhz,0,Qt::AlignBottom);frequencyRow->addSpacing(18);
    auto* signalColumn=new QVBoxLayout;signalColumn->setSpacing(2);meterLabel_=new QLabel("Signal not available");meterLabel_->setObjectName("radioFrontendSignal");meterLabel_->setWordWrap(false);badges->insertWidget(badges->count()-2,meterLabel_);meter_=new RadioSignalMeter;meter_->setMinimumWidth(240);signalColumn->addWidget(meter_);frequencyRow->addLayout(signalColumn,1);displayLayout->addLayout(frequencyRow);
    auto* readouts=new QHBoxLayout;readouts->setSpacing(8);
    const auto readout=[&](const char* name,const QString& text){auto* label=new QLabel(text);label->setObjectName(name);label->setProperty("instrumentReadout",true);readouts->addWidget(label,1);return label;};
    filterDisplay_=readout("radioFrontendFilterReadout","FILTER  —");powerDisplay_=readout("radioFrontendPowerReadout","RF POWER  —");ritDisplay_=readout("radioFrontendRitReadout","RIT  —");displayLayout->addLayout(readouts);root->addWidget(display);

    auto* scroll=new QScrollArea;scroll_=scroll;scroll->setObjectName("radioConsoleScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);root->addWidget(scroll,1);
    auto* body=new QWidget;body->setObjectName("radioConsoleBody");scroll->setWidget(body);auto* bodyLayout=new QVBoxLayout(body);bodyLayout_=bodyLayout;bodyLayout->setContentsMargins(0,0,4,0);bodyLayout->setSpacing(12);
    auto* deck=new QWidget;deck->setObjectName("radioControlDeck");auto* lower=new QHBoxLayout(deck);lower->setContentsMargins(0,0,0,0);lower->setSpacing(12);bodyLayout->addWidget(deck);
    auto* tuning=new QGroupBox("VFO TUNING");tuning->setObjectName("radioTuningCard");auto* tuningLayout=new QVBoxLayout(tuning);tuningLayout->setSpacing(7);
    auto* entry=new QHBoxLayout;frequency_=new QDoubleSpinBox;frequency_->setObjectName("radioFrontendFrequencyEntry");frequency_->setDecimals(6);frequency_->setRange(.000001,100000);frequency_->setSuffix(" MHz");frequency_->setKeyboardTracking(false);tune_=new QPushButton("Tune");tune_->setObjectName("radioFrontendTune");entry->addWidget(frequency_,1);entry->addWidget(tune_);tuningLayout->addLayout(entry);
    auto* knobRow=new QHBoxLayout;minus_=new QPushButton("−");minus_->setObjectName("radioFrontendDown");minus_->setFixedSize(42,42);plus_=new QPushButton("+");plus_->setObjectName("radioFrontendUp");plus_->setFixedSize(42,42);
    dial_=new TuningDial;dial_->setObjectName("radioFrontendDial");dial_->setRange(0,99);dial_->setWrapping(true);dial_->setNotchesVisible(true);dial_->setValue(dialPosition_);dial_->setFixedSize(112,112);dial_->setAccessibleName("Frequency tuning dial");knobRow->addWidget(dial_);auto* dialSide=new QVBoxLayout;auto* nudge=new QHBoxLayout;nudge->addWidget(minus_);nudge->addWidget(plus_);dialSide->addLayout(nudge);dialSide->addStretch();knobRow->addLayout(dialSide,1);tuningLayout->addLayout(knobRow);
    auto* stepRow=new QHBoxLayout;auto* stepCaption=new QLabel("TUNING STEP");stepCaption->setObjectName("radioConsoleSection");stepRow->addWidget(stepCaption);step_=new QComboBox;step_->setObjectName("radioFrontendStep");for(int hz:{1,10,50,100,500,1000,2500,5000,6250,10000,12500,25000,100000})step_->addItem(hz>=1000?QString("%1 kHz").arg(hz/1000.,0,'g',5):QString("%1 Hz").arg(hz),hz);step_->setCurrentIndex(std::max(0,step_->findData(QSettings().value("radioFrontend/stepHz",1000).toInt())));stepRow->addWidget(step_,1);dialSide->addLayout(stepRow);lower->addWidget(tuning,2);
    auto* operating=new QGroupBox("OPERATING CONTROLS");operating->setObjectName("radioOperatingCard");auto* operatingLayout=new QFormLayout(operating);operatingLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);operatingLayout->setSpacing(6);
    mode_=new QComboBox;mode_->setObjectName("radioFrontendMode");mode_->setEditable(true);mode_->setInsertPolicy(QComboBox::NoInsert);mode_->addItems(radio::modeNames(config.model));bandwidth_=new QSpinBox;bandwidth_->setObjectName("radioFrontendBandwidth");bandwidth_->setRange(0,1000000);bandwidth_->setSuffix(" Hz");bandwidth_->setSpecialValueText("Auto width");bandwidth_->setKeyboardTracking(false);setMode_=new QPushButton("Set");setMode_->setObjectName("radioFrontendSetMode");auto* modeRow=new QHBoxLayout;modeRow->addWidget(mode_,1);modeRow->addWidget(bandwidth_,1);modeRow->addWidget(setMode_);operatingLayout->addRow("Mode / filter",modeRow);
    vfo_=new QComboBox;vfo_->setObjectName("radioFrontendVfo");vfo_->addItems({"VFOA","VFOB","Main","Sub","MainA","MainB","SubA","SubB","VFO","MEM"});setVfo_=new QPushButton("Set");setVfo_->setObjectName("radioFrontendSetVfo");memoryVfo_=new QPushButton("M/V");memoryVfo_->setObjectName("radioFrontendMemoryVfo");memoryVfo_->setCheckable(true);memoryVfo_->setAccessibleName("Switch memory / VFO");auto* vfoRow=new QHBoxLayout;vfoRow->addWidget(vfo_,1);vfoRow->addWidget(setVfo_);vfoRow->addWidget(memoryVfo_);operatingLayout->addRow("VFO / memory",vfoRow);
    power_=new QDoubleSpinBox;power_->setObjectName("radioFrontendPower");power_->setRange(0,100);power_->setDecimals(1);power_->setSuffix(" %");power_->setKeyboardTracking(false);powerSlider_=new QSlider(Qt::Horizontal);powerSlider_->setObjectName("radioFrontendPowerSlider");powerSlider_->setRange(0,100);setPower_=new QPushButton("Set");setPower_->setObjectName("radioFrontendSetPower");auto* powerRow=new QHBoxLayout;powerRow->addWidget(powerSlider_,1);powerRow->addWidget(power_,1);powerRow->addWidget(setPower_);operatingLayout->addRow("RF power",powerRow);
    rit_=new QSpinBox;rit_->setObjectName("radioFrontendRit");rit_->setRange(-1000000,1000000);rit_->setSuffix(" Hz");rit_->setSingleStep(10);rit_->setKeyboardTracking(false);setRit_=new QPushButton("Set");setRit_->setObjectName("radioFrontendSetRit");zeroRit_=new QPushButton("Zero");zeroRit_->setObjectName("radioFrontendZeroRit");auto* ritRow=new QHBoxLayout;ritRow->addWidget(rit_,1);ritRow->addWidget(setRit_);ritRow->addWidget(zeroRit_);operatingLayout->addRow("RIT",ritRow);
    operating->setToolTip("Edit a value, then press Set. RF power is the radio's percentage scale.");lower->addWidget(operating,3);
    auto* footer=new QHBoxLayout;read_=new QPushButton("Read radio");read_->setObjectName("radioFrontendRefresh");auto* advanced=new QPushButton("Advanced controls…");advanced->setObjectName("radioFrontendAdvanced");footer->addWidget(read_);footer->addWidget(advanced);footer->addStretch();bodyLayout->addLayout(footer);bodyLayout->addStretch();
    status_=new QLabel;status_->setObjectName("radioFrontendStatus");status_->setWordWrap(true);root->addWidget(status_);
    connect(monitorButton_,&QPushButton::clicked,this,[this]{if(monitor_&&monitor_->parentWidget()==scroll_->widget())scroll_->verticalScrollBar()->setValue(monitor_->mapTo(scroll_->widget(),QPoint{}).y());});
    connect(settings_,&QPushButton::clicked,this,&RadioFrontend::settingsRequested);connect(connect_,&QPushButton::clicked,this,&RadioFrontend::connectRequested);connect(disconnect_,&QPushButton::clicked,this,&RadioFrontend::disconnectRequested);connect(stop_,&QPushButton::clicked,this,[this]{tuningTarget_.reset();tuningTimer_.stop();pendingMode_.reset();modeTimer_.stop();emit stopRequested();});connect(advanced,&QPushButton::clicked,this,&RadioFrontend::advancedRequested);connect(read_,&QPushButton::clicked,controller_,&radio::Controller::refresh);
    connect(frequency_,&QDoubleSpinBox::valueChanged,this,[this]{frequencyDirty_=true;});connect(mode_,&QComboBox::currentTextChanged,this,[this]{modeDirty_=true;const QSignalBlocker block(bandwidth_);bandwidth_->setValue(0);});connect(bandwidth_,&QSpinBox::valueChanged,this,[this]{modeDirty_=true;});connect(vfo_,&QComboBox::currentTextChanged,this,[this]{vfoDirty_=true;});connect(rit_,&QSpinBox::valueChanged,this,[this]{ritDirty_=true;});
    connect(power_,&QDoubleSpinBox::valueChanged,this,[this](double value){powerDirty_=true;const QSignalBlocker block(powerSlider_);powerSlider_->setValue(int(std::lround(value)));});connect(powerSlider_,&QSlider::valueChanged,this,[this](int value){power_->setValue(value);});
    frequency_->setSingleStep(step_->currentData().toInt()/1e6);connect(step_,&QComboBox::currentIndexChanged,this,[this]{frequency_->setSingleStep(step_->currentData().toInt()/1e6);QSettings().setValue("radioFrontend/stepHz",step_->currentData());});connect(minus_,&QPushButton::clicked,this,[this]{tuneSteps(-1);});connect(plus_,&QPushButton::clicked,this,[this]{tuneSteps(1);});connect(dial_,&QDial::valueChanged,this,[this](int position){int delta=position-dialPosition_;dialPosition_=position;if(delta>50)delta-=100;else if(delta<-50)delta+=100;tuneSteps(delta);});
    tuningTimer_.setSingleShot(true);connect(&tuningTimer_,&QTimer::timeout,this,&RadioFrontend::flushTuning);
    connect(memoryVfo_,&QPushButton::clicked,this,&RadioFrontend::toggleMemoryVfo);
    connect(tune_,&QPushButton::clicked,this,[this]{if(!canOperate())return;tuningTarget_=frequency_->value()*1e6;flushTuning();});connect(setMode_,&QPushButton::clicked,this,[this]{requestMode();});connect(setVfo_,&QPushButton::clicked,this,[this]{apply({radio::Control::Vfo,0,vfo_->currentText()},[this]{vfoDirty_=false;});});connect(setPower_,&QPushButton::clicked,this,[this]{apply({radio::Control::Power,power_->value(),{}},[this]{powerDirty_=false;});});connect(setRit_,&QPushButton::clicked,this,[this]{apply({radio::Control::Rit,double(rit_->value()),{}},[this]{ritDirty_=false;});});connect(zeroRit_,&QPushButton::clicked,this,[this]{apply({radio::Control::Rit,0,{}},[this]{ritDirty_=false;});});
    connect(mode_,qOverload<int>(&QComboBox::activated),this,[this](int){requestMode();});
    modeTimer_.setSingleShot(true);connect(&modeTimer_,&QTimer::timeout,this,&RadioFrontend::flushMode);
    updateChoices();connect(controller_,&radio::Controller::changed,this,&RadioFrontend::refresh);refresh();
}
void RadioFrontend::setConfig(const radio::Config& config){if(config_==config)return;pendingMode_.reset();modeTimer_.stop();config_=config;updateChoices();refresh();}
void RadioFrontend::updateChoices(){
    bandwidth_->setToolTip(config_.model==2039?"Set filter width on the radio; this Hamlib TS-990S driver does not set or report it":"Receive filter width; zero uses the driver default");
    const QSignalBlocker modeBlock(mode_),vfoBlock(vfo_);const auto mode=mode_->currentText(),vfo=vfo_->currentText();
    mode_->clear();mode_->addItems(radio::modeNames(config_.model));mode_->setCurrentText(mode);
    vfo_->clear();vfo_->addItems(config_.model==2039?QStringList{"Main","Sub"}:QStringList{"VFOA","VFOB","Main","Sub","MainA","MainB","SubA","SubB","VFO","MEM"});vfo_->setCurrentText(vfo);
}
void RadioFrontend::addAudioControls(QWidget* panel){bodyLayout_->insertWidget(bodyLayout_->count()-2,panel);audioButton_->setEnabled(true);connect(audioButton_,&QPushButton::clicked,this,[this,panel]{scroll_->verticalScrollBar()->setValue(panel->mapTo(scroll_->widget(),QPoint{}).y());});}
void RadioFrontend::setSkin(const ui::Skin& skin){const auto colors=ui::radioSkin(skin);setPalette(ui::skinPalette(colors));setStyleSheet(ui::radioStyleSheet(colors));static_cast<TuningDial*>(dial_)->setColors(ui::skinPalette(colors));meter_->setColors(ui::skinPalette(colors));refresh();}
void RadioFrontend::addAudioMonitor(QWidget* monitor){monitor_=monitor;bodyLayout_->insertWidget(1,monitor);monitorButton_->setEnabled(true);}
void RadioFrontend::addStationControls(QWidget* panel){rootLayout_->insertWidget(1,panel);}
void RadioFrontend::addVoiceControls(QWidget* panel){rootLayout_->insertWidget(rootLayout_->count()-1,panel);}
void RadioFrontend::showTuning(){scroll_->verticalScrollBar()->setValue(0);}
bool RadioFrontend::canOperate() const{return controller_->connected()&&!controller_->pttActive()&&controller_->state().value("ptt",0).toInt()==0&&idle_();}
void RadioFrontend::tuneSteps(int steps){
    if(!steps||!canOperate()||!controller_->state().contains("frequencyHz"))return;
    const double base=tuningTarget_.value_or(tuningInFlight_.value_or(controller_->state().value("frequencyHz").toDouble()));tuningTarget_=std::clamp(base+steps*double(step_->currentData().toInt()),1.,1e11);{const QSignalBlocker block(frequency_);frequency_->setValue(*tuningTarget_/1e6);}frequencyDirty_=true;tuningTimer_.start(80);status_->setText(QString("Tuning requested · %1 MHz").arg(*tuningTarget_/1e6,0,'f',6));
}
void RadioFrontend::flushTuning(){
    if(!tuningTarget_)return;
    if(!canOperate()){tuningTarget_.reset();return;}
    if(controller_->busy()){tuningTimer_.start(40);return;}
    const double target=*tuningTarget_;tuningTarget_.reset();tuningInFlight_=target;status_->setText("Tuning radio…");const QPointer<RadioFrontend> guard(this);
    controller_->apply({radio::Control::Frequency,target,{}},[guard](const QString& error){if(!guard)return;guard->tuningInFlight_.reset();if(!error.isEmpty()){guard->tuningTarget_.reset();guard->tuningTimer_.stop();guard->status_->setText(error);}else {if(!guard->tuningTarget_)guard->frequencyDirty_=false;guard->status_->setText("Radio updated");}guard->refresh();});
}
void RadioFrontend::apply(radio::Command command,std::function<void()> success){
    if(!canOperate()||controller_->busy())return;
    status_->setText("Applying radio control…");const QPointer<RadioFrontend> guard(this);controller_->apply(command,[guard,success=std::move(success)](const QString& error){if(!guard)return;if(error.isEmpty()){success();guard->status_->setText("Radio updated");}else {guard->status_->setText(error);guard->controller_->refresh();}guard->refresh();});
}
void RadioFrontend::requestMode(){
    if(!canOperate())return;
    pendingMode_=radio::Command{radio::Control::Mode,config_.model==2039?0.:double(bandwidth_->value()),mode_->currentText().trimmed()};
    modeDirty_=true;flushMode();
}
void RadioFrontend::flushMode(){
    if(!pendingMode_)return;
    if(!canOperate()){pendingMode_.reset();modeTimer_.stop();return;}
    if(controller_->busy()){modeTimer_.start(40);return;}
    const auto command=*pendingMode_;pendingMode_.reset();
    apply(command,[this,command]{modeDirty_=pendingMode_.has_value()||mode_->currentText().trimmed()!=command.text;});
}
void RadioFrontend::toggleMemoryVfo(){
    // Show only confirmed readback, including when the radio rejects the request.
    refresh();
    if(!memoryVfo_->isEnabled())return;
    const bool memory=controller_->state().value("vfo").toString()=="MEM";
    const QString target=memory?(lastVfo_.isEmpty()?QString("VFO"):lastVfo_):QString("MEM");
    // A delayed dial request must not tune the newly selected memory/VFO.
    tuningTimer_.stop();tuningTarget_.reset();
    apply({radio::Control::Vfo,0,target},[this]{frequencyDirty_=modeDirty_=vfoDirty_=false;});
}
void RadioFrontend::refresh(){
    const bool connected=controller_->connected(),operable=canOperate(),ready=operable&&!controller_->busy();const auto state=controller_->state();const bool tx=controller_->pttActive()||state.value("ptt",0).toInt()!=0;
    if(connected&&!wasConnected_){frequencyDirty_=modeDirty_=vfoDirty_=powerDirty_=ritDirty_=false;lastVfo_.clear();}
    if(!connected)lastVfo_.clear();
    wasConnected_=connected;
    if(!operable){tuningTimer_.stop();tuningTarget_.reset();modeTimer_.stop();pendingMode_.reset();}
    const auto identity=config_.network?QString("Network radio"):config_.model==1?QString("Simulation · Hamlib Dummy"):QString("Direct radio connection");
    connection_->setText((connected?"Connected · ":"Disconnected · ")+identity+" · "+(config_.targetVfo=="currVFO"?QString("Current VFO"):QString("Control target: ")+config_.targetVfo));connection_->setToolTip(config_.network?QString("%1:%2").arg(config_.host).arg(config_.port):config_.device);
    connect_->setEnabled(localControlAllowed_&&config_.enabled&&!connected&&!controller_->busy()&&!controller_->pttActive()&&idle_());disconnect_->setEnabled(localControlAllowed_&&(connected||controller_->pttActive()));settings_->setEnabled(true);stop_->setEnabled(connected||controller_->pttActive()||!idle_());read_->setEnabled(connected&&!controller_->busy()&&!tx);
    QString badge=!connected?(controller_->pttActive()?"PTT ?":"OFFLINE"):state.value("ptt",-1).toInt()>0?"TX":controller_->pttActive()?"PTT PENDING":state.contains("ptt")?"RX":"PTT —";stateBadge_->setText(badge);
    const auto badgeStyle="color:"+(tx?palette().color(QPalette::BrightText):connected?palette().color(QPalette::Highlight):palette().color(QPalette::PlaceholderText)).name()+";";
    if(stateBadge_->styleSheet()!=badgeStyle)stateBadge_->setStyleSheet(badgeStyle);
    frequencyDisplay_->setText(connected&&state.contains("frequencyHz")?QString::number(state["frequencyHz"].toDouble()/1e6,'f',6):"—.——————");modeDisplay_->setText("MODE "+(connected?state.value("mode","—").toString():"—"));vfoDisplay_->setText(connected?state.value("vfo","VFO —").toString():"VFO —");
    const auto width=state.value("passband");
    filterDisplay_->setText(connected&&width.isValid()?QString("FILTER  %1 %2").arg(width.toDouble()>=1000?QString::number(width.toDouble()/1000.,'g',4):QString::number(width.toInt()),width.toDouble()>=1000?"kHz":"Hz"):QString("FILTER  —"));
    powerDisplay_->setText(connected&&state.contains("powerPercent")?QString("RF POWER  %1 %").arg(state["powerPercent"].toDouble(),0,'g',4):QString("RF POWER  —"));
    ritDisplay_->setText(connected&&state.contains("ritHz")?QString("RIT  %1 Hz").arg(state["ritHz"].toInt()):QString("RIT  —"));
    const QString currentVfo=connected?state.value("vfo").toString():QString{};
    const bool memory=currentVfo=="MEM",knownVfo=vfo_->findText(currentVfo)>=0;
    if(knownVfo&&!memory)lastVfo_=currentVfo;
    memoryVfo_->setChecked(memory);memoryVfo_->setEnabled(ready&&knownVfo&&config_.model!=2039);
    memoryVfo_->setToolTip(config_.model==2039?QString("This Hamlib TS-990S driver exposes Main/Sub selection, but not memory/VFO switching"):
        !connected?QString("Connect a radio to switch memory / VFO"):
        !knownVfo?QString("Memory / VFO switching requires VFO readback"):
        memory?QString("Memory mode · switch to %1").arg(lastVfo_.isEmpty()?QString("VFO"):lastVfo_):
        QString("VFO mode · switch to memory (if supported by this radio)"));
    const auto enable=[&](QWidget* field,QPushButton* button,const QString& key){field->setEnabled(operable&&state.value(key).isValid()&&!state.value(key).isNull());button->setEnabled(ready&&state.value(key).isValid()&&!state.value(key).isNull());};enable(frequency_,tune_,"frequencyHz");enable(mode_,setMode_,"mode");bandwidth_->setEnabled(mode_->isEnabled()&&config_.model!=2039);enable(vfo_,setVfo_,"vfo");enable(power_,setPower_,"powerPercent");powerSlider_->setEnabled(power_->isEnabled());enable(rit_,setRit_,"ritHz");zeroRit_->setEnabled(setRit_->isEnabled());for(auto* widget:QList<QWidget*>{minus_,plus_,dial_})widget->setEnabled(operable&&state.contains("frequencyHz"));
    if(connected){
        if(!frequencyDirty_&&!frequency_->hasFocus()&&state.contains("frequencyHz")){const QSignalBlocker b(frequency_);frequency_->setValue(state["frequencyHz"].toDouble()/1e6);}
        if(!modeDirty_&&!mode_->hasFocus()&&!bandwidth_->hasFocus()){const QSignalBlocker b(mode_),c(bandwidth_);if(state.contains("mode"))mode_->setCurrentText(state["mode"].toString());if(state.contains("passband"))bandwidth_->setValue(state["passband"].toInt());}
        if(!vfoDirty_&&!vfo_->hasFocus()&&state.contains("vfo")){const QSignalBlocker b(vfo_);vfo_->setCurrentText(state["vfo"].toString());}
        if(!powerDirty_&&!power_->hasFocus()&&!powerSlider_->hasFocus()&&state.contains("powerPercent")){const QSignalBlocker b(power_),c(powerSlider_);power_->setValue(state["powerPercent"].toDouble());powerSlider_->setValue(int(std::lround(power_->value())));}
        if(!ritDirty_&&!rit_->hasFocus()&&state.contains("ritHz")){const QSignalBlocker b(rit_);rit_->setValue(state["ritHz"].toInt());}
    }
    if(connected&&!tx&&state.value("strengthDb").isValid()){const int db=state["strengthDb"].toInt();meter_->setReading(db);meterLabel_->setText(db>0?QString("S9 + %1 dB").arg(db):QString("S%1 · %2 dB relative to S9").arg(std::clamp(int(std::lround(9+db/6.)),0,9)).arg(db));}else {meter_->setReading({});meterLabel_->setText(tx?"Transmit · RX meter paused":connected?"Signal strength is not reported by this radio":"Connect a radio for signal readback");}
    if(!controller_->error().isEmpty())status_->setText(controller_->error());else if(!connected)status_->setText(config_.enabled?"Ready to connect":"Open Radio setup to configure a connection");else if(!idle_()&&!tx)status_->setText("Radio controls are locked while SSTV audio is active");else if(tx)status_->setText("Transmit controls locked · Stop / PTT OFF remains available");
}
