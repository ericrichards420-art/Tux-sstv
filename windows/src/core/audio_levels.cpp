#include "audio_levels.h"
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sstv {
double audioDb(double amplitude){return 20*std::log10(std::max(1e-6,std::abs(amplitude)));}
AudioLevel measureAudio(std::span<const float> samples){
    AudioLevel level;level.samples=samples.size();double energy=0;
    for(float sample:samples){if(!std::isfinite(sample))throw std::runtime_error("Non-finite audio sample.");level.peak=std::max(level.peak,std::abs(double(sample)));energy+=double(sample)*sample;if(std::abs(sample)>=.999f)level.clipped=true;}
    if(level.samples)level.rms=std::sqrt(energy/level.samples);
    return level;
}
void setAudioOutputLevel(Audio& audio,int percent){
    if(percent<0||percent>100)throw std::runtime_error("TX audio level must be between 0 and 100 percent.");
    if(percent==100)return;
    const float gain=percent/100.f;for(auto& sample:audio.samples)sample*=gain;
}
std::vector<AudioLevel> audioEnvelope(const Audio& audio,int milliseconds){
    if(audio.sampleRate<=0||milliseconds<=0)throw std::runtime_error("Invalid audio meter interval.");
    const size_t count=std::max(size_t(1),size_t(audio.sampleRate)*milliseconds/1000);std::vector<AudioLevel> envelope;envelope.reserve((audio.samples.size()+count-1)/count);
    for(size_t start=0;start<audio.samples.size();start+=count)envelope.push_back(measureAudio(std::span<const float>(audio.samples).subspan(start,std::min(count,audio.samples.size()-start))));
    return envelope;
}
PcmGainBlock PcmInputGain::push(const QByteArray& bytes,int gainDb,bool protectClipping){
    if(gainDb<-24||gainDb>24)throw std::runtime_error("RX audio gain must be between -24 and +24 dB.");
    const auto input=pending_+bytes;const auto length=input.size()/2*2;pending_=input.mid(length);PcmGainBlock result;result.pcm=input.first(length);result.input.samples=result.adjusted.samples=size_t(length/2);
    if(protectClipping){
        double peak=0;for(qsizetype i=0;i<length;i+=2)peak=std::max(peak,std::abs(qFromLittleEndian<qint16>(input.constData()+i)/32768.));
        // Inspect the original PCM before applying gain so this very block is
        // protected. Only reduce gain; silence never causes noise amplification.
        if(peak>0&&audioDb(peak)+gainDb> -1.){
            const int safe=std::clamp(int(std::floor(-6.-audioDb(peak))),-24,24);
            result.gainReduced=safe<gainDb;gainDb=std::min(gainDb,safe);
        }
    }
    result.gainDb=gainDb;const double gain=std::pow(10.,gainDb/20.);double beforeEnergy=0,afterEnergy=0;
    for(qsizetype i=0;i<length;i+=2){
        const qint16 raw=qFromLittleEndian<qint16>(input.constData()+i);const double before=raw/32768.;const double scaled=before*gain;const auto value=std::clamp(std::lround(std::clamp(scaled,-1.,32767./32768.)*32768),-32768l,32767l);const double after=value/32768.;
        if(gainDb)qToLittleEndian<qint16>(qint16(value),result.pcm.data()+i);
        result.input.peak=std::max(result.input.peak,std::abs(before));result.adjusted.peak=std::max(result.adjusted.peak,std::abs(after));beforeEnergy+=before*before;afterEnergy+=after*after;
        if(raw>=32760||raw<=-32760)result.input.clipped=true;
        if(scaled>32767./32768.||scaled< -1.)result.adjusted.clipped=true;
    }
    if(result.input.samples){result.input.rms=std::sqrt(beforeEnergy/result.input.samples);result.adjusted.rms=std::sqrt(afterEnergy/result.adjusted.samples);}result.adjusted.clipped=result.adjusted.clipped||result.input.clipped;return result;
}
}
