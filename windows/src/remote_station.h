#pragma once
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QIODevice>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QPointer>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>
#include <vector>

namespace remote {
constexpr qint64 MaxWaveBytes=64*1024*1024;
constexpr quint32 MaxFrameBytes=128*1024;
enum class Role {Local,Host,Client};
struct Config {
    Role role=Role::Local;
    QString host,key;
    quint16 port=48765;
    bool reconnect=true;
    static Config load(QSettings&);
    void save(QSettings&) const;
    QString validate() const;
};
QString newKey();
QStringList localAddresses();
struct Packet {QJsonObject message;QByteArray data;};
QByteArray frame(const QJsonObject&,const QByteArray& data={});
// Bounded incremental framing, independent of TCP packet boundaries.
class Decoder {
public:
    std::vector<Packet> feed(const QByteArray&);
private:
    QByteArray buffer_;
    quint32 expected_=0;
};
class Channel:public QObject {
    Q_OBJECT
public:
    explicit Channel(QIODevice*,QObject* parent=nullptr);
    bool send(QJsonObject,const QByteArray& data={});
    void secure(const QByteArray& key);
    void close(const QString& reason);
    bool closed() const{return closed_;}
    qint64 queuedBytes() const;
signals:
    void packet(const QJsonObject&,const QByteArray&);
    void ended(const QString&);
    void writable();
private:
    QPointer<QIODevice> device_;
    Decoder decoder_;
    QByteArray key_;
    qint64 sent_=0,received_=0;
    bool closed_=false;
    void read();
};

class Server:public QObject {
    Q_OBJECT
public:
    explicit Server(QObject* parent=nullptr,int leaseMs=6000);
    ~Server() override;
    bool start(quint16 port,const QString& key,const QHostAddress& address=QHostAddress::AnyIPv4);
    void stop();
    bool listening() const{return listener_.isListening();}
    quint16 port() const{return listener_.serverPort();}
    bool hasClient() const{return !owner_.isNull();}
    bool remoteTransmitBusy() const{return upload_.isOpen()||transmitting_||voiceToken_!=0;}
    QString reserveLocalTransmit();
    void releaseLocalTransmit();
    void startLocalMonitor();
    void publishMonitorPcm(const QByteArray&);
    void finishLocalMonitor(bool success);
    QString error() const{return error_;}
    quint64 session() const{return session_;}
    // The same session path accepts TCP and other reliable ordered transports.
    void acceptConnection(QIODevice*);
    void setSnapshot(const QJsonObject&);
    void publishPcm(const QByteArray&);
    void reply(qint64 id,const QString& error={},const QJsonObject& result={},quint64 session=0,bool includeSnapshot=true);
    void finishTransmit(bool success,const QString& message);
    void abortTransmit(const QString& message);
    void finishVoice(const QString& message={});
    using Ready=std::function<void(QString)>;
    std::function<void(Ready)> beginVoice;
    std::function<void(const QByteArray&)> voicePcm;
    std::function<QString()> canTransmit;
    std::function<QString(const QString&,qint64,qint64)> transmit;
signals:
    void clientChanged(bool);
    void activityChanged();
    void emergencyStop();
    void voiceStopped();
    void request(qint64 id,const QString& operation,const QJsonObject& arguments);
private:
    QTcpServer listener_;
    QSet<Channel*> connections_;
    QPointer<Channel> owner_;
    QTemporaryDir directory_;
    QFile upload_;
    QCryptographicHash uploadHash_{QCryptographicHash::Sha256};
    QByteArray expectedHash_;
    qint64 uploadSize_=0,uploadPosition_=0,morseStart_=-1,pcmSequence_=0;
    QJsonObject snapshot_;
    QString key_,error_;
    QTimer watchdog_;
    QElapsedTimer heartbeat_;
    int leaseMs_;
    bool transmitting_=false;
    qint64 uploadToken_=0,transmitToken_=0;
    quint64 session_=0;
    bool localReserved_=false,monitorActive_=false,monitorSupported_=false;
    qint64 monitorId_=0,monitorSequence_=0;
    qint64 voiceToken_=0,voiceSequence_=0,voiceRequest_=0;
    bool voiceReady_=false;
    void handle(const QJsonObject&,const QByteArray&);
    void cancelUpload();
    void lostOwner(Channel*);
};

class Client:public QObject {
    Q_OBJECT
public:
    using Done=std::function<void(QString)>;
    using ResultDone=std::function<void(QString,QJsonObject)>;
    explicit Client(QObject* parent=nullptr,int leaseMs=6000);
    ~Client() override;
    void connectToHost(const Config&);
    void connectDevice(QIODevice*,const QString& key);
    void disconnectFromHost();
    bool connected() const{return authenticated_;}
    bool transmitting() const{return txActive_;}
    QString status() const{return status_;}
    QJsonObject snapshot() const{return snapshot_;}
    void command(const QString&,const QJsonObject& arguments={},Done done={});
    void commandResult(const QString&,const QJsonObject&,ResultDone);
    bool sendWave(const QString& path,qint64 morseStart,QString* error=nullptr);
    void stopTransmit();
    bool voiceSupported() const{return voiceSupported_;}
    bool voiceActive() const{return voiceToken_!=0;}
    void startVoice(Done);
    void sendVoicePcm(const QByteArray&);
    void stopVoice();
signals:
    void changed();
    void snapshotChanged();
    void pcm(const QByteArray&);
    void transmitStarted(qint64 durationMs,qint64 morseStart);
    void transmitFinished(bool success,const QString& message);
    void monitorStarted();
    void monitorPcm(const QByteArray&);
    void monitorFinished(bool success);
    void voiceFinished(const QString& message);
private:
    QPointer<Channel> channel_;
    Config config_;
    QJsonObject snapshot_;
    QMap<qint64,Done> pending_;
    QMap<qint64,ResultDone> resultPending_;
    QMap<qint64,qint64> deadlines_;
    QElapsedTimer clock_,heartbeat_;
    QTimer tick_,retry_;
    QFile upload_;
    QString uploadProfile_;
    qint64 requestId_=0,uploadPosition_=0,uploadMorse_=-1,pcmSequence_=0;
    quint64 txGeneration_=0;
    qint64 monitorId_=0,monitorSequence_=0;
    qint64 voiceToken_=0,voiceSequence_=0,voiceGeneration_=0;
    bool voiceSupported_=false,voiceReady_=false;
    int leaseMs_;
    bool authenticated_=false,challengeReceived_=false,txActive_=false,uploadReady_=false,retryEnabled_=false;
    QString status_="Local USB / audio";
    void attach(QIODevice*,const QString&);
    void handle(const QJsonObject&,const QByteArray&);
    void lost(const QString&);
    void pump();
};
}
