#include "core/morse_receiver.h"
#include "core/morse.h"
#include "core/receiver.h"
#include "fsk_fixture.h"
#include <QtTest>
#include <QRandomGenerator>
#include <cmath>
#include <numbers>

namespace {
// Independent sample-time fixture: no transmitter symbol/timing functions.
sstv::Audio fixture(const QString& text,double wpm=20,double hz=800,double amplitude=.3,double noise=0,bool jitter=false){
    const QString alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/-.,?=+@";
    const QStringList codes={".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--..","-----",".----","..---","...--","....-",".....","-....","--...","---..","----.","-..-.","-....-",".-.-.-","--..--","..--..","-...-",".-.-.",".--.-."};
    sstv::Audio result;double phase=.31;QRandomGenerator random(4193);const double dot=1.2/wpm;
    const auto segment=[&](bool keyed,double units){
        const double seconds=units*dot*(jitter?.92+.16*random.generateDouble():1.);
        const int count=int(std::lround(seconds*12000));
        for(int n=0;n<count;++n){phase+=2*std::numbers::pi*hz/12000;phase=std::fmod(phase,2*std::numbers::pi);const double edge=std::min({1.,n/24.,(count-1-n)/24.});result.samples.push_back(float((keyed?amplitude*edge*std::sin(phase):0)+noise*(random.generateDouble()-.5)));}
    };
    segment(false,8);
    bool previous=false;
    for(auto character:text){
        if(character==' '){segment(false,4);continue;}
        const int index=alphabet.indexOf(character);if(index<0)qFatal("Unknown fixture character");
        if(previous)segment(false,3);
        const auto code=codes[index];for(int i=0;i<code.size();++i){if(i)segment(false,1);segment(true,code[i]=='-'?3:1);}previous=true;
    }
    segment(false,9);return result;
}
void feed(sstv::MorseReceiver& receiver,const sstv::Audio& audio){
    for(size_t pos=0;pos<audio.samples.size();pos+=4213)receiver.push(std::span(audio.samples).subspan(pos,std::min(size_t(4213),audio.samples.size()-pos)));
}
}
class MorseReceiveTests:public QObject{
    Q_OBJECT
private slots:
    void automaticToneAndSpeed_data(){
        QTest::addColumn<double>("wpm");QTest::addColumn<double>("hz");
        for(double speed:{5.,12.,20.,30.,40.})for(double tone:{400.,425.,800.,1900.,2475.,2500.})QTest::newRow(qPrintable(QString("%1wpm-%2hz").arg(speed).arg(tone)))<<speed<<tone;
    }
    void automaticToneAndSpeed(){
        QFETCH(double,wpm);QFETCH(double,hz);sstv::MorseReceiver receiver;feed(receiver,fixture("CQ DE KI5BZO W1AW/P",wpm,hz));
        const auto reading=receiver.reading();QCOMPARE(reading.text,QString("CQ DE KI5BZO W1AW/P"));QCOMPARE(reading.candidate,QString("W1AW/P"));
        QVERIFY(std::abs(reading.wpm-wpm)<wpm*.15);QVERIFY(std::abs(reading.toneHz-hz)<5);
    }
    void weakNoisyUnevenKeying(){
        for(double amplitude:{.004,.04,.5}){sstv::MorseReceiver receiver;feed(receiver,fixture("KI5BZO",18,723,amplitude,amplitude*.6,true));QCOMPARE(receiver.reading().candidate,QString("KI5BZO"));}
    }
    void transmitterCompatibility(){
        for(const auto& call:{"KI5BZO","K9TEST/P","W1AW","N0CALL"}){sstv::Audio audio;sstv::appendMorseId(audio,{call,20,800});sstv::MorseReceiver receiver;feed(receiver,audio);receiver.finish();QCOMPARE(receiver.reading().candidate,QString(call));}
    }
    void alphabetAndPunctuation(){sstv::MorseReceiver receiver;const QString text="ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 /-.,?=+@";feed(receiver,fixture(text));QCOMPARE(receiver.reading().text,text);QVERIFY(receiver.reading().candidate.isEmpty());}
    void ignoresNoiseSilenceAndUnkeyedTone(){
        for(int kind=0;kind<3;++kind){sstv::Audio audio;audio.samples.resize(12000*5);QRandomGenerator random(782);
            for(size_t n=0;n<audio.samples.size();++n)audio.samples[n]=kind==0?0:kind==1?float((random.generateDouble()-.5)*.4):float(.3*std::sin(2*std::numbers::pi*800*n/12000));
            sstv::MorseReceiver receiver;feed(receiver,audio);QVERIFY(receiver.reading().candidate.isEmpty());QVERIFY(receiver.reading().text.isEmpty());
        }
    }
    void pictureBoundaryAndSuccessiveImages_data(){QTest::addColumn<double>("hz");for(double hz:{800.,1500.,1900.})QTest::newRow(qPrintable(QString::number(hz)))<<hz;}
    void pictureBoundaryAndSuccessiveImages(){
        QFETCH(double,hz);sstv::Audio stream;for(const auto& mode:{"bw8","robot36"}){const auto picture=sstv::encode(sstv::testPattern(),*sstv::modeById(mode),12000);stream.samples.insert(stream.samples.end(),picture.samples.begin(),picture.samples.end());const auto cw=fixture(mode==QString("bw8")?"KI5BZO":"W1AW",20,hz);stream.samples.insert(stream.samples.end(),cw.samples.begin(),cw.samples.end());}
        sstv::Receiver receiver;QStringList images;
        for(size_t pos=0;pos<stream.samples.size();pos+=4213){sstv::Audio block;block.samples.assign(stream.samples.begin()+pos,stream.samples.begin()+std::min(pos+4213,stream.samples.size()));for(const auto& frame:receiver.push(block))if(frame.complete&&!frame.metadataOnly){images<<frame.modeId;QVERIFY(frame.callsign.isEmpty());}}
        QCOMPARE(images,QStringList({"bw8","robot36"}));QVERIFY2(receiver.morse().text.contains("KI5BZO"),qPrintable(receiver.morse().text));QCOMPARE(receiver.morse().candidate,QString("W1AW"));
    }
    void fskIsNotMorse(){sstv::MorseReceiver receiver;auto audio=fskFixture("K9TEST");audio.samples.insert(audio.samples.end(),24000,0.f);feed(receiver,audio);QVERIFY(receiver.reading().candidate.isEmpty());}
    void resetAndBoundedHistory(){
        sstv::MorseReceiver receiver;auto partial=fixture("KI5BZO");partial.samples.resize(12000);feed(receiver,partial);receiver.suppress();feed(receiver,fixture("W1AW"));QCOMPARE(receiver.reading().text,QString("W1AW"));
        QString many;for(int i=0;i<40;++i)many+=" W1AW";feed(receiver,fixture(many.trimmed(),40));QVERIFY(receiver.reading().text.size()<=160);QCOMPARE(receiver.reading().candidate,QString("W1AW"));
    }
    void rejectsBadInputAndCancellation(){
        sstv::MorseReceiver receiver;const std::array<float,1> invalid={std::numeric_limits<float>::quiet_NaN()};QVERIFY_EXCEPTION_THROWN(receiver.push(invalid),std::runtime_error);
        const auto cancel=std::make_shared<std::atomic_bool>(true);QVERIFY_EXCEPTION_THROWN(receiver.push(fixture("KI5BZO").samples,cancel),std::runtime_error);
    }
};
QTEST_GUILESS_MAIN(MorseReceiveTests)
#include "morse_receive_tests.moc"
