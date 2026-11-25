#include "mainwindow.h"
#include "core/morse.h"
#include "skins.h"
#include "composer.h"
#include "core/patterns.h"
#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>
#include <QListWidget>
#include <QAction>
#include <QDialog>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QSettings>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QRegularExpression>

class UiTests:public QObject {
    Q_OBJECT
private:
    QTemporaryDir directory_;
    QByteArray oldPath_;
    static QSet<QString> savedFiles(QListWidget* history) {
        QSet<QString> paths;
        for(int i=0;i<history->count();++i) {
            const auto path=history->item(i)->data(Qt::UserRole).toString();
            if(!path.isEmpty())paths.insert(path);
        }
        return paths;
    }
    static qint64 countdownSeconds(QProgressBar* progress) {
        const auto match=QRegularExpression("(\\d+) s remaining").match(progress->text());
        return match.hasMatch()?match.captured(1).toLongLong():-1;
    }
private slots:
    void tuxBrandingAndDeveloperCredit(){
        MainWindow window;QVERIFY(window.windowTitle().startsWith("Tux-sstv"));QVERIFY(!window.windowIcon().isNull());
        auto* tux=window.findChild<QLabel*>("radioTuxMascot");QVERIFY(tux);QVERIFY(!tux->pixmap().isNull());QVERIFY(window.findChild<QLabel*>("radioConsoleTitle")->text().contains("Tux-sstv"));
        bool seen=false;QTimer::singleShot(30,&window,[&]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);const auto close=qScopeGuard([box]{box->accept();});QCOMPARE(box->objectName(),QString("aboutTuxSstv"));QCOMPARE(box->windowTitle(),QString("About Tux-sstv"));QVERIFY(box->text().contains("Eric Richards"));QVERIFY(box->text().contains("KI5BZO"));QVERIFY(!box->iconPixmap().isNull());
            const auto path=qEnvironmentVariable("TUXSSTV_BRANDING_PREVIEW");if(!path.isEmpty()){QVERIFY(box->grab().save(path+"-about.png"));QVERIFY(window.windowIcon().pixmap(128,128).save(path+"-icon.png"));QVERIFY(tux->pixmap().save(path+"-mascot.png"));}seen=true;});window.findChild<QAction*>("aboutAction")->trigger();QVERIFY(seen);window.close();
    }
    void initTestCase() {
        QVERIFY(directory_.isValid());
        oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        QCoreApplication::setOrganizationName("OpenSSTVTests");QCoreApplication::setApplicationName("OpenSSTVTests");
        const auto audio=sstv::encode(sstv::testPattern(),*sstv::modeById("martin2"),12000);
        auto partial=audio;partial.samples.resize(size_t((.910+12*.226798+.1)*12000));
        sstv::writeWav(directory_.filePath("fixture.wav"),partial);
        qputenv("OPENSSTV_TEST_WAV",directory_.filePath("fixture.wav").toUtf8());
        auto sequence=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);
        auto manual=sequence;manual.samples.insert(manual.samples.end(),sequence.samples.begin(),sequence.samples.end());
        sstv::writeWav(directory_.filePath("manual.wav"),manual);qputenv("OPENSSTV_TEST_MANUAL",directory_.filePath("manual.wav").toUtf8());
        qputenv("OPENSSTV_TEST_FIRST_END",QByteArray::number(qint64(sequence.samples.size()*2)));
        auto next=sstv::encode(sstv::testPattern(),*sstv::modeById("bw12"),12000);
        sequence.samples.insert(sequence.samples.end(),next.samples.begin(),next.samples.end());
        qputenv("OPENSSTV_TEST_SECOND_END",QByteArray::number(qint64(sequence.samples.size()*2)));
        auto last=sstv::encode(sstv::testPattern(),*sstv::modeById("robot24"),12000);last.samples.resize(24000);
        sequence.samples.insert(sequence.samples.end(),last.samples.begin(),last.samples.end());
        sstv::writeWav(directory_.filePath("sequence.wav"),sequence);
        qputenv("OPENSSTV_TEST_SEQUENCE",directory_.filePath("sequence.wav").toUtf8());
        QFile helper(directory_.filePath("arecord"));QVERIFY(helper.open(QIODevice::WriteOnly));
        helper.write(R"(#!/usr/bin/python3
import os, signal, sys, time, wave
device=sys.argv[sys.argv.index('-D')+1]
if device == 'bad-test-device':
    print('Test capture device unavailable', file=sys.stderr)
    sys.exit(1)
if device == 'default':
    while True:
        os.write(1,bytes(2400));time.sleep(.1)
fixture='OPENSSTV_TEST_MANUAL' if device == 'manual-sequence-device' else 'OPENSSTV_TEST_SEQUENCE' if device in ('auto-sequence-device', 'fullscreen-sequence-device') else 'OPENSSTV_TEST_WAV'
with wave.open(os.environ[fixture], 'rb') as f:
    data=f.readframes(f.getnframes())
def send(block):
    while block:
        block=block[os.write(1, block):]
if device == 'manual-sequence-device':
    first_end=int(os.environ['OPENSSTV_TEST_FIRST_END'])
    send(data[:48000])
    time.sleep(2.5)
    send(data[48000:first_end+48000])
    time.sleep(2.5)
    send(data[first_end+48000:])
    signal.pause()
if device == 'fullscreen-sequence-device':
    first_end=int(os.environ['OPENSSTV_TEST_FIRST_END'])
    second_end=int(os.environ['OPENSSTV_TEST_SECOND_END'])
    send(data[:first_end])
    time.sleep(2.5)
    send(bytes(48000))
    time.sleep(2.5)
    send(data[first_end:first_end+48000])
    time.sleep(2.5)
    send(data[first_end+48000:second_end])
    time.sleep(2.5)
    send(data[second_end:])
    signal.pause()
if device == 'auto-sequence-device':
    first, data = data[:48000], data[48000:]
    while first:
        first=first[os.write(1, first):]
    time.sleep(2.5)
while data:
    data=data[os.write(1, data):]
signal.pause()
)");helper.close();QVERIFY(helper.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));
        player.write(R"(#!/usr/bin/python3
import os, shutil, sys, time, wave
device=sys.argv[sys.argv.index('-D')+1]
if device == 'bad-test-device':
    print('Test playback device unavailable', file=sys.stderr)
    sys.exit(1)
if device in ('morse-test-device', 'capture-tx-device'):
    target=os.environ['OPENSSTV_TEST_TX_COPY']
    shutil.copyfile(sys.argv[-1], target+'.tmp')
    os.replace(target+'.tmp', target)
if device == 'morse-test-device':
    with wave.open(sys.argv[-1], 'rb') as f:
        time.sleep(f.getnframes()/f.getframerate())
else:
    time.sleep(30 if device == 'slow-test-device' else 2.2)
)");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void correctionControlsPersist() {
        QSettings settings;settings.remove("rx/autoClock");settings.remove("rx/autoFrequency");
        {
            MainWindow window;
            auto* clock=window.findChild<QCheckBox*>("autoClock");auto* afc=window.findChild<QCheckBox*>("autoFrequency");
            QVERIFY(clock);QVERIFY(afc);QVERIFY(!clock->isChecked());QVERIFY(!afc->isChecked());
            clock->setChecked(true);afc->setChecked(true);window.close();
        }
        {
            MainWindow window;
            QVERIFY(window.findChild<QCheckBox*>("autoClock")->isChecked());QVERIFY(window.findChild<QCheckBox*>("autoFrequency")->isChecked());
            window.findChild<QCheckBox*>("autoClock")->setChecked(false);window.findChild<QCheckBox*>("autoFrequency")->setChecked(false);window.close();
        }
    }
    void classicOperatingControls() {
        const auto picture=directory_.filePath("stock-test.png");QVERIFY(sstv::testPattern().save(picture));
        QSettings settings;settings.setValue("pictures/stock",QStringList{picture});
        MainWindow window;window.show();
        auto* viewer=window.findChild<QTabWidget*>("imageTabs");
        auto* modes=window.findChild<QTabWidget*>("modeTabs");
        auto* tx=window.findChild<QComboBox*>("transmitMode");
        auto* rx=window.findChild<QComboBox*>("receiveMode");
        auto* stock=window.findChild<QListWidget*>("stockPictures");
        auto* scottie=window.findChild<QPushButton*>("txMode_scottie2");
        auto* robot=window.findChild<QPushButton*>("rxMode_robot36");
        auto* automatic=window.findChild<QPushButton*>("rxMode_auto");
        QVERIFY(viewer&&modes&&tx&&rx&&stock&&scottie&&robot&&automatic);
        QCOMPARE(tx->count(),43);QCOMPARE(rx->count(),44);QVERIFY(tx->findData("mca")<0);QVERIFY(rx->findData("mca")<0);QVERIFY(!window.findChild<QWidget*>("mcaTransmitProfile"));QVERIFY(!window.findChild<QPushButton*>("txMode_mca"));QVERIFY(!window.findChild<QPushButton*>("rxMode_mca"));
        for(const auto& mode:sstv::modes()){QVERIFY(tx->findData(mode.id)>=0);QVERIFY(rx->findData(mode.id)>=0);}
        QCOMPARE(viewer->currentIndex(),1);QCOMPARE(modes->currentIndex(),1);
        QTest::mouseClick(scottie,Qt::LeftButton);QCOMPARE(tx->currentData().toString(),QString("scottie2"));QVERIFY(scottie->isChecked());
        tx->setCurrentIndex(tx->findData("pd160"));QVERIFY(!scottie->isChecked());
        viewer->setCurrentIndex(0);QCOMPARE(modes->currentIndex(),0);
        QTest::mouseClick(robot,Qt::LeftButton);QCOMPARE(rx->currentData().toString(),QString("robot36"));
        QTest::mouseClick(automatic,Qt::LeftButton);QVERIFY(rx->currentData().toString().isEmpty());
        viewer->setCurrentIndex(2);QCOMPARE(viewer->tabText(2),QString("Sync"));QCOMPARE(modes->currentIndex(),0);
        QCOMPARE(stock->count(),1);QCOMPARE(stock->item(0)->data(Qt::UserRole).toString(),picture);QVERIFY(!stock->item(0)->icon().isNull());
        QTest::mouseClick(stock->viewport(),Qt::LeftButton,Qt::NoModifier,stock->visualItemRect(stock->item(0)).center());
        QCOMPARE(viewer->currentIndex(),1);QCOMPARE(modes->currentIndex(),1);

        auto* action=window.findChild<QAction*>("settingsAction");QVERIFY(action);
        bool edited=false;
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog)return;
            auto* field=dialog->findChild<QLineEdit*>("settingsInput");
            if(field){field->setText("new-test-input");edited=true;}
            dialog->accept();
        });
        action->trigger();QVERIFY(edited);
        QCOMPARE(window.findChild<QLineEdit*>("inputDevice")->text(),QString("new-test-input"));
        window.close();
    }
    void playbackProgressLifecycle() {
        MainWindow window;window.show();
        auto* play=window.findChild<QPushButton*>("playButton");
        auto* stop=window.findChild<QPushButton*>("stopButton");
        auto* output=window.findChild<QLineEdit*>("outputDevice");
        auto* mode=window.findChild<QComboBox*>("transmitMode");
        auto* progress=window.findChild<QProgressBar*>("transmitProgress");
        auto* detail=window.findChild<QLabel*>("transmitProgressInfo");
        auto* fullScreen=window.findChild<QPushButton*>("transmitFullScreenButton");
        auto* fullWindow=window.findChild<QWidget*>("transmitFullScreenWindow");
        QVERIFY(play&&stop&&output&&mode&&progress&&detail&&fullScreen&&fullWindow);
        const auto standardState=window.windowState();
        mode->setCurrentIndex(mode->findData("martin2"));
        QCOMPARE(progress->value(),0);QVERIFY(progress->isTextVisible());QVERIFY(progress->height()>=24);
        QVERIFY(countdownSeconds(progress)>0);
        // Space belongs to the TX viewer, never the RX viewer.
        window.findChild<QTabWidget*>("imageTabs")->setCurrentIndex(0);
        QTest::mouseClick(window.findChild<QPushButton*>("receiveFullScreenButton"),Qt::LeftButton);
        auto* rxFullWindow=window.findChild<QWidget*>("receiveFullScreenWindow");QTRY_VERIFY(rxFullWindow->isVisible());QTRY_VERIFY(rxFullWindow->isActiveWindow());
        QTest::keyClick(rxFullWindow,Qt::Key_Space);QVERIFY(play->isEnabled());QVERIFY(progress->format().contains("Ready"));
        QTest::keyClick(rxFullWindow,Qt::Key_Escape);QTRY_VERIFY(!rxFullWindow->isVisible());
        window.findChild<QTabWidget*>("imageTabs")->setCurrentIndex(1);
        output->setText("test-device");
        QVERIFY(fullScreen->isEnabled());QTest::mouseClick(fullScreen,Qt::LeftButton);
        QTRY_VERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());
        QTRY_VERIFY(fullWindow->isActiveWindow());
        QTest::keyClick(fullWindow,Qt::Key_Space);QVERIFY(progress->format().contains("Preparing"));
        QTest::keyClick(fullWindow,Qt::Key_Space);QVERIFY(!play->isEnabled());
        QVERIFY(fullWindow->isAncestorOf(progress));QVERIFY(fullWindow->isAncestorOf(detail));
        QVERIFY(progress->isVisible());QVERIFY(progress->mapTo(fullWindow,QPoint(0,0)).y()>fullWindow->height()/2);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Playing TX"),4000);
        const auto initialCountdown=countdownSeconds(progress);QVERIFY(initialCountdown>0);QVERIFY(progress->text().contains('%'));
        const auto initialProgress=progress->value();QTest::keyClick(fullWindow,Qt::Key_Space);
        QVERIFY(progress->format().contains("Playing TX"));QVERIFY(progress->value()>=initialProgress);
        QVERIFY(progress->value()<100);QVERIFY(detail->text().contains("Elapsed"));QVERIFY(detail->text().contains("Remaining"));
        QVERIFY(!window.findChild<QPushButton*>("txMode_scottie1")->isEnabled());
        QVERIFY(!window.findChild<QListWidget*>("stockPictures")->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("compositionButton")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(progress->value()>0,1800);QVERIFY(progress->value()<100);
        QTRY_VERIFY_WITH_TIMEOUT(countdownSeconds(progress)>=0 && countdownSeconds(progress)<initialCountdown,1500);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Playback complete"),4000);
        QCOMPARE(progress->value(),100);QVERIFY(detail->text().contains("Remaining 0:00"));
        QCOMPARE(countdownSeconds(progress),qint64(0));
        QTRY_VERIFY(!fullWindow->isVisible());
        QVERIFY(!fullWindow->isAncestorOf(progress));QVERIFY(progress->isVisible());QCOMPARE(progress->value(),100);
        QCOMPARE(window.windowState(),standardState);
        QVERIFY(play->isEnabled());

        output->setText("bad-test-device");QTest::mouseClick(play,Qt::LeftButton);
        QTest::mouseClick(fullScreen,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Playback failed"),4000);
        QVERIFY(progress->value()<100);QVERIFY(play->isEnabled());
        QVERIFY(fullWindow->isVisible());fullWindow->close();QTRY_VERIFY(!fullWindow->isVisible());
        QVERIFY(!fullWindow->isAncestorOf(progress));QVERIFY(progress->isVisible());

        output->setText("slow-test-device");QTest::mouseClick(play,Qt::LeftButton);
        QTest::mouseClick(fullScreen,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Playing TX"),4000);
        QTRY_VERIFY_WITH_TIMEOUT(progress->value()>0,1800);
        const auto playingValue=progress->value();
        QTest::keyClick(fullWindow,Qt::Key_Escape);QTRY_VERIFY(!fullWindow->isVisible());
        QVERIFY(progress->value()>=playingValue);QVERIFY(progress->format().contains("Playing TX"));QVERIFY(!play->isEnabled());
        QTest::mouseClick(stop,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().startsWith("Stopped"),2000);
        QTRY_VERIFY(play->isEnabled());
        const int frozen=progress->value();QVERIFY(frozen<100);
        QTest::qWait(350);QCOMPARE(progress->value(),frozen);

        // Changing the image/mode returns the display to ready instead of
        // retaining a previous image's completed or stopped progress.
        mode->setCurrentIndex(mode->findData("robot36"));
        QCOMPARE(progress->value(),0);QVERIFY(progress->format().contains("Ready"));
        QTest::mouseClick(fullScreen,Qt::LeftButton);
        QTest::mouseClick(play,Qt::LeftButton);QTest::mouseClick(stop,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),4000);
        QVERIFY(progress->format().startsWith("Preparation stopped"));
        QTest::qWait(150);QVERIFY(progress->format().startsWith("Preparation stopped"));
        QVERIFY(fullWindow->isVisible());window.close();QVERIFY(!fullWindow->isVisible());
    }
    void controlsAndCaptureLifecycle() {
        MainWindow window;window.show();
        auto* record=window.findChild<QPushButton*>("recordButton");
        auto* stop=window.findChild<QPushButton*>("stopButton");
        auto* status=window.findChild<QLabel*>("statusLabel");
        auto* input=window.findChild<QLineEdit*>("inputDevice");
        auto* tx=window.findChild<QComboBox*>("transmitMode");
        auto* rx=window.findChild<QComboBox*>("receiveMode");
        QVERIFY(record&&stop&&status&&input&&tx&&rx);QCOMPARE(tx->count(),43);QCOMPARE(rx->count(),44);QTRY_VERIFY(stop->isEnabled());
        input->setText("bad-test-device");QTest::mouseClick(record,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel*>("liveAudioState")->text().contains("Test capture device unavailable"),3000);QVERIFY(record->isEnabled());
        input->setText("test-device");QTest::mouseClick(record,Qt::LeftButton);
        QTRY_COMPARE(record->text(),QString("Finish recording"));QVERIFY(!tx->isEnabled());QVERIFY(stop->isEnabled());
        QTest::qWait(200);QTest::mouseClick(record,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel*>("receiveInfo")->text().contains("Martin 2")&&window.findChild<QLabel*>("receiveInfo")->text().contains("partial"),4000);
        const auto finishedStatus=status->text();QVERIFY(record->isEnabled());QVERIFY(tx->isEnabled());
        // A delayed kill belonging to the old recording must not kill this one.
        QTest::mouseClick(record,Qt::LeftButton);QTest::qWait(1700);QCOMPARE(record->text(),QString("Finish recording"));
        QTest::mouseClick(stop,Qt::LeftButton);QTRY_VERIFY(record->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel*>("liveAudioState")->text().startsWith("Receiving automatically"),2000);QVERIFY(stop->isEnabled());
        QVERIFY(!finishedStatus.isEmpty());
        window.close();
    }
    void automaticLiveModeDetection() {
        MainWindow window;window.show();
        auto* record=window.findChild<QPushButton*>("recordButton");
        auto* input=window.findChild<QLineEdit*>("inputDevice");
        auto* rx=window.findChild<QComboBox*>("receiveMode");
        auto* detected=window.findChild<QLabel*>("detectedReceiveMode");
        auto* status=window.findChild<QLabel*>("statusLabel");
        auto* history=window.findChild<QListWidget*>("receiveHistory");
        auto* start=window.findChild<QDoubleSpinBox*>("receiveStart");
        auto* fullScreen=window.findChild<QPushButton*>("receiveFullScreenButton");
        auto* fullWindow=window.findChild<QWidget*>("receiveFullScreenWindow");
        auto* fullImage=window.findChild<QLabel*>("receiveFullScreenImage");
        QVERIFY(record&&input&&rx&&detected&&status&&history&&start&&fullScreen&&fullWindow&&fullImage);
        const auto before=savedFiles(history).size();
        QVERIFY(rx->currentData().toString().isEmpty());QVERIFY(!start->isEnabled());
        // Returning to Auto ignores a stale manual timing override.
        rx->setCurrentIndex(rx->findData("robot36"));QVERIFY(start->isEnabled());start->setValue(40000);
        rx->setCurrentIndex(0);QVERIFY(!start->isEnabled());
        input->setText("auto-sequence-device");QTest::mouseClick(record,Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(detected->property("modeId").toString(),QString("bw8"),2200);
        QVERIFY(detected->text().contains("Auto detected"));QCOMPARE(record->text(),QString("Finish recording"));
        QTRY_VERIFY(window.findChild<QLabel*>("receiveInfo")->text().contains("partial"));
        const auto standardState=window.windowState();
        QVERIFY(fullScreen->isEnabled());QTest::mouseClick(fullScreen,Qt::LeftButton);
        QTRY_VERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());QVERIFY(!fullWindow->isModal());
        const auto firstPicture=fullImage->grab().toImage();
        QTRY_COMPARE_WITH_TIMEOUT(detected->property("modeId").toString(),QString("robot24"),4000);
        QTRY_VERIFY(fullImage->grab().toImage()!=firstPicture);
        QVERIFY(fullWindow->isVisible());
        QTest::keyClick(fullWindow,Qt::Key_Escape);
        QTRY_VERIFY(!fullWindow->isVisible());QCOMPARE(window.windowState(),standardState);
        QVERIFY(rx->currentData().toString().isEmpty());QCOMPARE(record->text(),QString("Finish recording"));
        QTRY_COMPARE(savedFiles(history).size(),before+3);QVERIFY(history->item(0)->text().endsWith(".png"));
        const auto saved=history->item(0)->data(Qt::UserRole).toString();QVERIFY(QFile::exists(saved));
        QVERIFY(saved!=history->item(1)->data(Qt::UserRole).toString());
        QTest::mouseClick(record,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel*>("receiveInfo")->text().contains("Robot 24")&&window.findChild<QLabel*>("receiveInfo")->text().contains("partial"),3000);
        QCOMPARE(savedFiles(history).size(),before+3);QCOMPARE(history->item(0)->data(Qt::UserRole).toString(),saved);
        QTest::mouseClick(fullScreen,Qt::LeftButton);QTRY_VERIFY(fullWindow->isVisible());
        QTest::keyClick(fullWindow,Qt::Key_Escape);QTRY_VERIFY(!fullWindow->isVisible());
        QTest::mouseClick(fullScreen,Qt::LeftButton);QTRY_VERIFY(fullWindow->isVisible());
        window.close();QVERIFY(!fullWindow->isVisible());
    }
    void fullScreenAcrossReceivedImages() {
        MainWindow window;window.show();
        auto* record=window.findChild<QPushButton*>("recordButton");
        auto* input=window.findChild<QLineEdit*>("inputDevice");
        auto* history=window.findChild<QListWidget*>("receiveHistory");
        auto* detected=window.findChild<QLabel*>("detectedReceiveMode");
        auto* fullScreen=window.findChild<QPushButton*>("receiveFullScreenButton");
        auto* fullWindow=window.findChild<QWidget*>("receiveFullScreenWindow");
        auto* fullImage=window.findChild<QLabel*>("receiveFullScreenImage");
        QVERIFY(record&&input&&history&&detected&&fullScreen&&fullWindow&&fullImage);
        const auto files=[&] {
            QSet<QString> paths;
            for(int i=0;i<history->count();++i) {
                const auto path=history->item(i)->data(Qt::UserRole).toString();
                if(!path.isEmpty())paths.insert(path);
            }
            return paths;
        };
        const auto before=files();
        input->setText("fullscreen-sequence-device");QTest::mouseClick(record,Qt::LeftButton);
        QTest::mouseClick(fullScreen,Qt::LeftButton);
        QTRY_VERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());
        QTRY_COMPARE_WITH_TIMEOUT(files().size(),before.size()+1,2200);
        const auto firstFile=*(files()-before).begin();
        QFile firstSaved(firstFile);QVERIFY(firstSaved.open(QIODevice::ReadOnly));
        const auto firstBytes=firstSaved.readAll();firstSaved.close();QVERIFY(!firstBytes.isEmpty());
        const auto firstPicture=fullImage->grab().toImage();
        // Enter and the gap between transmissions must not dismiss the viewer.
        QTest::keyClick(fullWindow,Qt::Key_Return);QTest::qWait(2200);
        QVERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());
        QCOMPARE(fullImage->grab().toImage(),firstPicture);QCOMPARE(files().size(),before.size()+1);
        QTRY_COMPARE_WITH_TIMEOUT(detected->property("modeId").toString(),QString("bw12"),4000);
        QTRY_VERIFY(fullImage->grab().toImage()!=firstPicture);
        QVERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());
        QCOMPARE(files().size(),before.size()+2);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel*>("receiveInfo")->text().contains("B/W 12")&&window.findChild<QLabel*>("receiveInfo")->text().contains("complete"),4000);
        QVERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());
        QVERIFY(firstSaved.open(QIODevice::ReadOnly));QCOMPARE(firstSaved.readAll(),firstBytes);firstSaved.close();
        for(const auto& path:files()-before)QVERIFY(QFile::exists(path));
        QTRY_COMPARE_WITH_TIMEOUT(detected->property("modeId").toString(),QString("robot24"),4000);
        QVERIFY(fullWindow->isVisible()&&fullWindow->isFullScreen());
        QCOMPARE(record->text(),QString("Finish recording"));
        QTest::keyClick(fullWindow,Qt::Key_Escape);QTRY_VERIFY(!fullWindow->isVisible());
        QCOMPARE(record->text(),QString("Finish recording"));
        QTest::mouseClick(record,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(record->text()=="Record audio"&&record->isEnabled(),3000);
        QCOMPARE(files().size(),before.size()+3);window.close();
    }
    void manualLiveAutoSave_data() {
        QTest::addColumn<bool>("manualStart");QTest::newRow("header-timed")<<false;QTest::newRow("manual-first-start")<<true;
    }
    void manualLiveAutoSave() {
        QFETCH(bool,manualStart);MainWindow window;window.show();
        auto* record=window.findChild<QPushButton*>("recordButton");
        auto* input=window.findChild<QLineEdit*>("inputDevice");
        auto* rx=window.findChild<QComboBox*>("receiveMode");
        auto* history=window.findChild<QListWidget*>("receiveHistory");
        auto* info=window.findChild<QLabel*>("receiveInfo");
        const auto before=savedFiles(history);
        rx->setCurrentIndex(rx->findData("bw8"));
        if(manualStart)window.findChild<QDoubleSpinBox*>("receiveStart")->setValue(910);
        input->setText("manual-sequence-device");QTest::mouseClick(record,Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(savedFiles(history).size(),before.size()+1,2200);
        const auto firstFile=*(savedFiles(history)-before).begin();
        QFile firstSaved(firstFile);QVERIFY(firstSaved.open(QIODevice::ReadOnly));const auto partial=firstSaved.readAll();firstSaved.close();
        QTRY_COMPARE_WITH_TIMEOUT(savedFiles(history).size(),before.size()+2,4000);
        QVERIFY(firstSaved.open(QIODevice::ReadOnly));const auto complete=firstSaved.readAll();firstSaved.close();QVERIFY(complete!=partial);
        QTRY_VERIFY_WITH_TIMEOUT(info->text().contains("complete")&&!info->text().contains("partial"),4000);
        QCOMPARE(savedFiles(history).size(),before.size()+2);
        QVERIFY(firstSaved.open(QIODevice::ReadOnly));QCOMPARE(firstSaved.readAll(),complete);firstSaved.close();
        for(const auto& path:savedFiles(history)-before)QVERIFY(!QImage(path).isNull());
        QTest::mouseClick(record,Qt::LeftButton);QTRY_VERIFY(record->text()=="Record audio"&&record->isEnabled());
        QCOMPARE(savedFiles(history).size(),before.size()+2);window.close();
    }
    void partialImageSurvivesStopAndClose_data() {
        QTest::addColumn<bool>("closeWindow");QTest::newRow("stop")<<false;QTest::newRow("close")<<true;
    }
    void partialImageSurvivesStopAndClose() {
        QFETCH(bool,closeWindow);MainWindow window;window.show();
        auto* history=window.findChild<QListWidget*>("receiveHistory");const auto before=savedFiles(history);
        window.findChild<QLineEdit*>("inputDevice")->setText("test-device");
        QTest::mouseClick(window.findChild<QPushButton*>("recordButton"),Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(savedFiles(history).size(),before.size()+1,2200);
        const auto path=*(savedFiles(history)-before).begin();QVERIFY(!QImage(path).isNull());
        if(closeWindow)window.close();else QTest::mouseClick(window.findChild<QPushButton*>("stopButton"),Qt::LeftButton);
        QTest::qWait(150);QVERIFY(QFile::exists(path));QCOMPARE(savedFiles(history).size(),before.size()+1);window.close();
    }
    void wavImagesAutoSave() {
        MainWindow window;window.show();
        auto* history=window.findChild<QListWidget*>("receiveHistory");const auto before=savedFiles(history).size();
        for(int attempt=1;attempt<=2;++attempt) {
            window.openReceiveWav(directory_.filePath("fixture.wav"));
            QTRY_COMPARE_WITH_TIMEOUT(savedFiles(history).size(),before+attempt,3000);
            QVERIFY(window.findChild<QLabel*>("receiveInfo")->text().contains("partial"));
        }
        window.close();
    }
    void autoSaveFailureAndRecovery() {
        MainWindow window;window.show();
        auto* record=window.findChild<QPushButton*>("recordButton");
        auto* history=window.findChild<QListWidget*>("receiveHistory");
        auto* saveInfo=window.findChild<QLabel*>("receiveAutoSaveInfo");
        const auto before=savedFiles(history).size();
        const auto path=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)+"/OpenSSTV";
        const auto backup=path+"-test-backup";QVERIFY(QDir().mkpath(path));QVERIFY(QDir().rename(path,backup));
        bool restored=false;
        const auto restore=qScopeGuard([&] {if(!restored){QFile::remove(path);QDir().rename(backup,path);}});
        QFile blocker(path);QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();
        window.findChild<QLineEdit*>("inputDevice")->setText("test-device");QTest::mouseClick(record,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(saveInfo->text().startsWith("Auto-save failed:"),2200);
        QCOMPARE(savedFiles(history).size(),before);
        QVERIFY(QFile::remove(path));QVERIFY(QDir().rename(backup,path));restored=true;
        QTest::mouseClick(record,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(saveInfo->text().startsWith("Saved automatically:"),3000);
        QCOMPARE(savedFiles(history).size(),before+1);QVERIFY(QFile::exists(saveInfo->toolTip()));window.close();
    }
    void morseSettingsAndPlayback() {
        QSettings settings;settings.setValue("tx/morseEnabled",false);settings.setValue("station/call","");
        MainWindow window;window.show();auto* action=window.findChild<QAction*>("settingsAction");bool edited=false;
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            auto* enabled=dialog->findChild<QCheckBox*>("settingsMorseEnabled");auto* speed=dialog->findChild<QSpinBox*>("settingsMorseWpm");auto* tone=dialog->findChild<QSpinBox*>("settingsMorseTone");
            auto* buttons=dialog->findChild<QDialogButtonBox*>();auto* call=dialog->findChild<QLineEdit*>("settingsCall");
            QVERIFY(enabled&&speed&&tone&&buttons&&call);QVERIFY(!enabled->isChecked());QVERIFY(!speed->isEnabled());
            enabled->setChecked(true);QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
            call->setText("N0CALL");speed->setValue(25);tone->setValue(1000);
            QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());QVERIFY(speed->isEnabled());edited=true;dialog->accept();
        });
        action->trigger();QVERIFY(edited);QVERIFY(settings.value("tx/morseEnabled").toBool());QCOMPARE(settings.value("tx/morseWpm").toInt(),25);QCOMPARE(settings.value("tx/morseTone").toInt(),1000);
        // Cancelling edits must retain the enabled ID and its settings.
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            dialog->findChild<QCheckBox*>("settingsMorseEnabled")->setChecked(false);dialog->reject();
        });action->trigger();QVERIFY(settings.value("tx/morseEnabled").toBool());
        auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("bw8"));
        window.findChild<QLineEdit*>("outputDevice")->setText("morse-test-device");
        const auto copy=directory_.filePath("morse-tx.wav");qputenv("OPENSSTV_TEST_TX_COPY",copy.toUtf8());
        QTest::mouseClick(window.findChild<QPushButton*>("playButton"),Qt::LeftButton);
        QTest::mouseClick(window.findChild<QPushButton*>("transmitFullScreenButton"),Qt::LeftButton);
        auto* fullWindow=window.findChild<QWidget*>("transmitFullScreenWindow");auto* progress=window.findChild<QProgressBar*>("transmitProgress");
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(copy),3000);
        const auto audio=sstv::readWav(copy);const auto* selected=sstv::modeById("bw8");
        const double expected=(selected->imageMs()+selected->headerMs()+200+sstv::morseIdDurationMs({"N0CALL",25,1000}))/1000;
        QVERIFY(std::abs(audio.seconds()-expected)<1./48000);QVERIFY(sstv::decode(audio).complete);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Sending Morse ID"),11000);
        QVERIFY(countdownSeconds(progress)>0);
        QVERIFY(fullWindow->isVisible());QVERIFY(progress->value()<100);
        QTRY_VERIFY_WITH_TIMEOUT(progress->format().contains("Playback complete"),6000);
        QCOMPARE(progress->value(),100);QVERIFY(!fullWindow->isVisible());window.close();
        QCOMPARE(countdownSeconds(progress),qint64(0));
        MainWindow reopened;reopened.show();bool restored=false;
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            QVERIFY(dialog->findChild<QCheckBox*>("settingsMorseEnabled")->isChecked());QCOMPARE(dialog->findChild<QSpinBox*>("settingsMorseWpm")->value(),25);QCOMPARE(dialog->findChild<QSpinBox*>("settingsMorseTone")->value(),1000);restored=true;dialog->reject();
        });reopened.findChild<QAction*>("settingsAction")->trigger();QVERIFY(restored);reopened.close();
        settings.setValue("tx/morseEnabled",false);settings.setValue("station/call","");
    }
    void skinPreviewSaveCancelAndRestore() {
        QSettings settings;settings.setValue("ui/skin","obsolete-skin");
        {MainWindow fallback;QCOMPARE(fallback.palette().color(QPalette::Window),ui::skinById("classic").background);fallback.close();}
        QCOMPARE(settings.value("ui/skin").toString(),QString("classic"));
        MainWindow window;window.show();bool previewed=false;
        const auto screenshots=qEnvironmentVariable("OPENSSTV_SKIN_PREVIEWS");
        if(!screenshots.isEmpty())QVERIFY(QDir().mkpath(screenshots));
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            bool finished=false;const auto close=qScopeGuard([&]{if(!finished)dialog->reject();});
            auto* choices=dialog->findChild<QComboBox*>("settingsSkin");QVERIFY(choices);QCOMPARE(choices->count(),int(ui::skins().size()));
            for(const auto& skin:ui::skins()) {
                const int index=choices->findData(skin.id);QVERIFY(index>=0);choices->setCurrentIndex(index);QApplication::processEvents();
                QCOMPARE(window.palette().color(QPalette::Window),skin.background);
                QCOMPARE(dialog->findChild<QLabel*>("skinDescription")->text(),skin.description);
                QCOMPARE(window.findChild<QWidget*>("radioFrontend")->palette().color(QPalette::Window),ui::radioSkin(skin).background);
                QCOMPARE(settings.value("ui/skin").toString(),QString("classic"));
                if(!screenshots.isEmpty()) {
                    QVERIFY(window.grab().save(screenshots+"/"+skin.id+".png"));
                    QVERIFY(dialog->grab().save(screenshots+"/settings-"+skin.id+".png"));
                    auto* tabs=window.findChild<QTabWidget*>("imageTabs");const int previous=tabs->currentIndex();tabs->setCurrentIndex(3);QApplication::processEvents();QVERIFY(window.grab().save(screenshots+"/radio-"+skin.id+".png"));window.findChild<QPushButton*>("radioFrontendAudio")->click();QApplication::processEvents();QVERIFY(window.grab().save(screenshots+"/audio-"+skin.id+".png"));window.findChild<QPushButton*>("audioLevelBack")->click();tabs->setCurrentIndex(previous);
                }
                if(skin.id=="midnight") {
                    auto* morse=dialog->findChild<QCheckBox*>("settingsMorseEnabled");const bool wasEnabled=morse->isChecked();morse->setChecked(true);QApplication::processEvents();
                    auto* speed=dialog->findChild<QSpinBox*>("settingsMorseWpm");const int original=speed->value();
                    QTest::mouseClick(speed,Qt::LeftButton,Qt::NoModifier,QPoint(speed->width()-9,5));QCOMPARE(speed->value(),original+1);speed->setValue(original);
                    if(!screenshots.isEmpty())QVERIFY(dialog->grab().save(screenshots+"/settings-midnight-checked.png"));morse->setChecked(wasEnabled);
                }
            }
            choices->setCurrentIndex(choices->findData("midnight"));previewed=true;finished=true;dialog->accept();
        });
        window.findChild<QAction*>("settingsAction")->trigger();QVERIFY(previewed);
        QCOMPARE(settings.value("ui/skin").toString(),QString("midnight"));const auto savedStyle=window.styleSheet();
        bool cancelled=false;
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            bool finished=false;const auto close=qScopeGuard([&]{if(!finished)dialog->reject();});
            auto* choices=dialog->findChild<QComboBox*>("settingsSkin");QVERIFY(choices);QCOMPARE(choices->currentData().toString(),QString("midnight"));
            choices->setCurrentIndex(choices->findData("ember"));QCOMPARE(window.palette().color(QPalette::Window),ui::skinById("ember").background);
            cancelled=true;finished=true;dialog->reject();
        });window.findChild<QAction*>("settingsAction")->trigger();QVERIFY(cancelled);
        QCOMPARE(window.styleSheet(),savedStyle);QCOMPARE(settings.value("ui/skin").toString(),QString("midnight"));window.close();
        {MainWindow restored;QCOMPARE(restored.palette().color(QPalette::Window),ui::skinById("midnight").background);restored.close();}
        settings.setValue("ui/skin","classic");
    }
    void textEmojiEditorAndTransmission() {
        QSettings settings;settings.setValue("ui/skin","midnight");settings.setValue("tx/caption/text","");settings.setValue("station/call","");settings.setValue("tx/morseEnabled",false);
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("martin2"));
        auto* compose=window.findChild<QPushButton*>("compositionButton");auto* imageView=window.findChild<QLabel*>("transmitImage");QVERIFY(compose&&imageView);
        const auto barePreview=imageView->grab().toImage();QString message;bool applied=false;
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            bool finished=false;const auto close=qScopeGuard([&]{if(!finished)dialog->reject();});
            auto* text=dialog->findChild<QPlainTextEdit*>("compositionText");auto* search=dialog->findChild<QLineEdit*>("emojiSearch");auto* grid=dialog->findChild<QListWidget*>("emojiGrid");
            QVERIFY(text&&search&&grid);text->setPlainText("73 from OpenSSTV ");text->moveCursor(QTextCursor::End);
            search->setText("radio");QApplication::processEvents();QVERIFY(grid->count()>0);
            QListWidgetItem* radio=nullptr;for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole).toString()=="📻")radio=grid->item(i);QVERIFY(radio);
            QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(radio).center());QVERIFY(text->toPlainText().endsWith("📻"));
            auto cursor=text->textCursor();cursor.insertText("\nHello ");text->setTextCursor(cursor);
            search->setText("hello");dialog->findChild<QComboBox*>("emojiSkinTone")->setCurrentIndex(3);QApplication::processEvents();QVERIFY(grid->count()>0);
            QListWidgetItem* wave=nullptr;for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole).toString()=="👋🏽")wave=grid->item(i);QVERIFY(wave);grid->setCurrentItem(wave);grid->setFocus();QApplication::processEvents();QTest::keyClick(grid,Qt::Key_Return);QVERIFY(text->toPlainText().endsWith("👋🏽"));
            cursor=text->textCursor();cursor.insertText(" ❤️ 👩‍🚀");text->setTextCursor(cursor);message=text->toPlainText();QVERIFY(message.contains("👋🏽"));
            search->clear();dialog->findChild<QComboBox*>("emojiCategory")->setCurrentIndex(1);QCOMPARE(grid->count(),2);
            search->setText("hello");QCOMPARE(grid->count(),1);search->clear();
            dialog->findChild<QCheckBox*>("compositionBacking")->setChecked(true);
            auto* apply=dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply);
            text->setPlainText(QString(501,'A'));QVERIFY(!apply->isEnabled());dialog->accept();QVERIFY(dialog->isVisible());
            text->setPlainText("a\nb\nc\nd\ne\nf\ng");QVERIFY(!apply->isEnabled());text->setPlainText(message);QVERIFY(apply->isEnabled());
            const auto snapshot=qEnvironmentVariable("OPENSSTV_COMPOSER_PREVIEW");if(!snapshot.isEmpty()){dialog->findChild<QComboBox*>("emojiCategory")->setCurrentIndex(0);QApplication::processEvents();QVERIFY(dialog->grab().save(snapshot));}
            finished=true;applied=true;dialog->accept();
        });QTest::mouseClick(compose,Qt::LeftButton);QVERIFY(applied);
        QVERIFY(!settings.contains("tx/caption/text"));QVERIFY(settings.value("tx/recentEmojis").toStringList().contains("👋🏽"));
        const auto composedPreview=imageView->grab().toImage();QVERIFY(composedPreview!=barePreview);
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("DO NOT SEND");dialog->reject();
        });QTest::mouseClick(compose,Qt::LeftButton);QCOMPARE(imageView->grab().toImage(),composedPreview);QVERIFY(!settings.contains("tx/caption/text"));
        const auto copy=directory_.filePath("caption.wav");qputenv("OPENSSTV_TEST_TX_COPY",copy.toUtf8());window.findChild<QLineEdit*>("outputDevice")->setText("capture-tx-device");
        QTest::mouseClick(window.findChild<QPushButton*>("playButton"),Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(copy),4000);
        const auto decoded=sstv::decode(sstv::readWav(copy));QVERIFY(decoded.complete);
        sstv::TextOverlay overlay;overlay.text=message;overlay.backing=true;const auto expected=sstv::composeTransmitImage(sstv::testPattern(),*sstv::modeById("martin2"),{},overlay).image;
        const auto bare=sstv::fitImage(sstv::testPattern(),*sstv::modeById("martin2"));double withText=0,withoutText=0;
        for(int y=128;y<256;++y)for(int x=0;x<320;++x) {
            const auto a=decoded.image.pixel(x,y),b=expected.pixel(x,y),c=bare.pixel(x,y);
            withText+=std::abs(qRed(a)-qRed(b))+std::abs(qGreen(a)-qGreen(b))+std::abs(qBlue(a)-qBlue(b));
            withoutText+=std::abs(qRed(a)-qRed(c))+std::abs(qGreen(a)-qGreen(c))+std::abs(qBlue(a)-qBlue(c));
        }
        QVERIFY(withText<withoutText*.7);QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton*>("playButton")->isEnabled(),4000);window.close();
        MainWindow reopened;reopened.show();bool restored=false;
        QTimer::singleShot(30,[&] {
            auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            bool finished=false;const auto close=qScopeGuard([&]{if(!finished)dialog->reject();});
            QVERIFY(dialog->findChild<QPlainTextEdit*>("compositionText")->toPlainText().isEmpty());
            QCOMPARE(dialog->findChild<QComboBox*>("emojiSkinTone")->currentIndex(),3);
            QTest::mouseClick(dialog->findChild<QPushButton*>("compositionClear"),Qt::LeftButton);QVERIFY(dialog->overlay().text.isEmpty());restored=true;finished=true;dialog->accept();
        });QTest::mouseClick(reopened.findChild<QPushButton*>("compositionButton"),Qt::LeftButton);QVERIFY(restored);QVERIFY(settings.value("tx/caption/text").toString().isEmpty());reopened.close();settings.setValue("ui/skin","classic");
    }
    void cleanupTestCase() {qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv) {
    QTemporaryDir environment;
    qputenv("XDG_CONFIG_HOME",environment.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME",environment.filePath("data").toUtf8());
    QDir().mkpath(environment.filePath("config"));
    QFile userDirs(environment.filePath("config/user-dirs.dirs"));
    if(!userDirs.open(QIODevice::WriteOnly))return 1;
    userDirs.write("XDG_PICTURES_DIR=\""+environment.filePath("pictures").toUtf8()+"\"\n");userDirs.close();
    QApplication app(argc,argv);UiTests test;return QTest::qExec(&test,argc,argv);
}
#include "ui_tests.moc"
