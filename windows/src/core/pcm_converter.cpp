#include "pcm_converter.h"
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace audio {
PcmConverter::PcmConverter(int inputRate,int inputChannels,int outputRate,int outputChannels):inputRate_(inputRate),inputChannels_(inputChannels),outputRate_(outputRate),outputChannels_(outputChannels){
    if(inputRate<8000||inputRate>192000||outputRate<8000||outputRate>192000||inputChannels<1||inputChannels>8||outputChannels<1||outputChannels>8)
        throw std::runtime_error("Unsupported audio format.");
    if(inputRate==outputRate)return;
    const double ratio=double(inputRate)/outputRate,cutoff=.47*std::min(1.,1/ratio);
    radius_=int(std::ceil(16*std::max(1.,ratio)));kernel_.resize(256);
    for(int phase=0;phase<256;++phase){
        auto& coefficients=kernel_[size_t(phase)];coefficients.resize(size_t(2*radius_+1));double sum=0;
        for(int k=-radius_;k<=radius_;++k){
            const double distance=k-double(phase)/256,x=2*cutoff*distance;
            const double sinc=std::abs(x)<1e-12?1:std::sin(std::numbers::pi*x)/(std::numbers::pi*x);
            const double window=.5+.5*std::cos(std::numbers::pi*distance/(radius_+1));
            const double value=2*cutoff*sinc*window;coefficients[size_t(k+radius_)]=value;sum+=value;
        }
        for(auto& value:coefficients)value/=sum;
    }
}
QByteArray PcmConverter::push(const QByteArray& bytes){
    if(finished_)throw std::runtime_error("Audio converter is already finished.");
    if(bytes.size()>8*1024*1024)throw std::runtime_error("Audio block is too large.");
    partial_+=bytes;const auto frames=partial_.size()/(inputChannels_*2);
    for(qsizetype frame=0;frame<frames;++frame){
        double value=0;for(int channel=0;channel<inputChannels_;++channel)value+=qFromLittleEndian<qint16>(partial_.constData()+(frame*inputChannels_+channel)*2)/32768.;
        samples_.push_back(float(value/inputChannels_));
    }
    partial_.remove(0,frames*inputChannels_*2);received_+=quint64(frames);return convert(false);
}
QByteArray PcmConverter::finish(){
    if(finished_)return {};
    if(!partial_.isEmpty())throw std::runtime_error("Audio ended inside a PCM frame.");
    finished_=true;return convert(true);
}
QByteArray PcmConverter::convert(bool final){
    QByteArray out;
    while(produced_*quint64(inputRate_)<received_*quint64(outputRate_)){
        const auto numerator=produced_*quint64(inputRate_),center=numerator/quint64(outputRate_);
        if(!final&&center+quint64(radius_)>=received_)break;
        double value=0;
        if(radius_==0)value=samples_.at(size_t(center-base_));
        else{
            const auto phase=size_t((numerator%quint64(outputRate_))*256/quint64(outputRate_));
            for(int k=-radius_;k<=radius_;++k){const auto index=qint64(center)+k;if(index>=qint64(base_)&&index<qint64(received_))value+=samples_[size_t(index-qint64(base_))]*kernel_[phase][size_t(k+radius_)];}
        }
        const auto sample=qint16(std::clamp(std::lround(value*32768),-32768L,32767L));
        for(int channel=0;channel<outputChannels_;++channel){const auto offset=out.size();out.resize(offset+2);qToLittleEndian<qint16>(sample,out.data()+offset);}
        ++produced_;
    }
    const auto next=produced_*quint64(inputRate_)/quint64(outputRate_);
    const auto keepFrom=next>quint64(radius_)?std::min(received_,next-quint64(radius_)):0;
    if(keepFrom>base_){samples_.erase(samples_.begin(),samples_.begin()+qsizetype(keepFrom-base_));base_=keepFrom;}
    return out;
}
}
