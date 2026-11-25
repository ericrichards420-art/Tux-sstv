#include "mainwindow.h"
#include "captionpreview.h"
#include "composer.h"
#include "templatesdialog.h"
#include "core/morse.h"
#include <QtTest>
#include <QtWidgets>
#include <QScopeGuard>
#include <cmath>

class IdentificationTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    template<class T> static T* child(QObject& parent,const char* name){return parent.findChild<T*>(name);}
    static void settings(MainWindow& w,const std::function<void(QDialog*)>& edit){
        bool seen=false;QTimer::singleShot(15,&w,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});seen=true;edit(dialog);});w.findChild<QAction*>("settingsAction")->trigger();QVERIFY(seen);
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVIdentificationTests");QCoreApplication::setApplicationName("Identification");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_ID_AUDIO",directory_.filePath("sent.wav").toUtf8());
        QFile file(directory_.filePath("aplay"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("#!/usr/bin/python3\nimport os,sys,shutil,time\np=os.environ['OPENSSTV_ID_AUDIO']\nshutil.copyfile(sys.argv[-1],p+'.tmp')\nos.replace(p+'.tmp',p)\ntime.sleep(.5)\n");file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings().clear();QFile::remove(directory_.filePath("sent.wav"));}
    void checkboxIndependenceAndPersistence(){
        QSettings settings;settings.setValue("station/call","N0CALL");MainWindow w;w.show();auto* visible=child<QCheckBox>(w,"showCallsignOnImage");auto* morse=child<QCheckBox>(w,"morseAfterTx");auto* image=child<CaptionPreview>(w,"transmitImage");QVERIFY(visible->isChecked());QVERIFY(!morse->isChecked());QVERIFY(!image->textWidgetRect("callsign").isEmpty());visible->setChecked(false);morse->setChecked(true);QVERIFY(image->textWidgetRect("callsign").isEmpty());QCOMPARE(child<QLineEdit>(w,"stationCall")->text(),QString("N0CALL"));QVERIFY(!settings.value("tx/showCallsignOnImage").toBool());QVERIFY(settings.value("tx/morseEnabled").toBool());
        const auto artifact=qEnvironmentVariable("OPENSSTV_IDENTIFICATION_PREVIEWS");if(!artifact.isEmpty()){QVERIFY(QDir().mkpath(artifact));child<QScrollArea>(w,"operatingControlsScroll")->ensureWidgetVisible(morse);QApplication::processEvents();QVERIFY(w.grab().save(artifact+"/morse-only.png"));}
        child<QPushButton>(w,"transmitFullScreenButton")->click();QVERIFY(child<CaptionPreview>(w,"transmitFullScreenImage")->textWidgetRect("callsign").isEmpty());for(auto* preview:w.findChildren<CaptionPreview*>())QVERIFY(preview->textWidgetRect("callsign").isEmpty());w.close();MainWindow restored;QVERIFY(!child<QCheckBox>(restored,"showCallsignOnImage")->isChecked());QVERIFY(child<QCheckBox>(restored,"morseAfterTx")->isChecked());QCOMPARE(child<QLineEdit>(restored,"stationCall")->text(),QString("N0CALL"));restored.close();
    }
    void settingsSaveCancelAndSidebarSync(){
        QSettings().setValue("station/call","N0CALL");MainWindow w;w.show();settings(w,[&](QDialog* dialog){child<QCheckBox>(*dialog,"settingsShowCallsign")->setChecked(false);child<QCheckBox>(*dialog,"settingsMorseEnabled")->setChecked(true);dialog->reject();});QVERIFY(child<QCheckBox>(w,"showCallsignOnImage")->isChecked());QVERIFY(!child<QCheckBox>(w,"morseAfterTx")->isChecked());
        settings(w,[&](QDialog* dialog){child<QCheckBox>(*dialog,"settingsShowCallsign")->setChecked(false);child<QCheckBox>(*dialog,"settingsMorseEnabled")->setChecked(true);child<QSpinBox>(*dialog,"settingsMorseWpm")->setValue(25);QVERIFY(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->isEnabled());const auto artifact=qEnvironmentVariable("OPENSSTV_IDENTIFICATION_PREVIEWS");if(!artifact.isEmpty()){QApplication::processEvents();QVERIFY(dialog->grab().save(artifact+"/settings.png"));}dialog->accept();});QVERIFY(!child<QCheckBox>(w,"showCallsignOnImage")->isChecked());QVERIFY(child<QCheckBox>(w,"morseAfterTx")->isChecked());child<QCheckBox>(w,"morseAfterTx")->setChecked(false);settings(w,[&](QDialog* dialog){QVERIFY(!child<QCheckBox>(*dialog,"settingsMorseEnabled")->isChecked());QVERIFY(!child<QCheckBox>(*dialog,"settingsShowCallsign")->isChecked());dialog->reject();});w.close();
    }
    void templatesSuppressStationAndPlaceholder(){
        for(const auto& design:sstv::imageTemplates()){
            sstv::TemplateFields fields;fields.design=design.id;fields.toCall="K1OTHER";fields.message="73!";QMap<QString,QRect> visible,hidden;
            const auto shown=sstv::renderTemplate(fields,{320,240},"N0CALL",{},&visible);const auto clean=sstv::renderTemplate(fields,{320,240},"N0CALL",{},&hidden,false);QVERIFY(visible.contains("callsign"));QVERIFY(!hidden.contains("callsign"));QVERIFY(shown!=clean);QCOMPARE(clean,sstv::renderTemplate(fields,{320,240},"W1OTHER",{},nullptr,false));QCOMPARE(clean,sstv::renderTemplate(fields,{320,240},{},{},nullptr,false));QVERIFY(clean!=sstv::renderTemplate(fields,{320,240},{}));
            TemplatesDialog dialog(fields,false,*sstv::modeById("robot36"),"N0CALL",{},nullptr,{}, {},{},false);QCOMPARE(dialog.image(),sstv::composeTransmitImage(sstv::renderTemplate(fields,{sstv::modeById("robot36")->width,sstv::modeById("robot36")->height},"N0CALL",{},nullptr,false),*sstv::modeById("robot36"),{}).image);
        }
    }
    void hiddenCallsignInEditorsAndContextMenu(){
        QSettings().setValue("station/call","N0CALL");MainWindow w;w.show();auto* view=child<CaptionPreview>(w,"transmitImage");bool seen=false;QTimer::singleShot(15,&w,[&]{auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu)return;seen=true;const auto close=qScopeGuard([menu]{menu->close();});auto* action=menu->findChild<QAction*>("txMenuShowCallsign");QVERIFY(action);action->trigger();menu->close();});QMetaObject::invokeMethod(view,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,QPoint(12,12)));QVERIFY(seen);QVERIFY(!child<QCheckBox>(w,"showCallsignOnImage")->isChecked());QVERIFY(view->textWidgetRect("callsign").isEmpty());
        seen=false;QTimer::singleShot(15,&w,[&]{auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;seen=true;const auto close=qScopeGuard([dialog]{dialog->reject();});QVERIFY(!child<QPushButton>(*dialog,"compositionResetCallsign"));QVERIFY(child<CaptionPreview>(*dialog,"compositionPreview")->textWidgetRect("callsign").isEmpty());});child<QPushButton>(w,"compositionButton")->click();QVERIFY(seen);
        seen=false;QTimer::singleShot(15,&w,[&]{auto* dialog=qobject_cast<TemplatesDialog*>(QApplication::activeModalWidget());if(!dialog)return;seen=true;const auto close=qScopeGuard([dialog]{dialog->reject();});for(auto* preview:dialog->findChildren<CaptionPreview*>())QVERIFY(preview->textWidgetRect("callsign").isEmpty());});child<QPushButton>(w,"templatesButton")->click();QVERIFY(seen);w.close();
    }
    void morseOnlyTransmit_data(){QTest::addColumn<QString>("id");for(const auto* id:{"bw8","robot36","martin2"})QTest::newRow(id)<<QString(id);}

    void morseOnlyTransmit(){
        QFETCH(QString,id);QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("tx/showCallsignOnImage",false);settings.setValue("tx/morseEnabled",true);MainWindow w;w.show();auto* mode=child<QComboBox>(w,"transmitMode");mode->setCurrentIndex(mode->findData(id));auto* show=child<QCheckBox>(w,"showCallsignOnImage");auto* morse=child<QCheckBox>(w,"morseAfterTx");QVERIFY(!show->isChecked()&&morse->isChecked());child<QPushButton>(w,"playButton")->click();QVERIFY(!show->isEnabled());QVERIFY(!morse->isEnabled());QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("sent.wav")),7000);const auto audio=sstv::readWav(directory_.filePath("sent.wav"));const auto decoded=sstv::decode(audio);QVERIFY(decoded.complete);
        const auto clean=sstv::composeTransmitImage(sstv::testPattern(),*sstv::modeById(id),{}).image;
        {const auto expectedAudio=sstv::encode(clean,*sstv::modeById(id),48000);QVERIFY(audio.samples.size()>expectedAudio.samples.size());double difference=0;for(size_t i=0;i<expectedAudio.samples.size();++i)difference=std::max(difference,std::abs(double(audio.samples[i]-expectedAudio.samples[i])));QVERIFY2(difference<.0001,qPrintable(QString("Clean-image waveform differs by %1").arg(difference)));}
        sstv::Audio cw;cw.sampleRate=48000;sstv::appendMorseId(cw,{"N0CALL",20,800});QVERIFY(audio.samples.size()>cw.samples.size());const auto start=audio.samples.size()-cw.samples.size();double energy=0,error=0;for(size_t i=0;i<cw.samples.size();++i){energy+=audio.samples[start+i]*audio.samples[start+i];error=std::max(error,std::abs(double(audio.samples[start+i]-cw.samples[i])));}QVERIFY(energy>100);QVERIFY(error<.0001);QTRY_VERIFY(show->isEnabled());QCOMPARE(settings.value("station/call").toString(),QString("N0CALL"));w.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);IdentificationTests tests;return QTest::qExec(&tests,argc,argv);}
#include "identification_tests.moc"
