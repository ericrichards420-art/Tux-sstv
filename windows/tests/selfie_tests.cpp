#include "selfie_dialog.h"
#include "mainwindow.h"
#include "signalview.h"
#include <QtTest>
#include <QtWidgets>
#include <QScopeGuard>
#include <csignal>

class SelfieTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    QImage frame_,second_;
    template<class T> static T* child(QObject& parent,const char* name){return parent.findChild<T*>(name);}
    static void click(QObject& parent,const char* name){child<QPushButton>(parent,name)->click();}
    QList<SelfieCamera> cameras() const{return {{"Test webcam","/dev/mock0"},{"Second webcam","/dev/mock1"},{"Busy webcam","/dev/busy"}};}
    QList<qint64> pids(){QList<qint64> result;QFile file(directory_.filePath("camera-starts"));if(file.open(QIODevice::ReadOnly))for(const auto& line:file.readAll().split('\n'))if(!line.isEmpty())result<<line.split(' ').first().toLongLong();return result;}
    static bool running(qint64 pid){return pid>0&&::kill(pid,0)==0;}
    QStringList selfies(){return QDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)+"/OpenSSTV/Selfies").entryList({"*.png"},QDir::Files);}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVSelfieTests");QCoreApplication::setApplicationName("Selfie");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_SELFIE_TEST",directory_.path().toUtf8());
        frame_=QImage(640,480,QImage::Format_RGB888);frame_.fill(Qt::red);{QPainter p(&frame_);p.fillRect(320,0,320,480,Qt::blue);}second_=frame_.copy();second_.fill(Qt::green);
        for(const auto& fixture:{std::pair{QString("frame.rgb"),frame_},std::pair{QString("second.rgb"),second_}}){QFile file(directory_.filePath(fixture.first));QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(reinterpret_cast<const char*>(fixture.second.constBits()),fixture.second.sizeInBytes()),fixture.second.sizeInBytes());}
        const QByteArray script=R"(#!/usr/bin/python3
import os,sys,time
root=os.environ['OPENSSTV_SELFIE_TEST']
name=os.path.basename(sys.argv[0])
if name=='aplay':
    open(root+'/unexpected-playback','w').write('started');sys.exit(1)
if name=='arecord':
    while True: os.write(1,bytes(2400));time.sleep(.1)
device=next(arg[7:] for arg in sys.argv if arg.startswith('device='))
with open(root+'/camera-starts','a') as f:f.write(str(os.getpid())+' '+device+'\n')
with open(root+'/camera-args','w') as f:f.write('\n'.join(sys.argv[1:]))
if device.endswith('busy'):
    print('Device or resource busy',file=sys.stderr);sys.exit(1)
frame=open(root+('/second.rgb' if device.endswith('mock1') else '/frame.rgb'),'rb').read()
def send(data):
    while data:data=data[os.write(1,data):]
while True:
    send(frame[:7]);time.sleep(.005);send(frame[7:]);time.sleep(.07)
)";
        for(const auto* name:{"gst-launch-1.0","arecord","aplay"}){QFile file(directory_.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(script);file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
    }
    void init(){QSettings().clear();for(const auto* name:{"camera-starts","camera-args","unexpected-playback"})QFile::remove(directory_.filePath(name));}
    void noCameraCanRefreshAndCancel(){
        QList<SelfieCamera> available;SelfieDialog dialog(nullptr,[&]{return available;});dialog.show();QTRY_VERIFY(child<QLabel>(dialog,"selfieStatus")->text().contains("No webcam"));QVERIFY(!child<QPushButton>(dialog,"selfieCapture")->isEnabled());QVERIFY(pids().isEmpty());dialog.accept();QVERIFY(dialog.isVisible());
        available=cameras();click(dialog,"selfieRefresh");QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());QTRY_VERIFY(!pids().isEmpty());const auto pid=pids().last();dialog.reject();QVERIFY(!running(pid));QVERIFY(dialog.photo().isNull());
    }
    void closingBeforeStartupNeverOpensCamera(){
        SelfieDialog dialog(nullptr,[this]{return cameras();});dialog.reject();QTest::qWait(100);QVERIFY(pids().isEmpty());QVERIFY(dialog.photo().isNull());
    }
    void captureStopsCameraAndRetakeRestarts(){
        SelfieDialog dialog(nullptr,[this]{return cameras();});dialog.show();QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());QTRY_COMPARE(pids().size(),1);const auto first=pids().last();
        click(dialog,"selfieCapture");QCOMPARE(dialog.photo(),frame_);QVERIFY(!running(first));QVERIFY(child<QPushButton>(dialog,"selfieUsePhoto")->isEnabled());QVERIFY(!child<QPushButton>(dialog,"selfieCapture")->isEnabled());
        click(dialog,"selfieRetake");QVERIFY(dialog.photo().isNull());QVERIFY(!child<QPushButton>(dialog,"selfieUsePhoto")->isEnabled());QTRY_COMPARE(pids().size(),2);QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());QVERIFY(pids().last()!=first);const auto second=pids().last();dialog.reject();QVERIFY(!running(second));
        QFile args(directory_.filePath("camera-args"));QVERIFY(args.open(QIODevice::ReadOnly));const auto arguments=args.readAll();QVERIFY(arguments.contains("format=RGB,width=640,height=480"));QVERIFY(!arguments.contains("audio"));
    }
    void mirrorAndCameraChoice(){
        SelfieDialog dialog(nullptr,[this]{return cameras();});dialog.show();QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());child<QCheckBox>(dialog,"selfieMirror")->setChecked(true);click(dialog,"selfieCapture");
        QImage mirrored=frame_.copy();mirrored.fill(Qt::blue);{QPainter painter(&mirrored);painter.fillRect(320,0,320,480,Qt::red);}QCOMPARE(dialog.photo(),mirrored);
        child<QComboBox>(dialog,"selfieCamera")->setCurrentIndex(1);QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());click(dialog,"selfieCapture");QCOMPARE(dialog.photo(),second_);QCOMPARE(dialog.device(),QString("/dev/mock1"));click(dialog,"selfieUsePhoto");QCOMPARE(dialog.result(),int(QDialog::Accepted));QVERIFY(!running(pids().last()));
    }
    void busyCameraReportsErrorAndRecovers(){
        SelfieDialog dialog(nullptr,[this]{return cameras();});dialog.show();QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());const auto first=pids().last();child<QComboBox>(dialog,"selfieCamera")->setCurrentIndex(2);
        QTRY_VERIFY(child<QLabel>(dialog,"selfieStatus")->text().contains("busy"));QVERIFY(!running(first));QVERIFY(!child<QPushButton>(dialog,"selfieCapture")->isEnabled());QVERIFY(!child<QPushButton>(dialog,"selfieUsePhoto")->isEnabled());const int starts=pids().size();QTest::qWait(150);QCOMPARE(pids().size(),starts);
        child<QComboBox>(dialog,"selfieCamera")->setCurrentIndex(0);QTRY_VERIFY(child<QPushButton>(dialog,"selfieCapture")->isEnabled());dialog.reject();QVERIFY(!running(pids().last()));
    }
    void missingHelperIsClear(){
        QTemporaryDir empty;qputenv("PATH",empty.path().toUtf8());const auto restore=qScopeGuard([this]{qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);});
        SelfieDialog dialog(nullptr,[this]{return cameras();});dialog.show();QTRY_VERIFY(child<QLabel>(dialog,"selfieStatus")->text().contains("unavailable"));QVERIFY(pids().isEmpty());QVERIFY(!child<QPushButton>(dialog,"selfieCapture")->isEnabled());dialog.reject();
    }
    void mainWindowCaptureOrCancel_data(){QTest::addColumn<bool>("accept");QTest::newRow("use-photo")<<true;QTest::newRow("cancel-photo")<<false;}
    void mainWindowCaptureOrCancel(){
        if(!discoverSelfieCameras().isEmpty())QSKIP("Main-window simulation does not open a physical webcam");
        QFETCH(bool,accept);MainWindow window;window.resize(900,680);window.show();auto* button=child<QPushButton>(window,"takeSelfieButton");QVERIFY(button&&button->isVisible());QVERIFY(window.rect().contains(QRect(button->mapTo(&window,QPoint(0,0)),button->size())));
        const auto before=selfies();const auto count=child<QListWidget>(window,"stockPictures")->count();bool seen=false,captured=false;QTimer poll;poll.setInterval(20);
        connect(&poll,&QTimer::timeout,&window,[&]{auto* dialog=window.findChild<SelfieDialog*>();if(!dialog)return;seen=true;auto* combo=child<QComboBox>(*dialog,"selfieCamera");if(combo->count()==0){combo->addItem("Simulated webcam","/dev/mock0");return;}
            if(!captured&&child<QPushButton>(*dialog,"selfieCapture")->isEnabled()){click(*dialog,"selfieCapture");captured=true;return;}
            if(captured){poll.stop();if(accept)click(*dialog,"selfieUsePhoto");else dialog->reject();}
        });poll.start();QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        if(accept)button->click();else window.findChild<QAction*>("takeSelfieAction")->trigger();QVERIFY(seen&&captured);QVERIFY(!running(pids().last()));QVERIFY(!QFile::exists(directory_.filePath("unexpected-playback")));
        auto* stock=child<QListWidget>(window,"stockPictures");
        if(accept){QCOMPARE(selfies().size(),before.size()+1);QCOMPARE(stock->count(),count+1);const auto path=stock->item(0)->data(Qt::UserRole).toString();QCOMPARE(QImage(path).convertToFormat(QImage::Format_RGB888),frame_);QCOMPARE(QSettings().value("camera/device").toString(),QString("/dev/mock0"));QVERIFY(child<QLabel>(window,"statusLabel")->text().contains("Selfie saved"));}
        else {QCOMPARE(selfies(),before);QCOMPARE(stock->count(),count);QVERIFY(!QSettings().contains("camera/device"));}
        QTRY_VERIFY(child<SignalView>(window,"signalScope")->inputActive());click(window,"recordButton");QVERIFY(!button->isEnabled());QVERIFY(!window.findChild<QAction*>("takeSelfieAction")->isEnabled());click(window,"stopButton");window.close();
    }
    void realPipelineHasPackedRgbFrames(){
        const auto helper=QStandardPaths::findExecutable("gst-launch-1.0",QString::fromLocal8Bit(oldPath_).split(':'));if(helper.isEmpty())QSKIP("GStreamer is not installed");
        QProcess process;process.start(helper,{"-q","videotestsrc","num-buffers=1","pattern=red","!","decodebin","!","videorate","!","videoconvert","!","videoscale","add-borders=true","!","video/x-raw,format=RGB,width=640,height=480,pixel-aspect-ratio=1/1,framerate=10/1","!","fdsink","fd=1","sync=false"});QVERIFY(process.waitForFinished(5000));QCOMPARE(process.exitCode(),0);const auto bytes=process.readAllStandardOutput();QCOMPARE(bytes.size(),640*480*3);QCOMPARE(bytes.left(3),QByteArray::fromHex("ff0000"));QCOMPARE(bytes.right(3),QByteArray::fromHex("ff0000"));
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);SelfieTests tests;return QTest::qExec(&tests,argc,argv);}
#include "selfie_tests.moc"
