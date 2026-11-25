#pragma once
#include <QDateTime>
#include <QImage>
#include <QList>
namespace sstv {
struct Contact {
    QString id,call,mode,sentReport,receivedReport,notes,rxImage,txImage;
    QDateTime utc=QDateTime::currentDateTimeUtc();
    double frequencyHz=0; // Zero means unknown, never a guessed radio frequency.
};
class ContactLog {
public:
    explicit ContactLog(QString directory);
    bool load(QList<Contact>& contacts,QString& error) const;
    bool append(Contact contact,const QImage& rx,const QImage& tx,QString& error) const;
    bool exportCsv(const QString& path,QString& error) const;
    QString directory() const{return directory_;}
private:
    QString directory_;
};
}
