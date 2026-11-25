#include "transmit_monitor.h"
#include <QtTest>
#include <QtEndian>

class TransmitMonitorTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QString wave(const QString& name,int level){
        QImage image(160,120,QImage::Format_RGB32);image.fill(qRgb(level,level,level));
        const auto path=directory_.filePath(name);sstv::writeWav(path,sstv::encode(image,*sstv::modeById("bw8"),48000));return path;
    }
private slots:
    void buildsOnlyPlayedRowsThenFinishes(){
        const auto path=wave("paced.wav",192);TransmitMonitor monitor;sstv::DecodeResult last;int frames=0;
        connect(&monitor,&TransmitMonitor::picture,&monitor,[&](const sstv::DecodeResult& frame){last=frame;++frames;});QSignalSpy finished(&monitor,&TransmitMonitor::finished);
        monitor.start(path);QTest::qWait(200);QCOMPARE(frames,0);QTRY_VERIFY_WITH_TIMEOUT(frames>0,3000);QVERIFY(last.rows>0&&last.rows<120);QVERIFY(!last.complete);QCOMPARE(last.modeId,QString("bw8"));
        monitor.finish(true);QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,3000);QVERIFY(finished[0][0].toBool());QVERIFY(last.complete);QCOMPARE(last.rows,120);QCOMPARE(last.image.size(),QSize(160,120));QVERIFY(std::abs(qGray(last.image.pixel(80,60))-192)<8);QVERIFY(!monitor.active());
    }
    void stopKeepsPartialAndDiscardsLateResults(){
        const auto path=wave("stopped.wav",220);TransmitMonitor monitor;int frames=0;bool complete=false;
        connect(&monitor,&TransmitMonitor::picture,&monitor,[&](const sstv::DecodeResult& frame){++frames;complete=frame.complete;});QSignalSpy finished(&monitor,&TransmitMonitor::finished);
        monitor.start(path);QTRY_VERIFY_WITH_TIMEOUT(frames>0,3000);monitor.stop();const auto count=frames;QTest::qWait(400);QCOMPARE(frames,count);QVERIFY(!complete);QVERIFY(!monitor.active());QCOMPARE(finished.size(),1);QVERIFY(!finished[0][0].toBool());
    }
    void nextTransmissionStartsFreshEvenWithPendingWorker(){
        const auto light=wave("light.wav",230),dark=wave("dark.wav",32);TransmitMonitor monitor;sstv::DecodeResult last;int frames=0;
        connect(&monitor,&TransmitMonitor::picture,&monitor,[&](const sstv::DecodeResult& frame){last=frame;++frames;});
        monitor.start(light);QTRY_VERIFY_WITH_TIMEOUT(frames>0,3000);monitor.start(dark);frames=0;QTest::qWait(200);QCOMPARE(frames,0);monitor.finish(true);QTRY_VERIFY_WITH_TIMEOUT(!monitor.active(),3000);QVERIFY(last.complete);QCOMPARE(last.rows,120);QVERIFY(qGray(last.image.pixel(80,60))<45);
    }
    void silentWaveDoesNotPretendAnImageWasTransmitted(){
        sstv::Audio audio;audio.sampleRate=48000;audio.samples.resize(48000,0);const auto path=directory_.filePath("silent.wav");sstv::writeWav(path,audio);TransmitMonitor monitor;int frames=0;
        connect(&monitor,&TransmitMonitor::picture,&monitor,[&](const sstv::DecodeResult&){++frames;});QSignalSpy finished(&monitor,&TransmitMonitor::finished);monitor.start(path);monitor.finish(true);QTRY_COMPARE(finished.size(),1);QVERIFY(!finished[0][0].toBool());QCOMPARE(frames,0);QVERIFY(!monitor.active());
    }
    void streamedAudioBuildsAndDrainsWithoutUsingLocalAudio(){
        QImage image(160,120,QImage::Format_RGB32);image.fill(qRgb(180,180,180));const auto audio=sstv::encode(image,*sstv::modeById("bw8"),12000);QByteArray pcm(qsizetype(audio.samples.size()*2),0);for(size_t n=0;n<audio.samples.size();++n)qToLittleEndian<qint16>(qint16(audio.samples[n]*32767),pcm.data()+n*2);
        TransmitMonitor monitor;sstv::DecodeResult last;connect(&monitor,&TransmitMonitor::picture,&monitor,[&](const sstv::DecodeResult& frame){last=frame;});QSignalSpy finished(&monitor,&TransmitMonitor::finished);monitor.startStream();monitor.pushPcm(pcm.first(36000));QTRY_VERIFY(last.rows>0);QVERIFY(!last.complete);QVERIFY(last.rows<120);
        for(qsizetype offset=36000;offset<pcm.size();offset+=7001)monitor.pushPcm(pcm.mid(offset,7001));monitor.finish(true);QTRY_COMPARE(finished.size(),1);QVERIFY(finished[0][0].toBool());QVERIFY(last.complete);QCOMPARE(last.rows,120);QVERIFY(std::abs(qGray(last.image.pixel(80,60))-180)<8);
    }
};
QTEST_GUILESS_MAIN(TransmitMonitorTests)
#include "transmit_monitor_tests.moc"
