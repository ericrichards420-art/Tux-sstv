#include "mainwindow.h"
#include "radio_profiles.h"
#include "audio_levels_panel.h"
#include "remote_test_transport.h"
#include "ui_refresh_probe.h"
#include <QtWidgets>
#include <QtTest>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QMutex>

struct ProfileRadioState {std::atomic_bool keyed=false,failClose=false;std::atomic_int closeDelay=0;QMutex mutex;QList<radio::Config> opened;QStringList events;};
class ProfileRadio:public radio::Backend {
    std::shared_ptr<ProfileRadioState> state_;
public:
    explicit ProfileRadio(std::shared_ptr<ProfileRadioState> state):state_(std::move(state)){}
    void open(const radio::Config& config) override{QMutexLocker lock(&state_->mutex);state_->opened<<config;state_->events<<"open";}
    QVariantMap read(bool=false) override{return {{"frequencyHz",14230000.},{"mode","USB"},{"vfo","Main"},{"ptt",state_->keyed?1:0}};}
    void setPtt(bool on) override{state_->keyed=on;QMutexLocker lock(&state_->mutex);state_->events<<(on?"PTT ON":"PTT OFF");}
    void apply(const radio::Command&) override{}
    QString close() override{if(state_->closeDelay)QThread::msleep(state_->closeDelay);if(state_->failClose)return "Simulated disconnect failure";state_->keyed=false;QMutexLocker lock(&state_->mutex);state_->events<<"close";return {};}
};
class ProfileTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    const QString key_="0123456789abcdef0123456789abcdef";
    static profiles::Settings a(){profiles::Settings p;p.radio.enabled=true;p.radio.model=2039;p.radio.device="/dev/fake-ts990";p.radio.pttType="RIGMICDATA";p.radio.pttValue=3;p.input="radio-a-input";p.output="radio-a-output";p.txLevel=73;p.rxGain=-3;p.autoRx=false;p.connection="radio-usb";p.clockPpm=41.2;p.offsetHz=-9.5;p.autoClock=true;return p;}
    static profiles::Settings b(){auto p=a();p.radio.model=1022;p.radio.device="/dev/fake-ft857";p.radio.baud=38400;p.radio.pttType="RTS";p.radio.pttValue=1;p.radio.pttDevice="/dev/fake-ptt";p.radio.leadMs=450;p.radio.tailMs=200;p.input="radio-b-input";p.output="radio-b-output";p.connection="signalink";p.txLevel=44;p.rxGain=6;p.autoRx=true;p.autoClock=false;p.autoFrequency=true;p.clockPpm=-22.4;p.offsetHz=17.3;return p;}
    static profiles::Store prepared(){auto store=profiles::Store::initial(a());store.entries[0].name="Kenwood TS-990S";const auto first=store.active;store.change("new",{},"Yaesu FT-857D",a());store.updateCurrent(b());store.change("switch",first,{},b());return store;}
    static void seed(const profiles::Store& store){const auto p=store.current()->settings;QSettings s;s.clear();p.radio.save(s);s.setValue("audio/input",p.input);s.setValue("audio/output",p.output);s.setValue("audio/setupConnection",p.connection);s.setValue("audio/txLevelPercent",p.txLevel);s.setValue("audio/rxGainDb",p.rxGain);s.setValue("audio/autoRxLevel",p.autoRx);s.setValue("rx/clock",p.clockPpm);s.setValue("rx/offset",p.offsetHz);s.setValue("rx/autoClock",p.autoClock);s.setValue("rx/autoFrequency",p.autoFrequency);s.setValue("radioProfiles/state",QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact));s.sync();}
    static radio::Controller* controller(MainWindow& window){return window.findChild<radio::Controller*>("radioController");}
    static QComboBox* choice(MainWindow& window){return window.findChild<QComboBox*>("radioProfileChoice");}
    static void select(MainWindow& window,const QString& id){auto* combo=choice(window);combo->setCurrentIndex(combo->findData(id));QMetaObject::invokeMethod(combo,"activated",Qt::DirectConnection,Q_ARG(int,combo->currentIndex()));}
    static QString settingsFile(){return QSettings().fileName();}
    static profiles::Store readStore(const QString& file){QSettings s(file,QSettings::IniFormat);profiles::Store store;QString error;const bool okay=profiles::Store::fromJson(QJsonDocument::fromJson(s.value("radioProfiles/state").toByteArray()).object(),store,error);if(!okay)qFatal("Invalid test profile store: %s",qPrintable(error));return store;}
    template<class F> static void manage(MainWindow& window,F edit){bool seen=false;QTimer::singleShot(20,&window,[&]{auto* dialog=window.findChild<QDialog*>("radioProfileDialog");QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});seen=true;edit(*dialog);});window.findChild<QAction*>("radioProfilesAction")->trigger();QVERIFY(seen);}
    static void named(QDialog& dialog,const char* button,const QString& name){QTimer::singleShot(0,&dialog,[name]{auto* prompt=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(prompt);prompt->setTextValue(name);prompt->accept();});dialog.findChild<QPushButton*>(button)->click();}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);QCoreApplication::setOrganizationName("TuxSSTVProfileTests");QCoreApplication::setApplicationName("TuxSSTVProfileTests");QSettings::setDefaultFormat(QSettings::IniFormat);
        QFile recorder(directory_.filePath("arecord"));QVERIFY(recorder.open(QIODevice::WriteOnly));recorder.write("#!/usr/bin/python3\nimport os,sys,time\nif '-L' in sys.argv: print('default');sys.exit(0)\nwhile True:\n os.write(1,b'\\0'*480)\n time.sleep(.02)\n");recorder.close();QVERIFY(recorder.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)));QSettings().clear();}
    void storeRoundTripsAllRadioAudioAndCalibration(){
        auto store=prepared();QString error;profiles::Store restored;QVERIFY(profiles::Store::fromJson(store.toJson(),restored,error));QCOMPARE(restored.toJson(),store.toJson());QVERIFY(restored.current()->settings==a());const auto second=restored.entries[1].id;QVERIFY(restored.change("switch",second,{},a()).isEmpty());QVERIFY(restored.current()->settings==b());QCOMPARE(restored.current()->settings.radio.pttDevice,QString("/dev/fake-ptt"));
    }
    void namesIdsLimitsAndMalformedDataAreRejected(){
        auto store=prepared();const auto before=store.toJson();QVERIFY(!store.change("new",{},"kenwood ts-990s",a()).isEmpty());QVERIFY(!store.change("rename",store.active,QString(65,'x'),a()).isEmpty());QVERIFY(!store.change("delete",store.active,{},a()).isEmpty());QCOMPARE(store.toJson(),before);QString error;profiles::Store decoded;
        for(const auto field:{"txLevel","rxGain","clockPpm","autoRx","input"}){auto json=store.toJson();auto items=json["items"].toArray();auto item=items[0].toObject();auto settings=item["settings"].toObject();settings[field]=QJsonValue::Null;item["settings"]=settings;items[0]=item;json["items"]=items;QVERIFY(!profiles::Store::fromJson(json,decoded,error));}
        auto json=store.toJson();json["active"]="missing";QVERIFY(!profiles::Store::fromJson(json,decoded,error));
        while(store.entries.size()<profiles::MaxProfiles)QVERIFY(store.change("new",{},QString("Radio %1").arg(store.entries.size()),store.current()->settings).isEmpty());QVERIFY(!store.change("new",{},"One more",store.current()->settings).isEmpty());
        // The advertised list remains well inside the authenticated frame limit.
        QVERIFY(remote::frame({{"type","snapshot"},{"snapshot",QJsonObject{{"profiles",store.summary()}}}}).size()<4096);
    }
    void legacySetupMigratesWithoutChangingStationPreferences(){
        auto old=prepared();seed(old);QSettings s;s.remove("radioProfiles");s.setValue("station/call","KI5BZO");s.setValue("voice/microphone","laptop-mic");s.setValue("remote/key",key_);s.sync();auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));const auto store=readStore(s.fileName());QCOMPARE(store.entries.size(),1);QVERIFY(store.current()->settings==a());QCOMPARE(choice(window)->currentText(),QString("Current radio"));QVERIFY(!controller(window)->connected());QCOMPARE(QSettings().value("station/call").toString(),QString("KI5BZO"));QCOMPARE(QSettings().value("voice/microphone").toString(),QString("laptop-mic"));QVERIFY(QSettings().value("remote/key").toString()==key_);QVERIFY(!state->keyed);window.close();
    }
    void switchingLoadsEverySettingClosesCatAndRestoresOnRestart(){
        const auto store=prepared();seed(store);const auto file=settingsFile();auto state=std::make_shared<ProfileRadioState>();
        {MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));QCOMPARE(choice(window)->count(),2);window.findChild<QAction*>("radioConnectAction")->trigger();QTRY_VERIFY(controller(window)->connected());QCOMPARE(state->opened.last().model,2039);state->closeDelay=150;select(window,store.entries[1].id);QTRY_COMPARE(choice(window)->currentData().toString(),store.entries[1].id);QVERIFY(!controller(window)->connected());QCOMPARE(window.findChild<QLineEdit*>("inputDevice")->text(),b().input);QCOMPARE(window.findChild<QLineEdit*>("outputDevice")->text(),b().output);QCOMPARE(window.findChild<AudioLevelsPanel*>()->transmitPercent(),44);QCOMPARE(window.findChild<AudioLevelsPanel*>()->receiveGainDb(),6);QVERIFY(readStore(file).current()->settings==b());QVERIFY(!state->keyed);window.findChild<QAction*>("radioConnectAction")->trigger();QTRY_VERIFY(controller(window)->connected());QVERIFY(state->opened.last()==b().radio);window.close();}
        {MainWindow restored(nullptr,std::make_unique<ProfileRadio>(state));QCOMPARE(choice(restored)->currentData().toString(),store.entries[1].id);QVERIFY(!controller(restored)->connected());select(restored,store.entries[0].id);QTRY_COMPARE(choice(restored)->currentData().toString(),store.entries[0].id);QVERIFY(readStore(file).current()->settings==a());restored.close();}
        QVERIFY(!state->events.contains("PTT ON"));
    }
    void autosaveStaysWithTheRadioBeingEdited(){
        const auto store=prepared();seed(store);const auto file=settingsFile();auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));window.findChild<QSpinBox*>("transmitAudioLevel")->setValue(61);window.findChild<QSpinBox*>("receiveAudioGain")->setValue(-8);window.findChild<QLineEdit*>("inputDevice")->setText("changed-radio-a");QMetaObject::invokeMethod(window.findChild<QLineEdit*>("inputDevice"),"editingFinished");
        // Switch before the debounce fires: the outgoing profile must still save.
        select(window,store.entries[1].id);QTRY_COMPARE(choice(window)->currentData().toString(),store.entries[1].id);const auto saved=readStore(file);QCOMPARE(saved.entries[0].settings.input,QString("changed-radio-a"));QCOMPARE(saved.entries[0].settings.txLevel,61);QCOMPARE(saved.entries[0].settings.rxGain,-8);QVERIFY(saved.entries[1].settings==b());QTest::qWait(650);QVERIFY(readStore(file).entries[1].settings==b());window.close();
    }
    void newRenameAndDeleteUseNamedProfiles(){
        seed(prepared());auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));const auto file=settingsFile();
        manage(window,[&](QDialog& dialog){named(dialog,"profileNew","Portable radio");QTRY_COMPARE(choice(window)->currentText(),QString("Portable radio"));QCOMPARE(dialog.findChild<QComboBox*>("profileDialogChoice")->currentText(),QString("Portable radio"));named(dialog,"profileRename","Portable SignaLink");QTRY_COMPARE(choice(window)->currentText(),QString("Portable SignaLink"));QVERIFY(!dialog.findChild<QPushButton*>("profileDelete")->isEnabled());});
        auto saved=readStore(file);QCOMPARE(saved.entries.size(),3);const auto cloned=saved.active;QVERIFY(saved.current()->settings==a());select(window,saved.entries[0].id);QTRY_COMPARE(choice(window)->currentData().toString(),saved.entries[0].id);
        manage(window,[&](QDialog& dialog){auto* list=dialog.findChild<QComboBox*>("profileDialogChoice");list->setCurrentIndex(list->findData(cloned));QVERIFY(dialog.findChild<QPushButton*>("profileDelete")->isEnabled());QTimer::singleShot(0,&dialog,[]{auto* confirm=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(confirm);confirm->button(QMessageBox::Yes)->click();});dialog.findChild<QPushButton*>("profileDelete")->click();QTRY_COMPARE(choice(window)->count(),2);});QCOMPARE(readStore(file).entries.size(),2);window.close();
    }
    void failedDisconnectKeepsOldProfileAndSettings(){
        const auto store=prepared();seed(store);const auto file=settingsFile();auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));window.findChild<QAction*>("radioConnectAction")->trigger();QTRY_VERIFY(controller(window)->connected());state->failClose=true;select(window,store.entries[1].id);QTRY_VERIFY(!controller(window)->busy());QCOMPARE(choice(window)->currentData().toString(),store.entries[0].id);QVERIFY(readStore(file).current()->settings==a());QCOMPARE(window.findChild<QLineEdit*>("inputDevice")->text(),a().input);QVERIFY(window.findChild<QLabel*>("statusLabel")->text().contains("disconnect failure"));state->failClose=false;controller(window)->release();QTRY_VERIFY(!controller(window)->busy());window.close();
    }
    void failedSettingsWriteDoesNotActivateAnotherProfile(){
        const auto store=prepared();seed(store);const auto file=settingsFile();auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));QVERIFY(QFile::rename(file,file+".saved"));QVERIFY(QDir().mkdir(file));
        select(window,store.entries[1].id);QCOMPARE(choice(window)->currentData().toString(),store.entries[0].id);QCOMPARE(window.findChild<QLineEdit*>("inputDevice")->text(),a().input);QVERIFY(window.findChild<QLabel*>("statusLabel")->text().contains("could not be saved"));QVERIFY(QDir().rmdir(file));QVERIFY(QFile::rename(file+".saved",file));window.close();QCOMPARE(readStore(file).active,store.entries[0].id);
    }
    void profilesCannotChangeDuringPtt(){
        const auto store=prepared();seed(store);auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));controller(window)->connectRadio(a().radio);QTRY_VERIFY(controller(window)->connected());controller(window)->key([](QString error){QVERIFY(error.isEmpty());});QTRY_VERIFY(state->keyed);QVERIFY(!choice(window)->isEnabled());select(window,store.entries[1].id);QCOMPARE(choice(window)->currentData().toString(),store.entries[0].id);QCOMPARE(window.findChild<QLineEdit*>("inputDevice")->text(),a().input);controller(window)->release();QTRY_VERIFY(!state->keyed);window.close();
    }
    void remoteSwitchUsesShackProfilesAndPreservesLaptopSettings(){
        const auto store=prepared();seed(store);const auto hostFile=settingsFile();auto state=std::make_shared<ProfileRadioState>();MainWindow shack(nullptr,std::make_unique<ProfileRadio>(state));remote::Config host;host.role=remote::Role::Host;host.key=key_;shack.setRemoteConfig(host);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("laptop-"+QUuid::createUuid().toString(QUuid::WithoutBraces)));auto laptopSettings=a();laptopSettings.radio.enabled=false;laptopSettings.input="laptop-local-input";laptopSettings.output="laptop-local-output";laptopSettings.txLevel=88;auto local=profiles::Store::initial(laptopSettings);local.entries[0].name="Laptop local radio";seed(local);const auto laptopFile=settingsFile();MainWindow laptop;auto clientConfig=host;clientConfig.role=remote::Role::Client;clientConfig.host="shack.invalid";clientConfig.reconnect=false;laptop.setRemoteConfig(clientConfig);
        auto [server,peer]=remoteTestPair();shack.findChild<remote::Server*>()->acceptConnection(server);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_COMPARE(choice(laptop)->currentText(),QString("Kenwood TS-990S"));QCOMPARE(laptop.findChild<AudioLevelsPanel*>()->transmitPercent(),73);QVERIFY(!laptop.findChild<QSpinBox*>("transmitAudioLevel")->isEnabled());
        manage(laptop,[&](QDialog& dialog){named(dialog,"profileNew","Remote spare");QTRY_COMPARE(choice(laptop)->currentText(),QString("Remote spare"));QTRY_COMPARE(choice(shack)->currentText(),QString("Remote spare"));});QVERIFY(readStore(hostFile).current()->settings==a());QVERIFY(readStore(laptopFile).current()->settings==laptopSettings);
        bool renamePaused=false;const auto observe=connect(client,&remote::Client::snapshotChanged,&laptop,[&]{if(client->snapshot().value("preparing").toBool())renamePaused=true;});manage(laptop,[&](QDialog& dialog){named(dialog,"profileRename","Remote spare renamed");QTRY_COMPARE(choice(laptop)->currentText(),QString("Remote spare renamed"));});disconnect(observe);QVERIFY(!renamePaused);
        select(laptop,store.entries[1].id);QTRY_COMPARE(choice(shack)->currentData().toString(),store.entries[1].id);QTRY_COMPARE(choice(laptop)->currentData().toString(),store.entries[1].id);QTRY_COMPARE(laptop.findChild<AudioLevelsPanel*>()->transmitPercent(),44);QVERIFY(readStore(hostFile).current()->settings==b());QVERIFY(readStore(laptopFile).current()->settings==laptopSettings);QCOMPARE(laptop.findChild<QLineEdit*>("inputDevice")->text(),laptopSettings.input);QVERIFY(!state->keyed);
        bool done=false;QString error;client->command("profiles.change",{{"operation","switch"},{"id",store.entries[0].id},{"expected",store.revision}},[&](QString problem){error=problem;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("changed"));QCOMPARE(choice(shack)->currentData().toString(),store.entries[1].id);
        done=false;error.clear();client->command("radio.configure",{{"config",b().radio.toJson()},{"expected",b().radio.toJson()},{"expectedProfile",store.entries[0].id}},[&](QString problem){error=problem;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("profile changed"));QVERIFY(readStore(hostFile).current()->settings==b());
        for(const auto* operation:{"radio.key","radio.connect","radio.apply"}){done=false;error.clear();client->command(operation,{{"expectedProfile",store.entries[0].id}},[&](QString problem){error=problem;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("profile changed"));QVERIFY(!state->keyed);}
        clientConfig.role=remote::Role::Local;laptop.setRemoteConfig(clientConfig);QTRY_COMPARE(choice(laptop)->currentText(),QString("Laptop local radio"));QCOMPARE(laptop.findChild<AudioLevelsPanel*>()->transmitPercent(),88);QVERIFY(readStore(laptopFile).current()->settings==laptopSettings);laptop.close();shack.close();
    }
    void unchangedRadioPollsDoNotRebuildOrBlinkProfiles(){
        seed(prepared());auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));window.show();controller(window)->connectRadio(a().radio);QTRY_VERIFY(controller(window)->connected());QTRY_VERIFY(!controller(window)->busy());UiRefreshProbe events;choice(window)->installEventFilter(&events);QSignalSpy indices(choice(window),&QComboBox::currentIndexChanged);for(int i=0;i<5;++i){controller(window)->refresh();QTRY_VERIFY(!controller(window)->busy());}QCOMPARE(events.enabledChanges,0);QCOMPARE(indices.count(),0);window.close();
    }
    void damagedProfileDataKeepsRecoveryCopyAndCurrentSetup(){
        seed(prepared());QSettings s;s.setValue("radioProfiles/state","not JSON");s.sync();auto state=std::make_shared<ProfileRadioState>();MainWindow window(nullptr,std::make_unique<ProfileRadio>(state));const auto saved=readStore(s.fileName());QCOMPARE(saved.entries.size(),1);QVERIFY(saved.current()->settings==a());QSettings check(s.fileName(),QSettings::IniFormat);check.beginGroup("radioProfiles/recovery");QCOMPARE(check.childKeys().size(),1);QCOMPARE(check.value(check.childKeys()[0]).toByteArray(),QByteArray("not JSON"));window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
QTEST_MAIN(ProfileTests)
#include "profile_tests.moc"
