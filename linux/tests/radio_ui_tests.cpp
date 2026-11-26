#include "mainwindow.h"
#include "radio_ui.h"
#include "fake_radio.h"
#include <QtTest>
#include <QtWidgets>
#include <cmath>

class RadioUiTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    QString audioCopy_;
    static radio::Controller* controller(MainWindow& w){return w.findChild<radio::Controller*>("radioController");}
    static QPushButton* button(MainWindow& w,const char* name){return w.findChild<QPushButton*>(name);}
    static QLabel* status(MainWindow& w){return w.findChild<QLabel*>("statusLabel");}
    static void connectRadio(MainWindow& w){w.findChild<QAction*>("radioConnectAction")->trigger();}
    static void play(MainWindow& w){button(w,"playButton")->click();}
    static void stop(MainWindow& w){button(w,"stopButton")->click();}
    void configure(int lead=300,int tail=100,bool morse=false){
        qputenv("OPENSSTV_RADIO_TEST_REALTIME",morse?"1":"0");QSettings settings;settings.clear();radio::Config c;c.enabled=true;c.leadMs=lead;c.tailMs=tail;c.save(settings);
        settings.setValue("tx/morseEnabled",morse);settings.setValue("tx/morseWpm",40);settings.setValue("station/call",morse?"E":"");settings.setValue("tx/mode","bw8");settings.setValue("audio/output","capture");
    }
    void useShortImage(MainWindow& w){
        auto* list=w.findChild<QListWidget*>("stockPictures");auto* item=list->item(0);list->setCurrentItem(item);QMetaObject::invokeMethod(list,"itemClicked",Q_ARG(QListWidgetItem*,item));
        auto* modes=w.findChild<QComboBox*>("transmitMode");modes->setCurrentIndex(modes->findData("bw8"));
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);
        QCoreApplication::setOrganizationName("OpenSSTVRadioTests");QCoreApplication::setApplicationName("OpenSSTVRadioTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        audioCopy_=directory_.filePath("played.wav");qputenv("OPENSSTV_RADIO_AUDIO_COPY",audioCopy_.toUtf8());
        QImage solid(320,240,QImage::Format_RGB32);solid.fill(qRgb(32,96,160));QVERIFY(solid.save(directory_.filePath("solid.png")));
        QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));
        player.write(R"(#!/usr/bin/python3
import os, shutil, sys, time, wave
device=sys.argv[sys.argv.index('-D')+1]
if device == 'bad':
    print('Simulated audio device failure', file=sys.stderr)
    sys.exit(1)
target=os.environ['OPENSSTV_RADIO_AUDIO_COPY']
shutil.copyfile(sys.argv[-1], target+'.tmp')
os.replace(target+'.tmp', target)
with wave.open(sys.argv[-1], 'rb') as f:
    seconds=f.getnframes()/f.getframerate()
time.sleep(30 if device == 'hung' else seconds if os.environ.get('OPENSSTV_RADIO_TEST_REALTIME') == '1' else .35)
)");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){configure();QFile::remove(audioCopy_);QSettings s;s.setValue("pictures/stock",QStringList{directory_.filePath("solid.png")});}
    void guidedFlexSetupRestoresAutomaticPttAndKeepsAudioIndependent(){
        radio::Config config;config.model=1022;config.device="COM18";config.baud=9600;config.automaticPtt=false;config.pttType="None";
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));RadioSettingsPage page(config,&controller);page.setAudioConnection("flex-dax");
        auto* preset=page.findChild<QComboBox*>("radioSetupPreset");preset->setCurrentIndex(preset->findData("flex-cat"));
        auto c=page.config();QVERIFY(c.enabled);QVERIFY(!c.network);QVERIFY(c.automaticPtt);QCOMPARE(c.model,2014);QCOMPARE(c.device,QString("COM18"));QCOMPARE(c.pttType,QString("RIG"));QCOMPARE(c.pttValue,1);QVERIFY(page.validationError().isEmpty());
        auto* keying=page.findChild<QComboBox*>("radioKeyingChoice");keying->setCurrentIndex(keying->findData("rts"));QVERIFY(page.validationError().contains("separate PTT"));
        page.findChild<QLineEdit*>("radioPttDevice")->setText("COM18");QVERIFY(!page.validationError().isEmpty());page.findChild<QLineEdit*>("radioPttDevice")->setText("COM19");QVERIFY(page.validationError().isEmpty());QCOMPARE(page.config().pttType,QString("RTS"));
        preset->setCurrentIndex(preset->findData("cat"));QVERIFY(page.config().automaticPtt);QCOMPARE(page.config().pttType,QString("RIG"));
        preset->setCurrentIndex(preset->findData("signalink"));QVERIFY(!page.config().automaticPtt);QCOMPARE(page.config().pttType,QString("None"));
        QVERIFY(!controller.connected());QVERIFY(state->history().isEmpty());
    }
    void directFlexAndRigctldUseDifferentProtocols(){
        radio::Config config;config.device="COM12";auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));RadioSettingsPage page(config,&controller);
        auto* preset=page.findChild<QComboBox*>("radioSetupPreset");preset->setCurrentIndex(preset->findData("flex-network"));
        QCOMPARE(page.config().model,23005);QVERIFY(!page.config().network);QVERIFY(page.config().automaticPtt);QVERIFY(!page.validationError().isEmpty());
        page.findChild<QComboBox*>("radioDevice")->setEditText("192.0.2.10");QVERIFY(page.validationError().contains("4992"));
        page.findChild<QComboBox*>("radioDevice")->setEditText("192.0.2.10:4992");QVERIFY(page.validationError().isEmpty());
        preset->setCurrentIndex(preset->findData("rigctld"));QVERIFY(page.config().network);QCOMPARE(page.config().port,4532);QVERIFY(page.config().automaticPtt);QVERIFY(state->history().isEmpty());
    }
    void openingGuidedSettingsPreservesCustomPtt(){
        radio::Config config;config.enabled=true;config.model=1022;config.device="COM24";config.pttType="GPIO";config.pttDevice="17";config.automaticPtt=true;config.leadMs=850;config.tailMs=200;
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));RadioSettingsPage page(config,&controller);page.setAudioConnection("radio-usb");QVERIFY(page.config()==config);QVERIFY(page.findChild<QLabel*>("radioKeyingHelp")->text().contains("GPIO"));QCOMPARE(page.findChild<QComboBox*>("radioKeyingChoice")->currentData().toString(),QString("advanced"));QVERIFY(state->history().isEmpty());
    }
    void ts990UsbPresetAndLegacyInputUseDataPttWithoutConnecting(){
        radio::Config config;config.enabled=true;config.model=2039;config.device="/dev/not-a-real-radio";config.pttValue=3;
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));RadioSettingsPage page(config,&controller);
        QCOMPARE(page.config().pttType,QString("RIGMICDATA"));QCOMPARE(page.config().pttValue,3);
        page.findChild<QComboBox*>("radioPttMethod")->setCurrentIndex(0);page.findChild<QComboBox*>("radioPttValue")->setCurrentIndex(0);page.findChild<QCheckBox*>("radioAutoPtt")->setChecked(false);
        page.findChild<QPushButton*>("radioUsbDataPtt")->click();QCOMPARE(page.config().pttType,QString("RIGMICDATA"));QCOMPARE(page.config().pttValue,3);QVERIFY(page.config().automaticPtt);QVERIFY(!controller.connected());QVERIFY(state->history().isEmpty());
    }
    void separateCatShortcutKeepsModelPortAndBaud(){
        radio::Config config;config.model=1022;config.device="COM18";config.baud=9600;config.enabled=false;config.network=true;
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));RadioSettingsPage page(config,&controller);
        page.findChild<QComboBox*>("radioSetupPreset")->setCurrentIndex(page.findChild<QComboBox*>("radioSetupPreset")->findData("signalink"));const auto selected=page.config();QVERIFY(selected.enabled);QVERIFY(!selected.network);QVERIFY(!selected.automaticPtt);QCOMPARE(selected.pttType,QString("None"));QCOMPARE(selected.model,1022);QCOMPARE(selected.device,QString("COM18"));QCOMPARE(selected.baud,9600);QVERIFY(selected.validate().isEmpty());
        page.findChild<QPushButton*>("radioRefreshDevices")->click();QCOMPARE(page.config().device,QString("COM18"));QCOMPARE(page.config().baud,9600);
        page.setDeviceChoices({"COM18","COM22"});QVERIFY(!page.findChild<QPushButton*>("radioRefreshDevices")->isEnabled());QCOMPARE(page.config().device,QString("COM18"));QVERIFY(!controller.connected());QVERIFY(state->history().isEmpty());
    }
    void settingsAndNativeDummyControls(){
        MainWindow w;w.show();QVERIFY(!controller(w)->connected());bool edited=false;
        QTimer::singleShot(30,[&]{
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);
            auto* tabs=dialog->findChild<QTabWidget*>("settingsTabs");QCOMPARE(tabs->count(),2);tabs->setCurrentIndex(1);
            auto* page=dialog->findChild<RadioSettingsPage*>();QVERIFY(page);QCOMPARE(page->config().model,1);
            auto* models=page->findChild<QComboBox*>("radioModel");QVERIFY(models->count()>100);
            page->findChild<QSpinBox*>("radioLeadMs")->setValue(450);page->findChild<QSpinBox*>("radioTailMs")->setValue(250);
            page->findChild<QPushButton*>("radioCheckConnection")->click();
            QTRY_VERIFY_WITH_TIMEOUT(page->findChild<QLabel*>("radioConnectionResult")->text().contains("Connection OK"),5000);
            QVERIFY(!controller(w)->connected());QCOMPARE(controller(w)->state()["ptt"].toInt(),0);
            const auto screenshot=qEnvironmentVariable("OPENSSTV_RADIO_SETTINGS_PREVIEW");if(!screenshot.isEmpty())QVERIFY(dialog->grab().save(screenshot));
            edited=true;dialog->accept();
        });w.findChild<QAction*>("settingsAction")->trigger();QVERIFY(edited);QSettings settings;QCOMPARE(settings.value("radio/leadMs").toInt(),450);
        connectRadio(w);QTRY_VERIFY_WITH_TIMEOUT(controller(w)->connected(),5000);QCOMPARE(controller(w)->state()["ptt"].toInt(),0);
        button(w,"radioControlsButton")->click();QCOMPARE(w.findChild<QTabWidget*>("imageTabs")->currentWidget()->objectName(),QString("radioPage"));button(w,"radioFrontendAdvanced")->click();auto* controls=w.findChild<RadioControls*>();QVERIFY(controls);QTRY_VERIFY(!controller(w)->busy());
        controls->findChild<QDoubleSpinBox*>("radioFrequency")->setValue(145.55);controls->findChild<QPushButton*>("radioSetFrequency")->click();QTRY_COMPARE(controller(w)->state()["frequencyHz"].toDouble(),145550000.);
        QTRY_VERIFY(!controller(w)->busy());controls->findChild<QComboBox*>("radioOperatingMode")->setCurrentText("FM");controls->findChild<QPushButton*>("radioSetMode")->click();QTRY_COMPARE(controller(w)->state()["mode"].toString(),QString("FM"));
        QTRY_VERIFY(controls->findChild<QPushButton*>("radioSetMode")->isEnabled());
        const auto screenshot=qEnvironmentVariable("OPENSSTV_RADIO_CONTROLS_PREVIEW");if(!screenshot.isEmpty())QVERIFY(controls->grab().save(screenshot));
        QTRY_VERIFY(!controller(w)->busy());controls->findChild<QPushButton*>("radioPttTest")->click();QTRY_COMPARE(controller(w)->state()["ptt"].toInt(),1);QTRY_COMPARE_WITH_TIMEOUT(controller(w)->state()["ptt"].toInt(),0,2500);QVERIFY(!controller(w)->pttActive());w.close();
        MainWindow restored;QVERIFY(!controller(restored)->connected());restored.close();
    }
    void automaticPttIncludesLeadMorseTailAndFullscreen(){
        configure(350,600,true);QSettings settings;settings.setValue("pictures/stock",QStringList{directory_.filePath("solid.png")});
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();useShortImage(w);
        play(w);QVERIFY(!QFile::exists(audioCopy_));QVERIFY(status(w)->text().contains("Connect"));QVERIFY(!state->keyed);
        connectRadio(w);QTRY_VERIFY(controller(w)->connected());
        button(w,"transmitFullScreenButton")->click();auto* full=w.findChild<QWidget*>("transmitFullScreenWindow");QVERIFY(full->isVisible());
        auto* progress=w.findChild<QProgressBar*>("transmitProgress");play(w);QVERIFY(!state->keyed);
        QTRY_VERIFY(state->keyed);QVERIFY(!QFile::exists(audioCopy_));QTest::qWait(100);QVERIFY(!QFile::exists(audioCopy_));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(audioCopy_),2000);QVERIFY(state->keyed);QVERIFY(full->isVisible());
        const int audioWaitMs=int(std::ceil(sstv::readWav(audioCopy_).seconds()*1000))+1000;
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Sending Morse ID"),audioWaitMs);QVERIFY(state->keyed);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("tail delay"),2500);QVERIFY(state->keyed);QVERIFY(full->isVisible());QVERIFY(progress->value()<100);
        QTRY_VERIFY_WITH_TIMEOUT(!state->keyed,2000);QTRY_COMPARE(progress->value(),100);QVERIFY(!full->isVisible());QVERIFY(status(w)->text().contains("Radio RX"));w.close();
    }
    void wavExportAndManualPlaybackNeverKey(){
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();useShortImage(w);const auto target=directory_.filePath("export.wav");bool selected=false;
        QTimer chooser;chooser.setInterval(30);connect(&chooser,&QTimer::timeout,&w,[&]{
            auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!dialog || !dialog->isVisible())return;
            dialog->selectFile(target);if(auto* name=dialog->findChild<QLineEdit*>("fileNameEdit"))name->setText(target);
            selected=true;chooser.stop();static_cast<QDialog*>(dialog)->accept();
        });chooser.start();QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&w,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        button(w,"exportButton")->click();QVERIFY(selected);QTRY_VERIFY(QFile::exists(target));QTRY_VERIFY(button(w,"exportButton")->isEnabled());QVERIFY(!QFile::exists(audioCopy_));QVERIFY(!state->keyed);QVERIFY(state->history().isEmpty());w.close();
        QSettings settings;settings.setValue("radio/automaticPtt",false);auto manualState=std::make_shared<FakeRadioState>();MainWindow manual(nullptr,std::make_unique<FakeRadio>(manualState));manual.show();useShortImage(manual);play(manual);QTRY_VERIFY(QFile::exists(audioCopy_));QTRY_COMPARE(manual.findChild<QProgressBar*>("transmitProgress")->value(),100);QVERIFY(!manualState->keyed);QVERIFY(manualState->history().isEmpty());manual.close();
    }
    void stopDuringLeadNeverStartsAudio(){
        configure(2000,100);auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();connectRadio(w);QTRY_VERIFY(controller(w)->connected());play(w);QTRY_VERIFY(state->keyed);stop(w);
        QTRY_VERIFY(!controller(w)->pttActive());QVERIFY(!state->keyed);QTest::qWait(2100);QVERIFY(!QFile::exists(audioCopy_));QVERIFY(w.findChild<QProgressBar*>("transmitProgress")->value()<100);w.close();
    }
    void signalinkPlaybackUsesAudioWithoutHamlibKeying(){
        QSettings settings;settings.setValue("audio/setupConnection","signalink");settings.setValue("radio/enabled",false);settings.setValue("radio/automaticPtt",false);
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();useShortImage(w);QVERIFY(w.findChild<QLabel*>("recordingInfo")->text().contains("SignaLink audio"));play(w);QTRY_VERIFY(QFile::exists(audioCopy_));QVERIFY(status(w)->text().contains("SignaLink audio PTT"));QVERIFY(!state->keyed);QVERIFY(state->history().isEmpty());QTRY_COMPARE(w.findChild<QProgressBar*>("transmitProgress")->value(),100);QVERIFY(status(w)->text().contains("Check SignaLink PTT indicator"));QVERIFY(!state->keyed);w.close();
    }
    void signalinkPlaybackWorksWithCatConnected(){
        QSettings settings;settings.setValue("audio/setupConnection","signalink");radio::Config config=radio::Config::load(settings);config.enabled=true;config.model=1022;config.device="COM18";config.baud=9600;config.automaticPtt=false;config.pttType="None";config.save(settings);
        class TuningRadio final:public FakeRadio {
            double frequency_=145550000.;QString mode_="FM";
        public:
            explicit TuningRadio(std::shared_ptr<FakeRadioState> state):FakeRadio(std::move(state)){}
            QVariantMap read(bool pttOnly=false) override{auto state=FakeRadio::read(pttOnly);state["frequencyHz"]=frequency_;state["mode"]=mode_;return state;}
            void apply(const radio::Command& command) override{FakeRadio::apply(command);if(command.control==radio::Control::Frequency)frequency_=command.value;if(command.control==radio::Control::Mode)mode_=command.text;}
        };
        auto state=std::make_shared<FakeRadioState>();MainWindow window(nullptr,std::make_unique<TuningRadio>(state));window.show();useShortImage(window);connectRadio(window);QTRY_VERIFY(controller(window)->connected());
        controller(window)->apply({radio::Control::Frequency,14230000.,{}});QTRY_VERIFY(!controller(window)->busy());QCOMPARE(controller(window)->state().value("frequencyHz").toDouble(),14230000.);QVERIFY(controller(window)->error().isEmpty());
        controller(window)->apply({radio::Control::Mode,0.,"USB"});QTRY_VERIFY(!controller(window)->busy());QCOMPARE(controller(window)->state().value("mode").toString(),QString("USB"));QVERIFY(controller(window)->error().isEmpty());
        QCOMPARE(state->history().count("apply"),2);play(window);QTRY_VERIFY(QFile::exists(audioCopy_));QTRY_COMPARE(window.findChild<QProgressBar*>("transmitProgress")->value(),100);
        QVERIFY(controller(window)->connected());QVERIFY(!state->keyed);QVERIFY(!state->history().contains("on-start"));QVERIFY(!state->history().contains("off-start"));QVERIFY(status(window)->text().contains("SignaLink"));window.close();
    }
    void cancelledPttTestCannotReleaseLaterTransmission_data(){
        QTest::addColumn<bool>("closeDialog");QTest::newRow("old-timeout")<<false;QTest::newRow("old-dialog-close")<<true;
    }
    void cancelledPttTestCannotReleaseLaterTransmission(){
        QFETCH(bool,closeDialog);
        auto state=std::make_shared<FakeRadioState>();radio::Controller controller(nullptr,std::make_unique<FakeRadio>(state));controller.connectRadio({});QTRY_VERIFY(controller.connected());
        RadioControls controls(&controller,[]{return true;},[&]{controller.release();});controls.setAttribute(Qt::WA_DeleteOnClose,false);controls.show();
        controls.findChild<QPushButton*>("radioPttTest")->click();QTRY_VERIFY(state->keyed);QTRY_VERIFY(!controller.busy());
        controls.findChild<QPushButton*>("radioPttOff")->click();QTRY_VERIFY(!controller.pttActive());QVERIFY(!state->keyed);
        controller.key([](QString error){QVERIFY(error.isEmpty());});QTRY_VERIFY(state->keyed);QTRY_VERIFY(!controller.busy());
        if(closeDialog){controls.close();QTest::qWait(100);}else QTest::qWait(1200);
        QVERIFY2(state->keyed,"An old PTT test must not release a later transmission");QVERIFY(controller.pttActive());
        controller.release();QTRY_VERIFY(!controller.pttActive());QVERIFY(!state->keyed);
    }
    void stopWhileKeyIsInFlight(){
        auto state=std::make_shared<FakeRadioState>();state->keyDelayMs=350;MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();connectRadio(w);QTRY_VERIFY(controller(w)->connected());play(w);QTRY_VERIFY(state->history().contains("on-start"));stop(w);
        QTRY_VERIFY_WITH_TIMEOUT(!controller(w)->pttActive(),2000);QVERIFY(!state->keyed);QTest::qWait(450);QVERIFY(!QFile::exists(audioCopy_));w.close();
    }
    void audioAndPttErrors(){
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();connectRadio(w);QTRY_VERIFY(controller(w)->connected());
        state->failOn=true;play(w);QTRY_VERIFY(status(w)->text().contains("Could not key"));QTRY_VERIFY(!controller(w)->pttActive());QVERIFY(!QFile::exists(audioCopy_));QVERIFY(!state->keyed);
        state->failOn=false;w.findChild<QLineEdit*>("outputDevice")->setText("bad");play(w);QTRY_VERIFY(status(w)->text().contains("Simulated audio device failure"));QTRY_VERIFY(!controller(w)->pttActive());QVERIFY(!state->keyed);
        w.findChild<QLineEdit*>("outputDevice")->setText("capture");play(w);QTRY_VERIFY(state->keyed);state->failOff=true;stop(w);QTRY_VERIFY(status(w)->text().contains("PTT OFF not confirmed"));QVERIFY(controller(w)->pttActive());QVERIFY(!button(w,"playButton")->isEnabled());QVERIFY(w.findChild<QAction*>("settingsAction")->isEnabled());
        state->failOff=false;w.findChild<QAction*>("radioReleaseAction")->trigger();QTRY_VERIFY(!controller(w)->pttActive());QVERIFY(!state->keyed);w.close();
    }
    void cableFaultStopsAudioAndReleases(){
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();connectRadio(w);QTRY_VERIFY(controller(w)->connected());play(w);QTRY_VERIFY(QFile::exists(audioCopy_));QVERIFY(state->keyed);
        QTRY_VERIFY(!controller(w)->busy());state->failRead=true;controller(w)->refresh();QTRY_VERIFY(!controller(w)->connected());QTRY_VERIFY(!state->keyed);QVERIFY(w.findChild<QProgressBar*>("transmitProgress")->value()<100);w.close();
    }
    void watchdogAndCloseReleasePtt(){
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();useShortImage(w);w.findChild<QLineEdit*>("outputDevice")->setText("hung");connectRadio(w);QTRY_VERIFY(controller(w)->connected());play(w);QTRY_VERIFY(QFile::exists(audioCopy_));QVERIFY(state->keyed);
        QTRY_VERIFY_WITH_TIMEOUT(!controller(w)->pttActive(),18000);QVERIFY(!state->keyed);QVERIFY(w.findChild<QProgressBar*>("transmitProgress")->value()<100);QVERIFY(status(w)->text().contains("exceeded its expected duration"));
        play(w);QTRY_VERIFY(state->keyed);w.close();QVERIFY(!state->keyed);QVERIFY(state->history().last()=="close");
    }
    void audioOnlyWatchdogStopsHungPlaybackWithoutCat_data(){
        QTest::addColumn<QString>("connection");QTest::newRow("signalink")<<QString("signalink");QTest::newRow("manual")<<QString("manual");
    }
    void audioOnlyWatchdogStopsHungPlaybackWithoutCat(){
        QFETCH(QString,connection);QSettings settings;settings.setValue("audio/setupConnection",connection);settings.setValue("radio/enabled",false);settings.setValue("radio/automaticPtt",false);
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();useShortImage(w);w.findChild<QLineEdit*>("outputDevice")->setText("hung");play(w);QTRY_VERIFY(QFile::exists(audioCopy_));
        // Wait beyond the WAV duration plus the five-second allowance, with
        // scheduling margin, but before the 30-second fake-player exit. Avoid
        // a retry macro that could count that eventual exit as a watchdog.
        const int deadline=int(std::ceil(sstv::readWav(audioCopy_).seconds()*1000))+6500;QVERIFY(deadline<25000);
        QTest::qWait(deadline);QVERIFY2(status(w)->text().contains("exceeded its expected duration"),qPrintable(status(w)->text()));QVERIFY(!status(w)->text().contains("PTT OFF"));
        QVERIFY(button(w,"playButton")->isEnabled());QVERIFY(w.findChild<QProgressBar*>("transmitProgress")->value()<100);QVERIFY(!state->keyed);QVERIFY(state->history().isEmpty());w.close();
    }
    void completedAudioOnlyPlaybackCancelsItsWatchdog(){
        QSettings settings;settings.setValue("audio/setupConnection","signalink");settings.setValue("radio/enabled",false);settings.setValue("radio/automaticPtt",false);
        auto state=std::make_shared<FakeRadioState>();MainWindow w(nullptr,std::make_unique<FakeRadio>(state));w.show();useShortImage(w);play(w);QTRY_VERIFY(QFile::exists(audioCopy_));QTRY_COMPARE(w.findChild<QProgressBar*>("transmitProgress")->value(),100);const auto completed=status(w)->text();QVERIFY(completed.contains("Playback complete"));
        QTest::qWait(int(std::ceil(sstv::readWav(audioCopy_).seconds()*1000))+6500);QCOMPARE(status(w)->text(),completed);QCOMPARE(w.findChild<QProgressBar*>("transmitProgress")->value(),100);QVERIFY(button(w,"playButton")->isEnabled());QVERIFY(state->history().isEmpty());w.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){
    QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();
    QApplication app(argc,argv);RadioUiTests test;return QTest::qExec(&test,argc,argv);
}
#include "radio_ui_tests.moc"
