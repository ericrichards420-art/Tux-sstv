#include "televisiontemplates.h"
#include "templatetext.h"
#include <QDateTime>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>

namespace sstv::detail {
void drawTelevisionTemplate(QPainter& p,const TemplateFields& f,const QString& callsign,const QImage& picture,QMap<QString,QRect>* textBounds,bool showCallsign){
    const bool breaking=f.design=="breaking-news",sports=f.design=="scoreboard",cinema=f.design=="now-showing",studio=f.design=="on-air";
    const QString title=breaking?"BREAKING NEWS":sports?"SCOREBOARD":cinema?"NOW SHOWING":studio?"ON AIR":"SSTV NEWS";
    const QString badge=sports?"SPORTS DESK":cinema?"PICTURE THEATER":studio?"FROM THE SHACK":"NEWSROOM";
    QColor accent=breaking?QColor("#ffca55"):sports?QColor("#b3f5b0"):cinema?QColor("#ffcf77"):studio?QColor("#72f2e0"):QColor("#7bdaff");
    QColor base=breaking?QColor("#5a101c"):sports?QColor("#103d32"):cinema?QColor("#382135"):studio?QColor("#251747"):QColor("#102e57");
    if(f.accentColor.isValid())accent=f.accentColor;
    if(f.backgroundColor.isValid())base=f.backgroundColor;
    const QColor panel(5,13,28,220);
    std::vector<std::function<void()>> moved;
    const auto text=[&](const QString& key,QRectF box,const QString& value,int pixels,QColor color,int alignment=Qt::AlignLeft|Qt::AlignVCenter){
        templateText(p,f,key,textBounds,box,value,pixels,color,true,alignment,16,&moved);
    };
    QLinearGradient background(0,0,640,480);background.setColorAt(0,base.darker(190));background.setColorAt(1,base.lighter(120));p.fillRect(QRectF(0,0,640,480),background);
    if(f.usePicture&&!picture.isNull()){
        const auto scaled=picture.scaled({640,480},Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
        p.drawImage(QPointF((640-scaled.width())/2.,(480-scaled.height())/2.),scaled);
        p.fillRect(QRectF(0,0,640,480),QColor(0,0,0,70));
    }else if(sports){
        p.setPen(QPen(QColor(180,255,204,35),2));
        for(int x=0;x<640;x+=64)p.drawLine(QPointF(x,80),QPointF(x,390));
        p.drawEllipse(QPointF(320,240),100,100);
    }else if(cinema){
        p.setPen(Qt::NoPen);p.setBrush(QColor(255,214,140,80));
        for(int y=91;y<390;y+=33){p.drawRoundedRect(QRectF(5,y,10,18),2,2);p.drawRoundedRect(QRectF(625,y,10,18),2,2);}
    }else{
        p.setPen(QPen(QColor(accent.red(),accent.green(),accent.blue(),35),2));
        for(int x=-480;x<640;x+=40)p.drawLine(QPointF(x,480),QPointF(x+480,80));
        p.setPen(QPen(QColor(accent.red(),accent.green(),accent.blue(),65),3));p.drawEllipse(QPointF(578,230),156,156);p.drawEllipse(QPointF(578,230),189,189);
    }
    p.fillRect(QRectF(0,0,640,76),QColor(5,13,28,240));
    if(showCallsign)text("callsign",{24,12,366,51},callsign.trimmed().isEmpty()?"YOUR STATION":callsign.trimmed().toUpper(),42,Qt::white);
    text("badge",{400,22,216,34},badge,20,accent,Qt::AlignRight|Qt::AlignVCenter);
    p.setPen(QPen(accent,3));p.drawLine(QPointF(24,76),QPointF(616,76));
    const QColor banner=breaking?QColor("#bf2033"):studio?QColor("#543881"):sports?QColor("#215a44"):cinema?QColor("#5b3846"):QColor("#185ea2");
    p.fillRect(QRectF(20,94,600,68),banner);p.fillRect(QRectF(20,94,7,68),accent);
    text("title",{36,98,568,58},title,52,cinema?accent:QColor(Qt::white));
    p.fillRect(QRectF(20,178,600,206),panel);
    if(sports){
        p.setPen(QPen(accent,2));p.drawLine(QPointF(320,191),QPointF(320,369));
        text("homeLabel",{36,186,252,25},"HOME",18,accent,Qt::AlignCenter);
        text("awayLabel",{352,186,252,25},"AWAY",18,accent,Qt::AlignCenter);
        text("homeTeam",{36,216,252,43},f.homeTeam,30,Qt::white,Qt::AlignCenter);
        text("awayTeam",{352,216,252,43},f.awayTeam,30,Qt::white,Qt::AlignCenter);
        text("homeScore",{36,266,252,100},f.homeScore.trimmed().isEmpty()?"—":f.homeScore,90,Qt::white,Qt::AlignCenter);
        text("awayScore",{352,266,252,100},f.awayScore.trimmed().isEmpty()?"—":f.awayScore,90,Qt::white,Qt::AlignCenter);
    }else{
        text("headline",{36,189,568,73},f.headline,50,Qt::white);
        text("subheadline",{36,270,568,47},f.subheadline,32,accent);
        if(cinema||studio)text("programInfo",{36,329,568,38},f.programInfo,27,Qt::white);
    }
    p.fillRect(QRectF(0,400,640,44),QColor(5,13,28,245));p.fillRect(QRectF(20,400,98,44),accent);
    // The station identity and these graphic labels remain generic; all story,
    // program and score content comes from the operator's draft.
    text("tickerLabel",{25,404,88,35},sports?"SCORES":cinema?"NEXT":studio?"SSTV":"UPDATE",18,QColor("#071727"),Qt::AlignCenter);
    text("ticker",{132,405,482,34},f.ticker,25,Qt::white);
    QString place=f.city.trimmed();if(!f.state.trimmed().isEmpty())place+=(place.isEmpty()?"":", ")+f.state.trimmed();
    QString stamp=f.date.isValid()?QLocale::c().toString(f.date,"dd MMM yyyy").toUpper():QString{};
    if(f.time.isValid()){
        const QString zone=f.utc?"UTC":f.date.isValid()?QDateTime(f.date,f.time,QTimeZone::systemTimeZone()).timeZoneAbbreviation():"Local";
        stamp+=(stamp.isEmpty()?"":" · ")+f.time.toString("HH:mm")+" "+zone;
    }
    text("location",{24,450,290,25},place,19,accent);
    text("stamp",{326,450,290,25},stamp,18,Qt::white,Qt::AlignRight|Qt::AlignVCenter);
    for(const auto& draw:moved)draw();
}
}
