#include "audio_levels_panel.h"
#include "mainwindow.h"
#include "core/codec.h"
#include "core/morse.h"
#include <QtTest>
#include <QtWidgets>
#include <QtEndian>
#include <cmath>
#include <limits>

namespace {
QByteArray pcm(std::initializer_list<qint16> values) {
    QByteArray bytes;for(auto value:values){char sample[2];qToLittleEndian(value,sample);bytes.append(sample,2);}return bytes;
}
QByteArray pcm(const sstv::Audio& audio) {
    QByteArray bytes(qsizetype(audio.samples.size()*2),Qt::Uninitialized);
    for(size_t i=0;i<audio.samples.size();++i)qToLittleEndian<qint16>(qint16(std::lround(audio.samples[i]*32767)),bytes.data()+i*2);
    return bytes;
}
}
class AudioLevelsTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    template<class T> static T* child(QObject& parent,const char* name){return parent.findChild<T*>(name);}
    static void click(QObject& parent,const char* name){child<QPushButton>(parent,name)->click();}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVAudioLevelsTests");QCoreApplication::setApplicationName("AudioLevels");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_LEVELS_TEST",directory_.path().toUtf8());
        const QByteArray script=R"(#!/usr/bin/python3
import sys,os,time,struct,wave,json
root=os.environ['OPENSSTV_LEVELS_TEST']
device=sys.argv[sys.argv.index('-D')+1]
if device=='broken':
    print('Selected device unavailable',file=sys.stderr)
    sys.exit(2)
if os.path.basename(sys.argv[0])=='arecord':
    samples=struct.pack('<h',32767 if device=='clipped' else 4096)*1024
    while True:
        os.write(1,samples[:3]);time.sleep(.005);os.write(1,samples[3:]);time.sleep(.05)
else:
    with wave.open(sys.argv[-1],'rb') as f:
        frames=f.readframes(f.getnframes());samples=struct.unpack('<'+'h'*(len(frames)//2),frames)
        result={'device':device,'peak':max(abs(x) for x in samples),'seconds':len(samples)/f.getframerate()}
    with open(os.path.join(root,'played.json'),'w') as f: json.dump(result,f)
    time.sleep(2)
)";
        for(const auto& name:{"arecord","aplay"}){QFile file(directory_.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(script);file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
    }
    void init(){QSettings().clear();QSettings().setValue("audio/autoRxLevel",false);QFile::remove(directory_.filePath("played.json"));}
    void unityAndSplitSamples(){
        const auto original=pcm({-32768,-32760,-10001,-1,0,1,10001,32760,32767});sstv::PcmInputGain gain;QByteArray restored;
        for(qsizetype i=0;i<original.size();i+=3)restored+=gain.push(original.mid(i,3),0).pcm;
        QCOMPARE(restored,original);gain.push(QByteArray(1,'x'),0);gain.reset();QCOMPARE(gain.push(pcm({1234}),0).pcm,pcm({1234}));
        sstv::Audio audio;audio.samples={-.7f,0,.7f};const auto before=audio.samples;sstv::setAudioOutputLevel(audio,100);QCOMPARE(audio.samples,before);
        QVERIFY_EXCEPTION_THROWN(sstv::setAudioOutputLevel(audio,101),std::runtime_error);QVERIFY_EXCEPTION_THROWN(gain.push({},25),std::runtime_error);
    }
    void gainHeadroomAndUpstreamClipping(){
        sstv::PcmInputGain gain;const auto boosted=gain.push(pcm({-4096,4096}),6);QVERIFY(std::abs(boosted.input.peak-.125)<1e-6);QVERIFY(std::abs(sstv::audioDb(boosted.adjusted.peak)-sstv::audioDb(.125)-6)<.003);QVERIFY(!boosted.adjusted.clipped);
        const auto over=gain.push(pcm({20000,-20000}),12);QVERIFY(!over.input.clipped);QVERIFY(over.adjusted.clipped);QCOMPARE(over.pcm,pcm({32767,-32768}));
        const auto upstream=gain.push(pcm({32767,-32768}),-24);QVERIFY(upstream.input.clipped);QVERIFY(upstream.adjusted.clipped);QVERIFY(upstream.adjusted.peak<.064);
        sstv::Audio audio;audio.sampleRate=12000;audio.samples.assign(2500,.5);auto envelope=sstv::audioEnvelope(audio);QCOMPARE(envelope.size(),size_t(3));QCOMPARE(envelope.back().samples,size_t(100));QCOMPARE(envelope.front().peak,.5);QCOMPARE(envelope.front().rms,.5);
        audio.samples={std::numeric_limits<float>::quiet_NaN()};QVERIFY_EXCEPTION_THROWN(sstv::audioEnvelope(audio),std::runtime_error);
    }
    void automaticGainProtectsCurrentBlockAndNeverRaisesSilence(){
        sstv::PcmInputGain gain;
        const auto strong=gain.push(pcm({20000,-20000}),10,true);
        QVERIFY(strong.gainReduced);QVERIFY(strong.gainDb<0);QVERIFY(!strong.adjusted.clipped);QVERIFY(sstv::audioDb(strong.adjusted.peak)<=-6.);
        const auto quiet=gain.push(pcm({0,0,10,-10}),strong.gainDb,true);QVERIFY(!quiet.gainReduced);QCOMPARE(quiet.gainDb,strong.gainDb);
        const auto clipped=gain.push(pcm({32767,-32768}),10,true);QVERIFY(clipped.input.clipped);QVERIFY(clipped.adjusted.clipped);QVERIFY(clipped.adjusted.peak<.502);QCOMPARE(clipped.gainDb,-6);
        gain.reset();const auto bytes=pcm({20000,-20000});QCOMPARE(gain.push(bytes.left(1),10,true).pcm.size(),0);QCOMPARE(gain.push(bytes.mid(1),10,true).pcm,strong.pcm);
        QSettings().setValue("audio/autoRxLevel",true);AudioLevelsPanel panel([]{return true;});panel.setTransmitPercent(100);panel.receiveLevels(strong);QCOMPARE(panel.receiveGainDb(),strong.gainDb);QCOMPARE(QSettings().value("audio/rxGainDb").toInt(),strong.gainDb);QCOMPARE(panel.transmitPercent(),100);
    }
    void captureVolumeReductionTargetsOnlySelectedInput(){
        const auto path=directory_.filePath("amixer");QFile script(path);QVERIFY(script.open(QIODevice::WriteOnly));script.write(R"(#!/usr/bin/python3
import sys,os
root=os.environ['OPENSSTV_LEVELS_TEST']
args=sys.argv[1:]
with open(root+'/mixer-commands','a') as f: f.write(' '.join(args)+'\n')
if args[:3]!=['-c','RX','--']: sys.exit(2)
volume=root+'/capture-volume'
value=int(open(volume).read()) if os.path.exists(volume) else 60
if args[3]=='info': print('Card RX USB input')
elif args[3]=='contents':
    print("numid=1,iface=MIXER,name='PCM Playback Volume'\n  ; type=INTEGER,access=rw------,values=1,min=0,max=100,step=0\n  : values=100")
    print("numid=2,iface=MIXER,name='Capture Volume'\n  ; type=INTEGER,access=rw------,values=1,min=0,max=100,step=0\n  : values="+str(value))
elif args[3]=='cset' and args[4]=='numid=2':
    open(volume,'w').write(args[5]);print(': values='+args[5])
else: sys.exit(3)
)");script.close();QVERIFY(script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QFile::remove(directory_.filePath("mixer-commands"));QFile::remove(directory_.filePath("capture-volume"));
        const auto unsupported=audio::reduceCapture("default");QVERIFY(!unsupported.changed);QVERIFY(!QFile::exists(directory_.filePath("mixer-commands")));
        const auto result=audio::reduceCapture("plughw:CARD=RX,DEV=0");QVERIFY2(result.changed,qPrintable(result.message));QFile volume(directory_.filePath("capture-volume"));QVERIFY(volume.open(QIODevice::ReadOnly));QCOMPARE(volume.readAll(),QByteArray("50"));volume.close();
        QSettings().setValue("audio/autoRxLevel",true);AudioLevelsPanel panel([]{return true;});panel.setDevices("plughw:CARD=RX,DEV=0","plughw:CARD=TX,DEV=0");panel.setTransmitPercent(100);
        sstv::PcmInputGain gain;const auto clipped=gain.push(QByteArray(4800,char(0x7f)),0,true);auto sourceClipped=clipped;sourceClipped.input.clipped=true;panel.receiveLevels(sourceClipped);
        QTRY_VERIFY_WITH_TIMEOUT(([&]{QFile file(directory_.filePath("capture-volume"));return file.open(QIODevice::ReadOnly)&&file.readAll()=="40";})(),3000);
        QCOMPARE(panel.transmitPercent(),100);panel.shutdown();QFile log(directory_.filePath("mixer-commands"));QVERIFY(log.open(QIODevice::ReadOnly));const auto commands=log.readAll();QVERIFY(!commands.contains("numid=1"));QVERIFY(!commands.contains("-c TX"));
        QVERIFY(volume.open(QIODevice::WriteOnly|QIODevice::Truncate));volume.write("0");volume.close();QVERIFY(!audio::reduceCapture("hw:RX,0").changed);
    }
    void waveformRoundtrip_data(){QTest::addColumn<QString>("mode");for(const auto* id:{"bw8","robot36","martin2"})QTest::newRow(id)<<QString(id);}

    void waveformRoundtrip(){
        QFETCH(QString,mode);const auto image=sstv::testPattern();
        auto audio=sstv::encode(image,*sstv::modeById(mode),12000);const auto expected=sstv::decode(audio);const auto count=audio.samples.size();const auto peak=sstv::measureAudio(audio.samples).peak;
        sstv::setAudioOutputLevel(audio,25);QCOMPARE(audio.samples.size(),count);QVERIFY(std::abs(sstv::measureAudio(audio.samples).peak-peak*.25)<1e-7);
        const auto bytes=pcm(audio);QByteArray amplified;sstv::PcmInputGain gain;
        for(qsizetype pos=0;pos<bytes.size();pos+=10001)amplified+=gain.push(bytes.mid(pos,10001),12).pcm;
        QCOMPARE(amplified.size(),bytes.size());for(size_t i=0;i<audio.samples.size();++i)audio.samples[i]=qFromLittleEndian<qint16>(amplified.constData()+i*2)/32768.f;
        const auto decoded=sstv::decode(audio);QVERIFY(decoded.complete);QCOMPARE(decoded.modeId,expected.modeId);
        {QCOMPARE(decoded.image.size(),expected.image.size());double error=0;for(int y=0;y<decoded.image.height();++y)for(int x=0;x<decoded.image.width();++x)error+=std::abs(qGray(decoded.image.pixel(x,y))-qGray(expected.image.pixel(x,y)));error/=decoded.image.width()*decoded.image.height();QVERIFY2(error<1.,qPrintable(QString("Legacy mean pixel change after gain/PCM: %1").arg(error)));}
    }
    void inputMeterGainPersistenceAndCleanup(){
        AudioLevelsPanel panel([]{return true;});panel.show();panel.setDevices("microphone","speaker");QVERIFY(!panel.checking());click(panel,"audioLevelListen");auto* meter=child<QWidget>(panel,"receiveAudioMeter");QTRY_VERIFY(meter->property("hasReading").toBool());QVERIFY(std::abs(meter->property("peakDb").toDouble()+18.0618)<.01);
        child<QSpinBox>(panel,"receiveAudioGain")->setValue(6);QTRY_VERIFY(std::abs(meter->property("peakDb").toDouble()+12.0618)<.02);QCOMPARE(QSettings().value("audio/rxGainDb").toInt(),6);
        panel.setDevices("clipped","speaker");QVERIFY(!panel.checking());child<QSpinBox>(panel,"receiveAudioGain")->setValue(-24);click(panel,"audioLevelListen");QTRY_VERIFY(meter->property("clipping").toBool());QVERIFY(child<QLabel>(panel,"receiveAudioStatus")->text().contains("before gain"));
        panel.setApplicationState(false,true,false,false);QVERIFY(!panel.checking());QVERIFY(!child<QPushButton>(panel,"audioLevelListen")->isEnabled());QVERIFY(!child<QSpinBox>(panel,"transmitAudioLevel")->isEnabled());panel.setApplicationState(false,false,false,false);QVERIFY(child<QSpinBox>(panel,"transmitAudioLevel")->isEnabled());
        AudioLevelsPanel restored([]{return true;});QCOMPARE(restored.receiveGainDb(),-24);
    }
    void outputCheckLevelAndFailure(){
        AudioLevelsPanel panel([]{return true;});panel.show();panel.setDevices("microphone","speaker");child<QSpinBox>(panel,"transmitAudioLevel")->setValue(25);click(panel,"audioLevelTone");QTRY_VERIFY(QFile::exists(directory_.filePath("played.json")));QFile file(directory_.filePath("played.json"));QVERIFY(file.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(file.readAll()).object();QCOMPARE(result["device"].toString(),QString("speaker"));QCOMPARE(result["seconds"].toDouble(),1.);QVERIFY(result["peak"].toInt()>5300&&result["peak"].toInt()<5400);QVERIFY(!child<QSpinBox>(panel,"transmitAudioLevel")->isEnabled());QTRY_VERIFY(child<QWidget>(panel,"transmitAudioMeter")->property("hasReading").toBool());panel.stopChecks();QVERIFY(!panel.checking());
        panel.setDevices("broken","broken");click(panel,"audioLevelListen");QTRY_VERIFY(child<QLabel>(panel,"receiveAudioStatus")->text().contains("unavailable"));click(panel,"audioLevelTone");QTRY_VERIFY(child<QLabel>(panel,"transmitAudioStatus")->text().contains("unavailable"));QVERIFY(!panel.checking());AudioLevelsPanel restored([]{return true;});QCOMPARE(restored.transmitPercent(),25);
    }
    void applicationCaptureAndTransmit(){
        QSettings settings;settings.setValue("audio/input","microphone");settings.setValue("audio/output","radio-output");settings.setValue("audio/txLevelPercent",25);settings.setValue("tx/morseEnabled",true);settings.setValue("station/call","N0CALL");
        MainWindow window;window.show();auto* panel=child<AudioLevelsPanel>(window,"radioAudioLevels");QVERIFY(panel);window.findChild<QAction*>("radioTabAction")->trigger();click(window,"radioFrontendAudio");QTest::qWait(100);
        const auto artifact=qEnvironmentVariable("OPENSSTV_AUDIO_LEVELS_PREVIEW");if(!artifact.isEmpty())QVERIFY(window.grab().save(artifact));
        click(window,"recordButton");window.findChild<QAction*>("radioTabAction")->trigger();auto* meter=child<QWidget>(window,"receiveAudioMeter");QTRY_VERIFY(meter->property("hasReading").toBool());child<QSpinBox>(window,"receiveAudioGain")->setValue(6);QTRY_VERIFY(std::abs(meter->property("peakDb").toDouble()+12.0618)<.02);QVERIFY(!child<QPushButton>(window,"audioLevelListen")->isEnabled());click(window,"stopButton");QTRY_VERIFY(child<QPushButton>(window,"playButton")->isEnabled());QTRY_VERIFY(meter->property("hasReading").toBool());
        auto* mode=child<QComboBox>(window,"transmitMode");mode->setCurrentIndex(mode->findData("bw8"));click(window,"playButton");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("played.json")),10000);QFile file(directory_.filePath("played.json"));QVERIFY(file.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(file.readAll()).object();QCOMPARE(result["device"].toString(),QString("radio-output"));QVERIFY(result["peak"].toInt()>5300&&result["peak"].toInt()<5400);QVERIFY(result["seconds"].toDouble()>sstv::morseIdDurationMs({"N0CALL",20,800})/1000+8);QVERIFY(!child<QSpinBox>(window,"transmitAudioLevel")->isEnabled());QTRY_VERIFY(child<QWidget>(window,"transmitAudioMeter")->property("hasReading").toBool());click(window,"stopButton");QTRY_VERIFY(child<QSpinBox>(window,"transmitAudioLevel")->isEnabled());window.close();
    }
    void automaticReceptionAdjustsGainWithoutChangingTransmit(){
        QSettings settings;settings.setValue("audio/autoRxLevel",true);settings.setValue("audio/rxGainDb",24);settings.setValue("audio/txLevelPercent",100);settings.setValue("audio/input","microphone");
        MainWindow window;window.show();auto* panel=child<AudioLevelsPanel>(window,"radioAudioLevels");
        QTRY_COMPARE(panel->receiveGainDb(),12);QCOMPARE(settings.value("audio/rxGainDb").toInt(),12);QCOMPARE(panel->transmitPercent(),100);
        auto* meter=child<QWidget>(window,"receiveAudioMeter");QTRY_VERIFY(meter->property("hasReading").toBool());QVERIFY(!meter->property("clipping").toBool());QVERIFY(meter->property("peakDb").toDouble()<=-6.);
        QCOMPARE(child<QPushButton>(window,"recordButton")->text(),QString("Record audio"));QVERIFY(!child<QPushButton>(window,"liveAudioButton"));window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);AudioLevelsTests tests;return QTest::qExec(&tests,argc,argv);}
#include "audio_levels_tests.moc"
