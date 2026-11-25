#pragma once
#include <QByteArray>
#include <vector>

namespace audio {
// Streaming signed 16-bit little-endian PCM. Downmix, band-limit, resample,
// and duplicate mono to output channels. Chunk boundaries may split samples.
class PcmConverter {
public:
    PcmConverter(int inputRate,int inputChannels,int outputRate,int outputChannels=1);
    QByteArray push(const QByteArray& bytes);
    QByteArray finish();
private:
    int inputRate_,inputChannels_,outputRate_,outputChannels_,radius_=0;
    quint64 received_=0,base_=0,produced_=0;
    QByteArray partial_;
    std::vector<float> samples_;
    std::vector<std::vector<double>> kernel_;
    bool finished_=false;
    QByteArray convert(bool final);
};
}
