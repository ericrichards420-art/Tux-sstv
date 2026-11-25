#include "contactlog.h"
#include "core/fskid.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#include <cmath>
namespace sstv {
namespace {
QString validate(const Contact& c){
    if(!validCallsign(c.call))return "Enter a callsign containing letters and a number (up to 12 characters).";
    if(!c.utc.isValid())return "Enter a valid contact date and time.";
    if(!std::isfinite(c.frequencyHz) || c.frequencyHz<0 || c.frequencyHz>1e12)return "Invalid contact frequency.";
    if(!modeById(c.mode))return "Select an SSTV mode.";
    if(c.sentReport.size()>32 || c.receivedReport.size()>32 || c.notes.size()>2000)return "Contact report or notes are too long.";
    return {};
}
QJsonObject json(const Contact& c){return {{"id",c.id},{"call",c.call},{"utc",c.utc.toUTC().toString(Qt::ISODateWithMs)},{"frequencyHz",c.frequencyHz},{"mode",c.mode},{"sentReport",c.sentReport},{"receivedReport",c.receivedReport},{"notes",c.notes},{"rxImage",c.rxImage},{"txImage",c.txImage}};}
bool saveImage(const QString& path,const QImage& image,QString& error){QSaveFile file(path);if(!file.open(QIODevice::WriteOnly) || !image.save(&file,"PNG") || !file.commit()){error="Could not save contact picture: "+file.errorString();return false;}return true;}
}
ContactLog::ContactLog(QString directory):directory_(QDir::cleanPath(std::move(directory))){}
bool ContactLog::load(QList<Contact>& contacts,QString& error) const {
    contacts.clear();error.clear();QFile file(directory_+"/contacts.json");if(!file.exists())return true;
    if(!file.open(QIODevice::ReadOnly)){error=file.errorString();return false;}
    if(file.size()>50*1024*1024){error="Contact log is too large to read.";return false;}
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);const auto root=document.object();
    if(parse.error!=QJsonParseError::NoError || !document.isObject() || root.value("version").toInt()!=1 || !root.value("contacts").isArray()){error="Contact log format is invalid or unsupported; the original file has been preserved.";return false;}
    QList<Contact> loaded;
    for(const auto& value:root.value("contacts").toArray()){
        const auto o=value.toObject();Contact c;c.id=o.value("id").toString();c.call=o.value("call").toString();c.utc=QDateTime::fromString(o.value("utc").toString(),Qt::ISODateWithMs);c.frequencyHz=o.value("frequencyHz").toDouble(-1);c.mode=o.value("mode").toString();c.sentReport=o.value("sentReport").toString();c.receivedReport=o.value("receivedReport").toString();c.notes=o.value("notes").toString();c.rxImage=o.value("rxImage").toString();c.txImage=o.value("txImage").toString();
        if(QUuid(c.id).isNull() || !validate(c).isEmpty()){error="Contact log contains an invalid record; the original file has been preserved.";return false;}
        loaded.push_back(c);
    }
    contacts=std::move(loaded);return true;
}
bool ContactLog::append(Contact c,const QImage& rx,const QImage& tx,QString& error) const {
    c.call=c.call.trimmed().toUpper();error=validate(c);if(!error.isEmpty())return false;
    if(!QDir().mkpath(directory_)){error="Cannot create the contact log folder.";return false;}
    QLockFile lock(directory_+"/contacts.lock");if(!lock.tryLock(0)){error="The contact log is being saved by another window. Try again.";return false;}
    QList<Contact> contacts;if(!load(contacts,error))return false;
    c.id=QUuid::createUuid().toString(QUuid::WithoutBraces);c.rxImage.clear();c.txImage.clear();QStringList created;
    auto attach=[&](const QImage& image,const QString& suffix,QString& destination){
        if(image.isNull())return true;
        const auto folder=directory_+"/pictures";if(!QDir().mkpath(folder)){error="Cannot create contact picture folder.";return false;}
        destination=folder+"/"+c.id+suffix+".png";
        if(!saveImage(destination,image,error))return false;
        created<<destination;
        return true;
    };
    const auto rollback=[&]{for(const auto& path:created)QFile::remove(path);};
    if(!attach(rx,"-rx",c.rxImage) || !attach(tx,"-tx",c.txImage)){rollback();return false;}
    contacts.append(c);QJsonArray array;for(const auto& contact:contacts)array.append(json(contact));
    QSaveFile file(directory_+"/contacts.json");const auto data=QJsonDocument(QJsonObject{{"version",1},{"contacts",array}}).toJson();
    if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()){error="Could not save contact log: "+file.errorString();rollback();return false;}
    return true;
}
bool ContactLog::exportCsv(const QString& path,QString& error) const {
    QList<Contact> contacts;if(!load(contacts,error))return false;
    QByteArray data="Callsign,UTC,Frequency MHz,Mode,Sent report,Received report,Notes,RX image,TX image\r\n";
    for(const auto& c:contacts){QStringList cells{c.call,c.utc.toUTC().toString(Qt::ISODate),c.frequencyHz>0?QString::number(c.frequencyHz/1e6,'f',6):QString{},c.mode,c.sentReport,c.receivedReport,c.notes,c.rxImage,c.txImage};QStringList quoted;for(auto cell:cells){cell.replace('"',"\"\"");quoted<<'"'+cell+'"';}data+=quoted.join(',').toUtf8()+"\r\n";}
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()){error=file.errorString();return false;}return true;
}
}
