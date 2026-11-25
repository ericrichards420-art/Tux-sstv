#pragma once
#include <QString>
#include <vector>

namespace sstv {
enum class Family { Robot, Scottie, Martin, Wraase, PD, Pasokon, Mono, AVT, MP, MR, ML, Narrow };
enum class Component { Tone, Hold, Red, Green, Blue, Luma, Y, Cr, Cb };
struct Segment {
    double ms;
    Component component = Component::Tone;
    double hz = 1500;
    int rowOffset = 0;
    bool averagePair = false;
};
struct Mode {
    QString id, name;
    Family family;
    int width, height, vis;
    double scanMs;
    bool implemented;
    QString note;
    int rowsPerLine() const;
    double imageMs() const;
    double initialMs() const;
    double syncOffsetMs() const;
    double syncMs() const;
    double headerMs() const;
    bool narrow() const { return family == Family::Narrow; }
    std::vector<Segment> line(int row) const;
};
const std::vector<Mode>& modes();
const Mode* modeById(const QString& id);
const Mode* modeByVis(int vis,bool narrow=false);
QString familyName(Family family);
}
