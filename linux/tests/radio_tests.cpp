#include "radio.h"
#include "fake_radio.h"
#include "ts990_test.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QScopeGuard>
#include <limits>

class RadioTests:public QObject {
    Q_OBJECT
private slots:
    void ts990NativeModesHaveNamesAndActualReadback(){
        Ts990Emulator emulator;QVERIFY(emulator.start().isEmpty());auto backend=radio::hamlibBackend();backend->open(emulator.config);
        for(const auto& mode:QStringList{"USB-D1","USB-D2","USB-D3","LSB-D1","LSB-D2","LSB-D3","USB","LSB"}){QVariantMap state;try{backend->apply({radio::Control::Mode,0,mode});state=backend->read();}catch(const std::exception& error){QFAIL(qPrintable(QString::fromUtf8(error.what())+"\n"+emulator.log()));}QCOMPARE(state.value("mode").toString(),mode);QVERIFY(emulator.log().contains("OM0\n"));}
        backend->apply({radio::Control::Frequency,14235000.,{}});QCOMPARE(backend->read().value("frequencyHz").toDouble(),14235000.);QVERIFY(!emulator.log().contains("TX"));QVERIFY(!emulator.log().contains("TO\n"));QVERIFY(backend->close().isEmpty());
    }
    void ts990NativePttUsesSelectedInput_data(){
        QTest::addColumn<int>("input");QTest::addColumn<QString>("method");QTest::addColumn<QByteArray>("command");QTest::newRow("default")<<1<<QString("RIG")<<QByteArray("TX\n");QTest::newRow("mic")<<2<<QString("RIG")<<QByteArray("TX0\n");QTest::newRow("data-with-generic-cat")<<3<<QString("RIG")<<QByteArray("TX1\n");QTest::newRow("data-explicit")<<3<<QString("RIGMICDATA")<<QByteArray("TX1\n");
    }
    void ts990NativePttUsesSelectedInput(){
        QFETCH(int,input);QFETCH(QString,method);QFETCH(QByteArray,command);Ts990Emulator emulator;QVERIFY(emulator.start(input,method).isEmpty());auto backend=radio::hamlibBackend();backend->open(emulator.config);backend->setPtt(true);QVERIFY2(emulator.log().contains(command),emulator.log().constData());backend->setPtt(false);QVERIFY(emulator.log().contains("RX\n"));QVERIFY(backend->close().isEmpty());
    }
    void ignoredNativeModeIsNotReportedAsSuccessful(){
        Ts990Emulator emulator;QVERIFY(emulator.start().isEmpty());QFile ignored(emulator.directory.filePath("ignore-mode"));QVERIFY(ignored.open(QIODevice::WriteOnly));ignored.close();radio::Controller controller;controller.connectRadio(emulator.config);QTRY_VERIFY(controller.connected());bool done=false;QString error;controller.apply({radio::Control::Mode,0,"USB-D1"},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("did not confirm"));QCOMPARE(controller.state().value("mode").toString(),QString("USB"));QVERIFY(!emulator.log().contains("TX"));controller.shutdown();
    }
    void allDummyControlsAreConfirmedThroughController(){
        const auto missing=QJsonObject{{"ctcssToneHz",QJsonValue::Null},{"ritHz",QJsonValue::Null}}.toVariantMap();QVERIFY(!radio::verifyReadback({radio::Control::Tone,0,{}},missing).isEmpty());QVERIFY(!radio::verifyReadback({radio::Control::Rit,0,{}},missing).isEmpty());
        radio::Controller controller;controller.connectRadio({});QTRY_VERIFY(controller.connected());
        const QList<radio::Command> commands={{radio::Control::Frequency,145550000.,{}},{radio::Control::Mode,12000,"FM"},{radio::Control::Vfo,0,"VFOB"},{radio::Control::Power,25,{}},{radio::Control::Rit,120,{}},{radio::Control::Split,0,"VFOA",1},{radio::Control::SplitFrequency,146150000.,{}},{radio::Control::RepeaterShift,0,{},2},{radio::Control::RepeaterOffset,600000,{}},{radio::Control::Tone,100,{}},{radio::Control::Tone,0,{}}};
        for(const auto& command:commands){QTRY_VERIFY(!controller.busy());bool done=false;QString error;controller.apply(command,[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));}controller.shutdown();
    }
    void ts990NativeTuningPowerRitAndSplitReadActualState(){
        Ts990Emulator emulator;QVERIFY(emulator.start().isEmpty());auto backend=radio::hamlibBackend();backend->open(emulator.config);
        const QList<radio::Command> commands={{radio::Control::Frequency,14235000.,{}},{radio::Control::Power,25,{}},{radio::Control::Rit,120,{}},{radio::Control::Rit,0,{}},{radio::Control::Vfo,0,"Sub"},{radio::Control::Mode,0,"LSB-D2"},{radio::Control::Vfo,0,"Main"},{radio::Control::Split,0,"Sub",1},{radio::Control::SplitFrequency,14240000.,{}},{radio::Control::Split,0,"Sub",0}};
        for(const auto& command:commands){try{backend->apply(command);const auto error=radio::verifyReadback(command,backend->read());QVERIFY2(error.isEmpty(),qPrintable(error+"\n"+emulator.log()));}catch(const std::exception& error){QFAIL(qPrintable(QString::fromUtf8(error.what())+"\n"+emulator.log()));}}
        QVERIFY(!emulator.log().contains("TX"));QVERIFY(!emulator.log().contains("TO\n"));QVERIFY(backend->close().isEmpty());
    }
    void configPersistenceAndValidation(){
        QTemporaryDir dir;QSettings settings(dir.filePath("radio.ini"),QSettings::IniFormat);
        radio::Config c;c.enabled=true;c.model=3073;c.device="/dev/serial/by-id/test-radio";c.baud=115200;c.pttType="RIGMICDATA";c.pttValue=3;c.extra="civaddr=0x94,serial_handshake=None";c.leadMs=450;c.tailMs=200;
        QVERIFY(c.validate().isEmpty());c.save(settings);QVERIFY(radio::Config::load(settings)==c);
        c.leadMs=-1;QVERIFY(!c.validate().isEmpty());c.leadMs=300;c.pttType="None";QVERIFY(!c.validate().isEmpty());c.automaticPtt=false;QVERIFY(c.validate().isEmpty());
        c.extra="invalid";QVERIFY(!c.validate().isEmpty());c.extra.clear();c.network=true;c.host="bad host";QVERIFY(!c.validate().isEmpty());c.host="::1";QVERIFY(c.validate().isEmpty());
    }
    void realHamlibCatalog(){
        QString error;const auto models=radio::models(&error);QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(models.size()>100);
        QVERIFY(std::any_of(models.begin(),models.end(),[](const auto& m){return m.first==1;}));
        QVERIFY(std::any_of(models.begin(),models.end(),[](const auto& m){return m.first==3073&&m.second.contains("7300");}));
        QVERIFY(std::any_of(models.begin(),models.end(),[](const auto& m){return m.first==1022&&m.second.contains("FT-857");}));
        QVERIFY(std::any_of(models.begin(),models.end(),[](const auto& m){return m.first==2039&&m.second.contains("TS-990S");}));
    }
    void configurationJsonIsTypedAndValidated(){
        radio::Config c;c.enabled=true;c.model=1022;c.device="/dev/serial/by-id/shack-cat";c.baud=38400;c.pttValue=3;c.leadMs=450;c.tailMs=200;
        radio::Config decoded;QString error;QVERIFY(radio::Config::fromJson(c.toJson(),decoded,error));QVERIFY(decoded==c);
        auto json=c.toJson();json["model"]=1022.5;QVERIFY(!radio::Config::fromJson(json,decoded,error));
        json=c.toJson();json["enabled"]="true";QVERIFY(!radio::Config::fromJson(json,decoded,error));
        json=c.toJson();json.remove("device");QVERIFY(!radio::Config::fromJson(json,decoded,error));
        json=c.toJson();json["host"]=QString(256,'x');QVERIFY(!radio::Config::fromJson(json,decoded,error));
        json=c.toJson();json["pttType"]="bad-method";QVERIFY(!radio::Config::fromJson(json,decoded,error));
        json=c.toJson();json["device"]="";QVERIFY(!radio::Config::fromJson(json,decoded,error));
        json["enabled"]=false;QVERIFY(radio::Config::fromJson(json,decoded,error));QVERIFY(!decoded.enabled);
    }
    void catControlWorksWithAudioInterfacePtt(){
        auto backend=radio::hamlibBackend();radio::Config config;config.enabled=true;config.automaticPtt=false;config.pttType="None";
        QVERIFY(config.validate().isEmpty());backend->open(config);
        backend->apply({radio::Control::Frequency,14230000.,{}});QCOMPARE(backend->read().value("frequencyHz").toDouble(),14230000.);
        backend->apply({radio::Control::Mode,2400.,"USB"});QCOMPARE(backend->read().value("mode").toString(),QString("USB"));QVERIFY(backend->close().isEmpty());
    }
    void realHamlibDummyControls(){
        auto backend=radio::hamlibBackend();radio::Config c;c.enabled=true;backend->open(c);
        QCOMPARE(backend->read().value("ptt").toInt(),0);
        backend->apply({radio::Control::Frequency,145550000,{}});QCOMPARE(backend->read()["frequencyHz"].toDouble(),145550000.);
        backend->apply({radio::Control::Mode,12000,"FM"});QCOMPARE(backend->read()["mode"].toString(),QString("FM"));
        backend->apply({radio::Control::Vfo,0,"VFOB"});QCOMPARE(backend->read()["vfo"].toString(),QString("VFOB"));
        backend->apply({radio::Control::Power,25,{}});QVERIFY(std::abs(backend->read()["powerPercent"].toDouble()-25)<.01);
        backend->apply({radio::Control::Rit,120,{}});QCOMPARE(backend->read()["ritHz"].toInt(),120);
        backend->apply({radio::Control::Split,0,"VFOA",1});QCOMPARE(backend->read()["split"].toInt(),1);
        backend->apply({radio::Control::SplitFrequency,146150000,{}});QCOMPARE(backend->read()["txFrequencyHz"].toDouble(),146150000.);
        backend->apply({radio::Control::RepeaterShift,0,{},2});QCOMPARE(backend->read()["repeaterShift"].toInt(),2);
        backend->apply({radio::Control::RepeaterOffset,600000,{}});QCOMPARE(backend->read()["repeaterOffsetHz"].toInt(),600000);
        backend->apply({radio::Control::Tone,100,{}});QCOMPARE(backend->read()["ctcssToneHz"].toDouble(),100.);
        backend->apply({radio::Control::Tone,0,{}});
        backend->setPtt(true);QCOMPARE(backend->read(true)["ptt"].toInt(),1);
        QVERIFY_EXCEPTION_THROWN(backend->apply({radio::Control::Frequency,145500000,{}}),std::runtime_error);
        backend->setPtt(false);QCOMPARE(backend->read(true)["ptt"].toInt(),0);
        QVERIFY_EXCEPTION_THROWN(backend->apply({radio::Control::Frequency,std::numeric_limits<double>::quiet_NaN(),{}}),std::runtime_error);
        QVERIFY(backend->close().isEmpty());
    }
    void controllerDummyAndShutdown(){
        radio::Controller controller;radio::Config c;c.enabled=true;QString error;bool done=false;
        controller.connectRadio(c,[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(controller.connected());
        done=false;controller.apply({radio::Control::Frequency,145600000,{}},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QCOMPARE(controller.state()["frequencyHz"].toDouble(),145600000.);
        done=false;controller.key([&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(controller.pttActive());
        QVERIFY(controller.shutdown().isEmpty());QVERIFY(!controller.connected());QVERIFY(!controller.pttActive());
    }
    void realHamlibMemoryVfoReadbackAndRecovery(){
        auto backend=radio::hamlibBackend();backend->open({});
        backend->apply({radio::Control::Vfo,0,"VFOB"});QCOMPARE(backend->read()["vfo"].toString(),QString("VFOB"));
        // Dummy accepts these VFO tokens but its frequency getter rejects MEM/VFO.
        // Hamlib 4.6.5 therefore reports an error after changing the actual VFO.
        const auto select=[&](const QString& vfo){
            try {backend->apply({radio::Control::Vfo,0,vfo});}
            catch(const std::runtime_error& error){QVERIFY2(QString::fromUtf8(error.what()).contains("Hamlib -1"),error.what());}
            QCOMPARE(backend->read()["vfo"].toString(),vfo);
        };
        select("MEM");
        backend->apply({radio::Control::Vfo,0,"VFOB"});QCOMPARE(backend->read()["vfo"].toString(),QString("VFOB"));
        select("MEM");select("VFO");
        backend->apply({radio::Control::Vfo,0,"VFOA"});QCOMPARE(backend->read()["vfo"].toString(),QString("VFOA"));
        QCOMPARE(backend->read()["ptt"].toInt(),0);QVERIFY(backend->close().isEmpty());
    }
    void realRigctldDummyServer(){
        QProcess probe;probe.start("python3",{"-c","import socket,sys\ntry: socket.socket().close()\nexcept PermissionError: sys.exit(77)"});QVERIFY(probe.waitForFinished(2000));
        if(probe.exitCode()==77)QSKIP("Loopback sockets prohibited by this environment; run the rigctld test on an unrestricted host.");
        QCOMPARE(probe.exitCode(),0);
        const int port=30000+int(QCoreApplication::applicationPid()%20000);QProcess server;
        server.start("rigctld",{"-m","1","-C","ptt_type=RIG","-T","127.0.0.1","-t",QString::number(port)});
        QVERIFY(server.waitForStarted());const auto cleanup=qScopeGuard([&]{server.terminate();server.waitForFinished(2000);});
        // Process startup does not mean the Dummy backend is accepting commands.
        // Wait only for the fixture; the product backend assertions below remain
        // single-shot, so connection or PTT failures are not retried or skipped.
        QProcess ready;ready.start("python3",{"-c",R"PY(import socket,sys,time
deadline=time.monotonic()+5
last_error="No reply"
while time.monotonic()<deadline:
    try:
        with socket.create_connection(("127.0.0.1",int(sys.argv[1])),timeout=0.25) as connection:
            connection.sendall(b"f\n")
            reply=b""
            while not reply.endswith(b"\n"):
                part=connection.recv(128)
                if not part or len(reply)>128:
                    raise OSError("Incomplete frequency reply")
                reply+=part
            if float(reply.strip())<=0:
                raise ValueError("Invalid frequency reply")
            sys.exit(0)
    except (OSError,ValueError) as error:
        last_error=str(error)
        time.sleep(0.02)
print(last_error,file=sys.stderr)
sys.exit(1)
)PY",QString::number(port)});
        const bool readyFinished=ready.waitForFinished(6000);if(!readyFinished){ready.kill();ready.waitForFinished(1000);}
        const auto readyError=QString("rigctld readiness failed (state %1, exit %2): %3; daemon stderr: %4")
            .arg(int(server.state())).arg(server.exitCode()).arg(QString::fromUtf8(ready.readAllStandardError()),QString::fromUtf8(server.readAllStandardError()));
        QVERIFY2(readyFinished&&ready.exitStatus()==QProcess::NormalExit&&ready.exitCode()==0,qPrintable(readyError));
        QVERIFY2(server.state()==QProcess::Running,server.readAllStandardError().constData());
        radio::Config config;config.network=true;config.port=port;auto backend=radio::hamlibBackend();backend->open(config);
        backend->apply({radio::Control::Frequency,145650000,{}});QCOMPARE(backend->read()["frequencyHz"].toDouble(),145650000.);
        backend->setPtt(true);QCOMPARE(backend->read(true)["ptt"].toInt(),1);backend->setPtt(false);QCOMPARE(backend->read(true)["ptt"].toInt(),0);QVERIFY(backend->close().isEmpty());
    }
    void cancellationDuringKey(){
        auto state=std::make_shared<FakeRadioState>();state->keyDelayMs=250;radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));
        controller.connectRadio({});QTRY_VERIFY(controller.connected());
        bool keyed=false,released=false;controller.key([&](QString error){keyed=error.isEmpty();});QTRY_VERIFY(state->history().contains("on-start"));
        controller.release([&](QString error){released=error.isEmpty();});QTRY_VERIFY(released);QVERIFY(!keyed);QVERIFY(!state->keyed);QVERIFY(!controller.pttActive());
        const auto events=state->history();QVERIFY(events.indexOf("off")>events.indexOf("on"));
    }
    void ambiguousKeyFailureAndReleaseRecovery(){
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));controller.connectRadio({});QTRY_VERIFY(controller.connected());
        state->failOn=true;bool done=false;controller.key([&](QString e){QVERIFY(!e.isEmpty());done=true;});QTRY_VERIFY(done);QVERIFY(state->history().contains("off"));
        controller.release();QTRY_VERIFY(!controller.pttActive());state->failOn=false;controller.key([](QString e){QVERIFY(e.isEmpty());});QTRY_VERIFY(state->keyed);
        state->failOff=true;done=false;controller.release([&](QString e){QVERIFY(!e.isEmpty());done=true;});QTRY_VERIFY(done);QVERIFY(controller.pttActive());
        controller.apply({radio::Control::Frequency,145500000,{}},[](QString e){QVERIFY(!e.isEmpty());});QVERIFY(!state->history().contains("apply"));
        state->failOff=false;controller.release();QTRY_VERIFY(!controller.pttActive());QVERIFY(!state->keyed);
    }
    void readFaultAndDisconnectCleanup(){
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));controller.connectRadio({});QTRY_VERIFY(controller.connected());
        controller.key([](QString e){QVERIFY(e.isEmpty());});QTRY_VERIFY(state->keyed);QTRY_VERIFY(!controller.busy());
        QSignalSpy fault(&controller,&radio::Controller::fault);state->failRead=true;controller.refresh();QTRY_COMPARE(fault.size(),1);QVERIFY(!controller.connected());
        controller.disconnectRadio();QTRY_VERIFY(!controller.busy());QVERIFY(!state->keyed);QVERIFY(!controller.pttActive());
    }
    void repeatedDisconnectPreservesUnconfirmedPtt_data(){
        QTest::addColumn<bool>("receiveReadback");QTest::newRow("confirmed-release")<<false;QTest::newRow("confirmed-receive")<<true;
    }
    void repeatedDisconnectPreservesUnconfirmedPtt(){
        QFETCH(bool,receiveReadback);
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));controller.connectRadio({});QTRY_VERIFY(controller.connected());
        controller.key([](QString e){QVERIFY(e.isEmpty());});QTRY_VERIFY(state->keyed);QTRY_VERIFY(!controller.busy());state->failOff=true;
        QString error;bool done=false;controller.disconnectRadio([&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());QVERIFY(controller.pttActive());QVERIFY(state->keyed);
        // The first close destroyed the connection after failing PTT OFF. A
        // second, no-op close is not evidence that the radio returned to RX.
        done=false;controller.disconnectRadio([&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(controller.pttActive());QVERIFY(!error.isEmpty());QVERIFY(!controller.error().isEmpty());QVERIFY(state->keyed);
        state->failOff=false;if(receiveReadback)state->keyed=false;
        controller.connectRadio({});QTRY_VERIFY(controller.connected());
        if(receiveReadback)QVERIFY(!controller.pttActive());
        else {QVERIFY(controller.pttActive());controller.release();QTRY_VERIFY(!controller.pttActive());QVERIFY(!state->keyed);}
        done=false;controller.disconnectRadio([&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(!controller.pttActive());
    }
    void shutdownPreservesUnconfirmedPttAfterFailedDisconnect_data(){
        QTest::addColumn<bool>("awaitCompletion");QTest::newRow("completed-disconnect")<<true;QTest::newRow("pending-disconnect")<<false;
    }
    void shutdownPreservesUnconfirmedPttAfterFailedDisconnect(){
        QFETCH(bool,awaitCompletion);
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));controller.connectRadio({});QTRY_VERIFY(controller.connected());
        controller.key([](QString e){QVERIFY(e.isEmpty());});QTRY_VERIFY(state->keyed);QTRY_VERIFY(!controller.busy());state->failOff=true;
        bool done=false;controller.disconnectRadio([&](QString e){QVERIFY(!e.isEmpty());done=true;});if(awaitCompletion){QTRY_VERIFY(done);}
        QVERIFY(!controller.shutdown().isEmpty());QVERIFY(controller.pttActive());QVERIFY(state->keyed);QVERIFY(!controller.shutdown().isEmpty());
    }
};
QTEST_GUILESS_MAIN(RadioTests)
#include "radio_tests.moc"
