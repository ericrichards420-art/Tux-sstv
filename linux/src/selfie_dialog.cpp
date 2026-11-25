#include "selfie_dialog.h"
#include "imageview.h"
#include <QtWidgets>
#ifdef Q_OS_LINUX
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#endif

QList<SelfieCamera> discoverSelfieCameras(){
    QList<SelfieCamera> result;
#ifdef Q_OS_LINUX
    const auto entries=QDir("/dev").entryInfoList({"video[0-9]*"},QDir::Files|QDir::System,QDir::Name);
    for(const auto& entry:entries){
        if(!QRegularExpression("^video[0-9]+$").match(entry.fileName()).hasMatch())continue;
        QString name;QFile device(entry.absoluteFilePath());
        if(device.open(QIODevice::ReadOnly|QIODevice::Unbuffered)){
            v4l2_capability capabilities{};
            if(ioctl(device.handle(),VIDIOC_QUERYCAP,&capabilities)==0){
                const auto flags=capabilities.capabilities&V4L2_CAP_DEVICE_CAPS?capabilities.device_caps:capabilities.capabilities;
                if(!(flags&(V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_VIDEO_CAPTURE_MPLANE)))continue;
                name=QString::fromLocal8Bit(reinterpret_cast<const char*>(capabilities.card),sizeof(capabilities.card)).section(QChar(0),0,0).trimmed();
            }
        }
        if(name.isEmpty()){
            QFile label("/sys/class/video4linux/"+entry.fileName()+"/name");
            if(label.open(QIODevice::ReadOnly))name=QString::fromLocal8Bit(label.readAll()).trimmed();
        }
        if(name.isEmpty())name="Webcam";
        result.append({name+" ("+entry.absoluteFilePath()+")",entry.absoluteFilePath()});
    }
#endif
    return result;
}

SelfieDialog::SelfieDialog(QWidget* parent,Discovery discover):QDialog(parent),discover_(std::move(discover)){
    setObjectName("selfieDialog");setWindowTitle("Take selfie");resize(720,610);if(parent)setPalette(parent->palette());
    auto* root=new QVBoxLayout(this);auto* devices=new QHBoxLayout;
    devices->addWidget(new QLabel("Camera"));cameras_=new QComboBox;cameras_->setObjectName("selfieCamera");devices->addWidget(cameras_,1);
    auto* refresh=new QPushButton("Refresh");refresh->setObjectName("selfieRefresh");devices->addWidget(refresh);root->addLayout(devices);
    preview_=new ImageView("Opening webcam…");preview_->setObjectName("selfiePreview");root->addWidget(preview_,1);
    mirror_=new QCheckBox("Mirror photo");mirror_->setObjectName("selfieMirror");mirror_->setChecked(QSettings().value("camera/mirror",false).toBool());root->addWidget(mirror_);
    status_=new QLabel("Starting webcam…");status_->setObjectName("selfieStatus");status_->setWordWrap(true);status_->setMinimumHeight(status_->fontMetrics().lineSpacing()*2);root->addWidget(status_);
    auto* note=new QLabel("The camera turns off when you capture, cancel, or close this window.");note->setWordWrap(true);root->addWidget(note);
    auto* controls=new QDialogButtonBox;capture_=controls->addButton("Capture",QDialogButtonBox::ActionRole);capture_->setObjectName("selfieCapture");capture_->setEnabled(false);capture_->setDefault(true);
    retake_=controls->addButton("Retake",QDialogButtonBox::ActionRole);retake_->setObjectName("selfieRetake");retake_->setEnabled(false);
    use_=controls->addButton("Use photo",QDialogButtonBox::AcceptRole);use_->setObjectName("selfieUsePhoto");use_->setEnabled(false);controls->addButton(QDialogButtonBox::Cancel);root->addWidget(controls);
    connect(controls,&QDialogButtonBox::accepted,this,&SelfieDialog::accept);connect(controls,&QDialogButtonBox::rejected,this,&SelfieDialog::reject);
    connect(refresh,&QPushButton::clicked,this,&SelfieDialog::refreshCameras);connect(cameras_,&QComboBox::currentIndexChanged,this,[this]{startCamera();});connect(mirror_,&QCheckBox::toggled,this,[this]{updatePreview();});
    connect(capture_,&QPushButton::clicked,this,[this]{if(live_.isNull())return;frozen_=live_;stopCamera();capture_->setEnabled(false);retake_->setEnabled(true);use_->setEnabled(true);use_->setDefault(true);status_->setText("Photo captured · Use photo or Retake");updatePreview();});
    connect(retake_,&QPushButton::clicked,this,&SelfieDialog::startCamera);
    connect(&camera_,&QProcess::readyReadStandardOutput,this,&SelfieDialog::readFrames);
    connect(&camera_,&QProcess::readyReadStandardError,this,[this]{errors_=(errors_+QString::fromLocal8Bit(camera_.readAllStandardError())).right(2000);});
    connect(&camera_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(!stopping_&&!closed_&&error==QProcess::FailedToStart)fail("Webcam support could not start. Reinstall Tux-sstv with its camera dependencies.");});
    connect(&camera_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){
        if(stopping_||closed_||!frozen_.isNull())return;
        errors_=(errors_+QString::fromLocal8Bit(camera_.readAllStandardError())).right(2000);
        fail(errors_.contains("busy",Qt::CaseInsensitive)?"This webcam is busy. Close the other camera app, then click Refresh.":errors_.contains("Permission denied",Qt::CaseInsensitive)?"Cannot access this webcam. Check the camera privacy switch and permissions, then click Refresh.":"Could not use this webcam. Check its connection, then click Refresh or choose another camera.");
    });
    timeout_.setSingleShot(true);connect(&timeout_,&QTimer::timeout,this,[this]{fail("The webcam is not sending pictures. Check its connection/privacy shutter, then click Refresh.");});
    QTimer::singleShot(0,this,&SelfieDialog::refreshCameras);
}
SelfieDialog::~SelfieDialog(){closed_=true;stopCamera();}
QString SelfieDialog::device() const{return cameras_->currentData().toString();}
bool SelfieDialog::mirrored() const{return mirror_->isChecked();}
QImage SelfieDialog::photo() const{return mirrored()?frozen_.transformed(QTransform::fromScale(-1,1)):frozen_;}
void SelfieDialog::accept(){if(!frozen_.isNull())QDialog::accept();}
void SelfieDialog::done(int result){closed_=true;stopCamera();QDialog::done(result);}
void SelfieDialog::refreshCameras(){
    if(closed_)return;
    const auto previous=device().isEmpty()?QSettings().value("camera/device").toString():device();stopCamera();
    {const QSignalBlocker block(cameras_);cameras_->clear();for(const auto& camera:discover_())cameras_->addItem(camera.name,camera.device);const int index=cameras_->findData(previous);if(index>=0)cameras_->setCurrentIndex(index);}
    startCamera();
}
void SelfieDialog::startCamera(){
    if(closed_)return;
    stopCamera();live_={};frozen_={};errors_.clear();capture_->setEnabled(false);retake_->setEnabled(false);use_->setEnabled(false);capture_->setDefault(true);preview_->setImage({});preview_->setText("Opening webcam…");status_->setToolTip({});
    if(device().isEmpty()){fail("No webcam found. Connect a camera, then click Refresh.");return;}
    const auto helper=QStandardPaths::findExecutable("gst-launch-1.0");
    if(helper.isEmpty()){fail("Webcam support is unavailable. Reinstall Tux-sstv with its camera dependencies.");return;}
    status_->setText("Opening webcam…");
    camera_.start(helper,{"-q","v4l2src","device="+device(),"!","decodebin","!","videorate","!","videoconvert","!","videoscale","add-borders=true","!","video/x-raw,format=RGB,width=640,height=480,pixel-aspect-ratio=1/1,framerate=10/1","!","fdsink","fd=1","sync=false"});timeout_.start(10000);
}
void SelfieDialog::stopCamera(){
    stopping_=true;timeout_.stop();camera_.kill();if(camera_.state()!=QProcess::NotRunning)camera_.waitForFinished(500);if(camera_.isOpen())camera_.readAllStandardOutput();pending_.clear();stopping_=false;
}
void SelfieDialog::readFrames(){
    const auto bytes=camera_.readAllStandardOutput();if(stopping_||closed_||!frozen_.isNull())return;
    pending_+=bytes;const auto frames=pending_.size()/frameBytes;if(!frames)return;
    live_=QImage(reinterpret_cast<const uchar*>(pending_.constData()+(frames-1)*frameBytes),frameWidth,frameHeight,frameWidth*3,QImage::Format_RGB888).copy();pending_.remove(0,frames*frameBytes);
    timeout_.start(5000);capture_->setEnabled(true);status_->setText("Live preview · Capture when ready");updatePreview();
}
void SelfieDialog::updatePreview(){const auto image=frozen_.isNull()?live_:frozen_;if(!image.isNull())preview_->setImage(mirrored()?image.transformed(QTransform::fromScale(-1,1)):image);}
void SelfieDialog::fail(const QString& message){stopCamera();live_={};capture_->setEnabled(false);use_->setEnabled(false);preview_->setImage({});preview_->setText("No live camera picture");status_->setText(message);status_->setToolTip(errors_);}
