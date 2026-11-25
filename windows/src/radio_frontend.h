#pragma once
#include "radio.h"
#include <QWidget>
#include <QTimer>
#include <functional>
#include <optional>
namespace ui {struct Skin;}
class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QDial;
class QPushButton;
class RadioSignalMeter;
class QVBoxLayout;
class QScrollArea;

class RadioFrontend:public QWidget {
    Q_OBJECT
public:
    RadioFrontend(radio::Controller*,const radio::Config&,std::function<bool()> idle,QWidget* parent=nullptr);
    void setConfig(const radio::Config&);
    void setLocalControlAllowed(bool allowed){localControlAllowed_=allowed;}
    void refresh();
    void addAudioControls(QWidget*);
    void addAudioMonitor(QWidget*);
    void addStationControls(QWidget*);
    void addVoiceControls(QWidget*);
    void showTuning();
    void setSkin(const ui::Skin&);
signals:
    void connectRequested();
    void disconnectRequested();
    void settingsRequested();
    void advancedRequested();
    void stopRequested();
private:
    radio::Controller* controller_;
    radio::Config config_;
    std::function<bool()> idle_;
    QLabel *frequencyDisplay_,*stateBadge_,*modeDisplay_,*vfoDisplay_,*meterLabel_,*connection_,*status_;
    QLabel *filterDisplay_,*powerDisplay_,*ritDisplay_;
    RadioSignalMeter* meter_;
    QVBoxLayout* bodyLayout_;
    QVBoxLayout* rootLayout_;
    QScrollArea* scroll_;
    QPushButton* audioButton_;
    QPushButton* monitorButton_;
    QWidget* monitor_=nullptr;
    QDoubleSpinBox *frequency_,*power_;
    QSpinBox *bandwidth_,*rit_;
    QComboBox *mode_,*vfo_,*step_;
    QSlider* powerSlider_;
    QDial* dial_;
    QPushButton* memoryVfo_;
    QString lastVfo_;
    QPushButton *connect_,*disconnect_,*settings_,*stop_,*tune_,*minus_,*plus_,*setMode_,*setVfo_,*setPower_,*setRit_,*zeroRit_,*read_;
    bool frequencyDirty_=false,modeDirty_=false,vfoDirty_=false,powerDirty_=false,ritDirty_=false;
    QTimer tuningTimer_,modeTimer_;
    std::optional<radio::Command> pendingMode_;
    std::optional<double> tuningTarget_;
    std::optional<double> tuningInFlight_;
    bool wasConnected_=false;
    bool localControlAllowed_=true;
    int dialPosition_=50;
    bool canOperate() const;
    void tuneSteps(int steps);
    void flushTuning();
    void requestMode();
    void flushMode();
    void toggleMemoryVfo();
    void updateChoices();
    void apply(radio::Command,std::function<void()> success);
};
