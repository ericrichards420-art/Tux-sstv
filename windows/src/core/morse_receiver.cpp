#include "morse_receiver.h"
#include "fskid.h"
#include <algorithm>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace sstv {
namespace {
QChar letter(const QString& code){
    static const char* codes[]={".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--..","-----",".----","..---","...--","....-",".....","-....","--...","---..","----.","-..-.","-....-",".-.-.-","--..--","..--..","-...-",".-.-.",".--.-."};
    static const QString alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/-.,?=+@";
    for(qsizetype i=0;i<alphabet.size();++i)if(code==QLatin1String(codes[i]))return alphabet[i];
    return QChar(0xfffd);
}
}
void MorseReceiver::suppress(){
    ring_.fill(0);position_=total_=0;runs_.clear();pitch_=level_=0;runFrames_=quietFrames_=0;keyed_=blocked_=false;
}
double MorseReceiver::timing(double* error) const{
    double best=1e9,chosen=unit_;
    for(const auto& source:runs_)for(int divisor:{1,3}){
        const double unit=source.ms/divisor;if(unit<27||unit>255)continue;
        double cost=0;
        for(const auto& run:runs_){const double ratio=run.ms/unit;double fit=std::min(std::pow(ratio-1,2),std::pow(ratio-3,2));if(!run.keyed)fit=std::min(fit,std::pow(ratio-7,2));cost+=std::min(9.,fit);}
        cost/=std::max(size_t(1),runs_.size());
        if(cost<best-1e-6||(std::abs(cost-best)<1e-6&&unit>chosen)){best=cost;chosen=unit;}
    }
    if(error)*error=best;
    return chosen;
}
QString MorseReceiver::word(bool final) const{
    if(runs_.empty())return {};
    const double unit=timing();QString result,code;
    for(const auto& run:runs_){
        if(run.keyed)code+=run.ms>2*unit?'-':'.';
        else if(run.ms>2*unit){result+=letter(code);code.clear();if(run.ms>=5.5*unit)result+=' ';}
    }
    if(!code.isEmpty()&&(final||(!keyed_&&runFrames_*5>=2*unit)))result+=letter(code);
    return result.trimmed();
}
void MorseReceiver::commit(){
    double error=0;const double unit=timing(&error);const auto decoded=word(true);
    if(error<.30&&!decoded.isEmpty()&&!decoded.contains(QChar(0xfffd))){
        if(!text_.isEmpty())text_+=' ';
        text_+=decoded;text_=text_.right(160);
        const auto lastWord=decoded.section(' ',-1);
        candidate_=validCallsign(lastWord)?lastWord:QString{};lastPitch_=pitch_;lastUnit_=unit;
    }
    runs_.clear();unit_=unit;
}
MorseReading MorseReceiver::reading() const{
    auto text=text_;const auto partial=word(false);
    if(!partial.isEmpty())text+=(text.isEmpty()?QString{}:QString(" "))+partial;
    return {text.right(160),runs_.empty()?candidate_:QString{},lastPitch_,lastUnit_>0?1200/lastUnit_:0};
}
void MorseReceiver::finish(){if(!keyed_&&runFrames_*5>=2*timing())commit();}
void MorseReceiver::push(std::span<const float> samples,Cancel cancel){
    for(const float sample:samples){
        if(!std::isfinite(sample))throw std::runtime_error("Non-finite Morse audio sample.");
        if((total_&4095)==0&&cancel&&cancel->load())throw std::runtime_error("Cancelled.");
        ring_[position_]=sample;position_=(position_+1)%ring_.size();++total_;
        if(total_>=ring_.size()&&(total_-ring_.size())%60==0)frame();
    }
}
void MorseReceiver::frame(){
    // 21 ms Hann window, 5 ms hop. A narrow, coherent peak rejects broadband
    // noise; a competing tone abandons the word instead of reading FSK as CW.
    std::array<std::complex<double>,512> spectrum{};double windowSum=0;
    for(size_t n=0;n<ring_.size();++n){const double w=.5-.5*std::cos(2*std::numbers::pi*n/(ring_.size()-1));spectrum[n]=ring_[(position_+n)%ring_.size()]*w;windowSum+=w;}
    for(size_t i=1,j=0;i<spectrum.size();++i){size_t bit=spectrum.size()/2;for(;j&bit;bit/=2)j^=bit;j^=bit;if(i<j)std::swap(spectrum[i],spectrum[j]);}
    for(size_t length=2;length<=spectrum.size();length*=2){
        const auto step=std::polar(1.,-2*std::numbers::pi/length);
        for(size_t start=0;start<spectrum.size();start+=length){std::complex<double> phase=1;for(size_t n=0;n<length/2;++n){const auto a=spectrum[start+n],b=spectrum[start+n+length/2]*phase;spectrum[start+n]=a+b;spectrum[start+n+length/2]=a-b;phase*=step;}}
    }
    int peak=17;double power=0,band=0;
    for(int k=1;k<256;++k){power+=std::norm(spectrum[k]);if(k>=17&&k<=107&&std::norm(spectrum[k])>std::norm(spectrum[peak]))peak=k;}
    for(int k=peak-3;k<=peak+3;++k)band+=std::norm(spectrum[k]);
    const double left=std::log(std::max(1e-20,std::norm(spectrum[peak-1]))),center=std::log(std::max(1e-20,std::norm(spectrum[peak]))),right=std::log(std::max(1e-20,std::norm(spectrum[peak+1])));
    const double bend=left-2*center+right;
    const double hz=(peak+(std::abs(bend)>1e-9?std::clamp(.5*(left-right)/bend,-.5,.5):0))*12000/512;
    const double amplitude=2*std::abs(spectrum[peak])/windowSum;
    const bool pure=power>0&&band/power>.72&&amplitude>.0002;
    if(blocked_){if(!pure){if(++quietFrames_>=16){blocked_=false;quietFrames_=0;}}else quietFrames_=0;return;}
    if(pure&&pitch_>0&&std::abs(hz-pitch_)>60&&amplitude>level_*.25){
        const bool separated=!keyed_&&runFrames_*5>=80;
        suppress();
        if(!separated){blocked_=true;return;}
    }
    if(pure&&pitch_==0){pitch_=hz;level_=amplitude;}
    const bool on=pure&&std::abs(hz-pitch_)<45&&amplitude>=std::max(.0002,level_*.18);
    if(on)level_=std::max(amplitude,level_*.995);
    if(on==keyed_)++runFrames_;
    else{
        if(on&&runs_.empty())candidate_.clear();
        const double duration=runFrames_*5.;
        if(keyed_){
            if(duration<15||duration>850){runs_.clear();candidate_.clear();}
            else runs_.push_back({true,duration});
        }else if(!runs_.empty())runs_.push_back({false,duration});
        keyed_=on;runFrames_=1;
        if(runs_.size()>180){runs_.clear();candidate_.clear();}
        if(!runs_.empty())unit_=timing();
    }
    if(!keyed_){
        if(!runs_.empty()&&runFrames_*5>=5.5*unit_)commit();
        if(runFrames_>=400){pitch_=level_=0;runFrames_=400;}
    }
}
}
