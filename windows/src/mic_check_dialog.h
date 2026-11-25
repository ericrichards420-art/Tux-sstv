#pragma once
#include "voice_audio.h"
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
#include <QStringList>
#include <deque>

class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class MicLevelMeter;

// A local capture-only tool: no player, radio controller or remote transport.
class MicCheckDialog:public QDialog {
    Q_OBJECT
public:
    MicCheckDialog(const QStringList& devices,const QString& selected,int gain,QWidget* parent=nullptr);
    ~MicCheckDialog() override;
    QString microphone() const;
    int gainPercent() const;
public slots:
    void done(int result) override;
private:
    struct Reading {qint64 at;double input,rms,voice;bool limited,clipped;};
    VoiceCapture capture_;
    QTimer refresh_;
    QElapsedTimer clock_;
    std::deque<Reading> readings_;
    QString device_;
    QComboBox* microphone_;
    QSpinBox* gain_;
    MicLevelMeter *inputMeter_,*voiceMeter_;
    QLabel *assessment_,*state_;
    QPushButton *toggle_,*apply_;
    void startCheck();
    void resetReadings();
    void updateReadings();
    void assess(const QString& state,const QString& text);
};
