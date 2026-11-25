#include "core/spectrum.h"
#include "signalview.h"
#include "mainwindow.h"
#include "audio_wizard.h"
#include "selfie_dialog.h"
#include "core/wav.h"
#include "core/morse.h"
#include "skins.h"
#include "ui_refresh_probe.h"
#include <QtTest>
#include <QtWidgets>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>
#include <QScopeGuard>

namespace {
std::vector<float> tone(int rate,double seconds,double frequency,double amplitude=.5){
    std::vector<float> result(size_t(std::llround(rate*seconds)));for(size_t i=0;i<result.size();++i)result[i]=float(amplitude*std::sin(2*std::numbers::pi*frequency*i/rate));return result;
}
QByteArray pcm(const std::vector<float>& samples){QByteArray result(qsizetype(samples.size()*2),Qt::Uninitialized);for(size_t i=0;i<samples.size();++i)qToLittleEndian<qint16>(qint16(std::lround(samples[i]*32767)),result.data()+i*2);return result;}
double frequency(const sstv::SpectrumFrame& frame){return sstv::spectrumLowHz+double(std::max_element(frame.db.begin(),frame.db.end())-frame.db.begin())*(sstv::spectrumHighHz-sstv::spectrumLowHz)/(sstv::spectrumBins-1);}
}
class StationStatusRadio:public radio::Backend {
    int reads_=0;
public:
    void open(const radio::Config&) override{}
    QVariantMap read(bool=false) override{return {{"frequencyHz",14230000.},{"mode","USB"},{"ptt",0},{"strengthDb",-42+(reads_++%12)}};}
    void setPtt(bool) override{}
    void apply(const radio::Command&) override{}
    QString close() override{return {};}
};
class WaterfallTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    template<class T> static T* child(QObject& parent,const char* name){return parent.findChild<T*>(name);}
    static void click(QObject& parent,const char* name){child<QPushButton>(parent,name)->click();}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVWaterfallTests");QCoreApplication::setApplicationName("Waterfall");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_WATERFALL_TEST",directory_.path().toUtf8());
        const QByteArray script=R"(#!/usr/bin/python3
import os,sys,time,math,struct
root=os.environ['OPENSSTV_WATERFALL_TEST']
if os.path.basename(sys.argv[0])=='aplay':
    open(root+'/unexpected-playback','w').write('started')
    if sys.argv[sys.argv.index('-D')+1]=='monitor-output':
        time.sleep(.5)
        sys.exit(0)
    sys.exit(1)
if '-L' in sys.argv:
    print('default\n    Test microphone')
    sys.exit(0)
device=sys.argv[sys.argv.index('-D')+1]
with open(root+'/capture-attempts','a') as f: f.write(device+'\n')
if device=='broken' or (device=='recovering-input' and not os.path.exists(root+'/device-ready')):
    print('Selected device unavailable',file=sys.stderr)
    sys.exit(2)
open(root+'/input-open','w').write(device)
with open(root+'/input-opens','a') as f: f.write(device+'\n')
if device=='stop-receive-input' and not os.path.exists(root+'/stopped-session'):
    open(root+'/stopped-session','w').write('ready')
    while True: os.write(1,bytes(2400));time.sleep(.1)
if device=='morse-picture-input':
    data=open(root+'/picture.pcm','rb').read()+open(root+'/morse.pcm','rb').read()
    for start in range(0,len(data),2400):
        block=data[start:start+2400]
        while block:block=block[os.write(1,block):]
        time.sleep(.003)
    while True:os.write(1,bytes(2400));time.sleep(.1)
if device in ('separate-pictures','interrupted-picture'):
    picture=open(root+'/picture.pcm','rb').read()
    first=picture[:-40800] if device=='interrupted-picture' else picture
    def send(data):
        for start in range(0,len(data),2400):
            block=data[start:start+2400]
            while block: block=block[os.write(1,block):]
            time.sleep(.001)
    send(first)
    open(root+'/first-picture-sent','w').write('ready')
    while not os.path.exists(root+'/send-second-picture'): time.sleep(.01)
    if device=='separate-pictures': send(bytes(12000*2*4))
    send(picture[:12000]);time.sleep(.2);send(picture[12000:])
    while True: time.sleep(1)
if device=='stalled-once-input' and not os.path.exists(root+'/stalled-session'):
    open(root+'/stalled-session','w').write('yes')
    data=open(root+'/picture.pcm','rb').read()[:48000]
    while data:data=data[os.write(1,data):]
    while True:time.sleep(1)
if device=='silence-then-picture-input':
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:os.write(1,bytes(2400));time.sleep(.1)
if device=='disconnect-once-input' and not os.path.exists(root+'/disconnected'):
    open(root+'/disconnected','w').write('yes')
    os.write(1,open(root+'/picture.pcm','rb').read()[:12000]);time.sleep(.1)
    sys.exit(2)
if device in ('picture-input','auto-detect-input','recovering-input','disconnect-once-input','stop-receive-input','glitched-header-input','clipped-header-input','muted-header-input','stalled-once-input','silence-then-picture-input'):
    picture=open(root+('/glitched-picture.pcm' if device=='glitched-header-input' else '/picture.pcm'),'rb').read()
    if device in ('clipped-header-input','muted-header-input'):picture=picture[12000:]
    if device=='muted-header-input':picture=bytes(16800)+picture
    data=picture*2
    split=12000 if device=='picture-input' else 36000
    head=data[:split]
    while head: head=head[os.write(1,head):]
    open(root+'/header-started','w').write('ready')
    while not os.path.exists(root+'/continue-picture'): time.sleep(.01)
    for start in range(split,len(data),2400):
        block=data[start:start+2400]
        while block: block=block[os.write(1,block):]
        time.sleep(.003)
    while True: time.sleep(1)
pos=0
while True:
    data=bytes(2400) if device=='silent-input' else struct.pack('<1200h',*[int(12000*math.sin(2*math.pi*1900*(pos+i)/12000)) for i in range(1200)])
    os.write(1,data[:3]);time.sleep(.003);os.write(1,data[3:]);pos+=1200;time.sleep(.08)
)";
        for(const auto& name:{"arecord","aplay"}){QFile file(directory_.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(script);file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
        QFile picture(directory_.filePath("picture.pcm"));QVERIFY(picture.open(QIODevice::WriteOnly));picture.write(pcm(sstv::encode(sstv::testPattern(),*sstv::modeById("robot36"),12000).samples));
        auto glitched=sstv::encode(sstv::testPattern(),*sstv::modeById("robot36"),12000);std::fill(glitched.samples.begin()+7836,glitched.samples.begin()+7884,0.f);
        QFile glitch(directory_.filePath("glitched-picture.pcm"));QVERIFY(glitch.open(QIODevice::WriteOnly));glitch.write(pcm(glitched.samples));
    }
    void init(){QSettings().clear();for(const auto& file:{"input-open","input-opens","unexpected-playback","header-started","continue-picture","first-picture-sent","send-second-picture","capture-attempts","device-ready","disconnected","stopped-session"})QFile::remove(directory_.filePath(file));}
    int inputOpens(){QFile file(directory_.filePath("input-opens"));if(!file.open(QIODevice::ReadOnly))return 0;return file.readAll().count('\n');}
    void stationAudioStaysStillDuringCaptureAndRadioPolling(){
        const QString device="plughw:CARD=Kenwood_TS_990S_USB_AUDIO_CODEC,DEV=0";
        QSettings settings;settings.setValue("audio/input",device);settings.setValue("audio/output",device);
        radio::Config config;config.enabled=true;config.save(settings);
        MainWindow window(nullptr,std::make_unique<StationStatusRadio>());window.resize(1100,850);child<QTabWidget>(window,"imageTabs")->setCurrentIndex(0);window.show();
        auto* scope=child<SignalView>(window,"signalScope");auto* radio=child<radio::Controller>(window,"radioController");QTRY_VERIFY(scope->historyRows()>4);radio->connectRadio(config);QTRY_VERIFY(radio->connected());QTRY_VERIFY(!radio->busy());
        const auto rows=scope->historyRows();QTRY_VERIFY(scope->historyRows()>rows+1);
        auto* info=child<QLabel>(window,"recordingInfo");auto* input=child<QLineEdit>(window,"inputDevice");auto* output=child<QLineEdit>(window,"outputDevice");
        child<QScrollArea>(window,"operatingControlsScroll")->ensureWidgetVisible(info,0,0);QTest::qWait(30);QVERIFY(!info->visibleRegion().isEmpty());
        QVERIFY(input->isVisible());QVERIFY(!input->visibleRegion().isEmpty());const auto steady=info->text();
        const auto geometry=[&](QWidget* field){return QRect(field->mapTo(&window,QPoint{}),field->size());};const auto inputRect=geometry(input),outputRect=geometry(output),infoRect=geometry(info);
        UiRefreshProbe inputs,outputs,summary;input->installEventFilter(&inputs);output->installEventFilter(&outputs);info->installEventFilter(&summary);
        for(int i=0;i<6;++i){radio->refresh();QTRY_VERIFY(!radio->busy());QCOMPARE(info->text(),steady);const auto before=scope->historyRows();QTRY_VERIFY(scope->historyRows()>before);QCOMPARE(info->text(),steady);QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QCOMPARE(geometry(info),infoRect);}
        QCOMPARE(inputs.enabledChanges,0);QCOMPARE(outputs.enabledChanges,0);QCOMPARE(summary.paints,0);QVERIFY(info->text().contains("Automatic RX"));QVERIFY(info->text().contains("Hamlib"));
        click(window,"recordButton");QTRY_VERIFY(info->text().contains("Recording"));QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QCOMPARE(info->height(),infoRect.height());QVERIFY(info->toolTip().contains(device));
        click(window,"recordButton");QTRY_COMPARE(info->text(),steady);QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QVERIFY(scope->inputActive());QCOMPARE(inputOpens(),1);
        QApplication::processEvents();const auto preview=qEnvironmentVariable("OPENSSTV_STATION_AUDIO_PREVIEW");if(!preview.isEmpty())QVERIFY(window.grab().save(preview));window.close();
    }
    void automaticReceiveAndOptionalRecordingReuseInput(){
        QSettings().setValue("audio/input","working-input");MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");QVERIFY(!scope->inputActive());QTRY_VERIFY(scope->historyRows()>4);QVERIFY(scope->inputActive());QCOMPARE(inputOpens(),1);QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));QVERIFY(child<QPushButton>(window,"playButton")->isEnabled());QVERIFY(child<QLabel>(window,"recordingInfo")->text().contains("Automatic RX"));
        const auto preview=qEnvironmentVariable("OPENSSTV_LIVE_AUDIO_PREVIEW");if(!preview.isEmpty()){window.findChild<QAction*>("radioTabAction")->trigger();QApplication::processEvents();QVERIFY(window.grab().save(preview));}
        const auto rows=scope->historyRows();click(window,"recordButton");QCOMPARE(inputOpens(),1);QVERIFY(scope->historyRows()>=rows);QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Finish recording"));QTest::qWait(200);click(window,"recordButton");QTRY_COMPARE(inputOpens(),1);QTRY_VERIFY(scope->inputActive());QVERIFY(!child<QPushButton>(window,"liveAudioButton"));
        const auto recordings=QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recordings").entryInfoList({"*.wav"},QDir::Files,QDir::Time);QVERIFY(!recordings.isEmpty());const auto saved=sstv::readWav(recordings.first().absoluteFilePath());QVERIFY(std::any_of(saved.samples.begin(),saved.samples.end(),[](float value){return value!=0;}));
        click(window,"stopButton");QTRY_COMPARE(inputOpens(),2);QTRY_VERIFY(scope->inputActive());QVERIFY(!child<QPushButton>(window,"liveAudioButton"));QVERIFY(!QFile::exists(directory_.filePath("unexpected-playback")));window.close();
    }
    void automaticReceiveDecodesWithoutRecord(){
        const auto recordingDir=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recordings";const int wavs=QDir(recordingDir).entryList({"*.wav"},QDir::Files).size();QSettings().setValue("audio/input","picture-input");MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");QTRY_VERIFY(QFile::exists(directory_.filePath("header-started")));QTRY_VERIFY(scope->historyRows()>1);QVERIFY(!child<QPushButton>(window,"liveAudioButton"));QFile go(directory_.filePath("continue-picture"));QVERIFY(go.open(QIODevice::WriteOnly));go.write("go");go.close();QTRY_COMPARE_WITH_TIMEOUT(child<QListWidget>(window,"receiveHistory")->count(),2,8000);QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(window,"receiveInfo")->text().contains("240/240"),8000);QVERIFY(child<QLabel>(window,"receiveInfo")->text().contains("Robot 36"));QCOMPARE(QDir(recordingDir).entryList({"*.wav"},QDir::Files).size(),wavs);QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));QCOMPARE(inputOpens(),1);click(window,"stopButton");window.close();
    }
    void liveMorseCallsignNeedsConfirmation(){
        sstv::Audio cw;sstv::appendMorseId(cw,{"KI5BZO",20,800});cw.samples.insert(cw.samples.end(),12000,0.f);
        QFile morse(directory_.filePath("morse.pcm"));QVERIFY(morse.open(QIODevice::WriteOnly));morse.write(pcm(cw.samples));morse.close();
        QSettings().setValue("audio/input","morse-picture-input");MainWindow window;window.show();
        auto* text=child<QLineEdit>(window,"receivedMorseText");auto* use=child<QPushButton>(window,"useMorseCallsign");auto* call=child<QLabel>(window,"receivedCallsign");
        QTRY_VERIFY_WITH_TIMEOUT(text->text().contains("KI5BZO"),8000);QTRY_VERIFY_WITH_TIMEOUT(use->isEnabled(),8000);
        QVERIFY(text->isReadOnly());QVERIFY(!call->text().contains("KI5BZO"));
        const auto saved=child<QLabel>(window,"receiveAutoSaveInfo")->toolTip();QVERIFY(!saved.isEmpty());QVERIFY(QImage(saved).text("Callsign").isEmpty());
        use->click();QVERIFY(call->text().contains("KI5BZO"));QVERIFY(call->text().contains("Morse (confirmed)"));QVERIFY(child<QPushButton>(window,"replyReceived")->isEnabled());
        QCOMPARE(QImage(saved).text("Callsign"),QString("KI5BZO"));QCOMPARE(QImage(saved).text("CallsignSource"),QString("Morse (confirmed)"));
        QVERIFY(child<SignalView>(window,"signalScope")->inputActive());QCOMPARE(inputOpens(),1);QVERIFY(!QFile::exists(directory_.filePath("unexpected-playback")));window.close();
    }
    void automaticReceiveNeedsNoControls_data(){
        QTest::addColumn<QString>("scenario");
        for(const auto* scenario:{"startup","stop","temporary-error","capture-exit","header-glitch","clipped-header","muted-header"})QTest::newRow(scenario)<<QString(scenario);
    }
    void automaticReceiveNeedsNoControls(){
        QFETCH(QString,scenario);QSettings settings;
        settings.setValue("audio/input",scenario=="stop"?"stop-receive-input":scenario=="temporary-error"?"recovering-input":scenario=="capture-exit"?"disconnect-once-input":scenario=="header-glitch"?"glitched-header-input":scenario=="clipped-header"?"clipped-header-input":scenario=="muted-header"?"muted-header-input":"auto-detect-input");
        settings.setValue("rx/autoClock",true);settings.setValue("rx/autoFrequency",true);
        MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");
        auto* history=child<QListWidget>(window,"receiveHistory");const auto savedCount=[&]{int count=0;for(int i=0;i<history->count();++i)if(!history->item(i)->data(Qt::UserRole).toString().isEmpty())++count;return count;};const auto before=savedCount();
        QVERIFY(child<QComboBox>(window,"receiveMode")->currentData().toString().isEmpty());
        if(scenario=="stop"){
            QTRY_VERIFY(scope->historyRows()>2);click(window,"stopButton");
        }else if(scenario=="temporary-error"){
            QTRY_VERIFY(child<QLabel>(window,"liveAudioState")->text().contains("Retrying automatically"));
            QFile ready(directory_.filePath("device-ready"));QVERIFY(ready.open(QIODevice::WriteOnly));ready.write("ready");ready.close();
        }
        // No Auto selection, Record click, or device editingFinished event.
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("header-started")),5000);
        QTRY_COMPARE_WITH_TIMEOUT(child<QLabel>(window,"detectedReceiveMode")->property("modeId").toString(),QString("robot36"),3000);
        QTRY_VERIFY(child<QLabel>(window,"receiveInfo")->text().contains("partial"));
        QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));
        const int opened=inputOpens();QFile go(directory_.filePath("continue-picture"));QVERIFY(go.open(QIODevice::WriteOnly));go.write("go");go.close();
        QTRY_COMPARE_WITH_TIMEOUT(savedCount(),before+2,8000);QTRY_VERIFY(child<QLabel>(window,"receiveInfo")->text().contains("240/240"));
        QVERIFY(scope->inputActive());QCOMPARE(inputOpens(),opened);QVERIFY(!QFile::exists(directory_.filePath("unexpected-playback")));window.close();
    }
    void automaticReceiveRecoversFromStalledCapture_data(){
        QTest::addColumn<QString>("device");QTest::addColumn<int>("expectedOpens");
        QTest::newRow("live-process-stops-sending")<<QString("stalled-once-input")<<2;
        QTest::newRow("silent-audio-still-arrives")<<QString("silence-then-picture-input")<<1;
    }
    void automaticReceiveRecoversFromStalledCapture(){
        QFETCH(QString,device);QFETCH(int,expectedOpens);QFile::remove(directory_.filePath("stalled-session"));
        QSettings().setValue("audio/input",device);MainWindow window;window.show();
        auto* history=child<QListWidget>(window,"receiveHistory");const auto savedCount=[&]{int count=0;for(int i=0;i<history->count();++i)if(!history->item(i)->data(Qt::UserRole).toString().isEmpty())++count;return count;};const int before=savedCount();
        if(expectedOpens==2)QTRY_COMPARE_WITH_TIMEOUT(child<QLabel>(window,"detectedReceiveMode")->property("modeId").toString(),QString("robot36"),4000);
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("header-started")),18000);
        QCOMPARE(inputOpens(),expectedOpens);
        QFile go(directory_.filePath("continue-picture"));QVERIFY(go.open(QIODevice::WriteOnly));go.write("go");go.close();
        QTRY_VERIFY_WITH_TIMEOUT(savedCount()>=before+2,8000);QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(window,"receiveInfo")->text().contains("240/240"),8000);
        QVERIFY(child<SignalView>(window,"signalScope")->inputActive());QCOMPARE(inputOpens(),expectedOpens);
        QVERIFY(!QFile::exists(directory_.filePath("unexpected-playback")));window.close();
    }
    void missingCaptureHelperAutomaticallyRecovers(){
        QSettings().setValue("audio/input","auto-detect-input");QTemporaryDir empty;const auto path=qgetenv("PATH");qputenv("PATH",empty.path().toUtf8());
        const auto restore=qScopeGuard([&]{qputenv("PATH",path);});MainWindow window;window.show();
        QTRY_VERIFY(child<QLabel>(window,"liveAudioState")->text().contains("Cannot start arecord"));qputenv("PATH",path);
        QTRY_COMPARE_WITH_TIMEOUT(child<QLabel>(window,"detectedReceiveMode")->property("modeId").toString(),QString("robot36"),5000);
        QVERIFY(child<SignalView>(window,"signalScope")->inputActive());QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));window.close();
    }
    void separateTransmissionsAutomaticallyRearm_data(){
        QTest::addColumn<bool>("interrupted");QTest::newRow("complete-then-gap")<<false;QTest::newRow("partial-then-fresh-header")<<true;
    }
    void separateTransmissionsAutomaticallyRearm(){
        QFETCH(bool,interrupted);QSettings settings;settings.setValue("audio/input",interrupted?"interrupted-picture":"separate-pictures");settings.setValue("rx/autoClock",true);settings.setValue("rx/autoFrequency",true);
        MainWindow window;window.show();auto* history=child<QListWidget>(window,"receiveHistory");
        const auto paths=[&]{QSet<QString> result;for(int i=0;i<history->count();++i){const auto path=history->item(i)->data(Qt::UserRole).toString();if(!path.isEmpty())result.insert(path);}return result;};
        const auto before=paths();QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("first-picture-sent")),4000);QTRY_COMPARE_WITH_TIMEOUT(paths().size(),before.size()+1,5000);
        QTRY_VERIFY(child<QLabel>(window,"receiveInfo")->text().contains(interrupted?"partial":"complete"));
        const auto firstPath=*(paths()-before).begin();QFile first(firstPath);QVERIFY(first.open(QIODevice::ReadOnly));const auto firstBytes=first.readAll();first.close();
        if(!interrupted){QTRY_VERIFY(child<QLabel>(window,"statusLabel")->text().contains("Ready for next transmission"));QVERIFY(child<QLabel>(window,"detectedReceiveMode")->property("modeId").toString().isEmpty());}
        QFile next(directory_.filePath("send-second-picture"));QVERIFY(next.open(QIODevice::WriteOnly));next.write("go");next.close();
        QTRY_COMPARE_WITH_TIMEOUT(paths().size(),before.size()+2,6000);QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(window,"receiveInfo")->text().contains("240/240")&&child<QLabel>(window,"receiveInfo")->text().contains("complete"),6000);
        QTRY_VERIFY(child<QLabel>(window,"statusLabel")->text().contains("Ready for next transmission"));QVERIFY(child<QLabel>(window,"detectedReceiveMode")->property("modeId").toString().isEmpty());
        QCOMPARE(inputOpens(),1);QVERIFY(QFile::exists(firstPath));if(!interrupted){QVERIFY(first.open(QIODevice::ReadOnly));QCOMPARE(first.readAll(),firstBytes);}
        QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));QVERIFY(child<SignalView>(window,"signalScope")->inputActive());window.close();
    }
    void automaticReceiveShowsSilenceAndRecoversAfterDeviceChange(){
        QSettings().setValue("audio/input","silent-input");MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");QTRY_VERIFY(scope->historyRows()>2);QTRY_VERIFY(scope->property("inputSilent").toBool());QVERIFY(child<QLabel>(window,"liveAudioState")->text().contains("silence"));auto* input=child<QLineEdit>(window,"inputDevice");input->setText("working-input");QMetaObject::invokeMethod(input,"editingFinished");QTRY_COMPARE(inputOpens(),2);QTRY_VERIFY(scope->historyRows()>3);QVERIFY(!scope->property("inputSilent").toBool());QVERIFY(!child<QLabel>(window,"liveAudioState")->text().contains("silence"));window.close();
    }
    void recordingChangesPreserveAutomaticHeaderAndReceiver(){
        QSettings().setValue("audio/input","picture-input");MainWindow window;window.show();
        const int before=child<QListWidget>(window,"receiveHistory")->count();
        QTRY_VERIFY(QFile::exists(directory_.filePath("header-started")));QTest::qWait(1200);
        QTRY_VERIFY(child<QPushButton>(window,"recordButton")->isEnabled());click(window,"recordButton");
        QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Finish recording"));click(window,"recordButton");
        QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));QCOMPARE(inputOpens(),1);
        QFile go(directory_.filePath("continue-picture"));QVERIFY(go.open(QIODevice::WriteOnly));go.write("go");go.close();
        QTRY_COMPARE_WITH_TIMEOUT(child<QListWidget>(window,"receiveHistory")->count(),before+2,8000);
        QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(window,"receiveInfo")->text().contains("240/240"),8000);
        QCOMPARE(inputOpens(),1);window.close();
    }
    void automaticReceivePausesForPlaybackAndResumes(){
        QSettings().setValue("audio/output","monitor-output");MainWindow window;window.show();auto* mode=child<QComboBox>(window,"transmitMode");mode->setCurrentIndex(mode->findData("bw8"));auto* scope=child<SignalView>(window,"signalScope");QTRY_VERIFY(scope->historyRows()>3);click(window,"playButton");QTRY_VERIFY(QFile::exists(directory_.filePath("unexpected-playback")));QVERIFY(!scope->inputActive());QVERIFY(!child<QPushButton>(window,"liveAudioButton"));QTRY_VERIFY(scope->inputActive());QTRY_COMPARE(inputOpens(),2);window.close();
    }
    void pictureDialogsKeepLocalReceiveRunning_data(){
        QTest::addColumn<bool>("selfie");QTest::newRow("selfie")<<true;QTest::newRow("screenshots")<<false;
    }
    void pictureDialogsKeepLocalReceiveRunning(){
        QFETCH(bool,selfie);if(selfie&&!discoverSelfieCameras().isEmpty())QSKIP("Do not open a physical webcam during the receive test");
        QSettings().setValue("audio/input","auto-detect-input");MainWindow window;window.show();
        auto* history=child<QListWidget>(window,"receiveHistory");const auto savedCount=[&]{int count=0;for(int i=0;i<history->count();++i)if(!history->item(i)->data(Qt::UserRole).toString().isEmpty())++count;return count;};const auto before=savedCount();
        auto* scope=child<SignalView>(window,"signalScope");QTRY_VERIFY(QFile::exists(directory_.filePath("header-started")));QTRY_COMPARE(child<QLabel>(window,"detectedReceiveMode")->property("modeId").toString(),QString("robot36"));const auto opened=inputOpens();bool checked=false;
        QTimer::singleShot(100,&window,[&]{
            auto* dialog=selfie?static_cast<QDialog*>(window.findChild<SelfieDialog*>()):static_cast<QDialog*>(window.findChild<QFileDialog*>("loadScreenshotDialog"));QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});
            QVERIFY(scope->inputActive());QFile go(directory_.filePath("continue-picture"));QVERIFY(go.open(QIODevice::WriteOnly));go.write("go");go.close();
            QTRY_COMPARE_WITH_TIMEOUT(savedCount(),before+2,8000);QVERIFY(scope->inputActive());QCOMPARE(inputOpens(),opened);checked=true;
        });
        window.findChild<QAction*>(selfie?"takeSelfieAction":"loadScreenshotAction")->trigger();
        QVERIFY(checked);QVERIFY(scope->inputActive());QCOMPARE(inputOpens(),opened);window.close();
    }
    void automaticReceivePausesForWizardAndRestarts(){
        MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");
        QTRY_VERIFY(scope->historyRows()>3);QVERIFY(child<QPushButton>(window,"audioLevelDevicesButton")->isEnabled());bool seen=false;
        QTimer::singleShot(0,&window,[&]{
            auto* setup=window.findChild<QWizard*>("setupWizard");QVERIFY(setup);const auto close=qScopeGuard([setup]{setup->reject();});
            QVERIFY(scope->inputActive());setup->next();QCOMPARE(setup->currentId(),1);
            QTimer::singleShot(50,&window,[&]{auto* audio=window.findChild<AudioWizard*>();QVERIFY(audio);const auto close=qScopeGuard([audio]{audio->reject();});QVERIFY(!scope->inputActive());seen=true;});
            setup->findChild<QPushButton*>("setupConfigureAudio")->click();
        });click(window,"audioLevelDevicesButton");QVERIFY(seen);QTRY_COMPARE(inputOpens(),2);QTRY_VERIFY(scope->inputActive());window.close();
    }
    void automaticRetriesBackOffAndCloseStopsThem(){
        QSettings settings;settings.setValue("audio/liveMonitorAtStartup",false);settings.setValue("audio/input","broken");MainWindow window;window.show();
        const auto attempts=[&]{QFile file(directory_.filePath("capture-attempts"));return file.open(QIODevice::ReadOnly)?file.readAll().count('\n'):0;};
        QTRY_VERIFY(child<QLabel>(window,"liveAudioState")->text().contains("Selected device unavailable"));
        QTRY_COMPARE_WITH_TIMEOUT(attempts(),2,2000);QTest::qWait(400);QCOMPARE(attempts(),2);
        QVERIFY(!window.findChild<QAction*>("liveAudioStartupAction"));QVERIFY(!child<QPushButton>(window,"liveAudioButton"));
        window.close();const int before=attempts();QTest::qWait(2200);QCOMPARE(attempts(),before);QVERIFY(!child<SignalView>(window,"signalScope")->inputActive());
    }
    void calibratedTones_data(){QTest::addColumn<int>("rate");for(int rate:{8000,11025,12000,44100,48000,96000})QTest::newRow(qPrintable(QString::number(rate)))<<rate;}
    void calibratedTones(){
        QFETCH(int,rate);const auto samples=tone(rate,.5,1900);const auto frame=sstv::SpectrumAnalyzer::tail(samples,rate);QVERIFY(std::abs(frequency(frame)-1900)<10);const auto db=*std::max_element(frame.db.begin(),frame.db.end());QVERIFY2(std::abs(db+6.0206)<1.5,qPrintable(QString("Peak amplitude: %1 dBFS").arg(db)));QVERIFY(frame.peak<=.501&&frame.peak>.49);
        sstv::SpectrumAnalyzer stream;int frames=0;stream.push(samples,rate,[&](const auto& row){++frames;QVERIFY(std::abs(frequency(row)-1900)<10);});QVERIFY(frames>=5);
    }
    void silenceAndTwoTones(){
        const auto silent=sstv::SpectrumAnalyzer::tail(std::vector<float>(12000),12000);for(float db:silent.db)QCOMPARE(db,-90.f);QCOMPARE(silent.peak,0.);
        auto samples=tone(12000,.5,1200,.25);const auto other=tone(12000,.5,2300,.25);for(size_t i=0;i<samples.size();++i)samples[i]+=other[i];const auto frame=sstv::SpectrumAnalyzer::tail(samples,12000);
        const auto at=[&](double hz){const auto index=int(std::lround((hz-500)/2500*255));return *std::max_element(frame.db.begin()+index-1,frame.db.begin()+index+2);};QVERIFY(std::abs(at(1200)+12.0412)<1.5);QVERIFY(std::abs(at(2300)+12.0412)<1.5);QVERIFY(frame.db.front()< -65);
    }
    void chunkTimingAndRateReset(){
        auto samples=tone(12000,2,1200);const auto next=tone(12000,2,2300);samples.insert(samples.end(),next.begin(),next.end());sstv::SpectrumAnalyzer whole,chunked;std::vector<sstv::SpectrumFrame> a,b;whole.push(samples,12000,[&](const auto& frame){a.push_back(frame);});
        for(size_t pos=0;pos<samples.size();pos+=997)chunked.push(std::span<const float>(samples).subspan(pos,std::min(size_t(997),samples.size()-pos)),12000,[&](const auto& frame){b.push_back(frame);});QCOMPARE(a.size(),b.size());QVERIFY(a.size()>70);
        for(size_t i=0;i<a.size();++i){QVERIFY(a[i].db==b[i].db);QCOMPARE(a[i].endSeconds,b[i].endSeconds);if(i)QVERIFY(std::abs(a[i].endSeconds-a[i-1].endSeconds-.05)<1e-10);}QVERIFY(std::abs(frequency(a.front())-1200)<10);QVERIFY(std::abs(frequency(a.back())-2300)<10);
        int frames=0;chunked.push(tone(48000,.05,1500),48000,[&](const auto&){++frames;});QCOMPARE(frames,0);QVERIFY_EXCEPTION_THROWN(chunked.push({},1000,[](const auto&){}),std::runtime_error);QVERIFY_EXCEPTION_THROWN(sstv::SpectrumAnalyzer::tail({std::array<float,1>{std::numeric_limits<float>::quiet_NaN()}},12000),std::runtime_error);
    }
    void frozenDisplayAndBoundedHistory(){
        SignalView view;view.resize(500,200);view.show();view.beginInput();const auto input=pcm(tone(12000,1,1900));for(int i=0;i<40;++i)view.pushPcm(input);QCOMPARE(view.historyRows(),SignalView::maxRows);QVERIFY(view.inputActive());view.setFrozen(true);const auto frozen=view.grab().toImage();view.pushPcm(pcm(tone(12000,1,1200)));QCOMPARE(view.grab().toImage(),frozen);view.setFrozen(false);QVERIFY(view.grab().toImage()!=frozen);view.clearHistory();QCOMPARE(view.historyRows(),size_t(0));QVERIFY(view.inputActive());
        view.pushPcm(input.left(3));view.pushPcm(input.mid(3));QVERIFY(view.historyRows()>0);view.endInput();QVERIFY(!view.inputActive());QVERIFY(view.historyRows()>0);view.setSamples(tone(48000,2,2300),48000);QVERIFY(view.historyRows()<30);view.beginInput();QCOMPARE(view.historyRows(),size_t(0));QVERIFY(!view.frozen());
    }
    void liveApplicationAndRadioNavigation(){
        QSettings settings;settings.setValue("audio/input","fake-microphone");settings.setValue("ui/skin","synthwave");MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");QVERIFY(scope);QVERIFY(scope->waterfall());QTRY_VERIFY(scope->historyRows()>=5);QVERIFY(!child<QPushButton>(window,"liveAudioButton"));
        click(window,"recordButton");QTRY_VERIFY(scope->historyRows()>=5);QVERIFY(scope->inputActive());auto* freeze=child<QPushButton>(window,"audioMonitorFreeze");freeze->click();QVERIFY(scope->frozen());const auto count=scope->historyRows();QTRY_VERIFY(scope->historyRows()>count+5);QVERIFY(child<QPushButton>(window,"recordButton")->text().contains("Finish"));freeze->click();
        auto* tabs=child<QTabWidget>(window,"imageTabs");for(int index:{3,1,3,2,0,3}){tabs->setCurrentIndex(index);QVERIFY(scope->isVisible());QVERIFY(child<QWidget>(window,index==3?"radioFrontend":"operatingControls")->isAncestorOf(scope));}QCOMPARE(window.findChildren<SignalView*>().size(),1);
        const auto screenshot=qEnvironmentVariable("OPENSSTV_WATERFALL_PREVIEWS");if(!screenshot.isEmpty()){
            QVERIFY(QDir().mkpath(screenshot));std::vector<float> fixture(12000*31);double phase=0;for(size_t i=0;i<fixture.size();++i){const double hz=1200+1100*std::fmod(double(i)/12000.,4.)/4.;phase+=2*std::numbers::pi*hz/12000;fixture[i]=float(.5*std::sin(phase));}scope->clearHistory();scope->pushPcm(pcm(fixture));QApplication::processEvents();QVERIFY(window.grab().save(screenshot+"/radio-waterfall.png"));tabs->setCurrentIndex(0);QApplication::processEvents();QVERIFY(window.grab().save(screenshot+"/rx-waterfall.png"));scope->setWaterfall(false);QVERIFY(window.grab().save(screenshot+"/rx-spectrum.png"));scope->setWaterfall(true);
        }
        click(window,"stopButton");QTRY_VERIFY(scope->inputActive());QTRY_VERIFY(child<QPushButton>(window,"playButton")->isEnabled());tabs->setCurrentIndex(3);click(window,"audioMonitorClear");QCOMPARE(scope->historyRows(),size_t(0));QTRY_VERIFY(scope->historyRows()>3);QVERIFY(!child<QPushButton>(window,"audioLevelListen")->isEnabled());
        auto* choice=child<QComboBox>(window,"audioMonitorView");choice->setCurrentIndex(1);QVERIFY(!scope->waterfall());QCOMPARE(settings.value("ui/audioView").toString(),QString("spectrum"));QVERIFY(!QFile::exists(directory_.filePath("unexpected-playback")));window.close();MainWindow restored;QVERIFY(!child<SignalView>(restored,"signalScope")->waterfall());restored.close();
    }
    void minimumWindowKeepsMonitorAndTransportVisible(){
        MainWindow window;window.resize(900,680);window.show();QApplication::processEvents();auto* scroll=child<QScrollArea>(window,"operatingControlsScroll");auto* scope=child<SignalView>(window,"signalScope");scroll->ensureWidgetVisible(scope);QApplication::processEvents();
        const QRect visible(scroll->viewport()->mapToGlobal(QPoint(0,0)),scroll->viewport()->size()),monitor(scope->mapToGlobal(QPoint(0,0)),scope->size());QVERIFY(visible.contains(monitor));
        for(const auto* name:{"recordButton","stopButton"}){auto* button=child<QPushButton>(window,name);QVERIFY(window.rect().contains(QRect(button->mapTo(&window,QPoint(0,0)),button->size())));QVERIFY(button->isVisible());}window.close();
    }
    void unavailableInputAndNewSession(){
        QSettings().setValue("audio/input","broken");MainWindow window;window.show();auto* scope=child<SignalView>(window,"signalScope");QTRY_VERIFY(child<QLabel>(window,"liveAudioState")->text().contains("Selected device unavailable"));QTRY_VERIFY(!scope->inputActive());QCOMPARE(scope->historyRows(),size_t(0));QVERIFY(child<QPushButton>(window,"playButton")->isEnabled());child<QLineEdit>(window,"inputDevice")->setText("working-input");click(window,"recordButton");QTRY_VERIFY(scope->historyRows()>4);click(window,"audioMonitorFreeze");QVERIFY(scope->frozen());click(window,"stopButton");QTRY_VERIFY(child<QPushButton>(window,"recordButton")->isEnabled());click(window,"recordButton");QVERIFY(!child<QPushButton>(window,"audioMonitorFreeze")->isChecked());QTRY_VERIFY(scope->inputActive());QTRY_VERIFY(scope->historyRows()>4);window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);WaterfallTests tests;return QTest::qExec(&tests,argc,argv);}
#include "waterfall_tests.moc"
