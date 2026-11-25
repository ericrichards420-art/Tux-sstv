#include "mic_check_dialog.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>

namespace {
double db(double value){return value>0?20*std::log10(value):-90.;}
}

class MicLevelMeter:public QWidget {
    double db_=-90.;
public:
    explicit MicLevelMeter(QWidget* parent=nullptr):QWidget(parent){setMinimumSize(360,65);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);}
    void setLevel(double value){const auto next=db(value);if(std::abs(next-db_)<.05)return;db_=next;setProperty("peakDb",db_);setAccessibleDescription(QString("Peak %1 dBFS").arg(db_,0,'f',1));update();}
protected:
    void paintEvent(QPaintEvent*) override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const bool dark=palette().color(QPalette::Window).lightness()<128;
        const QRectF bar(10,5,width()-20,20);const double segment=bar.width()/60.;
        for(int i=0;i<60;++i){const auto level=-60+i+1;const auto color=level<=-18?QColor(dark?"#d4b362":"#9e790e"):level<=-6?QColor(dark?"#50e2b8":"#248252"):QColor(dark?"#ff8d99":"#b52a35");p.setPen(Qt::NoPen);p.setBrush(db_>=level?color:palette().color(QPalette::Mid).darker(dark?170:110));p.drawRoundedRect(QRectF(bar.left()+i*segment,bar.top(),std::max(1.,segment-1),bar.height()),1,1);}
        p.setPen(palette().color(QPalette::Text));QFont font=p.font();font.setPixelSize(10);p.setFont(font);
        for(int tick:{-60,-36,-24,-18,-12,-6,0}){const double x=bar.x()+(tick+60)/60.*bar.width();p.drawText(QRectF(x-12,29,24,15),Qt::AlignCenter,QString::number(tick));}
        p.drawText(QRect(10,46,width()-20,18),Qt::AlignLeft,QString("Peak %1 dBFS").arg(db_<-80?QString("—"):QString::number(db_,'f',1)));
    }
};

MicCheckDialog::MicCheckDialog(const QStringList& devices,const QString& selected,int gain,QWidget* parent):QDialog(parent),capture_(this){
    setObjectName("micCheckDialog");setWindowTitle("Microphone check — No transmission");resize(580,480);
    auto* root=new QVBoxLayout(this);auto* help=new QLabel("Speak normally into your microphone. Aim for voice peaks in the green region, between −18 and −6 dBFS. This check does not transmit.");help->setWordWrap(true);root->addWidget(help);
    auto* form=new QFormLayout;microphone_=new QComboBox;microphone_->setObjectName("micCheckDevice");microphone_->setEditable(true);microphone_->addItems(devices);if(microphone_->findText(selected)<0)microphone_->addItem(selected);microphone_->setCurrentText(selected);form->addRow("Microphone on this computer",microphone_);
    gain_=new QSpinBox;gain_->setObjectName("micCheckGain");gain_->setRange(25,300);gain_->setSuffix(" %");gain_->setKeyboardTracking(false);gain_->setValue(gain);form->addRow("Mic gain",gain_);root->addLayout(form);
    root->addWidget(new QLabel("Original microphone input"));inputMeter_=new MicLevelMeter;inputMeter_->setObjectName("micCheckInputMeter");inputMeter_->setAccessibleName("Original microphone input peak");root->addWidget(inputMeter_);
    root->addWidget(new QLabel("Voice level with mic gain · before peak limiting"));voiceMeter_=new MicLevelMeter;voiceMeter_->setObjectName("micCheckVoiceMeter");voiceMeter_->setAccessibleName("Voice peak with microphone gain");root->addWidget(voiceMeter_);
    assessment_=new QLabel;assessment_->setObjectName("micCheckAssessment");assessment_->setTextFormat(Qt::PlainText);assessment_->setWordWrap(true);assessment_->setFixedHeight(assessment_->fontMetrics().lineSpacing()*3+6);root->addWidget(assessment_);
    state_=new QLabel;state_->setObjectName("micCheckState");state_->setTextFormat(Qt::PlainText);state_->setWordWrap(true);root->addWidget(state_);
    auto* controls=new QHBoxLayout;toggle_=new QPushButton("Stop check");toggle_->setObjectName("micCheckToggle");controls->addWidget(toggle_);auto* reset=new QPushButton("Reset readings");reset->setObjectName("micCheckReset");controls->addWidget(reset);controls->addStretch();root->addLayout(controls);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);apply_=buttons->addButton("Use settings",QDialogButtonBox::AcceptRole);apply_->setObjectName("micCheckApply");apply_->setEnabled(false);root->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    clock_.start();refresh_.setInterval(100);connect(&refresh_,&QTimer::timeout,this,&MicCheckDialog::updateReadings);
    connect(&capture_,&VoiceCapture::levels,this,[this](double input,double rms,double voice,bool limited,bool clipped){readings_.push_back({clock_.elapsed(),input,rms,voice,limited,clipped});while(readings_.size()>100)readings_.pop_front();apply_->setEnabled(true);});
    connect(&capture_,&VoiceCapture::opened,this,[this]{state_->setText("Checking microphone · Not transmitting");});
    connect(&capture_,&VoiceCapture::fault,this,[this](const QString& error){refresh_.stop();toggle_->setText("Start check");apply_->setEnabled(false);state_->setText(error);assess("error","Could not check this microphone. Choose another input or start the check again.");});
    connect(toggle_,&QPushButton::clicked,this,[this]{if(capture_.active()){capture_.stop();refresh_.stop();toggle_->setText("Start check");state_->setText("Stopped · Last readings shown");}else startCheck();});
    connect(reset,&QPushButton::clicked,this,&MicCheckDialog::resetReadings);
    connect(gain_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int value){const bool checked=apply_->isEnabled();capture_.setGain(value);resetReadings();apply_->setEnabled(checked);});
    const auto chooseDevice=[this]{if(microphone()==device_)return;device_=microphone();if(capture_.active())startCheck();else resetReadings();};connect(microphone_,qOverload<int>(&QComboBox::activated),this,[chooseDevice](int){chooseDevice();});connect(microphone_->lineEdit(),&QLineEdit::editingFinished,this,chooseDevice);
    resetReadings();QTimer::singleShot(0,this,[this]{if(isVisible())startCheck();});
}
MicCheckDialog::~MicCheckDialog(){capture_.stop();}
QString MicCheckDialog::microphone() const{const auto selected=microphone_->currentText().trimmed();return selected.isEmpty()?QString("default"):selected;}
int MicCheckDialog::gainPercent() const{return gain_->value();}
void MicCheckDialog::done(int result){refresh_.stop();capture_.stop();QDialog::done(result);}
void MicCheckDialog::startCheck(){device_=microphone();resetReadings();state_->setText("Opening microphone…");toggle_->setText("Stop check");capture_.start(device_,gainPercent());refresh_.start();}
void MicCheckDialog::resetReadings(){readings_.clear();inputMeter_->setLevel(0);voiceMeter_->setLevel(0);apply_->setEnabled(false);assess("waiting","Speak normally for a few seconds to check your level.");}
void MicCheckDialog::assess(const QString& state,const QString& text){
    if(assessment_->text()!=text)assessment_->setText(text);
    assessment_->setProperty("levelState",state);
    const bool dark=palette().color(QPalette::Window).lightness()<128;const auto color=state=="good"?(dark?"#50e2b8":"#216a3b"):state=="clipping"||state=="limited"||state=="hot"||state=="error"?(dark?"#ff8d99":"#a61b1b"):palette().color(QPalette::Text).name();
    const auto style=QString("font-weight:bold;color:%1;").arg(color);if(assessment_->styleSheet()!=style)assessment_->setStyleSheet(style);
}
void MicCheckDialog::updateReadings(){
    const auto now=clock_.elapsed();while(!readings_.empty()&&now-readings_.front().at>1200)readings_.pop_front();
    if(readings_.empty()){inputMeter_->setLevel(0);voiceMeter_->setLevel(0);assess("waiting","Waiting for microphone audio…");return;}
    double input=0,voice=0,energy=0;bool limited=false,clipped=false;for(const auto& r:readings_){input=std::max(input,r.input);voice=std::max(voice,r.voice);energy+=r.rms*r.rms;limited=limited||r.limited;clipped=clipped||r.clipped;}
    inputMeter_->setLevel(input);voiceMeter_->setLevel(voice);
    state_->setText(QString("Checking microphone · Average input %1 dBFS · Not transmitting").arg(db(std::sqrt(energy/readings_.size())),0,'f',1));
    if(clipped)assess("clipping","Microphone input is clipping. Lower Ubuntu's microphone input level or move farther away. Mic gain cannot repair clipped input.");
    else if(limited)assess("limited","Too loud · The limiter is reducing voice peaks. Lower Mic gain until normal speech stays in the green region.");
    else if(db(voice)>-6)assess("hot","Voice level is high. Lower Mic gain a little to leave room for louder words.");
    else if(db(input)<-55)assess("silent","No speech detected. Speak into the selected microphone, or check the microphone choice and Ubuntu input level.");
    else if(db(voice)<-18)assess("quiet",gain_->value()>=300?"Voice is too quiet at maximum Mic gain. Move closer or raise Ubuntu's microphone input level.":"Voice is quiet. Speak closer or raise Mic gain, then speak normally again.");
    else assess("good","Good speech level · Voice peaks have useful headroom without clipping.");
}
