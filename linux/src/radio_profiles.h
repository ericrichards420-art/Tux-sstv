#pragma once
#include "radio.h"
#include <QJsonArray>
namespace profiles {
inline constexpr int MaxProfiles=16;
struct Settings {
    radio::Config radio;
    QString input="default",output="default",connection="other";
    int txLevel=100,rxGain=0;
    bool autoRx=true,autoClock=false,autoFrequency=false;
    double clockPpm=0,offsetHz=0;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject&,Settings&,QString& error);
    bool operator==(const Settings&) const=default;
};
struct Profile {QString id,name;Settings settings;};
class Store {
public:
    QList<Profile> entries;
    QString active,revision;
    static Store initial(const Settings&);
    static bool fromJson(const QJsonObject&,Store&,QString& error);
    QJsonObject toJson() const;
    QJsonObject summary() const;
    const Profile* current() const;
    const Profile* find(const QString&) const;
    bool updateCurrent(const Settings&);
    QString change(const QString& operation,const QString& id,const QString& name,const Settings& live);
};
QString validateName(const QString& name);
}
