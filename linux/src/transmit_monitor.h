#pragma once
#include "core/receiver.h"
#include <QObject>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QTimer>

// A local loopback of the played waveform, independent of antenna RX and PTT.
// The worker reads only audio whose playback time has elapsed.
class TransmitMonitor:public QObject {
    Q_OBJECT
public:
    explicit TransmitMonitor(QObject* parent=nullptr);
    ~TransmitMonitor() override;
    void start(const QString& wav);
    void startStream();
    void pushPcm(const QByteArray& pcm);
    void finish(bool success);
    void stop();
    bool active() const{return state_!=nullptr;}
signals:
    void picture(const sstv::DecodeResult&);
    void audio(const QByteArray&);
    void finished(bool complete,const QString& message);
private:
    struct State {
        QString path;
        qint64 position=0,total=0;
        bool stream=false;
        sstv::Receiver receiver;
        sstv::Cancel cancel=std::make_shared<std::atomic_bool>(false);
    };
    struct Result {
        quint64 generation=0;
        QByteArray pcm;
        std::vector<sstv::DecodeResult> frames;
        QString error;
    };
    std::shared_ptr<State> state_;
    QFutureWatcher<Result> job_;
    QTimer timer_;
    QElapsedTimer clock_;
    quint64 generation_=0;
    bool draining_=false,complete_=false,busy_=false;
    QByteArray pendingPcm_;
    void advance();
};
