#pragma once
#include "core/audio_levels.h"
#include "playback_mixer.h"
#include <QFutureWatcher>
#include <QGroupBox>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QElapsedTimer>
#include <functional>
namespace ui {struct Skin;}
class QLabel;
class QSlider;
class QSpinBox;
class QPushButton;
class AudioPeakMeter;
class QCheckBox;

class AudioLevelsPanel:public QGroupBox {
    Q_OBJECT
public:
    explicit AudioLevelsPanel(std::function<bool()> canCheck,QWidget* parent=nullptr);
    ~AudioLevelsPanel() override;
    void setSkin(const ui::Skin&);
    int transmitPercent() const;
    void setTransmitPercent(int);
    int receiveGainDb() const;
    void restoreLevels(int tx,int rx,bool automatic);
    void setRemoteTransmitLevel(int percent);
    bool automaticReceiveLevel() const;
    bool checking() const;
    void setDevices(const QString& input,const QString& output);
    void setApplicationState(bool receiving,bool transmitting,bool preparing,bool busy,bool monitoring=false);
    void receiveLevels(const sstv::PcmGainBlock& block);
    void setTransmitEnvelope(std::vector<sstv::AudioLevel> levels);
    void transmitPosition(qint64 milliseconds);
    void endTransmitMeter();
    void stopChecks();
    void shutdown();
    void setRemoteAudio(bool remote);
signals:
    void levelsChanged();
    void inputMonitorStarted();
    void inputMonitorStopped();
    void inputMonitorPcm(const QByteArray& pcm);
    void devicesRequested();
    void activityChanged();
    void tuningRequested();
private:
    std::function<bool()> canCheck_;
    QString input_="default",output_="default";
    QLabel *devices_,*rxStatus_,*txStatus_;
    QSlider *txSlider_,*rxSlider_;
    QSpinBox *txValue_,*rxValue_;
    QCheckBox* autoRx_;
    QPushButton *inputCheck_,*tone_,*devicesButton_;
    AudioPeakMeter *rxMeter_,*txMeter_;
    QProcess capture_,playback_;
    QTimer limit_,meterTimer_;
    QTemporaryDir temporary_;
    sstv::PcmInputGain inputGain_;
    std::vector<sstv::AudioLevel> transmitEnvelope_,testEnvelope_;
    QElapsedTimer testClock_;
    QFutureWatcher<audio::CaptureAdjustment> captureLevelJob_;
    QElapsedTimer captureLevelClock_;
    QString captureLevelNote_;
    size_t clippedSamples_=0;
    bool captureLevelUnavailable_=false;
    bool receiving_=false,transmitting_=false,preparing_=false,busy_=false,stopping_=false,closed_=false;
    bool monitoring_=false;
    bool remoteAudio_=false,restoringLevels_=false;
    int remoteTransmitLevel_=-1;
    void updateButtons();
    void startInputCheck();
    void playTone();
};
