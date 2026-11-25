#pragma once
#include "codec.h"

namespace sstv {
struct MorseId {
    QString callsign;
    int wpm=20;
    int toneHz=800;
};
// Includes a 500 ms leading gap and 200 ms trailing silence.
double morseIdDurationMs(const MorseId& id);
void appendMorseId(Audio& audio,const MorseId& id,Cancel cancel={});
}
