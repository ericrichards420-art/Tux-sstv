#pragma once
#include <QWizard>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QFutureWatcher>
#include <QMap>
#include <QJsonObject>
#include "playback_mixer.h"
class QComboBox;
class QProgressBar;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;

// Audio checks never request PTT. Radio checks use the existing bounded test.
class AudioWizard:public QWizard {
    Q_OBJECT
public:
    AudioWizard(const QString& input,const QString& output,QWidget* parent=nullptr,bool relay=false);
    ~AudioWizard() override;
    QString inputDevice() const;
    QString outputDevice() const;
    bool openRadioControls() const;
    QString connectionType() const;
    bool useInterfacePtt() const;
    int transmitPercent() const;
    QString mixerError() const{return restoreError_;}
    struct PcmDevice {QString name,description;};
    static QList<PcmDevice> pcmDevices(const QByteArray& listing);
    static QStringList pcmNames(const QByteArray& listing);
    QJsonObject relayState() const;
    void applyRelayState(const QJsonObject&);
    QString relayOperation(const QString&,const QJsonObject&);
    void finishRelay(int result){relayClosing_=true;QWizard::done(result);}
signals:
    void relayRequest(const QString&,const QJsonObject&);
protected:
    void done(int result) override;
private:
    QComboBox *input_,*output_;
    QComboBox *connection_,*device_;
    QCheckBox *advanced_,*interfacePtt_;
    QPushButton* refresh_;
    QLabel *connectionHelp_,*discoveryStatus_,*selection_,*receiveDevice_,*transmitDevice_,*rxHint_,*txHint_,*review_,*radioHint_;
    QList<PcmDevice> inputs_,outputs_;
    bool inputListed_=false,outputListed_=false;
    QStringList discoveryErrors_;
    QProgressBar* level_;
    QLabel *inputStatus_,*outputStatus_;
    QPushButton *listen_,*tone_;
    QCheckBox* radio_;
    QSpinBox* txLevel_;
    QPushButton* optimize_;
    QLabel* mixerStatus_;
    QFutureWatcher<audio::MixerSetup> mixerJob_;
    QMap<QString,audio::MixerSetup> mixerOriginals_;
    bool optimizationPending_=false,mixerClosed_=false;
    QString restoreError_,mixerStatusCard_;
    QProcess capture_,playback_,inputList_,outputList_;
    QTimer limit_;
    QTimer discoveryLimit_;
    QTemporaryDir temporary_;
    QByteArray oddByte_;
    bool stopping_=false;
    bool relay_=false,relayApplying_=false,relayClosing_=false;
    void stopChecks();
    void measure();
    void updateButtons();
    void discoverDevices();
    void discoveryFinished(QProcess&,bool,int,QProcess::ExitStatus);
    void rebuildDevices();
    void synchronizeDevice();
    void updateGuidance();
    QString deviceLabel(const QString&,const QList<PcmDevice>&) const;
    void optimizeLevels();
    void finishOptimization();
    void finishMixer(bool keep);
};
