#include "core/fskid.h"
#include "core/receiver.h"
#include "fsk_fixture.h"
#include "mainwindow.h"
#include <QtTest>
#include <QtWidgets>

class FskTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
private slots:
    void initTestCase(){
        QCoreApplication::setOrganizationName("OpenSSTVFskTests");QCoreApplication::setApplicationName("OpenSSTVFskTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("config"));
    }
    void waveform_data(){QTest::addColumn<double>("offset");QTest::addColumn<double>("ppm");QTest::addColumn<bool>("narrow");QTest::addColumn<bool>("idle");QTest::newRow("standard")<<0.<<0.<<false<<false;QTest::newRow("offset-clock")<<125.<<4000.<<false<<true;QTest::newRow("narrow")<<-85.<<-2500.<<true<<false;}
    void waveform(){QFETCH(double,offset);QFETCH(double,ppm);QFETCH(bool,narrow);QFETCH(bool,idle);const auto f=sstv::demodulate(fskFixture("K9TEST/P",false,offset,ppm,narrow,idle));const auto id=sstv::detectFskId(f,0,offset,ppm);QVERIFY(id);QCOMPARE(id->callsign,QString("K9TEST/P"));}
    void rejectsDamaged(){
        QVERIFY(!sstv::detectFskId(sstv::demodulate(fskFixture("K9TEST",true))));
        QVERIFY(!sstv::detectFskId(sstv::demodulate(fskFixture("NOTACALL"))));
        auto partial=fskFixture();partial.samples.resize(partial.samples.size()-1500);QVERIFY(!sstv::detectFskId(sstv::demodulate(partial)));
        std::vector<float> flat(12000*5,1900);QVERIFY(!sstv::detectFskId(flat));
    }
    void trailerAcrossBlocksAndConsecutiveImages(){
        auto image=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);const auto id=fskFixture();
        sstv::Audio first=image;first.samples.insert(first.samples.end(),id.samples.begin(),id.samples.begin()+7200);
        sstv::Receiver receiver;const auto frames=receiver.push(first);QCOMPARE(frames.size(),size_t(1));QVERIFY(frames[0].complete);QVERIFY(frames[0].callsign.isEmpty());
        sstv::Audio next;next.sampleRate=12000;next.samples.assign(id.samples.begin()+7200,id.samples.end());next.samples.insert(next.samples.end(),image.samples.begin(),image.samples.end());
        const auto secondId=fskFixture("W1AW");next.samples.insert(next.samples.end(),secondId.samples.begin(),secondId.samples.end());
        const auto results=receiver.push(next);QCOMPARE(results.size(),size_t(2));QVERIFY(results[0].metadataOnly);QCOMPARE(results[0].frameId,frames[0].frameId);QCOMPARE(results[0].callsign,QString("K9TEST"));
        QVERIFY(results[1].complete);QCOMPARE(results[1].callsign,QString("W1AW"));QVERIFY(results[1].frameId!=frames[0].frameId);
    }
    void receivedCallAndReply(){
        auto image=sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000);const auto id=fskFixture();image.samples.insert(image.samples.end(),id.samples.begin(),id.samples.end());
        const auto file=directory_.filePath("received.wav");sstv::writeWav(file,image);
        MainWindow window;window.show();window.openReceiveWav(file);auto* call=window.findChild<QLabel*>("receivedCallsign");auto* reply=window.findChild<QPushButton*>("replyReceived");
        QTRY_VERIFY_WITH_TIMEOUT(reply->isEnabled(),5000);QVERIFY(call->text().contains("K9TEST"));
        const auto saved=window.findChild<QLabel*>("receiveAutoSaveInfo")->toolTip();QVERIFY(!saved.isEmpty());QCOMPARE(QImage(saved).text("Callsign"),QString("K9TEST"));
        QTest::mouseClick(reply,Qt::LeftButton);bool inspected=false;
        QTimer::singleShot(20,&window,[&]{auto* to=window.findChild<QLineEdit*>("templateToCall");if(!to)return;inspected=true;QCOMPARE(to->text(),QString("K9TEST"));QVERIFY(window.findChild<QLineEdit*>("templateCity")->text().isEmpty());QVERIFY(window.findChild<QLineEdit*>("templateDate")->text().isEmpty());qobject_cast<QDialog*>(to->window())->reject();});
        auto* templates=window.findChild<QPushButton*>("templatesButton");QVERIFY(templates);QTest::mouseClick(templates,Qt::LeftButton);QVERIFY(inspected);
        const auto blank=directory_.filePath("no-id.wav");sstv::writeWav(blank,sstv::encode(sstv::testPattern(),*sstv::modeById("bw8"),12000));window.openReceiveWav(blank);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton*>("decodeFileButton")->isEnabled(),5000);QVERIFY(!reply->isEnabled());QVERIFY(!call->text().contains("K9TEST"));window.close();
    }
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);FskTests tests;return QTest::qExec(&tests,argc,argv);}
#include "fsk_tests.moc"
