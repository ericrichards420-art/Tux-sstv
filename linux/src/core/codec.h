#pragma once
#include "modes.h"
#include "wav.h"
#include <QImage>
#include <atomic>
#include <functional>
#include <memory>

namespace sstv {
using Cancel = std::shared_ptr<std::atomic_bool>;
using Progress = std::function<void(int)>;
using ModeDetected = std::function<void(const QString&)>;
struct DecodeOptions {
    QString forceMode; // Empty: require valid, supported VIS. Forced mode still requires VIS unless manualStartMs >= 0.
    double manualStartMs = -1;
    double clockPpm = 0; // Positive means more recorded samples per nominal second.
    double frequencyOffset = 0; // Positive means received tones are higher.
    bool autoClock = false; // Estimate line timing from a consistent series of sync pulses.
    bool autoFrequency = false; // Estimate the absolute tone offset from a validated VIS leader.
    bool trackSync = true;
    bool allowPartial = false;
};
struct DecodeResult {
    QImage image;
    QString modeId;
    QString callsign; // Checksum-validated received FSKID; empty until available.
    bool metadataOnly=false;
    quint64 frameId=0; // Live-session identity, shared by every preview of a frame.
    double startMs=0;
    double endMs=0; // Actual end boundary, including applied clock correction.
    double clockPpm=0, frequencyOffset=0;
    bool clockMeasured=false, frequencyMeasured=false;
    int rows=0;
    bool complete=false;
    QString message;
};
struct VisHeader {
    int code;
    double imageStartMs; // After VIS/N-VIS; AVT subsequently resolves its countdown.
    bool narrow=false;
};
QImage fitImage(const QImage& image,const Mode& mode);
QImage testPattern();
Audio encode(const QImage& image,const Mode& mode,int sampleRate=48000,Cancel cancel={},Progress progress={});
std::vector<float> demodulate(const Audio& audio,Cancel cancel={});
VisHeader detectVis(const std::vector<float>& frequencies,double offset=0,Cancel cancel={},bool supportedOnly=false,bool requireFullPreamble=false);
// Reports the selected mode before frame assembly (and before the AVT countdown).
DecodeResult decode(const Audio& audio,const DecodeOptions& options={},Cancel cancel={},Progress progress={},ModeDetected detected={});
}
