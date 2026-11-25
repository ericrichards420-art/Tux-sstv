#pragma once
#include "core/wav.h"
#include <QString>
#include <cmath>
#include <numbers>
#include <vector>
// Standalone FSKID waveform fixture built from protocol facts, not an app encoder.
inline sstv::Audio fskFixture(QString call="K9TEST",bool badChecksum=false,double offset=0,double ppm=0,bool narrow=false,bool idle=false) {
    std::vector<std::pair<double,double>> tones{{300,narrow?1900.:1500.},{100,2100},{22,1900}};
    auto symbol=[&](int value){for(int b=0;b<6;++b)tones.emplace_back(22,(value>>b)&1?1900:2100);};
    if(idle)symbol(0x20);symbol(0x2a);int checksum=0;
    for(const auto c:call){const int value=c.unicode()-32;checksum^=value;symbol(value);}
    symbol(1);symbol(checksum^(badChecksum?2:0));tones.emplace_back(50,1500);
    sstv::Audio audio;audio.sampleRate=12000;double phase=.25,total=0;const double scale=1+ppm*1e-6;
    for(const auto& [ms,hz]:tones){total+=ms*12*scale;const size_t end=size_t(std::llround(total));while(audio.samples.size()<end){phase+=2*std::numbers::pi*(hz+offset)/12000;audio.samples.push_back(float(.5*std::sin(phase)));}}
    return audio;
}
