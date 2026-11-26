#pragma once
#include "radio.h"
#include <QDialog>
#include <QWidget>
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;

class RadioSettingsPage:public QWidget {
    Q_OBJECT
public:
    RadioSettingsPage(const radio::Config&,radio::Controller*,QWidget* parent=nullptr);
    radio::Config config() const;
    void setDeviceChoices(const QStringList&);
    void setAudioConnection(const QString&);
    QString validationError() const;
signals:
    void edited();
private:
    QCheckBox *enabled_,*automatic_;
    QComboBox *connection_,*model_,*device_,*baud_,*method_,*pttValue_,*vfo_;
    QLineEdit *host_,*pttDevice_,*extra_;
    QSpinBox *port_,*lead_,*tail_;
    QPushButton* refreshDevices_;
    QComboBox *preset_,*keying_;
    QLabel *setupHelp_,*keyingHelp_,*deviceLabel_,*modelLabel_,*baudLabel_,*serverLabel_,*pttDeviceLabel_;
    bool guiding_=false;
    QString audioConnection_;
    void applyPreset(const QString&);
    void selectKeying(const QString&);
    void updateGuidance();
};
class RadioControls:public QDialog {
    Q_OBJECT
public:
    RadioControls(radio::Controller*,std::function<bool()> idle,std::function<void()> emergency,QWidget* parent=nullptr,int model=0);
protected:
    void closeEvent(QCloseEvent*) override;
private:
    radio::Controller* controller_;
    QLabel* status_;
    QList<QWidget*> controls_;
    QPushButton* test_;
    bool testOwned_=false;
    quint64 testGeneration_=0;
    std::function<bool()> idle_;
    void updateState();
};
