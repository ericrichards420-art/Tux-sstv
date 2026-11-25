#include "audio_wizard.h"
#include "mainwindow.h"
#include "remote_test_transport.h"
#include <QtTest>
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonObject>

class AudioTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    void listing(const char* name,const QByteArray& contents){QFile file(directory_.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));QCOMPARE(file.write(contents),contents.size());}
    static int deviceIndex(QComboBox* box,const QString& name){for(int i=0;i<box->count();++i)if(box->itemData(i).toMap().value("input").toString()==name)return i;return -1;}
    QJsonObject mixerState(const QString& card="USB"){QFile f(directory_.filePath("mixer-state.json"));if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object().value(card).toObject();}
    // Follow the public Setup wizard to its audio section, then close the
    // surrounding wizard after the existing audio save/cancel workflow ends.
    void openAudioSetup(MainWindow& window){
        QTimer navigate;int stage=0;QElapsedTimer elapsed;elapsed.start();navigate.setInterval(5);
        connect(&navigate,&QTimer::timeout,&window,[&]{
            auto* setup=window.findChild<QWizard*>("setupWizard");if(!setup)return;
            if(stage==0){stage=1;setup->next();QCOMPARE(setup->currentId(),1);setup->findChild<QPushButton*>("setupConfigureAudio")->click();stage=2;return;}
            if(stage==2&&setup->isEnabled()&&!window.findChild<AudioWizard*>()){navigate.stop();setup->reject();}
            if(elapsed.elapsed()>17000){navigate.stop();if(auto* audio=window.findChild<AudioWizard*>())audio->reject();setup->reject();}
        });navigate.start();window.findChild<QAction*>("audioWizardAction")->trigger();
    }
    void audioPair(MainWindow& shack,MainWindow& laptop){
        remote::Config c;c.role=remote::Role::Host;c.key="0123456789abcdef0123456789abcdef";shack.setRemoteConfig(c);c.role=remote::Role::Client;c.host="shack.invalid";c.reconnect=false;laptop.setRemoteConfig(c);
        auto [host,client]=remoteTestPair();shack.findChild<remote::Server*>()->acceptConnection(host);laptop.findChild<remote::Client*>()->connectDevice(client,c.key);
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);
        qputenv("OPENSSTV_AUDIO_TEST",directory_.path().toUtf8());
        QCoreApplication::setOrganizationName("OpenSSTVAudioTests");QCoreApplication::setApplicationName("OpenSSTVAudioTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("config"));
        const QByteArray script=R"(#!/usr/bin/python3
import sys,os,struct,time,wave
if '-L' in sys.argv:
    root=os.environ['OPENSSTV_AUDIO_TEST']
    if os.path.exists(os.path.join(root,'fail-list')):
        print('Simulated unavailable devices',file=sys.stderr)
        sys.exit(2)
    if os.path.exists(os.path.join(root,'delay-list')): time.sleep(.2)
    print(open(os.path.join(root,os.path.basename(sys.argv[0])+'-list')).read())
    sys.exit(0)
device=sys.argv[sys.argv.index('-D')+1]
if device=='broken':
    print('Selected device unavailable',file=sys.stderr)
    sys.exit(2)
if os.path.basename(sys.argv[0])=='arecord':
    open(os.path.join(os.environ['OPENSSTV_AUDIO_TEST'],'captured'),'w').write(device)
    data=struct.pack('<h',16384)*4096
    os.write(1,data[:3]);time.sleep(.02);os.write(1,data[3:]);time.sleep(30)
else:
    with wave.open(sys.argv[-1],'rb') as f:
        assert f.getframerate()==48000 and f.getnframes()==48000
        samples=struct.unpack('<48000h',f.readframes(48000))
        expected_path=os.path.join(os.environ['OPENSSTV_AUDIO_TEST'],'expected-peak')
        expected=int(open(expected_path).read()) if os.path.exists(expected_path) else 3277
        assert abs(max(samples)-expected)<4, (max(samples),expected)
    open(os.path.join(os.environ['OPENSSTV_AUDIO_TEST'],'played'),'w').write(device)
)";
        for(const auto& name:{"arecord","aplay"}){QFile file(directory_.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(script);file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
        QFile mixer(directory_.filePath("amixer"));QVERIFY(mixer.open(QIODevice::WriteOnly));mixer.write(R"(#!/usr/bin/python3
import os,sys,json,time
root=os.environ['OPENSSTV_AUDIO_TEST']; args=sys.argv[1:]; assert args[2]=='--',args; del args[2]; card=args[1]; action=args[2]
with open(root+'/mixer-log','a') as log: log.write(json.dumps(args)+'\n')
with open(root+'/mixer-state.json') as f: state=json.load(f)
current=state.get(card,{'volume':'12,23','switch':'off,on','capture':'7'})
def save():
    state[card]=current
    with open(root+'/mixer-state.json.tmp','w') as f: json.dump(state,f)
    os.replace(root+'/mixer-state.json.tmp',root+'/mixer-state.json')
def flag(name): return os.path.exists(root+'/'+name)
if action=='info':
    print("Card hw:1 '%s'\n  Mixer name : '%s'"%(card,'HDA Intel' if card=='PCH' or flag('device-replaced') else 'USB Mixer'))
elif action=='contents':
    if not flag('no-controls'):
        limits='min=-16384,max=0' if flag('signed-mixer') else 'min=0,max=100'
        print("numid=1,iface=MIXER,name='PCM Playback Volume'\n  ; type=INTEGER,access=rw---R--,values=2,"+limits+",step=0\n  : values="+current['volume'])
        print("numid=2,iface=MIXER,name='PCM Playback Switch'\n  ; type=BOOLEAN,access=rw------,values=2\n  : values="+current['switch'])
    print("numid=3,iface=MIXER,name='Mic Capture Volume'\n  ; type=INTEGER,access=rw---R--,values=1,min=0,max=100\n  : values="+current['capture'])
elif action=='scontents':
    if not flag('no-controls'):
        print("Simple mixer control 'PCM',0\n  Capabilities: pvolume pswitch\n  Playback channels: Front Left - Front Right\n  Limits: Playback 0 - 100")
        for channel,value,on in zip(['Front Left','Front Right'],current['volume'].split(','),current['switch'].split(',')):
            percent=round((int(value)+16384)/16384*100) if flag('signed-mixer') else int(value)
            db=-64*(100-percent)/100
            print('  %s: Playback %s [%s%%] [%.2fdB] [%s]'%(channel,value,percent,db,on))
elif action=='sset':
    assert card!='PCH' and args[3:] == ['PCM,0','playback','0dB','unmute'],args
    if flag('delay-mixer'): time.sleep(.25)
    if flag('fail-mixer'):
        print('Simulated mixer write failure',file=sys.stderr);sys.exit(2)
    if not flag('ignore-mixer'):
        current['volume']='0,0' if flag('signed-mixer') else '100,100';current['switch']='on,on';save()
    if flag('partial-mixer'):
        print('Simulated partial mixer write',file=sys.stderr);sys.exit(2)
elif action=='cset':
    if flag('fail-restore'): print('Simulated restore failure',file=sys.stderr);sys.exit(2)
    assert args[3] in ['numid=1','numid=2'],args
    current['volume' if args[3]=='numid=1' else 'switch']=args[4];save()
else: raise RuntimeError(args)
)");mixer.close();QVERIFY(mixer.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){
        QSettings().clear();for(const auto& name:{"played","captured","fail-list","delay-list","expected-peak","mixer-log","no-controls","fail-mixer","ignore-mixer","partial-mixer","delay-mixer","fail-restore","device-replaced","signed-mixer"})QFile::remove(directory_.filePath(name));
        listing("mixer-state.json",R"({"USB":{"volume":"12,23","switch":"off,on","capture":"7"},"IC7300":{"volume":"12,23","switch":"off,on","capture":"7"}})");
        const QByteArray common="default\n    Default sound device\nsysdefault:CARD=USB\n    USB AUDIO CODEC, USB Audio\nhw:CARD=USB,DEV=0\n    USB AUDIO CODEC, USB Audio\nplughw:CARD=USB,DEV=0\n    USB AUDIO CODEC, USB Audio\n    Hardware with conversions\nplughw:CARD=IC7300,DEV=0\n    IC-7300 USB Audio\nnull\n    Discard samples\n";
        listing("arecord-list",common+"plughw:CARD=MIC,DEV=0\n    Computer microphone\n");listing("aplay-list",common+"plughw:CARD=HDMI,DEV=0\n    HDMI speakers\n");
    }
    void friendlyDeviceParsing(){
        const auto devices=AudioWizard::pcmDevices("# ignored\n\nplughw:CARD=CODEC,DEV=0\n    USB AUDIO CODEC, USB Audio\n    Conversion details\ndefault\n    Default sound device\n");
        QCOMPARE(devices.size(),2);QCOMPARE(devices[0].name,QString("plughw:CARD=CODEC,DEV=0"));QCOMPARE(devices[0].description,QString("USB AUDIO CODEC, USB Audio"));QCOMPARE(devices[1].description,QString("Default sound device"));
    }
    void relayWizardNeverOpensLaptopAudio(){
        AudioWizard wizard("laptop-input","laptop-output",nullptr,true);QSignalSpy requests(&wizard,&AudioWizard::relayRequest);wizard.show();wizard.findChild<QPushButton*>("wizardRefresh")->click();wizard.next();wizard.findChild<QPushButton*>("wizardListen")->click();wizard.next();wizard.findChild<QPushButton*>("wizardTone")->click();wizard.findChild<QPushButton*>("wizardOptimizeLevels")->click();QTest::qWait(100);
        QVERIFY(requests.size()>=4);QVERIFY(!QFile::exists(directory_.filePath("captured")));QVERIFY(!QFile::exists(directory_.filePath("played")));QVERIFY(!QFile::exists(directory_.filePath("mixer-log")));wizard.reject();
    }
    void pairedShackWizardRemainsAvailableAndReservesTransmitter(){
        MainWindow shack,laptop;audioPair(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QVERIFY(shack.findChild<QAction*>("audioWizardAction")->isEnabled());QVERIFY(shack.findChild<QToolButton*>("radioAudioSetupButton")->isEnabled());QVERIFY(shack.findChild<QMenu*>("radioMenu")->actions().contains(shack.findChild<QAction*>("audioWizardAction")));
        bool seen=false;QTimer::singleShot(30,&shack,[&]{auto* wizard=shack.findChild<AudioWizard*>();QVERIFY(wizard);const auto close=qScopeGuard([wizard]{wizard->reject();});QVERIFY(wizard->windowTitle().contains("Shack"));QVERIFY(!shack.findChild<QPushButton*>("playButton")->isEnabled());QTRY_VERIFY(!laptop.findChild<QPushButton*>("playButton")->isEnabled());seen=true;});openAudioSetup(shack);QVERIFY(seen);QVERIFY(client->connected());QTRY_VERIFY(laptop.findChild<QAction*>("audioWizardAction")->isEnabled());shack.close();laptop.close();
    }
    void offlineWizardProvidesLocalUsbSetupInsteadOfDeadEnd(){
        QSettings().setValue("audio/input","local-input");QSettings().setValue("audio/output","local-output");MainWindow window;remote::Config config;config.role=remote::Role::Client;config.host="offline.invalid";config.key="0123456789abcdef0123456789abcdef";config.reconnect=false;window.setRemoteConfig(config);QVERIFY(window.findChild<QAction*>("audioWizardAction")->isEnabled());bool seen=false;
        QTimer::singleShot(0,&window,[&]{
            auto* setup=window.findChild<QWizard*>("setupWizard");QVERIFY(setup);
            setup->findChild<QComboBox*>("setupRole")->setCurrentIndex(0);
            setup->next();QCOMPARE(setup->currentId(),1);
            QTimer::singleShot(30,&window,[&]{auto* audio=window.findChild<AudioWizard*>();QVERIFY(audio);QCOMPARE(audio->inputDevice(),QString("local-input"));QCOMPARE(audio->outputDevice(),QString("local-output"));seen=true;audio->reject();});
            setup->findChild<QPushButton*>("setupConfigureAudio")->click();setup->reject();
        });window.findChild<QAction*>("audioWizardAction")->trigger();QVERIFY(seen);QCOMPARE(QSettings().value("remote/role").toInt(),0);QCOMPARE(QSettings().value("remote/key").toString(),config.key);window.close();
    }
    void laptopWizardChecksOptimizesAndSavesShackAudio(){
        QSettings().setValue("audio/input","shack-input");QSettings().setValue("audio/output","shack-output");MainWindow shack;QSettings().setValue("audio/input","laptop-input");QSettings().setValue("audio/output","laptop-output");MainWindow laptop;audioPair(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());
        listing("expected-peak","21299");bool finished=false,accepted=false;int stage=0;QTimer choose;choose.setInterval(30);
        connect(&choose,&QTimer::timeout,&laptop,[&]{
            auto* wizard=laptop.findChild<AudioWizard*>();if(!wizard)return;
            if(stage==0){auto* devices=wizard->findChild<QComboBox*>("wizardDevice");const auto index=deviceIndex(devices,"plughw:CARD=USB,DEV=0");if(index<0)return;connect(wizard,&QWizard::finished,&laptop,[&](int code){accepted=code==QDialog::Accepted;finished=true;});wizard->findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);devices->setCurrentIndex(index);wizard->next();stage=1;return;}
            if(stage==1&&wizard->currentId()==1){wizard->findChild<QPushButton*>("wizardListen")->click();stage=2;return;}
            if(stage==2&&wizard->findChild<QProgressBar*>("wizardInputLevel")->value()==50){wizard->next();stage=3;return;}
            if(stage==3&&wizard->currentId()==2){wizard->findChild<QPushButton*>("wizardOptimizeLevels")->click();stage=4;return;}
            if(stage==4&&wizard->findChild<QLabel*>("wizardMixerStatus")->text().contains("unmuted and raised")&&wizard->findChild<QPushButton*>("wizardTone")->isEnabled()){QCOMPARE(mixerState().value("volume").toString(),QString("100,100"));QVERIFY(!QFile::exists(directory_.filePath("played")));wizard->findChild<QPushButton*>("wizardTone")->click();stage=5;return;}
            if(stage==5&&wizard->findChild<QLabel*>("wizardOutputStatus")->text().contains("Tone finished")){wizard->next();stage=6;return;}
            if(stage==6&&wizard->currentId()==3&&wizard->button(QWizard::FinishButton)->isEnabled()){choose.stop();wizard->button(QWizard::FinishButton)->click();stage=7;}
        });choose.start();QTimer::singleShot(12000,&laptop,[&]{if(!finished){choose.stop();if(auto* wizard=laptop.findChild<AudioWizard*>())wizard->reject();}});
        openAudioSetup(laptop);QTRY_VERIFY_WITH_TIMEOUT(finished,14000);QVERIFY(accepted);QCOMPARE(stage,7);QVERIFY(client->connected());QCOMPARE(shack.findChild<QLineEdit*>("inputDevice")->text(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(shack.findChild<QLineEdit*>("outputDevice")->text(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(laptop.findChild<QLineEdit*>("inputDevice")->text(),QString("laptop-input"));QCOMPARE(laptop.findChild<QLineEdit*>("outputDevice")->text(),QString("laptop-output"));QCOMPARE(mixerState().value("volume").toString(),QString("100,100"));QVERIFY(!shack.findChild<radio::Controller*>()->pttActive());QTRY_VERIFY(laptop.findChild<QPushButton*>("playButton")->isEnabled());laptop.close();shack.close();
    }
    void remoteWizardCancelAndDisconnectRestoreMixer_data(){QTest::addColumn<bool>("disconnect");QTest::addColumn<bool>("restoreFailure");QTest::newRow("cancel")<<false<<false;QTest::newRow("disconnect")<<true<<false;QTest::newRow("restore failure is reported")<<false<<true;}
    void remoteWizardCancelAndDisconnectRestoreMixer(){
        QFETCH(bool,disconnect);QFETCH(bool,restoreFailure);QSettings().setValue("audio/input","shack-input");QSettings().setValue("audio/output","shack-output");MainWindow shack;MainWindow laptop;audioPair(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());bool finished=false;int stage=0;QTimer choose;choose.setInterval(30);
        connect(&choose,&QTimer::timeout,&laptop,[&]{auto* wizard=laptop.findChild<AudioWizard*>();if(!wizard)return;
            if(stage==0){auto* devices=wizard->findChild<QComboBox*>("wizardDevice");const auto index=deviceIndex(devices,"plughw:CARD=USB,DEV=0");if(index<0)return;connect(wizard,&QWizard::finished,&laptop,[&](int code){QCOMPARE(code,int(QDialog::Rejected));finished=true;});wizard->findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);devices->setCurrentIndex(index);wizard->next();wizard->next();stage=1;return;}
            if(stage==1&&wizard->currentId()==2){wizard->findChild<QPushButton*>("wizardOptimizeLevels")->click();stage=2;return;}
            if(stage==2&&wizard->findChild<QLabel*>("wizardMixerStatus")->text().contains("unmuted and raised")){choose.stop();stage=3;if(restoreFailure)listing("fail-restore",{});if(disconnect)client->disconnectFromHost();else wizard->reject();}
        });choose.start();QTimer::singleShot(10000,&laptop,[&]{if(!finished){choose.stop();if(auto* wizard=laptop.findChild<AudioWizard*>())wizard->reject();}});openAudioSetup(laptop);QTRY_VERIFY_WITH_TIMEOUT(finished,12000);QCOMPARE(stage,3);if(restoreFailure){QTRY_VERIFY(laptop.findChild<QLabel*>("statusLabel")->text().contains("Could not restore"));QCOMPARE(mixerState().value("volume").toString(),QString("100,100"));}else{QTRY_COMPARE(mixerState().value("volume").toString(),QString("12,23"));QCOMPARE(mixerState().value("switch").toString(),QString("off,on"));}QCOMPARE(shack.findChild<QLineEdit*>("inputDevice")->text(),QString("shack-input"));QCOMPARE(shack.findChild<QLineEdit*>("outputDevice")->text(),QString("shack-output"));QVERIFY(!QFile::exists(directory_.filePath("played")));QTRY_VERIFY(shack.findChild<QAction*>("audioWizardAction")->isEnabled());laptop.close();shack.close();
    }
    void optimizePlaybackAndCancelRestoreExactLevels(){
        QSettings().setValue("audio/txLevelPercent",25);AudioWizard wizard("plughw:CARD=USB,DEV=0","plughw:CARD=USB,DEV=0");wizard.show();wizard.findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);wizard.next();wizard.next();auto* optimize=wizard.findChild<QPushButton*>("wizardOptimizeLevels");optimize->click();
        QTRY_VERIFY(wizard.findChild<QLabel*>("wizardMixerStatus")->text().contains("unmuted and raised"));QCOMPARE(mixerState()["volume"].toString(),QString("100,100"));QCOMPARE(mixerState()["switch"].toString(),QString("on,on"));QCOMPARE(mixerState()["capture"].toString(),QString("7"));QCOMPARE(wizard.transmitPercent(),100);QCOMPARE(QSettings().value("audio/txLevelPercent").toInt(),25);QVERIFY(!QFile::exists(directory_.filePath("played")));
        const auto preview=qEnvironmentVariable("OPENSSTV_AUDIO_OPTIMIZE_PREVIEW");if(!preview.isEmpty())QVERIFY(wizard.grab().save(preview));listing("expected-peak","21299");wizard.findChild<QPushButton*>("wizardTone")->click();QTRY_VERIFY(wizard.findChild<QLabel*>("wizardOutputStatus")->text().contains("Tone finished"));wizard.reject();QVERIFY(wizard.mixerError().isEmpty());QCOMPARE(mixerState()["volume"].toString(),QString("12,23"));QCOMPARE(mixerState()["switch"].toString(),QString("off,on"));QCOMPARE(mixerState()["capture"].toString(),QString("7"));QCOMPARE(QSettings().value("audio/txLevelPercent").toInt(),25);
    }
    void optimizeFailureRestoresLevels_data(){QTest::addColumn<QString>("flag");QTest::newRow("write fails")<<"fail-mixer";QTest::newRow("partial write fails")<<"partial-mixer";QTest::newRow("readback unchanged")<<"ignore-mixer";QTest::newRow("unsupported controls")<<"no-controls";}
    void optimizeFailureRestoresLevels(){
        QFETCH(QString,flag);listing(qPrintable(flag),{});const auto result=audio::optimizePlayback("plughw:CARD=USB,DEV=0");QVERIFY(!result.optimized);QVERIFY(!result.error.isEmpty());QCOMPARE(mixerState()["volume"].toString(),QString("12,23"));QCOMPARE(mixerState()["switch"].toString(),QString("off,on"));QCOMPARE(mixerState()["capture"].toString(),QString("7"));
    }
    void optimizeCannotChangeDefaultSpeakers_data(){QTest::addColumn<QString>("pcm");QTest::newRow("default")<<"default";QTest::newRow("custom route")<<"pipewire";QTest::newRow("laptop card")<<"plughw:CARD=PCH,DEV=0";}
    void optimizeCannotChangeDefaultSpeakers(){
        QFETCH(QString,pcm);const auto result=audio::optimizePlayback(pcm);QVERIFY(!result.optimized);QVERIFY(result.original.isEmpty());QFile log(directory_.filePath("mixer-log"));if(log.open(QIODevice::ReadOnly)){const auto history=log.readAll();QVERIFY(!history.contains("sset"));QVERIFY(!history.contains("cset"));}QCOMPARE(mixerState()["volume"].toString(),QString("12,23"));
    }
    void restoreCannotWriteToReplacementDevice(){
        const auto result=audio::optimizePlayback("plughw:CARD=USB,DEV=0");QVERIFY(result.optimized);QVERIFY(QFile::remove(directory_.filePath("mixer-log")));listing("device-replaced",{});const auto error=audio::restorePlayback(result);QVERIFY(error.contains("different device"));QFile log(directory_.filePath("mixer-log"));QVERIFY(log.open(QIODevice::ReadOnly));QVERIFY(!log.readAll().contains("cset"));
    }
    void restoreSignedUsbCodecValues(){
        listing("signed-mixer",{});listing("mixer-state.json",R"({"USB":{"volume":"-12288,-4096","switch":"off,on","capture":"7"}})");const auto result=audio::optimizePlayback("plughw:CARD=USB,DEV=0");QVERIFY2(result.optimized,qPrintable(result.error));QCOMPARE(mixerState()["volume"].toString(),QString("0,0"));QVERIFY(audio::restorePlayback(result).isEmpty());QCOMPARE(mixerState()["volume"].toString(),QString("-12288,-4096"));QCOMPARE(mixerState()["switch"].toString(),QString("off,on"));
    }
    void cancelDuringOptimizationAndRestoreFailure(){
        listing("delay-mixer",{});AudioWizard wizard("plughw:CARD=USB,DEV=0","plughw:CARD=USB,DEV=0");wizard.show();wizard.findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);wizard.next();wizard.next();wizard.findChild<QPushButton*>("wizardOptimizeLevels")->click();QVERIFY(!wizard.findChild<QPushButton*>("wizardTone")->isEnabled());QVERIFY(!wizard.findChild<QComboBox*>("wizardConnection")->isEnabled());wizard.reject();QCOMPARE(mixerState()["volume"].toString(),QString("12,23"));QVERIFY(wizard.mixerError().isEmpty());
        AudioWizard failed("plughw:CARD=USB,DEV=0","plughw:CARD=USB,DEV=0");failed.show();failed.findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);failed.next();failed.next();failed.findChild<QPushButton*>("wizardOptimizeLevels")->click();QTRY_VERIFY(failed.findChild<QLabel*>("wizardMixerStatus")->text().contains("unmuted and raised"));listing("fail-restore",{});failed.reject();QVERIFY(!failed.mixerError().isEmpty());
    }
    void finishKeepsOnlySelectedDeviceOptimization(){
        AudioWizard wizard("plughw:CARD=USB,DEV=0","plughw:CARD=USB,DEV=0");wizard.show();QTRY_VERIFY(wizard.findChild<QPushButton*>("wizardRefresh")->isEnabled());wizard.findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);wizard.next();wizard.next();auto* optimize=wizard.findChild<QPushButton*>("wizardOptimizeLevels");optimize->click();QTRY_VERIFY(wizard.findChild<QLabel*>("wizardMixerStatus")->text().contains("unmuted and raised"));wizard.back();wizard.back();auto* devices=wizard.findChild<QComboBox*>("wizardDevice");devices->setCurrentIndex(deviceIndex(devices,"plughw:CARD=IC7300,DEV=0"));wizard.next();wizard.next();optimize->click();QTRY_VERIFY(optimize->isEnabled());QCOMPARE(mixerState("IC7300")["volume"].toString(),QString("100,100"));wizard.next();wizard.button(QWizard::FinishButton)->click();QCOMPARE(wizard.result(),int(QDialog::Accepted));QCOMPARE(mixerState()["volume"].toString(),QString("12,23"));QCOMPARE(mixerState("IC7300")["volume"].toString(),QString("100,100"));QVERIFY(wizard.mixerError().isEmpty());
    }
    void oneSoundDeviceSetsBothDirections(){
        listing("expected-peak","21299");
        AudioWizard wizard("default","default");wizard.show();auto* refresh=wizard.findChild<QPushButton*>("wizardRefresh");QTRY_VERIFY(refresh->isEnabled());auto* choices=wizard.findChild<QComboBox*>("wizardDevice");auto* profile=wizard.findChild<QComboBox*>("wizardConnection");
        QCOMPARE(wizard.inputDevice(),QString("default"));QVERIFY(!wizard.findChild<QWidget*>("wizardAdvancedDevices")->isVisible());QCOMPARE(choices->count(),4);QVERIFY(deviceIndex(choices,"null")<0);QVERIFY(deviceIndex(choices,"plughw:CARD=MIC,DEV=0")<0);QVERIFY(deviceIndex(choices,"plughw:CARD=HDMI,DEV=0")<0);
        profile->setCurrentIndex(profile->findData("signalink"));QVERIFY(wizard.findChild<QLabel*>("wizardConnectionHelp")->text().contains("USB AUDIO CODEC"));QCOMPARE(wizard.inputDevice(),QString("default"));
        const int codec=deviceIndex(choices,"plughw:CARD=USB,DEV=0");QVERIFY(codec>=0);QVERIFY(choices->itemText(codec).contains("USB AUDIO CODEC"));QVERIFY(!choices->itemText(codec).contains("plughw"));choices->setCurrentIndex(codec);
        QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(wizard.outputDevice(),wizard.inputDevice());QVERIFY(!QFile::exists(directory_.filePath("captured")));QVERIFY(!QFile::exists(directory_.filePath("played")));
        const auto preview=qEnvironmentVariable("OPENSSTV_AUDIO_WIZARD_PREVIEW");if(!preview.isEmpty())QVERIFY(wizard.grab().save(preview));
        wizard.next();QVERIFY(wizard.findChild<QLabel*>("wizardReceiveDevice")->text().contains("USB AUDIO CODEC"));wizard.findChild<QPushButton*>("wizardListen")->click();QTRY_COMPARE(wizard.findChild<QProgressBar*>("wizardInputLevel")->value(),50);
        QFile captured(directory_.filePath("captured"));QVERIFY(captured.open(QIODevice::ReadOnly));QCOMPARE(captured.readAll(),QByteArray("plughw:CARD=USB,DEV=0"));wizard.next();QVERIFY(wizard.findChild<QLabel*>("wizardTransmitHelp")->text().contains("SignaLink can key"));wizard.findChild<QPushButton*>("wizardTone")->click();QTRY_VERIFY(wizard.findChild<QLabel*>("wizardOutputStatus")->text().contains("Tone finished"));
        QFile played(directory_.filePath("played"));QVERIFY(played.open(QIODevice::ReadOnly));QCOMPARE(played.readAll(),QByteArray("plughw:CARD=USB,DEV=0"));wizard.next();QVERIFY(wizard.useInterfacePtt());QVERIFY(wizard.findChild<QLabel*>("wizardReview")->text().contains("USB AUDIO CODEC"));wizard.reject();
    }
    void signalinkLabelsRefreshWithoutChoosingAnAudioDevice(){
        AudioWizard wizard("default","default");wizard.show();auto* refresh=wizard.findChild<QPushButton*>("wizardRefresh");QTRY_VERIFY(refresh->isEnabled());
        auto* profile=wizard.findChild<QComboBox*>("wizardConnection");auto* choices=wizard.findChild<QComboBox*>("wizardDevice");profile->setCurrentIndex(profile->findData("signalink"));
        const auto index=deviceIndex(choices,"plughw:CARD=USB,DEV=0");QVERIFY(index>=0);QVERIFY(choices->itemText(index).startsWith("SignaLink USB"));
        QCOMPARE(wizard.inputDevice(),QString("default"));QCOMPARE(wizard.outputDevice(),QString("default"));
        choices->setCurrentIndex(index);refresh->click();QTRY_VERIFY(refresh->isEnabled());
        QVERIFY(choices->currentText().startsWith("SignaLink USB"));QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(wizard.outputDevice(),wizard.inputDevice());
        profile->setCurrentIndex(profile->findData("other"));QVERIFY(!choices->currentText().startsWith("SignaLink USB"));
        QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(wizard.outputDevice(),wizard.inputDevice());
        QVERIFY(!QFile::exists(directory_.filePath("captured")));QVERIFY(!QFile::exists(directory_.filePath("played")));wizard.reject();
    }
    void builtInUsbAudioAndSavedChoices(){
        AudioWizard wizard("plughw:CARD=IC7300,DEV=0","plughw:CARD=IC7300,DEV=0");wizard.show();QTRY_VERIFY(wizard.findChild<QPushButton*>("wizardRefresh")->isEnabled());auto* choices=wizard.findChild<QComboBox*>("wizardDevice");QVERIFY(choices->currentText().contains("IC-7300"));QVERIFY(!wizard.findChild<QCheckBox*>("wizardAdvanced")->isChecked());
        auto* profile=wizard.findChild<QComboBox*>("wizardConnection");profile->setCurrentIndex(profile->findData("radio-usb"));QVERIFY(wizard.findChild<QLabel*>("wizardConnectionHelp")->text().contains("CAT"));QVERIFY(!wizard.useInterfacePtt());QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=IC7300,DEV=0"));wizard.next();wizard.next();QVERIFY(wizard.findChild<QLabel*>("wizardTransmitHelp")->text().contains("transmit-audio source"));wizard.next();QVERIFY(!wizard.findChild<QCheckBox*>("wizardInterfacePtt")->isVisible());wizard.reject();
    }
    void signalinkToneUsesTransmitLevel_data(){QTest::addColumn<int>("percent");QTest::newRow("normal TX level")<<100;QTest::newRow("reduced TX level")<<25;QTest::newRow("muted TX level")<<0;}
    void signalinkToneUsesTransmitLevel(){
        QFETCH(int,percent);QSettings().setValue("audio/txLevelPercent",percent);listing("expected-peak",QByteArray::number(qRound(32767*.65*percent/100.)));
        AudioWizard wizard("default","plughw:CARD=USB,DEV=0");wizard.show();auto* profile=wizard.findChild<QComboBox*>("wizardConnection");profile->setCurrentIndex(profile->findData("signalink"));wizard.next();wizard.next();wizard.findChild<QPushButton*>("wizardTone")->click();
        auto* status=wizard.findChild<QLabel*>("wizardOutputStatus");if(!percent){QVERIFY(status->text().contains("TX level is 0%"));QVERIFY(!QFile::exists(directory_.filePath("played")));}
        else {QTRY_VERIFY(status->text().contains("Tone finished"));QVERIFY(status->text().contains("not monitored"));QFile played(directory_.filePath("played"));QVERIFY(played.open(QIODevice::ReadOnly));QCOMPARE(played.readAll(),QByteArray("plughw:CARD=USB,DEV=0"));}
        wizard.reject();
    }
    void discoveryAndRefreshPreserveManualChoices(){
        listing("delay-list",{});AudioWizard wizard("old-input","old-output");wizard.show();auto* input=wizard.findChild<QComboBox*>("wizardInput");auto* output=wizard.findChild<QComboBox*>("wizardOutput");input->setEditText("custom-input");output->setEditText("custom-output");QTRY_VERIFY(wizard.findChild<QPushButton*>("wizardRefresh")->isEnabled());QCOMPARE(wizard.inputDevice(),QString("custom-input"));QCOMPARE(wizard.outputDevice(),QString("custom-output"));QVERIFY(wizard.findChild<QCheckBox*>("wizardAdvanced")->isChecked());
        auto* choices=wizard.findChild<QComboBox*>("wizardDevice");choices->setCurrentIndex(deviceIndex(choices,"plughw:CARD=USB,DEV=0"));listing("arecord-list","default\n    Default device\n");listing("aplay-list","default\n    Default device\n");wizard.findChild<QPushButton*>("wizardRefresh")->click();QTRY_VERIFY(wizard.findChild<QPushButton*>("wizardRefresh")->isEnabled());QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(wizard.outputDevice(),wizard.inputDevice());QCOMPARE(choices->currentIndex(),0);QVERIFY(wizard.findChild<QLabel*>("wizardDiscoveryStatus")->text().contains("No sound device"));
        listing("fail-list",{});wizard.findChild<QPushButton*>("wizardRefresh")->click();QTRY_VERIFY(wizard.findChild<QPushButton*>("wizardRefresh")->isEnabled());QVERIFY(wizard.findChild<QLabel*>("wizardDiscoveryStatus")->text().contains("Could not scan"));QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=USB,DEV=0"));wizard.reject();
    }
    void duplicateUsbNamesStaySeparate(){
        const QByteArray duplicate="plughw:CARD=CODEC,DEV=0\n    USB AUDIO CODEC\nplughw:CARD=CODEC_1,DEV=0\n    USB AUDIO CODEC\n";listing("arecord-list",duplicate);listing("aplay-list",duplicate);AudioWizard wizard("default","default");wizard.show();QTRY_VERIFY(wizard.findChild<QPushButton*>("wizardRefresh")->isEnabled());
        auto* profile=wizard.findChild<QComboBox*>("wizardConnection");profile->setCurrentIndex(profile->findData("signalink"));
        auto* choices=wizard.findChild<QComboBox*>("wizardDevice");QCOMPARE(choices->count(),4);const int first=deviceIndex(choices,"plughw:CARD=CODEC,DEV=0"),second=deviceIndex(choices,"plughw:CARD=CODEC_1,DEV=0");QVERIFY(first>=0&&second>=0);
        QVERIFY(choices->itemText(first).startsWith("SignaLink USB"));QVERIFY(choices->itemText(second).startsWith("SignaLink USB"));QVERIFY(choices->itemText(first)!=choices->itemText(second));
        choices->setCurrentIndex(second);QCOMPARE(wizard.inputDevice(),QString("plughw:CARD=CODEC_1,DEV=0"));QCOMPARE(wizard.outputDevice(),wizard.inputDevice());wizard.reject();
    }
    void devicesAndInputCleanup(){
        AudioWizard wizard("my-input","my-output");wizard.show();
        auto* input=wizard.findChild<QComboBox*>("wizardInput");auto* output=wizard.findChild<QComboBox*>("wizardOutput");
        QTRY_VERIFY(input->findText("plughw:CARD=USB,DEV=0")>=0);QTRY_VERIFY(output->findText("plughw:CARD=USB,DEV=0")>=0);
        QCOMPARE(input->currentText(),QString("my-input"));QCOMPARE(output->currentText(),QString("my-output"));
        QTest::mouseClick(wizard.button(QWizard::NextButton),Qt::LeftButton);
        auto* listen=wizard.findChild<QPushButton*>("wizardListen");auto* level=wizard.findChild<QProgressBar*>("wizardInputLevel");
        QTest::mouseClick(listen,Qt::LeftButton);QTRY_COMPARE(level->value(),50);
        QVERIFY(wizard.findChild<QLabel*>("wizardInputStatus")->text().contains("Signal present"));
        QTest::mouseClick(wizard.button(QWizard::NextButton),Qt::LeftButton);QCOMPARE(level->value(),0);QCOMPARE(listen->text(),QString("Start input meter"));
        QTest::mouseClick(wizard.button(QWizard::BackButton),Qt::LeftButton);QTest::mouseClick(listen,Qt::LeftButton);QTRY_COMPARE(level->value(),50);
        wizard.reject();QCOMPARE(level->value(),0);QCOMPARE(wizard.result(),int(QDialog::Rejected));
    }
    void outputAndFailures(){
        AudioWizard wizard("broken","my-output");wizard.show();wizard.next();
        auto* listen=wizard.findChild<QPushButton*>("wizardListen");QTest::mouseClick(listen,Qt::LeftButton);
        QTRY_VERIFY(wizard.findChild<QLabel*>("wizardInputStatus")->text().contains("unavailable"));
        wizard.next();auto* play=wizard.findChild<QPushButton*>("wizardTone");QTest::mouseClick(play,Qt::LeftButton);
        QTRY_VERIFY(wizard.findChild<QLabel*>("wizardOutputStatus")->text().contains("Tone finished"));
        QFile played(directory_.filePath("played"));QVERIFY(played.open(QIODevice::ReadOnly));QCOMPARE(played.readAll(),QByteArray("my-output"));
        wizard.back();wizard.back();wizard.findChild<QComboBox*>("wizardOutput")->setEditText("broken");wizard.next();wizard.next();QTest::mouseClick(play,Qt::LeftButton);
        QTRY_VERIFY(wizard.findChild<QLabel*>("wizardOutputStatus")->text().contains("unavailable"));
        wizard.next();wizard.findChild<QCheckBox*>("wizardOpenRadio")->setChecked(true);QTest::mouseClick(wizard.button(QWizard::FinishButton),Qt::LeftButton);
        QCOMPARE(wizard.result(),int(QDialog::Accepted));QVERIFY(wizard.openRadioControls());
    }
    void applicationCancelAndSave(){
        QSettings settings;settings.setValue("audio/input","original-input");settings.setValue("audio/output","original-output");settings.setValue("radio/automaticPtt",true);settings.setValue("audio/txLevelPercent",25);
        MainWindow window;window.show();auto* action=window.findChild<QAction*>("audioWizardAction");QVERIFY(action);QVERIFY(action->isEnabled());
        bool opened=false;
        QTimer::singleShot(10,&window,[&]{
            auto* wizard=window.findChild<AudioWizard*>();if(!wizard)return;opened=true;
            wizard->findChild<QComboBox*>("wizardConnection")->setCurrentIndex(0);wizard->findChild<QComboBox*>("wizardInput")->setEditText("changed");wizard->findChild<QSpinBox*>("wizardTxLevel")->setValue(45);wizard->reject();
        });
        openAudioSetup(window);QVERIFY(opened);QCOMPARE(settings.value("audio/input").toString(),QString("original-input"));QVERIFY(settings.value("radio/automaticPtt").toBool());QVERIFY(!settings.contains("audio/setupConnection"));QCOMPARE(settings.value("audio/txLevelPercent").toInt(),25);
        opened=false;QTimer::singleShot(10,&window,[&]{
            auto* wizard=window.findChild<AudioWizard*>();if(!wizard)return;opened=true;
            wizard->findChild<QComboBox*>("wizardInput")->setEditText("radio-input");wizard->findChild<QComboBox*>("wizardOutput")->setEditText("radio-output");
            wizard->findChild<QSpinBox*>("wizardTxLevel")->setValue(80);
            wizard->next();wizard->next();wizard->next();QTest::mouseClick(wizard->button(QWizard::FinishButton),Qt::LeftButton);
        });
        openAudioSetup(window);QVERIFY(opened);QCOMPARE(settings.value("audio/input").toString(),QString("radio-input"));QCOMPARE(settings.value("audio/output").toString(),QString("radio-output"));QCOMPARE(settings.value("audio/txLevelPercent").toInt(),80);QCOMPARE(window.findChild<QSpinBox*>("transmitAudioLevel")->value(),80);window.close();
    }
    void applicationSavesSignaLinkAndInterfacePtt(){
        QSettings settings;settings.setValue("radio/enabled",true);settings.setValue("radio/automaticPtt",true);settings.setValue("radio/model",1);settings.setValue("radio/leadMs",450);settings.setValue("audio/txLevelPercent",25);
        MainWindow window;window.show();bool saved=false,adjusting=false;QTimer choose;choose.setInterval(10);
        connect(&choose,&QTimer::timeout,&window,[&]{
            auto* wizard=window.findChild<AudioWizard*>();if(!wizard||!wizard->findChild<QPushButton*>("wizardRefresh")->isEnabled())return;
            if(!adjusting){auto* profile=wizard->findChild<QComboBox*>("wizardConnection");profile->setCurrentIndex(profile->findData("signalink"));auto* devices=wizard->findChild<QComboBox*>("wizardDevice");const int selected=deviceIndex(devices,"plughw:CARD=USB,DEV=0");QVERIFY(selected>=0);devices->setCurrentIndex(selected);wizard->next();wizard->next();adjusting=true;wizard->findChild<QPushButton*>("wizardOptimizeLevels")->click();return;}
            if(!wizard->findChild<QPushButton*>("wizardOptimizeLevels")->isEnabled())return;choose.stop();QVERIFY(wizard->findChild<QLabel*>("wizardMixerStatus")->text().contains("unmuted and raised"));wizard->next();QVERIFY(wizard->useInterfacePtt());saved=true;wizard->button(QWizard::FinishButton)->click();
        });
        QTimer::singleShot(5000,&window,[&]{if(!saved)if(auto* wizard=window.findChild<AudioWizard*>())wizard->reject();});choose.start();openAudioSetup(window);QVERIFY(saved);
        QCOMPARE(settings.value("audio/input").toString(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(settings.value("audio/output").toString(),QString("plughw:CARD=USB,DEV=0"));QCOMPARE(settings.value("audio/setupConnection").toString(),QString("signalink"));QVERIFY(!settings.value("radio/automaticPtt").toBool());QVERIFY(settings.value("radio/enabled").toBool());QCOMPARE(settings.value("radio/leadMs").toInt(),450);
        QCOMPARE(settings.value("audio/txLevelPercent").toInt(),100);QCOMPARE(window.findChild<QSpinBox*>("transmitAudioLevel")->value(),100);QCOMPARE(mixerState()["volume"].toString(),QString("100,100"));QCOMPARE(mixerState()["switch"].toString(),QString("on,on"));QCOMPARE(mixerState()["capture"].toString(),QString("7"));
        AudioWizard reopened(settings.value("audio/input").toString(),settings.value("audio/output").toString());QCOMPARE(reopened.connectionType(),QString("signalink"));QVERIFY(reopened.useInterfacePtt());reopened.reject();window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
QTEST_MAIN(AudioTests)
#include "audio_tests.moc"
