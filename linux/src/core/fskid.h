#pragma once
#include "codec.h"
#include <optional>
namespace sstv {
struct FskId { QString callsign; double endMs=0; };
bool validCallsign(const QString& call);
// Frequencies at 12000 Hz. Only a leader, marker, terminator and valid XOR
// checksum can produce an ID. Incomplete/corrupted data never produces a call.
std::optional<FskId> detectFskId(const std::vector<float>& frequencies,double beginMs=0,double offset=0,double clockPpm=0,Cancel cancel={});
}
