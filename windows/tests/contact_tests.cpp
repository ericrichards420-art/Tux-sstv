#include "contactlog.h"
#include "contactlog_ui.h"
#include <QtTest>
#include <QtWidgets>
#include <QLockFile>

class ContactTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    sstv::Contact contact() const {sstv::Contact c;c.call="K9TEST";c.mode="martin2";c.frequencyHz=14230000;c.utc=QDateTime::fromString("2026-11-20T20:15:30Z",Qt::ISODate);c.sentReport="595";c.receivedReport="575";c.notes="Hello, \"operator\"\n73 📻";return c;}
private slots:
    void saveReloadPicturesAndCsv(){
        sstv::ContactLog log(directory_.filePath("normal"));QString error;QImage image(100,80,QImage::Format_RGB32);image.fill(Qt::red);
        QVERIFY2(log.append(contact(),image,image,error),qPrintable(error));QList<sstv::Contact> records;QVERIFY(log.load(records,error));QCOMPARE(records.size(),1);const auto saved=records[0];QCOMPARE(saved.utc,contact().utc);QCOMPARE(saved.notes,contact().notes);QCOMPARE(saved.frequencyHz,14230000.);QCOMPARE(QImage(saved.rxImage),image);QCOMPARE(QImage(saved.txImage),image);
        sstv::ContactLog another(directory_.filePath("normal"));auto next=contact();next.call="W1AW";QVERIFY(another.append(next,{},{},error));QVERIFY(log.load(records,error));QCOMPARE(records.size(),2);
        const auto csv=directory_.filePath("contacts.csv");QVERIFY(log.exportCsv(csv,error));QFile file(csv);QVERIFY(file.open(QIODevice::ReadOnly));const auto data=file.readAll();QVERIFY(data.contains("14.230000"));QVERIFY(data.contains("\"Hello, \"\"operator\"\"\n73"));
    }
    void invalidAndCorruptArePreserved(){
        sstv::ContactLog log(directory_.filePath("invalid"));QString error;auto c=contact();c.call="BAD";QVERIFY(!log.append(c,{},{},error));QVERIFY(!QFile::exists(log.directory()+"/contacts.json"));
        QDir().mkpath(log.directory());QFile file(log.directory()+"/contacts.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write("damaged original");file.close();QVERIFY(!log.append(contact(),{},{},error));QVERIFY(error.contains("preserved"));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("damaged original"));
    }
    void lockPreventsOverwrite(){
        sstv::ContactLog log(directory_.filePath("locked"));QDir().mkpath(log.directory());QLockFile lock(log.directory()+"/contacts.lock");QVERIFY(lock.tryLock(0));QString error;QVERIFY(!log.append(contact(),{},{},error));QVERIFY(error.contains("another"));lock.unlock();QVERIFY(log.append(contact(),{},{},error));
    }
    void editorValidationCancelAndViewer(){
        sstv::ContactLog log(directory_.filePath("editor"));auto seed=contact();seed.call.clear();
        {
            ContactEditor editor(log,seed,{},{});editor.show();auto* buttons=editor.findChild<QDialogButtonBox*>();QTest::mouseClick(buttons->button(QDialogButtonBox::Save),Qt::LeftButton);QVERIFY(editor.isVisible());QVERIFY(!editor.findChild<QLabel*>("contactError")->text().isEmpty());
            editor.findChild<QLineEdit*>("contactCall")->setText("K9TEST");editor.reject();QVERIFY(!QFile::exists(log.directory()+"/contacts.json"));
        }
        {
            ContactEditor editor(log,contact(),{},{});editor.show();QVERIFY(editor.findChild<QLineEdit*>("contactSentReport")->text().isEmpty());QVERIFY(editor.findChild<QLineEdit*>("contactReceivedReport")->text().isEmpty());QTest::mouseClick(editor.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);QCOMPARE(editor.result(),int(QDialog::Accepted));
        }
        ContactLogDialog viewer(log);auto* table=viewer.findChild<QTableWidget*>("contactTable");QCOMPARE(table->rowCount(),1);QCOMPARE(table->item(0,1)->text(),QString("K9TEST"));QCOMPARE(table->item(0,2)->text(),QString("14.230000"));
    }
};
QTEST_MAIN(ContactTests)
#include "contact_tests.moc"
