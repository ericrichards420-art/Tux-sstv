#include "mainwindow.h"
#include "remote_test_transport.h"
#include "core/morse.h"
#include "fsk_fixture.h"
#include "imageview.h"
#include "transmit_monitor.h"
#include "radio_ui.h"
#include "ui_refresh_probe.h"
#include "captionpreview.h"
#include "signalview.h"
#include <QtWidgets>
#include <QtTest>
#include <QMutex>
#include <QtEndian>
#include <cmath>

struct RemoteRadioState {std::atomic_bool keyed=false,failOff=false;std::atomic<double> frequency=145550000.;QMutex mutex;QString vfo="VFOA";radio::Config opened;};
class RemoteRadio:public radio::Backend {
    std::shared_ptr<RemoteRadioState> state_;
public:
    explicit RemoteRadio(std::shared_ptr<RemoteRadioState> state):state_(std::move(state)){}
    void open(const radio::Config& config) override{QMutexLocker lock(&state_->mutex);state_->opened=config;}
    QVariantMap read(bool=false) override{QMutexLocker lock(&state_->mutex);return {{"frequencyHz",state_->frequency.load()},{"mode","FM"},{"vfo",state_->vfo},{"ptt",state_->keyed?1:0}};}
    void setPtt(bool on) override{if(!on&&state_->failOff)throw std::runtime_error("PTT OFF not confirmed: simulated radio failure");state_->keyed=on;}
    void apply(const radio::Command& command) override{if(command.control==radio::Control::Frequency)state_->frequency=command.value;if(command.control==radio::Control::Vfo){QMutexLocker lock(&state_->mutex);state_->vfo=command.text;}}
    QString close() override{state_->keyed=false;return {};}
};
class RemoteUiTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    const QString key_="0123456789abcdef0123456789abcdef";
    static radio::Controller* controller(MainWindow& w){return w.findChild<radio::Controller*>("radioController");}
    static QPushButton* button(MainWindow& w,const char* name){return w.findChild<QPushButton*>(name);}
    static QLabel* label(MainWindow& w,const char* name){return w.findChild<QLabel*>(name);}
    void bridge(MainWindow& shack,MainWindow& laptop){
        auto [host,client]=remoteTestPair();shack.findChild<remote::Server*>()->acceptConnection(host);laptop.findChild<remote::Client*>()->connectDevice(client,key_);
    }
    remote::Config hostConfig(){remote::Config c;c.role=remote::Role::Host;c.key=key_;return c;}
    remote::Config clientConfig(){auto c=hostConfig();c.role=remote::Role::Client;c.host="shack.invalid";c.reconnect=false;return c;}
    void configureLocal(const QString& name,bool radio=true){
        QSettings settings;settings.setValue("remote/role",0);settings.setValue("audio/input",name+"-input");settings.setValue("audio/output",name+"-output");settings.setValue("audio/autoRxLevel",false);radio::Config config;config.enabled=radio;config.leadMs=20;config.tailMs=20;config.save(settings);
    }
    void send(MainWindow& laptop){auto* modes=laptop.findChild<QComboBox*>("transmitMode");modes->setCurrentIndex(modes->findData("bw8"));button(laptop,"playButton")->click();}
    QString playbackLog(){QFile file(directory_.filePath("playback.txt"));if(!file.open(QIODevice::ReadOnly))return {};return QString::fromUtf8(file.readAll());}
private slots:
    void remoteMorseDoesNotOverwriteFskCallsign(){
        configureLocal("laptop",false);MainWindow laptop;laptop.show();laptop.setRemoteConfig(clientConfig());
        remote::Server server;server.start(0,key_);server.setSnapshot({{"radioEnabled",false},{"radioPttActive",false},{"radio",QJsonObject{{"ptt",0}}},{"transmitting",false},{"preparing",false},{"receiving",true},{"input","shack-input"}});
        auto [host,peer]=remoteTestPair();server.acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_VERIFY(laptop.findChild<SignalView*>("signalScope")->inputActive());
        auto stream=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);const auto fsk=fskFixture("K9TEST");stream.samples.insert(stream.samples.end(),fsk.samples.begin(),fsk.samples.end());sstv::appendMorseId(stream,{"KI5BZO",20,800});stream.samples.insert(stream.samples.end(),12000,0.f);
        QByteArray pcm(qsizetype(stream.samples.size()*2),Qt::Uninitialized);for(size_t i=0;i<stream.samples.size();++i)qToLittleEndian<qint16>(qint16(std::lround(stream.samples[i]*32767)),pcm.data()+i*2);
        for(qsizetype pos=0;pos<pcm.size();pos+=4093)server.publishPcm(pcm.mid(pos,4093));
        auto* call=laptop.findChild<QLabel*>("receivedCallsign");auto* text=laptop.findChild<QLineEdit*>("receivedMorseText");auto* use=laptop.findChild<QPushButton*>("useMorseCallsign");
        QTRY_VERIFY_WITH_TIMEOUT(text->text().contains("KI5BZO"),8000);QTRY_VERIFY_WITH_TIMEOUT(use->isEnabled(),8000);QVERIFY(call->text().contains("K9TEST"));QVERIFY(!call->text().contains("KI5BZO"));
        const auto saved=laptop.findChild<QLabel*>("receiveAutoSaveInfo")->toolTip();QCOMPARE(QImage(saved).text("Callsign"),QString("K9TEST"));
        use->click();QVERIFY(call->text().contains("Morse (confirmed)"));QCOMPARE(QImage(saved).text("Callsign"),QString("KI5BZO"));QCOMPARE(QImage(saved).text("CallsignSource"),QString("Morse (confirmed)"));
        QVERIFY(client->connected());QVERIFY(laptop.findChild<SignalView*>("signalScope")->inputActive());QVERIFY(playbackLog().isEmpty());QVERIFY(!controller(laptop)->pttActive());laptop.close();
    }
    void remoteAutomaticReceiveAcceptsClippedLeaders_data(){
        QTest::addColumn<int>("silenceMs");QTest::newRow("late-open")<<0;QTest::newRow("muted-leading-tone")<<700;
    }
    void remoteAutomaticReceiveAcceptsClippedLeaders(){
        QFETCH(int,silenceMs);configureLocal("laptop",false);MainWindow laptop;laptop.show();laptop.setRemoteConfig(clientConfig());
        remote::Server server;server.start(0,key_);server.setSnapshot({{"radioEnabled",false},{"radioPttActive",false},{"radio",QJsonObject{{"ptt",0}}},{"transmitting",false},{"preparing",false},{"receiving",true},{"input","shack-input"}});
        auto [host,peer]=remoteTestPair();server.acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_VERIFY(laptop.findChild<SignalView*>("signalScope")->inputActive());
        auto* history=laptop.findChild<QListWidget*>("receiveHistory");const auto paths=[&]{QSet<QString> result;for(int i=0;i<history->count();++i){const auto path=history->item(i)->data(Qt::UserRole).toString();if(!path.isEmpty())result.insert(path);}return result;};const auto before=paths();
        sstv::Audio stream;const auto* mode=sstv::modeById("bw8");
        for(const auto color:{Qt::white,Qt::black}){
            QImage image(160,120,QImage::Format_RGB32);image.fill(color);const auto audio=sstv::encode(image,*mode,12000);
            stream.samples.insert(stream.samples.end(),size_t(silenceMs*12),0.f);
            stream.samples.insert(stream.samples.end(),audio.samples.begin()+6000,audio.samples.end());stream.samples.insert(stream.samples.end(),6000,0.f);
        }
        QByteArray pcm(qsizetype(stream.samples.size()*2),Qt::Uninitialized);for(size_t i=0;i<stream.samples.size();++i)qToLittleEndian<qint16>(qint16(std::lround(stream.samples[i]*32767)),pcm.data()+i*2);
        for(qsizetype pos=0;pos<pcm.size();pos+=4093)server.publishPcm(pcm.mid(pos,4093));
        QTRY_COMPARE_WITH_TIMEOUT(paths().size(),before.size()+2,8000);QVERIFY(client->connected());QVERIFY(laptop.findChild<SignalView*>("signalScope")->inputActive());
        QVERIFY(!controller(laptop)->pttActive());QVERIFY(playbackLog().isEmpty());laptop.close();
    }
    void stationAudioFieldsKeepTheirGeometryWithRemoteDevices(){
        configureLocal("laptop",false);MainWindow laptop;laptop.setRemoteConfig(clientConfig());
        remote::Server server;server.start(0,key_,QHostAddress::LocalHost);
        const QString inputName="plughw:CARD=Kenwood_TS_990S_USB_AUDIO_CODEC_CAPTURE,DEV=0",outputName="plughw:CARD=Kenwood_TS_990S_USB_AUDIO_CODEC_PLAYBACK,DEV=0";
        QJsonObject radio{{"ptt",0},{"strengthDb",-42}};QJsonObject state{{"radioEnabled",false},{"radioConnected",false},{"radioPttActive",false},{"radio",radio},{"transmitting",false},{"preparing",false},{"receiving",true},{"input",inputName},{"output",outputName}};
        server.setSnapshot(state);auto [host,peer]=remoteTestPair();server.acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());
        laptop.resize(1100,850);laptop.findChild<QTabWidget*>("imageTabs")->setCurrentIndex(0);laptop.show();QTest::qWait(40);
        auto* input=laptop.findChild<QLineEdit*>("inputDevice");auto* output=laptop.findChild<QLineEdit*>("outputDevice");auto* info=label(laptop,"recordingInfo");QVERIFY(!input->visibleRegion().isEmpty());
        laptop.findChild<QScrollArea*>("operatingControlsScroll")->ensureWidgetVisible(info,0,0);QTest::qWait(30);QVERIFY(!info->visibleRegion().isEmpty());
        const auto geometry=[&](QWidget* field){return QRect(field->mapTo(&laptop,QPoint{}),field->size());};const auto inputRect=geometry(input),outputRect=geometry(output),infoRect=geometry(info);const auto text=info->text();
        QVERIFY(info->toolTip().contains(inputName));QVERIFY(info->toolTip().contains(outputName));
        QSignalSpy pcm(client,&remote::Client::pcm);QByteArray sound(2400,0);for(int i=1;i<sound.size();i+=2)sound[i]=char(0x10);server.publishPcm(sound);QTRY_COMPARE(pcm.size(),1);QTest::qWait(30);
        UiRefreshProbe inputs,outputs,summary;input->installEventFilter(&inputs);output->installEventFilter(&outputs);info->installEventFilter(&summary);
        for(int i=0;i<8;++i){radio["strengthDb"]=-42+i;state["radio"]=radio;server.setSnapshot(state);server.publishPcm(i%2?sound:QByteArray(2400,0));QTRY_COMPARE(pcm.size(),i+2);QCOMPARE(info->text(),text);QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QCOMPARE(geometry(info),infoRect);}
        QCOMPARE(inputs.enabledChanges,0);QCOMPARE(outputs.enabledChanges,0);QCOMPARE(summary.paints,0);
        state["input"]="USB";state["output"]="USB";server.setSnapshot(state);QTRY_VERIFY(info->text()!=text);QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QCOMPARE(geometry(info),infoRect);
        button(laptop,"recordButton")->click();server.publishPcm(sound);QTRY_COMPARE(pcm.size(),10);QTRY_VERIFY(info->text().contains("Recording"));QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QCOMPARE(info->height(),infoRect.height());
        button(laptop,"recordButton")->click();QTRY_VERIFY(info->text().contains("USB"));QCOMPARE(geometry(input),inputRect);QCOMPARE(geometry(output),outputRect);QVERIFY(!input->isEnabled());QVERIFY(!output->isEnabled());laptop.close();
    }
    void remoteAudioPacketsKeepReceiveStatusAndAddressSteady(){
        configureLocal("laptop",false);MainWindow laptop;auto config=clientConfig();config.host="192.0.2.25";laptop.setRemoteConfig(config);
        remote::Server server;server.start(0,key_,QHostAddress::LocalHost);
        QJsonObject radio{{"ptt",0},{"strengthDb",-42}};
        QJsonObject state{{"radioEnabled",false},{"automaticPtt",false},{"radioConnected",false},{"radioPttActive",false},{"radio",radio},{"transmitting",false},{"preparing",false},{"receiving",true},{"input","shack-input"},{"output","shack-output"}};
        server.setSnapshot(state);auto [host,peer]=remoteTestPair();server.acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());
        laptop.show();laptop.findChild<QAction*>("radioTabAction")->trigger();auto* scope=laptop.findChild<SignalView*>("signalScope");QTRY_VERIFY(scope->inputActive());QTest::qWait(50);
        auto* status=label(laptop,"liveAudioState");auto* info=label(laptop,"recordingInfo");auto* connection=label(laptop,"remoteStationStatus");QVERIFY(status->isVisible());
        const auto steady=status->text(),deviceInfo=info->text(),connectionInfo=connection->text();
        QSignalSpy pcm(client,&remote::Client::pcm);QByteArray silence(2400,0),sound(2400,0);for(int i=1;i<sound.size();i+=2)sound[i]=char(0x10);
        server.publishPcm(sound);QTRY_COMPARE(pcm.size(),1);QCOMPARE(status->text(),steady);QCOMPARE(info->text(),deviceInfo);
        // Let the audio meters replace their initial placeholders before probing
        // repeated updates; their first reading can legitimately relayout a panel.
        QTest::qWait(30);UiRefreshProbe display;status->installEventFilter(&display);
        for(int i=0;i<10;++i){
            server.publishPcm(i%2?sound:silence);QTRY_COMPARE(pcm.size(),i+2);
            QCOMPARE(status->text(),steady);QCOMPARE(info->text(),deviceInfo);
            radio["strengthDb"]=-42+i;state["radio"]=radio;server.setSnapshot(state);QCoreApplication::processEvents();
            QCOMPARE(status->text(),steady);QCOMPARE(info->text(),deviceInfo);QCOMPARE(connection->text(),connectionInfo);
        }
        QVERIFY(steady.startsWith("Receiving automatically"));QCOMPARE(steady.section('\n',-1),QString("Shack PC · 192.0.2.25"));QVERIFY(scope->historyRows()>5);QCOMPARE(display.paints,0);
        const auto preview=qEnvironmentVariable("OPENSSTV_STATUS_PREVIEW");if(!preview.isEmpty())QVERIFY(status->parentWidget()->grab().save(preview));
        button(laptop,"recordButton")->click();QTRY_VERIFY(status->text().startsWith("Recording shack audio"));server.publishPcm(sound);QTRY_COMPARE(pcm.size(),12);QVERIFY(info->text().contains(config.host));QVERIFY(!info->text().contains("laptop-input"));
        button(laptop,"recordButton")->click();QTRY_COMPARE(status->text(),steady);QCOMPARE(info->text(),deviceInfo);
        state["transmitting"]=true;server.setSnapshot(state);QTRY_VERIFY(status->text().startsWith("Remote RX paused"));QCOMPARE(status->text().section('\n',-1),QString("Shack PC · 192.0.2.25"));
        state["transmitting"]=false;state["receiving"]=false;state["receiveError"]="USB audio input unavailable";server.setSnapshot(state);QTRY_VERIFY(status->text().startsWith("Shack audio error"));QVERIFY(status->toolTip().contains("USB audio input unavailable"));QVERIFY(status->text().contains(config.host));
        state["receiveError"]="";server.setSnapshot(state);QTRY_VERIFY(status->text().startsWith("Waiting for shack audio"));QVERIFY(status->toolTip().isEmpty());
        state["receiving"]=true;server.setSnapshot(state);QTRY_COMPARE(status->text(),steady);
        client->disconnectFromHost();QTRY_VERIFY(status->text().startsWith("Remote RX · Not connected"));QVERIFY(status->text().contains(config.host));laptop.close();
    }
    void remoteMeterUpdatesKeepControlsAndPictureSteady(){
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());
        remote::Server server;server.start(0,key_,QHostAddress::LocalHost);
        QJsonObject radio{{"frequencyHz",145550000.},{"mode","FM"},{"vfo","VFOA"},{"ptt",0},{"strengthDb",-42}};
        QJsonObject state{{"radioEnabled",true},{"automaticPtt",false},{"radioConnected",true},{"radioPttActive",false},{"radio",radio},{"transmitting",false},{"preparing",false},{"receiving",true},{"input","shack-input"},{"output","shack-output"}};
        server.setSnapshot(state);auto [host,peer]=remoteTestPair();server.acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        laptop.show();laptop.findChild<QAction*>("radioTabAction")->trigger();QTest::qWait(80);
        auto* input=laptop.findChild<QLineEdit*>("inputDevice");auto* output=laptop.findChild<QLineEdit*>("outputDevice");auto* badge=label(laptop,"radioFrontendBadge");QVERIFY(badge);QVERIFY(!input->isEnabled());QVERIFY(!output->isEnabled());
        UiRefreshProbe inputs,outputs,badges;input->installEventFilter(&inputs);output->installEventFilter(&outputs);badge->installEventFilter(&badges);
        auto* frequency=laptop.findChild<QDoubleSpinBox*>("radioFrontendFrequencyEntry");frequency->setValue(145.625);
        for(int i=0;i<12;++i){radio["strengthDb"]=-42+i;state["radio"]=radio;server.setSnapshot(state);QCoreApplication::processEvents();}
        QTRY_COMPARE(client->snapshot().value("radio").toObject().value("strengthDb").toInt(),-31);
        QCOMPARE(inputs.enabledChanges,0);QCOMPARE(outputs.enabledChanges,0);QCOMPARE(badges.styleChanges,0);QCOMPARE(frequency->value(),145.625);QCOMPARE(badge->text(),QString("RX"));
        QCOMPARE(laptop.findChild<QWidget*>("radioFrontendMeter")->property("readingDb").toInt(),-31);
        // A TX picture is unchanged while radio-only snapshots arrive.
        auto* tabs=laptop.findChild<QTabWidget*>("imageTabs");tabs->setCurrentWidget(laptop.findChild<QWidget*>("transmitPage"));QTest::qWait(30);
        auto* picture=laptop.findChild<CaptionPreview*>("transmitImage");QVERIFY(picture->isVisible());UiRefreshProbe pictures;picture->installEventFilter(&pictures);
        for(int i=0;i<8;++i){radio["strengthDb"]=-30+i;state["radio"]=radio;server.setSnapshot(state);QCoreApplication::processEvents();}
        QTRY_COMPARE(client->snapshot().value("radio").toObject().value("strengthDb").toInt(),-23);QCoreApplication::processEvents();QCOMPARE(pictures.paints,0);
        radio["ptt"]=1;state["radio"]=radio;state["radioPttActive"]=true;state["transmitting"]=true;server.setSnapshot(state);QTRY_COMPARE(badge->text(),QString("TX"));QVERIFY(!button(laptop,"playButton")->isEnabled());QVERIFY(badges.styleChanges>0);
        radio["ptt"]=0;state["radio"]=radio;state["radioPttActive"]=false;state["transmitting"]=false;server.setSnapshot(state);QTRY_COMPARE(badge->text(),QString("RX"));QTRY_VERIFY(button(laptop,"playButton")->isEnabled());QVERIFY(!input->isEnabled());QVERIFY(!output->isEnabled());laptop.close();
    }
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_REMOTE_TEST_DIR",directory_.path().toUtf8());
        QCoreApplication::setOrganizationName("OpenSSTVRemoteTests");QCoreApplication::setApplicationName("OpenSSTVRemoteTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        QFile recorder(directory_.filePath("arecord"));QVERIFY(recorder.open(QIODevice::WriteOnly));recorder.write(R"(#!/usr/bin/python3
import os,sys,time
device=sys.argv[sys.argv.index('-D')+1]
with open(os.environ['OPENSSTV_REMOTE_TEST_DIR']+'/capture.txt','a') as f:f.write(device+'\n')
if device != 'shack-input':sys.exit(3)
with open(os.environ['OPENSSTV_REMOTE_TEST_DIR']+'/receive.raw','rb') as f:data=f.read()
root=os.environ['OPENSSTV_REMOTE_TEST_DIR']
if os.environ.get('OPENSSTV_REMOTE_TEST_STALL_CAPTURE')=='1' and not os.path.exists(root+'/stalled-capture'):
    open(root+'/stalled-capture','w').write('yes')
    sys.stdout.buffer.write(data[:48000]);sys.stdout.buffer.flush()
    while True:time.sleep(1)
time.sleep(.3)
for i in range(0,len(data),4096):
    sys.stdout.buffer.write(data[i:i+4096]);sys.stdout.buffer.flush();time.sleep(.025)
while True:
    sys.stdout.buffer.write(bytes(2400));sys.stdout.buffer.flush();time.sleep(.1)
)");recorder.close();QVERIFY(recorder.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write(R"(#!/usr/bin/python3
import os,sys,time,shutil
device=sys.argv[sys.argv.index('-D')+1]
shutil.copyfile(sys.argv[-1],os.environ['OPENSSTV_REMOTE_TEST_DIR']+'/played.wav')
with open(os.environ['OPENSSTV_REMOTE_TEST_DIR']+'/playback.txt','a') as f:f.write(device+'\n')
time.sleep(float(os.environ.get('OPENSSTV_REMOTE_TEST_PLAY_SECONDS','1')))
)");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QImage first(160,120,QImage::Format_RGB32),second(160,120,QImage::Format_RGB32);first.fill(Qt::white);second.fill(Qt::darkGray);
        const auto* mode=sstv::modeById("bw8");auto one=sstv::encode(first,*mode,12000);auto two=sstv::encode(second,*mode,12000);one.samples.insert(one.samples.end(),6000,0);one.samples.insert(one.samples.end(),two.samples.begin(),two.samples.end());one.samples.insert(one.samples.end(),6000,0);sstv::writeWav(directory_.filePath("receive.wav"),one);
        QFile wav(directory_.filePath("receive.wav"));QVERIFY(wav.open(QIODevice::ReadOnly));wav.read(44);QFile raw(directory_.filePath("receive.raw"));QVERIFY(raw.open(QIODevice::WriteOnly));raw.write(wav.readAll());
    }
    void init(){QSettings().clear();QFile::remove(directory_.filePath("capture.txt"));QFile::remove(directory_.filePath("playback.txt"));QFile::remove(directory_.filePath("stalled-capture"));qunsetenv("OPENSSTV_REMOTE_TEST_STALL_CAPTURE");qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","1");}
    void shackCaptureStallRecoversForLaptop(){
        qputenv("OPENSSTV_REMOTE_TEST_STALL_CAPTURE","1");configureLocal("shack",false);MainWindow shack;shack.show();shack.setRemoteConfig(hostConfig());
        configureLocal("laptop",false);MainWindow laptop;laptop.show();laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());
        QTRY_COMPARE_WITH_TIMEOUT(laptop.findChild<QLabel*>("detectedReceiveMode")->property("modeId").toString(),QString("bw8"),4000);
        const auto opens=[&]() -> qsizetype {QFile file(directory_.filePath("capture.txt"));if(!file.open(QIODevice::ReadOnly))return 0;return file.readAll().count("shack-input");};
        QTRY_COMPARE_WITH_TIMEOUT(opens(),2,15000);
        QTRY_VERIFY_WITH_TIMEOUT(shack.findChild<QLabel*>("receiveInfo")->text().contains("120/120"),10000);
        QTRY_VERIFY_WITH_TIMEOUT(laptop.findChild<QLabel*>("receiveInfo")->text().contains("120/120"),10000);
        const auto savedGray=[&]{QImage image(laptop.findChild<QLabel*>("receiveAutoSaveInfo")->toolTip());return !image.isNull()&&std::abs(qGray(image.pixel(80,60))-qGray(QColor(Qt::darkGray).rgb()))<5;};
        QTRY_VERIFY_WITH_TIMEOUT(savedGray(),10000);
        QVERIFY(client->connected());QVERIFY(laptop.findChild<SignalView*>("signalScope")->inputActive());QCOMPARE(opens(),2);
        QVERIFY(!controller(shack)->pttActive());QVERIFY(playbackLog().isEmpty());laptop.close();shack.close();
    }
    void laptopHamlibSetupEditsShackAndKeepsLocalConfiguration(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");{QSettings s;s.setValue("radio/model",3073);s.setValue("radio/device","/dev/laptop-cat");}MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        bool edited=false;QTimer::singleShot(30,&laptop,[&]{
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});QCOMPARE(dialog->objectName(),QString("radioSetupDialog"));QVERIFY(dialog->windowTitle().contains("Hamlib"));QVERIFY(!dialog->findChild<QComboBox*>("remoteRole"));
            auto* page=dialog->findChild<RadioSettingsPage*>();QVERIFY(page&&page->isEnabled());QCOMPARE(page->config().model,1);auto* models=page->findChild<QComboBox*>("radioModel");QVERIFY(models->count()>100);QVERIFY(models->findData(1022)>=0);models->setCurrentIndex(models->findData(1022));page->findChild<QComboBox*>("radioDevice")->setCurrentText("/dev/shack-cat");page->findChild<QSpinBox*>("radioLeadMs")->setValue(450);
            auto* save=dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save);QTRY_VERIFY(save->isEnabled());save->click();QTRY_COMPARE(dialog->result(),int(QDialog::Accepted));edited=true;
        });button(laptop,"radioFrontendSettings")->click();QVERIFY(edited);QTRY_COMPARE(client->snapshot().value("radioConfig").toObject().value("model").toInt(),1022);QVERIFY(!controller(shack)->connected());QVERIFY(!state->keyed.load());QVERIFY(client->connected());
        laptop.findChild<QAction*>("radioConnectAction")->trigger();QTRY_VERIFY(controller(shack)->connected());{QMutexLocker lock(&state->mutex);QCOMPARE(state->opened.model,1022);QCOMPARE(state->opened.device,QString("/dev/shack-cat"));QCOMPARE(state->opened.leadMs,450);}QVERIFY(!state->keyed.load());
        remote::Config local;laptop.setRemoteConfig(local);bool preserved=false;QTimer::singleShot(30,&laptop,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});auto* page=dialog->findChild<RadioSettingsPage*>();QVERIFY(page);QCOMPARE(page->config().model,3073);QCOMPARE(page->config().device,QString("/dev/laptop-cat"));preserved=true;});laptop.findChild<QAction*>("radioSetupAction")->trigger();QVERIFY(preserved);laptop.close();shack.close();
    }
    void pairedShackCanOpenHamlibAndSaveWithoutUnpairing(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());bool edited=false;
        QTimer::singleShot(30,&shack,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});auto* page=dialog->findChild<RadioSettingsPage*>();QVERIFY(page&&page->isEnabled());page->findChild<QSpinBox*>("radioTailMs")->setValue(250);auto* save=dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save);QTRY_VERIFY(save->isEnabled());save->click();QTRY_COMPARE(dialog->result(),int(QDialog::Accepted));edited=true;});button(shack,"radioFrontendSettings")->click();QVERIFY(edited);QTRY_COMPARE(client->snapshot().value("radioConfig").toObject().value("tailMs").toInt(),250);QVERIFY(client->connected());QVERIFY(!state->keyed.load());laptop.close();shack.close();
    }
    void pairedShackGeneralSettingsRemainAccessible(){
        configureLocal("shack");MainWindow shack;shack.setRemoteConfig(hostConfig());configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());auto* settings=shack.findChild<QAction*>("settingsAction");QVERIFY(settings->isEnabled());bool seen=false;
        QTimer::singleShot(30,&shack,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});QCOMPARE(dialog->objectName(),QString("settingsDialog"));QVERIFY(dialog->findChild<QLabel*>("settingsStationConnection")->text().contains("Shared"));QVERIFY(dialog->findChild<QPushButton*>("settingsOpenAudioWizard")->isEnabled());auto* tabs=dialog->findChild<QTabWidget*>("settingsTabs");tabs->setCurrentIndex(1);QVERIFY(dialog->findChild<QPushButton*>("settingsShackRadioSetup")->isEnabled());QTRY_VERIFY(client->snapshot().value("preparing").toBool());QVERIFY(!shack.findChild<remote::Server*>()->reserveLocalTransmit().isEmpty());seen=true;});settings->trigger();QVERIFY(seen);QVERIFY(client->connected());QTRY_VERIFY(!client->snapshot().value("preparing").toBool());laptop.close();shack.close();
    }
    void offlineRemoteModeCanSwitchToLocalTs990Setup(){
        configureLocal("laptop");auto state=std::make_shared<RemoteRadioState>();MainWindow laptop(nullptr,std::make_unique<RemoteRadio>(state));laptop.setRemoteConfig(clientConfig());bool saved=false;
        QTimer::singleShot(30,&laptop,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});QCOMPARE(dialog->objectName(),QString("radioSetupDialog"));auto* local=dialog->findChild<QPushButton*>("radioSetupUseLocal");QVERIFY(local&&local->isEnabled());
            QTimer::singleShot(50,&laptop,[&]{auto* settings=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(settings);const auto close=qScopeGuard([settings]{if(settings->isVisible())settings->reject();});QCOMPARE(settings->objectName(),QString("settingsDialog"));QVERIFY(settings->findChild<QLabel*>("settingsStationConnection")->text().contains("Local USB"));auto* page=settings->findChild<RadioSettingsPage*>();QVERIFY(page&&page->isEnabled());auto* model=page->findChild<QComboBox*>("radioModel");const auto index=model->findData(2039);QVERIFY(index>=0);QVERIFY(model->itemText(index).contains("TS-990S"));model->setCurrentIndex(index);page->findChild<QComboBox*>("radioDevice")->setCurrentText("/dev/ts990-test");settings->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();saved=true;});local->click();
        });laptop.findChild<QAction*>("radioSetupAction")->trigger();QTRY_VERIFY(saved);QCOMPARE(QSettings().value("remote/role").toInt(),0);QCOMPARE(QSettings().value("remote/key").toString(),key_);laptop.findChild<QAction*>("radioConnectAction")->trigger();QTRY_VERIFY(controller(laptop)->connected());{QMutexLocker lock(&state->mutex);QCOMPARE(state->opened.model,2039);QCOMPARE(state->opened.device,QString("/dev/ts990-test"));}QVERIFY(!state->keyed.load());laptop.close();
    }
    void remoteHamlibChangesRejectBusyStaleAndMalformedRequests(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());const auto original=client->snapshot().value("radioConfig").toObject();auto changed=original;changed["leadMs"]=450;
        QString error;bool done=false;auto malformed=changed;malformed["model"]="1022";client->command("radio.configure",{{"config",malformed},{"expected",original}},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("Invalid"));
        done=false;auto stale=original;stale["leadMs"]=999;client->command("radio.configure",{{"config",changed},{"expected",stale}},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("changed"));
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","30");send(shack);QTRY_VERIFY(state->keyed.load());done=false;client->command("radio.configure",{{"config",changed},{"expected",original}},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("Stop transmission"));QCOMPARE(client->snapshot().value("radioConfig").toObject(),original);button(shack,"stopButton")->click();QTRY_VERIFY(!state->keyed.load());laptop.close();shack.close();
    }
    void disconnectedLaptopShowsHamlibUpdateHintInsteadOfPairing(){
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bool seen=false;QTimer::singleShot(30,&laptop,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});QCOMPARE(dialog->objectName(),QString("radioSetupDialog"));QVERIFY(dialog->findChild<QLabel*>("radioSetupLocation")->text().contains("1.3.2"));QVERIFY(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->isEnabled());seen=true;});laptop.findChild<QAction*>("radioSetupAction")->trigger();QVERIFY(seen);bool linked=false;
        QTimer::singleShot(30,&laptop,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});auto* tabs=dialog->findChild<QTabWidget*>("settingsTabs");QVERIFY(tabs);tabs->setCurrentIndex(1);QVERIFY(dialog->findChild<QPushButton*>("settingsShackRadioSetup")->isEnabled());linked=true;});laptop.findChild<QAction*>("settingsAction")->trigger();QVERIFY(linked);laptop.close();
    }
    void receiveTuneMemoryAndTransmitAtShack(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.show();shack.setRemoteConfig(hostConfig());shack.findChild<QAction*>("radioConnectAction")->trigger();QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.show();laptop.setRemoteConfig(clientConfig());auto* history=laptop.findChild<QListWidget*>("receiveHistory");QVERIFY(history);QSet<QString> before;for(int i=0;i<history->count();++i)before.insert(history->item(i)->data(Qt::UserRole).toString());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        QVERIFY(button(shack,"playButton")->isEnabled());QVERIFY(button(laptop,"playButton")->isEnabled());QVERIFY(!button(laptop,"audioLevelTone")->isEnabled());QVERIFY(!laptop.findChild<QLineEdit*>("inputDevice")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(laptop.findChild<QWidget*>("receiveAudioMeter")->property("hasReading").toBool(),4000);QTRY_VERIFY_WITH_TIMEOUT(!dynamic_cast<ImageView*>(laptop.findChild<QLabel*>("receiveImage"))->imageSize().isEmpty(),10000);
        // Both simulated stations save into this test's picture directory.
        // Wait for both source images, rather than counting two saves of the
        // same first image or assuming the two newest files are different.
        const auto receivedBoth=[&]{bool white=false,gray=false;for(int i=0;i<history->count();++i){const auto path=history->item(i)->data(Qt::UserRole).toString();if(before.contains(path))continue;QImage image(path);if(image.isNull())continue;const int shade=qGray(image.pixel(80,60));white|=shade>240;gray|=std::abs(shade-qGray(QColor(Qt::darkGray).rgb()))<10;}return white&&gray;};
        QTRY_VERIFY_WITH_TIMEOUT(receivedBoth(),10000);
        controller(laptop)->apply({radio::Control::Frequency,145525000,{}});QTRY_COMPARE(state->frequency.load(),145525000.);QTRY_COMPARE(controller(laptop)->state().value("frequencyHz").toDouble(),145525000.);
        controller(laptop)->apply({radio::Control::Vfo,0,"MEM"});QTRY_COMPARE(controller(laptop)->state().value("vfo").toString(),QString("MEM"));controller(laptop)->apply({radio::Control::Vfo,0,"VFOA"});QTRY_COMPARE(controller(laptop)->state().value("vfo").toString(),QString("VFOA"));
        send(laptop);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);QTRY_VERIFY(playbackLog().contains("shack-output"));QVERIFY(!playbackLog().contains("laptop"));QTRY_VERIFY_WITH_TIMEOUT(!state->keyed.load(),5000);QTRY_VERIFY(!client->transmitting());QTRY_COMPARE(laptop.findChild<QProgressBar*>("transmitProgress")->value(),100);
        QFile capture(directory_.filePath("capture.txt"));QVERIFY(capture.open(QIODevice::ReadOnly));QVERIFY(!capture.readAll().contains("laptop"));QVERIFY(button(laptop,"playButton")->isEnabled());laptop.close();shack.close();
    }
    void stopAndLostConnectionReleasePtt(){
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","30");configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        send(laptop);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);button(laptop,"stopButton")->click();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!controller(shack)->pttActive());QTRY_VERIFY(button(laptop,"playButton")->isEnabled());
        send(laptop);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);client->disconnectFromHost();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!controller(shack)->pttActive());QVERIFY(!button(laptop,"playButton")->isEnabled());
        bridge(shack,laptop);QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());QTest::qWait(250);QVERIFY(!state->keyed.load());QVERIFY(!client->transmitting());QTRY_VERIFY(button(laptop,"playButton")->isEnabled());laptop.close();shack.close();
    }
    void signalinkDoesNotNeedCatAndLocalSettingsSurvive(){
        configureLocal("shack",false);MainWindow shack;shack.setRemoteConfig(hostConfig());configureLocal("laptop",false);MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QVERIFY(!controller(laptop)->connected());send(laptop);QTRY_VERIFY_WITH_TIMEOUT(playbackLog().contains("shack-output"),5000);QVERIFY(!playbackLog().contains("laptop"));button(laptop,"stopButton")->click();
        remote::Config local;laptop.setRemoteConfig(local);QCOMPARE(laptop.findChild<QLineEdit*>("inputDevice")->text(),QString("laptop-input"));QCOMPARE(laptop.findChild<QLineEdit*>("outputDevice")->text(),QString("laptop-output"));QTRY_VERIFY(button(laptop,"playButton")->isEnabled());laptop.close();shack.close();
    }
    void failedPttReleaseRemainsVisibleAndCanBeRetried(){
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","30");configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(controller(laptop)->connected());send(laptop);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);
        state->failOff=true;bool done=false;QString failure;controller(laptop)->release([&](QString error){failure=error;done=true;});QTRY_VERIFY(done);QVERIFY(failure.contains("PTT OFF not confirmed"));QVERIFY(state->keyed.load());QVERIFY(controller(laptop)->pttActive());QVERIFY(!button(laptop,"playButton")->isEnabled());
        state->failOff=false;done=false;controller(laptop)->release([&](QString error){failure=error;done=true;});QTRY_VERIFY(done);QVERIFY(failure.isEmpty());QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!controller(laptop)->pttActive());QTRY_VERIFY(button(laptop,"playButton")->isEnabled());laptop.close();shack.close();
    }
    void staleLaptopPttTestCannotReleaseNewShackActivation_data(){
        QTest::addColumn<bool>("closeDialog");QTest::addColumn<bool>("delaySnapshots");
        QTest::newRow("timer-current-snapshot")<<false<<false;
        QTest::newRow("close-current-snapshot")<<true<<false;
        QTest::newRow("timer-delayed-snapshot")<<false<<true;
        QTest::newRow("close-delayed-snapshot")<<true<<true;
    }
    void staleLaptopPttTestCannotReleaseNewShackActivation(){
        QFETCH(bool,closeDialog);QFETCH(bool,delaySnapshots);
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());
        auto [host,peer]=remoteTestPair();auto* server=shack.findChild<remote::Server*>();auto* client=laptop.findChild<remote::Client*>();server->acceptConnection(host);client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        RadioControls controls(controller(laptop),[]{return true;},[&]{controller(laptop)->release();},&laptop);controls.setAttribute(Qt::WA_DeleteOnClose,false);controls.show();auto* test=controls.findChild<QPushButton*>("radioPttTest");QTRY_VERIFY(test->isEnabled());test->click();QTRY_VERIFY(state->keyed.load());QTRY_VERIFY(!controller(laptop)->busy());
        // Hold server-to-laptop bytes after the first key acknowledgement. The
        // laptop still believes it owns the old activation when its release is
        // delivered to the host; a client-side snapshot check cannot fix this.
        std::unique_ptr<QSignalBlocker> delayed;if(delaySnapshots)delayed=std::make_unique<QSignalBlocker>(peer);
        QPointer<RemoteTestDevice> peerGuard=peer;const auto resume=qScopeGuard([&]{delayed.reset();if(peerGuard)QMetaObject::invokeMethod(peerGuard,"readyRead",Qt::DirectConnection);});
        button(shack,"stopButton")->click();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!controller(shack)->pttActive());QTRY_VERIFY(!controller(shack)->busy());
        bool newKeyReady=false;controller(shack)->key([&](QString error){QVERIFY2(error.isEmpty(),qPrintable(error));newKeyReady=true;});QTRY_VERIFY(newKeyReady);QVERIFY(state->keyed.load());
        QSignalSpy requests(server,&remote::Server::request);const auto ownedReleaseArrived=[&]{for(const auto& request:requests)if(request.at(1).toString()=="radio.release")return true;return false;};
        if(closeDialog)controls.close();
        QTRY_VERIFY_WITH_TIMEOUT(ownedReleaseArrived(),3000);QTRY_VERIFY(!controller(shack)->busy());
        QVERIFY2(state->keyed.load(),"A stale laptop PTT-test release must not stop the shack's newer activation");
        // Explicit emergency Stop must still release the current transmission,
        // even while the laptop has not received its newer snapshots.
        button(laptop,"stopButton")->click();QTRY_VERIFY(!state->keyed.load());
        delayed.reset();QMetaObject::invokeMethod(peer,"readyRead",Qt::DirectConnection);QTRY_VERIFY(!controller(laptop)->busy());controls.close();laptop.close();shack.close();
    }
    void remotePttTestCloseBeforeAcknowledgementReleasesItsActivation(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());auto [host,peer]=remoteTestPair();shack.findChild<remote::Server*>()->acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        RadioControls controls(controller(laptop),[]{return true;},[&]{controller(laptop)->release();},&laptop);controls.setAttribute(Qt::WA_DeleteOnClose,false);controls.show();auto* test=controls.findChild<QPushButton*>("radioPttTest");QTRY_VERIFY(test->isEnabled());
        {QSignalBlocker delayed(peer);test->click();QTRY_VERIFY(state->keyed.load());QVERIFY(controller(laptop)->busy());controls.close();}
        QMetaObject::invokeMethod(peer,"readyRead",Qt::DirectConnection);QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!controller(laptop)->busy());laptop.close();shack.close();
    }
    void remotePttTestRequiresHostOwnershipSupport(){
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());remote::Server server;server.start(0,key_,QHostAddress::LocalHost);
        server.setSnapshot({{"radioConnected",true},{"radioPttActive",false},{"radio",QJsonObject{{"ptt",0}}}});auto [host,peer]=remoteTestPair();server.acceptConnection(host);auto* client=laptop.findChild<remote::Client*>();client->connectDevice(peer,key_);QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        RadioControls controls(controller(laptop),[]{return true;},[&]{controller(laptop)->release();},&laptop);controls.setAttribute(Qt::WA_DeleteOnClose,false);controls.show();auto* test=controls.findChild<QPushButton*>("radioPttTest");QVERIFY(!test->isEnabled());QVERIFY(test->toolTip().contains("1.13.1"));QVERIFY(controls.findChild<QPushButton*>("radioPttOff")->isEnabled());controls.close();laptop.close();
    }
    void cancelledRemotePttWatchdogCannotReleaseLaterTransmission(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        QElapsedTimer elapsed;elapsed.start();bool done=false;QString error;
        client->command("radio.key",{},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(state->keyed.load());
        done=false;client->command("radio.release",{},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(!state->keyed.load());
        // Start another activation in the same authenticated session, shortly
        // before the first activation's ten-second watchdog expires.
        QTest::qWait(8500);done=false;client->command("radio.key",{},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(state->keyed.load());
        QTest::qWait(int(std::max<qint64>(0,11000-elapsed.elapsed())));QVERIFY(client->connected());QVERIFY2(state->keyed.load(),"An old remote watchdog must not release a later transmission");QVERIFY(controller(shack)->pttActive());
        done=false;client->command("radio.release",{},[&](QString e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(!state->keyed.load());laptop.close();shack.close();
    }
    void lostConnectionCancelsPreparationEvenAfterReconnect(){
        configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());QTRY_VERIFY(controller(laptop)->connected());
        send(laptop);client->disconnectFromHost();bridge(shack,laptop);QTRY_VERIFY(client->connected());QTRY_VERIFY_WITH_TIMEOUT(button(laptop,"playButton")->isEnabled(),5000);QTest::qWait(500);QVERIFY(!state->keyed.load());QVERIFY(playbackLog().isEmpty());QVERIFY(!client->transmitting());laptop.close();shack.close();
    }
    void shackDisplaysLaptopWaveformProgressivelyAndResets(){
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","10");configureLocal("shack",false);MainWindow shack;shack.show();shack.setRemoteConfig(hostConfig());
        // Actual RX settings must not distort the clean local transmit monitor.
        auto* rxMode=shack.findChild<QComboBox*>("receiveMode");QVERIFY(rxMode);rxMode->setCurrentIndex(rxMode->findData("pd120"));
        configureLocal("laptop",false);MainWindow laptop;laptop.show();laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());
        auto* image=shack.findChild<QLabel*>("receiveImage");auto* info=label(shack,"receiveInfo");auto* history=shack.findChild<QListWidget*>("receiveHistory");QVERIFY(image&&info&&history);const auto historyCount=history->count();
        send(laptop);QTRY_VERIFY_WITH_TIMEOUT(shack.findChild<TransmitMonitor*>()->active(),5000);QCOMPARE(shack.findChild<QTabWidget*>("imageTabs")->currentWidget()->objectName(),QString("receivePage"));
        QTRY_VERIFY_WITH_TIMEOUT(image->property("remoteTxMonitorRows").toInt()>0,4000);QVERIFY(image->property("remoteTxMonitorRows").toInt()<120);QVERIFY(!image->property("remoteTxMonitorComplete").toBool());QVERIFY(info->text().contains("Remote TX monitor"));
        QTRY_VERIFY_WITH_TIMEOUT(image->property("remoteTxMonitorComplete").toBool(),12000);QCOMPARE(image->property("remoteTxMonitorRows").toInt(),120);QTRY_VERIFY(!laptop.findChild<remote::Client*>()->transmitting());QCOMPARE(history->count(),historyCount);QCOMPARE(rxMode->currentData().toString(),QString("pd120"));
        // Stop a second image mid-frame and keep its partial preview frozen.
        send(laptop);QTRY_VERIFY_WITH_TIMEOUT(shack.findChild<TransmitMonitor*>()->active(),5000);QTRY_VERIFY_WITH_TIMEOUT(image->property("remoteTxMonitorRows").toInt()>0,4000);button(laptop,"stopButton")->click();QTRY_VERIFY(!shack.findChild<TransmitMonitor*>()->active());const auto rows=image->property("remoteTxMonitorRows").toInt();QVERIFY(rows>0&&rows<120);QVERIFY(!image->property("remoteTxMonitorComplete").toBool());QTest::qWait(400);QCOMPARE(image->property("remoteTxMonitorRows").toInt(),rows);QCOMPARE(history->count(),historyCount);laptop.close();shack.close();
    }
    void shackCanTransmitAndLaptopAutomaticallyReceives(){
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","10");configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.show();shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());
        configureLocal("laptop");MainWindow laptop;laptop.show();laptop.setRemoteConfig(clientConfig());auto* fixed=laptop.findChild<QComboBox*>("receiveMode");fixed->setCurrentIndex(fixed->findData("pd120"));bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());QTRY_VERIFY(button(shack,"playButton")->isEnabled());
        auto* image=laptop.findChild<QLabel*>("receiveImage");auto* info=label(laptop,"receiveInfo");button(laptop,"transmitFullScreenButton")->click();QVERIFY(laptop.findChild<QWidget*>("transmitFullScreenWindow")->isVisible());send(shack);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);QTRY_VERIFY(playbackLog().contains("shack-output"));QTRY_VERIFY_WITH_TIMEOUT(laptop.findChild<TransmitMonitor*>()->active(),4000);QCOMPARE(laptop.findChild<QTabWidget*>("imageTabs")->currentWidget()->objectName(),QString("receivePage"));QVERIFY(!laptop.findChild<QWidget*>("transmitFullScreenWindow")->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(image->property("remoteTxMonitorRows").toInt()>0,4000);QVERIFY(image->property("remoteTxMonitorRows").toInt()<120);QVERIFY(info->text().contains("Shack TX monitor"));QVERIFY(!button(laptop,"playButton")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(image->property("remoteTxMonitorComplete").toBool(),12000);QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(button(laptop,"playButton")->isEnabled());QCOMPARE(fixed->currentData().toString(),QString("pd120"));QVERIFY(!playbackLog().contains("laptop"));
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","1");send(laptop);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);QTRY_VERIFY(shack.findChild<TransmitMonitor*>()->active());QTRY_VERIFY_WITH_TIMEOUT(shack.findChild<QLabel*>("receiveImage")->property("remoteTxMonitorComplete").toBool(),5000);QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(button(shack,"playButton")->isEnabled());laptop.close();shack.close();
    }
    void simultaneousRequestsUseOneTransmitterAndBothStopsWork(){
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","30");configureLocal("shack");auto state=std::make_shared<RemoteRadioState>();MainWindow shack(nullptr,std::make_unique<RemoteRadio>(state));shack.setRemoteConfig(hostConfig());controller(shack)->connectRadio({});QTRY_VERIFY(controller(shack)->connected());configureLocal("laptop");MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);auto* client=laptop.findChild<remote::Client*>();QTRY_VERIFY(client->connected());
        send(shack);send(laptop);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);QTRY_VERIFY(laptop.findChild<TransmitMonitor*>()->active());QVERIFY(!client->transmitting());QTRY_COMPARE(playbackLog().count("shack-output"),1);button(laptop,"stopButton")->click();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!laptop.findChild<TransmitMonitor*>()->active());QTRY_VERIFY(button(shack,"playButton")->isEnabled());
        send(shack);QTRY_VERIFY_WITH_TIMEOUT(state->keyed.load(),5000);QTRY_VERIFY(laptop.findChild<TransmitMonitor*>()->active());button(shack,"stopButton")->click();QTRY_VERIFY(!state->keyed.load());QTRY_VERIFY(!laptop.findChild<TransmitMonitor*>()->active());QTRY_VERIFY(button(laptop,"playButton")->isEnabled());laptop.close();shack.close();
    }
    void browsingReceiveHistoryDoesNotInterruptSharedAudio(){
        qputenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS","3");configureLocal("shack",false);MainWindow shack;shack.setRemoteConfig(hostConfig());configureLocal("laptop",false);MainWindow laptop;laptop.setRemoteConfig(clientConfig());bridge(shack,laptop);QTRY_VERIFY(laptop.findChild<remote::Client*>()->connected());send(shack);QTRY_VERIFY_WITH_TIMEOUT(laptop.findChild<TransmitMonitor*>()->active(),5000);
        QImage old(160,120,QImage::Format_RGB32);old.fill(Qt::red);const auto path=directory_.filePath("old-picture.png");QVERIFY(old.save(path));auto* history=laptop.findChild<QListWidget*>("receiveHistory");auto* item=new QListWidgetItem("Old received picture",history);item->setData(Qt::UserRole,path);QVERIFY(QMetaObject::invokeMethod(history,"itemDoubleClicked",Q_ARG(QListWidgetItem*,item)));QVERIFY(laptop.findChild<TransmitMonitor*>()->active());QTRY_VERIFY_WITH_TIMEOUT(!laptop.findChild<TransmitMonitor*>()->active(),7000);QTRY_VERIFY(button(laptop,"playButton")->isEnabled());QVERIFY(label(laptop,"receiveInfo")->text().contains("Old received picture"));laptop.close();shack.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);qunsetenv("OPENSSTV_REMOTE_TEST_DIR");qunsetenv("OPENSSTV_REMOTE_TEST_PLAY_SECONDS");QSettings().clear();}
};
int main(int argc,char** argv){
    QTemporaryDir environment;
    qputenv("XDG_CONFIG_HOME",environment.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",environment.filePath("data").toUtf8());QDir().mkpath(environment.filePath("config"));
    QFile userDirs(environment.filePath("config/user-dirs.dirs"));if(!userDirs.open(QIODevice::WriteOnly))return 1;
    userDirs.write("XDG_PICTURES_DIR=\""+environment.filePath("pictures").toUtf8()+"\"\n");userDirs.close();
    QApplication app(argc,argv);RemoteUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "remote_ui_tests.moc"
