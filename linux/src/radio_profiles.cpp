#include "radio_profiles.h"
#include <QUuid>
#include <QSet>
#include <cmath>
namespace profiles {
namespace {
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
bool validId(const QString& text){return text.size()==36&&!QUuid(text).isNull();}
}
QString validateName(const QString& name){
    if(name.trimmed().isEmpty())return "Enter a radio profile name.";
    if(name!=name.trimmed()||name.toUtf8().size()>64)return "Use a shorter profile name, without spaces at either end.";
    for(const auto ch:name)if(ch.isNull()||ch.category()==QChar::Other_Control)return "Use a one-line profile name.";
    return {};
}
QJsonObject Settings::toJson() const{return {{"radio",radio.toJson()},{"input",input},{"output",output},{"connection",connection},{"txLevel",txLevel},{"rxGain",rxGain},{"autoRx",autoRx},{"autoClock",autoClock},{"autoFrequency",autoFrequency},{"clockPpm",clockPpm},{"offsetHz",offsetHz}};}
bool Settings::fromJson(const QJsonObject& json,Settings& result,QString& error){
    Settings value;
    if(!radio::Config::fromJson(json.value("radio").toObject(),value.radio,error))return false;
    const auto text=[&](const char* key,QString& out){const auto v=json.value(key);if(!v.isString()||v.toString().trimmed().isEmpty()||v.toString().size()>500||v.toString().contains(QChar::Null))return false;out=v.toString();return true;};
    const auto number=[&](const char* key,double low,double high,bool integral){const auto v=json.value(key);const double n=v.toDouble(low-1);return v.isDouble()&&std::isfinite(n)&&n>=low&&n<=high&&(!integral||n==std::floor(n));};
    if(!text("input",value.input)||!text("output",value.output)||!text("connection",value.connection)||!QStringList{"other","usb","signalink","radio-usb"}.contains(value.connection)||
       !number("txLevel",0,100,true)||!number("rxGain",-24,24,true)||!number("clockPpm",-10000,10000,false)||!number("offsetHz",-200,200,false)||
       !json.value("autoRx").isBool()||!json.value("autoClock").isBool()||!json.value("autoFrequency").isBool()){error="Invalid radio profile audio or receive settings.";return false;}
    value.txLevel=json["txLevel"].toInt();value.rxGain=json["rxGain"].toInt();value.clockPpm=json["clockPpm"].toDouble();value.offsetHz=json["offsetHz"].toDouble();
    value.autoRx=json["autoRx"].toBool();value.autoClock=json["autoClock"].toBool();value.autoFrequency=json["autoFrequency"].toBool();result=value;return true;
}
Store Store::initial(const Settings& settings){Store store;store.active=uuid();store.revision=uuid();store.entries.append({store.active,"Current radio",settings});return store;}
const Profile* Store::find(const QString& id) const{for(const auto& profile:entries)if(profile.id==id)return &profile;return nullptr;}
const Profile* Store::current() const{return find(active);}
bool Store::updateCurrent(const Settings& settings){for(auto& profile:entries)if(profile.id==active){if(profile.settings==settings)return false;profile.settings=settings;return true;}return false;}
QJsonObject Store::toJson() const{QJsonArray items;for(const auto& p:entries)items.append(QJsonObject{{"id",p.id},{"name",p.name},{"settings",p.settings.toJson()}});return {{"version",1},{"active",active},{"revision",revision},{"items",items}};}
QJsonObject Store::summary() const{QJsonArray items;for(const auto& p:entries)items.append(QJsonObject{{"id",p.id},{"name",p.name}});return {{"version",1},{"active",active},{"revision",revision},{"items",items},{"txLevel",current()?current()->settings.txLevel:100}};}
bool Store::fromJson(const QJsonObject& json,Store& result,QString& error){
    const auto items=json.value("items");Store store;store.active=json.value("active").toString();store.revision=json.value("revision").toString();
    if(json.value("version")!=QJsonValue(1)||!items.isArray()||items.toArray().isEmpty()||items.toArray().size()>MaxProfiles||!validId(store.active)||!validId(store.revision)){error="Invalid radio profile collection.";return false;}
    QSet<QString> ids,names;
    for(const auto item:items.toArray()){
        const auto object=item.toObject();Profile p;p.id=object.value("id").toString();p.name=object.value("name").toString();
        if(!item.isObject()||!validId(p.id)||!validateName(p.name).isEmpty()||ids.contains(p.id)||names.contains(p.name.toCaseFolded())){error="Invalid or duplicate radio profile name or identifier.";return false;}
        if(!Settings::fromJson(object.value("settings").toObject(),p.settings,error))return false;
        ids.insert(p.id);names.insert(p.name.toCaseFolded());store.entries<<p;
    }
    if(!store.current()){error="Active radio profile is missing.";return false;}result=store;return true;
}
QString Store::change(const QString& operation,const QString& id,const QString& name,const Settings& live){
    if(operation!="save"&&operation!="new"&&operation!="switch"&&operation!="rename"&&operation!="delete")return "Unknown radio profile operation.";
    if(operation=="new"||operation=="rename"){
        const auto error=validateName(name);if(!error.isEmpty())return error;
        for(const auto& p:entries)if(p.name.compare(name,Qt::CaseInsensitive)==0&&(operation=="new"||p.id!=id))return "A radio profile with that name already exists.";
    }
    if(operation=="new"&&entries.size()>=MaxProfiles)return QString("You can save up to %1 radio profiles.").arg(MaxProfiles);
    if(operation!="save"&&operation!="new"&&!find(id))return "That radio profile no longer exists. Reopen Profiles.";
    if(operation=="delete"&&(id==active||entries.size()<=1))return "Switch to another radio profile before deleting this one.";
    Settings validated;QString error;if(!Settings::fromJson(live.toJson(),validated,error))return error;
    updateCurrent(live);
    if(operation=="save")return {};
    if(operation=="switch"){if(active==id)return {};active=id;}
    if(operation=="new"){active=uuid();entries.append({active,name,live});}
    if(operation=="rename")for(auto& p:entries)if(p.id==id)p.name=name;
    if(operation=="delete")for(qsizetype i=0;i<entries.size();++i)if(entries[i].id==id){entries.removeAt(i);break;}
    revision=uuid();return {};
}
}
