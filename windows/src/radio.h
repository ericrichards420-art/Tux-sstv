#pragma once
#include <QObject>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QJsonObject>
#include <atomic>
#include <functional>
#include <memory>

namespace radio {
struct Config {
    bool enabled=false,network=false,automaticPtt=true;
    int model=1,baud=0,port=4532,pttValue=1,leadMs=300,tailMs=100;
    QString device,host="127.0.0.1",pttType="RIG",pttDevice,targetVfo="currVFO",extra;
    QString validate() const;
    static Config load(QSettings& settings);
    void save(QSettings& settings) const;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject&,Config&,QString& error);
    bool operator==(const Config&) const=default;
};
enum class Control {Frequency,Mode,Vfo,Power,Rit,Split,SplitFrequency,RepeaterShift,RepeaterOffset,Tone};
struct Command {Control control;double value=0;QString text;int option=0;};
QStringList modeNames(int model=0);
QString verifyReadback(const Command&,const QVariantMap&);
class Backend {
public:
    virtual ~Backend()=default;
    virtual void open(const Config&)=0;
    virtual QVariantMap read(bool pttOnly=false)=0;
    virtual void setPtt(bool)=0;
    virtual void apply(const Command&)=0;
    virtual QString close()=0; // Always attempts to release owned/uncertain PTT first.
};
std::unique_ptr<Backend> hamlibBackend();
QList<QPair<int,QString>> models(QString* error=nullptr);
QStringList serialDevices();

// All Hamlib operations run on one dedicated worker; there is only one open RIG.
// A shared desired-PTT flag prevents a delayed key response from starting TX
// after Stop or close. PTT OFF can always be queued, including after a failure.
class Controller:public QObject {
    Q_OBJECT
public:
    using Done=std::function<void(QString)>;
    using RemoteRequest=std::function<void(const QString&,const QVariantMap&,Done)>;
    explicit Controller(QObject* parent=nullptr,std::unique_ptr<Backend> backend={});
    ~Controller() override;
    bool connected() const{return connected_;}
    bool busy() const{return pending_>0;}
    bool pttActive() const{return remote_?(remoteKeyPending_||remotePttActive_||state_.value("ptt",0).toInt()!=0):(wanted_->load() || held_ || uncertain_);}
    // An accepted key advances once, before changed(); release and connection
    // changes invalidate it so delayed work cannot release another activation.
    quint64 pttGeneration() const{return pttGeneration_;}
    bool supportsOwnedPtt() const{return !remote_||!remotePttActivation_.isEmpty();}
    QVariantMap state() const{return state_;}
    QString error() const{return error_;}
    void connectRadio(const Config&,Done done={});
    void disconnectRadio(Done done={});
    void testConnection(const Config&,Done done);
    void refresh();
    void apply(Command,Done done={});
    void key(Done done);
    void release(Done done={});
    void releaseIfCurrent(quint64 generation);
    QString shutdown();
    void setRemote(RemoteRequest);
    void setRemoteState(bool connected,const QVariantMap& state,const QString& error={},bool pttActive=false,const QString& pttActivation={});
signals:
    void changed();
    void fault(const QString&);
private:
    QThread thread_;
    QObject* worker_;
    std::shared_ptr<Backend> backend_;
    // Access only on the worker, so shutdown sees even a close failure whose
    // completion has not reached the GUI yet.
    std::shared_ptr<QString> unconfirmedClose_=std::make_shared<QString>();
    std::shared_ptr<std::atomic_bool> wanted_=std::make_shared<std::atomic_bool>(false);
    QTimer poll_;
    QVariantMap state_;
    QString error_;
    int pending_=0;
    bool connected_=false,held_=false,uncertain_=false,stopped_=false;
    RemoteRequest remote_;
    bool remoteKeyPending_=false,remotePttActive_=false;
    QString remotePttActivation_,remoteKeyActivation_;
    quint64 remoteGeneration_=0,pttGeneration_=0,remoteOwnedReleasePending_=0;
    void remoteRun(const QString&,const QVariantMap&,Done);
    void run(std::function<QVariantMap()> work,Done done={},bool communicationFault=false);
};
}
