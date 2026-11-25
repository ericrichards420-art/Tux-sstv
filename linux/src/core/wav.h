#pragma once
#include <QString>
#include <vector>
namespace sstv {
struct Audio {
    int sampleRate = 12000;
    std::vector<float> samples;
    double seconds() const { return double(samples.size()) / sampleRate; }
};
// channel: 0=left/mono, 1=right (requires stereo), 2=average.
Audio readWav(const QString& path,int channel=0);
void writeWav(const QString& path,const Audio& audio);
}
