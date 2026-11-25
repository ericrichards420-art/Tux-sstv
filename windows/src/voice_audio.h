#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>

// Voice uses 48 kHz mono S16_LE, in 20 ms packets. Neither class controls PTT.
class VoiceCapture:public QObject {
    Q_OBJECT
public:
    explicit VoiceCapture(QObject* parent=nullptr);
    ~VoiceCapture() override;
    void start(const QString& device,int gainPercent);
    void setGain(int gainPercent);
    void stop();
    bool active() const{return active_;}
signals:
    void opened();
    void pcm(const QByteArray&);
    void level(int percent);
    void levels(double inputPeak,double inputRms,double gainedPeak,bool limited,bool inputClipped);
    void fault(const QString&);
private:
    QProcess process_;
    QTimer watchdog_;
    QElapsedTimer received_;
    QByteArray pending_;
    bool active_=false;
    double gain_=1,limiter_=1;
    void read();
    void fail(const QString&);
};

class VoicePlayer:public QObject {
    Q_OBJECT
public:
    explicit VoicePlayer(QObject* parent=nullptr);
    ~VoicePlayer() override;
    void start(const QString& device);
    void allowPlayback();
    void push(const QByteArray&);
    void stop();
    bool active() const{return active_;}
signals:
    void opened();
    void fault(const QString&);
private:
    QProcess process_;
    QTimer pump_;
    QElapsedTimer received_,held_;
    QByteArray pending_;
    bool active_=false,allowed_=false,primed_=false,hasPcm_=false;
    void tick();
    void fail(const QString&);
};

// Independent laptop RX playback. It never owns a microphone or PTT.
class RemoteSpeaker:public QObject {
    Q_OBJECT
public:
    explicit RemoteSpeaker(QObject* parent=nullptr);
    ~RemoteSpeaker() override;
    void start(const QString& device,int volumePercent);
    void setVolume(int volumePercent);
    void push(const QByteArray&);
    void stop();
    bool active() const{return active_;}
signals:
    void fault(const QString&);
private:
    QProcess process_;
    QTimer pump_;
    QElapsedTimer received_;
    QByteArray pending_;
    bool active_=false,primed_=false;
    int volume_=75;
    void tick();
    void fail(const QString&);
};
