#include "mainwindow.h"
#include "radio_frontend.h"
#include "radio_ui.h"
#include "captionpreview.h"
#include "skins.h"
#include <QtTest>
#include <QtWidgets>
#include <QMutex>
#include <atomic>
#include <thread>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <QScopeGuard>

struct ConsoleState {
    QMutex mutex;
    QVariantMap values={{"frequencyHz",145550000.},{"mode","FM"},{"passband",12000},{"vfo","VFOA"},{"powerPercent",35.},{"ritHz",0},{"strengthDb",-12},{"ptt",0}};
    QList<radio::Command> commands;
    QStringList events;
    std::atomic_int frequencyDelay=0;
    std::atomic_bool failFrequency=false,failRead=false,failVfo=false,partialVfoFailure=false;
    QVariantMap read(){QMutexLocker lock(&mutex);return values;}
    void set(const QString& key,const QVariant& value){QMutexLocker lock(&mutex);values[key]=value;}
    void event(const QString& event){QMutexLocker lock(&mutex);events<<event;}
    QStringList history(){QMutexLocker lock(&mutex);return events;}
    int count(radio::Control control){QMutexLocker lock(&mutex);return std::count_if(commands.begin(),commands.end(),[&](const auto& c){return c.control==control;});}
};
class ConsoleRadio:public radio::Backend {
    std::shared_ptr<ConsoleState> state_;
public:
    explicit ConsoleRadio(std::shared_ptr<ConsoleState> state):state_(std::move(state)){}
    void open(const radio::Config&) override{state_->event("open");}
    QVariantMap read(bool pttOnly=false) override{if(state_->failRead)throw std::runtime_error("Simulated connection lost");const auto values=state_->read();return pttOnly?QVariantMap{{"ptt",values["ptt"]}}:values;}
    void setPtt(bool on) override{state_->set("ptt",on?1:0);state_->event(on?"PTT ON":"PTT OFF");}
    void apply(const radio::Command& command) override{
        if(command.control==radio::Control::Vfo&&state_->failVfo){if(state_->partialVfoFailure)state_->set("vfo",command.text);throw std::runtime_error("Memory mode is not supported by this radio");}
        if(command.control==radio::Control::Frequency){state_->event("frequency-start");std::this_thread::sleep_for(std::chrono::milliseconds(state_->frequencyDelay.load()));if(state_->failFrequency)throw std::runtime_error("Simulated unsupported frequency");}
        QMutexLocker lock(&state_->mutex);state_->commands<<command;
        switch(command.control){
        case radio::Control::Frequency:state_->values["frequencyHz"]=command.value;break;
        case radio::Control::Mode:state_->values["mode"]=command.text;state_->values["passband"]=command.value?command.value:command.text=="FM"?12000.:2700.;break;
        case radio::Control::Vfo:state_->values["vfo"]=command.text;break;
        case radio::Control::Power:state_->values["powerPercent"]=command.value;break;
        case radio::Control::Rit:state_->values["ritHz"]=command.value;break;
        default:throw std::runtime_error("Unsupported simulated control");
        }
    }
    QString close() override{state_->set("ptt",0);state_->event("close");return {};}
};
class RadioFrontendTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    static QPushButton* button(MainWindow& w,const char* name){return w.findChild<QPushButton*>(name);}
    static QLabel* label(MainWindow& w,const char* name){return w.findChild<QLabel*>(name);}
    static radio::Controller* controller(MainWindow& w){return w.findChild<radio::Controller*>("radioController");}
    static void showConsole(MainWindow& w){w.findChild<QAction*>("radioTabAction")->trigger();QCOMPARE(w.findChild<QTabWidget*>("imageTabs")->currentIndex(),3);}
    static void connectConsole(MainWindow& w){showConsole(w);QVERIFY(button(w,"radioFrontendConnect")->isEnabled());button(w,"radioFrontendConnect")->click();QTRY_VERIFY(controller(w)->connected());QTRY_VERIFY(!controller(w)->busy());}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVRadioFrontendTests");QCoreApplication::setApplicationName("OpenSSTVRadioFrontendTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_CONSOLE_AUDIO",directory_.filePath("playing").toUtf8());QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write("#!/usr/bin/python3\nimport os,time\nopen(os.environ['OPENSSTV_CONSOLE_AUDIO'],'w').write('playing')\ntime.sleep(10)\n");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings settings;settings.clear();radio::Config config;config.enabled=true;config.save(settings);QFile::remove(directory_.filePath("playing"));}
    void modeSelectionWaitsForBusyRadioAndStopCancelsPendingSelection(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);state->frequencyDelay=300;
        auto* modes=w.findChild<QComboBox*>("radioFrontendMode");const auto select=[&](const QString& mode){modes->setCurrentText(mode);QMetaObject::invokeMethod(modes,"activated",Qt::DirectConnection,Q_ARG(int,modes->currentIndex()));};
        controller(w)->apply({radio::Control::Frequency,145560000.,{}});QVERIFY(controller(w)->busy());select("USB");select("LSB");QTRY_COMPARE(state->read()["mode"].toString(),QString("LSB"));QTRY_VERIFY(!controller(w)->busy());QCOMPARE(state->count(radio::Control::Mode),1);
        controller(w)->apply({radio::Control::Frequency,145570000.,{}});QVERIFY(controller(w)->busy());select("USB");button(w,"radioFrontendStop")->click();QTRY_VERIFY(!controller(w)->busy());QTest::qWait(120);QCOMPARE(state->read()["mode"].toString(),QString("LSB"));QCOMPARE(state->count(radio::Control::Mode),1);QVERIFY(!state->history().contains("PTT ON"));w.close();
    }
    void consoleNavigationNeverConnectsOrKeys(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();auto* tabs=w.findChild<QTabWidget*>("imageTabs");QCOMPARE(tabs->count(),4);QCOMPARE(tabs->tabText(3),QString("Radio"));const auto txMode=w.findChild<QComboBox*>("transmitMode")->currentData();const auto imageSize=w.findChild<CaptionPreview*>("transmitImage")->imageSize();
        showConsole(w);QVERIFY(w.findChild<QWidget*>("operatingControls")->isHidden());QVERIFY(w.findChild<QTabWidget*>("galleryTabs")->isHidden());QCOMPARE(label(w,"radioFrontendFrequency")->text(),QString("—.——————"));QVERIFY(!w.findChild<QWidget*>("radioFrontendMeter")->property("hasReading").toBool());QVERIFY(!button(w,"radioFrontendTune")->isEnabled());
        w.findChild<QDial*>("radioFrontendDial")->setValue(51);QTest::qWait(120);QVERIFY(state->history().isEmpty());
        const auto artifact=qEnvironmentVariable("OPENSSTV_RADIO_FRONTEND_PREVIEW");if(!artifact.isEmpty()){auto path=artifact;path.replace(".png","-offline.png");QVERIFY(w.grab().save(path));}
        tabs->setCurrentIndex(1);QVERIFY(w.findChild<QWidget*>("operatingControls")->isVisible());QVERIFY(w.findChild<QTabWidget*>("galleryTabs")->isVisible());QCOMPARE(w.findChild<QComboBox*>("transmitMode")->currentData(),txMode);QCOMPARE(w.findChild<CaptionPreview*>("transmitImage")->imageSize(),imageSize);
        button(w,"radioControlsButton")->click();QCOMPARE(tabs->currentIndex(),3);QVERIFY(state->history().isEmpty());tabs->setCurrentIndex(2);QCOMPARE(w.findChild<QTabWidget*>("modeTabs")->currentIndex(),0);tabs->setCurrentIndex(0);QCOMPARE(w.findChild<QTabWidget*>("modeTabs")->currentIndex(),0);w.close();
    }
    void settingsOpensRadioPage(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();showConsole(w);bool seen=false;
        QTimer::singleShot(30,&w,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);const auto close=qScopeGuard([dialog]{dialog->reject();});QCOMPARE(dialog->findChild<QTabWidget*>("settingsTabs")->currentIndex(),1);seen=true;});button(w,"radioFrontendSettings")->click();QVERIFY(seen);QVERIFY(state->history().isEmpty());w.close();
    }
    void readbackAndIndependentControls(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);const auto txMode=w.findChild<QComboBox*>("transmitMode")->currentData();
        QCOMPARE(label(w,"radioFrontendFrequency")->text(),QString("145.550000"));QCOMPARE(label(w,"radioFrontendBadge")->text(),QString("RX"));QCOMPARE(w.findChild<QWidget*>("radioFrontendMeter")->property("readingDb").toInt(),-12);QVERIFY(label(w,"radioFrontendSignal")->text().startsWith("S7"));
        auto* frequency=w.findChild<QDoubleSpinBox*>("radioFrontendFrequencyEntry");frequency->setValue(14.23);controller(w)->refresh();QTRY_VERIFY(!controller(w)->busy());QCOMPARE(frequency->value(),14.23);QCOMPARE(label(w,"radioFrontendFrequency")->text(),QString("145.550000"));QCOMPARE(state->count(radio::Control::Frequency),0);
        QTRY_VERIFY(button(w,"radioFrontendTune")->isEnabled());button(w,"radioFrontendTune")->click();QTRY_COMPARE(state->read()["frequencyHz"].toDouble(),14230000.);QTRY_COMPARE(label(w,"radioFrontendFrequency")->text(),QString("14.230000"));
        QTRY_VERIFY(button(w,"radioFrontendSetMode")->isEnabled());w.findChild<QComboBox*>("radioFrontendMode")->setCurrentText("USB");QCOMPARE(w.findChild<QSpinBox*>("radioFrontendBandwidth")->value(),0);button(w,"radioFrontendSetMode")->click();QTRY_COMPARE(state->read()["mode"].toString(),QString("USB"));QTRY_VERIFY(!controller(w)->busy());
        w.findChild<QComboBox*>("radioFrontendVfo")->setCurrentText("VFOB");button(w,"radioFrontendSetVfo")->click();QTRY_COMPARE(state->read()["vfo"].toString(),QString("VFOB"));QTRY_VERIFY(!controller(w)->busy());
        w.findChild<QDoubleSpinBox*>("radioFrontendPower")->setValue(25.5);QCOMPARE(state->read()["powerPercent"].toDouble(),35.);button(w,"radioFrontendSetPower")->click();QTRY_COMPARE(state->read()["powerPercent"].toDouble(),25.5);QTRY_VERIFY(!controller(w)->busy());
        w.findChild<QSpinBox*>("radioFrontendRit")->setValue(120);button(w,"radioFrontendSetRit")->click();QTRY_COMPARE(state->read()["ritHz"].toInt(),120);QTRY_VERIFY(!controller(w)->busy());button(w,"radioFrontendZeroRit")->click();QTRY_COMPARE(state->read()["ritHz"].toInt(),0);QCOMPARE(w.findChild<QComboBox*>("transmitMode")->currentData(),txMode);QVERIFY(!state->history().contains("PTT ON"));
        QTRY_VERIFY(!controller(w)->busy());QTRY_VERIFY(button(w,"radioFrontendSetRit")->isEnabled());const auto artifact=qEnvironmentVariable("OPENSSTV_RADIO_FRONTEND_PREVIEW");if(!artifact.isEmpty())QVERIFY(w.grab().save(artifact));w.close();
    }
    void frontPanelKeepsReadbackAndPttVisibleWhileScrolling_data(){
        QTest::addColumn<QString>("skin");QTest::newRow("classic")<<QString("classic");QTest::newRow("arctic")<<QString("arctic");QTest::newRow("neon")<<QString("neon-circuit");
    }
    void frontPanelKeepsReadbackAndPttVisibleWhileScrolling(){
        QFETCH(QString,skin);QSettings().setValue("ui/skin",skin);auto state=std::make_shared<ConsoleState>();state->set("frequencyHz",14230000.);state->set("mode","USB");state->set("passband",2700);MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.resize(1000,720);w.show();connectConsole(w);QTest::qWait(30);
        auto* scroll=w.findChild<QScrollArea*>("radioConsoleScroll");auto* frequency=label(w,"radioFrontendFrequency");auto* voice=button(w,"voicePttButton");auto* stop=button(w,"radioFrontendStop");QVERIFY(scroll);QVERIFY(voice->isVisible());
        QCOMPARE(label(w,"radioFrontendFilterReadout")->text(),QString("FILTER  2.7 kHz"));QCOMPARE(label(w,"radioFrontendPowerReadout")->text(),QString("RF POWER  35 %"));QCOMPARE(label(w,"radioFrontendRitReadout")->text(),QString("RIT  0 Hz"));
        QCOMPARE(voice->parentWidget()->parentWidget(),w.findChild<QWidget*>("radioFrontend"));
        const auto frequencyPosition=frequency->mapTo(&w,QPoint{}),voicePosition=voice->mapTo(&w,QPoint{}),stopPosition=stop->mapTo(&w,QPoint{});
        for(auto* control:QList<QWidget*>{frequency,voice,stop})QVERIFY(w.rect().contains(QRect(control->mapTo(&w,QPoint{}),control->size())));
        for(const auto* name:{"radioFrontendDial","radioFrontendStep","radioFrontendZeroRit"}){auto* control=w.findChild<QWidget*>(name);QVERIFY(control);QVERIFY2(scroll->viewport()->rect().contains(QRect(control->mapTo(scroll->viewport(),QPoint{}),control->size())),name);}
        QVERIFY(scroll->verticalScrollBar()->maximum()>0);scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QApplication::processEvents();
        QCOMPARE(frequency->mapTo(&w,QPoint{}),frequencyPosition);QCOMPARE(voice->mapTo(&w,QPoint{}),voicePosition);QCOMPARE(stop->mapTo(&w,QPoint{}),stopPosition);
        button(w,"radioFrontendWaterfall")->click();QApplication::processEvents();QVERIFY(!w.findChild<QWidget*>("audioMonitor")->visibleRegion().isEmpty());
        button(w,"radioFrontendAudio")->click();QApplication::processEvents();QVERIFY(!w.findChild<QWidget*>("radioAudioLevels")->visibleRegion().isEmpty());
        button(w,"audioLevelBack")->click();QCOMPARE(scroll->verticalScrollBar()->value(),0);
        state->set("powerPercent",40.);state->set("ritHz",120);controller(w)->refresh();QTRY_COMPARE(label(w,"radioFrontendPowerReadout")->text(),QString("RF POWER  40 %"));QCOMPARE(label(w,"radioFrontendRitReadout")->text(),QString("RIT  120 Hz"));
        controller(w)->disconnectRadio([](QString error){QVERIFY(error.isEmpty());});QTRY_VERIFY(!controller(w)->connected());QCOMPARE(label(w,"radioFrontendFilterReadout")->text(),QString("FILTER  —"));QCOMPARE(label(w,"radioFrontendPowerReadout")->text(),QString("RF POWER  —"));QCOMPARE(label(w,"radioFrontendRitReadout")->text(),QString("RIT  —"));QVERIFY(!state->history().contains("PTT ON"));
        const auto artifact=qEnvironmentVariable("OPENSSTV_RADIO_FRONTEND_PREVIEW");if(!artifact.isEmpty()){auto path=artifact;path.replace(".png","-"+skin+"-panel.png");QVERIFY(w.grab().save(path));}w.close();
    }
    void rapidTuningCoalescesAndKeepsInflightSteps(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);auto* step=w.findChild<QComboBox*>("radioFrontendStep");step->setCurrentIndex(step->findData(5000));auto* dial=w.findChild<QDial*>("radioFrontendDial");dial->setValue(51);dial->setValue(52);dial->setValue(53);QTRY_COMPARE(state->read()["frequencyHz"].toDouble(),145565000.);QTRY_VERIFY(!controller(w)->busy());QCOMPARE(state->count(radio::Control::Frequency),1);
        state->frequencyDelay=250;button(w,"radioFrontendUp")->click();QTRY_VERIFY(controller(w)->busy());QTRY_COMPARE(state->history().count("frequency-start"),2);button(w,"radioFrontendUp")->click();button(w,"radioFrontendUp")->click();QTRY_COMPARE_WITH_TIMEOUT(state->read()["frequencyHz"].toDouble(),145580000.,3000);QTRY_VERIFY(!controller(w)->busy());QCOMPARE(state->count(radio::Control::Frequency),3);QVERIFY(!state->history().contains("PTT ON"));QCOMPARE(QSettings().value("radioFrontend/stepHz").toInt(),5000);w.close();
    }
    void memoryVfoSwitchRestoresLastVfoAndCancelsPendingTune(){
        auto state=std::make_shared<ConsoleState>();state->set("vfo","VFOB");MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();showConsole(w);
        auto* mv=button(w,"radioFrontendMemoryVfo");QVERIFY(mv);QCOMPARE(mv->text(),QString("M/V"));QVERIFY(!mv->isEnabled());QVERIFY(!mv->isChecked());connectConsole(w);QVERIFY(mv->isEnabled());
        button(w,"radioFrontendUp")->click();mv->click();QTRY_COMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("MEM"));QVERIFY(mv->isChecked());QCOMPARE(w.findChild<QComboBox*>("radioFrontendVfo")->currentText(),QString("MEM"));QTRY_VERIFY(!controller(w)->busy());
        QTest::qWait(130);QCOMPARE(state->count(radio::Control::Frequency),0);QCOMPARE(state->read()["frequencyHz"].toDouble(),145550000.);
        const auto artifact=qEnvironmentVariable("OPENSSTV_RADIO_FRONTEND_PREVIEW");if(!artifact.isEmpty()){auto path=artifact;path.replace(".png","-memory.png");QVERIFY(w.grab().save(path));}
        mv->click();QTRY_COMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("VFOB"));QVERIFY(!mv->isChecked());QCOMPARE(state->count(radio::Control::Vfo),2);
        QTRY_VERIFY(!controller(w)->busy());state->set("vfo","MEM");controller(w)->refresh();QTRY_VERIFY(mv->isChecked());QTRY_VERIFY(mv->isEnabled());mv->click();QTRY_COMPARE(state->read()["vfo"].toString(),QString("VFOB"));QVERIFY(!state->history().contains("PTT ON"));w.close();
    }
    void memoryVfoInitialMemoryAndReconnect(){
        auto state=std::make_shared<ConsoleState>();state->set("vfo","MEM");MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);auto* mv=button(w,"radioFrontendMemoryVfo");QVERIFY(mv->isChecked());mv->click();QTRY_COMPARE(state->read()["vfo"].toString(),QString("VFO"));QTRY_VERIFY(mv->isEnabled());QVERIFY(!mv->isChecked());
        state->set("vfo","SubB");controller(w)->refresh();QTRY_COMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("SubB"));QTRY_VERIFY(mv->isEnabled());mv->click();QTRY_VERIFY(mv->isChecked());QTRY_VERIFY(mv->isEnabled());
        button(w,"radioFrontendDisconnect")->click();QTRY_VERIFY(!controller(w)->connected());QVERIFY(!mv->isEnabled());QVERIFY(!mv->isChecked());QTRY_VERIFY(!controller(w)->busy());connectConsole(w);QVERIFY(mv->isChecked());mv->click();QTRY_COMPARE(state->read()["vfo"].toString(),QString("VFO"));w.close();
    }
    void memoryVfoRejectedAndUnknownReadback(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);auto* mv=button(w,"radioFrontendMemoryVfo");state->failVfo=true;mv->click();QTRY_VERIFY(label(w,"radioFrontendStatus")->text().contains("not supported"));QVERIFY(!mv->isChecked());QCOMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("VFOA"));QCOMPARE(state->count(radio::Control::Vfo),0);QVERIFY(controller(w)->connected());
        state->failVfo=false;state->set("vfo","MEM");controller(w)->refresh();QTRY_VERIFY(mv->isChecked());QTRY_VERIFY(mv->isEnabled());state->failVfo=true;mv->click();QTRY_VERIFY(label(w,"radioFrontendStatus")->text().contains("not supported"));QVERIFY(mv->isChecked());QCOMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("MEM"));
        QTRY_VERIFY(!controller(w)->busy());state->set("vfo",QVariant{});controller(w)->refresh();QTRY_COMPARE(label(w,"radioFrontendVfoReadout")->text(),QString{});QVERIFY(!mv->isEnabled());QVERIFY(!mv->isChecked());QTRY_VERIFY(!controller(w)->busy());state->set("vfo","unknown");controller(w)->refresh();QTRY_COMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("unknown"));QVERIFY(!mv->isEnabled());QVERIFY(!state->history().contains("PTT ON"));w.close();
    }
    void memoryVfoLocksDuringAudioAndBusyRequests(){
        auto state=std::make_shared<ConsoleState>();radio::Controller controller(nullptr,std::make_unique<ConsoleRadio>(state));bool idle=false;RadioFrontend frontend(&controller,{},[&]{return idle;});auto* mv=frontend.findChild<QPushButton*>("radioFrontendMemoryVfo");
        controller.connectRadio({});QTRY_VERIFY(controller.connected());QTRY_VERIFY(!controller.busy());QVERIFY(!mv->isEnabled());mv->click();QCOMPARE(state->count(radio::Control::Vfo),0);
        idle=true;frontend.refresh();QVERIFY(mv->isEnabled());state->frequencyDelay=200;controller.apply({radio::Control::Frequency,146520000.,{}});QVERIFY(controller.busy());QVERIFY(!mv->isEnabled());mv->click();QCOMPARE(state->count(radio::Control::Vfo),0);QTRY_VERIFY(!controller.busy());QVERIFY(mv->isEnabled());
        controller.key([](const QString& error){QVERIFY(error.isEmpty());});QTRY_VERIFY(!controller.busy());QVERIFY(!mv->isEnabled());mv->click();QCOMPARE(state->count(radio::Control::Vfo),0);controller.release();QTRY_VERIFY(!controller.busy());QVERIFY(mv->isEnabled());
    }
    void memoryVfoRefreshesReadbackAfterPartialFailure(){
        auto state=std::make_shared<ConsoleState>();state->set("vfo","VFOB");MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);auto* mv=button(w,"radioFrontendMemoryVfo");
        state->failVfo=true;state->partialVfoFailure=true;mv->click();QTRY_VERIFY(mv->isChecked());QTRY_VERIFY(!controller(w)->busy());QVERIFY(label(w,"radioFrontendStatus")->text().contains("not supported"));QCOMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("MEM"));
        state->failVfo=false;mv->click();QTRY_COMPARE(label(w,"radioFrontendVfoReadout")->text(),QString("VFOB"));QVERIFY(!mv->isChecked());QVERIFY(!state->history().contains("PTT ON"));w.close();
    }
    void pttCancelsPendingTuneAndStopReleases(){
        auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);button(w,"radioFrontendUp")->click();controller(w)->key([](const QString& error){QVERIFY(error.isEmpty());});QTRY_COMPARE(state->read()["ptt"].toInt(),1);QTest::qWait(130);QCOMPARE(state->count(radio::Control::Frequency),0);QVERIFY(!button(w,"radioFrontendTune")->isEnabled());QVERIFY(!button(w,"radioFrontendMemoryVfo")->isEnabled());button(w,"radioFrontendMemoryVfo")->click();QCOMPARE(state->count(radio::Control::Vfo),0);QVERIFY(!w.findChild<QDial*>("radioFrontendDial")->isEnabled());QVERIFY(!w.findChild<QWidget*>("radioFrontendMeter")->property("hasReading").toBool());QVERIFY(button(w,"radioFrontendStop")->isEnabled());button(w,"radioFrontendStop")->click();QTRY_VERIFY(!controller(w)->pttActive());QTRY_COMPARE(state->read()["ptt"].toInt(),0);QTRY_VERIFY(button(w,"radioFrontendTune")->isEnabled());w.close();
    }
    void missingReadingsErrorsAndReconnect(){
        auto state=std::make_shared<ConsoleState>();{QMutexLocker lock(&state->mutex);state->values.remove("vfo");state->values.remove("powerPercent");state->values.remove("ritHz");state->values.remove("strengthDb");}
        MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();connectConsole(w);QVERIFY(!button(w,"radioFrontendMemoryVfo")->isEnabled());QVERIFY(!button(w,"radioFrontendSetVfo")->isEnabled());QVERIFY(!button(w,"radioFrontendSetPower")->isEnabled());QVERIFY(!button(w,"radioFrontendSetRit")->isEnabled());QVERIFY(!w.findChild<QWidget*>("radioFrontendMeter")->property("hasReading").toBool());
        state->set("strengthDb",18);controller(w)->refresh();QTRY_COMPARE(label(w,"radioFrontendSignal")->text(),QString("S9 + 18 dB"));QTRY_VERIFY(!controller(w)->busy());state->set("strengthDb",QVariant{});controller(w)->refresh();QTRY_VERIFY(!controller(w)->busy());QVERIFY(!w.findChild<QWidget*>("radioFrontendMeter")->property("hasReading").toBool());
        state->failFrequency=true;w.findChild<QDoubleSpinBox*>("radioFrontendFrequencyEntry")->setValue(999);button(w,"radioFrontendTune")->click();QTRY_VERIFY(label(w,"radioFrontendStatus")->text().contains("unsupported frequency"));QCOMPARE(label(w,"radioFrontendFrequency")->text(),QString("145.550000"));
        QTRY_VERIFY(!controller(w)->busy());state->failRead=true;controller(w)->refresh();QTRY_VERIFY(!controller(w)->connected());QCOMPARE(label(w,"radioFrontendFrequency")->text(),QString("—.——————"));QVERIFY(!button(w,"radioFrontendTune")->isEnabled());state->failRead=false;state->failFrequency=false;state->set("frequencyHz",146520000.);button(w,"radioFrontendConnect")->click();QTRY_VERIFY(controller(w)->connected());QTRY_COMPARE(w.findChild<QDoubleSpinBox*>("radioFrontendFrequencyEntry")->value(),146.52);QVERIFY(!state->history().contains("PTT ON"));w.close();
    }
    void consoleStopWorksWithoutHamlibConnection(){
        QSettings settings;settings.setValue("radio/enabled",false);settings.setValue("audio/output","console-capture");auto state=std::make_shared<ConsoleState>();MainWindow w(nullptr,std::make_unique<ConsoleRadio>(state));w.show();auto* tx=w.findChild<QComboBox*>("transmitMode");tx->setCurrentIndex(tx->findData("bw8"));button(w,"playButton")->click();QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("playing")),5000);showConsole(w);QVERIFY(!w.findChild<QAction*>("radioReleaseAction")->isEnabled());QVERIFY(button(w,"radioFrontendStop")->isEnabled());QVERIFY(!button(w,"radioFrontendTune")->isEnabled());QVERIFY(!button(w,"radioFrontendMemoryVfo")->isEnabled());button(w,"radioFrontendStop")->click();QTRY_VERIFY(button(w,"playButton")->isEnabled());QVERIFY(w.findChild<QProgressBar*>("transmitProgress")->value()<100);QVERIFY(state->history().isEmpty());w.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);RadioFrontendTests tests;return QTest::qExec(&tests,argc,argv);}
#include "radio_frontend_tests.moc"
