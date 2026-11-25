#include "modes.h"
#include <numeric>

namespace sstv {
// Protocol facts: N7CXI, Dayton SSTV Forum, 20 May 2000 (see docs/SOURCES.md).
// The complete compatibility target is JE3HHT's SSTVModeList in MMSSTV 1.13.
const std::vector<Mode>& modes() {
    static const std::vector<Mode> table = {
        {"robot36", "Robot 36", Family::Robot, 320,240,8,88,true,{}},
        {"robot72", "Robot 72", Family::Robot, 320,240,12,138,true,{}},
        {"avt90", "AVT 90", Family::AVT,320,240,68,125,true,"Normal RGB profile; RX accepts the published and MMSSTV legacy countdown markers. External interoperability pending."},
        {"scottie1", "Scottie 1", Family::Scottie,320,256,60,138.240,true,{}},
        {"scottie2", "Scottie 2", Family::Scottie,320,256,56,88.064,true,{}},
        {"scottiedx", "Scottie DX", Family::Scottie,320,256,76,345.600,true,{}},
        {"martin1", "Martin 1", Family::Martin,320,256,44,146.432,true,{}},
        {"martin2", "Martin 2", Family::Martin,320,256,40,73.216,true,{}},
        {"sc2180", "SC2 180", Family::Wraase,320,256,55,235,true,{}},
        {"sc2120", "SC2 120", Family::Wraase,320,256,63,156.5,true,{}},
        {"sc260", "SC2 60", Family::Wraase,320,256,59,78.128,true,{}},
        {"pd50", "PD50", Family::PD,320,256,93,91.520,true,{}},
        {"pd90", "PD90", Family::PD,320,256,99,170.240,true,{}},
        {"pd120", "PD120", Family::PD,640,496,95,121.600,true,{}},
        {"pd160", "PD160", Family::PD,512,400,98,195.584,true,{}},
        {"pd180", "PD180", Family::PD,640,496,96,183.040,true,{}},
        {"pd240", "PD240", Family::PD,640,496,97,244.480,true,{}},
        {"pd290", "PD290", Family::PD,800,616,94,228.800,true,{}},
        {"p3", "P3", Family::Pasokon,640,496,113,640.0/4.8,true,{}},
        {"p5", "P5", Family::Pasokon,640,496,114,200,true,{}},
        {"p7", "P7", Family::Pasokon,640,496,115,640.0/2.4,true,{}},
        {"mr73", "MR73", Family::MR,320,256,0x4523,138,true,{}},
        {"mr90", "MR90", Family::MR,320,256,0x4623,171,true,{}},
        {"mr115", "MR115", Family::MR,320,256,0x4923,220,true,{}},
        {"mr140", "MR140", Family::MR,320,256,0x4a23,269,true,{}},
        {"mr175", "MR175", Family::MR,320,256,0x4c23,337,true,{}},
        {"mp73", "MP73", Family::MP,320,256,0x2523,140,true,{}},
        {"mp115", "MP115", Family::MP,320,256,0x2923,223,true,{}},
        {"mp140", "MP140", Family::MP,320,256,0x2a23,270,true,{}},
        {"mp175", "MP175", Family::MP,320,256,0x2c23,340,true,{}},
        {"ml180", "ML180", Family::ML,640,496,0x8523,176.5,true,{}},
        {"ml240", "ML240", Family::ML,640,496,0x8623,236.5,true,{}},
        {"ml280", "ML280", Family::ML,640,496,0x8923,277.5,true,{}},
        {"ml320", "ML320", Family::ML,640,496,0x8a23,317.5,true,{}},
        {"robot24", "Robot 24", Family::Robot,160,120,4,88,true,{}},
        {"bw8", "B/W 8", Family::Mono,160,120,2,59.9,true,"7 ms sync / 59.9 ms full-range luminance profile. External interoperability pending."},
        {"bw12", "B/W 12", Family::Mono,160,120,6,93,true,"7 ms sync / 93 ms full-range luminance profile. External interoperability pending."},
        {"mp73n", "MP73-N", Family::Narrow,320,256,2,140,true,{}},
        {"mp110n", "MP110-N", Family::Narrow,320,256,4,212,true,{}},
        {"mp140n", "MP140-N", Family::Narrow,320,256,5,270,true,{}},
        {"mc110n", "MC110-N", Family::Narrow,320,256,20,143,true,{}},
        {"mc140n", "MC140-N", Family::Narrow,320,256,21,180,true,{}},
        {"mc180n", "MC180-N", Family::Narrow,320,256,22,232,true,{}}
    };
    return table;
}
const Mode* modeById(const QString& id) {
    for (const auto& m : modes()) if (m.id == id || m.name.compare(id, Qt::CaseInsensitive) == 0) return &m;
    return nullptr;
}
const Mode* modeByVis(int vis,bool narrow) {
    for (const auto& m : modes()) if (vis >= 0 && m.vis == vis && m.narrow()==narrow) return &m;
    return nullptr;
}
QString familyName(Family f) {
    switch (f) {
    case Family::Robot: return "Robot"; case Family::Scottie: return "Scottie";
    case Family::Martin: return "Martin"; case Family::Wraase: return "Wraase";
    case Family::PD: return "PD"; case Family::Pasokon: return "Pasokon";
    case Family::Mono: return "Monochrome"; case Family::AVT: return "AVT";
    case Family::MP: return "MP"; case Family::MR: return "MR"; case Family::ML: return "ML";
    case Family::Narrow: return "Narrowband";
    }
    return {};
}
int Mode::rowsPerLine() const { return family == Family::PD || family==Family::MP || (narrow() && id.startsWith("mp")) ? 2 : 1; }
double Mode::headerMs() const {
    // AVT follows standard VIS with 32 words, each one start + 16 data bits.
    return family==Family::AVT ? 910+32*17*(1000./102.4) : narrow() ? 950 : vis>127 ? 1150 : 910;
}
double Mode::initialMs() const { return family == Family::Scottie ? 9 : 0; }
double Mode::syncOffsetMs() const { return family == Family::Scottie ? 3 + 2*scanMs : 0; }
double Mode::syncMs() const {
    switch (family) {
    case Family::Martin: return 4.862;
    case Family::Scottie: case Family::Robot: return 9;
    case Family::MP: case Family::MR: case Family::ML: return 9;
    case Family::Narrow: return id.startsWith("mc")?8:9;
    case Family::Wraase: return id=="sc260" ? 5.5006 : id=="sc2120" ? 5.52248 : 5.5225;
    case Family::Mono: return 7;
    case Family::PD: return 20;
    case Family::Pasokon: return 25 * scanMs / 640;
    default: return 0;
    }
}
std::vector<Segment> Mode::line(int row) const {
    using C = Component;
    auto tone = [](double ms, double hz = 1500) { return Segment{ms,C::Tone,hz}; };
    auto scan = [](double ms,C c,int offset=0,bool pair=false) { return Segment{ms,c,0,offset,pair}; };
    if (!implemented) return {};
    switch (family) {
    case Family::Martin:
        return {tone(4.862,1200),tone(.572),scan(scanMs,C::Green),tone(.572),scan(scanMs,C::Blue),tone(.572),scan(scanMs,C::Red),tone(.572)};
    case Family::Scottie:
        return {tone(1.5),scan(scanMs,C::Green),tone(1.5),scan(scanMs,C::Blue),tone(9,1200),tone(1.5),scan(scanMs,C::Red)};
    case Family::Wraase:
        return {tone(syncMs(),1200),tone(.5),scan(scanMs,C::Red),scan(scanMs,C::Green),scan(scanMs,C::Blue)};
    case Family::Mono:
        return {tone(7,1200),scan(scanMs,C::Luma)};
    case Family::AVT:
        return {scan(scanMs,C::Red),scan(scanMs,C::Green),scan(scanMs,C::Blue)};
    case Family::Pasokon: {
        const double gap = 5*scanMs/640;
        return {tone(syncMs(),1200),tone(gap),scan(scanMs,C::Red),tone(gap),scan(scanMs,C::Green),tone(gap),scan(scanMs,C::Blue),tone(gap)};
    }
    case Family::PD:
        return {tone(20,1200),tone(2.08),scan(scanMs,C::Y),scan(scanMs,C::Cr,0,true),scan(scanMs,C::Cb,0,true),scan(scanMs,C::Y,1)};
    case Family::MP:
        return {tone(9,1200),tone(1),scan(scanMs,C::Y),scan(scanMs,C::Cr,0,true),scan(scanMs,C::Cb,0,true),scan(scanMs,C::Y,1)};
    case Family::MR: case Family::ML:
        return {tone(9,1200),tone(1),scan(scanMs,C::Y),{.1,C::Hold},scan(scanMs/2,C::Cr),{.1,C::Hold},scan(scanMs/2,C::Cb),{.1,C::Hold}};
    case Family::Narrow:
        if(id.startsWith("mc"))return {tone(8,1900),tone(.5,2044),scan(scanMs,C::Red),scan(scanMs,C::Green),scan(scanMs,C::Blue)};
        return {tone(9,1900),tone(1,2044),scan(scanMs,C::Y),scan(scanMs,C::Cr,0,true),scan(scanMs,C::Cb,0,true),scan(scanMs,C::Y,1)};
    case Family::Robot:
        if (id == "robot36")
            return {tone(9,1200),tone(3),scan(88,C::Y),tone(4.5,row%2 ? 2300 : 1500),tone(1.5,1900),scan(44,row%2 ? C::Cb : C::Cr,0,true)};
        return {tone(9,1200),tone(3),scan(scanMs,C::Y),tone(4.5),tone(1.5,1900),scan(scanMs/2,C::Cr),tone(4.5,2300),tone(1.5),scan(scanMs/2,C::Cb)};
    default: return {};
    }
}
double Mode::imageMs() const {
    if (!implemented) return 0;
    const auto parts = line(0);
    return initialMs() + std::accumulate(parts.begin(),parts.end(),0.0,[](double s,const Segment& p){return s+p.ms;}) * (height / rowsPerLine());
}
}
