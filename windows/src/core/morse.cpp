#include "morse.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace sstv {
namespace {
const char* symbol(QChar c) {
    static constexpr const char* letters[]={".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--.."};
    static constexpr const char* digits[]={"-----",".----","..---","...--","....-",".....","-....","--...","---..","----."};
    if(c>=u'A' && c<=u'Z')return letters[c.unicode()-'A'];
    if(c>=u'0' && c<=u'9')return digits[c.unicode()-'0'];
    if(c==u'/')return "-..-.";
    if(c==u'-')return "-....-";
    throw std::runtime_error("Morse callsign must use A-Z, 0-9, / or -.");
}
QString validated(const MorseId& id) {
    if(id.wpm<5 || id.wpm>40 || id.toneHz<400 || id.toneHz>2500)
        throw std::runtime_error("Morse speed must be 5-40 WPM and tone 400-2500 Hz.");
    const auto call=id.callsign.trimmed().toUpper();
    if(call.isEmpty() || call.size()>20)throw std::runtime_error("Enter a station callsign of 1-20 characters for Morse ID.");
    for(const auto c:call)symbol(c);
    return call;
}
int units(const QString& call) {
    int result=0;
    for(int i=0;i<call.size();++i) {
        if(i)result+=3;
        const auto* code=symbol(call[i]);
        for(int j=0;code[j];++j){if(j)++result;result+=code[j]=='-'?3:1;}
    }
    return result;
}
void check(const Cancel& cancel) {if(cancel && cancel->load())throw std::runtime_error("Cancelled.");}
}
double morseIdDurationMs(const MorseId& id) {return 700.+units(validated(id))*1200./id.wpm;}
void appendMorseId(Audio& audio,const MorseId& id,Cancel cancel) {
    check(cancel);const auto call=validated(id);
    if(audio.sampleRate<8000 || audio.sampleRate>96000)throw std::runtime_error("Invalid Morse sample rate.");
    const double duration=700.+units(call)*1200./id.wpm;
    const auto count=size_t(std::llround(duration*audio.sampleRate/1000));
    if(audio.samples.size()+count>size_t(audio.sampleRate)*600)throw std::runtime_error("Audio with Morse ID exceeds ten minutes.");
    const auto initial=audio.samples.size();audio.samples.reserve(initial+count);
    long double time=0;double phase=0;
    const double step=2*std::numbers::pi*id.toneHz/audio.sampleRate;
    auto segment=[&](bool keyed,double ms) {
        time+=static_cast<long double>(ms)*audio.sampleRate/1000;
        const auto end=initial+size_t(std::llround(time));
        const auto begin=audio.samples.size(),length=end-begin;
        const auto ramp=std::min(size_t(std::llround(audio.sampleRate*.005)),length/2);
        for(size_t n=0;n<length;++n) {
            if((n&4095)==0)check(cancel);
            double envelope=0;
            if(keyed) {
                const auto edge=std::min(n,length-1-n);
                envelope=edge>=ramp || ramp==0?1:.5-.5*std::cos(std::numbers::pi*double(edge)/ramp);
            }
            audio.samples.push_back(float(.65*envelope*std::sin(phase)));
            phase+=step;if(phase>=2*std::numbers::pi)phase-=2*std::numbers::pi;
        }
    };
    segment(false,500);
    const double dot=1200./id.wpm;
    for(int i=0;i<call.size();++i) {
        if(i)segment(false,3*dot);
        const auto* code=symbol(call[i]);
        for(int j=0;code[j];++j){if(j)segment(false,dot);segment(true,(code[j]=='-'?3:1)*dot);}
    }
    segment(false,200);check(cancel);
}
}
