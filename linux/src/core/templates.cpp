#include "templates.h"
#include "exchangetemplates.h"
#include "televisiontemplates.h"
#include "templatetext.h"
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sstv {
const QList<ImageTemplate>& imageTemplates(){
    static const QList<ImageTemplate> designs={
        {"weather","Weather Station","Midnight blue, bright cyan and a bold weather display."},
        {"sunset","Sunset Ridge","Warm sunset colors and layered mountain silhouettes."},
        {"aurora","Aurora","Violet skies, flowing green lights and luminous accents."},
        {"postcard","Paper QSL","Cream paper, navy lettering and a bright orange stamp."},
        {"cq","CQ SSTV","Invite someone to swap pictures. Large CQ text and your callsign.",false},
        {"reply","Back to You","Call the other station and keep the picture exchange going.",false},
        {"report","Picture Report","Send the other station a report on their picture.",false},
        {"thanks","Thanks!","Thank someone for the picture they just sent.",false},
        {"your-turn","Your Turn","Pass it back: your turn to send a picture!",false},
        {"73","See You Again","Finish the exchange with a big 73 and your own message.",false},
        {"photo","Picture Swap","Put your own photo first, with callsigns and a short message.",false},
        {"newsroom","Newsroom","Your own TV news desk: a headline, story details and a news ticker.",false,true},
        {"breaking-news","Breaking News","A bold red bulletin with a big headline and an update ticker.",false,true},
        {"scoreboard","Sports Scoreboard","A stadium-style score graphic with team names, scores and a ticker.",false,true},
        {"now-showing","Now Showing","A cinema marquee for a movie, program or your next SSTV show.",false,true},
        {"on-air","On Air","A studio graphic for your show, guest, topic or shack broadcast.",false,true}
    };return designs;
}
const ImageTemplate* imageTemplateById(const QString& id){
    const auto& designs=imageTemplates();const auto found=std::find_if(designs.begin(),designs.end(),[&](const auto& design){return design.id==id;});return found==designs.end()?nullptr:&*found;
}
QString TemplateFields::validate() const{
    const auto* selected=imageTemplateById(design);if(!selected)return "Choose a template design.";
    if(unit!="F" && unit!="C")return "Choose °F or °C.";
    if(!QStringList{"mph","km/h","kn","m/s"}.contains(windUnit))return "Choose a wind speed unit.";
    if(rainUnit!="in"&&rainUnit!="mm")return "Choose inches or millimeters for rainfall.";
    if(pressureUnit!="inHg"&&pressureUnit!="hPa")return "Choose inHg or hPa (mb) for barometric pressure.";
    if(selected->weatherCard && !temperature.trimmed().isEmpty()){
        bool ok=false;const double value=temperature.toDouble(&ok);
        if(!ok || !std::isfinite(value) || value<(unit=="C"?-150:-238) || value>(unit=="C"?150:302))return "Enter a temperature between −150 and 150 °C (−238 and 302 °F), or leave it blank.";
    }
    if(selected->weatherCard){
        const auto valid=[](const QString& text,double minimum,double maximum,bool positive=false){
            if(text.trimmed().isEmpty())return true;
            bool ok=false;const double value=text.toDouble(&ok);
            return text.size()<=12&&ok&&std::isfinite(value)&&value>=minimum&&value<=maximum&&(!positive||value>0);
        };
        if(!valid(windSpeed,0,1000000))return "Enter a nonnegative wind speed (up to 1,000,000), or leave it blank.";
        if(!valid(rainfall,0,1000000))return "Enter nonnegative rainfall (up to 1,000,000), or leave it blank.";
        if(!valid(humidity,0,100))return "Enter humidity from 0 to 100%, or leave it blank.";
        if(!valid(pressure,0,1000000,true))return "Enter positive barometric pressure (up to 1,000,000), or leave it blank.";
    }
    for(const auto& field:{city,state,weather,message,toCall,report,headline,subheadline,ticker,programInfo,homeTeam,awayTeam,homeScore,awayScore})if(field.size()>128 || field.contains('\n') || field.contains('\r'))return "Keep each template field to a single line of 128 characters or fewer.";
    return {};
}
void TemplateFields::useCurrentTime(){const auto now=utc?QDateTime::currentDateTimeUtc():QDateTime::currentDateTime();date=now.date();time=now.time();}
void TemplateFields::clearFields(){city.clear();state.clear();weather.clear();temperature.clear();windSpeed.clear();rainfall.clear();humidity.clear();pressure.clear();headline.clear();subheadline.clear();ticker.clear();programInfo.clear();homeTeam.clear();awayTeam.clear();homeScore.clear();awayScore.clear();message.clear();toCall.clear();report.clear();usePicture=false;date={};time={};textPositions.clear();}
TemplateFields TemplateFields::load(QSettings& settings){
    TemplateFields f;settings.beginGroup("tx/template");
    f.design=settings.value("design",f.design).toString();f.unit=settings.value("unit","F").toString();f.utc=settings.value("utc",false).toBool();
    f.windUnit=settings.value("windUnit","mph").toString();f.rainUnit=settings.value("rainUnit","in").toString();f.pressureUnit=settings.value("pressureUnit","inHg").toString();
    settings.endGroup();
    if(std::none_of(imageTemplates().begin(),imageTemplates().end(),[&](const auto& d){return d.id==f.design;}))f.design="weather";
    if(f.unit!="F" && f.unit!="C")f.unit="F";
    if(!QStringList{"mph","km/h","kn","m/s"}.contains(f.windUnit))f.windUnit="mph";
    if(f.rainUnit!="in"&&f.rainUnit!="mm")f.rainUnit="in";
    if(f.pressureUnit!="inHg"&&f.pressureUnit!="hPa")f.pressureUnit="inHg";
    return f;
}
void TemplateFields::save(QSettings& settings) const{
    settings.beginGroup("tx/template");settings.remove("");settings.setValue("design",design);settings.setValue("unit",unit);settings.setValue("utc",utc);settings.setValue("windUnit",windUnit);settings.setValue("rainUnit",rainUnit);settings.setValue("pressureUnit",pressureUnit);settings.endGroup();
}
namespace {
void text(QPainter& p,const QRectF& box,const QString& value,int pixels,const QColor& color,bool bold=true,int alignment=Qt::AlignLeft|Qt::AlignVCenter){
    QFont font;font.setFamilies(QStringList{"DejaVu Sans"}+QFontDatabase::systemFont(QFontDatabase::GeneralFont).families());font.setBold(bold);
    for(;pixels>16;--pixels){font.setPixelSize(pixels);if(QFontMetricsF(font).horizontalAdvance(value)<=box.width() && QFontMetricsF(font).height()<=box.height())break;}
    font.setPixelSize(pixels);p.setFont(font);p.setPen(color);
    p.drawText(box,alignment,QFontMetricsF(font).elidedText(value,Qt::ElideRight,box.width()));
}
void weatherIcon(QPainter& p,const QString& condition,const QColor& ink,const QColor& accent,bool compact){
    p.save();p.translate(444,compact?180:194);if(compact)p.scale(.7,.7);p.setPen(QPen(ink,6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    const auto value=condition.toLower();const bool snow=value.contains("snow")||value.contains("sleet"),storm=value.contains("storm")||value.contains("thunder"),rain=value.contains("rain")||value.contains("drizzle")||value.contains("shower");
    const bool cloud=value.contains("cloud")||value.contains("overcast")||rain||snow||storm;
    if(value.contains("fog")||value.contains("mist")){
        for(int y=10;y<90;y+=24)p.drawLine(QPointF(y%3*12,y),QPointF(126-y%3*10,y));
    }else if(value.contains("wind")){
        for(int y=0;y<3;++y){QPainterPath path;path.moveTo(0,22+y*28);path.lineTo(85-y*12,22+y*28);path.cubicTo(136,22+y*28,120,y*28,99,y*28);p.drawPath(path);}
    }else if(cloud || value.contains("sun") || value.contains("clear")){
        if(!cloud || value.contains("partly") || value.contains("sun")){
            p.setPen(QPen(accent,6,Qt::SolidLine,Qt::RoundCap));p.setBrush(accent);p.drawEllipse(QPointF(63,34),23,23);
            for(int i=0;i<8;++i){const double a=i*std::numbers::pi/4;p.drawLine(QPointF(63+34*std::cos(a),34+34*std::sin(a)),QPointF(63+46*std::cos(a),34+46*std::sin(a)));}
        }
        if(cloud){
            QPainterPath shape;shape.moveTo(21,76);shape.cubicTo(-4,76,-4,40,23,38);shape.cubicTo(25,6,77,4,84,38);shape.cubicTo(119,29,140,76,104,76);shape.closeSubpath();
            p.setPen(QPen(ink,5,Qt::SolidLine,Qt::RoundCap));p.setBrush(QColor("#253449"));p.drawPath(shape);
            p.setPen(QPen(accent,6,Qt::SolidLine,Qt::RoundCap));
            if(snow){for(int x:{28,65,102})for(int i=0;i<3;++i){const double a=i*std::numbers::pi/3;p.drawLine(QPointF(x-9*std::cos(a),100-9*std::sin(a)),QPointF(x+9*std::cos(a),100+9*std::sin(a)));}}
            else if(storm){p.setBrush(accent);p.drawPolygon(QPolygonF{{67,79},{48,103},{65,103},{56,124},{86,94},{68,94}});}
            else if(rain){for(int x:{28,65,102})p.drawLine(QPointF(x,90),QPointF(x-7,106));}
        }
    }else{
        p.setBrush(Qt::NoBrush);p.drawEllipse(QRectF(5,-4,112,112));text(p,{5,12,112,76},"WX",43,ink,true,Qt::AlignCenter);
    }
    p.restore();
}
}
QImage renderTemplate(const TemplateFields& fields,QSize size,const QString& callsign,const QImage& picture,QMap<QString,QRect>* textBounds,bool showCallsign){
    if(textBounds)textBounds->clear();
    if(size.width()<1 || size.height()<1 || size.width()>4096 || size.height()>4096)return {};
    QImage image(size,QImage::Format_RGB32);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);p.scale(size.width()/640.,size.height()/480.);
    if(const auto* design=imageTemplateById(fields.design);design && !design->weatherCard){
        if(design->tvCard)detail::drawTelevisionTemplate(p,fields,callsign,picture,textBounds,showCallsign);
        else detail::drawExchangeTemplate(p,fields,callsign,picture,textBounds,showCallsign);
        return image;
    }
    std::vector<std::function<void()>> movedText;
    const auto fieldText=[&](const QString& key,QRectF box,const QString& value,int pixels,QColor color,bool bold=true,int alignment=Qt::AlignLeft|Qt::AlignVCenter){detail::templateText(p,fields,key,textBounds,box,value,pixels,color,bold,alignment,16,&movedText);};
    const bool paper=fields.design=="postcard";QColor accent("#63e8ff"),ink=Qt::white,muted("#bfcede"),panel(8,23,41,232);
    QLinearGradient sky(0,0,640,480);sky.setColorAt(0,QColor("#071727"));sky.setColorAt(1,QColor("#174c6a"));
    if(fields.design=="sunset"){sky.setColorAt(0,QColor("#331644"));sky.setColorAt(.6,QColor("#e36e55"));sky.setColorAt(1,QColor("#ffbf69"));accent=QColor("#ffcf87");panel=QColor(35,17,44,228);}
    else if(fields.design=="aurora"){sky.setColorAt(0,QColor("#100c32"));sky.setColorAt(1,QColor("#26386b"));accent=QColor("#8df3c9");panel=QColor(13,16,48,232);}
    else if(paper){sky.setColorAt(0,QColor("#f8efda"));sky.setColorAt(1,QColor("#e1d7bb"));ink=QColor("#16334b");muted=QColor("#3c5467");accent=QColor("#b44b20");panel=QColor(255,251,238,246);}
    if(fields.accentColor.isValid())accent=fields.accentColor;
    if(fields.backgroundColor.isValid()){sky.setColorAt(0,fields.backgroundColor.darker(150));sky.setColorAt(1,fields.backgroundColor.lighter(110));panel=fields.backgroundColor.darker(230);panel.setAlpha(232);}
    p.fillRect(QRectF(0,0,640,480),sky);
    if(fields.design=="sunset"){
        p.setPen(Qt::NoPen);p.setBrush(QColor("#ffcc87"));p.drawEllipse(QPointF(510,185),108,108);
        for(int layer=0;layer<3;++layer){p.setBrush(QColor(layer==0?"#8a4b66":layer==1?"#583954":"#272b43"));const int y=270+layer*65;p.drawPolygon(QPolygonF{{-30.,480.},{-30.,double(y+25)},{96.,double(y-90)},{182.,double(y+25)},{336.,double(y-50)},{450.,double(y+24)},{563.,double(y-100)},{680.,double(y)},{680.,480.}});}
    }else if(fields.design=="aurora"){
        p.setPen(Qt::NoPen);for(int ribbon=0;ribbon<3;++ribbon){QPainterPath path;path.moveTo(-40,90+ribbon*65);path.cubicTo(150,-30+ribbon*30,305,230+ribbon*35,690,20+ribbon*60);path.lineTo(690,90+ribbon*60);path.cubicTo(350,280+ribbon*30,140,30+ribbon*30,-40,165+ribbon*65);path.closeSubpath();p.fillPath(path,QColor(80+ribbon*35,245-ribbon*10,190+ribbon*20,65));}
        for(int i=0;i<30;++i){p.setBrush(QColor(220,244,255,140));p.drawEllipse(QPointF((i*113+19)%640,(i*67+11)%480),1.5,1.5);}
    }else if(paper){
        p.setPen(QPen(QColor("#c9b891"),2));for(int x=-480;x<640;x+=24)p.drawLine(QPointF(x,480),QPointF(x+480,0));
        p.fillRect(QRectF(16,16,608,62),QColor("#16334b"));
    }else{
        p.setPen(QPen(QColor(99,232,255,35),2));for(int radius=80;radius<650;radius+=65)p.drawEllipse(QPointF(610,90),radius,radius);
        p.setPen(QPen(accent,3));QPainterPath wave;wave.moveTo(0,452);for(int x=0;x<=640;x+=4)wave.lineTo(x,449+11*std::sin(x*.042));p.drawPath(wave);
    }
    const bool readings=!fields.windSpeed.trimmed().isEmpty()||!fields.rainfall.trimmed().isEmpty()||!fields.humidity.trimmed().isEmpty()||!fields.pressure.trimmed().isEmpty();
    p.setPen(QPen(accent,2));p.setBrush(panel);p.drawRoundedRect(QRectF(16,92,608,readings?280:234),18,18);p.drawRoundedRect(QRectF(16,readings?384:338,608,readings?46:68),12,12);
    const auto headerInk=paper?QColor(Qt::white):ink;
    if(showCallsign)fieldText("callsign",{30,19,392,55},callsign.trimmed().isEmpty()?"YOUR STATION":callsign.trimmed().toUpper(),44,headerInk);
    fieldText("heading",{430,25,180,38},"STATION / WX",20,paper?QColor("#ffca86"):accent,true,Qt::AlignRight|Qt::AlignVCenter);
    p.setPen(QPen(accent,4));p.drawLine(QPointF(30,80),QPointF(610,80));
    QString location=fields.city.trimmed();if(!fields.state.trimmed().isEmpty())location+=(location.isEmpty()?"":", ")+fields.state.trimmed();
    fieldText("location",{32,105,576,49},location.isEmpty()?"YOUR CITY, STATE":location,38,ink);
    fieldText("weather",{32,158,560,35},fields.weather.trimmed().isEmpty()?"Weather not entered":fields.weather.trimmed(),28,paper?muted:accent);
    bool validTemperature=false;const double temperature=fields.temperature.toDouble(&validTemperature);
    const auto reading=validTemperature&&std::isfinite(temperature)?QString::number(temperature,'g',6):QString("—");
    fieldText("temperature",{28,199,395,double(readings?74:112)},reading+"°"+fields.unit,readings?70:102,ink);
    weatherIcon(p,fields.weather,paper?QColor("#16334b"):QColor("#e2f4ff"),accent,readings);
    if(readings){
        const auto observation=[&](const QString& key,const QString& label,const QString& value,const QString& unit,QRectF box){
            if(value.trimmed().isEmpty())return;
            bool ok=false;const double number=value.toDouble(&ok);if(!ok||!std::isfinite(number))return;
            fieldText(key,box,label+": "+QString::number(number,'g',8)+(unit=="%"?"":" ")+unit,27,ink);
        };
        observation("windSpeed","Wind",fields.windSpeed,fields.windUnit,{32,282,276,34});
        observation("humidity","Humidity",fields.humidity,"%",{330,282,278,34});
        observation("rainfall","Rain",fields.rainfall,fields.rainUnit,{32,326,276,34});
        observation("pressure","BP",fields.pressure,fields.pressureUnit,{330,326,278,34});
    }
    const QString zone=fields.utc?"UTC":fields.date.isValid()?QDateTime(fields.date,fields.time,QTimeZone::systemTimeZone()).timeZoneAbbreviation():"Local";
    fieldText("date",{32,double(readings?389:346),290,double(readings?36:50)},fields.date.isValid()?QLocale::c().toString(fields.date,"dd MMM yyyy").toUpper():QString{},readings?25:30,ink);
    fieldText("time",{330,double(readings?389:346),278,double(readings?36:50)},fields.time.isValid()?fields.time.toString("HH:mm")+" "+zone:QString{},readings?25:30,ink,true,Qt::AlignRight|Qt::AlignVCenter);
    fieldText("message",{28,double(readings?439:417),584,double(readings?35:48)},fields.message,readings?27:32,paper?ink:QColor(Qt::white),true,Qt::AlignCenter);
    for(const auto& draw:movedText)draw();
    return image;
}
}
