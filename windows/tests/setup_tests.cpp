#include "mainwindow.h"
#include "remote_test_transport.h"
#include <QtWidgets>
#include <QtTest>

struct SetupRadioState {radio::Config opened;bool keyed=false;int keyRequests=0;};
class SetupRadio final:public radio::Backend {
    std::shared_ptr<SetupRadioState> state_;
public:
    explicit SetupRadio(std::shared_ptr<SetupRadioState> state):state_(std::move(state)){}
    void open(const radio::Config& config) override {state_->opened=config;}
    QVariantMap read(bool=false) override {return {{"ptt",state_->keyed?1:0}};}
    void setPtt(bool on) override {state_->keyed=on;if(on)++state_->keyRequests;}
    void apply(const radio::Command&) override {}
    QString close() override {state_->keyed=false;return {};}
};
class SetupTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    const QString key_="0123456789abcdef0123456789abcdef";
    void profile(const QString& name,int model=1){
        QSettings settings;settings.clear();settings.setValue("remote/role",0);
        settings.setValue("audio/input",name+"-input");settings.setValue("audio/output",name+"-output");
        settings.setValue("audio/autoRxLevel",false);
        radio::Config config;config.model=model;config.device="/dev/"+name;config.save(settings);
    }
    static QWizard* setup(MainWindow& window){return window.findChild<QWizard*>("setupWizard");}
    static void open(MainWindow& window){window.findChild<QAction*>("audioWizardAction")->trigger();}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");
        qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);
        QCoreApplication::setOrganizationName("TuxSstvSetupTests");QCoreApplication::setApplicationName("TuxSstvSetupTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        for(const auto* name:{"arecord","aplay"}){
            QFile file(directory_.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("#!/bin/sh\nexit 0\n");file.close();
            QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        }
    }
    void summaryDoesNotClaimAutomaticPttWithDisabledRadio(){
        profile("local");QSettings settings;settings.setValue("radio/enabled",false);settings.setValue("radio/automaticPtt",true);MainWindow window;bool seen=false;
        QTimer::singleShot(0,&window,[&]{auto* wizard=setup(window);QVERIFY(wizard);wizard->next();wizard->next();wizard->next();QCOMPARE(wizard->currentId(),3);const auto text=wizard->findChild<QLabel*>("setupSummary")->text();QVERIFY(text.contains("computer radio control is disabled"));QVERIFY(!wizard->findChild<QPushButton*>("setupPttTest")->isEnabled());seen=true;wizard->reject();});open(window);QVERIFY(seen);window.close();
    }
    void guidedPttTestReleasesOnLeavingReview_data(){QTest::addColumn<bool>("goBack");QTest::newRow("close wizard")<<false;QTest::newRow("back to settings")<<true;}
    void guidedPttTestReleasesOnLeavingReview(){
        QFETCH(bool,goBack);profile("local");QSettings settings;settings.setValue("radio/enabled",true);settings.setValue("radio/automaticPtt",true);auto state=std::make_shared<SetupRadioState>();MainWindow window(nullptr,std::make_unique<SetupRadio>(state));
        int stage=0;bool seen=false;QTimer drive;drive.setInterval(10);
        connect(&drive,&QTimer::timeout,&window,[&]{auto* wizard=setup(window);if(!wizard)return;
            if(stage==0){stage=1;wizard->next();wizard->next();wizard->next();QCOMPARE(wizard->currentId(),3);QCOMPARE(state->keyRequests,0);wizard->findChild<QPushButton*>("setupConnectRadio")->click();return;}
            if(stage==1&&wizard->findChild<QPushButton*>("setupPttTest")->isEnabled()){stage=2;QCOMPARE(state->keyRequests,0);wizard->findChild<QPushButton*>("setupPttTest")->click();return;}
            if(stage==2&&state->keyed){seen=true;drive.stop();if(goBack){wizard->back();QCOMPARE(wizard->currentId(),2);QTRY_VERIFY_WITH_TIMEOUT(!state->keyed,300);}wizard->reject();}
        });drive.start();QTimer::singleShot(7000,&window,[&]{if(auto* wizard=setup(window))wizard->reject();});open(window);QVERIFY(seen);QTRY_VERIFY(!state->keyed);QCOMPARE(state->keyRequests,1);window.close();
    }
    void reviewPttTestCompletesAndReturnsToReceive(){
        profile("local");QSettings settings;settings.setValue("radio/enabled",true);settings.setValue("radio/automaticPtt",true);auto state=std::make_shared<SetupRadioState>();MainWindow window(nullptr,std::make_unique<SetupRadio>(state));
        int stage=0;bool complete=false;QTimer drive;drive.setInterval(10);
        connect(&drive,&QTimer::timeout,&window,[&]{auto* wizard=setup(window);if(!wizard)return;
            if(stage==0){stage=1;wizard->next();wizard->next();wizard->next();QCOMPARE(state->keyRequests,0);wizard->findChild<QPushButton*>("setupConnectRadio")->click();return;}
            if(stage==1&&wizard->findChild<QPushButton*>("setupPttTest")->isEnabled()){stage=2;wizard->findChild<QPushButton*>("setupPttTest")->click();return;}
            if(stage==2&&state->keyed){stage=3;return;}
            if(stage==3&&!state->keyed&&wizard->findChild<QLabel*>("setupCheckStatus")->text().contains("PTT commands completed")){complete=true;drive.stop();wizard->reject();}
        });drive.start();QTimer::singleShot(6000,&window,[&]{if(auto* wizard=setup(window))wizard->reject();});open(window);QVERIFY(complete);QCOMPARE(state->keyRequests,1);QVERIFY(!state->keyed);window.close();
    }
    void cancelBeforeNextPreservesAllSettings(){
        profile("local",2039);QSettings settings;settings.setValue("remote/key",key_);settings.sync();
        const auto before=settings.allKeys();QMap<QString,QVariant> values;for(const auto& key:before)values[key]=settings.value(key);
        MainWindow window;bool seen=false;
        QTimer::singleShot(0,&window,[&]{auto* wizard=setup(window);QVERIFY(wizard);const auto close=qScopeGuard([wizard]{wizard->reject();});
            QCOMPARE(wizard->windowTitle(),QString("Tux-sstv setup wizard"));
            wizard->findChild<QComboBox*>("setupRole")->setCurrentIndex(2);
            wizard->findChild<QLineEdit*>("setupHost")->setText("new-shack.invalid");
            wizard->findChild<QLineEdit*>("setupKey")->setText("different-connection-key");seen=true;
        });open(window);QVERIFY(seen);
        for(auto it=values.cbegin();it!=values.cend();++it)QCOMPARE(settings.value(it.key()),it.value());
        window.close();
    }
    void invalidRemoteAddressStaysOnConnectionStep(){
        profile("local");MainWindow window;bool seen=false;
        QTimer::singleShot(0,&window,[&]{auto* wizard=setup(window);QVERIFY(wizard);const auto close=qScopeGuard([wizard]{wizard->reject();});
            wizard->findChild<QComboBox*>("setupRole")->setCurrentIndex(2);
            wizard->findChild<QLineEdit*>("setupKey")->setText(key_);wizard->next();
            QCOMPARE(wizard->currentId(),0);QVERIFY(wizard->findChild<QLabel*>("setupConnectionError")->text().contains("address"));
            QCOMPARE(QSettings().value("remote/role").toInt(),0);seen=true;
        });open(window);QVERIFY(seen);window.close();
    }
    void localCatSavesFromWizardAndKeepsAudio(){
        profile("local");auto state=std::make_shared<SetupRadioState>();MainWindow window(nullptr,std::make_unique<SetupRadio>(state));
        int stage=0;bool finished=false;QTimer drive;drive.setInterval(10);
        connect(&drive,&QTimer::timeout,&window,[&]{auto* wizard=setup(window);if(!wizard)return;
            if(stage==0){stage=1;wizard->next();QCOMPARE(wizard->currentId(),1);QVERIFY(wizard->findChild<QLabel*>("setupAudioTarget")->text().contains("local USB"));wizard->next();QCOMPARE(wizard->currentId(),2);
                wizard->findChild<QCheckBox*>("radioEnabled")->setChecked(true);auto* model=wizard->findChild<QComboBox*>("radioModel");const int index=model->findData(2039);QVERIFY(index>=0);model->setCurrentIndex(index);wizard->findChild<QComboBox*>("radioDevice")->setEditText("/dev/ttyUSB990");wizard->next();return;}
            if(stage==1&&wizard->currentId()==3){drive.stop();QCOMPARE(QSettings().value("radio/model").toInt(),2039);QVERIFY(QSettings().value("radio/enabled").toBool());QVERIFY(wizard->findChild<QLabel*>("setupSummary")->text().contains("ttyUSB990"));QVERIFY(!state->keyed);QVERIFY(!window.findChild<radio::Controller*>()->connected());finished=true;wizard->accept();}
        });drive.start();QTimer::singleShot(8000,&window,[&]{if(auto* wizard=setup(window))wizard->reject();});open(window);QVERIFY(finished);
        QCOMPARE(QSettings().value("audio/input").toString(),QString("local-input"));QCOMPARE(QSettings().value("audio/output").toString(),QString("local-output"));window.close();
    }
    void separateCatCableKeepsSignalinkAudio_data(){
        QTest::addColumn<int>("model");QTest::newRow("Yaesu FT-857")<<1022;QTest::newRow("another radio")<<2039;
    }
    void separateCatCableKeepsSignalinkAudio(){
        QFETCH(int,model);profile("signalink",model);QSettings settings;settings.setValue("audio/setupConnection","signalink");
        auto state=std::make_shared<SetupRadioState>();MainWindow window(nullptr,std::make_unique<SetupRadio>(state));
        int stage=0;bool finished=false;QTimer drive;drive.setInterval(10);
        connect(&drive,&QTimer::timeout,&window,[&]{auto* wizard=setup(window);if(!wizard)return;
            if(stage==0){stage=1;wizard->next();wizard->next();QCOMPARE(wizard->currentId(),2);
                wizard->findChild<QComboBox*>("radioSetupPreset")->setCurrentIndex(wizard->findChild<QComboBox*>("radioSetupPreset")->findData("signalink"));QVERIFY(wizard->findChild<QCheckBox*>("radioEnabled")->isChecked());QVERIFY(!wizard->findChild<QCheckBox*>("radioAutoPtt")->isChecked());
                QCOMPARE(wizard->findChild<QComboBox*>("radioModel")->currentData().toInt(),model);
                wizard->findChild<QComboBox*>("radioDevice")->setEditText("COM27");wizard->findChild<QComboBox*>("radioBaud")->setCurrentIndex(wizard->findChild<QComboBox*>("radioBaud")->findData(9600));wizard->next();return;}
            if(stage==1&&wizard->currentId()==3){drive.stop();const auto text=wizard->findChild<QLabel*>("setupSummary")->text();QVERIFY(text.contains("signalink-input"));QVERIFY(text.contains("signalink-output"));QVERIFY(text.contains("COM27"));QVERIFY(text.contains("CAT remains independent"));finished=true;wizard->accept();}
        });drive.start();QTimer::singleShot(8000,&window,[&]{if(auto* wizard=setup(window))wizard->reject();});open(window);QVERIFY(finished);
        const auto saved=radio::Config::load(settings);QVERIFY(saved.enabled);QVERIFY(!saved.network);QVERIFY(!saved.automaticPtt);QCOMPARE(saved.pttType,QString("None"));QCOMPARE(saved.model,model);QCOMPARE(saved.device,QString("COM27"));QCOMPARE(saved.baud,9600);
        QCOMPARE(settings.value("audio/input").toString(),QString("signalink-input"));QCOMPARE(settings.value("audio/output").toString(),QString("signalink-output"));QCOMPARE(settings.value("audio/setupConnection").toString(),QString("signalink"));
        auto* controller=window.findChild<radio::Controller*>();controller->connectRadio(saved);QTRY_VERIFY(controller->connected());QCOMPARE(state->opened.device,QString("COM27"));QCOMPARE(state->opened.pttType,QString("None"));QVERIFY(!state->keyed);window.close();
        auto restoredState=std::make_shared<SetupRadioState>();MainWindow restored(nullptr,std::make_unique<SetupRadio>(restoredState));
        QCOMPARE(restored.findChild<QLineEdit*>("inputDevice")->text(),QString("signalink-input"));QCOMPARE(restored.findChild<QLineEdit*>("outputDevice")->text(),QString("signalink-output"));QVERIFY(!restored.findChild<radio::Controller*>()->connected());restored.close();
    }
    void separateCatShortcutCancelKeepsSavedSettings(){
        profile("signalink",1022);QSettings settings;settings.setValue("audio/setupConnection","signalink");const auto before=radio::Config::load(settings);MainWindow window;bool seen=false;
        QTimer::singleShot(0,&window,[&]{auto* wizard=setup(window);QVERIFY(wizard);const auto close=qScopeGuard([wizard]{wizard->reject();});wizard->next();wizard->next();wizard->findChild<QComboBox*>("radioSetupPreset")->setCurrentIndex(wizard->findChild<QComboBox*>("radioSetupPreset")->findData("signalink"));wizard->findChild<QComboBox*>("radioDevice")->setEditText("COM42");seen=true;});open(window);QVERIFY(seen);
        QVERIFY(radio::Config::load(settings)==before);QCOMPARE(settings.value("audio/input").toString(),QString("signalink-input"));QCOMPARE(settings.value("audio/output").toString(),QString("signalink-output"));window.close();
    }
    void invalidCatStaysOnCatStep(){
        profile("local",2039);MainWindow window;bool seen=false;
        QTimer::singleShot(0,&window,[&]{auto* wizard=setup(window);QVERIFY(wizard);const auto close=qScopeGuard([wizard]{wizard->reject();});
            wizard->next();wizard->next();QCOMPARE(wizard->currentId(),2);
            wizard->findChild<QCheckBox*>("radioEnabled")->setChecked(true);
            wizard->findChild<QComboBox*>("radioDevice")->setEditText("");wizard->next();
            QCOMPARE(wizard->currentId(),2);QVERIFY(!wizard->findChild<QLabel*>("setupCatError")->text().isEmpty());QVERIFY(!QSettings().value("radio/enabled").toBool());seen=true;
        });open(window);QVERIFY(seen);window.close();
    }
    void connectedLaptopEditsShackCatAndKeepsLaptopProfile(){
        profile("shack",1022);auto state=std::make_shared<SetupRadioState>();MainWindow shack(nullptr,std::make_unique<SetupRadio>(state));
        remote::Config config;config.role=remote::Role::Host;config.key=key_;shack.setRemoteConfig(config);
        profile("laptop",1);MainWindow laptop;config.role=remote::Role::Client;config.host="shack.invalid";config.reconnect=false;laptop.setRemoteConfig(config);
        auto [host,peer]=remoteTestPair();shack.findChild<remote::Server*>()->acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());
        int stage=0;bool finished=false;QTimer drive;drive.setInterval(10);
        connect(&drive,&QTimer::timeout,&laptop,[&]{auto* wizard=setup(laptop);if(!wizard)return;
            if(stage==0){stage=1;wizard->next();QCOMPARE(wizard->currentId(),1);QVERIFY(client->connected());QVERIFY(wizard->findChild<QLabel*>("setupAudioTarget")->text().contains("Shack PC over Wi-Fi"));QVERIFY(wizard->findChild<QLabel*>("setupAudioSummary")->text().contains("shack-input"));wizard->next();
                QCOMPARE(wizard->currentId(),2);auto* model=wizard->findChild<QComboBox*>("radioModel");QCOMPARE(model->currentData().toInt(),1022);model->setCurrentIndex(model->findData(2039));wizard->findChild<QComboBox*>("radioDevice")->setEditText("/dev/shack990");wizard->findChild<QCheckBox*>("radioEnabled")->setChecked(true);wizard->next();return;}
            if(stage==1&&wizard->currentId()==3){drive.stop();QVERIFY(client->connected());QVERIFY(wizard->findChild<QLabel*>("setupSummary")->text().contains("shack990"));finished=true;wizard->accept();}
        });drive.start();QTimer::singleShot(8000,&laptop,[&]{if(auto* wizard=setup(laptop))wizard->reject();});open(laptop);QVERIFY(finished);
        QTRY_COMPARE(client->snapshot().value("radioConfig").toObject().value("model").toInt(),2039);
        auto* radio=shack.findChild<radio::Controller*>();radio::Config saved;QString error;QVERIFY(radio::Config::fromJson(client->snapshot().value("radioConfig").toObject(),saved,error));radio->connectRadio(saved);QTRY_VERIFY(radio->connected());QCOMPARE(state->opened.device,QString("/dev/shack990"));QVERIFY(!state->keyed);
        QCOMPARE(laptop.findChild<QLineEdit*>("inputDevice")->text(),QString("laptop-input"));QCOMPARE(laptop.findChild<QLineEdit*>("outputDevice")->text(),QString("laptop-output"));
        // Switching the laptop to local operation reveals its own CAT profile.
        config.role=remote::Role::Local;laptop.setRemoteConfig(config);bool localSeen=false;
        QTimer::singleShot(0,&laptop,[&]{auto* wizard=setup(laptop);QVERIFY(wizard);const auto close=qScopeGuard([wizard]{wizard->reject();});wizard->next();wizard->next();QCOMPARE(wizard->findChild<QComboBox*>("radioModel")->currentData().toInt(),1);QCOMPARE(wizard->findChild<QComboBox*>("radioDevice")->currentText(),QString("/dev/laptop"));localSeen=true;});open(laptop);QVERIFY(localSeen);
        laptop.close();shack.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);QSettings().clear();}
};
int main(int argc,char** argv){
    QTemporaryDir environment;qputenv("XDG_CONFIG_HOME",environment.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",environment.filePath("data").toUtf8());QDir().mkpath(environment.filePath("config"));
    QFile dirs(environment.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+environment.filePath("pictures").toUtf8()+"\"\n");dirs.close();
    QApplication app(argc,argv);SetupTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "setup_tests.moc"
