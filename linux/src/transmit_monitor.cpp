#include "transmit_monitor.h"
#include <QFile>
#include <QFileInfo>
#include <QtConcurrentRun>
#include <QtEndian>
#include <algorithm>
#include <stdexcept>

TransmitMonitor::TransmitMonitor(QObject* parent):QObject(parent){
    setObjectName("remoteTransmitMonitor");timer_.setInterval(250);
    connect(&timer_,&QTimer::timeout,this,&TransmitMonitor::advance);
    connect(&job_,&QFutureWatcher<Result>::finished,this,[this]{
        const auto result=job_.result();
        busy_=false;
        if(!state_||result.generation!=generation_){if(state_)advance();return;}
        if(!result.error.isEmpty()){
            const auto error=result.error;state_->cancel->store(true);state_.reset();timer_.stop();
            emit finished(false,"Remote TX monitor unavailable: "+error);return;
        }
        if(!result.pcm.isEmpty())emit audio(result.pcm);
        if(!state_||result.generation!=generation_)return;
        for(const auto& frame:result.frames){
            if(frame.metadataOnly||frame.image.isNull()||frame.rows<=0)continue;
            complete_=complete_||frame.complete;emit picture(frame);
            if(!state_||result.generation!=generation_)return;
        }
        if(draining_&&(state_->stream?pendingPcm_.isEmpty():state_->position>=state_->total)){
            const bool complete=complete_;state_.reset();timer_.stop();
            emit finished(complete,complete?"Remote TX monitor · Image complete":"Remote TX monitor · Playback ended without a complete image");
        }else if(draining_||(state_->stream&&!pendingPcm_.isEmpty()))advance();
    });
}
TransmitMonitor::~TransmitMonitor(){blockSignals(true);stop();job_.waitForFinished();}
void TransmitMonitor::start(const QString& path){
    stop();state_=std::make_shared<State>();state_->path=path;
    // Remote uploads have a validated canonical mono 48 kHz / signed-16 header.
    state_->total=std::max(qint64(0),(QFileInfo(path).size()-44+7)/8);
    draining_=complete_=false;clock_.start();timer_.start();
}
void TransmitMonitor::startStream(){
    stop();state_=std::make_shared<State>();state_->stream=true;draining_=complete_=false;timer_.start();
}
void TransmitMonitor::pushPcm(const QByteArray& pcm){
    if(!state_||!state_->stream||draining_)return;
    if(pendingPcm_.size()+pcm.size()>12000*2*600){stop();return;}
    pendingPcm_+=pcm;advance();
}
void TransmitMonitor::stop(){
    ++generation_;timer_.stop();clock_.invalidate();pendingPcm_.clear();
    if(!state_)return;
    state_->cancel->store(true);state_.reset();draining_=false;
    emit finished(complete_,complete_?"Remote TX monitor · Image complete · Playback stopped":"Remote TX monitor · Stopped");
}
void TransmitMonitor::finish(bool success){
    if(!state_)return;
    if(!success){stop();return;}
    draining_=true;advance();
}
void TransmitMonitor::advance(){
    if(!state_||busy_)return;
    const auto state=state_;const auto generation=generation_;
    QByteArray stream;
    if(state->stream){const auto count=pendingPcm_.size()/2*2;stream=pendingPcm_.first(count);pendingPcm_.remove(0,count);}
    const auto target=state->stream?state->position+stream.size()/2:draining_?state->total:std::min(state->total,clock_.elapsed()*12);
    if(target<=state->position){
        if(draining_){const bool complete=complete_;state_.reset();timer_.stop();emit finished(complete,complete?"Remote TX monitor · Image complete":"Remote TX monitor · No supported SSTV image");}
        return;
    }
    busy_=true;job_.setFuture(QtConcurrent::run([state,target,generation,stream=std::move(stream)]{
        Result result;result.generation=generation;
        try{
            if(state->cancel->load())throw std::runtime_error("Cancelled");
            QByteArray raw=stream;const qint64 stride=state->stream?2:8;
            if(!state->stream){
                QFile file(state->path);if(!file.open(QIODevice::ReadOnly)||!file.seek(44+state->position*8))throw std::runtime_error("Cannot read played waveform");
                const auto bytes=std::min((target-state->position)*8,file.size()-file.pos());raw=file.read(bytes);if(raw.size()!=bytes||raw.size()<2)throw std::runtime_error("Played waveform was truncated");
            }
            sstv::Audio audio;audio.sampleRate=12000;audio.samples.resize(size_t((raw.size()+stride-1)/stride));result.pcm.resize(qsizetype(audio.samples.size()*2));
            for(size_t n=0;n<audio.samples.size();++n){const auto sample=qFromLittleEndian<qint16>(raw.constData()+n*stride);audio.samples[n]=sample/32768.f;qToLittleEndian<qint16>(sample,result.pcm.data()+n*2);}
            result.frames=state->receiver.push(audio,state->cancel);state->position=target;
        }catch(const std::exception& e){result.error=QString::fromUtf8(e.what());}
        return result;
    }));
}
