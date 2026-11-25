#include "core/pcm_converter.h"
#include "core/codec.h"
#include "audio_process.h"
#include <QtTest>
#include <QtEndian>
#include <cmath>
#include <numbers>

namespace {
QByteArray tone(int rate,int channels,int frames,double hz){
    QByteArray bytes(frames*channels*2,Qt::Uninitialized);
    for(int n=0;n<frames;++n)for(int c=0;c<channels;++c)qToLittleEndian<qint16>(qint16(std::lround(14000*std::sin(2*std::numbers::pi*hz*n/rate))),bytes.data()+(n*channels+c)*2);
    return bytes;
}
double rms(const QByteArray& data,int trim=100){double sum=0;int count=0;for(int n=trim;n<data.size()/2-trim;++n){const double value=qFromLittleEndian<qint16>(data.constData()+n*2)/32768.;sum+=value*value;++count;}return count?std::sqrt(sum/count):0;}
}
class PcmConverterTests:public QObject{
    Q_OBJECT
private slots:
    void formats_data(){
        QTest::addColumn<int>("inRate");QTest::addColumn<int>("inChannels");QTest::addColumn<int>("outRate");QTest::addColumn<int>("outChannels");
        for(int rate:{12000,44100,48000,96000})for(int channels:{1,2})for(int target:{12000,48000})QTest::newRow(qPrintable(QString("%1-%2-to-%3").arg(rate).arg(channels).arg(target)))<<rate<<channels<<target<<(target==48000?2:1);
    }
    void formats(){
        QFETCH(int,inRate);QFETCH(int,inChannels);QFETCH(int,outRate);QFETCH(int,outChannels);
        const auto input=tone(inRate,inChannels,inRate,1900);audio::PcmConverter one(inRate,inChannels,outRate,outChannels),chunked(inRate,inChannels,outRate,outChannels);
        auto expected=one.push(input);expected+=one.finish();QByteArray actual;
        for(qsizetype pos=0;pos<input.size();pos+=4093)actual+=chunked.push(input.mid(pos,4093));actual+=chunked.finish();
        QCOMPARE(actual,expected);QCOMPARE(actual.size(),outRate*outChannels*2);QVERIFY(std::abs(rms(actual)-14000/32768./std::sqrt(2.))<.006);
        if(outChannels==2)for(qsizetype n=0;n<actual.size();n+=4)QCOMPARE(actual.mid(n,2),actual.mid(n+2,2));
    }
    void rejectsAliasing(){
        audio::PcmConverter converter(48000,1,12000);auto output=converter.push(tone(48000,1,48000,10000));output+=converter.finish();QVERIFY2(rms(output)<.003,qPrintable(QString::number(rms(output))));
    }
    void preservesSstvHeaderAndImage_data(){QTest::addColumn<int>("rate");QTest::newRow("44100 Hz USB")<<44100;QTest::newRow("48000 Hz USB")<<48000;}
    void preservesSstvHeaderAndImage(){
        QFETCH(int,rate);QImage image(160,120,QImage::Format_RGB32);image.fill(Qt::white);for(int y=0;y<120;++y)for(int x=0;x<80;++x)image.setPixel(x,y,qRgb(0,0,0));
        const auto encoded=sstv::encode(image,*sstv::modeById("bw8"),rate);QByteArray input(qsizetype(encoded.samples.size()*2),Qt::Uninitialized);
        for(size_t n=0;n<encoded.samples.size();++n)qToLittleEndian<qint16>(qint16(std::lround(encoded.samples[n]*32767)),input.data()+n*2);
        audio::PcmConverter converter(rate,1,12000);QByteArray output;for(qsizetype pos=0;pos<input.size();pos+=4093)output+=converter.push(input.mid(pos,4093));output+=converter.finish();
        sstv::Audio receive;receive.samples.resize(size_t(output.size()/2));for(size_t n=0;n<receive.samples.size();++n)receive.samples[n]=qFromLittleEndian<qint16>(output.constData()+n*2)/32768.f;
        const auto decoded=sstv::decode(receive);QVERIFY(decoded.complete);QCOMPARE(decoded.modeId,QString("bw8"));QVERIFY(qGray(decoded.image.pixel(20,60))<10);QVERIFY(qGray(decoded.image.pixel(140,60))>245);
    }
    void stereoDownmix(){
        auto input=tone(48000,2,48000,800);for(qsizetype n=0;n<input.size();n+=4)qToLittleEndian<qint16>(-qFromLittleEndian<qint16>(input.constData()+n),input.data()+n+2);
        audio::PcmConverter converter(48000,2,12000);auto output=converter.push(input);output+=converter.finish();QCOMPARE(rms(output),0.);
    }
    void fractionalDuration(){
        audio::PcmConverter converter(44100,1,12000);const int frames=44101;auto output=converter.push(tone(44100,1,frames,800));output+=converter.finish();QCOMPARE(output.size()/2,(qint64(frames)*12000+44099)/44100);QVERIFY(converter.finish().isEmpty());
    }
    void rejectsInvalidFrames(){
        QVERIFY_EXCEPTION_THROWN(audio::PcmConverter(0,1,12000),std::runtime_error);QVERIFY_EXCEPTION_THROWN(audio::PcmConverter(48000,0,12000),std::runtime_error);
        audio::PcmConverter converter(48000,2,12000);converter.push(QByteArray(3,0));QVERIFY_EXCEPTION_THROWN(converter.finish(),std::runtime_error);
        audio::PcmConverter done(12000,1,12000);done.finish();QVERIFY_EXCEPTION_THROWN(done.push(QByteArray(2,0)),std::runtime_error);
    }

};
QTEST_GUILESS_MAIN(PcmConverterTests)
#include "pcm_converter_tests.moc"
