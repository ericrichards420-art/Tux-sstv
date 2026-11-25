#pragma once
#include <QDate>
#include <QTime>
#include <QImage>
#include <QSettings>
#include <QMap>
#include <QPointF>
#include <QRect>
#include <QColor>

namespace sstv {
struct ImageTemplate {
    QString id,name,description;
    bool weatherCard=true;
    bool tvCard=false;
};
const QList<ImageTemplate>& imageTemplates();
const ImageTemplate* imageTemplateById(const QString& id);
struct TemplateFields {
    QString design="weather",city,state,weather,temperature,unit="F",message,toCall,report;
    QString windSpeed,windUnit="mph",rainfall,rainUnit="in",humidity,pressure,pressureUnit="inHg";
    QString headline,subheadline,ticker,programInfo,homeTeam,awayTeam,homeScore,awayScore;
    QDate date;
    QTime time;
    bool utc=false,usePicture=false;
    QMap<QString,QPointF> textPositions;
    QColor accentColor,textColor,backgroundColor; // Invalid means the design default.
    QString validate() const;
    void useCurrentTime();
    void clearFields();
    // Only design/unit/zone preferences persist; readings belong to an image.
    static TemplateFields load(QSettings&);
    void save(QSettings&) const;
    bool operator==(const TemplateFields&) const=default;
};
// Pure image rendering: never fetches weather, reads a clock, or opens a radio.
// showCallsign=false omits the station field, including its blank-call placeholder.
QImage renderTemplate(const TemplateFields&,QSize size,const QString& callsign,const QImage& picture={},QMap<QString,QRect>* textBounds=nullptr,bool showCallsign=true);
}
