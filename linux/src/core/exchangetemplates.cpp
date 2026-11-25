#include "exchangetemplates.h"
#include "templatetext.h"
#include <QDateTime>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <cmath>

namespace sstv::detail {
namespace {
QString location(const TemplateFields& f){QString value=f.city.trimmed();if(!f.state.trimmed().isEmpty())value+=(value.isEmpty()?"":", ")+f.state.trimmed();return value;}
QString stamp(const TemplateFields& f){
    QString value=f.date.isValid()?QLocale::c().toString(f.date,"dd MMM yyyy").toUpper():QString{};
    if(f.time.isValid()){
        const QString zone=f.utc?"UTC":f.date.isValid()?QDateTime(f.date,f.time,QTimeZone::systemTimeZone()).timeZoneAbbreviation():"Local";
        value+=(value.isEmpty()?"":" · ")+f.time.toString("HH:mm")+" "+zone;
    }return value;
}
}
void drawExchangeTemplate(QPainter& p,const TemplateFields& f,const QString& callsign,const QImage& picture,QMap<QString,QRect>* textBounds,bool showCallsign){
    std::vector<std::function<void()>> movedText;
    const auto fieldText=[&](const QString& key,QRectF box,const QString& value,int pixels,QColor color,int alignment=Qt::AlignCenter){templateText(p,f,key,textBounds,box,value,pixels,color,true,alignment,18,&movedText);};
    QString title,subtitle,badge;QColor accent("#69e1f1"),bottom("#195074");
    if(f.design=="cq"){title="CQ SSTV";subtitle="ANYONE FOR A PICTURE?";badge="CALLING CQ";}
    else if(f.design=="reply"){title="BACK TO YOU";subtitle="LET'S SWAP PICTURES";badge="SSTV REPLY";accent=QColor("#b8e591");bottom=QColor("#215b53");}
    else if(f.design=="report"){title="YOUR PICTURE";subtitle="PICTURE REPORT";badge="REPORT";accent=QColor("#ffda8b");bottom=QColor("#5b4039");}
    else if(f.design=="thanks"){title="NICE PICTURE!";subtitle="THANKS FOR SENDING IT";badge="THANK YOU";accent=QColor("#e0bafa");bottom=QColor("#4c3c76");}
    else if(f.design=="your-turn"){title="YOUR TURN!";subtitle="SEND ONE BACK!";badge="PICTURE SWAP";accent=QColor("#ffc39e");bottom=QColor("#623c56");}
    else if(f.design=="73"){title="73!";subtitle="SEE YOU AGAIN ON SSTV";badge="GOOD CONTACT";accent=QColor("#a6ddfa");bottom=QColor("#245766");}
    else {badge="PICTURE SWAP";accent=QColor("#f4dfac");bottom=QColor("#3e4d5d");}
    if(f.accentColor.isValid())accent=f.accentColor;
    if(f.backgroundColor.isValid())bottom=f.backgroundColor;
    const bool photo=f.usePicture&&!picture.isNull(),photoCard=f.design=="photo";
    QLinearGradient sky(0,0,640,480);sky.setColorAt(0,f.backgroundColor.isValid()?f.backgroundColor.darker(180):QColor("#08182a"));sky.setColorAt(1,bottom);p.fillRect(QRectF(0,0,640,480),sky);
    if(photo){
        const auto scaled=picture.scaled({640,480},Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawImage(QPointF((640-scaled.width())/2.,(480-scaled.height())/2.),scaled);
        if(!photoCard)p.fillRect(QRectF(0,0,640,480),QColor(0,0,0,45));
    }else{
        p.setPen(QPen(QColor(accent.red(),accent.green(),accent.blue(),35),2));p.setBrush(Qt::NoBrush);
        for(int radius=60;radius<650;radius+=58)p.drawEllipse(QPointF(632,225),radius,radius);
        QPainterPath wave;wave.moveTo(0,240);for(int x=0;x<=640;x+=4)wave.lineTo(x,240+32*std::sin(x*.025));p.setPen(QPen(QColor(accent.red(),accent.green(),accent.blue(),100),4));p.drawPath(wave);
    }
    p.fillRect(QRectF(0,0,640,76),QColor(6,17,31,235));p.setPen(QPen(accent,4));p.drawLine(QPointF(24,76),QPointF(616,76));
    if(showCallsign)fieldText("callsign",{24,10,406,55},callsign.trimmed().isEmpty()?"YOUR STATION":callsign.trimmed().toUpper(),46,Qt::white,Qt::AlignLeft|Qt::AlignVCenter);
    fieldText("badge",{436,21,180,34},badge,20,accent,Qt::AlignRight|Qt::AlignVCenter);
    const QString to=f.toCall.trimmed().isEmpty()?QString{}:"TO "+f.toCall.trimmed().toUpper();
    if(!photoCard){
        if(photo)p.fillRect(QRectF(16,94,608,94),QColor(6,17,31,190));
        fieldText("title",{24,95,592,92},title,f.design=="cq"?90:f.design=="73"?112:70,accent);
        if(!to.isEmpty()){
            if(photo)p.fillRect(QRectF(24,198,592,57),QColor(6,17,31,185));
            fieldText("toCall",{32,199,576,55},to,44,Qt::white);
        }
        if(f.design=="report" && !f.report.trimmed().isEmpty()){
            p.fillRect(QRectF(24,268,592,65),QColor(6,17,31,215));fieldText("reportLabel",{36,277,160,42},"REPORT",24,accent,Qt::AlignLeft|Qt::AlignVCenter);fieldText("report",{204,269,396,62},f.report.trimmed(),58,Qt::white);
        }else{
            if(photo)p.fillRect(QRectF(24,277,592,46),QColor(6,17,31,180));
            fieldText("subtitle",{32,279,576,42},subtitle,30,Qt::white);
        }
    }
    const int footerTop=photoCard?326:346;
    p.fillRect(QRectF(0,footerTop,640,480-footerTop),QColor(6,17,31,photoCard?205:222));p.setPen(QPen(accent,2));p.drawLine(QPointF(24,footerTop),QPointF(616,footerTop));
    if(photoCard)fieldText("toCall",{24,330,592,44},to,36,accent);
    fieldText("message",{24,double(photoCard?375:353),592,45},f.message,34,Qt::white);
    const auto place=location(f),when=stamp(f);
    if(photoCard){
        fieldText("stamp",{24,424,592,40},place+(place.isEmpty()||when.isEmpty()?"":" · ")+when,24,accent);
    }else{
        fieldText("location",{24,405,592,32},place,26,Qt::white);fieldText("stamp",{24,444,592,28},when,22,accent);
    }
    if(photoCard&&!photo)fieldText("pictureLabel",{34,156,572,92},"YOUR PICTURE HERE",46,accent);
    for(const auto& draw:movedText)draw();
}
}
