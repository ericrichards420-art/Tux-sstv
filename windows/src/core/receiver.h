#pragma once
#include "codec.h"
#include "morse_receiver.h"

namespace sstv {
// One receive session, with automatic or explicit mode selection. Feed 12000 Hz mono audio blocks
// from a single worker. Completed frames are returned once; partial frames may
// be returned again as more rows arrive. A new header can select a new mode.
class Receiver {
public:
    explicit Receiver(DecodeOptions options={});
    std::vector<DecodeResult> push(const Audio& block,Cancel cancel={},Progress progress={},ModeDetected detected={});
    QString modeId() const { return modeId_; }
    MorseReading morse() const {return morse_.reading();}
private:
    DecodeOptions options_;
    Audio pending_;
    QString modeId_;
    double pictureStartMs_=-1;
    double frameOffset_=0;
    bool frameTuned_=false;
    size_t controlDeadline_=0;
    size_t headerScannedSamples_=0;
    quint64 frameId_=1,trailerFrame_=0;
    double trailerOffset_=0,trailerClock_=0;
    QString trailerMode_;
    MorseReceiver morse_;
    size_t discardedSamples_=0;
    void discard(size_t samples);
    void resetFrame();
};
}
