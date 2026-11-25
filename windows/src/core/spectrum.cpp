#include "spectrum.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace sstv {
void SpectrumAnalyzer::reset(){rate_=0;ring_.clear();window_.clear();work_.clear();position_=total_=next_=hops_=0;}
void SpectrumAnalyzer::configure(int rate){
    if(rate<8000||rate>96000)throw std::runtime_error("Audio monitor requires 8000–96000 Hz audio.");
    if(rate==rate_)return;
    reset();rate_=rate;
    size_t count=1;while(count<size_t(rate)/6)count*=2;
    ring_.assign(count,0);window_.resize(count);work_.resize(count);windowSum_=0;
    for(size_t i=0;i<count;++i){window_[i]=.5-.5*std::cos(2*std::numbers::pi*i/(count-1));windowSum_+=window_[i];}
    next_=count;
}
SpectrumFrame SpectrumAnalyzer::analyze(){
    SpectrumFrame result;result.db.fill(-90);result.endSeconds=double(total_)/rate_;
    const auto count=ring_.size();
    for(size_t i=0;i<count;++i){const double sample=ring_[(position_+i)%count];result.peak=std::max(result.peak,std::abs(sample));work_[i]=sample*window_[i];}
    for(size_t i=1,j=0;i<count;++i){size_t bit=count>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(work_[i],work_[j]);}
    for(size_t length=2;length<=count;length*=2){
        const auto step=std::polar(1.,-2*std::numbers::pi/length);
        for(size_t start=0;start<count;start+=length){std::complex<double> phase=1;for(size_t i=0;i<length/2;++i){const auto a=work_[start+i],b=work_[start+i+length/2]*phase;work_[start+i]=a+b;work_[start+i+length/2]=a-b;phase*=step;}}
    }
    const double spacing=(spectrumHighHz-spectrumLowHz)/(spectrumBins-1),resolution=double(rate_)/count;
    for(int bin=0;bin<spectrumBins;++bin){
        const double hz=spectrumLowHz+bin*spacing;
        const int first=std::max(1,int(std::ceil((hz-spacing/2)/resolution))),last=std::min(int(count/2)-1,int(std::floor((hz+spacing/2)/resolution)));
        double magnitude=0;for(int k=first;k<=last;++k)magnitude=std::max(magnitude,std::abs(work_[size_t(k)]));
        result.db[size_t(bin)]=float(std::clamp(20*std::log10(std::max(1e-9,2*magnitude/windowSum_)),-90.,0.));
    }
    return result;
}
void SpectrumAnalyzer::push(std::span<const float> samples,int rate,const std::function<void(const SpectrumFrame&)>& frame){
    configure(rate);
    for(float sample:samples){
        if(!std::isfinite(sample))throw std::runtime_error("Non-finite audio monitor sample.");
        ring_[position_]=sample;position_=(position_+1)%ring_.size();++total_;
        if(total_==next_){frame(analyze());next_=ring_.size()+(++hops_*size_t(rate_)+10)/20;}
    }
}
SpectrumFrame SpectrumAnalyzer::tail(std::span<const float> samples,int rate){
    SpectrumAnalyzer preview;preview.configure(rate);preview.total_=samples.size();const auto count=std::min(samples.size(),preview.ring_.size());
    const auto start=samples.size()-count;for(size_t i=0;i<count;++i){if(!std::isfinite(samples[start+i]))throw std::runtime_error("Non-finite audio monitor sample.");preview.ring_[preview.ring_.size()-count+i]=samples[start+i];}
    return preview.analyze();
}
}
