#include "playback_mixer.h"

namespace audio {
QString mixerCard(const QString& pcm){return pcm.startsWith("winmm:")?pcm:QString{};}
MixerSetup optimizePlayback(const QString& pcm){
    MixerSetup result;result.card=mixerCard(pcm);
    result.error="Set the selected USB output level and unmute it in Windows Settings → System → Sound. Then use the Tux-sstv TX level and Play test tone to check SignaLink keying. Windows hardware mixer adjustment is not automated in this preview.";
    return result;
}
QString restorePlayback(const MixerSetup&){return {};}
CaptureAdjustment reduceCapture(const QString& pcm){return {pcm,"Lower the selected input level in Windows Sound settings, or lower the radio/interface RX control. Software gain cannot repair input already clipped by the device.",false};}
}
