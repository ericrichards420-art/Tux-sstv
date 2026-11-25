#include "legacy_test_audio.h"
#include "mainwindow.h"
#include "captionpreview.h"
#include "composer.h"
#include "insetdialog.h"
#include "templatesdialog.h"
#include "patternsdialog.h"
#include <QtTest>
#include <QtWidgets>
#include <QUndoStack>
#include <QScopeGuard>

class UndoTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    int serial_=0;
    template<class T> static T* child(QObject& owner,const char* name){return owner.findChild<T*>(name);}
    static QUndoStack* stack(MainWindow& w){return child<QUndoStack>(w,"txUndoStack");}
    static void undo(MainWindow& w){child<QAction>(w,"undoTransmitAction")->trigger();}
    static void redo(MainWindow& w){child<QAction>(w,"redoTransmitAction")->trigger();}
    template<class T> static void modal(QWidget& owner,const std::function<void()>& trigger,const std::function<void(T*)>& edit){
        bool seen=false;QTimer poll;poll.setInterval(10);connect(&poll,&QTimer::timeout,&owner,[&]{auto* dialog=qobject_cast<T*>(QApplication::activeModalWidget());if(!dialog)return;poll.stop();seen=true;const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});edit(dialog);});poll.start();QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&owner,[]{if(auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();if(auto* m=qobject_cast<QMenu*>(QApplication::activePopupWidget()))m->close();});deadline.start(5000);trigger();QVERIFY(seen);
    }
    static void caption(MainWindow& w,const QString& text){modal<CompositionDialog>(w,[&]{child<QPushButton>(w,"compositionButton")->click();},[&](auto* d){child<QPlainTextEdit>(*d,"compositionText")->setPlainText(text);d->accept();});}
    QImage picture(MainWindow& w){
        const auto path=directory_.filePath(QString("snapshot-%1.png").arg(++serial_));modal<QFileDialog>(w,[&]{child<QAction>(w,"saveTransmitImageAction")->trigger();},[&](auto* d){d->selectFile(path);child<QLineEdit>(*d,"fileNameEdit")->setText(path);QMetaObject::invokeMethod(d,"accept",Qt::DirectConnection);});return QImage(path);
    }
    static void context(CaptionPreview* preview,const QString& key,const QString& label){
        const auto point=preview->textWidgetRect(key).center().toPoint();QVERIFY(!preview->textWidgetRect(key).isEmpty());bool seen=false;QTimer::singleShot(20,preview,[&]{auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu)return;const auto close=qScopeGuard([menu]{menu->close();});for(auto* action:menu->actions())if(action->text()==label){seen=true;action->trigger();break;}});QMetaObject::invokeMethod(preview,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,point));QVERIFY(seen);
    }
    static void design(TemplatesDialog* d,const QString& id){auto* list=child<QListWidget>(*d,"templateDesigns");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==id){list->setCurrentRow(i);return;}QFAIL("Missing template design");}
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVUndoTests");QCoreApplication::setApplicationName("Undo");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_UNDO_TEST",directory_.path().toUtf8());
        const QByteArray script=R"(#!/usr/bin/python3
import sys,os,shutil,time,struct,signal
root=os.environ['OPENSSTV_UNDO_TEST']
if os.path.basename(sys.argv[0])=='aplay':
    shutil.copyfile(sys.argv[-1],root+'/sent.wav.tmp');os.replace(root+'/sent.wav.tmp',root+'/sent.wav');time.sleep(1)
else:
    block=struct.pack('<h',1024)*12000
    while block: block=block[os.write(1,block):]
    signal.pause()
)";
        for(const auto& name:{"arecord","aplay"}){QFile f(directory_.filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));f.write(script);f.close();QVERIFY(f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
        QImage inset(120,80,QImage::Format_RGB32);inset.fill(Qt::red);{QPainter p(&inset);p.fillRect(QRect(60,0,60,80),Qt::blue);}QVERIFY(inset.save(directory_.filePath("inset.png")));
    }
    void init(){QSettings().clear();QSettings().setValue("station/call","N0CALL");QFile::remove(directory_.filePath("sent.wav"));}
    void captionsDragEditDeleteAndRedoBranch(){
        MainWindow w;w.show();QVERIFY(!child<QAction>(w,"undoTransmitAction")->isEnabled());const auto original=picture(w);caption(w,"First text 😎");QCOMPARE(stack(w)->count(),1);const auto first=picture(w);QVERIFY(first!=original);auto* view=child<CaptionPreview>(w,"transmitImage");const auto box=view->textWidgetRect("caption");QVERIFY(!box.isEmpty());
        QTest::mouseClick(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint());QCOMPARE(stack(w)->count(),1);QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint());for(int i=1;i<=8;++i)QTest::mouseMove(view,box.center().toPoint()+QPoint(i*3,-i*2),1);QCOMPARE(stack(w)->count(),1);QVERIFY(!child<QAction>(w,"undoTransmitAction")->isEnabled());QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint()+QPoint(24,-16));QCOMPARE(stack(w)->count(),2);QVERIFY(child<QAction>(w,"undoTransmitAction")->text().contains("Move text"));const auto moved=picture(w);QVERIFY(moved!=first);undo(w);QCOMPARE(picture(w),first);redo(w);QCOMPARE(picture(w),moved);
        modal<CompositionDialog>(w,[&]{context(view,"caption","Edit text & emoji…");},[](auto* d){child<QPlainTextEdit>(*d,"compositionText")->setPlainText("Edited content");d->accept();});const auto edited=picture(w);QVERIFY(edited!=moved);undo(w);QCOMPARE(picture(w),moved);redo(w);QCOMPARE(picture(w),edited);w.close();
    }
    void addRemoveAndCancelledEditor(){
        MainWindow w;w.show();caption(w,"Keep this caption");const auto drawn=picture(w);auto* view=child<CaptionPreview>(w,"transmitImage");context(view,"caption","Remove text block");QVERIFY(view->textWidgetRect("caption").isEmpty());QCOMPARE(stack(w)->count(),2);undo(w);QCOMPARE(picture(w),drawn);const int index=stack(w)->index();modal<CompositionDialog>(w,[&]{child<QPushButton>(w,"compositionButton")->click();},[](auto* d){child<QPlainTextEdit>(*d,"compositionText")->setPlainText("Cancelled!");d->reject();});QCOMPARE(stack(w)->index(),index);QVERIFY(stack(w)->canRedo());caption(w,"A new branch");QVERIFY(!stack(w)->canRedo());QVERIFY(!child<QAction>(w,"redoTransmitAction")->isEnabled());undo(w);QCOMPARE(picture(w),drawn);w.close();
    }
    void callsignPlacementVisibilityAndSettingsScope(){
        MainWindow w;w.show();auto* view=child<CaptionPreview>(w,"transmitImage");const auto original=picture(w);const auto rect=view->textWidgetRect("callsign");QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,rect.center().toPoint());QTest::mouseMove(view,rect.center().toPoint()+QPoint(40,70),2);QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,rect.center().toPoint()+QPoint(40,70));QCOMPARE(stack(w)->count(),1);const auto moved=picture(w);undo(w);QCOMPARE(picture(w),original);redo(w);QCOMPARE(picture(w),moved);context(view,"callsign","Reset callsign position");QCOMPARE(picture(w),original);undo(w);QCOMPARE(picture(w),moved);
        const int count=stack(w)->index();child<QCheckBox>(w,"showCallsignOnImage")->setChecked(false);QCOMPARE(stack(w)->index(),count+1);child<QCheckBox>(w,"morseAfterTx")->setChecked(true);child<QLineEdit>(w,"stationCall")->setText("K1NEW");undo(w);QVERIFY(child<QCheckBox>(w,"showCallsignOnImage")->isChecked());QCOMPARE(child<QLineEdit>(w,"stationCall")->text(),QString("K1NEW"));QVERIFY(child<QCheckBox>(w,"morseAfterTx")->isChecked());w.close();
    }
    void interruptedDragAndVisibility(){
        MainWindow w;w.show();const auto original=picture(w);auto* view=child<CaptionPreview>(w,"transmitImage");auto box=view->textWidgetRect("callsign");QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint());QTest::mouseMove(view,box.center().toPoint()+QPoint(45,55),2);QCOMPARE(stack(w)->count(),0);child<QCheckBox>(w,"showCallsignOnImage")->setChecked(false);QCOMPARE(stack(w)->count(),2);QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint()+QPoint(45,55));undo(w);QVERIFY(child<QCheckBox>(w,"showCallsignOnImage")->isChecked());QVERIFY(picture(w)!=original);undo(w);QCOMPARE(picture(w),original);
        box=view->textWidgetRect("callsign");QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint());QTest::mouseMove(view,box.center().toPoint()+QPoint(20,40),2);child<QPushButton>(w,"recordButton")->click();QVERIFY(!child<QAction>(w,"undoTransmitAction")->isEnabled());child<QPushButton>(w,"stopButton")->click();QTRY_VERIFY(child<QPushButton>(w,"recordButton")->isEnabled());undo(w);QCOMPARE(picture(w),original);w.close();
    }
    void templateTransactionAndClear(){
        MainWindow w;w.show();caption(w,"Layer before template");const auto original=picture(w);const int count=stack(w)->count();modal<TemplatesDialog>(w,[&]{child<QPushButton>(w,"templatesButton")->click();},[](auto* d){design(d,"reply");child<QLineEdit>(*d,"templateToCall")->setText("K1OTHER");child<QLineEdit>(*d,"templateMessage")->setText("A new reply card");child<QPushButton>(*d,"templateApply")->click();});QCOMPARE(stack(w)->count(),count+1);const auto card=picture(w);QVERIFY(card!=original);undo(w);QCOMPARE(picture(w),original);redo(w);QCOMPARE(picture(w),card);
        modal<TemplatesDialog>(w,[&]{child<QPushButton>(w,"templatesButton")->click();},[](auto* d){child<QPushButton>(*d,"templatePictureOnly")->click();});const auto cleared=picture(w);QVERIFY(cleared!=card);undo(w);QCOMPARE(picture(w),card);redo(w);QCOMPARE(picture(w),cleared);w.close();
    }
    void insetAddResizeMoveRemove(){
        MainWindow w;w.show();const auto original=picture(w);modal<InsetDialog>(w,[&]{child<QAction>(w,"pictureInPictureAction")->trigger();},[&](auto* d){modal<QFileDialog>(*d,[&]{child<QPushButton>(*d,"insetChooseImage")->click();},[&](auto* f){const auto path=directory_.filePath("inset.png");f->selectFile(path);child<QLineEdit>(*f,"fileNameEdit")->setText(path);QMetaObject::invokeMethod(f,"accept",Qt::DirectConnection);});child<QSpinBox>(*d,"insetWidthPercent")->setValue(40);child<QPushButton>(*d,"insetApply")->click();});QCOMPARE(stack(w)->count(),1);const auto added=picture(w);QVERIFY(added!=original);undo(w);QCOMPARE(picture(w),original);redo(w);QCOMPARE(picture(w),added);
        modal<InsetDialog>(w,[&]{child<QAction>(w,"pictureInPictureAction")->trigger();},[](auto* d){child<QSpinBox>(*d,"insetWidthPercent")->setValue(60);child<QCheckBox>(*d,"insetFrame")->setChecked(false);child<QPushButton>(*d,"insetApply")->click();});QCOMPARE(stack(w)->count(),2);const auto resized=picture(w);QVERIFY(resized!=added);undo(w);QCOMPARE(picture(w),added);redo(w);auto* view=child<CaptionPreview>(w,"transmitImage");const auto box=view->textWidgetRect("inset");QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint());QTest::mouseMove(view,box.center().toPoint()-QPoint(25,30),2);QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,box.center().toPoint()-QPoint(25,30));QCOMPARE(stack(w)->count(),3);const auto moved=picture(w);context(view,"inset","Remove received inset");QVERIFY(view->textWidgetRect("inset").isEmpty());undo(w);QCOMPARE(picture(w),moved);undo(w);QCOMPARE(picture(w),resized);w.close();
    }
    void fullscreenShortcutsAndNativeTextUndo(){
        MainWindow w;w.show();caption(w,"Fullscreen text");child<QPushButton>(w,"transmitFullScreenButton")->click();auto* full=child<QWidget>(w,"transmitFullScreenWindow");QTRY_VERIFY(full->isActiveWindow());auto* view=child<CaptionPreview>(w,"transmitFullScreenImage");QVERIFY(child<QPushButton>(w,"undoTransmitButton")->isVisible());QTest::keyClick(full,Qt::Key_Z,Qt::ControlModifier);QCOMPARE(stack(w)->index(),0);QVERIFY(view->textWidgetRect("caption").isEmpty());QVERIFY(full->isVisible());QTest::keyClick(full,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);QCOMPARE(stack(w)->index(),1);QVERIFY(!view->textWidgetRect("caption").isEmpty());const int count=stack(w)->count();modal<CompositionDialog>(w,[&]{child<QPushButton>(w,"compositionButton")->click();},[](auto* d){auto* edit=child<QPlainTextEdit>(*d,"compositionText");edit->setFocus();QTest::keyClicks(edit,"temporary");QTest::keyClick(edit,Qt::Key_Z,Qt::ControlModifier);QVERIFY(edit->toPlainText().isEmpty());d->reject();});QCOMPARE(stack(w)->count(),count);QVERIFY(full->isVisible());
        const auto artifact=qEnvironmentVariable("OPENSSTV_UNDO_PREVIEW");if(!artifact.isEmpty()){QVERIFY(QDir().mkpath(artifact));QVERIFY(full->grab().save(artifact+"/fullscreen.png"));}QTRY_VERIFY(full->isActiveWindow());QTest::keyClick(full,Qt::Key_Escape);QTRY_VERIFY(!full->isVisible());if(!artifact.isEmpty())QVERIFY(w.grab().save(artifact+"/normal.png"));w.close();
    }
    void stationFieldKeepsNativeUndo(){
        MainWindow w;w.show();caption(w,"Document stays put");const int count=stack(w)->count();auto* call=child<QLineEdit>(w,"stationCall");call->setFocus();call->selectAll();QTest::keyClicks(call,"K1NEW");QTest::keyClick(call,Qt::Key_Z,Qt::ControlModifier);QCOMPARE(call->text(),QString("N0CALL"));QCOMPARE(stack(w)->count(),count);QCOMPARE(stack(w)->index(),count);QVERIFY(!child<CaptionPreview>(w,"transmitImage")->textWidgetRect("caption").isEmpty());w.close();
    }
    void historyLimitAndNewPictureReset(){
        MainWindow w;w.show();auto* view=child<CaptionPreview>(w,"transmitImage");for(int i=0;i<65;++i)view->textMoved("callsign",{(i+1)/100.,.1});QCOMPARE(stack(w)->count(),50);for(int i=0;i<50;++i)undo(w);QVERIFY(!stack(w)->canUndo());QVERIFY(stack(w)->canRedo());modal<PatternsDialog>(w,[&]{child<QPushButton>(w,"testPatternsButton")->click();},[](auto* d){child<QListWidget>(*d,"testPatternList")->setCurrentRow(1);child<QPushButton>(*d,"useTestPattern")->click();});QCOMPARE(stack(w)->count(),0);QVERIFY(!child<QAction>(w,"redoTransmitAction")->isEnabled());caption(w,"Do not carry to next picture");
        const auto photo=directory_.filePath("new-photo.png");QImage source(100,100,QImage::Format_RGB32);source.fill(Qt::green);QVERIFY(source.save(photo));modal<QFileDialog>(w,[&]{child<QPushButton>(w,"addStockPhotosButton")->click();},[&](auto* d){d->selectFile(photo);child<QLineEdit>(*d,"fileNameEdit")->setText(photo);QMetaObject::invokeMethod(d,"accept",Qt::DirectConnection);});QCOMPARE(stack(w)->count(),0);QVERIFY(view->textWidgetRect("caption").isEmpty());w.close();
    }
    void sendAfterUndoAndLocks_data(){QTest::addColumn<QString>("profile");QTest::newRow("Robot 36")<<QString("robot36");}
    void sendAfterUndoAndLocks(){
        QFETCH(QString,profile);MainWindow w;w.show();auto* mode=child<QComboBox>(w,"transmitMode");mode->setCurrentIndex(mode->findData("robot36"));const auto clean=picture(w);caption(w,"Undo before sending");const auto annotated=picture(w);undo(w);QCOMPARE(picture(w),clean);const int index=stack(w)->index();child<QPushButton>(w,"playButton")->click();QVERIFY(!child<QAction>(w,"redoTransmitAction")->isEnabled());redo(w);QCOMPARE(stack(w)->index(),index);QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory_.filePath("sent.wav")),5000);const auto decoded=sstv::decode(sstv::readWav(directory_.filePath("sent.wav")));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(clean));QTRY_VERIFY(child<QPushButton>(w,"playButton")->isEnabled());redo(w);QCOMPARE(picture(w),annotated);
        child<QPushButton>(w,"recordButton")->click();QVERIFY(!child<QAction>(w,"undoTransmitAction")->isEnabled());const int recordingIndex=stack(w)->index();undo(w);QCOMPARE(stack(w)->index(),recordingIndex);child<QPushButton>(w,"stopButton")->click();QTRY_VERIFY(child<QPushButton>(w,"recordButton")->isEnabled());QVERIFY(child<QAction>(w,"undoTransmitAction")->isEnabled());w.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);UndoTests tests;return QTest::qExec(&tests,argc,argv);}
#include "undo_tests.moc"
