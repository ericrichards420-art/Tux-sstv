#pragma once
#include "wav.h"
#include <QByteArray>
#include <span>

namespace sstv {
struct AudioLevel {
    double peak=0,rms=0;
    size_t samples=0;
    bool clipped=false;
};
double audioDb(double amplitude);
AudioLevel measureAudio(std::span<const float> samples);
void setAudioOutputLevel(Audio& audio,int percent);
std::vector<AudioLevel> audioEnvelope(const Audio& audio,int milliseconds=100);
struct PcmGainBlock {
    QByteArray pcm;
    AudioLevel input,adjusted;
    int gainDb=0;
    bool gainReduced=false;
};
class PcmInputGain {
public:
    PcmGainBlock push(const QByteArray& bytes,int gainDb,bool protectClipping=false);
    void reset(){pending_.clear();}
private:
    QByteArray pending_;
};
}
