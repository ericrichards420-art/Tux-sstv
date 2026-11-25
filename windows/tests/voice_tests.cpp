#include "mainwindow.h"
#include "voice_audio.h"
#include "mic_check_dialog.h"
#include "radio_ui.h"
#include "signalview.h"
#include "ts990_test.h"
#include "remote_test_transport.h"
#include "ui_refresh_probe.h"
#include "selfie_dialog.h"
#include <QtWidgets>
#include <QtTest>
#include <QtEndian>
#include <QMessageAuthenticationCode>

struct VoiceRadioState {std::atomic_bool keyed=false,failOff=false;std::atomic_int delay=0,disconnectDelay=0;};
class VoiceRadio:public radio::Backend {
    std::shared_ptr<VoiceRadioState> state_;
public:
    explicit VoiceRadio(std::shared_ptr<VoiceRadioState> state):state_(std::move(state)){}
    void open(const radio::Config&) override{}
    QVariantMap read(bool=false) override{return {{"frequencyHz",145550000.},{"mode","FM"},{"ptt",state_->keyed?1:0}};}
    void setPtt(bool on) override{if(on&&state_->delay)QThread::msleep(state_->delay);if(!on&&state_->failOff)throw std::runtime_error("PTT OFF not confirmed");state_->keyed=on;}
    void apply(const radio::Command&) override{}
    QString close() override{if(state_->disconnectDelay)QThread::msleep(state_->disconnectDelay);state_->keyed=false;return {};}
};
class VoiceTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    const QString key_="0123456789abcdef0123456789abcdef";
    template<class T> static T* child(MainWindow& w,const char* name){return w.findChild<T*>(name);}
    static radio::Controller* radio(MainWindow& w){return child<radio::Controller>(w,"radioController");}
    static QPushButton* ptt(MainWindow& w){return child<QPushButton>(w,"voicePttButton");}
    static void lockPtt(MainWindow& w){ptt(w)->click();}
    static void unlockPtt(MainWindow& w){ptt(w)->click();}
    remote::Config config(remote::Role role){remote::Config c;c.role=role;c.key=key_;c.host="shack.invalid";c.reconnect=false;return c;}
    void pair(remote::Server& server,remote::Client& client){auto [a,b]=remoteTestPair();server.acceptConnection(a);client.connectDevice(b,key_);}
    void bridge(MainWindow& shack,MainWindow& laptop){pair(*shack.findChild<remote::Server*>(),*laptop.findChild<remote::Client*>());}
    void configure(const QString& name,bool cat=true){QSettings s;s.setValue("remote/role",0);s.setValue("audio/input",name+"-input");s.setValue("audio/output",name+"-output");s.setValue("voice/microphone","voice-mic");s.setValue("voice/gain",100);radio::Config c;c.enabled=cat;c.leadMs=20;c.tailMs=20;c.save(s);}
    QByteArray read(const QString& name){QFile f(directory_.filePath(name));return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
private slots:
    void remoteTs990ModeAndMicrophoneUseRealDataCatCommands(){
        Ts990Emulator emulator;QVERIFY(emulator.start().isEmpty());configure("shack");{QSettings settings;emulator.config.save(settings);}MainWindow shack;shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio(emulator.config);QTRY_VERIFY(!radio(shack)->busy());QVERIFY2(radio(shack)->connected(),qPrintable(radio(shack)->error()+"\n"+emulator.log()));
        configure("laptop");MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(radio(laptop)->connected());auto* modes=child<QComboBox>(laptop,"radioFrontendMode");QVERIFY(modes->findText("USB-D1")>=0);QVERIFY(modes->findText("LSB-D3")>=0);QVERIFY(child<QComboBox>(laptop,"radioFrontendVfo")->findText("Main")>=0);QVERIFY(!child<QPushButton>(laptop,"radioFrontendMemoryVfo")->isEnabled());QVERIFY(!child<QSpinBox>(laptop,"radioFrontendBandwidth")->isEnabled());
        child<QPushButton>(laptop,"radioFrontendAdvanced")->click();auto* advanced=laptop.findChild<RadioControls*>();QVERIFY(advanced);QVERIFY(!advanced->findChild<QSpinBox*>("radioPassband")->isEnabled());QVERIFY(!advanced->findChild<QPushButton*>("radioSetRepeaterShift")->isEnabled());QVERIFY(!advanced->findChild<QPushButton*>("radioSetTone")->isEnabled());QCOMPARE(advanced->findChild<QComboBox*>("radioSplit")->count(),2);advanced->close();
        QTRY_VERIFY(child<QPushButton>(laptop,"radioFrontendSetMode")->isEnabled());modes->setCurrentText("USB-D1");QMetaObject::invokeMethod(modes,"activated",Qt::DirectConnection,Q_ARG(int,modes->currentIndex()));QTRY_COMPARE(radio(laptop)->state().value("mode").toString(),QString("USB-D1"));QVERIFY(emulator.log().contains("OM0D\n"));QVERIFY(!emulator.log().contains("TX"));
        QTRY_VERIFY(ptt(laptop)->isEnabled());lockPtt(laptop);QTRY_VERIFY(emulator.log().contains("TX1\n"));QTRY_VERIFY(read("voice.raw").size()>=1920);QVERIFY(read("output.txt").contains("shack-output"));QCOMPARE(qFromLittleEndian<qint16>(read("voice.raw").constData()),qint16(1000));unlockPtt(laptop);QTRY_VERIFY(emulator.log().contains("RX\n"));QTRY_VERIFY(!ptt(laptop)->isChecked());laptop.close();shack.close();
    }
    void radioDisconnectStopsSpeakersBeforeReplyAndUntilReconnect_data(){
        QTest::addColumn<bool>("frontPanel");QTest::newRow("menu-and-shack-reconnect")<<false;QTest::newRow("front-panel-and-laptop-reconnect")<<true;
    }
    void radioDisconnectStopsSpeakersBeforeReplyAndUntilReconnect(){
        QFETCH(bool,frontPanel);configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());
        configure("laptop");QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/listenToShack",true);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();auto* speaker=laptop.findChild<RemoteSpeaker*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(radio(laptop)->connected());QTRY_VERIFY(read("speaker.raw").size()>=480);QVERIFY(speaker->active());
        const auto opens=read("output.txt").count("laptop-speaker");state->disconnectDelay=250;
        if(frontPanel)child<QPushButton>(laptop,"radioFrontendDisconnect")->click();else child<QAction>(laptop,"radioDisconnectAction")->trigger();
        QVERIFY(!speaker->active());const auto bytes=read("speaker.raw").size();QTest::qWait(60);QVERIFY(!speaker->active());QVERIFY(client->connected());QVERIFY(child<QCheckBox>(laptop,"listenToShack")->isChecked());QCOMPARE(read("output.txt").count("laptop-speaker"),opens);
        QTRY_VERIFY(!radio(laptop)->connected());QTRY_VERIFY(!radio(laptop)->busy());QTRY_VERIFY(client->snapshot().value("receiving").toBool());auto* scope=child<SignalView>(laptop,"signalScope");const auto rows=scope->historyRows();QTRY_VERIFY(scope->historyRows()>rows+2);QVERIFY(!speaker->active());QCOMPARE(read("speaker.raw").size(),bytes);QCOMPARE(read("output.txt").count("laptop-speaker"),opens);QVERIFY(!state->keyed.load());
        state->disconnectDelay=0;if(frontPanel){QTRY_VERIFY(child<QPushButton>(laptop,"radioFrontendConnect")->isEnabled());child<QPushButton>(laptop,"radioFrontendConnect")->click();}else radio(shack)->connectRadio({});
        QTRY_VERIFY(radio(laptop)->connected());QTRY_VERIFY(speaker->active());QTRY_VERIFY(read("speaker.raw").size()>bytes);QVERIFY(!state->keyed.load());client->disconnectFromHost();QVERIFY(!speaker->active());laptop.close();shack.close();
    }
    void configuredRadioStateGatesSpeakersButAudioOnlyStillWorks(){
        configure("laptop",false);QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/listenToShack",true);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));remote::Server server;server.start(0,key_);
        QJsonObject state{{"radioEnabled",true},{"radioConnected",true},{"radioPttActive",false},{"radio",QJsonObject{{"ptt",0}}},{"receiving",true},{"transmitting",false},{"preparing",false},{"input","shack-input"},{"output","shack-output"}};server.setSnapshot(state);auto* client=laptop.findChild<remote::Client*>();pair(server,*client);QTRY_VERIFY(client->connected());auto* speaker=laptop.findChild<RemoteSpeaker*>();QTRY_VERIFY(speaker->active());
        state["radioConnected"]=false;server.setSnapshot(state);QTRY_VERIFY(!speaker->active());QVERIFY(child<QLabel>(laptop,"remoteSpeakerStatus")->text().contains("radio"));QVERIFY(client->connected());state["radioEnabled"]=false;server.setSnapshot(state);QTRY_VERIFY(speaker->active());state["radioEnabled"]=true;server.setSnapshot(state);QTRY_VERIFY(!speaker->active());state["radioConnected"]=true;server.setSnapshot(state);QTRY_VERIFY(speaker->active());laptop.close();
    }
    void failedRadioDisconnectKeepsSpeakersOff(){
        configure("laptop",false);QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/listenToShack",true);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));remote::Server server;server.start(0,key_);
        QJsonObject state{{"radioEnabled",true},{"radioConnected",true},{"radioPttActive",false},{"radio",QJsonObject{{"ptt",0}}},{"receiving",true},{"transmitting",false},{"preparing",false},{"input","shack-input"},{"output","shack-output"}};server.setSnapshot(state);connect(&server,&remote::Server::request,&server,[&](qint64 id,const QString&,const QJsonObject&){server.reply(id,"Simulated disconnect failed");});auto* client=laptop.findChild<remote::Client*>();pair(server,*client);QTRY_VERIFY(client->connected());auto* speaker=laptop.findChild<RemoteSpeaker*>();QTRY_VERIFY(speaker->active());
        child<QAction>(laptop,"radioDisconnectAction")->trigger();QVERIFY(!speaker->active());QTRY_VERIFY(!radio(laptop)->busy());QVERIFY(!speaker->active());QVERIFY(!child<QCheckBox>(laptop,"listenToShack")->isChecked());QVERIFY(child<QLabel>(laptop,"statusLabel")->text().contains("Simulated disconnect failed"));QVERIFY(client->connected());laptop.close();
    }
    void latchedVoicePttKeepsItsStyleUntilReleased(){
        configure("local");auto state=std::make_shared<VoiceRadioState>();MainWindow window(nullptr,std::make_unique<VoiceRadio>(state));radio(window)->connectRadio({});QTRY_VERIFY(radio(window)->connected());lockPtt(window);QTRY_VERIFY(state->keyed.load());QTRY_VERIFY(window.findChild<VoicePlayer*>()->active());
        UiRefreshProbe changes;ptt(window)->installEventFilter(&changes);const auto style=ptt(window)->styleSheet();QVERIFY(style.contains("#b3261e"));
        for(int i=0;i<5;++i){radio(window)->refresh();QTest::qWait(20);}
        QCOMPARE(changes.styleChanges,0);QCOMPARE(ptt(window)->styleSheet(),style);QVERIFY(ptt(window)->isChecked());QVERIFY(state->keyed.load());
        unlockPtt(window);QTRY_VERIFY(!state->keyed.load());QVERIFY(!ptt(window)->isChecked());QVERIFY(ptt(window)->styleSheet().isEmpty());QVERIFY(changes.styleChanges>0);window.close();
    }
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_VOICE_TEST_DIR",directory_.path().toUtf8());
        QCoreApplication::setOrganizationName("OpenSSTVVoiceTests");QCoreApplication::setApplicationName("OpenSSTVVoiceTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        QFile recorder(directory_.filePath("arecord"));QVERIFY(recorder.open(QIODevice::WriteOnly));recorder.write(R"(#!/usr/bin/python3
import os,sys,time,struct
if '-L' in sys.argv:
    print('voice-mic\n    Laptop microphone\nother-mic\n    Headset microphone');sys.exit()
device=sys.argv[sys.argv.index('-D')+1]
rate=int(sys.argv[sys.argv.index('-r')+1])
with open(os.environ['OPENSSTV_VOICE_TEST_DIR']+'/capture.txt','a') as f:f.write(device+' '+str(rate)+'\n')
if device=='bad-mic':sys.exit(2)
value={'loud-mic':20000,'good-mic':8000,'quiet-mic':100,'silent-mic':0,'clipped-mic':32767,'alternating-mic':8000}.get(device,1000)
packet=0
while True:
    sample=0 if device=='alternating-mic' and (packet//5)%2 else value
    sys.stdout.buffer.write(struct.pack('<h',sample)*(rate//50));sys.stdout.buffer.flush();packet+=1
    time.sleep(5 if device=='stalled-mic' else .02)
)");recorder.close();QVERIFY(recorder.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write(R"(#!/usr/bin/python3
import os,sys,time
if '-L' in sys.argv:
    print('laptop-speaker\n    Laptop speakers\nheadset-output\n    USB headset');sys.exit()
device=sys.argv[sys.argv.index('-D')+1]
rate=int(sys.argv[sys.argv.index('-r')+1])
with open(os.environ['OPENSSTV_VOICE_TEST_DIR']+'/output.txt','a') as f:f.write(device+'\n')
if device=='bad-output':sys.exit(2)
with open(os.environ['OPENSSTV_VOICE_TEST_DIR']+('/speaker.raw' if rate==12000 else '/voice.raw'),'ab',buffering=0) as f:
    while True:
        block=sys.stdin.buffer.read(rate//50*2)
        if not block:break
        f.write(block)
)");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QFile camera(directory_.filePath("gst-launch-1.0"));QVERIFY(camera.open(QIODevice::WriteOnly));camera.write(R"(#!/usr/bin/python3
import os,time
frame=bytes([200,80,40])*(640*480)
while True:
    data=frame
    while data:data=data[os.write(1,data):]
    time.sleep(.1)
)");camera.close();QVERIFY(camera.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings().clear();for(const auto* name:{"capture.txt","output.txt","voice.raw","speaker.raw"})QFile::remove(directory_.filePath(name));}
    void micMetricsExposeInputClippingBeforeGainAndLimiting(){
        VoiceCapture mic;QSignalSpy levels(&mic,&VoiceCapture::levels),audio(&mic,&VoiceCapture::pcm);mic.start("clipped-mic",25);QTRY_VERIFY(!levels.isEmpty());mic.stop();QVERIFY(levels[0][0].toDouble()>.999);QVERIFY(levels[0][4].toBool());QVERIFY(!levels[0][3].toBool());QVERIFY(levels[0][2].toDouble()<.26);QCOMPARE(qFromLittleEndian<qint16>(audio[0][0].toByteArray().constData()),qint16(8192));
        levels.clear();audio.clear();mic.start("loud-mic",300);QTRY_VERIFY(!levels.isEmpty());mic.stop();QVERIFY(!levels[0][4].toBool());QVERIFY(levels[0][3].toBool());QVERIFY(levels[0][2].toDouble()>1.);QCOMPARE(qFromLittleEndian<qint16>(audio[0][0].toByteArray().constData()),qint16(30000));
    }
    void micCheckClosedBeforeDeferredStartCannotOpenMicrophone(){
        MicCheckDialog dialog({"good-mic"},"good-mic",100);dialog.reject();QTest::qWait(50);QVERIFY(!dialog.findChild<VoiceCapture*>()->active());QVERIFY(read("capture.txt").isEmpty());QVERIFY(read("output.txt").isEmpty());
    }
    void remoteMicCheckDoesNotRequireShackConnection(){
        configure("laptop",false);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));QVERIFY(!laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(child<QPushButton>(laptop,"voiceMicCheck")->isEnabled());bool checked=false;
        QTimer::singleShot(30,&laptop,[&]{auto* dialog=laptop.findChild<MicCheckDialog*>();QVERIFY(dialog);const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});QTRY_COMPARE(dialog->findChild<QLabel*>("micCheckAssessment")->property("levelState").toString(),QString("quiet"));QVERIFY(dialog->findChild<VoiceCapture*>()->active());QVERIFY(!laptop.findChild<VoiceCapture*>()->active());QVERIFY(read("output.txt").isEmpty());checked=true;dialog->reject();});child<QPushButton>(laptop,"voiceMicCheck")->click();QVERIFY(checked);laptop.close();
    }
    void micCheckClassifiesLevels_data(){
        QTest::addColumn<QString>("device");QTest::addColumn<int>("gain");QTest::addColumn<QString>("assessment");
        QTest::newRow("silence")<<QString("silent-mic")<<100<<QString("silent");QTest::newRow("quiet")<<QString("quiet-mic")<<100<<QString("quiet");QTest::newRow("good")<<QString("good-mic")<<100<<QString("good");QTest::newRow("hot")<<QString("loud-mic")<<100<<QString("hot");QTest::newRow("limiting")<<QString("loud-mic")<<300<<QString("limited");QTest::newRow("input-clipping-despite-low-gain")<<QString("clipped-mic")<<25<<QString("clipping");
        QTest::newRow("quiet-at-maximum-gain")<<QString("quiet-mic")<<300<<QString("quiet");
    }
    void micCheckClassifiesLevels(){
        QFETCH(QString,device);QFETCH(int,gain);QFETCH(QString,assessment);MicCheckDialog dialog({device},device,gain);dialog.show();auto* result=dialog.findChild<QLabel*>("micCheckAssessment");QTRY_COMPARE(result->property("levelState").toString(),assessment);QVERIFY(dialog.findChild<VoiceCapture*>()->active());QVERIFY(read("output.txt").isEmpty());QVERIFY(dialog.windowTitle().contains("No transmission"));
        if(assessment=="clipping"||(assessment=="quiet"&&gain==300))QVERIFY(result->text().contains("Ubuntu"));
        const auto preview=qEnvironmentVariable("OPENSSTV_MIC_CHECK_PREVIEW");if(!preview.isEmpty()){auto path=preview;path.replace(".png","-"+assessment+".png");QVERIFY(dialog.grab().save(path));}dialog.reject();QVERIFY(!dialog.findChild<VoiceCapture*>()->active());
    }
    void micCheckHoldAndFailureRecovery(){
        MicCheckDialog dialog({"alternating-mic","bad-mic","good-mic"},"alternating-mic",100);dialog.show();auto* result=dialog.findChild<QLabel*>("micCheckAssessment");QTRY_COMPARE(result->property("levelState").toString(),QString("good"));QTest::qWait(150);UiRefreshProbe changes;result->installEventFilter(&changes);QTest::qWait(450);QCOMPARE(result->property("levelState").toString(),QString("good"));QCOMPARE(changes.styleChanges,0);
        auto* device=dialog.findChild<QComboBox*>("micCheckDevice");device->setCurrentText("bad-mic");QMetaObject::invokeMethod(device->lineEdit(),"editingFinished",Qt::DirectConnection);QTRY_COMPARE(result->property("levelState").toString(),QString("error"));QVERIFY(!dialog.findChild<VoiceCapture*>()->active());QVERIFY(!dialog.findChild<QPushButton*>("micCheckApply")->isEnabled());
        device->setCurrentText("good-mic");QMetaObject::invokeMethod(device->lineEdit(),"editingFinished",Qt::DirectConnection);dialog.findChild<QPushButton*>("micCheckToggle")->click();QTRY_COMPARE(result->property("levelState").toString(),QString("good"));dialog.findChild<QPushButton*>("micCheckReset")->click();QCOMPARE(result->property("levelState").toString(),QString("waiting"));QTRY_COMPARE(result->property("levelState").toString(),QString("good"));dialog.findChild<QPushButton*>("micCheckToggle")->click();QVERIFY(!dialog.findChild<VoiceCapture*>()->active());dialog.reject();
    }
    void micCheckAppliesOnlyAcceptedSettings_data(){QTest::addColumn<bool>("accept");QTest::newRow("use-settings")<<true;QTest::newRow("cancel")<<false;}
    void micCheckAppliesOnlyAcceptedSettings(){
        QFETCH(bool,accept);configure("local",false);QSettings().setValue("voice/microphone","good-mic");QSettings().setValue("voice/gain",25);auto state=std::make_shared<VoiceRadioState>();MainWindow window(nullptr,std::make_unique<VoiceRadio>(state));window.show();bool checked=false;
        QTimer::singleShot(30,&window,[&]{auto* dialog=window.findChild<MicCheckDialog*>("micCheckDialog");QVERIFY(dialog);const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});QTRY_COMPARE(dialog->findChild<QLabel*>("micCheckAssessment")->property("levelState").toString(),QString("quiet"));QVERIFY(!ptt(window)->isEnabled());QVERIFY(!child<QPushButton>(window,"playButton")->isEnabled());QVERIFY(!window.findChild<VoiceCapture*>()->active());
            const auto original=dialog->findChild<QWidget*>("micCheckInputMeter")->property("peakDb").toDouble();dialog->findChild<QSpinBox*>("micCheckGain")->setValue(100);QTRY_COMPARE(dialog->findChild<QLabel*>("micCheckAssessment")->property("levelState").toString(),QString("good"));QCOMPARE(dialog->findChild<QWidget*>("micCheckInputMeter")->property("peakDb").toDouble(),original);QCOMPARE(child<QSpinBox>(window,"voiceGain")->value(),25);QVERIFY(!state->keyed.load());QVERIFY(read("output.txt").isEmpty());checked=true;if(accept)QTest::mouseClick(dialog->findChild<QPushButton*>("micCheckApply"),Qt::LeftButton);else dialog->reject();});
        child<QPushButton>(window,"voiceMicCheck")->click();QVERIFY(checked);QCOMPARE(child<QSpinBox>(window,"voiceGain")->value(),accept?100:25);QCOMPARE(QSettings().value("voice/gain").toInt(),accept?100:25);QTRY_VERIFY(child<SignalView>(window,"signalScope")->inputActive());QVERIFY(!state->keyed.load());window.close();
    }
    void remoteMicCheckKeepsRxAndCannotTransmit_data(){QTest::addColumn<bool>("stop");QTest::newRow("cancel")<<false;QTest::newRow("stop-button")<<true;}
    void remoteMicCheckKeepsRxAndCannotTransmit(){
        QFETCH(bool,stop);configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());configure("laptop");QSettings().setValue("voice/microphone","good-mic");QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/listenToShack",true);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());auto* speaker=laptop.findChild<RemoteSpeaker*>();QTRY_VERIFY(speaker->active());auto* scope=child<SignalView>(laptop,"signalScope");QTRY_VERIFY(scope->historyRows()>0);QTRY_VERIFY(child<QPushButton>(laptop,"voiceMicCheck")->isEnabled());bool checked=false;
        QTimer::singleShot(30,&laptop,[&]{auto* dialog=laptop.findChild<MicCheckDialog*>();QVERIFY(dialog);const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});QTRY_COMPARE(dialog->findChild<QLabel*>("micCheckAssessment")->property("levelState").toString(),QString("good"));QVERIFY(!speaker->active());const auto rows=scope->historyRows();QTRY_VERIFY(scope->historyRows()>rows);QVERIFY(!state->keyed.load());QVERIFY(!shack.findChild<remote::Server*>()->remoteTransmitBusy());QVERIFY(!shack.findChild<VoicePlayer*>()->active());QVERIFY(!laptop.findChild<VoiceCapture*>()->active());QVERIFY(!ptt(laptop)->isEnabled());checked=true;if(stop)child<QAction>(laptop,"radioReleaseAction")->trigger();else dialog->reject();});
        child<QPushButton>(laptop,"voiceMicCheck")->click();QVERIFY(checked);QVERIFY(client->connected());QTRY_VERIFY(speaker->active());QVERIFY(!state->keyed.load());QVERIFY(!ptt(laptop)->isChecked());laptop.close();shack.close();
    }
    void speakerVolumeOddBoundariesAndOutputFailure(){
        RemoteSpeaker player;QSignalSpy failure(&player,&RemoteSpeaker::fault);QByteArray pcm(2400,0);for(int i=0;i<pcm.size();i+=2)qToLittleEndian<qint16>(1000,pcm.data()+i);
        player.start("laptop-speaker",50);player.push(pcm.first(3));player.push(pcm.mid(3));QTRY_VERIFY(read("speaker.raw").size()>=480);auto audio=read("speaker.raw");for(int i=0;i<480;i+=2)QCOMPARE(qFromLittleEndian<qint16>(audio.constData()+i),qint16(500));QCOMPARE(failure.size(),0);
        player.setVolume(0);QTest::qWait(150);player.push(pcm);QTest::qWait(150);audio=read("speaker.raw");QVERIFY(audio.endsWith(QByteArray(480,0)));player.stop();const auto bytes=read("speaker.raw").size();player.push(pcm);QTest::qWait(40);QCOMPARE(read("speaker.raw").size(),bytes);QVERIFY(!player.active());
        player.start("bad-output",100);QTRY_COMPARE(failure.size(),1);QVERIFY(!player.active());
    }
    void pictureDialogsKeepRemoteAudioAndReceiveRunning_data(){
        QTest::addColumn<bool>("selfie");QTest::addColumn<bool>("accept");
        QTest::newRow("selfie-use-photo")<<true<<true;QTest::newRow("selfie-cancel")<<true<<false;
        QTest::newRow("screenshots-load")<<false<<true;QTest::newRow("screenshots-cancel")<<false<<false;
    }
    void pictureDialogsKeepRemoteAudioAndReceiveRunning(){
        QFETCH(bool,selfie);QFETCH(bool,accept);
        if(selfie&&!discoverSelfieCameras().isEmpty())QSKIP("Do not open a physical webcam during the simulated station test");
        configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());
        configure("laptop");QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/listenToShack",true);
        const auto picture=directory_.filePath("dialog-picture.png");QImage image(64,48,QImage::Format_RGB32);image.fill(Qt::green);QVERIFY(image.save(picture));QSettings().setValue("pictures/screenshotDirectory",directory_.path());
        MainWindow laptop;laptop.show();laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());
        auto* speaker=laptop.findChild<RemoteSpeaker*>();auto* scope=child<SignalView>(laptop,"signalScope");QTRY_VERIFY(speaker->active());QTRY_VERIFY(read("speaker.raw").size()>480);QTRY_VERIFY(scope->historyRows()>2);
        const auto starts=read("output.txt").count("laptop-speaker");bool checked=false;
        QTimer::singleShot(100,&laptop,[&]{
            auto* dialog=selfie?static_cast<QDialog*>(laptop.findChild<SelfieDialog*>()):static_cast<QDialog*>(laptop.findChild<QFileDialog*>("loadScreenshotDialog"));QVERIFY(dialog);
            const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});
            if(selfie){auto* camera=dialog->findChild<QComboBox*>("selfieCamera");if(camera->count()==0)camera->addItem("Simulated webcam","/dev/mock0");QTRY_VERIFY(dialog->findChild<QPushButton*>("selfieCapture")->isEnabled());}
            QVERIFY(scope->inputActive());QVERIFY(speaker->active());const auto bytes=read("speaker.raw").size();const auto rows=scope->historyRows();
            QTRY_VERIFY(read("speaker.raw").size()>bytes+960);QTRY_VERIFY(scope->historyRows()>rows);QCOMPARE(read("output.txt").count("laptop-speaker"),starts);QVERIFY(!state->keyed.load());
            if(selfie){dialog->findChild<QPushButton*>("selfieCapture")->click();QVERIFY(scope->inputActive());QVERIFY(speaker->active());if(accept)dialog->findChild<QPushButton*>("selfieUsePhoto")->click();}
            else if(accept){auto* chooser=static_cast<QFileDialog*>(dialog);chooser->selectFile(picture);chooser->findChild<QLineEdit*>("fileNameEdit")->setText(picture);static_cast<QDialog*>(chooser)->accept();}
            checked=true;
        });
        if(selfie)child<QAction>(laptop,"takeSelfieAction")->trigger();else child<QAction>(laptop,"loadScreenshotAction")->trigger();
        QVERIFY(checked);QVERIFY(scope->inputActive());QVERIFY(speaker->active());QCOMPARE(read("output.txt").count("laptop-speaker"),starts);QVERIFY(!state->keyed.load());laptop.close();shack.close();
    }
    void speakerCatchesUpAfterBurstsAndSurvivesReceiveGaps(){
        RemoteSpeaker player;QSignalSpy failure(&player,&RemoteSpeaker::fault);player.start("laptop-speaker",100);player.push(QByteArray(8000,'a'));player.push(QByteArray(30000,'b'));QTRY_VERIFY(read("speaker.raw").size()>=480);QVERIFY(read("speaker.raw").startsWith(QByteArray(480,'b')));
        QTest::qWait(900);QVERIFY(player.active());QCOMPARE(failure.size(),0);player.push(QByteArray(2400,'c'));QTRY_VERIFY(read("speaker.raw").contains(QByteArray(480,'c')));player.stop();
    }
    void speakerDeviceDiscoveryKeepsSelectionsAndDoesNotStartVoice(){
        configure("local",false);QSettings().setValue("voice/speaker","custom-speaker");MainWindow window;auto* mic=child<QComboBox>(window,"voiceMicrophone");auto* speaker=child<QComboBox>(window,"remoteSpeakerDevice");auto* discover=child<QPushButton>(window,"voiceFindMicrophones");discover->click();QTRY_VERIFY(discover->isEnabled());QVERIFY(mic->findText("other-mic")>=0);QVERIFY(speaker->findText("headset-output")>=0);QCOMPARE(mic->currentText(),QString("voice-mic"));QCOMPARE(speaker->currentText(),QString("custom-speaker"));QVERIFY(!window.findChild<VoiceCapture*>()->active());QVERIFY(!window.findChild<RemoteSpeaker*>()->active());QVERIFY(read("output.txt").isEmpty());window.close();
    }
    void laptopListensMutesForLatchedPttAndResumesWithoutRoutingAudioBack(){
        configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());
        configure("laptop");QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/speakerVolume",50);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(radio(laptop)->connected());QTRY_VERIFY(client->snapshot().value("receiving").toBool());
        auto* speaker=laptop.findChild<RemoteSpeaker*>();auto* listen=child<QCheckBox>(laptop,"listenToShack");QVERIFY(!listen->isChecked());QVERIFY(!speaker->active());listen->setChecked(true);QTRY_VERIFY(read("speaker.raw").size()>=480);QVERIFY(read("output.txt").contains("laptop-speaker"));QVERIFY(!read("output.txt").contains("laptop-output"));QCOMPARE(qFromLittleEndian<qint16>(read("speaker.raw").constData()),qint16(500));QVERIFY(!state->keyed.load());QVERIFY(!laptop.findChild<VoiceCapture*>()->active());
        auto* scope=child<SignalView>(laptop,"signalScope");const auto rows=scope->historyRows();QTRY_VERIFY(scope->historyRows()>rows);laptop.resize(1000,780);laptop.show();child<QTabWidget>(laptop,"imageTabs")->setCurrentWidget(child<QWidget>(laptop,"radioPage"));QTest::qWait(30);const auto preview=qEnvironmentVariable("OPENSSTV_REMOTE_SPEAKER_PREVIEW");if(!preview.isEmpty())QVERIFY(laptop.grab().save(preview));
        lockPtt(laptop);QVERIFY(!speaker->active());QTRY_VERIFY(state->keyed.load());QVERIFY(listen->isChecked());QTRY_VERIFY(read("voice.raw").size()>=1920);QCOMPARE(qFromLittleEndian<qint16>(read("voice.raw").constData()),qint16(1000));QVERIFY(read("output.txt").contains("shack-output"));QVERIFY(ptt(laptop)->isChecked());QVERIFY(!speaker->active());
        const auto received=read("speaker.raw").size();unlockPtt(laptop);QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(speaker->active());QTRY_VERIFY(read("speaker.raw").size()>received);QVERIFY(!ptt(laptop)->isChecked());
        QTRY_VERIFY(ptt(shack)->isEnabled());lockPtt(shack);QTRY_VERIFY(state->keyed.load());QTRY_VERIFY(!speaker->active());QVERIFY(listen->isChecked());unlockPtt(shack);QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(speaker->active());
        client->disconnectFromHost();QVERIFY(!speaker->active());QVERIFY(listen->isChecked());bridge(shack,laptop);QTRY_VERIFY(client->connected());QTRY_VERIFY(speaker->active());QVERIFY(!state->keyed.load());QVERIFY(!laptop.findChild<VoiceCapture*>()->active());
        auto* device=child<QComboBox>(laptop,"remoteSpeakerDevice");device->setCurrentText("bad-output");QMetaObject::invokeMethod(device->lineEdit(),"editingFinished",Qt::DirectConnection);QTRY_VERIFY(!listen->isChecked());QVERIFY(!speaker->active());QVERIFY(child<QLabel>(laptop,"remoteSpeakerStatus")->text().contains("Laptop speaker"));QVERIFY(client->connected());const auto afterError=scope->historyRows();QTRY_VERIFY(scope->historyRows()>afterError);QVERIFY(ptt(laptop)->isEnabled());
        device->setCurrentText("laptop-speaker");QMetaObject::invokeMethod(device->lineEdit(),"editingFinished",Qt::DirectConnection);listen->setChecked(true);QTRY_VERIFY(speaker->active());laptop.close();QVERIFY(!speaker->active());shack.close();
    }
    void speakerFollowsStationStateWithoutRestartingOnMeterUpdates(){
        configure("laptop",false);QSettings().setValue("voice/speaker","laptop-speaker");QSettings().setValue("voice/listenToShack",true);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));remote::Server server;server.start(0,key_);
        QJsonObject radioState{{"ptt",0},{"strengthDb",-42}};QJsonObject state{{"radioEnabled",false},{"radioConnected",false},{"radioPttActive",false},{"radio",radioState},{"receiving",true},{"transmitting",false},{"preparing",false},{"input","shack-input"},{"output","shack-output"}};server.setSnapshot(state);auto* client=laptop.findChild<remote::Client*>();pair(server,*client);QTRY_VERIFY(client->connected());auto* speaker=laptop.findChild<RemoteSpeaker*>();QTRY_VERIFY(speaker->active());QTRY_COMPARE(read("output.txt").count("laptop-speaker"),1);
        auto* listen=child<QCheckBox>(laptop,"listenToShack");auto* status=child<QLabel>(laptop,"remoteSpeakerStatus");const auto text=status->text();UiRefreshProbe changes;listen->installEventFilter(&changes);
        for(int i=0;i<10;++i){radioState["strengthDb"]=-42+i;state["radio"]=radioState;server.setSnapshot(state);QCoreApplication::processEvents();QCOMPARE(status->text(),text);QVERIFY(speaker->active());}
        QCOMPARE(read("output.txt").count("laptop-speaker"),1);QCOMPARE(changes.enabledChanges,0);QCOMPARE(changes.styleChanges,0);
        for(const auto* flag:{"transmitting","preparing","voice"}){state[flag]=true;server.setSnapshot(state);QTRY_VERIFY(!speaker->active());QVERIFY(listen->isChecked());state[flag]=false;server.setSnapshot(state);QTRY_VERIFY(speaker->active());}
        state["receiveError"]="USB input unavailable";server.setSnapshot(state);QTRY_VERIFY(!speaker->active());state["receiveError"]="";state["receiving"]=false;server.setSnapshot(state);QVERIFY(!speaker->active());state["receiving"]=true;server.setSnapshot(state);QTRY_VERIFY(speaker->active());listen->setChecked(false);QVERIFY(!speaker->active());QVERIFY(client->connected());laptop.close();
    }
    void microphoneGainLimiterAndStop(){
        VoiceCapture mic;QSignalSpy audio(&mic,&VoiceCapture::pcm),level(&mic,&VoiceCapture::level),failure(&mic,&VoiceCapture::fault);
        mic.start("loud-mic",300);QTRY_VERIFY(audio.size()>=3);mic.stop();QVERIFY(!mic.active());QCOMPARE(failure.size(),0);
        const auto block=audio[0][0].toByteArray();QCOMPARE(block.size(),1920);for(int n=0;n<block.size();n+=2)QCOMPARE(qFromLittleEndian<qint16>(block.constData()+n),qint16(30000));QVERIFY(level[0][0].toInt()<=92);
        audio.clear();mic.start("voice-mic",50);QTRY_VERIFY(!audio.isEmpty());mic.stop();QCOMPARE(qFromLittleEndian<qint16>(audio[0][0].toByteArray().constData()),qint16(500));
    }
    void microphoneFailureAndStallAreReported(){
        VoiceCapture mic;QSignalSpy failure(&mic,&VoiceCapture::fault);mic.start("bad-mic",100);QTRY_COMPARE(failure.size(),1);QVERIFY(!mic.active());
        mic.start("stalled-mic",100);QTRY_COMPARE_WITH_TIMEOUT(failure.size(),2,3500);QVERIFY(!mic.active());
    }
    void playerWaitsForPttAndStopsOnStreamLoss(){
        VoicePlayer player;QSignalSpy open(&player,&VoicePlayer::opened),failure(&player,&VoicePlayer::fault);player.start("shack-output");QTRY_COMPARE(open.size(),1);
        for(int n=0;n<6;++n)player.push(QByteArray(1920,'v'));QTest::qWait(100);QVERIFY(read("voice.raw").isEmpty());player.allowPlayback();QTRY_VERIFY(read("voice.raw").size()>=1920);QVERIFY(read("voice.raw").startsWith(QByteArray(1920,'v')));
        QTRY_COMPARE_WITH_TIMEOUT(failure.size(),1,2500);QVERIFY(!player.active());
    }
    void playerRejectsUnboundedAndOddAudio(){
        VoicePlayer player;QSignalSpy failure(&player,&VoicePlayer::fault);player.start("shack-output");player.push(QByteArray(3,'x'));QCOMPARE(failure.size(),1);QVERIFY(!player.active());
        player.start("shack-output");for(int n=0;n<6;++n)player.push(QByteArray(9600,'x'));QCOMPARE(failure.size(),2);QVERIFY(!player.active());
    }
    void protocolStreamsExactPcmAndReservesTransmitter(){
        remote::Server server;server.start(0,key_);server.canTransmit=[]{return QString{};};server.beginVoice=[](auto ready){ready({});};QByteArray received;server.voicePcm=[&](const QByteArray& pcm){received+=pcm;};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QVERIFY(client.voiceSupported());
        bool ready=false;client.startVoice([&](QString error){QVERIFY(error.isEmpty());ready=true;});QTRY_VERIFY(ready);QVERIFY(server.remoteTransmitBusy());QVERIFY(!server.reserveLocalTransmit().isEmpty());QByteArray audio(1920,'x');client.sendVoicePcm(audio);client.sendVoicePcm(QByteArray(1920,'y'));QTRY_COMPARE(received,audio+QByteArray(1920,'y'));
        QSignalSpy stopped(&server,&remote::Server::voiceStopped);client.stopVoice();QTRY_COMPARE(stopped.size(),1);QVERIFY(!server.remoteTransmitBusy());QVERIFY(server.reserveLocalTransmit().isEmpty());
        QString error;client.startVoice([&](QString e){error=e;});QTRY_VERIFY(!error.isEmpty());QVERIFY(!server.remoteTransmitBusy());server.releaseLocalTransmit();
    }
    void releaseBeforeAcknowledgmentCannotStartOldVoice(){
        remote::Server server;server.start(0,key_);server.canTransmit=[]{return QString{};};std::vector<remote::Server::Ready> callbacks;server.beginVoice=[&](auto ready){callbacks.push_back(std::move(ready));};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());int starts=0;client.startVoice([&](QString){++starts;});QTRY_COMPARE(callbacks.size(),size_t(1));client.stopVoice();QTRY_VERIFY(!server.remoteTransmitBusy());client.startVoice([&](QString error){QVERIFY(error.isEmpty());++starts;});QTRY_COMPARE(callbacks.size(),size_t(2));callbacks[0]({});QTest::qWait(30);QCOMPARE(starts,0);QVERIFY(client.voiceActive());callbacks[1]({});QTRY_COMPARE(starts,1);
        auto* channel=client.findChild<remote::Channel*>();channel->send({{"type","voice.end"},{"id",999},{"stream",1}});QTest::qWait(30);QVERIFY(server.remoteTransmitBusy());client.stopVoice();QTRY_VERIFY(!server.remoteTransmitBusy());
    }
    void olderAuthenticatedServerDoesNotOfferVoice(){
        auto [host,laptop]=remoteTestPair();remote::Channel peer(host);remote::Client client;
        const QByteArray nonce(32,'a');connect(&peer,&remote::Channel::packet,&peer,[&](const QJsonObject& message,const QByteArray&){
            if(message.value("type")!="auth")return;
            peer.secure(QMessageAuthenticationCode::hash("OpenSSTV session v1:"+nonce,key_.toUtf8(),QCryptographicHash::Sha256));peer.send({{"type","welcome"},{"snapshot",QJsonObject{}}});
        });client.connectDevice(laptop,key_);peer.send({{"type","challenge"},{"version",1},{"nonce",QString::fromLatin1(nonce)}});QTRY_VERIFY(client.connected());QVERIFY(!client.voiceSupported());QString error;client.startVoice([&](QString e){error=e;});QVERIFY(error.contains("1.3"));QVERIFY(!client.voiceActive());
    }
    void protocolRejectsMissingSamplesAndDisconnectStopsVoice(){
        remote::Server server;server.start(0,key_);server.canTransmit=[]{return QString{};};server.beginVoice=[](auto ready){ready({});};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());bool ready=false;client.startVoice([&](QString e){QVERIFY(e.isEmpty());ready=true;});QTRY_VERIFY(ready);QSignalSpy stopped(&server,&remote::Server::voiceStopped);QSignalSpy ended(&client,&remote::Client::voiceFinished);
        client.findChild<remote::Channel*>()->send({{"type","voice.pcm"},{"stream",1},{"offset",1920},{"rate",48000}},QByteArray(1920,'v'));QTRY_COMPARE(stopped.size(),1);QTRY_COMPARE(ended.size(),1);QVERIFY(!server.remoteTransmitBusy());
        ready=false;client.startVoice([&](QString e){QVERIFY(e.isEmpty());ready=true;});QTRY_VERIFY(ready);client.disconnectFromHost();QTRY_COMPARE(stopped.size(),2);QVERIFY(!server.remoteTransmitBusy());
    }
    void laptopMicrophoneStaysLockedUntilSecondClick(){
        configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());
        configure("laptop");MainWindow laptop;laptop.show();laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(radio(laptop)->connected());auto* tabs=child<QTabWidget>(laptop,"imageTabs");tabs->setCurrentWidget(child<QWidget>(laptop,"radioPage"));QTRY_VERIFY(ptt(laptop)->isEnabled());
        QTest::mousePress(ptt(laptop),Qt::LeftButton);QVERIFY(!laptop.findChild<VoiceCapture*>()->active());QTest::mouseRelease(ptt(laptop),Qt::LeftButton);QTRY_VERIFY(state->keyed.load());QVERIFY(ptt(laptop)->isChecked());QVERIFY(!ptt(laptop)->isDown());QTRY_VERIFY(child<QLabel>(laptop,"voiceStatus")->text().contains("Talk now"));QTRY_VERIFY(read("voice.raw").size()>=1920);QVERIFY(read("output.txt").contains("shack-output"));QVERIFY(!read("output.txt").contains("laptop"));QVERIFY(read("capture.txt").contains("voice-mic 48000"));QVERIFY(!read("capture.txt").contains("laptop-input"));QVERIFY(!child<QPushButton>(shack,"playButton")->isEnabled());QVERIFY(!child<QPushButton>(laptop,"playButton")->isEnabled());
        const auto pcm=read("voice.raw");QCOMPARE(qFromLittleEndian<qint16>(pcm.constData()),qint16(1000));QTest::qWait(100);QVERIFY(state->keyed.load());QTest::mouseClick(ptt(laptop),Qt::LeftButton);QVERIFY(!ptt(laptop)->isChecked());QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!shack.findChild<VoicePlayer*>()->active());QTRY_VERIFY(child<QPushButton>(laptop,"playButton")->isEnabled());QTRY_VERIFY(read("capture.txt").contains("shack-input 12000"));laptop.close();shack.close();
    }
    void shackStopAndLostConnectionStopVoiceWithoutRestart(){
        configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());configure("laptop");MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(radio(laptop)->connected());lockPtt(laptop);QTRY_VERIFY(state->keyed.load());
        child<QAction>(shack,"radioReleaseAction")->trigger();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!laptop.findChild<VoiceCapture*>()->active());QVERIFY(!ptt(laptop)->isChecked());QTRY_VERIFY(ptt(laptop)->isEnabled());lockPtt(laptop);QTRY_VERIFY(state->keyed.load());client->disconnectFromHost();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!laptop.findChild<VoiceCapture*>()->active());bridge(shack,laptop);QTRY_VERIFY(client->connected());QTest::qWait(250);QVERIFY(!state->keyed.load());QVERIFY(!client->voiceActive());
        QTRY_VERIFY(ptt(laptop)->isEnabled());lockPtt(laptop);client->disconnectFromHost();QVERIFY(!laptop.findChild<VoiceCapture*>()->active());QTest::qWait(100);QVERIFY(!state->keyed.load());laptop.close();shack.close();
    }
    void toggleDuringKeyingAndFocusChangesPreserveLock(){
        configure("shack");auto state=std::make_shared<VoiceRadioState>();state->delay=250;MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());configure("laptop");MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(radio(laptop)->connected());
        lockPtt(laptop);unlockPtt(laptop);QTest::qWait(350);QVERIFY(!state->keyed.load());QVERIFY(!laptop.findChild<VoiceCapture*>()->active());lockPtt(laptop);QTRY_VERIFY(shack.findChild<VoicePlayer*>()->active());unlockPtt(laptop);QTest::qWait(600);QVERIFY(!state->keyed.load());QTRY_VERIFY(ptt(laptop)->isEnabled());lockPtt(laptop);QTRY_VERIFY(state->keyed.load());QEvent deactivate(QEvent::WindowDeactivate);QApplication::sendEvent(&laptop,&deactivate);QEvent appDeactivate(QEvent::ApplicationDeactivate);QApplication::sendEvent(qApp,&appDeactivate);auto* tabs=child<QTabWidget>(laptop,"imageTabs");tabs->setCurrentWidget(child<QWidget>(laptop,"radioPage"));tabs->setCurrentWidget(child<QWidget>(laptop,"receivePage"));QTest::qWait(100);QVERIFY(state->keyed.load());QVERIFY(ptt(laptop)->isChecked());QVERIFY(laptop.findChild<VoiceCapture*>()->active());QTest::keyClick(ptt(laptop),Qt::Key_Space);QTRY_VERIFY(!state->keyed.load());QVERIFY(!ptt(laptop)->isChecked());QVERIFY(!laptop.findChild<VoiceCapture*>()->active());laptop.close();shack.close();
    }
    void signalinkVoiceAndLocalMicrophoneWorkWithoutCat(){
        configure("shack",false);MainWindow shack;shack.setRemoteConfig(config(remote::Role::Host));configure("laptop",false);MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());lockPtt(laptop);QTRY_VERIFY(read("voice.raw").size()>=1920);QVERIFY(!radio(shack)->pttActive());unlockPtt(laptop);QTRY_VERIFY(!shack.findChild<VoicePlayer*>()->active());laptop.close();shack.close();
        configure("local",false);MainWindow local;lockPtt(local);QTRY_VERIFY(local.findChild<VoicePlayer*>()->active());QTRY_VERIFY(read("output.txt").contains("local-output"));unlockPtt(local);QVERIFY(!local.findChild<VoiceCapture*>()->active());QVERIFY(!local.findChild<VoicePlayer*>()->active());local.close();
    }
    void microphoneOrOutputFailureReleasesRadio(){
        configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());configure("laptop");MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(radio(laptop)->connected());child<QComboBox>(laptop,"voiceMicrophone")->setCurrentText("stalled-mic");lockPtt(laptop);QTRY_VERIFY(state->keyed.load());QTRY_VERIFY_WITH_TIMEOUT(!state->keyed.load(),3500);QTRY_VERIFY(!laptop.findChild<VoiceCapture*>()->active());
        child<QComboBox>(laptop,"voiceMicrophone")->setCurrentText("voice-mic");child<QLineEdit>(shack,"outputDevice")->setText("bad-output");QTRY_VERIFY(ptt(laptop)->isEnabled());lockPtt(laptop);QTRY_VERIFY(!laptop.findChild<VoiceCapture*>()->active());QTRY_VERIFY(!radio(shack)->pttActive());QVERIFY(!state->keyed.load());laptop.close();shack.close();
    }
    void voiceReleaseFailureBlocksTxUntilPttOffSucceeds(){
        configure("shack");auto state=std::make_shared<VoiceRadioState>();MainWindow shack(nullptr,std::make_unique<VoiceRadio>(state));shack.setRemoteConfig(config(remote::Role::Host));radio(shack)->connectRadio({});QTRY_VERIFY(radio(shack)->connected());configure("laptop");MainWindow laptop;laptop.setRemoteConfig(config(remote::Role::Client));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(radio(laptop)->connected());lockPtt(laptop);QTRY_VERIFY(state->keyed.load());state->failOff=true;unlockPtt(laptop);
        QTRY_VERIFY(!shack.findChild<VoicePlayer*>()->active());QTRY_VERIFY(radio(shack)->error().contains("PTT OFF"));QVERIFY(state->keyed.load());QVERIFY(!ptt(laptop)->isEnabled());QVERIFY(!child<QPushButton>(shack,"playButton")->isEnabled());state->failOff=false;child<QAction>(shack,"radioReleaseAction")->trigger();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(ptt(laptop)->isEnabled());laptop.close();shack.close();
    }
    void settingsEntryExplainsLockedPttAndCanStopBeforeOpening(){
        configure("local");auto state=std::make_shared<VoiceRadioState>();MainWindow window(nullptr,std::make_unique<VoiceRadio>(state));radio(window)->connectRadio({});QTRY_VERIFY(radio(window)->connected());lockPtt(window);QTRY_VERIFY(state->keyed.load());QVERIFY(window.findChild<QAction*>("settingsAction")->isEnabled());QVERIFY(window.findChild<QAction*>("audioWizardAction")->isEnabled());QVERIFY(window.findChild<QPushButton*>("radioFrontendSettings")->isEnabled());QVERIFY(window.findChild<QPushButton*>("audioLevelDevicesButton")->isEnabled());
        QTimer::singleShot(30,&window,[&]{auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(message);QCOMPARE(message->objectName(),QString("setupBusyDialog"));QVERIFY(message->text().contains("Voice PTT"));message->button(QMessageBox::Cancel)->click();});window.findChild<QAction*>("settingsAction")->trigger();QVERIFY(state->keyed.load());QVERIFY(ptt(window)->isChecked());
        bool seen=false;int stage=0;QTimer choose;choose.setInterval(20);connect(&choose,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;if(stage==0&&dialog->objectName()=="setupBusyDialog"){stage=1;dialog->findChild<QAbstractButton*>("setupStopAndOpen")->click();return;}if(stage==1&&dialog->objectName()=="settingsDialog"){choose.stop();QVERIFY(!state->keyed.load());QVERIFY(!ptt(window)->isChecked());QVERIFY(!window.findChild<VoiceCapture*>()->active());seen=true;dialog->reject();}});choose.start();window.findChild<QAction*>("settingsAction")->trigger();QTRY_VERIFY_WITH_TIMEOUT(seen,5000);window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);qunsetenv("OPENSSTV_VOICE_TEST_DIR");QSettings().clear();}
};
int main(int argc,char** argv){
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QTemporaryDir environment;qputenv("XDG_CONFIG_HOME",environment.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",environment.filePath("data").toUtf8());QDir().mkpath(environment.filePath("config"));QFile dirs(environment.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+environment.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);VoiceTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "voice_tests.moc"
