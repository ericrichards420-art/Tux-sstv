#pragma once
#include <QDialog>
#include <QImage>
#include <QProcess>
#include <QTimer>
#include <functional>
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class ImageView;

struct SelfieCamera {QString name,device;};
QList<SelfieCamera> discoverSelfieCameras();

class SelfieDialog:public QDialog {
    Q_OBJECT
public:
    using Discovery=std::function<QList<SelfieCamera>()>;
    explicit SelfieDialog(QWidget* parent=nullptr,Discovery discover=discoverSelfieCameras);
    ~SelfieDialog() override;
    QImage photo() const;
    QString device() const;
    bool mirrored() const;
    void accept() override;
    void done(int result) override;
private:
    static constexpr int frameWidth=640,frameHeight=480,frameBytes=frameWidth*frameHeight*3;
    Discovery discover_;
    QComboBox* cameras_;
    QCheckBox* mirror_;
    QPushButton *capture_,*retake_,*use_;
    QLabel* status_;
    ImageView* preview_;
    QProcess camera_;
    QTimer timeout_;
    QByteArray pending_;
    QString errors_;
    QImage live_,frozen_;
    bool stopping_=false,closed_=false;
    void refreshCameras();
    void startCamera();
    void stopCamera();
    void readFrames();
    void updatePreview();
    void fail(const QString& message);
};
