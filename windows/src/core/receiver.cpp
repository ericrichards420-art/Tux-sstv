#include "receiver.h"
#include "fskid.h"
#include <algorithm>
#include <optional>
#include <cmath>
#include <stdexcept>

namespace sstv {
namespace {
struct HeaderBoundary {size_t start=0,keep=0;};
std::optional<HeaderBoundary> nextHeader(const Audio& audio,size_t begin,size_t end,const DecodeOptions& options,const Cancel& cancel){
    if(end<=begin+3600)return {};
    Audio window;window.samples.assign(audio.samples.begin()+begin,audio.samples.begin()+end);
    std::optional<HeaderBoundary> found;
    const auto remember=[&](double start,size_t preroll=600){
        if(start<0)return;
        const size_t sample=begin+size_t(std::floor(start));
        if(!found||sample<found->start)found=HeaderBoundary{sample,sample>preroll?sample-preroll:0};
    };
    {
        const auto frequencies=demodulate(window,cancel);
        for(int trial=-1;trial<(options.autoFrequency?17:0);++trial){
            const double offset=trial<0?options.frequencyOffset:-200+trial*25.;
            try{
                const auto header=detectVis(frequencies,offset,cancel,true,true);
                const auto* mode=modeByVis(header.code,header.narrow);
                if(mode&&(options.forceMode.isEmpty()||options.forceMode==mode->id))
                    remember((header.imageStartMs-(header.narrow?950:header.code>127?1150:910))*12);
            }catch(const std::runtime_error&){if(cancel&&cancel->load())throw;}
        }
    }
    return found;
}
}
Receiver::Receiver(DecodeOptions options):options_(std::move(options)) {
    if(!std::isfinite(options.clockPpm) || std::abs(options.clockPpm)>10000 || !std::isfinite(options.frequencyOffset) || std::abs(options.frequencyOffset)>200)
        throw std::runtime_error("Invalid receive correction settings.");
    if(!std::isfinite(options_.manualStartMs) || options_.manualStartMs < -1 || (!options_.forceMode.isEmpty() && !modeById(options_.forceMode)))
        throw std::runtime_error("Invalid receive mode or image start.");
    if(options_.forceMode.isEmpty())options_.manualStartMs=-1;
    options_.allowPartial=true;
    pending_.sampleRate=12000;
}
void Receiver::discard(size_t samples) {
    samples=std::min(samples,pending_.samples.size());
    discardedSamples_+=samples;
    pending_.samples.erase(pending_.samples.begin(),pending_.samples.begin()+samples);
}
void Receiver::resetFrame(){
    modeId_.clear();pictureStartMs_=-1;frameOffset_=0;frameTuned_=false;
    controlDeadline_=0;headerScannedSamples_=0;++frameId_;
    options_.manualStartMs=-1; // A manual start applies only to the first image.
}
std::vector<DecodeResult> Receiver::push(const Audio& block,Cancel cancel,Progress progress,ModeDetected detected) {
    if(cancel && cancel->load())throw std::runtime_error("Cancelled.");
    if(block.sampleRate!=12000)throw std::runtime_error("Live receive requires 12000 Hz mono audio.");
    if(pending_.samples.size()+block.samples.size()>12000*600)throw std::runtime_error("Receive buffer exceeds ten minutes.");
    for(const auto sample:block.samples)if(!std::isfinite(sample))throw std::runtime_error("Non-finite audio sample.");
    const auto blockStart=discardedSamples_+pending_.samples.size();
    std::vector<std::pair<size_t,size_t>> pictureAudio;
    pending_.samples.insert(pending_.samples.end(),block.samples.begin(),block.samples.end());
    std::vector<DecodeResult> frames;
    while(!pending_.samples.empty()) {
        if(cancel && cancel->load())throw std::runtime_error("Cancelled.");
        if(trailerFrame_) {
            const auto id=detectFskId(demodulate(pending_,cancel),0,trailerOffset_,trailerClock_,cancel);
            if(id){DecodeResult update;update.frameId=trailerFrame_;update.modeId=trailerMode_;update.metadataOnly=true;update.callsign=id->callsign;update.message="FSKID · "+id->callsign;frames.push_back(std::move(update));trailerFrame_=0;}
            else if(pending_.samples.size()>5*12000)trailerFrame_=0;
        }
        auto options=options_;
        // Once picture timing is known, retain it across previews rather than
        // repeatedly searching a potentially damaged header.
        if(pictureStartMs_>=0){options.forceMode=modeId_;options.manualStartMs=pictureStartMs_;}
        if(frameTuned_){options.frequencyOffset=frameOffset_;options.autoFrequency=false;}
        DecodeResult frame;
        try {
            frame=decode(pending_,options,cancel,progress,[&](const QString& id) {
                if(modeId_.isEmpty()) {
                    trailerFrame_=0;modeId_=id;
                    controlDeadline_=pending_.samples.size()+18*12000;
                    if(detected)detected(id);
                }
            });
        } catch(const std::runtime_error&) {
            if(cancel && cancel->load())throw;
            if(!modeId_.isEmpty())pictureAudio.emplace_back(discardedSamples_,discardedSamples_+pending_.samples.size());
            if(modeId_.isEmpty()) {
                // Keep enough overlap for every header type, including a header
                // split across audio blocks. Long silence is scanned only once.
                const size_t overlap=12000*(trailerFrame_?5:2);
                if(pending_.samples.size()>overlap)discard(pending_.samples.size()-overlap);
            } else if(pictureStartMs_<0 && modeById(modeId_)->family==Family::AVT && pending_.samples.size()>controlDeadline_) {
                // An incomplete/invalid AVT countdown must not hold the receiver
                // forever. Resume searching after this header.
                const auto frequencies=demodulate(pending_,cancel);
                std::optional<VisHeader> header;
                for(int trial=-1;trial<(options_.autoFrequency?17:0);++trial) {
                    const double offset=trial<0?options_.frequencyOffset:-200+trial*25.;
                    try {const auto found=detectVis(frequencies,offset,cancel,true);if(!header || found.imageStartMs<header->imageStartMs)header=found;}
                    catch(const std::runtime_error&){if(cancel && cancel->load())throw;}
                }
                if(header)discard(size_t(std::ceil(header->imageStartMs*12)));
                else if(pending_.samples.size()>24000)discard(pending_.samples.size()-24000);
                modeId_.clear();frameTuned_=false;
                continue;
            }
            break; // No full header yet, or waiting for AVT control words.
        }
        // An interrupted transmission can look like an almost finished image.
        // Find a fresh validated header before letting its tones fill old rows.
        const size_t pictureBegin=size_t(std::ceil(frame.startMs*12))+1;
        const size_t scanBegin=std::max(pictureBegin,headerScannedSamples_>24000?headerScannedSamples_-24000:0);
        const size_t scanEnd=std::min(pending_.samples.size(),size_t(std::ceil(frame.endMs*12))+24000);
        const auto next=nextHeader(pending_,scanBegin,scanEnd,options_,cancel);
        headerScannedSamples_=scanEnd;
        if(next&&next->start+24<size_t(std::ceil(frame.endMs*12))){
            pictureAudio.emplace_back(discardedSamples_,discardedSamples_+next->start);
            Audio interrupted;interrupted.samples.assign(pending_.samples.begin(),pending_.samples.begin()+next->start);
            try{
                auto partial=decode(interrupted,options,cancel,progress);
                partial.frameId=frameId_;partial.message+=" · interrupted by next transmission";
                frames.push_back(std::move(partial));
            }catch(const std::runtime_error&){if(cancel&&cancel->load())throw;}
            discard(next->keep);trailerFrame_=0;resetFrame();continue;
        }
        pictureStartMs_=frame.startMs;
        if(frame.frequencyMeasured){frameOffset_=frame.frequencyOffset;frameTuned_=true;}
        else if(frameTuned_){frame.frequencyMeasured=true;frame.message+=QString(" · AFC %1 Hz").arg(frameOffset_,0,'f',1);}
        const auto end=size_t(std::ceil(frame.endMs*12));
        pictureAudio.emplace_back(discardedSamples_,discardedSamples_+std::min(end,pending_.samples.size()));
        const bool complete=frame.complete;
        frame.frameId=frameId_;
        if(complete && frame.callsign.isEmpty()){trailerFrame_=frameId_;trailerMode_=frame.modeId;trailerOffset_=frame.frequencyOffset;trailerClock_=frame.clockPpm;}
        frames.push_back(std::move(frame));
        if(!complete)break;
        // Consume only the picture, leaving its tail and the next header intact.
        discard(end);resetFrame();
    }
    // CW uses only audio outside known SSTV pictures. Absolute capture
    // positions let a block contain the last image row and the first CW mark
    // without dropping that mark or feeding picture tones to the CW reader.
    size_t position=0;
    for(const auto& [first,last]:pictureAudio){
        const auto begin=std::min(block.samples.size(),first>blockStart?first-blockStart:0);
        const auto end=std::min(block.samples.size(),last>blockStart?last-blockStart:0);
        if(end<=position)continue;
        if(begin>position)morse_.push(std::span(block.samples).subspan(position,begin-position),cancel);
        morse_.suppress();position=end;
    }
    if(position<block.samples.size())morse_.push(std::span(block.samples).subspan(position),cancel);
    return frames;
}
}
