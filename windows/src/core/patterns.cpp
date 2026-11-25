#include "patterns.h"
#include "codec.h"
#include <QPainter>
#include <QPainterPath>
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sstv {
const QList<TestPattern>& testPatterns(){
    static const QList<TestPattern> patterns={
        {"test-pattern","Test pattern","Tux-sstv original","Full-color bars, a gray ramp and a checkerboard. The original Tux-sstv test image."},
        {"smpte","SMPTE-style bars","Analog color television","The familiar American three-band color-bar layout. Compare colors and dark detail between stations."},
        {"ebu","EBU 75% bars","European color television","Full-height European 100/0/75/0 bars: full white, 75% colors and black. Compare color balance and brightness."},
        {"pm5544","PM5544-style card","Classic color television","An original drawing inspired by the Philips circle card: color bars, gray steps, a grid and fine-detail blocks. Check shape and color together."},
        {"monoscope","Monoscope-style card","Early monochrome television","An original early-TV-style circle chart with resolution wedges, gray steps and corner targets. Compare geometry and fine detail.",true},
        {"grayscale","Grayscale steps","Monochrome and color television","Eleven gray steps, a smooth ramp and a reverse staircase. Look for missing shadow or highlight detail and unwanted color tints.",true},
        {"crosshatch","Crosshatch","Television alignment","A square grid and center target. Straight lines help reveal slant, stretching and image alignment problems.",true},
        {"multiburst","Multiburst","Broadcast and service testing","Six groups of increasingly fine stripes. Compare how much detail survives the selected SSTV mode and radio link. Labels are spatial cycles, not MHz.",true},
        {"siemens","Siemens star","Optical and television focus tests","Alternating radial wedges converge at the center. Compare sharpness, blur and compression between modes.",true},
        {"checkerboard","Checkerboard","Television and monitor testing","Alternating black and white squares. Look for edge ringing, blur, uneven shading and lost fine detail.",true}
    };return patterns;
}
const TestPattern* testPatternById(const QString& id){for(const auto& pattern:testPatterns())if(pattern.id==id)return &pattern;return nullptr;}
namespace {
constexpr int width=768,height=576;
const std::array<QColor,8> bars={QColor(191,191,191),QColor(191,191,0),QColor(0,191,191),QColor(0,191,0),QColor(191,0,191),QColor(191,0,0),QColor(0,0,191),QColor(0,0,0)};
void colorBars(QPainter& p,QRect area,bool white100=false,int count=8){for(int i=0;i<count;++i){const int left=area.left()+area.width()*i/count,right=area.left()+area.width()*(i+1)/count;p.fillRect(QRect(left,area.top(),right-left,area.height()),i==0 && white100?QColor(Qt::white):bars[size_t(i)]);}}
void steps(QPainter& p,QRect area,int count=11,bool reverse=false){for(int i=0;i<count;++i){const int value=255*(reverse?count-1-i:i)/(count-1),left=area.left()+area.width()*i/count,right=area.left()+area.width()*(i+1)/count;p.fillRect(QRect(left,area.top(),right-left,area.height()),QColor(value,value,value));}}
void label(QPainter& p,QRect box,const QString& text,int size=20,QColor ink=Qt::black){QFont font("DejaVu Sans");font.setPixelSize(size);font.setBold(true);p.setFont(font);p.setPen(ink);p.drawText(box,Qt::AlignCenter,text);}
void grid(QPainter& p,int pitch,QColor color){p.setPen(QPen(color,1));for(int x=0;x<width;x+=pitch)p.drawLine(x,0,x,height);for(int y=0;y<height;y+=pitch)p.drawLine(0,y,width,y);}
void checker(QPainter& p,QRect area,int pitch){for(int y=area.top();y<area.bottom()+1;y+=pitch)for(int x=area.left();x<area.right()+1;x+=pitch)p.fillRect(QRect(x,y,std::min(pitch,area.right()+1-x),std::min(pitch,area.bottom()+1-y)),((x-area.left())/pitch+(y-area.top())/pitch)%2?Qt::black:Qt::white);}
void fan(QPainter& p,QPointF origin,double angle){p.save();p.translate(origin);p.rotate(angle);p.setPen(Qt::NoPen);p.setBrush(Qt::black);for(int n=0;n<12;n+=2){const double a=-60+n*10,b=a+10;p.drawPolygon(QPolygonF{{0,0},{a,-150},{b,-150}});}p.restore();}
void pmCard(QPainter& p){
    p.fillRect(QRect(0,0,width,height),QColor(122,122,122));grid(p,48,QColor(210,210,210));
    checker(p,{0,0,width,24},24);checker(p,{0,height-24,width,24},24);checker(p,{0,24,24,height-48},24);checker(p,{width-24,24,24,height-48},24);
    const QRectF circle(140,44,488,488);QPainterPath clip;clip.addEllipse(circle);p.save();p.setClipPath(clip);p.fillRect(circle,Qt::white);
    checker(p,{140,44,488,72},12);steps(p,{140,116,488,60});colorBars(p,{140,176,488,112},true);
    p.fillRect(QRect(140,288,488,88),Qt::white);
    for(int block=0;block<6;++block){const int x=140+block*82,pitch=std::max(2,12-block*2);for(int line=x;line<x+80;line+=pitch*2)p.fillRect(QRect(line,296,pitch,72),Qt::black);}
    p.fillRect(QRect(140,376,488,68),Qt::black);label(p,{150,380,468,60},"OPEN SSTV",32,Qt::white);steps(p,{140,444,488,60},11,true);checker(p,{140,504,488,28},8);p.restore();
    p.setPen(QPen(Qt::black,4));p.setBrush(Qt::NoBrush);p.drawEllipse(circle);p.setPen(QPen(Qt::white,2));p.drawLine(24,288,140,288);p.drawLine(628,288,744,288);p.drawLine(384,24,384,44);p.drawLine(384,532,384,552);
}
void monoscope(QPainter& p){
    p.fillRect(QRect(0,0,width,height),Qt::white);grid(p,48,QColor(205,205,205));
    p.setBrush(Qt::NoBrush);p.setPen(QPen(Qt::black,5));p.drawRect(QRectF(12,12,744,552));p.drawEllipse(QPointF(384,288),232,232);p.setPen(QPen(Qt::black,2));p.drawEllipse(QPointF(384,288),220,220);
    for(const auto& point:QList<QPointF>{{80,80},{688,80},{80,496},{688,496}}){p.setPen(QPen(Qt::black,2));p.drawEllipse(point,42,42);p.drawEllipse(point,30,30);p.drawLine(point+QPointF(-47,0),point+QPointF(47,0));p.drawLine(point+QPointF(0,-47),point+QPointF(0,47));}
    fan(p,{384,256},0);fan(p,{384,320},180);fan(p,{350,288},270);fan(p,{418,288},90);
    p.fillRect(QRect(320,250,128,76),Qt::white);p.setPen(QPen(Qt::black,2));p.drawRect(QRectF(320,250,128,76));label(p,{322,253,124,32},"OPEN",24);label(p,{322,286,124,36},"SSTV",24);
    steps(p,{258,442,252,26},11);label(p,{230,16,308,30},"TELEVISION TEST",19);label(p,{252,530,264,30},"MONOSCOPE",19);
}
void multiburst(QPainter& p){
    p.fillRect(QRect(0,0,width,height),Qt::white);const std::array<int,6> cycles={4,8,12,16,24,32};
    QImage band(width,400,QImage::Format_RGB32);
    for(int y=0;y<400;++y){auto* line=reinterpret_cast<QRgb*>(band.scanLine(y));for(int x=0;x<width;++x){const int block=x/128;const double phase=y<232?(x%128)/128.:((y-252)/148.);const int value=y>=232 && y<252?255:int(std::lround(127.5+127.5*std::cos(2*std::numbers::pi*cycles[size_t(block)]*phase)));line[x]=qRgb(value,value,value);}}
    p.drawImage(0,96,band);label(p,{0,18,width,44},"MULTIBURST / DETAIL",26);
    for(int i=0;i<6;++i){label(p,{i*128,66,128,28},QString::number(cycles[size_t(i)]),20);p.setPen(QPen(Qt::black,1));p.drawRect(QRect(i*128,96,128,400));}label(p,{0,514,width,40},"CYCLES / BLOCK",19);
}
}
QImage renderTestPattern(const QString& id){
    if(id=="test-pattern")return testPattern();
    if(!testPatternById(id))return {};
    QImage image(width,height,QImage::Format_RGB32);image.fill(Qt::black);QPainter p(&image);
    if(id=="smpte"){
        colorBars(p,{0,0,width,384},false,7);
        const std::array<QColor,7> reverse={bars[6],Qt::black,bars[4],Qt::black,bars[2],Qt::black,bars[0]};
        for(int i=0;i<7;++i)p.fillRect(QRect(width*i/7,384,width*(i+1)/7-width*i/7,48),reverse[size_t(i)]);
        // Full-range RGB adaptation: superblack cannot exist in an SSTV image.
        p.fillRect(QRect(0,432,137,144),QColor(0,68,130));p.fillRect(QRect(137,432,137,144),Qt::white);p.fillRect(QRect(274,432,137,144),QColor(67,0,130));
        p.fillRect(QRect(576,432,32,144),QColor(0,0,0));p.fillRect(QRect(608,432,32,144),QColor(8,8,8));p.fillRect(QRect(640,432,32,144),QColor(20,20,20));
    }else if(id=="ebu")colorBars(p,{0,0,width,height},true);
    else if(id=="pm5544")pmCard(p);
    else if(id=="monoscope")monoscope(p);
    else if(id=="grayscale"){
        steps(p,{0,0,width,288});QLinearGradient ramp(0,0,width-1,0);ramp.setColorAt(0,Qt::black);ramp.setColorAt(1,Qt::white);p.fillRect(QRect(0,288,width,144),ramp);steps(p,{0,432,width,144},11,true);
    }else if(id=="crosshatch"){
        grid(p,48,Qt::white);p.setPen(QPen(Qt::white,3));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(1,1,width-3,height-3));p.drawEllipse(QPointF(384,288),144,144);p.drawLine(360,288,408,288);p.drawLine(384,264,384,312);
    }else if(id=="multiburst")multiburst(p);
    else if(id=="siemens"){
        p.fillRect(QRect(0,0,width,height),QColor(225,225,225));p.setPen(Qt::NoPen);
        for(int sector=0;sector<72;++sector){p.setBrush(sector%2?Qt::black:Qt::white);p.drawPie(QRectF(144,48,480,480),sector*80,80);}
        p.setBrush(QColor(128,128,128));p.drawEllipse(QPointF(384,288),5,5);p.setPen(QPen(Qt::black,2));p.setBrush(Qt::NoBrush);p.drawEllipse(QRectF(144,48,480,480));
    }else if(id=="checkerboard")checker(p,{0,0,width,height},32);
    return image;
}
}
