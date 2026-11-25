#include "audio_process.h"
#include "voice_audio.h"
#include <QtEndian>
#include <algorithm>
#include <cmath>

namespace {
QStringList arguments(const QString& device,int rate=48000){return {"-q","-D",device,"-t","raw","-f","S16_LE","-r",QString::number(rate),"-c","1","--buffer-time=100000","--period-time=20000"};}
QString audioError(QProcess& process,const QString& fallback){const auto detail=QString::fromLocal8Bit(process.readAllStandardError()).trimmed();return detail.isEmpty()?fallback:detail.left(800);}
}
VoiceCapture::VoiceCapture(QObject* parent):QObject(parent){
    setObjectName("voiceCapture");
    connect(&process_,&QProcess::started,this,[this]{if(active_)emit opened();});
    connect(&process_,&QProcess::readyReadStandardOutput,this,&VoiceCapture::read);
    connect(&process_,&QProcess::errorOccurred,this,[this]{if(active_)fail(audioError(process_,process_.errorString()));});
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{if(active_)fail(audioError(process_,"Microphone stopped."));});
    watchdog_.setInterval(250);connect(&watchdog_,&QTimer::timeout,this,[this]{if(active_&&received_.elapsed()>2000)fail("Microphone stopped providing audio.");});
}
VoiceCapture::~VoiceCapture(){stop();}
void VoiceCapture::start(const QString& device,int gain){stop();setGain(gain);active_=true;received_.start();watchdog_.start();audio::startCapture(process_,arguments(device));}
void VoiceCapture::setGain(int gain){gain_=std::clamp(gain,25,300)/100.;limiter_=1;}
void VoiceCapture::stop(){active_=false;watchdog_.stop();pending_.clear();if(process_.state()!=QProcess::NotRunning){process_.kill();process_.waitForFinished(500);}if(process_.isOpen()){process_.readAllStandardOutput();process_.readAllStandardError();}}
void VoiceCapture::fail(const QString& error){stop();emit fault("Voice microphone: "+error);}
void VoiceCapture::read(){
    if(!active_)return;
    pending_+=process_.readAllStandardOutput();
    if(pending_.size()>96000){fail("Microphone audio is arriving too quickly.");return;}
    while(active_&&pending_.size()>=1920){
        auto block=pending_.first(1920);pending_.remove(0,1920);received_.restart();
        double rawPeak=0,energy=0;bool clipped=false;
        for(int n=0;n<block.size();n+=2){const auto sample=qFromLittleEndian<qint16>(block.constData()+n);rawPeak=std::max(rawPeak,std::abs(double(sample)));energy+=double(sample)*sample;clipped=clipped||sample>=32760||sample<=-32760;}
        const double peak=rawPeak*gain_;
        const auto target=peak>30000?30000/peak:1.;limiter_=std::min(target,limiter_+.01);
        for(int n=0;n<block.size();n+=2)qToLittleEndian<qint16>(qint16(std::clamp(std::lround(qFromLittleEndian<qint16>(block.constData()+n)*gain_*limiter_),-30000L,30000L)),block.data()+n);
        emit levels(rawPeak/32768.,std::sqrt(energy/960)/32768.,peak/32768.,limiter_<.999,clipped);
        emit level(int(std::min(100.,peak*limiter_*100/32768)));emit pcm(block);
    }
}
VoicePlayer::VoicePlayer(QObject* parent):QObject(parent){
    setObjectName("voicePlayer");
    connect(&process_,&QProcess::started,this,[this]{if(active_)emit opened();});
    connect(&process_,&QProcess::errorOccurred,this,[this]{if(active_)fail(audioError(process_,process_.errorString()));});
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{if(active_)fail(audioError(process_,"Radio audio output stopped."));});
    pump_.setInterval(20);pump_.setTimerType(Qt::PreciseTimer);connect(&pump_,&QTimer::timeout,this,&VoicePlayer::tick);
}
VoicePlayer::~VoicePlayer(){stop();}
void VoicePlayer::start(const QString& device){stop();active_=true;held_.start();received_.start();pump_.start();audio::startPlayback(process_,arguments(device));}
void VoicePlayer::allowPlayback(){if(active_){allowed_=true;received_.restart();}}
void VoicePlayer::push(const QByteArray& pcm){
    if(!active_)return;
    if(pcm.isEmpty()||pcm.size()>9600||pcm.size()%2){fail("Invalid voice audio packet.");return;}
    if(pending_.size()+pcm.size()>48000){fail("Wi-Fi voice audio is delayed. Release PTT and try again.");return;}
    pending_+=pcm;received_.restart();hasPcm_=true;
}
void VoicePlayer::stop(){active_=allowed_=primed_=hasPcm_=false;pump_.stop();pending_.clear();if(process_.state()!=QProcess::NotRunning){process_.kill();process_.waitForFinished(500);}if(process_.isOpen())process_.readAllStandardError();}
void VoicePlayer::fail(const QString& error){stop();emit fault("Voice output: "+error);}
void VoicePlayer::tick(){
    if(!active_)return;
    if(held_.elapsed()>180000){fail("Three-minute voice TX limit reached. Click PTT again to start a new transmission.");return;}
    if(received_.elapsed()>(hasPcm_?1500:5000)){fail("Voice audio stopped arriving; PTT released.");return;}
    if(!allowed_||process_.state()!=QProcess::Running)return;
    if(!primed_){if(pending_.size()<11520)return;primed_=true;}
    if(process_.bytesToWrite()>19200){fail("Radio audio output is stalled.");return;}
    auto block=pending_.first(1920);pending_.remove(0,block.size());block.resize(1920,0);
    if(process_.write(block)!=block.size())fail("Cannot write radio voice audio.");
}

RemoteSpeaker::RemoteSpeaker(QObject* parent):QObject(parent){
    setObjectName("remoteSpeakerPlayer");
    connect(&process_,&QProcess::errorOccurred,this,[this]{if(active_)fail(audioError(process_,process_.errorString()));});
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{if(active_)fail(audioError(process_,"Speaker output stopped."));});
    pump_.setInterval(20);pump_.setTimerType(Qt::PreciseTimer);connect(&pump_,&QTimer::timeout,this,&RemoteSpeaker::tick);
}
RemoteSpeaker::~RemoteSpeaker(){stop();}
void RemoteSpeaker::start(const QString& device,int volume){stop();setVolume(volume);active_=true;received_.start();pump_.start();audio::startPlayback(process_,arguments(device,12000));}
void RemoteSpeaker::setVolume(int volume){volume_=std::clamp(volume,0,100);}
void RemoteSpeaker::push(const QByteArray& pcm){
    if(!active_||pcm.isEmpty())return;
    // Capture/network boundaries may split samples. Keep their bytes in order;
    // only drop whole samples when catching up after a network burst.
    pending_+=pcm;received_.restart();
    if(pending_.size()>12000){const auto drop=(pending_.size()-4800)/2*2;pending_.remove(0,drop);primed_=false;}
}
void RemoteSpeaker::stop(){active_=primed_=false;pump_.stop();pending_.clear();if(process_.state()!=QProcess::NotRunning){process_.kill();process_.waitForFinished(500);}if(process_.isOpen())process_.readAllStandardError();}
void RemoteSpeaker::fail(const QString& error){stop();emit fault("Laptop speaker: "+error);}
void RemoteSpeaker::tick(){
    if(!active_||process_.state()!=QProcess::Running)return;
    if(received_.elapsed()>750){pending_=pending_.right(pending_.size()%2);primed_=false;}
    if(!primed_){if(pending_.size()<1920)return;primed_=true;}
    if(process_.bytesToWrite()>4800){fail("Speaker output is stalled.");return;}
    const auto count=std::min(qsizetype(480),pending_.size()/2*2);auto block=pending_.first(count);pending_.remove(0,count);block.resize(480,0);
    if(volume_!=100)for(int n=0;n<block.size();n+=2)qToLittleEndian<qint16>(qint16(qFromLittleEndian<qint16>(block.constData()+n)*volume_/100),block.data()+n);
    if(process_.write(block)!=block.size())fail("Cannot write speaker audio.");
}
