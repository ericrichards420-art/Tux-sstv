#pragma once
#include "codec.h"
#include <array>
#include <span>

namespace sstv {
struct MorseReading {
    QString text;
    QString candidate; // Unconfirmed CW text; never a checksum-validated FSKID.
    double toneHz=0,wpm=0;
};
// Streaming, single-tone CW at 12000 Hz. Own this on the receive worker.
class MorseReceiver {
public:
    void push(std::span<const float> samples,Cancel cancel={});
    void finish(); // End of a recording, after a complete final character gap.
    void suppress(); // Drop in-flight symbols during SSTV, preserving decoded words.
    MorseReading reading() const;
private:
    struct Run {bool keyed;double ms;};
    std::array<float,256> ring_{};
    size_t position_=0,total_=0;
    std::vector<Run> runs_;
    QString text_,candidate_;
    double pitch_=0,level_=0,unit_=60,lastPitch_=0,lastUnit_=0;
    int runFrames_=0,quietFrames_=0;
    bool keyed_=false,blocked_=false;
    void frame();
    double timing(double* error=nullptr) const;
    QString word(bool final) const;
    void commit();
};
}
