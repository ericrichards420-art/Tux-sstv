#include "codec.h"
#include "fskid.h"
#include <QPainter>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace sstv {
namespace {
constexpr double pi=std::numbers::pi;
constexpr int dspRate=12000;
[[noreturn]] void fail(const QString& s) { throw std::runtime_error(s.toStdString()); }
void check(const Cancel& cancel) { if(cancel && cancel->load()) fail("Cancelled."); }
double clip(double v) { return std::clamp(v,0.,255.); }
double value(QRgb p,Component c) {
    const double r=qRed(p),g=qGreen(p),b=qBlue(p);
    switch(c) {
    case Component::Red: return r;
    case Component::Green: return g;
    case Component::Blue: return b;
    case Component::Luma: return .299*r+.587*g+.114*b;
    // Studio-range BT.601 as specified in N7CXI Appendix B.
    case Component::Y: return 16+(65.738*r+129.057*g+25.064*b)/256;
    case Component::Cr: return 128+(112.439*r-94.154*g-18.285*b)/256;
    case Component::Cb: return 128+(-37.945*r-74.494*g+112.439*b)/256;
    default: return 0;
    }
}
QRgb rgb(double y,double cr,double cb) {
    y-=16; cr-=128; cb-=128;
    return qRgb(int(std::lround(clip((298.082*y+408.583*cr)/256))),
                int(std::lround(clip((298.082*y-100.291*cb-208.120*cr)/256))),
                int(std::lround(clip((298.082*y+516.411*cb)/256))));
}
double sinc(double x) { return std::abs(x)<1e-10 ? 1 : std::sin(pi*x)/(pi*x); }
std::vector<float> resample(const Audio& audio,const Cancel& cancel) {
    if(audio.sampleRate==dspRate) return audio.samples;
    const double ratio=double(audio.sampleRate)/dspRate;
    const int radius=int(std::ceil(16*std::max(1.,ratio)));
    const double cutoff=.46*std::min(1.,1/ratio);
    constexpr int phases=256;
    std::vector<std::vector<double>> kernel(phases,std::vector<double>(2*radius+1));
    for(int p=0;p<phases;++p) {
        double sum=0;
        for(int k=-radius;k<=radius;++k) {
            const double d=k-double(p)/phases;
            const double w=.5+.5*std::cos(pi*d/(radius+1));
            kernel[p][k+radius]=2*cutoff*sinc(2*cutoff*d)*w;
            sum+=kernel[p][k+radius];
        }
        for(auto& c:kernel[p]) c/=sum;
    }
    std::vector<float> out(size_t(std::floor(audio.samples.size()/ratio)));
    for(size_t n=0;n<out.size();++n) {
        if((n&16383)==0) check(cancel);
        const double pos=n*ratio;
        const auto center=qint64(std::floor(pos));
        const int phase=std::min(phases-1,int((pos-center)*phases));
        double sum=0;
        for(int k=-radius;k<=radius;++k) {
            const auto i=center+k;
            if(i>=0 && i<qint64(audio.samples.size())) sum+=audio.samples[size_t(i)]*kernel[phase][k+radius];
        }
        out[n]=float(sum);
    }
    return out;
}
double medianAt(const std::vector<float>& f,double sample,int radius=4) {
    std::array<double,41> v{};
    int count=0;
    const auto center=qint64(std::lround(sample));
    for(int k=-std::min(radius,20);k<=std::min(radius,20);++k) {
        const auto i=center+k;
        if(i>=0 && i<qint64(f.size()) && std::isfinite(f[size_t(i)])) v[count++]=f[size_t(i)];
    }
    if(!count) return 0;
    std::nth_element(v.begin(),v.begin()+count/2,v.begin()+count);
    return v[count/2];
}
// Find the center of a sync run close to its expected position. Windowing by
// expected width prevents the 1500 Hz black level from masquerading as sync.
double syncAdjustment(const std::vector<float>& f,double expected,double width,double offset,double syncHz) {
    const int radius=48; // +/-4 ms, sufficient for small sample clock error per line.
    const int first=std::max(0,int(expected)-radius);
    const int last=std::min(int(f.size())-1,int(expected+width)+radius);
    int run=-1,bestStart=-1,bestEnd=-1;
    double best=1e9;
    auto consider=[&](int end) {
        if(run<0) return;
        const double length=end-run;
        if(length>width*.55 && length<width*1.5) {
            const double distance=std::abs((run+end)*.5-(expected+width*.5));
            if(distance<best) { best=distance; bestStart=run; bestEnd=end; }
        }
    };
    for(int i=first;i<=last;++i) {
        if(std::abs(f[size_t(i)]-offset-syncHz)<(syncHz==1900?40:65)) { if(run<0) run=i; }
        else if(run>=0) { consider(i); run=-1; }
    }
    consider(last+1);
    return bestStart<0 ? 0 : (bestStart+bestEnd)*.5-(expected+width*.5);
}
// Fit absolute sync centers against nominal sample positions. Each match must
// lie near the predicted line and have the mode's sync width. Missing pulses
// are tolerated; a scattered/noisy set never changes the manual correction.
struct ClockFit { double origin=0, scale=1; bool valid=false; };
ClockFit measureClock(const std::vector<float>& f,const Mode& mode,double start,double seed,double offset,const Cancel& cancel) {
    ClockFit fit{start,seed,false};
    if(mode.syncMs()<=0)return fit; // AVT has no line timing reference.
    double period=0;for(const auto& part:mode.line(0))period+=part.ms*12;
    const double width=mode.syncMs()*12, first=(mode.initialMs()+mode.syncOffsetMs())*12+width/2;
    std::vector<std::pair<double,double>> points;
    auto regression=[](const auto& data) {
        double mx=0,my=0;for(const auto& [x,y]:data){mx+=x;my+=y;}mx/=data.size();my/=data.size();
        double xx=0,xy=0;for(const auto& [x,y]:data){xx+=(x-mx)*(x-mx);xy+=(x-mx)*(y-my);}
        const double scale=xx>0?xy/xx:1.;return ClockFit{my-scale*mx,scale,true};
    };
    int available=0;
    for(int line=0;line<mode.height/mode.rowsPerLine();++line) {
        check(cancel);
        const double x=first+period*line, expected=fit.origin+fit.scale*x;
        const int radius=int(period*.035+36);
        if(expected+width/2+radius>=double(f.size()))break;
        ++available;
        const int lo=std::max(0,int(expected)-radius-int(width)),hi=std::min(int(f.size())-1,int(expected)+radius+int(width));
        int begin=-1;double nearest=radius+1,center=0;
        for(int i=lo;i<=hi;++i) {
            if(std::abs(f[size_t(i)]-offset-(mode.narrow()?1900:1200))<(mode.narrow()?35:55)) {if(begin<0)begin=i;}
            else if(begin>=0) {
                const double length=i-begin,mid=(i+begin)*.5;
                if(length>width*.65 && length<width*1.3 && std::abs(mid-expected)<nearest){nearest=std::abs(mid-expected);center=mid;}
                begin=-1;
            }
        }
        if(nearest>radius)continue;
        points.emplace_back(x,center);
        if(points.size()>=6) {
            const auto candidate=regression(points);
            if(std::abs(candidate.scale-1)<=.0101)fit=candidate;
        }
    }
    fit.valid=false;
    if(points.size()<12 || points.size()<size_t(available*.6) || points.back().first-points.front().first<36000)return fit;
    auto candidate=regression(points);
    std::vector<double> errors;for(const auto& [x,y]:points)errors.push_back(std::abs(y-candidate.origin-candidate.scale*x));
    std::nth_element(errors.begin(),errors.begin()+errors.size()/2,errors.end());
    const double limit=std::clamp(errors[errors.size()/2]*4,3.,24.);
    std::vector<std::pair<double,double>> good;
    for(const auto& point:points)if(std::abs(point.second-candidate.origin-candidate.scale*point.first)<=limit)good.push_back(point);
    if(good.size()<12 || good.size()<points.size()*.8)return fit;
    candidate=regression(good);
    double error=0;for(const auto& [x,y]:good)error+=std::abs(y-candidate.origin-candidate.scale*x);
    if(error/good.size()>12 || std::abs(candidate.scale-1)>.01 || std::abs(candidate.origin-start)>period*.08)return fit;
    candidate.valid=true;return candidate;
}
struct HeaderTune { VisHeader header; double offset; bool measured; };
HeaderTune tunedHeader(const std::vector<float>& f,double seed,bool automatic,const Cancel& cancel,bool supportedOnly) {
    if(!automatic)return {detectVis(f,seed,cancel,supportedOnly),seed,false};
    std::optional<VisHeader> earliest;
    double chosen=seed;
    // VIS parity/checksum and the leader must all validate at the same offset.
    // Truncate subsequent searches at the first candidate to keep long files cheap.
    auto window=f;
    for(int trial=-1;trial<=16;++trial) {
        check(cancel);
        const double offset=trial<0?seed:-200+trial*25.;
        try {
            const auto h=detectVis(window,offset,cancel,supportedOnly);
            if(!earliest || h.imageStartMs<earliest->imageStartMs){earliest=h;chosen=offset;window.resize(std::min(window.size(),size_t(std::ceil(h.imageStartMs*12))+24));}
        } catch(const std::runtime_error&){check(cancel);}
    }
    if(!earliest)fail("No valid supported VIS header found.");
    const auto& h=*earliest;
    const double sync=h.imageStartMs-(h.narrow?550:h.code>127?540:300);
    std::vector<double> offsets;
    // Measure the remaining leader nearest the mode word. Older probes could
    // precede the recording (or land in muted audio) after late acquisition.
    // N-VIS has a 100 ms break between its leader and mode word.
    for(const int before: {20,40,60,80}) {
        const double position=(sync-(h.narrow?100+before/2:before))*12;
        for(int k=-96;k<=96;k+=8)if(position+k>=20 && position+k+20<double(f.size()))
            offsets.push_back(medianAt(f,position+k,12)-1900);
    }
    if(offsets.empty())return {h,seed,false};
    std::sort(offsets.begin(),offsets.end());
    const double measured=offsets[offsets.size()/2];
    const bool valid=std::abs(measured)<=200 && offsets[offsets.size()*3/4]-offsets[offsets.size()/4]<12 && std::abs(measured-chosen)<70;
    return {h,valid?measured:seed,valid};
}
// AVT's image has no line syncs. Recover its origin from complemented control
// words instead of assuming the image begins immediately after VIS. Require a
// consistent pair of counters so a single accidental word cannot start a frame.
double avtImageStart(const std::vector<float>& frequencies,double beginMs,double offset,double scale,bool allowLegacy,const Cancel& cancel) {
    const double bit=1000./102.4*dspRate/1000.*scale;
    const double word=17*bit;
    const auto begin=std::max(qint64(0),qint64(beginMs*dspRate/1000));
    const auto end=std::min(qint64(frequencies.size()),begin+qint64(15000*dspRate/1000.*scale));
    struct Candidate {double start;int counter,marker;};
    std::vector<Candidate> candidates;
    qint64 run=-1;
    for(qint64 i=begin;i<end;++i) {
        if((i&16383)==0)check(cancel);
        if(std::abs(frequencies[size_t(i)]-offset-1900)<45) {if(run<0)run=i;continue;}
        if(run<0)continue;
        const auto length=i-run;
        const double markerStart=(run+i)*.5-bit*.5;
        run=-1;
        if(length<bit*.6 || length>bit*1.3 || markerStart+17*bit>double(frequencies.size()))continue;
        int code=0;bool valid=true;
        for(int n=0;n<16;++n) {
            const double hz=medianAt(frequencies,markerStart+(n+1.5)*bit,8)-offset;
            const int value=std::abs(hz-1600)<100 ? 0 : std::abs(hz-2200)<100 ? 1 : -1;
            if(value<0){valid=false;break;}
            code=(code<<1)|value;
        }
        const int upper=code>>8,marker=upper>>5,counter=upper&31;
        if(!valid || (code&255)!=(upper^255) || (marker!=5 && !(allowLegacy && marker==2)))continue;
        for(auto previous=candidates.rbegin();previous!=candidates.rend();++previous) {
            const double separation=markerStart-previous->start;
            if(separation>word*5.2)break;
            const int gap=int(std::llround(separation/word));
            if(gap<1 || std::abs(separation-gap*word)>bit*.2 || marker!=previous->marker)continue;
            const int difference=counter-previous->counter;
            if(std::abs(difference)!=gap)continue;
            const bool ascending=difference>0;
            const int remaining=ascending?31-counter:counter;
            return (markerStart+(remaining+1)*word)/(dspRate/1000.);
        }
        candidates.push_back({markerStart,counter,marker});
    }
    fail("No valid AVT countdown found. A consistent pair of control words is required, or set a manual image start.");
}
}
QImage fitImage(const QImage& image,const Mode& mode) {
    if(image.isNull()) fail("Open an image first.");
    QImage out(mode.width,mode.height,QImage::Format_RGB32); out.fill(Qt::black);
    const auto scaled=image.scaled(out.size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QPainter p(&out); p.drawImage((out.width()-scaled.width())/2,(out.height()-scaled.height())/2,scaled);
    p.end();
    if(mode.family==Family::Mono) {
        for(int y=0;y<out.height();++y)for(int x=0;x<out.width();++x) {
            const auto pixel=out.pixel(x,y);
            const int gray=int(std::lround(value(pixel,Component::Luma)));
            out.setPixel(x,y,qRgb(gray,gray,gray));
        }
    }
    return out;
}
QImage testPattern() {
    QImage image(640,496,QImage::Format_RGB32);
    const std::array<QRgb,8> colors={qRgb(255,255,255),qRgb(255,255,0),qRgb(0,255,255),qRgb(0,255,0),qRgb(255,0,255),qRgb(255,0,0),qRgb(0,0,255),qRgb(0,0,0)};
    for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
        if(y<320) image.setPixel(x,y,colors[x/80]);
        else if(y<400) { const int v=x*255/639; image.setPixel(x,y,qRgb(v,v,v)); }
        else { const int v=((x/16+y/16)%2)*255; image.setPixel(x,y,qRgb(v,v,v)); }
    }
    return image;
}
Audio encode(const QImage& source,const Mode& mode,int rate,Cancel cancel,Progress progress) {
    check(cancel);
    if(!mode.implemented) fail(mode.name+": "+mode.note);
    if(rate<8000 || rate>96000) fail("Sample rate must be 8000–96000 Hz.");
    const auto image=fitImage(source,mode);
    Audio out; out.sampleRate=rate;
    out.samples.reserve(size_t(std::ceil((mode.imageMs()+mode.headerMs()+200)*rate/1000)));
    long double time=0;
    double phase=0;
    auto tone=[&](double hz,double ms) {
        time+=static_cast<long double>(ms)*rate/1000;
        const auto target=size_t(std::llround(time));
        const double step=2*pi*hz/rate;
        while(out.samples.size()<target) {
            out.samples.push_back(float(.65*std::sin(phase)));
            phase+=step;
            if(phase>=2*pi) phase-=2*pi;
        }
    };
    if(mode.narrow()) {
        tone(1900,300);tone(2100,100);tone(1900,22);
        for(int symbol:{0b101101,0b010101,mode.vis,0b010101^mode.vis})
            for(int bit=5;bit>=0;--bit)tone((symbol>>bit)&1?1900:2100,22);
    } else {
        tone(1900,300); tone(1200,10); tone(1900,300); tone(1200,30);
        int ones=0;
        for(int bit=0;bit<(mode.vis>127?16:7);++bit) { const bool set=(mode.vis>>bit)&1; ones+=set; tone(set?1100:1300,30); }
        if(mode.vis<128)tone(ones%2?1100:1300,30);
        tone(1200,30);
    }
    if(mode.family==Family::AVT) {
        constexpr double bitMs=1000./102.4;
        for(int counter=0;counter<32;++counter) {
            check(cancel);
            tone(1900,bitMs);
            const int upper=(5<<5)|counter;
            const int word=(upper<<8)|(upper^255);
            for(int bit=15;bit>=0;--bit)tone((word>>bit)&1?2200:1600,bitMs);
        }
    }
    if(mode.initialMs()) tone(1200,mode.initialMs());
    for(int row=0;row<mode.height;row+=mode.rowsPerLine()) {
        check(cancel);
        double lastHz=1500;
        for(const auto& segment:mode.line(row)) {
            if(segment.component==Component::Tone) { tone(segment.hz,segment.ms); continue; }
            if(segment.component==Component::Hold) {tone(lastHz,segment.ms);continue;}
            for(int x=0;x<mode.width;++x) {
                int y=row+segment.rowOffset;
                double v=value(image.pixel(x,y),segment.component);
                if(segment.averagePair) {
                    y=mode.id=="robot36" ? (row&~1) : row;
                    v=(value(image.pixel(x,y),segment.component)+value(image.pixel(x,y+1),segment.component))*.5;
                }
                lastHz=(mode.narrow()?2044:1500)+(mode.narrow()?256:800)*clip(v)/255;
                tone(lastHz,segment.ms/mode.width);
            }
        }
        if(progress) progress((row+mode.rowsPerLine())*100/mode.height);
    }
    // End pad provides the receive filter with lookahead for the final pixels.
    tone(1500,200);
    check(cancel);
    return out;
}
std::vector<float> demodulate(const Audio& audio,Cancel cancel) {
    if(audio.sampleRate<8000 || audio.sampleRate>96000 || audio.samples.empty() || audio.seconds()>600) fail("Invalid audio buffer.");
    for(float s:audio.samples) if(!std::isfinite(s)) fail("Non-finite audio sample.");
    const auto samples=resample(audio,cancel);
    constexpr int taps=49,delay=(taps-1)/2;
    std::array<double,taps> fir{};
    double sum=0;
    for(int k=0;k<taps;++k) {
        fir[k]=2*1000./dspRate*sinc(2*1000./dspRate*(k-delay))*(.54-.46*std::cos(2*pi*k/(taps-1)));
        sum+=fir[k];
    }
    for(auto& c:fir) c/=sum;
    std::array<std::complex<double>,taps> ring{};
    std::vector<float> result(samples.size(),0);
    const std::complex<double> step=std::polar(1.,-2*pi*1900/dspRate);
    std::complex<double> oscillator(1,0),previous(0,0);
    for(size_t i=0;i<samples.size();++i) {
        if((i&16383)==0) { check(cancel); oscillator/=std::abs(oscillator); }
        ring[i%taps]=double(samples[i])*oscillator; oscillator*=step;
        std::complex<double> z(0,0);
        for(int k=0;k<taps;++k) z+=fir[k]*ring[(i+taps-size_t(k))%taps];
        if(i>delay) {
            const auto d=z*std::conj(previous);
            result[i-delay]=std::norm(z)>1e-10 && std::norm(previous)>1e-10
                ? float(1900+std::arg(d)*dspRate/(2*pi)) : 0;
        }
        previous=z;
    }
    check(cancel);
    return result;
}
VisHeader detectVis(const std::vector<float>& f,double offset,Cancel cancel,bool supportedOnly,bool requireFullPreamble) {
    const double spms=dspRate/1000.;
    // Vote across the interior of each symbol rather than trusting one tiny
    // sample at its center. A brief fade must not erase an otherwise valid VIS.
    // Parity, checksums, leader and stop requirements still all apply.
    const auto tone=[&](double center,double hz,double spacing=4.){
        int votes=0;
        for(int n=-2;n<=2;++n)if(std::abs(medianAt(f,center+n*spacing*spms,6)-offset-hz)<65)++votes;
        return votes>=3;
    };
    const auto bitAt=[&](double center,double one,double zero,double spacing,double tolerance){
        int ones=0,zeros=0;
        for(int n=-2;n<=2;++n){
            const double hz=medianAt(f,center+n*spacing*spms,6)-offset;
            if(std::abs(hz-one)<tolerance)++ones;
            else if(std::abs(hz-zero)<tolerance)++zeros;
        }
        return ones>=3?1:zeros>=3?0:-1;
    };
    // An idle receiver may open after part of the leader has already passed.
    // Require the complete validated mode word, but accept a shorter remaining
    // leader. Replacing an active image still requires the full preamble.
    for(size_t i=1200;i+size_t(300*spms)<f.size();++i) {
        if((i&16383)==0) check(cancel);
        auto near=[&](double ms,double hz,double spacing=4.) {return tone(double(i)+ms*spms,hz,spacing);};
        // N-VIS is four six-bit symbols, MSB first, with fixed prefix and XOR.
        if(std::abs(f[i]-offset-1900)<35 && std::abs(f[i-1]-offset-1900)>=35 && i+size_t(550*spms)<f.size()
           && near(-50,2100) && near(-85,2100) && near(11,1900)) {
            const bool leader=near(-200,1900)||(!requireFullPreamble&&near(-120,1900)&&near(-130,1900)&&near(-140,1900));
            if(!leader)continue;
            if(requireFullPreamble&&(!near(-350,1900)||!near(-250,1900)))continue;
            int word=0;bool valid=true;
            for(int bit=0;bit<24;++bit) {
                const int v=bitAt(i+(33+22*bit)*spms,1900,2100,3.,55.);
                if(v<0){valid=false;break;}word=(word<<1)|v;
            }
            const int code=(word>>6)&63;
            if(valid && (word>>12)==((45<<6)|21) && (word&63)==(21^code)
               && (!supportedOnly || modeByVis(code,true)))return {code,double(i)/spms+550,true};
        }
        if(std::abs(f[i]-offset-1200)>45 || std::abs(f[i-1]-offset-1200)<=45) continue;
        if(!near(15,1200))continue;
        bool leader=near(-50,1900)&&near(-150,1900)&&near(-250,1900);
        if(!leader&&!requireFullPreamble){
            leader=true;
            for(int before=20;before<=90;before+=10)if(!near(-before,1900)){leader=false;break;}
        }
        if(!leader)continue;
        // Preempting a picture requires the whole leader/break/leader pattern,
        // not just VIS-like tones that might occur inside picture audio.
        if(requireFullPreamble&&(!near(-360,1900)||!near(-460,1900)||!near(-560,1900)||!near(-305,1200,1.)))continue;
        int code=0,ones=0,rawByte=0; bool good=true;
        for(int bit=0;bit<8;++bit) {
            const int v=bitAt(i+(45+bit*30)*spms,1100,1300,4.,65.);
            if(v<0) { good=false; break; }
            ones+=v;
            rawByte|=v<<bit;
            if(bit<7) code|=v<<bit;
        }
        if(good && rawByte==0x23 && i+size_t(540*spms)<f.size() && near(525,1200)) {
            int ext=rawByte;
            for(int bit=8;bit<16;++bit) {
                const int v=bitAt(i+(45+bit*30)*spms,1100,1300,4.,65.);
                if(v<0){good=false;break;}ext|=v<<bit;
            }
            if(good && (!supportedOnly || modeByVis(ext)))return {ext,double(i)/spms+540,false};
            continue;
        }
        if(good && ones%2==0 && near(285,1200) && (!supportedOnly || modeByVis(code))) return {code,double(i)/spms+300,false};
    }
    fail("No valid VIS, extended VIS, or N-VIS header found. Use a complete recording, or select a mode and manual image start.");
}
DecodeResult decode(const Audio& audio,const DecodeOptions& options,Cancel cancel,Progress progress,ModeDetected detected) {
    if(!std::isfinite(options.clockPpm) || std::abs(options.clockPpm)>10000 || !std::isfinite(options.frequencyOffset) || std::abs(options.frequencyOffset)>200 || !std::isfinite(options.manualStartMs) || options.manualStartMs < -1)
        fail("Invalid receive correction settings.");
    if(progress) progress(0);
    const Mode* mode=nullptr;
    if(!options.forceMode.isEmpty()) {
        mode=modeById(options.forceMode);
        if(!mode) fail("Unknown receive mode.");
        if(!mode->implemented) fail(mode->name+": "+mode->note);
    }
    if(options.manualStartMs>=0 && !mode) fail("Select a mode for manual image start.");
    const auto freq=demodulate(audio,cancel);
    if(progress) progress(25);
    double start=options.manualStartMs;
    double offset=options.frequencyOffset,clockPpm=options.clockPpm;
    bool frequencyMeasured=false;
    if(start<0) {
        std::optional<VisHeader> vis;
        try {
            const auto tuned=tunedHeader(freq,offset,options.autoFrequency,cancel,!mode);
            vis=tuned.header;offset=tuned.offset;frequencyMeasured=tuned.measured;
        } catch(const std::runtime_error&) {
            check(cancel);
            if(!mode || mode->family!=Family::AVT)throw;
        }
        if(vis) {
            if(!mode)mode=modeByVis(vis->code,vis->narrow);
            if(!mode)fail(QString("VIS %1 is not in the MMSSTV compatibility catalog.").arg(vis->code));
            if(!mode->implemented)fail(mode->name+": "+mode->note);
            start=vis->imageStartMs;
        }
        if(detected)detected(mode->id);
        if(mode->family==Family::AVT)
            start=avtImageStart(freq,vis?start:0,offset,1+options.clockPpm*1e-6,true,cancel);
    }
    if(options.manualStartMs>=0 && detected)detected(mode->id);
    if(start>=audio.seconds()*1000) fail("Image start is beyond the end of the recording.");
    const auto fit=options.autoClock?measureClock(freq,*mode,start*12,1+clockPpm*1e-6,offset,cancel):ClockFit{};
    if(fit.valid){start=fit.origin/12;clockPpm=(fit.scale-1)*1e6;}
    const double clockScale=1+clockPpm*1e-6;
    const double spms=dspRate/1000.*clockScale;
    double lineStart=start*dspRate/1000.+mode->initialMs()*spms;
    DecodeResult result; result.modeId=mode->id; result.startMs=start;
    result.endMs=start+mode->imageMs()*clockScale;
    result.clockPpm=clockPpm;result.frequencyOffset=offset;result.clockMeasured=fit.valid;result.frequencyMeasured=frequencyMeasured;
    result.image=QImage(mode->width,mode->height,QImage::Format_RGB32); result.image.fill(QColor(35,39,45));
    std::vector<double> cr(mode->width,128),cb(mode->width,128),previousY(mode->width,16);
    const bool colorDifference=mode->family==Family::Robot || mode->family==Family::PD || mode->family==Family::MP || mode->family==Family::MR || mode->family==Family::ML || (mode->narrow() && mode->id.startsWith("mp"));
    for(int row=0;row<mode->height;row+=mode->rowsPerLine()) {
        check(cancel);
        const auto parts=mode->line(row);
        double lineMs=0; for(const auto& p:parts) lineMs+=p.ms;
        if(lineStart+lineMs*spms>double(freq.size())+24) {
            if(!options.allowPartial) fail(QString("Incomplete %1 image: %2 of %3 rows available.").arg(mode->name).arg(row).arg(mode->height));
            break;
        }
        if(options.trackSync && mode->syncMs()>0) {
            const double shift=syncAdjustment(freq,lineStart+mode->syncOffsetMs()*spms,mode->syncMs()*spms,offset,mode->narrow()?1900:1200);
            lineStart+=std::clamp(shift,-24.,24.);
        }
        std::vector<double> r(mode->width,0),g=r,b=r,y0=r,y1=r;
        double pos=lineStart;
        for(const auto& p:parts) {
            if(p.component!=Component::Tone && p.component!=Component::Hold) {
                std::vector<double>* dest=nullptr;
                switch(p.component) {
                case Component::Red: dest=&r; break; case Component::Green: dest=&g; break; case Component::Blue: dest=&b; break;
                case Component::Luma: dest=&y0;break;
                case Component::Y: dest=p.rowOffset?&y1:&y0; break; case Component::Cr: dest=&cr; break; case Component::Cb: dest=&cb; break;
                default: break;
                }
                const double pixelSamples=p.ms*spms/mode->width;
                for(int x=0;x<mode->width;++x) {
                    const double hz=medianAt(freq,pos+(x+.5)*pixelSamples,std::clamp(int(pixelSamples*.2),0,6))-offset;
                    (*dest)[x]=clip((hz-(mode->narrow()?2044:1500))*255/(mode->narrow()?256:800));
                }
            }
            pos+=p.ms*spms;
        }
        for(int x=0;x<mode->width;++x) {
            if(colorDifference) {
                result.image.setPixel(x,row,rgb(y0[x],cr[x],cb[x]));
                if(mode->rowsPerLine()==2) result.image.setPixel(x,row+1,rgb(y1[x],cr[x],cb[x]));
                // Robot 36 shares one chroma pair between adjacent rows. Render
                // both rows once the second chroma component has arrived.
                if(mode->id=="robot36" && row%2) result.image.setPixel(x,row-1,rgb(previousY[x],cr[x],cb[x]));
            } else if(mode->family==Family::Mono)result.image.setPixel(x,row,qRgb(int(y0[x]),int(y0[x]),int(y0[x])));
            else result.image.setPixel(x,row,qRgb(int(r[x]),int(g[x]),int(b[x])));
        }
        previousY=std::move(y0);
        result.rows=row+mode->rowsPerLine();
        lineStart+=lineMs*spms;
        if(progress) progress(25+result.rows*75/mode->height);
    }
    result.complete=result.rows==mode->height;
    // Sync tracking can move every line relative to the nominal clock. Consume
    // only the audio actually decoded, preserving a following header or FSKID.
    if(result.complete)result.endMs=lineStart/(dspRate/1000.);
    if(result.complete)if(const auto id=detectFskId(freq,result.endMs,offset,clockPpm,cancel))result.callsign=id->callsign;
    result.message=QString("%1 · %2/%3 rows · %4").arg(mode->name).arg(result.rows).arg(mode->height).arg(result.complete?"complete":"partial");
    if(!result.callsign.isEmpty())result.message+=" · FSKID "+result.callsign;
    if(options.autoClock)result.message+=fit.valid?QString(" · clock %1 ppm").arg(clockPpm,0,'f',0):" · clock: manual (no reliable sync fit)";
    if(options.autoFrequency)result.message+=frequencyMeasured?QString(" · AFC %1 Hz").arg(offset,0,'f',1):" · AFC: manual (no leader estimate)";
    return result;
}
}
