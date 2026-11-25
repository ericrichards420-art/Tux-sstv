#include "layouts.h"
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#include <cmath>

namespace sstv {
namespace {
bool point(const QJsonValue& value,QPointF& p){const auto a=value.toArray();if(a.size()!=2 || !a[0].isDouble() || !a[1].isDouble())return false;p={a[0].toDouble(),a[1].toDouble()};return std::isfinite(p.x()) && std::isfinite(p.y()) && p.x()>=0 && p.x()<=1 && p.y()>=0 && p.y()<=1;}
QJsonArray point(QPointF p){return {std::clamp(p.x(),0.,1.),std::clamp(p.y(),0.,1.)};}
QString color(const QColor& c){return c.isValid()?c.name(QColor::HexArgb):QString{};}
}
QList<TemplateLayout> LayoutStore::list(QString& error) const {
    error.clear();QList<TemplateLayout> result;
    for(const auto& info:QDir(directory_).entryInfoList({"*.json"},QDir::Files,QDir::Name)) {
        QFile file(info.absoluteFilePath());if(!file.open(QIODevice::ReadOnly) || file.size()>30*1024*1024){error="Some saved layouts could not be read.";continue;}
        QJsonParseError parse;const auto root=QJsonDocument::fromJson(file.readAll(),&parse).object();
        TemplateLayout layout;layout.id=root.value("id").toString();layout.name=root.value("name").toString().trimmed();layout.fields.design=root.value("design").toString();
        if(parse.error!=QJsonParseError::NoError || root.value("version").toInt()!=1 || QUuid(layout.id).isNull() || layout.name.isEmpty() || layout.name.size()>64 || !imageTemplateById(layout.fields.design)){error="Some saved layouts have an invalid or unsupported format.";continue;}
        layout.fields.unit=root.value("unit").toString()=="C"?"C":"F";layout.fields.utc=root.value("utc").toBool();
        const auto wind=root.value("windUnit").toString();layout.fields.windUnit=QStringList{"mph","km/h","kn","m/s"}.contains(wind)?wind:"mph";
        layout.fields.rainUnit=root.value("rainUnit").toString()=="mm"?"mm":"in";layout.fields.pressureUnit=root.value("pressureUnit").toString()=="hPa"?"hPa":"inHg";
        layout.fields.accentColor=QColor(root.value("accent").toString());layout.fields.textColor=QColor(root.value("ink").toString());layout.fields.backgroundColor=QColor(root.value("background").toString());
        const auto positions=root.value("positions").toObject();for(auto it=positions.begin();it!=positions.end();++it){QPointF p;if(it.key().size()<=64 && point(it.value(),p))layout.fields.textPositions[it.key()]=p;}
        const auto caption=root.value("caption").toObject();layout.caption.color=QColor(caption.value("color").toString());if(!layout.caption.color.isValid())layout.caption.color=Qt::white;
        layout.caption.sizePercent=std::clamp(caption.value("size").toInt(7),3,18);layout.caption.position=std::clamp(caption.value("anchor").toInt(7),0,8);layout.caption.bold=caption.value("bold").toBool(true);layout.caption.outline=caption.value("outline").toBool(true);layout.caption.backing=caption.value("backing").toBool();
        QPointF p;layout.caption.freePosition=caption.value("free").toBool() && point(caption.value("position"),p);layout.caption.relativePosition=p;
        const auto encoded=root.value("picture").toString().toLatin1();if(!encoded.isEmpty()){const auto png=QByteArray::fromBase64(encoded);QBuffer buffer;buffer.setData(png);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"PNG");const auto size=reader.size();if(size.width()>0 && size.height()>0 && size.width()<=4096 && size.height()<=4096)layout.picture=reader.read();if(layout.picture.isNull()){error="A saved layout picture could not be read.";continue;}}
        layout.fields.usePicture=!layout.picture.isNull();result<<layout;
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.name.localeAwareCompare(b.name)<0;});return result;
}
bool LayoutStore::save(const QString& input,const TemplateFields& fields,const TextOverlay& caption,const QImage& picture,QString& error) const {
    const auto name=input.trimmed();error.clear();if(name.isEmpty() || name.size()>64){error="Enter a layout name (1–64 characters).";return false;}
    if(!imageTemplateById(fields.design)){error="Unknown template design.";return false;}
    if(!QDir().mkpath(directory_)){error="Cannot create saved layout folder.";return false;}
    QLockFile lock(directory_+"/layouts.lock");if(!lock.tryLock(0)){error="Another window is saving layouts. Try again.";return false;}
    QString ignored;const auto existing=list(ignored);QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);for(const auto& item:existing)if(item.name.compare(name,Qt::CaseInsensitive)==0){id=item.id;break;}
    QJsonObject positions;for(auto it=fields.textPositions.begin();it!=fields.textPositions.end();++it)if(std::isfinite(it->x()) && std::isfinite(it->y()))positions[it.key()]=point(it.value());
    QJsonObject style{{"color",color(caption.color)},{"size",caption.sizePercent},{"anchor",caption.position},{"bold",caption.bold},{"outline",caption.outline},{"backing",caption.backing},{"free",caption.freePosition},{"position",point(caption.relativePosition)}};
    QJsonObject root{{"version",1},{"id",id},{"name",name},{"design",fields.design},{"unit",fields.unit},{"windUnit",fields.windUnit},{"rainUnit",fields.rainUnit},{"pressureUnit",fields.pressureUnit},{"utc",fields.utc},{"accent",color(fields.accentColor)},{"ink",color(fields.textColor)},{"background",color(fields.backgroundColor)},{"positions",positions},{"caption",style}};
    if(fields.usePicture && !picture.isNull()){
        auto saved=picture;if(saved.width()>2048 || saved.height()>2048)saved=saved.scaled({2048,2048},Qt::KeepAspectRatio,Qt::SmoothTransformation);
        // Artwork is retained; image metadata (including old captions/calls) is not.
        QImage artwork(saved.size(),QImage::Format_ARGB32_Premultiplied);artwork.fill(Qt::transparent);QPainter painter(&artwork);painter.drawImage(0,0,saved);painter.end();
        QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);if(!artwork.save(&buffer,"PNG")){error="Cannot encode layout artwork.";return false;}root["picture"]=QString::fromLatin1(png.toBase64());
    }
    const auto data=QJsonDocument(root).toJson();QSaveFile file(directory_+"/"+id+".json");if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()){error=file.errorString();return false;}return true;
}
}
