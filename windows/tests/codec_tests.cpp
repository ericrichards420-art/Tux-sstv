#include "core/codec.h"
#include "core/receiver.h"
#include "core/morse.h"
#include "core/composition.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QRandomGenerator>
#include <QtEndian>
#include <cmath>
#include <bit>
#include <numbers>
#include <set>

namespace {
QImage bars(int w,int h) {
    QImage image(w,h,QImage::Format_RGB32);
    const QRgb colors[]={qRgb(255,255,255),qRgb(255,255,0),qRgb(0,255,255),qRgb(0,255,0),qRgb(255,0,255),qRgb(255,0,0),qRgb(0,0,255),qRgb(0,0,0)};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)image.setPixel(x,y,colors[x*8/w]);
    return image;
}
QImage grayscale(QImage image) {
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x) {
        const auto p=image.pixel(x,y);const int v=int(std::lround(.299*qRed(p)+.587*qGreen(p)+.114*qBlue(p)));
        image.setPixel(x,y,qRgb(v,v,v));
    }
    return image;
}
double barError(const QImage& actual,const QImage& expected) {
    double total=0;int n=0;
    for(int y=8;y<actual.height()-8;y+=17)for(int bar=0;bar<8;++bar) {
        const int x=(2*bar+1)*actual.width()/16;
        const auto a=actual.pixel(x,y),b=expected.pixel(x,y);
        total+=std::abs(qRed(a)-qRed(b))+std::abs(qGreen(a)-qGreen(b))+std::abs(qBlue(a)-qBlue(b));n+=3;
    }
    return total/n;
}
// Independent standard Martin 2 fixture: direct sample-time classification,
// no use of Mode::line(), encode(), or core color conversion. Eight color bars,
// 217 ms leading silence, configurable offset, clock error and VIS parity.
sstv::Audio referenceMartin(int rate,double offset=0,double ppm=0,bool badParity=false,int rows=256,int code=40) {
    constexpr double lead=.217,header=.910,line=.226798;
    const double stretch=1+ppm*1e-6;
    sstv::Audio audio;audio.sampleRate=rate;
    audio.samples.resize(size_t(std::ceil((lead+(header+line*rows+.05)*stretch)*rate)));
    const auto image=bars(320,256);
    double phase=.37;
    for(size_t n=0;n<audio.samples.size();++n) {
        double t=(double(n)/rate-lead)/stretch;
        if(t<0)continue;
        double hz=1500;
        if(t<.3)hz=1900;
        else if(t<.31)hz=1200;
        else if(t<.61)hz=1900;
        else if(t<.64)hz=1200;
        else if(t<.88) {
            const int bit=std::min(7,int((t-.64)/.03));
            const bool one=bit==7?(bool(std::popcount(unsigned(code))&1)^badParity):bool((code>>bit)&1);
            hz=one?1100:1300;
        } else if(t<.91)hz=1200;
        else if(t<header+line*rows) {
            const double pos=std::fmod(t-header,line);
            if(pos<.004862)hz=1200;
            else {
                const double data=pos-.005434;
                const int component=int(std::floor(data/.073788));
                const double scan=data-component*.073788;
                if(component>=0 && component<3 && scan>=0 && scan<.073216) {
                    const int x=std::min(319,int(scan/.073216*320));const auto pixel=image.pixel(x,0);
                    const double v=component==0?qGreen(pixel):component==1?qBlue(pixel):qRed(pixel);
                    hz=1500+800*v/255;
                }
            }
        }
        // Clock stretch changes timing, not pitch. Offset is an independent knob.
        phase+=2*std::numbers::pi*(hz+offset)/rate;
        if(phase>2*std::numbers::pi)phase-=2*std::numbers::pi;
        audio.samples[n]=float(.5*std::sin(phase));
    }
    return audio;
}
// Independently classify the author-specified MC140-N waveform in sample time.
sstv::Audio referenceNarrow(bool badChecksum=false,double offset=0) {
    constexpr double lead=.123,header=.950,line=.5485;
    const int symbols[]={45,21,21,badChecksum?1:0};
    sstv::Audio audio;audio.sampleRate=12000;
    audio.samples.resize(size_t((lead+header+line*256+.05)*audio.sampleRate));
    const auto image=bars(320,256);double phase=.23;
    for(size_t n=0;n<audio.samples.size();++n) {
        const double t=double(n)/audio.sampleRate-lead;
        if(t<0)continue;
        double hz=2044;
        if(t<.3)hz=1900;
        else if(t<.4)hz=2100;
        else if(t<.422)hz=1900;
        else if(t<header) {
            const int bit=std::min(23,int((t-.422)/.022));
            hz=(symbols[bit/6]>>(5-bit%6))&1?1900:2100;
        } else if(t<header+line*256) {
            const double pos=std::fmod(t-header,line);
            if(pos<.008)hz=1900;
            else if(pos>=.0085) {
                const double scan=pos-.0085;
                const int component=std::min(2,int(scan/.180));
                const int x=std::clamp(int(std::fmod(scan,.180)/.180*320),0,319);
                const auto color=image.pixel(x,0);
                const int value=component==0?qRed(color):component==1?qGreen(color):qBlue(color);
                hz=2044+256.*value/255;
            }
        }
        phase+=2*std::numbers::pi*(hz+offset)/audio.sampleRate;
        if(phase>2*std::numbers::pi)phase-=2*std::numbers::pi;
        audio.samples[n]=float(.45*std::sin(phase));
    }
    return audio;
}
// Independently generated sample-time fixtures. These constants do not come
// from Mode::line(), the production synthesizer or its color conversion.
sstv::Audio referenceLegacy(const QString& id) {
    const bool mono=id.startsWith("bw"),robot=id=="robot24";
    const int width=(mono||robot)?160:320,height=(mono||robot)?120:256;
    const int vis=id=="bw8"?2:id=="bw12"?6:robot?4:id=="sc260"?59:63;
    const double sync=mono?.007:robot?.009:id=="sc260"?.0055006:.00552248;
    const double scan=id=="bw8"?.0599:id=="bw12"?.093:robot?.088:id=="sc260"?.078128:.1565;
    const double line=mono?sync+scan:robot?.200:sync+.0005+3*scan;
    constexpr double lead=.137,header=.910;
    const auto picture=bars(width,height);
    sstv::Audio audio;audio.sampleRate=12000;audio.samples.resize(size_t(std::ceil((lead+header+line*height+.05)*audio.sampleRate)));
    double phase=.19;
    for(size_t n=0;n<audio.samples.size();++n) {
        const double t=double(n)/audio.sampleRate-lead;if(t<0)continue;
        double hz=1500;
        if(t<.300)hz=1900;
        else if(t<.310)hz=1200;
        else if(t<.610)hz=1900;
        else if(t<.640)hz=1200;
        else if(t<.880) {
            const int bit=std::min(7,int((t-.640)/.030));
            const int ones=std::popcount(unsigned(vis));
            const bool set=bit==7?bool(ones%2):bool((vis>>bit)&1);hz=set?1100:1300;
        } else if(t<header)hz=1200;
        else if(t<header+line*height) {
            const double pos=std::fmod(t-header,line);
            if(pos<sync)hz=1200;
            else {
                double coordinate=-1;int component=-1;
                if(mono){coordinate=(pos-sync)/scan;component=0;}
                else if(robot) {
                    if(pos>=.012 && pos<.100){coordinate=(pos-.012)/.088;component=0;}
                    else if(pos>=.100 && pos<.1045)hz=1500;
                    else if(pos>=.1045 && pos<.106)hz=1900;
                    else if(pos>=.106 && pos<.150){coordinate=(pos-.106)/.044;component=1;}
                    else if(pos>=.150 && pos<.1545)hz=2300;
                    else if(pos>=.1545 && pos<.156)hz=1500;
                    else if(pos>=.156){coordinate=(pos-.156)/.044;component=2;}
                } else if(pos>=sync+.0005) {
                    const double data=pos-sync-.0005;component=std::min(2,int(data/scan));coordinate=(data-component*scan)/scan;
                }
                if(coordinate>=0 && component>=0) {
                    const auto p=picture.pixel(std::clamp(int(coordinate*width),0,width-1),0);
                    const double r=qRed(p),g=qGreen(p),b=qBlue(p);double value;
                    if(mono)value=.299*r+.587*g+.114*b;
                    else if(robot) {
                        if(component==0)value=16+219*(.299*r+.587*g+.114*b)/255;
                        else if(component==1)value=128+224*(.5*r-.418688*g-.081312*b)/255;
                        else value=128+224*(-.168736*r-.331264*g+.5*b)/255;
                    } else value=component==0?r:component==1?g:b;
                    hz=1500+800*value/255;
                }
            }
        }
        phase+=2*std::numbers::pi*hz/audio.sampleRate;if(phase>2*std::numbers::pi)phase-=2*std::numbers::pi;
        audio.samples[n]=float(.5*std::sin(phase));
    }
    return audio;
}
struct AvtFixture {
    int marker=5;
    bool descending=false;
    quint32 corruptWords=0;
    int sampleRate=12000;
    double ppm=0,offset=0,attention=0;
    int rows=240;
};
sstv::Audio referenceAvt(const AvtFixture& config={}) {
    constexpr double lead=.083,vis=.910,bit=20./2048,word=17*bit;
    const double header=vis+config.attention+32*word,scale=1+config.ppm*1e-6;
    sstv::Audio audio;audio.sampleRate=config.sampleRate;
    audio.samples.resize(size_t(std::ceil((lead+(header+.375*config.rows+.05)*scale)*audio.sampleRate)));
    const auto picture=bars(320,240);double phase=.27;
    for(size_t n=0;n<audio.samples.size();++n) {
        const double t=(double(n)/audio.sampleRate-lead)/scale;if(t<0)continue;
        double hz=1500;
        if(t<.300)hz=1900;
        else if(t<.310)hz=1200;
        else if(t<.610)hz=1900;
        else if(t<.640)hz=1200;
        else if(t<.880){const int b=std::min(7,int((t-.640)/.030));hz=b<7 && ((68>>b)&1)?1100:1300;}
        else if(t<vis)hz=1200;
        else if(t<vis+config.attention)hz=1900;
        else if(t<header) {
            const double time=t-vis-config.attention;const int frame=std::min(31,int(time/word));
            const double pos=time-frame*word;
            if(pos<bit)hz=1900;
            else {
                const int counter=config.descending?31-frame:frame,upper=(config.marker<<5)|counter;
                int code=(upper<<8)|(upper^255);if(config.corruptWords&(quint32(1)<<frame))code^=1;
                const int b=std::min(15,int((pos-bit)/bit));hz=(code>>(15-b))&1?2200:1600;
            }
        } else if(t<header+.375*config.rows) {
            const double pos=std::fmod(t-header,.375);const int component=std::min(2,int(pos/.125));
            const int x=std::clamp(int((pos-component*.125)/.125*320),0,319);
            const auto p=picture.pixel(x,0);const int value=component==0?qRed(p):component==1?qGreen(p):qBlue(p);hz=1500+800.*value/255;
        }
        phase+=2*std::numbers::pi*(hz+config.offset)/audio.sampleRate;if(phase>2*std::numbers::pi)phase-=2*std::numbers::pi;
        audio.samples[n]=float(.45*std::sin(phase));
    }
    return audio;
}
double toneFrequency(const sstv::Audio& audio,double startMs,double endMs) {
    std::vector<double> crossings;
    const auto first=size_t(startMs*audio.sampleRate/1000),last=size_t(endMs*audio.sampleRate/1000);
    for(size_t n=std::max(size_t(1),first);n<last;++n) {
        const auto a=audio.samples[n-1],b=audio.samples[n];
        if(a<=0 && b>0)crossings.push_back(n-1+double(-a)/(b-a));
    }
    if(crossings.size()<2)return 0;
    return (crossings.size()-1)*audio.sampleRate/(crossings.back()-crossings.front());
}
}
class CodecTests:public QObject {
    Q_OBJECT
private slots:
    void compositionModes_data() {
        QTest::addColumn<QString>("id");for(const auto& mode:sstv::modes())QTest::newRow(qPrintable(mode.id))<<mode.id;
    }
    void compositionModes() {
        QFETCH(QString,id);const auto& mode=*sstv::modeById(id);
        QImage source(160,120,QImage::Format_RGB32);source.fill(QColor("#224466"));const auto original=source;
        sstv::TextOverlay text;text.text="73! 📻 👋🏽\nHello from Earth 🌎";text.color=Qt::yellow;text.backing=true;
        const auto result=sstv::composeTransmitImage(source,mode,"K1ABC",text);
        QCOMPARE(result.image.size(),QSize(mode.width,mode.height));QCOMPARE(source,original);
        const auto base=sstv::composeTransmitImage(source,mode,"K1ABC").image;QVERIFY(result.image!=base);
        const int callHeight=std::max(12,mode.width/20)+8;
        for(int y=0;y<callHeight;++y)for(int x=0;x<mode.width;++x)QCOMPARE(result.image.pixel(x,y),base.pixel(x,y));
        if(mode.family==sstv::Family::Mono)for(int y=0;y<mode.height;++y)for(int x=0;x<mode.width;++x) {
            const auto p=result.image.pixel(x,y);QCOMPARE(qRed(p),qGreen(p));QCOMPARE(qRed(p),qBlue(p));
        }
    }
    void colorEmojiComposition() {
        QImage source(320,256,QImage::Format_RGB32);source.fill(Qt::black);const auto& mode=*sstv::modeById("martin2");
        for(const auto& emoji:{"😀","👋🏽","👩‍🚀","❤️","🇨🇦"}) {
            sstv::TextOverlay overlay;overlay.text=QString::fromUtf8(emoji);overlay.sizePercent=18;overlay.position=4;overlay.outline=false;
            const auto result=sstv::composeTransmitImage(source,mode,{},overlay);int colored=0;
            for(int y=0;y<result.image.height();++y)for(int x=0;x<result.image.width();++x){const auto p=result.image.pixel(x,y);if(std::max({qRed(p),qGreen(p),qBlue(p)})-std::min({qRed(p),qGreen(p),qBlue(p)})>30)++colored;}
            QVERIFY2(colored>30,emoji);
        }
        for(const auto anchor:{0,4,8}) {
            sstv::TextOverlay overlay;overlay.text="73";overlay.position=anchor;overlay.outline=false;
            const auto image=sstv::composeTransmitImage(source,mode,{},overlay).image;QRect ink;
            for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(qRed(image.pixel(x,y))>20)ink=ink.united(QRect(x,y,1,1));
            QVERIFY(!ink.isEmpty());if(anchor==0){QVERIFY(ink.center().x()<100);QVERIFY(ink.center().y()<80);}else if(anchor==8){QVERIFY(ink.center().x()>220);QVERIFY(ink.center().y()>180);}else{QVERIFY(std::abs(ink.center().x()-160)<20);QVERIFY(std::abs(ink.center().y()-128)<20);}
        }
        sstv::TextOverlay overlay;overlay.text="HELLO 73 📻";const auto image=sstv::composeTransmitImage(source,mode,{},overlay).image;
        const auto decoded=sstv::decode(sstv::encode(image,mode,12000));QVERIFY(decoded.complete);
        int bright=0;for(int y=mode.height/2;y<mode.height;++y)for(int x=0;x<mode.width;++x)if(qRed(decoded.image.pixel(x,y))>100)++bright;QVERIFY(bright>100);
    }
    void cleanTextPreservesBackgroundAndHasNoColoredFringe(){
        const auto& mode=*sstv::modeById("martin2");QImage source(mode.width,mode.height,QImage::Format_RGB32);source.fill(QColor(140,140,140));const auto original=source;
        sstv::TextOverlay overlay;overlay.text="OpenSSTV 73!";overlay.sizePercent=8;overlay.position=4;overlay.backing=false;
        const auto first=sstv::composeTransmitImage(source,mode,"KI5BZO",overlay);std::set<int> edgeLevels;int changed=0;
        for(int y=0;y<mode.height;++y)for(int x=0;x<mode.width;++x){const auto pixel=first.image.pixel(x,y);QCOMPARE(qRed(pixel),qGreen(pixel));QCOMPARE(qRed(pixel),qBlue(pixel));
            if(!first.captionBounds.contains(x,y)&&!first.callsignBounds.contains(x,y))QCOMPARE(pixel,source.pixel(x,y));
            if(pixel!=source.pixel(x,y)){++changed;if(qRed(pixel)>0&&qRed(pixel)<255)edgeLevels.insert(qRed(pixel));}
        }
        QVERIFY(changed>100);QVERIFY(edgeLevels.size()>20);QCOMPARE(source,original);
        for(int size:{3,18,5,11,8}){auto other=overlay;other.sizePercent=size;other.text="Different text 📻";sstv::composeTransmitImage(source,mode,"N0CALL",other);}
        QCOMPARE(sstv::composeTransmitImage(source,mode,"KI5BZO",overlay).image,first.image);
        overlay.freePosition=true;overlay.relativePosition={.1,.75};const auto moved=sstv::composeTransmitImage(source,mode,"KI5BZO",overlay);
        for(int y=first.captionBounds.top();y<=first.captionBounds.bottom();++y)for(int x=first.captionBounds.left();x<=first.captionBounds.right();++x)
            if(!moved.captionBounds.contains(x,y)&&!moved.callsignBounds.contains(x,y))QCOMPARE(moved.image.pixel(x,y),source.pixel(x,y));
    }
    void plainTextCoverageHasNoColorFringe_data(){
        QTest::addColumn<int>("layers");QTest::addColumn<int>("background");
        for(int layers:{0,1,2})for(int background:{0,140,255})
            QTest::newRow(qPrintable(QString("layers-%1-background-%2").arg(layers).arg(background)))<<layers<<background;
    }
    void plainTextCoverageHasNoColorFringe(){
        QFETCH(int,layers);QFETCH(int,background);
        const auto& mode=*sstv::modeById("martin2");QImage source(mode.width,mode.height,QImage::Format_RGB32);source.fill(QColor(background,background,background));const auto original=source;
        sstv::TextOverlay overlay;if(layers!=0)overlay.text="Clean edges 73!";overlay.sizePercent=8;overlay.position=4;overlay.outline=false;
        overlay.color=background==255?QColor(0,0,0,128):QColor(255,255,255,128);
        const auto result=sstv::composeTransmitImage(source,mode,layers==1?QString{}:QString("KI5BZO"),overlay);
        int changed=0;std::set<int> levels;
        for(int y=0;y<mode.height;++y)for(int x=0;x<mode.width;++x){
            const auto pixel=result.image.pixel(x,y);QCOMPARE(qRed(pixel),qGreen(pixel));QCOMPARE(qRed(pixel),qBlue(pixel));
            if(!result.captionBounds.contains(x,y)&&!result.callsignBounds.contains(x,y))QCOMPARE(pixel,source.pixel(x,y));
            if(pixel!=source.pixel(x,y)){++changed;levels.insert(qRed(pixel));}
        }
        QVERIFY(changed>100);QVERIFY(levels.size()>5);QCOMPARE(source,original);
    }
    void plainCaptionPreservesChosenColorAndOpacity(){
        const auto& mode=*sstv::modeById("martin2");QImage source(mode.width,mode.height,QImage::Format_RGB32);source.fill(Qt::black);
        sstv::TextOverlay overlay;overlay.text="COLORED 73";overlay.color=QColor(200,80,20);overlay.sizePercent=18;overlay.position=4;overlay.outline=false;
        const auto opaque=sstv::composeTransmitImage(source,mode,{},overlay).image;
        overlay.color.setAlpha(128);const auto image=sstv::composeTransmitImage(source,mode,{},overlay).image;int visible=0;
        for(int y=0;y<mode.height;++y)for(int x=0;x<mode.width;++x){const auto pixel=image.pixel(x,y);
            const auto full=opaque.pixel(x,y);if(qRed(full)>10)++visible;
            // Compare against the same glyph coverage, independent of which
            // font the platform selects. Allow one step of 8-bit rounding.
            QVERIFY2(std::abs(qRed(pixel)-(qRed(full)*128+127)/255)<=1,qPrintable(QString("red at %1,%2: opaque %3 half %4").arg(x).arg(y).arg(qRed(full)).arg(qRed(pixel))));
            QVERIFY2(std::abs(qGreen(pixel)-(qGreen(full)*128+127)/255)<=1,qPrintable(QString("green at %1,%2: opaque %3 half %4").arg(x).arg(y).arg(qGreen(full)).arg(qGreen(pixel))));
            QVERIFY2(std::abs(qBlue(pixel)-(qBlue(full)*128+127)/255)<=1,qPrintable(QString("blue at %1,%2: opaque %3 half %4").arg(x).arg(y).arg(qBlue(full)).arg(qBlue(pixel))));
            QVERIFY(qRed(pixel)>=qGreen(pixel));QVERIFY(qGreen(pixel)>=qBlue(pixel));
        }
        QVERIFY(visible>100);
    }
    void transparentCaptionKeepsBackgroundAndOutlineTransparent(){
        const auto& mode=*sstv::modeById("martin2");QImage source(mode.width,mode.height,QImage::Format_RGB32);source.fill(QColor(24,65,95));
        sstv::TextOverlay overlay;overlay.text="TRANSPARENT 73";overlay.color=QColor(200,80,20,0);overlay.sizePercent=18;overlay.position=4;overlay.outline=true;
        QCOMPARE(sstv::composeTransmitImage(source,mode,{},overlay).image,source);
    }
    void morseTiming_data() {
        QTest::addColumn<int>("rate");QTest::addColumn<int>("wpm");QTest::addColumn<int>("tone");
        QTest::newRow("8k-slow")<<8000<<5<<400;QTest::newRow("44k-fractional")<<44100<<13<<1000;
        QTest::newRow("48k-default")<<48000<<20<<800;QTest::newRow("96k-fast")<<96000<<40<<2500;
    }
    void morseTiming() {
        QFETCH(int,rate);QFETCH(int,wpm);QFETCH(int,tone);
        sstv::Audio audio;audio.sampleRate=rate;audio.samples.assign(17,.123f);
        const sstv::MorseId id{"et",wpm,tone};const double dot=1200./wpm;
        QVERIFY(std::abs(sstv::morseIdDurationMs(id)-(700+7*dot))<1e-8);
        sstv::appendMorseId(audio,id);
        const auto index=[&](double ms){return size_t(17+std::llround(ms*rate/1000));};
        QCOMPARE(audio.samples.size(),index(700+7*dot));
        QVERIFY(std::all_of(audio.samples.begin(),audio.samples.begin()+17,[](float v){return v==.123f;}));
        const auto quiet=[&](size_t from,size_t to){return std::all_of(audio.samples.begin()+from,audio.samples.begin()+to,[](float v){return v==0;});};
        QVERIFY(quiet(17,index(500)));QVERIFY(quiet(index(500+dot),index(500+4*dot)));QVERIFY(quiet(index(500+7*dot),audio.samples.size()));
        for(const auto [from,to]:{std::pair{index(500),index(500+dot)},std::pair{index(500+4*dot),index(500+7*dot)}}) {
            double sum=0;int crossings=0;
            for(size_t i=from;i<to;++i){sum+=audio.samples[i]*audio.samples[i];if(i+1<to && audio.samples[i]<=0 && audio.samples[i+1]>0)++crossings;}
            QVERIFY(std::sqrt(sum/(to-from))>.3);QVERIFY(std::abs(double(crossings)*rate/(to-from)-tone)<double(rate)/(to-from)+1);
            QCOMPARE(audio.samples[from],0.f);QCOMPARE(audio.samples[to-1],0.f);
        }
    }
    void morseCallsignAndSstvTail() {
        sstv::Audio keyed;const sstv::MorseId id{"k1abc/p",20,800};sstv::appendMorseId(keyed,id);
        // Independent expected ITU pattern; inspect energy at each dot-unit center.
        const QStringList expected={"-.-",".----",".-","-...","-.-.","-..-.",".--."};int unit=0;
        const auto checkUnits=[&](bool on,int count) {
            for(int n=0;n<count;++n) {
                const int center=int((500+(unit+n+.5)*60)*12);double energy=0;
                for(int i=center-36;i<center+36;++i)energy+=keyed.samples[size_t(i)]*keyed.samples[size_t(i)];
                if(on && energy<5)return false;if(!on && energy!=0)return false;
            }
            unit+=count;return true;
        };
        for(int i=0;i<expected.size();++i) {
            if(i)QVERIFY(checkUnits(false,3));
            for(int j=0;j<expected[i].size();++j){if(j)QVERIFY(checkUnits(false,1));QVERIFY(checkUnits(true,expected[i][j]=='-'?3:1));}
        }
        QCOMPARE(keyed.samples.size(),size_t((700+unit*60)*12));
        auto audio=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);const auto original=audio.samples;
        sstv::appendMorseId(audio,id);QVERIFY(std::equal(original.begin(),original.end(),audio.samples.begin()));
        const auto image=sstv::decode(audio);QVERIFY(image.complete);QCOMPARE(image.modeId,QString("bw8"));
        for(const auto& call:{QString{},QString("N0 CALL"),QString("K1ABC?"),QString(21,'A')}) {
            const auto before=audio.samples.size();QVERIFY_EXCEPTION_THROWN(sstv::appendMorseId(audio,{call,20,800}),std::runtime_error);QCOMPARE(audio.samples.size(),before);
        }
        QVERIFY_EXCEPTION_THROWN(sstv::morseIdDurationMs({"K1ABC",0,800}),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(sstv::morseIdDurationMs({"K1ABC",20,300}),std::runtime_error);
        auto cancel=std::make_shared<std::atomic_bool>(true);QVERIFY_EXCEPTION_THROWN(sstv::appendMorseId(audio,id,cancel),std::runtime_error);
    }
    void catalog() {
        QCOMPARE(sstv::modes().size(),size_t(43));
        std::set<QString> names;std::set<int> codes;int implemented=0;
        for(const auto& m:sstv::modes()) {
            QVERIFY(names.insert(m.id).second);
            if(m.implemented){++implemented;QVERIFY(m.vis>=0);QVERIFY(codes.insert(m.vis+(m.narrow()?0x1000000:0)).second);QVERIFY(m.imageMs()>0);}
            else QVERIFY(!m.note.isEmpty());
        }
        QCOMPARE(implemented,43);
    }
    void compositionCachesFollowEditedPixels(){
        const auto& color=*sstv::modeById("robot36");const auto& mono=*sstv::modeById("bw8");
        auto source=sstv::testPattern();const auto saved=source.copy();
        auto first=sstv::composeTransmitLayers(source,color,{},{});first.image.fill(Qt::magenta);
        QCOMPARE(sstv::composeTransmitLayers(source,color,{},{}).image,sstv::fitImage(saved,color));
        source.fill(Qt::red);QCOMPARE(sstv::composeTransmitLayers(source,color,{},{}).image,sstv::fitImage(source,color));
        QCOMPARE(sstv::composeTransmitLayers(source,mono,{},{}).image,sstv::fitImage(source,mono));
        const auto a=sstv::composeTransmitLayers(source,color,"N0CALL",{});const auto b=sstv::composeTransmitLayers(source,color,"K1ABC",{});QVERIFY(a.image!=b.image);
        sstv::TextOverlay caption;caption.text="Clean text";caption.freePosition=true;caption.relativePosition={.1,.2};
        const auto before=sstv::composeTransmitImage(saved,color,"N0CALL",caption);caption.relativePosition={.6,.7};const auto after=sstv::composeTransmitImage(saved,color,"N0CALL",caption);
        const auto base=sstv::composeTransmitLayers(saved,color,"N0CALL",{});QCOMPARE(after.image.copy(before.captionBounds),base.image.copy(before.captionBounds));QVERIFY(after.captionBounds!=before.captionBounds);
    }
    void removedModeIsUnsupported(){
        QVERIFY(!sstv::modeById("mca"));sstv::DecodeOptions options;options.forceMode="mca";
        sstv::Audio audio;audio.samples.resize(12000);QVERIFY_EXCEPTION_THROWN(sstv::decode(audio,options),std::runtime_error);QVERIFY_EXCEPTION_THROWN(sstv::Receiver{options},std::runtime_error);
    }
    void timingContracts() {
        const auto* s=sstv::modeById("scottie1");QVERIFY(std::abs(s->imageMs()-109633.32)<.001);QCOMPARE(s->initialMs(),9.);
        const auto parts=s->line(0);QCOMPARE(parts[0].hz,1500.);QCOMPARE(parts[4].hz,1200.);QCOMPARE(parts[1].component,sstv::Component::Green);
        QVERIFY(std::abs(sstv::modeById("martin1")->imageMs()-114290.176)<.001);
        QVERIFY(std::abs(sstv::modeById("pd120")->imageMs()-126103.04)<.001);
        QCOMPARE(sstv::modeById("robot36")->imageMs(),36000.);
        QCOMPARE(sstv::modeById("robot72")->imageMs(),72000.);
        QCOMPARE(sstv::modeById("robot24")->imageMs(),24000.);
        QVERIFY(std::abs(sstv::modeById("bw8")->imageMs()-8028)<.001);
        QCOMPARE(sstv::modeById("bw12")->imageMs(),12000.);
        QVERIFY(std::abs(sstv::modeById("sc260")->imageMs()-61538.4576)<.001);
        QVERIFY(std::abs(sstv::modeById("sc2120")->imageMs()-121733.75488)<.001);
        QCOMPARE(sstv::modeById("avt90")->imageMs(),90000.);
        QCOMPARE(sstv::modeById("avt90")->headerMs(),6222.5);
        const auto pd=sstv::modeById("pd50")->line(0);
        QCOMPARE(pd[3].component,sstv::Component::Cr);QVERIFY(pd[3].averagePair);QCOMPARE(pd[5].rowOffset,1);
    }
    void roundtrip_data() {
        QTest::addColumn<QString>("id");
        for(const auto& m:sstv::modes())if(m.implemented)QTest::newRow(qPrintable(m.id))<<m.id;
    }
    void roundtrip() {
        QFETCH(QString,id);const auto& m=*sstv::modeById(id);const auto image=bars(m.width,m.height);
        const auto audio=sstv::encode(image,m,12000);
        QVERIFY(std::abs(audio.seconds()-(m.imageMs()+m.headerMs()+200)/1000)<1./12000);
        const auto decoded=sstv::decode(audio);QCOMPARE(decoded.modeId,id);QVERIFY(decoded.complete);QCOMPARE(decoded.image.size(),image.size());
        const double error=barError(decoded.image,m.family==sstv::Family::Mono?grayscale(image):image);QVERIFY2(error<7,qPrintable(QString("%1: mean channel error %2").arg(id).arg(error)));
    }
    void automaticHeaderDetection_data() {roundtrip_data();}
    void automaticHeaderDetection() {
        QFETCH(QString,id);const auto& mode=*sstv::modeById(id);
        auto audio=sstv::encode(bars(mode.width,mode.height),mode,12000);
        // AVT must announce its VIS mode before its long countdown finishes.
        const auto headerMs=mode.family==sstv::Family::AVT?910:mode.headerMs();
        audio.samples.resize(size_t((headerMs+60)*12));
        sstv::Receiver receiver;QStringList detected;
        auto notify=[&](const QString& value){detected<<value;};
        const auto split=size_t((headerMs-40)*12);
        sstv::Audio first,second;first.sampleRate=second.sampleRate=12000;
        first.samples.assign(audio.samples.begin(),audio.samples.begin()+split);
        second.samples.assign(audio.samples.begin()+split,audio.samples.end());
        receiver.push(first,{}, {},notify);QVERIFY(detected.isEmpty());
        receiver.push(second,{}, {},notify);
        QCOMPARE(detected,QStringList{id});QCOMPARE(receiver.modeId(),id);
        sstv::Audio empty;empty.sampleRate=12000;receiver.push(empty,{}, {},notify);
        QCOMPARE(detected.size(),1);
    }
    void consecutiveAutomaticModes() {
        sstv::Audio stream;stream.sampleRate=12000;stream.samples.resize(12000*7);
        const QStringList expected={"bw8","robot24","bw12"};
        for(const auto& id:expected) {
            const auto audio=sstv::encode(sstv::testPattern(),*sstv::modeById(id),12000);
            stream.samples.insert(stream.samples.end(),audio.samples.begin(),audio.samples.end());
        }
        sstv::Receiver receiver;QStringList detected,completed;int partials=0;
        // Irregular blocks split headers, rows and frame boundaries.
        for(size_t start=0;start<stream.samples.size();start+=20113) {
            sstv::Audio block;block.sampleRate=12000;
            block.samples.assign(stream.samples.begin()+start,stream.samples.begin()+std::min(start+20113,stream.samples.size()));
            for(const auto& frame:receiver.push(block,{}, {},[&](const QString& id){detected<<id;})) {
                QCOMPARE(frame.frameId,quint64(expected.indexOf(frame.modeId)+1));
                if(frame.complete){completed<<frame.modeId;QCOMPARE(frame.rows,sstv::modeById(frame.modeId)->height);}
                else ++partials;
            }
        }
        QCOMPARE(detected,expected);QCOMPARE(completed,expected);QVERIFY(partials>0);QVERIFY(receiver.modeId().isEmpty());
        sstv::Audio empty;empty.sampleRate=12000;QVERIFY(receiver.push(empty).empty());
    }
    void automaticDetectsClippedOpeningLeader_data(){
        QTest::addColumn<QString>("id");QTest::addColumn<int>("silenceMs");
        for(const auto& mode:sstv::modes())if(mode.implemented){
            QTest::newRow(qPrintable(mode.id+"-late-open"))<<mode.id<<0;
            QTest::newRow(qPrintable(mode.id+"-muted-leader"))<<mode.id<<700;
        }
    }
    void automaticDetectsClippedOpeningLeader(){
        QFETCH(QString,id);QFETCH(int,silenceMs);const auto* mode=sstv::modeById(id);
        const auto source=sstv::encode(bars(mode->width,mode->height),*mode,12000);
        // Preserve the complete VIS word while losing part of the opening tone.
        // N-VIS also retains its 100 ms frequency break and 50 ms of leader.
        const auto trim=size_t((mode->narrow()?250:500)*12);
        const auto end=std::min(source.samples.size(),size_t((mode->family==sstv::Family::AVT?970:mode->headerMs()+80)*12));
        sstv::Audio stream;stream.samples.resize(size_t(silenceMs*12));stream.samples.insert(stream.samples.end(),source.samples.begin()+trim,source.samples.begin()+end);
        sstv::DecodeOptions options;options.autoFrequency=true;sstv::Receiver receiver(options);QStringList detected;
        for(size_t start=0;start<stream.samples.size();start+=4213){
            sstv::Audio block;block.samples.assign(stream.samples.begin()+start,stream.samples.begin()+std::min(start+4213,stream.samples.size()));
            receiver.push(block,{}, {},[&](const QString& value){detected<<value;});
        }
        QCOMPARE(detected,QStringList{id});QCOMPARE(receiver.modeId(),id);
    }
    void clippedLeaderRetainsAutomaticTuning_data(){
        QTest::addColumn<bool>("narrow");QTest::addColumn<double>("offset");QTest::addColumn<int>("silenceMs");
        for(bool narrow:{false,true})for(double offset:{-125.,125.})for(int silence:{0,700})
            QTest::newRow(qPrintable(QString("%1-%2-%3").arg(narrow?"narrow":"standard").arg(offset).arg(silence)))<<narrow<<offset<<silence;
    }
    void clippedLeaderRetainsAutomaticTuning(){
        QFETCH(bool,narrow);QFETCH(double,offset);QFETCH(int,silenceMs);
        const auto source=narrow?referenceNarrow(false,offset):referenceMartin(12000,offset,0,false,8);
        const auto begin=size_t((narrow?123+250:217+500)*12);
        sstv::Audio clipped;clipped.samples.resize(size_t(silenceMs*12));
        clipped.samples.insert(clipped.samples.end(),source.samples.begin()+begin,source.samples.begin()+std::min(source.samples.size(),begin+24000));
        sstv::DecodeOptions options;options.autoFrequency=true;options.autoClock=true;options.allowPartial=true;
        const auto result=sstv::decode(clipped,options);
        QCOMPARE(result.modeId,QString(narrow?"mc140n":"martin2"));QVERIFY(result.frequencyMeasured);
        QVERIFY2(std::abs(result.frequencyOffset-offset)<3,qPrintable(QString::number(result.frequencyOffset)));
        QVERIFY(qGray(result.image.pixel(300,0))<10);
    }
    void clippedLeaderDoesNotBypassHeaderValidation(){
        for(bool narrow:{false,true}){
            auto source=narrow?referenceNarrow(true):referenceMartin(12000,0,0,true,8);
            const auto begin=size_t((narrow?123+250:217+500)*12);
            sstv::Audio clipped;clipped.samples.assign(source.samples.begin()+begin,source.samples.begin()+std::min(source.samples.size(),begin+24000));
            sstv::Receiver receiver;QStringList detected;QVERIFY(receiver.push(clipped,{}, {},[&](const QString&id){detected<<id;}).empty());QVERIFY(detected.empty());
        }
        auto source=referenceMartin(12000,0,0,false,8);
        sstv::Audio late;late.samples.assign(source.samples.begin()+(217+500)*12,source.samples.end());
        // A clipped leader can acquire idle RX but must not replace an active
        // image based on a weaker preamble match inside picture data.
        QVERIFY_EXCEPTION_THROWN(sstv::detectVis(sstv::demodulate(late),0,{},true,true),std::runtime_error);
        for(int trim:{580,680}){
            sstv::Audio missing;missing.samples.assign(source.samples.begin()+(217+trim)*12,source.samples.end());
            sstv::Receiver receiver;QStringList detected;QVERIFY(receiver.push(missing,{}, {},[&](const QString&id){detected<<id;}).empty());QVERIFY(detected.empty());
        }
    }
    void automaticHeaderSurvivesBriefDropout_data(){
        QTest::addColumn<QString>("kind");QTest::addColumn<double>("centerMs");QTest::addColumn<int>("dropoutMs");
        for(int gap:{2,4}){
            QTest::newRow(qPrintable(QString("standard-bit-%1ms").arg(gap)))<<QString("standard")<<872.<<gap;
            QTest::newRow(qPrintable(QString("scottie-bit-%1ms").arg(gap)))<<QString("scottie")<<655.<<gap;
            QTest::newRow(qPrintable(QString("extended-bit-%1ms").arg(gap)))<<QString("extended")<<955.<<gap;
            QTest::newRow(qPrintable(QString("narrow-bit-%1ms").arg(gap)))<<QString("narrow")<<776.<<gap;
        }
        QTest::newRow("leader-4ms")<<QString("standard")<<677.<<4;
        QTest::newRow("stop-4ms")<<QString("standard")<<1112.<<4;
    }
    void automaticHeaderSurvivesBriefDropout(){
        QFETCH(QString,kind);QFETCH(double,centerMs);QFETCH(int,dropoutMs);
        auto audio=kind=="standard"?referenceMartin(12000,0,0,false,8):kind=="narrow"?referenceNarrow():sstv::encode(bars(320,256),*sstv::modeById(kind=="scottie"?"scottie2":"mr73"),12000);
        audio.samples.resize(std::min(audio.samples.size(),size_t(2*12000)));
        const auto begin=size_t((centerMs-dropoutMs*.5)*12),end=size_t((centerMs+dropoutMs*.5)*12);
        std::fill(audio.samples.begin()+begin,audio.samples.begin()+end,0.f);
        sstv::Receiver receiver;QStringList detected;
        const auto frames=receiver.push(audio,{}, {},[&](const QString&id){detected<<id;});
        const auto expected=kind=="standard"?QString("martin2"):kind=="narrow"?QString("mc140n"):kind=="scottie"?QString("scottie2"):QString("mr73");
        QCOMPARE(detected,QStringList{expected});QCOMPARE(receiver.modeId(),expected);
        QVERIFY(!frames.empty());QVERIFY(frames.back().rows>0);
    }
    void briefDropoutDoesNotRepairInvalidParity(){
        auto audio=referenceMartin(12000,0,0,true,8);
        std::fill(audio.samples.begin()+10440,audio.samples.begin()+10488,0.f);
        sstv::Receiver receiver;QStringList detected;
        QVERIFY(receiver.push(audio,{}, {},[&](const QString&id){detected<<id;}).empty());
        QVERIFY(detected.empty());
    }
    void consecutiveManualFrames_data() {
        QTest::addColumn<bool>("manualStart");QTest::newRow("header-timed")<<false;QTest::newRow("manual-first-start")<<true;
    }
    void completedClockDriftPreservesSplitNextHeader(){
        auto first=referenceMartin(12000,0,-8000);
        const auto next=sstv::encode(sstv::testPattern(),*sstv::modeById("robot36"),12000);
        first.samples.insert(first.samples.end(),next.samples.begin(),next.samples.begin()+6000);
        sstv::Receiver receiver;const auto frames=receiver.push(first);
        QVERIFY(std::any_of(frames.begin(),frames.end(),[](const auto& frame){return frame.complete&&frame.modeId=="martin2";}));
        sstv::Audio rest;rest.samples.assign(next.samples.begin()+6000,next.samples.end());
        const auto second=receiver.push(rest);
        QVERIFY2(std::any_of(second.begin(),second.end(),[](const auto& frame){return frame.complete&&frame.frameId==2&&frame.modeId=="robot36";}),"Nominal picture end discarded the next header after sync tracking shortened the first image");
    }
    void interruptedPictureDoesNotConsumeNextHeader_data(){
        QTest::addColumn<QString>("nextMode");QTest::addColumn<bool>("chunked");QTest::addColumn<bool>("corrections");
        QTest::newRow("same-mode-block")<<QString("robot36")<<false<<false;
        QTest::newRow("same-mode-split-header-afc-clock")<<QString("robot36")<<true<<true;
        QTest::newRow("different-mode")<<QString("bw8")<<true<<false;
        QTest::newRow("narrow-mode")<<QString("mp73n")<<false<<false;
    }
    void interruptedPictureDoesNotConsumeNextHeader(){
        QFETCH(QString,nextMode);QFETCH(bool,chunked);QFETCH(bool,corrections);
        const auto* mode=sstv::modeById("robot36");
        auto first=sstv::encode(bars(320,240),*mode,12000);
        first.samples.resize(size_t((mode->headerMs()+mode->imageMs()-1500)*12));
        sstv::DecodeOptions options;options.autoClock=options.autoFrequency=corrections;
        sstv::Receiver receiver(options);const auto partial=receiver.push(first);
        QCOMPARE(partial.size(),size_t(1));QVERIFY(!partial[0].complete);
        const auto second=sstv::encode(sstv::testPattern(),*sstv::modeById(nextMode),12000);
        std::vector<sstv::DecodeResult> frames;
        const size_t step=chunked?9973:second.samples.size();
        for(size_t pos=0;pos<second.samples.size();pos+=step){
            sstv::Audio block;block.samples.assign(second.samples.begin()+pos,second.samples.begin()+std::min(pos+step,second.samples.size()));
            const auto updates=receiver.push(block);frames.insert(frames.end(),updates.begin(),updates.end());
        }
        const auto complete=std::find_if(frames.begin(),frames.end(),[](const auto& frame){return frame.complete&&frame.frameId==2;});
        QVERIFY2(complete!=frames.end(),"The next header was consumed as the end of the previous image");QCOMPARE(complete->modeId,nextMode);
        QVERIFY(receiver.modeId().isEmpty());
        QVERIFY(std::none_of(frames.begin(),frames.end(),[](const auto& frame){return frame.complete&&frame.frameId==1;}));
    }
    void consecutiveManualFrames() {
        QFETCH(bool,manualStart);
        const auto picture=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);
        auto stream=picture;stream.samples.insert(stream.samples.end(),picture.samples.begin(),picture.samples.end());
        sstv::DecodeOptions options;options.forceMode="bw8";if(manualStart)options.manualStartMs=910;
        sstv::Receiver receiver(options);std::vector<quint64> completed;int partials=0;
        for(size_t start=0;start<stream.samples.size();start+=24000) {
            sstv::Audio block;block.samples.assign(stream.samples.begin()+start,stream.samples.begin()+std::min(start+24000,stream.samples.size()));
            for(const auto& frame:receiver.push(block)) {
                QCOMPARE(frame.modeId,QString("bw8"));
                if(frame.complete)completed.push_back(frame.frameId);else ++partials;
            }
        }
        QCOMPARE(completed,std::vector<quint64>({1,2}));QVERIFY(partials>0);
    }
    void automaticRejectsInvalidHeaders() {
        sstv::Receiver receiver;QStringList detected;
        auto notify=[&](const QString& id){detected<<id;};
        sstv::Audio silence;silence.sampleRate=12000;silence.samples.resize(20*12000);
        QVERIFY(receiver.push(silence,{}, {},notify).empty());
        auto bad=referenceMartin(12000,0,0,true,2);
        QVERIFY(receiver.push(bad,{}, {},notify).empty());QVERIFY(detected.isEmpty());
        auto good=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);
        // A valid but unsupported VIS must not hide a following supported frame.
        auto unknown=referenceMartin(12000,0,0,false,2,127);
        unknown.samples.insert(unknown.samples.end(),good.samples.begin(),good.samples.end());
        good=std::move(unknown);
        const auto frames=receiver.push(good,{}, {},notify);
        QCOMPARE(detected,QStringList{"bw8"});QCOMPARE(frames.size(),size_t(1));QVERIFY(frames[0].complete);
        auto cancel=std::make_shared<std::atomic_bool>(true);
        QVERIFY_EXCEPTION_THROWN(receiver.push(good,cancel),std::runtime_error);
    }
    void automaticRecoversAfterInvalidAvtCountdown_data() {
        QTest::addColumn<double>("offset");QTest::newRow("normal")<<0.;QTest::newRow("afc")<<125.;
    }
    void automaticRecoversAfterInvalidAvtCountdown() {
        QFETCH(double,offset);
        auto invalid=referenceAvt(AvtFixture{.corruptWords=0xffffffff,.offset=offset,.rows=4});
        invalid.samples.resize(size_t((.083+.910+5.3125)*12000));
        sstv::DecodeOptions options;options.autoFrequency=offset!=0;sstv::Receiver receiver(options);QStringList detected;
        auto notify=[&](const QString& id){detected<<id;};
        receiver.push(invalid,{}, {},notify);QCOMPARE(detected,QStringList{"avt90"});
        sstv::Audio silence;silence.samples.resize(19*12000);receiver.push(silence,{}, {},notify);
        const auto good=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);
        const auto frames=receiver.push(good,{}, {},notify);
        QCOMPARE(detected,QStringList({"avt90","bw8"}));QCOMPARE(frames.size(),size_t(1));QVERIFY(frames[0].complete);
    }
    void independentReference_data() {
        QTest::addColumn<int>("rate");QTest::addColumn<double>("offset");QTest::addColumn<double>("ppm");
        QTest::newRow("12000")<<12000<<0.<<0.;
        QTest::newRow("48000-offset-clock")<<48000<<42.<<1500.;
        QTest::newRow("44100-negative-clock")<<44100<<-35.<<-1200.;
    }
    void legacyReference_data() {
        QTest::addColumn<QString>("id");
        for(const auto& id:{"bw8","bw12","robot24","sc260","sc2120"})QTest::newRow(id)<<QString(id);
    }
    void legacyReference() {
        QFETCH(QString,id);const auto& mode=*sstv::modeById(id);
        const auto result=sstv::decode(referenceLegacy(id));
        QCOMPARE(result.modeId,id);QVERIFY(result.complete);QVERIFY(std::abs(result.startMs-1047)<1);
        const auto expected=bars(mode.width,mode.height);
        QVERIFY(barError(result.image,mode.family==sstv::Family::Mono?grayscale(expected):expected)<7);
    }
    void avtReference_data() {
        QTest::addColumn<int>("marker");QTest::addColumn<bool>("descending");QTest::addColumn<int>("rate");QTest::addColumn<double>("ppm");
        QTest::newRow("published-ascending")<<5<<false<<12000<<0.;
        QTest::newRow("legacy-marker-descending")<<2<<true<<12000<<0.;
        QTest::newRow("48000-clock-error")<<5<<false<<48000<<1200.;
    }
    void avtReference() {
        QFETCH(int,marker);QFETCH(bool,descending);QFETCH(int,rate);QFETCH(double,ppm);
        AvtFixture config;config.marker=marker;config.descending=descending;config.sampleRate=rate;config.ppm=ppm;
        config.offset=25;config.attention=1.25;config.corruptWords=0b10101;
        auto audio=referenceAvt(config);QRandomGenerator random(819);
        for(auto& sample:audio.samples)sample+=float((random.generateDouble()-.5)*.015);
        sstv::DecodeOptions options;options.frequencyOffset=config.offset;options.clockPpm=ppm;
        const auto result=sstv::decode(audio,options);
        QCOMPARE(result.modeId,QString("avt90"));QVERIFY(result.complete);
        QVERIFY2(std::abs(result.startMs-(83+(6222.5+1250)*(1+ppm*1e-6)))<1,qPrintable(QString::number(result.startMs)));
        QVERIFY(barError(result.image,bars(320,240))<7);
    }
    void avtHeaderRecoveryAndRejection() {
        const auto complete=referenceAvt();auto late=complete;
        const auto cut=size_t(std::llround((.083+.910+8*17*20./2048)*12000));
        late.samples.erase(late.samples.begin(),late.samples.begin()+qptrdiff(cut));
        sstv::DecodeOptions options;options.forceMode="avt90";
        auto result=sstv::decode(late,options);QVERIFY(result.complete);QVERIFY(barError(result.image,bars(320,240))<7);
        AvtFixture corrupt;corrupt.corruptWords=0xffffffff;corrupt.rows=4;
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceAvt(corrupt)),std::runtime_error);
        AvtFixture wrongMode;wrongMode.marker=3;wrongMode.rows=4;
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceAvt(wrongMode)),std::runtime_error);
        auto noControl=complete;noControl.samples.erase(noControl.samples.begin(),noControl.samples.begin()+size_t((.083+6.2225)*12000));
        options.manualStartMs=0;result=sstv::decode(noControl,options);QVERIFY(result.complete);
        AvtFixture partial;partial.rows=64;
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceAvt(partial)),std::runtime_error);
        options.manualStartMs=-1;options.allowPartial=true;result=sstv::decode(referenceAvt(partial),options);QCOMPARE(result.rows,64);QVERIFY(!result.complete);
    }
    void legacyToneContracts() {
        const auto mono=sstv::fitImage(bars(160,120),*sstv::modeById("bw8"));
        for(int x=0;x<160;++x){const auto p=mono.pixel(x,60);QCOMPARE(qRed(p),qGreen(p));QCOMPARE(qGreen(p),qBlue(p));}
        const auto audio=sstv::encode(bars(160,120),*sstv::modeById("bw8"),12000);
        QVERIFY(std::abs(toneFrequency(audio,911,916)-1200)<3);
        QVERIFY(std::abs(toneFrequency(audio,918,923)-2300)<3);
        QVERIFY(std::abs(toneFrequency(audio,971,975)-1500)<3);
        const auto avt=sstv::encode(bars(320,240),*sstv::modeById("avt90"),12000);
        constexpr double bit=1000./102.4;
        QVERIFY(std::abs(toneFrequency(avt,912,917)-1900)<3);
        for(int b=0;b<16;++b) {
            const double start=910+(b+1)*bit;
            const int expected=(0xa05f>>(15-b))&1?2200:1600;
            QVERIFY(std::abs(toneFrequency(avt,start+2,start+7)-expected)<4);
        }
    }
    void independentReference() {
        QFETCH(int,rate);QFETCH(double,offset);QFETCH(double,ppm);
        auto audio=referenceMartin(rate,offset,ppm);
        QRandomGenerator random(4621);
        for(auto& sample:audio.samples)sample=float(sample*.5+(random.generateDouble()-.5)*.025);
        sstv::DecodeOptions options;options.frequencyOffset=offset;options.clockPpm=ppm;
        const auto result=sstv::decode(audio,options);
        QVERIFY(result.complete);QCOMPARE(result.modeId,QString("martin2"));
        QVERIFY(std::abs(result.startMs-(217+910*(1+ppm*1e-6)))<2);
        QVERIFY(barError(result.image,bars(320,256))<10);
    }
    void automaticCorrections_data() {
        QTest::addColumn<double>("offset");QTest::addColumn<double>("ppm");
        QTest::newRow("positive")<<125.<<4500.;
        QTest::newRow("negative")<<-140.<<-3500.;
        QTest::newRow("clean")<<0.<<0.;
    }
    void automaticCorrections() {
        QFETCH(double,offset);QFETCH(double,ppm);
        const auto audio=referenceMartin(12000,offset,ppm);
        sstv::DecodeOptions options;options.autoClock=true;options.autoFrequency=true;
        const auto result=sstv::decode(audio,options);
        QVERIFY(result.complete);QVERIFY(result.clockMeasured);QVERIFY(result.frequencyMeasured);
        QVERIFY2(std::abs(result.clockPpm-ppm)<30,qPrintable(QString::number(result.clockPpm)));
        QVERIFY(std::abs(result.frequencyOffset-offset)<2);
        QVERIFY2(barError(result.image,bars(320,256))<10,qPrintable(QString::number(barError(result.image,bars(320,256)))));
    }
    void automaticCorrectionsPartialAndNextFrame() {
        auto first=referenceMartin(12000,105,2500);const auto second=referenceMartin(12000,-90,-1500);
        first.samples.insert(first.samples.end(),second.samples.begin(),second.samples.end());
        sstv::DecodeOptions options;options.autoClock=true;options.autoFrequency=true;
        sstv::Receiver receiver(options);std::vector<sstv::DecodeResult> finished;
        bool partialMeasured=false;
        for(size_t begin=0;begin<first.samples.size();begin+=12000*5) {
            sstv::Audio block;block.sampleRate=12000;
            const auto end=std::min(first.samples.size(),begin+12000*5);block.samples.assign(first.samples.begin()+begin,first.samples.begin()+end);
            for(const auto& frame:receiver.push(block)) {
                if(frame.complete)finished.push_back(frame);
                else if(frame.clockMeasured && frame.frequencyMeasured)partialMeasured=true;
            }
        }
        QVERIFY(partialMeasured);QCOMPARE(finished.size(),size_t(2));
        for(const auto& frame:finished)QVERIFY(barError(frame.image,bars(320,256))<10);
        QVERIFY(std::abs(finished[0].frequencyOffset-105)<2);QVERIFY(std::abs(finished[1].frequencyOffset+90)<2);
    }
    void automaticCorrectionNoiseAndMissingSync() {
        auto audio=referenceMartin(12000,95,4000);
        QRandomGenerator noise(42);
        for(auto& sample:audio.samples)sample+=float((noise.generateDouble()-.5)*.035);
        // Lose isolated sync pulses; retain the data and the other line references.
        for(int line=10;line<240;line+=13) {
            const int begin=int((.217+(.910+line*.226798)*1.004)*12000);
            for(int i=begin;i<begin+60 && i<int(audio.samples.size());++i)audio.samples[size_t(i)]=0;
        }
        sstv::DecodeOptions options;options.autoClock=true;options.autoFrequency=true;
        const auto frame=sstv::decode(audio,options);
        QVERIFY(frame.clockMeasured);QVERIFY(frame.frequencyMeasured);
        QVERIFY(std::abs(frame.clockPpm-4000)<40);QVERIFY(barError(frame.image,bars(320,256))<12);
    }
    void automaticCorrectionAllModes_data() {
        QTest::addColumn<QString>("id");
        for(const auto& mode:sstv::modes())QTest::newRow(qPrintable(mode.id))<<mode.id;
    }
    void automaticCorrectionAllModes() {
        QFETCH(QString,id);const auto* mode=sstv::modeById(id);
        sstv::DecodeOptions options;options.autoClock=true;options.autoFrequency=true;
        const auto frame=sstv::decode(sstv::encode(sstv::testPattern(),*mode,12000),options);
        QVERIFY(frame.complete);QCOMPARE(frame.modeId,id);
        if(mode->family!=sstv::Family::AVT) {
            QVERIFY2(frame.clockMeasured,qPrintable(id));QVERIFY(std::abs(frame.clockPpm)<40);
            QVERIFY2(frame.frequencyMeasured,qPrintable(id));
        }
        else QVERIFY(!frame.clockMeasured);
    }
    void automaticCorrectionsNeedEvidence() {
        sstv::DecodeOptions options;options.autoClock=true;options.autoFrequency=true;options.allowPartial=true;
        const auto partial=sstv::decode(referenceMartin(12000,0,0,false,2),options);
        QVERIFY(!partial.clockMeasured);QCOMPARE(partial.clockPpm,0.);
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceMartin(12000,125,0,true,30),options),std::runtime_error);
        options.forceMode="martin2";options.manualStartMs=1127;
        sstv::Audio silence;silence.sampleRate=12000;silence.samples.resize(12000*10);
        const auto blank=sstv::decode(silence,options);QVERIFY(!blank.clockMeasured);QVERIFY(!blank.frequencyMeasured);
        auto cancel=std::make_shared<std::atomic_bool>(true);
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceMartin(12000,0,0,false,15),options,cancel),std::runtime_error);
    }
    void invalidVis() {
        const auto bad=referenceMartin(12000,0,0,true,2);
        QVERIFY_EXCEPTION_THROWN(sstv::decode(bad),std::runtime_error);
        sstv::Audio silence;silence.samples.resize(24000);QVERIFY_EXCEPTION_THROWN(sstv::decode(silence),std::runtime_error);
    }
    void extendedAndNarrowContracts() {
        const auto& mr=*sstv::modeById("mr175");
        QCOMPARE(mr.vis,0x4c23);QCOMPARE(mr.headerMs(),1150.);
        const auto audio=sstv::encode(bars(320,256),mr,12000);
        for(int bit=0;bit<16;++bit) {
            const double hz=toneFrequency(audio,640+30*bit+5,640+30*bit+25);
            QVERIFY(std::abs(hz-(((0x4c23>>bit)&1)?1100:1300))<3);
        }
        const auto header=sstv::detectVis(sstv::demodulate(audio));
        QCOMPARE(header.code,0x4c23);QVERIFY(!header.narrow);QVERIFY(std::abs(header.imageStartMs-1150)<1);
        auto narrow=referenceNarrow();auto result=sstv::decode(narrow);
        QCOMPARE(result.modeId,QString("mc140n"));QVERIFY(result.complete);QVERIFY(barError(result.image,bars(320,256))<7);
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceNarrow(true)),std::runtime_error);
    }
    void rowGeometry() {
        for(const auto& id:{"robot24","robot36","pd50","mp73","mr73","ml180","mp73n","mc110n","avt90","bw8","bw12","sc260","sc2120"}) {
            const auto& mode=*sstv::modeById(id);QImage image(mode.width,mode.height,QImage::Format_RGB32);
            for(int y=0;y<mode.height;++y)for(int x=0;x<mode.width;++x) {
                const int v=(y%2?210:40);image.setPixel(x,y,qRgb(v,v,v));
            }
            const auto result=sstv::decode(sstv::encode(image,mode,12000));
            for(int y=10;y<mode.height-10;++y) {
                const int value=qRed(result.image.pixel(mode.width/2,y));
                QVERIFY2(std::abs(value-(y%2?210:40))<8,id);
            }
        }
    }
    void partialAndManual() {
        auto audio=referenceMartin(12000,0,0,false,64);
        QVERIFY_EXCEPTION_THROWN(sstv::decode(audio),std::runtime_error);
        sstv::DecodeOptions o;o.allowPartial=true;auto partial=sstv::decode(audio,o);QVERIFY(!partial.complete);QCOMPARE(partial.rows,64);
        const size_t start=size_t((.217+.910)*12000);
        audio.samples.erase(audio.samples.begin(),audio.samples.begin()+qptrdiff(start));
        o.forceMode="martin2";o.manualStartMs=0;partial=sstv::decode(audio,o);QCOMPARE(partial.rows,64);
    }
    void invalidInputsAndCancel() {
        auto cancel=std::make_shared<std::atomic_bool>(true);
        QVERIFY_EXCEPTION_THROWN(sstv::encode(bars(320,256),*sstv::modeById("martin1"),12000,cancel),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(sstv::encode(bars(320,256),*sstv::modeById("avt90"),100),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(sstv::encode(QImage{},*sstv::modeById("bw8")),std::runtime_error);
        sstv::DecodeOptions o;o.forceMode="unknown-mode";
        QVERIFY_EXCEPTION_THROWN(sstv::decode(referenceMartin(12000,0,0,false,1),o),std::runtime_error);
    }
    void wavIO() {
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("audio.wav");
        auto audio=referenceMartin(12000,0,0,false,1);sstv::writeWav(path,audio);const auto read=sstv::readWav(path);
        QCOMPARE(read.sampleRate,12000);QCOMPARE(read.samples.size(),audio.samples.size());QVERIFY(std::abs(read.samples[15000]-audio.samples[15000])<.0001);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();file.close();
        auto junk=QByteArray("JUNK\3\0\0\0abc\0",12);bytes.insert(12,junk);qToLittleEndian(quint32(bytes.size()-8),bytes.data()+4);
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();QCOMPARE(sstv::readWav(path).samples.size(),audio.samples.size());
        bytes.chop(5);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();QVERIFY_EXCEPTION_THROWN(sstv::readWav(path),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(sstv::readWav(dir.filePath("missing.wav")),std::runtime_error);
    }
    void stereoWavChannels() {
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("stereo.wav");
        QByteArray bytes("RIFF");auto add16=[&](quint16 n){char p[2];qToLittleEndian(n,p);bytes.append(p,2);};
        auto add32=[&](quint32 n){char p[4];qToLittleEndian(n,p);bytes.append(p,4);};
        add32(44);bytes+="WAVEfmt ";add32(16);add16(1);add16(2);add32(48000);add32(192000);add16(4);add16(16);bytes+="data";add32(8);
        add16(8192);add16(quint16(qint16(-8192)));add16(16384);add16(0);
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();
        const auto left=sstv::readWav(path,0),right=sstv::readWav(path,1),mix=sstv::readWav(path,2);
        QCOMPARE(left.sampleRate,48000);QCOMPARE(left.samples.size(),size_t(2));
        QCOMPARE(left.samples[0],.25f);QCOMPARE(right.samples[0],-.25f);QCOMPARE(mix.samples[0],0.f);QCOMPARE(mix.samples[1],.25f);
    }
};
QTEST_MAIN(CodecTests)
#include "codec_tests.moc"
