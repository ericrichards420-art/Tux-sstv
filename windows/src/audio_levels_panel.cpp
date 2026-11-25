#include "audio_process.h"
#include "audio_levels_panel.h"
#include "skins.h"
#include <QtConcurrentRun>
#include <QtWidgets>
#include <algorithm>
#include <cmath>
#include <numbers>

class AudioPeakMeter:public QWidget {
public:
    explicit AudioPeakMeter(QWidget* parent=nullptr):QWidget(parent){setMinimumHeight(62);setAccessibleName("Audio peak and RMS level in dBFS");}
    void setColors(const QPalette& colors){colors_=colors;update();}
    void setLevel(sstv::AudioLevel level,bool sourceClip=false){level_=level;sourceClip_=sourceClip;setProperty("hasReading",level.samples!=0);setProperty("peakDb",level.samples?QVariant(sstv::audioDb(level.peak)):QVariant{});setProperty("rmsDb",level.samples?QVariant(sstv::audioDb(level.rms)):QVariant{});setProperty("clipping",sourceClip||level.clipped);update();}
protected:
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),colors_.color(QPalette::Base));const QRectF track(5,7,std::max(1,width()-10),20);const auto x=[&](double db){return track.left()+std::clamp((db+60)/60.,0.,1.)*track.width();};
        p.fillRect(track,colors_.color(QPalette::AlternateBase));p.fillRect(QRectF(x(-30),track.top(),x(-3)-x(-30),track.height()),QColor(colors_.color(QPalette::Base).lightness()>128?"#d6eadf":"#183e37"));
        if(level_.samples){const double peak=sstv::audioDb(level_.peak),rms=sstv::audioDb(level_.rms);const bool light=colors_.color(QPalette::Base).lightness()>128;const auto color=sourceClip_||level_.clipped||peak>-.2?QColor(light?"#b7223f":"#ff7e87"):peak> -3?QColor(light?"#8a5a05":"#ffd37a"):QColor(light?"#087751":"#55ddbb");p.fillRect(QRectF(track.left(),track.top()+4,x(rms)-track.left(),track.height()-8),color.darker(150));p.setPen(QPen(color,3));p.drawLine(QPointF(x(peak),track.top()),QPointF(x(peak),track.bottom()));}
        p.setPen(colors_.color(QPalette::Text));QFont font("DejaVu Sans");font.setPixelSize(10);p.setFont(font);for(int db:{-60,-40,-30,-20,-12,-6,0}){const double at=x(db);p.drawLine(QPointF(at,29),QPointF(at,33));p.drawText(QRectF(std::clamp(at-14.,0.,std::max(0.,width()-28.)),35,28,16),Qt::AlignCenter,QString::number(db));}
    }
private:
    QPalette colors_=ui::skinPalette(ui::radioSkin(ui::skinById("classic")));
    sstv::AudioLevel level_;
    bool sourceClip_=false;
};
namespace {
QString levelText(const sstv::AudioLevel& level){return level.samples?QString("Peak %1 dBFS · RMS %2 dBFS").arg(sstv::audioDb(level.peak),0,'f',1).arg(sstv::audioDb(level.rms),0,'f',1):QString("No audio measured");}
}
AudioLevelsPanel::AudioLevelsPanel(std::function<bool()> canCheck,QWidget* parent):QGroupBox("AUDIO LEVELS",parent),canCheck_(std::move(canCheck)){
    setObjectName("radioAudioLevels");auto* root=new QVBoxLayout(this);auto* top=new QHBoxLayout;devices_=new QLabel;devices_->setObjectName("audioLevelDevices");devices_->setWordWrap(true);top->addWidget(devices_,1);auto* back=new QPushButton("Back to tuning");back->setObjectName("audioLevelBack");top->addWidget(back);connect(back,&QPushButton::clicked,this,&AudioLevelsPanel::tuningRequested);devicesButton_=new QPushButton("Setup wizard…");devicesButton_->setObjectName("audioLevelDevicesButton");top->addWidget(devicesButton_);root->addLayout(top);
    auto* columns=new QHBoxLayout;columns->setSpacing(20);root->addLayout(columns);
    auto* rx=new QVBoxLayout;auto* tx=new QVBoxLayout;columns->addLayout(rx,1);columns->addLayout(tx,1);
    rx->addWidget(new QLabel("RECEIVE / MICROPHONE INPUT"));rxMeter_=new AudioPeakMeter;rxMeter_->setObjectName("receiveAudioMeter");rx->addWidget(rxMeter_);rxStatus_=new QLabel("Input meter stopped");rxStatus_->setObjectName("receiveAudioStatus");rxStatus_->setWordWrap(true);rx->addWidget(rxStatus_);
    auto* rxRow=new QHBoxLayout;rxRow->addWidget(new QLabel("RX gain"));rxSlider_=new QSlider(Qt::Horizontal);rxSlider_->setObjectName("receiveAudioGainSlider");rxSlider_->setRange(-24,24);rxValue_=new QSpinBox;rxValue_->setObjectName("receiveAudioGain");rxValue_->setRange(-24,24);rxValue_->setSuffix(" dB");rxRow->addWidget(rxSlider_,1);rxRow->addWidget(rxValue_);rx->addLayout(rxRow);inputCheck_=new QPushButton("Start input meter");inputCheck_->setObjectName("audioLevelListen");rx->addWidget(inputCheck_);
    autoRx_=new QCheckBox("Automatically prevent RX clipping");autoRx_->setObjectName("automaticReceiveLevel");autoRx_->setChecked(QSettings().value("audio/autoRxLevel",true).toBool());autoRx_->setToolTip("Reduces RX gain before samples clip. If the device input is already clipped, lowers a supported capture volume on the selected card. Transmit levels stay unchanged.");rx->addWidget(autoRx_);
    connect(autoRx_,&QCheckBox::toggled,this,[this](bool enabled){if(!restoringLevels_){QSettings().setValue("audio/autoRxLevel",enabled);emit levelsChanged();}clippedSamples_=0;captureLevelUnavailable_=false;captureLevelNote_.clear();captureLevelClock_.invalidate();});
    connect(&captureLevelJob_,&QFutureWatcher<audio::CaptureAdjustment>::finished,this,[this]{if(closed_)return;const auto result=captureLevelJob_.result();if(result.pcm!=input_)return;captureLevelUnavailable_=!result.changed;captureLevelNote_=result.message;captureLevelClock_.start();});
    tx->addWidget(new QLabel("SSTV TRANSMIT OUTPUT"));txMeter_=new AudioPeakMeter;txMeter_->setObjectName("transmitAudioMeter");tx->addWidget(txMeter_);txStatus_=new QLabel("TX audio stopped");txStatus_->setObjectName("transmitAudioStatus");txStatus_->setWordWrap(true);tx->addWidget(txStatus_);
    auto* txRow=new QHBoxLayout;txRow->addWidget(new QLabel("TX level"));txSlider_=new QSlider(Qt::Horizontal);txSlider_->setObjectName("transmitAudioLevelSlider");txSlider_->setRange(0,100);txValue_=new QSpinBox;txValue_->setObjectName("transmitAudioLevel");txValue_->setRange(0,100);txValue_->setSuffix(" %");txRow->addWidget(txSlider_,1);txRow->addWidget(txValue_);tx->addLayout(txRow);tone_=new QPushButton("Play 1 s output check");tone_->setObjectName("audioLevelTone");tx->addWidget(tone_);
    QSettings settings;rxValue_->setValue(std::clamp(settings.value("audio/rxGainDb",0).toInt(),-24,24));rxSlider_->setValue(rxValue_->value());txValue_->setValue(std::clamp(settings.value("audio/txLevelPercent",100).toInt(),0,100));txSlider_->setValue(txValue_->value());
    auto* note=new QLabel("Green marks digital headroom, not the radio's RF level. Lower the microphone/interface input if it clips before RX gain. For speaker-to-radio-microphone SSTV, also check the radio's ALC or deviation. Set TX level before sending. Output checks play a 1900 Hz tone at the selected TX level; VOX can respond.");note->setWordWrap(true);note->setObjectName("audioLevelHelp");root->addWidget(note);
    connect(rxSlider_,&QSlider::valueChanged,rxValue_,&QSpinBox::setValue);connect(rxValue_,&QSpinBox::valueChanged,rxSlider_,&QSlider::setValue);connect(rxValue_,&QSpinBox::valueChanged,this,[this](int value){if(!restoringLevels_){QSettings().setValue("audio/rxGainDb",value);emit levelsChanged();}});connect(txSlider_,&QSlider::valueChanged,txValue_,&QSpinBox::setValue);connect(txValue_,&QSpinBox::valueChanged,txSlider_,&QSlider::setValue);connect(txValue_,&QSpinBox::valueChanged,this,[this](int value){if(!restoringLevels_){QSettings().setValue("audio/txLevelPercent",value);emit levelsChanged();}});
    connect(inputCheck_,&QPushButton::clicked,this,&AudioLevelsPanel::startInputCheck);connect(tone_,&QPushButton::clicked,this,&AudioLevelsPanel::playTone);connect(devicesButton_,&QPushButton::clicked,this,[this]{stopChecks();emit devicesRequested();});
    connect(&capture_,&QProcess::started,this,[this]{if(!closed_)emit inputMonitorStarted();});
    connect(&capture_,&QProcess::readyReadStandardOutput,this,[this]{if(!closed_){const auto block=inputGain_.push(capture_.readAllStandardOutput(),receiveGainDb(),automaticReceiveLevel());receiveLevels(block);emit inputMonitorPcm(block.pcm);}});
    connect(&capture_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{if(!closed_)emit inputMonitorStopped();});
    connect(&capture_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(!closed_&&error==QProcess::FailedToStart)emit inputMonitorStopped();});
    connect(&playback_,&QProcess::started,this,[this]{testClock_.start();meterTimer_.start(100);});connect(&meterTimer_,&QTimer::timeout,this,[this]{if(!closed_&&testClock_.isValid()&&!testEnvelope_.empty()){const auto index=size_t(testClock_.elapsed()/100);if(index<testEnvelope_.size()){txMeter_->setLevel(testEnvelope_[index]);txStatus_->setText(levelText(testEnvelope_[index]));}}});
    auto watch=[this](QProcess& process,QLabel* status){connect(&process,&QProcess::stateChanged,this,[this](QProcess::ProcessState){if(!closed_){updateButtons();emit activityChanged();}});connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,&process,status](int code,QProcess::ExitStatus exit){if(closed_)return;status->setText(stopping_?"Audio check stopped":code==0&&exit==QProcess::NormalExit?"Audio check finished":"Audio check failed: "+QString::fromLocal8Bit(process.readAllStandardError()).left(350));limit_.stop();if(&process==&playback_){meterTimer_.stop();txMeter_->setLevel({});}else rxMeter_->setLevel({});updateButtons();emit activityChanged();});connect(&process,&QProcess::errorOccurred,this,[this,&process,status](QProcess::ProcessError error){if(closed_)return;if(error==QProcess::FailedToStart){status->setText("Cannot open audio device: "+process.errorString());limit_.stop();updateButtons();emit activityChanged();}});};watch(capture_,rxStatus_);watch(playback_,txStatus_);limit_.setSingleShot(true);connect(&limit_,&QTimer::timeout,this,&AudioLevelsPanel::stopChecks);setDevices("default","default");updateButtons();
}
AudioLevelsPanel::~AudioLevelsPanel(){shutdown();}
void AudioLevelsPanel::setRemoteAudio(bool remote){if(remoteAudio_!=remote)stopChecks();remoteAudio_=remote;updateButtons();}
void AudioLevelsPanel::setSkin(const ui::Skin& skin){const auto colors=ui::skinPalette(ui::radioSkin(skin));rxMeter_->setColors(colors);txMeter_->setColors(colors);}
int AudioLevelsPanel::transmitPercent() const{return txValue_->value();}
void AudioLevelsPanel::setTransmitPercent(int value){txValue_->setValue(std::clamp(value,0,100));}
void AudioLevelsPanel::restoreLevels(int tx,int rx,bool automatic){restoringLevels_=true;txValue_->setValue(std::clamp(tx,0,100));rxValue_->setValue(std::clamp(rx,-24,24));autoRx_->setChecked(automatic);restoringLevels_=false;}
void AudioLevelsPanel::setRemoteTransmitLevel(int percent){
    if(remoteTransmitLevel_==percent)return;
    remoteTransmitLevel_=percent;restoringLevels_=true;txValue_->setValue(percent>=0?std::clamp(percent,0,100):std::clamp(QSettings().value("audio/txLevelPercent",100).toInt(),0,100));restoringLevels_=false;updateButtons();
}
int AudioLevelsPanel::receiveGainDb() const{return rxValue_->value();}
bool AudioLevelsPanel::automaticReceiveLevel() const{return autoRx_->isChecked();}
bool AudioLevelsPanel::checking() const{return capture_.state()!=QProcess::NotRunning||playback_.state()!=QProcess::NotRunning;}
void AudioLevelsPanel::setDevices(const QString& input,const QString& output){if(input_!=input||output_!=output)stopChecks();if(input_!=input){captureLevelUnavailable_=false;captureLevelNote_.clear();clippedSamples_=0;captureLevelClock_.invalidate();}input_=input;output_=output;devices_->setText("RX: "+input_+" · TX: "+output_);}
void AudioLevelsPanel::setApplicationState(bool receiving,bool transmitting,bool preparing,bool busy,bool monitoring){if(receiving_&&!receiving){rxMeter_->setLevel({});rxStatus_->setText("RX audio stopped");}receiving_=receiving;transmitting_=transmitting;preparing_=preparing;busy_=busy;monitoring_=monitoring;if((receiving||transmitting||preparing||busy)&&checking())stopChecks();updateButtons();}
void AudioLevelsPanel::receiveLevels(const sstv::PcmGainBlock& block){
    if(!block.adjusted.samples)return;
    if(automaticReceiveLevel()&&block.gainReduced)rxValue_->setValue(block.gainDb);
    rxMeter_->setLevel(block.adjusted,block.input.clipped);
    QString message=block.input.clipped?"Input clipped before gain · lower the device input level":block.adjusted.clipped?"RX gain is clipping · reduce gain":levelText(block.adjusted)+QString("\nSource peak %1 dBFS").arg(sstv::audioDb(block.input.peak),0,'f',1);
    if(automaticReceiveLevel()){
        message=block.gainReduced?QString("Auto RX reduced gain to %1 dB · ").arg(block.gainDb)+message:message;
        clippedSamples_=block.input.clipped?clippedSamples_+block.input.samples:0;
        if(block.input.clipped){
            if(!remoteAudio_&&clippedSamples_>=1200&&!captureLevelUnavailable_&&!captureLevelJob_.isRunning()&&(!captureLevelClock_.isValid()||captureLevelClock_.elapsed()>=2000)){
                const auto input=input_;captureLevelJob_.setFuture(QtConcurrent::run([input]{return audio::reduceCapture(input);}));clippedSamples_=0;captureLevelClock_.start();
            }
            message+=captureLevelJob_.isRunning()?"\nAuto RX is lowering device input volume…":captureLevelNote_.isEmpty()?QString{}:"\n"+captureLevelNote_;
        }
    }
    rxStatus_->setText(message);
}
void AudioLevelsPanel::setTransmitEnvelope(std::vector<sstv::AudioLevel> levels){transmitEnvelope_=std::move(levels);}
void AudioLevelsPanel::transmitPosition(qint64 milliseconds){if(milliseconds<0)return;const auto index=size_t(milliseconds/100);if(index<transmitEnvelope_.size()){txMeter_->setLevel(transmitEnvelope_[index]);txStatus_->setText(levelText(transmitEnvelope_[index]));}}
void AudioLevelsPanel::endTransmitMeter(){txMeter_->setLevel({});if(!checking())txStatus_->setText("TX audio stopped");}
void AudioLevelsPanel::startInputCheck(){if(capture_.state()!=QProcess::NotRunning){stopChecks();return;}if(!canCheck_())return;stopChecks();inputGain_.reset();rxStatus_->setText("Opening input…");audio::startCapture(capture_,{"-q","-D",input_,"-t","raw","-f","S16_LE","-r","12000","-c","1"});limit_.start(30000);updateButtons();}
void AudioLevelsPanel::playTone(){
    if(playback_.state()!=QProcess::NotRunning){stopChecks();return;}if(!canCheck_())return;stopChecks();
    try{sstv::Audio audio;audio.sampleRate=48000;audio.samples.resize(48000);for(int n=0;n<48000;++n){const double ramp=std::min({1.,n/240.,(47999-n)/240.});audio.samples[size_t(n)]=float(.65*ramp*std::sin(2*std::numbers::pi*1900*n/48000));}sstv::setAudioOutputLevel(audio,transmitPercent());testEnvelope_=sstv::audioEnvelope(audio);if(!temporary_.isValid())throw std::runtime_error("Cannot create test audio file.");const auto path=temporary_.filePath("output-check.wav");sstv::writeWav(path,audio);txStatus_->setText("Playing output check…");audio::startPlayback(playback_,{"-q","-D",output_,path});limit_.start(5000);updateButtons();}catch(const std::exception& e){txStatus_->setText(QString::fromUtf8(e.what()));}
}
void AudioLevelsPanel::stopChecks(){
    if(stopping_)return;
    stopping_=true;const bool active=checking();limit_.stop();meterTimer_.stop();capture_.kill();playback_.kill();if(capture_.state()!=QProcess::NotRunning)capture_.waitForFinished(500);if(playback_.state()!=QProcess::NotRunning)playback_.waitForFinished(500);inputGain_.reset();stopping_=false;if(!closed_){if(!receiving_)rxMeter_->setLevel({});if(!transmitting_)txMeter_->setLevel({});updateButtons();if(active)emit activityChanged();}
}
void AudioLevelsPanel::shutdown(){if(closed_)return;closed_=true;blockSignals(true);stopChecks();captureLevelJob_.waitForFinished();}
void AudioLevelsPanel::updateButtons(){if(closed_)return;const bool input=capture_.state()!=QProcess::NotRunning,output=playback_.state()!=QProcess::NotRunning;const bool idle=!remoteAudio_&&!receiving_&&!transmitting_&&!preparing_&&!busy_&&canCheck_();inputCheck_->setText(input?"Stop input meter":receiving_?"RX meter active":"Start input meter");inputCheck_->setEnabled(input||idle);tone_->setText(output?"Stop output check":"Play 1 s output check");tone_->setEnabled(output||idle);devicesButton_->setEnabled(true);txSlider_->setEnabled(remoteTransmitLevel_<0&&!transmitting_&&!preparing_&&!output);txValue_->setEnabled(txSlider_->isEnabled());}
