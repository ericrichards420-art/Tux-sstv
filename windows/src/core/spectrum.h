#pragma once
#include <array>
#include <complex>
#include <functional>
#include <span>
#include <vector>

namespace sstv {
inline constexpr int spectrumBins=256;
inline constexpr double spectrumLowHz=500, spectrumHighHz=3000;
struct SpectrumFrame {
    std::array<float,spectrumBins> db{};
    double peak=0,endSeconds=0;
};
// A Hann-windowed, single-sided amplitude spectrum. Frames advance in 50 ms
// of input audio, irrespective of capture chunk sizes or GUI repaint timing.
class SpectrumAnalyzer {
public:
    void reset();
    void push(std::span<const float> samples,int rate,const std::function<void(const SpectrumFrame&)>& frame);
    static SpectrumFrame tail(std::span<const float> samples,int rate);
private:
    int rate_=0;
    size_t position_=0,total_=0,next_=0,hops_=0;
    std::vector<float> ring_;
    std::vector<double> window_;
    std::vector<std::complex<double>> work_;
    double windowSum_=0;
    void configure(int rate);
    SpectrumFrame analyze();
};
}
