#include "legacy_test_audio.h"
#include "captionpreview.h"
#include "composer.h"
#include "templatesdialog.h"
#include "mainwindow.h"
#include "patternsdialog.h"
#include "signalview.h"
#include <QtTest>
#include <QtWidgets>
#include <cmath>

class DragTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    static QImage source(){QImage image(640,480,QImage::Format_RGB32);image.fill(QColor("#224466"));return image;}
    static void hover(QWidget* widget,QPoint point){
        // Exercise hover input with explicit local/global coordinates, including
        // on headless Qt platforms without a native pointing device.
        QMouseEvent event(QEvent::MouseMove,QPointF(point),QPointF(widget->mapToGlobal(point)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(widget,&event);
    }
    static void drag(CaptionPreview* preview,QPoint start,QPoint finish){QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(preview,finish,10);QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,finish);QApplication::processEvents();}
    static void selectOriginalPattern(MainWindow& window){
        bool chosen=false;QTimer timer;timer.setInterval(20);connect(&timer,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<PatternsDialog*>(QApplication::activeModalWidget());if(!dialog)return;timer.stop();dialog->findChild<QListWidget*>("testPatternList")->setCurrentRow(0);chosen=true;dialog->findChild<QPushButton*>("useTestPattern")->click();});timer.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);window.findChild<QPushButton*>("testPatternsButton")->click();QVERIFY(chosen);
    }
    static void editCaption(MainWindow& window,const std::function<void(CompositionDialog*)>& edit){
        QTimer timer;timer.setInterval(25);bool edited=false;connect(&timer,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;timer.stop();edited=true;edit(dialog);});timer.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        window.findChild<QPushButton*>("compositionButton")->click();QVERIFY(edited);
    }
    static void captionMenu(CaptionPreview* preview,const QString& key,const QString& actionText,const std::function<void(CompositionDialog*)>& edit={}){
        bool chosen=false,edited=!edit;QTimer menuTimer,editorTimer;menuTimer.setInterval(20);editorTimer.setInterval(25);
        connect(&menuTimer,&QTimer::timeout,preview,[&]{
            auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu)return;
            for(auto* action:menu->actions())if(action->text()==actionText){menuTimer.stop();chosen=true;menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);return;}
        });
        connect(&editorTimer,&QTimer::timeout,preview,[&]{auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog||!edit)return;editorTimer.stop();edited=true;edit(dialog);});
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,preview,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();if(auto* menu=QApplication::activePopupWidget())menu->close();});deadline.start(5000);menuTimer.start();editorTimer.start();
        const auto point=preview->textWidgetRect(key).center().toPoint();QVERIFY(!preview->textWidgetRect(key).isEmpty());QVERIFY(QMetaObject::invokeMethod(preview,"customContextMenuRequested",Q_ARG(QPoint,point)));QVERIFY(chosen);QVERIFY(edited);
    }
    static void editTemplate(MainWindow& window,const std::function<void(TemplatesDialog*)>& edit){
        QTimer timer;timer.setInterval(25);bool edited=false;connect(&timer,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<TemplatesDialog*>(QApplication::activeModalWidget());if(!dialog)return;timer.stop();edited=true;edit(dialog);});timer.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        window.findChild<QPushButton*>("templatesButton")->click();QVERIFY(edited);
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);
        QCoreApplication::setOrganizationName("OpenSSTVDragTests");QCoreApplication::setApplicationName("OpenSSTVDragTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        qputenv("OPENSSTV_DRAG_TX_COPY",directory_.filePath("tx.wav").toUtf8());QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write(R"(#!/usr/bin/python3
import os,shutil,sys,time
target=os.environ['OPENSSTV_DRAG_TX_COPY']
shutil.copyfile(sys.argv[-1],target+'.tmp')
os.replace(target+'.tmp',target)
time.sleep(.3)
)");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        auto incoming=sstv::encode(sstv::testPattern(),*sstv::modeById("robot36"),12000);incoming.samples.resize(24000);
        sstv::writeWav(directory_.filePath("rx.wav"),incoming);qputenv("OPENSSTV_DRAG_RX_WAV",directory_.filePath("rx.wav").toUtf8());
        QFile input(directory_.filePath("arecord"));QVERIFY(input.open(QIODevice::WriteOnly));input.write(R"(#!/usr/bin/python3
import os,sys,time,wave
if sys.argv[sys.argv.index('-D')+1]!='smooth-rx': sys.exit(1)
with wave.open(os.environ['OPENSSTV_DRAG_RX_WAV']) as f: data=f.readframes(f.getnframes())
while data: data=data[os.write(1,data):]
while True: os.write(1,bytes(2400));time.sleep(.1)
)");input.close();QVERIFY(input.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings s;s.clear();QFile::remove(directory_.filePath("tx.wav"));}
    void backgroundReceiveDoesNotInterruptDrag(){
        QSettings settings;settings.setValue("audio/input","smooth-rx");settings.setValue("station/call","N0CALL");MainWindow window;window.show();
        auto* view=window.findChild<CaptionPreview*>("transmitImage");auto* scope=window.findChild<SignalView*>("signalScope");auto* editor=window.findChild<QPushButton*>("compositionButton");
        QTRY_VERIFY(scope->historyRows()>2);const auto rows=scope->historyRows();const auto original=view->textWidgetRect("callsign");QVERIFY(!original.isEmpty());
        QSignalSpy finished(view,&CaptionPreview::editFinished);const auto start=original.center().toPoint();QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,start);
        bool interrupted=false;QElapsedTimer time;time.start();int moves=0;QPoint last=start;
        while(time.elapsed()<2300){last=start+QPoint(40+moves%50,30+moves%30);QTest::mouseMove(view,last);QTest::qWait(20);++moves;if(finished.count()!=0||!editor->isEnabled()||view->textWidgetRect("callsign").isEmpty()){interrupted=true;break;}}
        QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,last);
        QVERIFY2(!interrupted,"A background receive update disabled editing or cancelled the drag");QCOMPARE(finished.count(),1);QVERIFY(view->textWidgetRect("callsign")!=original);
        QTRY_COMPARE(window.findChild<QLabel*>("detectedReceiveMode")->property("modeId").toString(),QString("robot36"));
        QVERIFY(scope->inputActive());QVERIFY(scope->historyRows()>rows);QCOMPARE(window.findChild<QPushButton*>("recordButton")->text(),QString("Record audio"));window.close();
    }
    void freeCaptionFitsEveryMode(){
        for(const auto& mode:sstv::modes()){
            const auto base=sstv::composeTransmitImage(source(),mode,"N0CALL").image;
            for(const auto point:{QPointF(-1,-1),QPointF(0,0),QPointF(.3,.4),QPointF(1,1),QPointF(100,100)}){
                sstv::TextOverlay overlay;overlay.text="73! 📻\nHELLO";overlay.freePosition=true;overlay.relativePosition=point;overlay.backing=true;
                const auto rendered=sstv::composeTransmitImage(source(),mode,"N0CALL",overlay);QVERIFY(!rendered.captionBounds.isEmpty());QVERIFY(rendered.placementBounds.contains(rendered.captionBounds));QVERIFY(rendered.image!=base);
                if(point==QPointF(0,0))QCOMPARE(rendered.captionBounds.topLeft(),QPoint(0,0));
                if(point==QPointF(1,1))QCOMPARE(rendered.captionBounds.bottomRight(),rendered.image.rect().bottomRight());
                auto restored=rendered.image;{QPainter p(&restored);p.drawImage(rendered.captionBounds,base,rendered.captionBounds);}QCOMPARE(restored,base);
                if(mode.family==sstv::Family::Mono)for(int y=0;y<mode.height;y+=7)for(int x=0;x<mode.width;x+=7){const auto pixel=rendered.image.pixel(x,y);QCOMPARE(qRed(pixel),qGreen(pixel));QCOMPARE(qGreen(pixel),qBlue(pixel));}
            }
        }
    }
    void captionMouseDrag_data(){QTest::addColumn<QString>("mode");for(const auto* id:{"robot36","martin2","bw8","pd120"})QTest::newRow(id)<<QString(id);}
    void captionMouseDrag(){
        QFETCH(QString,mode);const auto& selected=*sstv::modeById(mode);sstv::TextOverlay overlay;overlay.text="Hello 73 📻";overlay.backing=true;
        CompositionDialog dialog(source(),selected,"N0CALL",overlay,{});dialog.show();QApplication::processEvents();auto* preview=dialog.findChild<CaptionPreview*>("compositionPreview");QVERIFY(preview);
        const auto before=preview->captionBounds();const auto frame=preview->imageRect();const auto start=preview->captionWidgetRect().center().toPoint();const auto delta=QPoint(-frame.width()/10,-frame.height()/3);drag(preview,start,start+delta);
        QVERIFY(dialog.overlay().freePosition);QCOMPARE(dialog.findChild<QComboBox*>("compositionPosition")->currentIndex(),9);
        const auto moved=preview->captionBounds();QVERIFY(std::abs(moved.x()-before.x()-double(delta.x())*selected.width/frame.width())<3);QVERIFY(std::abs(moved.y()-before.y()-double(delta.y())*selected.height/frame.height())<3);
        const auto position=dialog.overlay().relativePosition;dialog.resize(1100,720);QApplication::processEvents();QCOMPARE(dialog.overlay().relativePosition,position);QCOMPARE(preview->captionBounds(),moved);
        drag(preview,{1,1},{200,200});QCOMPARE(dialog.overlay().relativePosition,position);
        drag(preview,preview->captionWidgetRect().center().toPoint(),{-300,-300});const auto clamped=preview->captionBounds();const auto composition=sstv::composeTransmitImage(source(),selected,"N0CALL",dialog.overlay());QVERIFY(composition.placementBounds.contains(clamped));QCOMPARE(clamped.topLeft(),composition.placementBounds.topLeft());
        dialog.findChild<QComboBox*>("compositionPosition")->setCurrentIndex(8);QVERIFY(!dialog.overlay().freePosition);
        const auto artifact=qEnvironmentVariable("OPENSSTV_DRAG_PREVIEW");if(mode=="robot36"&&!artifact.isEmpty()){drag(preview,preview->captionWidgetRect().center().toPoint(),preview->imageRect().center());QVERIFY(dialog.grab().save(artifact));}
        dialog.findChild<QPushButton*>("compositionClear")->click();QVERIFY(dialog.overlay().text.isEmpty());QVERIFY(!dialog.overlay().freePosition);QVERIFY(preview->captionBounds().isEmpty());dialog.reject();QVERIFY(!overlay.freePosition);
    }
    void templateFieldsMoveAndReset(){
        sstv::TemplateFields fields;fields.city="Austin";fields.state="Texas";fields.weather="Sunny";fields.temperature="72";fields.date={2026,11,20};fields.time={19,45};fields.utc=true;
        for(const auto& design:sstv::imageTemplates()){
            auto f=fields;f.design=design.id;f.toCall="K1ABC";f.report="GOOD COPY";f.message="73!";QMap<QString,QRect> bounds;
            const auto original=sstv::renderTemplate(f,{320,240},"N0CALL",{},&bounds);QVERIFY(bounds.contains("callsign"));
            f.textPositions["callsign"]={1.,1.};QMap<QString,QRect> movedBounds;const auto moved=sstv::renderTemplate(f,{320,240},"N0CALL",{},&movedBounds);QVERIFY(moved!=original);QVERIFY(moved.rect().contains(movedBounds["callsign"]));
            auto restored=moved;{QPainter p(&restored);p.drawImage(bounds["callsign"],original,bounds["callsign"]);p.drawImage(movedBounds["callsign"],original,movedBounds["callsign"]);}QCOMPARE(restored,original);
            QSettings settings;f.save(settings);QVERIFY(sstv::TemplateFields::load(settings).textPositions.isEmpty());f.clearFields();QVERIFY(f.textPositions.isEmpty());
        }
        TemplatesDialog dialog(fields,true,*sstv::modeById("robot36"),"N0CALL",{});dialog.show();QApplication::processEvents();auto* preview=dialog.findChild<CaptionPreview*>("templatePreview");QVERIFY(preview);
        QMap<QString,QRect> bounds;const auto original=sstv::renderTemplate(fields,{320,240},"N0CALL",{},&bounds);
        const auto start=preview->textWidgetRect("location").center().toPoint();drag(preview,start,start+QPoint(preview->imageRect().width()/5,0));QVERIFY(dialog.fields().textPositions.contains("location"));QVERIFY(dialog.image()!=original);
        const auto artifact=qEnvironmentVariable("OPENSSTV_TEMPLATE_DRAG_PREVIEW");if(!artifact.isEmpty())QVERIFY(dialog.grab().save(artifact));
        dialog.findChild<QPushButton*>("templateResetPositions")->click();QVERIFY(dialog.fields().textPositions.isEmpty());QCOMPARE(dialog.image(),original);
        dialog.reject();QVERIFY(fields.textPositions.isEmpty());
    }
    void applyCancelAndActualTransmission(){
        QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-drag-device");MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));sstv::TextOverlay expected;
        editCaption(window,[&](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("73 📻");auto* preview=dialog->findChild<CaptionPreview*>("compositionPreview");drag(preview,preview->captionWidgetRect().center().toPoint(),preview->imageRect().center());expected=dialog->overlay();QVERIFY(expected.freePosition);dialog->accept();});
        captionMenu(window.findChild<CaptionPreview*>("transmitImage"),"caption","Edit text & emoji…",[&](CompositionDialog* dialog){QCOMPARE(dialog->overlay().relativePosition,expected.relativePosition);auto* preview=dialog->findChild<CaptionPreview*>("compositionPreview");drag(preview,preview->captionWidgetRect().center().toPoint(),{10,10});dialog->reject();});
        const auto prepared=(sstv::composeTransmitImage(sstv::testPattern(),*sstv::modeById("robot36"),"N0CALL",expected).image);
        window.findChild<QPushButton*>("playButton")->click();const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(prepared));QTRY_VERIFY(window.findChild<QPushButton*>("playButton")->isEnabled());
        selectOriginalPattern(window);
        editCaption(window,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());QVERIFY(!dialog->overlay().freePosition);dialog->reject();});window.close();
        MainWindow reopened;reopened.show();editCaption(reopened,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());QVERIFY(!dialog->overlay().freePosition);dialog->reject();});reopened.close();
    }
    void templateAndCaptionDragReachTransmittedImage(){
        QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-drag-device");MainWindow window;window.show();window.findChild<QComboBox*>("transmitMode")->setCurrentIndex(window.findChild<QComboBox*>("transmitMode")->findData("robot36"));
        sstv::TemplateFields expectedFields;sstv::TextOverlay expectedCaption;
        editTemplate(window,[&](TemplatesDialog* dialog){
            dialog->findChild<QLineEdit*>("templateCity")->setText("Austin");dialog->findChild<QLineEdit*>("templateTemperature")->setText("72");auto* preview=dialog->findChild<CaptionPreview*>("templatePreview");const auto start=preview->textWidgetRect("location").center().toPoint();drag(preview,start,start+QPoint(60,0));expectedFields=dialog->fields();QVERIFY(expectedFields.textPositions.contains("location"));dialog->findChild<QPushButton*>("templateApply")->click();
        });
        editCaption(window,[](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("73 📻");dialog->accept();});
        editTemplate(window,[&](TemplatesDialog* dialog){
            auto* preview=dialog->findChild<CaptionPreview*>("templatePreview");drag(preview,preview->textWidgetRect("caption").center().toPoint(),preview->imageRect().center());expectedCaption=dialog->captionOverlay();QVERIFY(expectedCaption.freePosition);dialog->findChild<QPushButton*>("templateApply")->click();
        });
        editTemplate(window,[&](TemplatesDialog* dialog){QCOMPARE(dialog->fields().textPositions,expectedFields.textPositions);dialog->findChild<QPushButton*>("templateResetPositions")->click();dialog->reject();});
        const auto* mode=sstv::modeById("robot36");const auto prepared=(sstv::composeTransmitImage(sstv::renderTemplate(expectedFields,{320,240},"N0CALL"),*mode,{},expectedCaption).image);
        window.findChild<QPushButton*>("playButton")->click();const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(prepared));QTRY_VERIFY(window.findChild<QPushButton*>("playButton")->isEnabled());window.close();
        QVERIFY(!settings.contains("tx/template/textPositions"));
    }
    void freeCallsignFitsEveryMode(){
        for(const auto& mode:sstv::modes()){
            const auto original=source();const auto base=sstv::fitImage(original,mode);
            for(const auto point:{QPointF(-1,-1),QPointF(0,0),QPointF(.3,.4),QPointF(1,1),QPointF(100,100)}){
                sstv::CallsignPlacement placement;placement.freePosition=true;placement.relativePosition=point;
                const auto composed=sstv::composeTransmitImage(original,mode,"N0CALL",{},placement);QVERIFY(!composed.callsignBounds.isEmpty());QVERIFY(composed.image.rect().contains(composed.callsignBounds));QVERIFY(composed.image!=base);
                auto restored=composed.image;{QPainter p(&restored);p.drawImage(composed.callsignBounds,base,composed.callsignBounds);}QCOMPARE(restored,base);QCOMPARE(original,source());
                if(point==QPointF(0,0))QCOMPARE(composed.callsignBounds.topLeft(),QPoint(0,0));
                if(point==QPointF(1,1))QCOMPARE(composed.callsignBounds.bottomRight(),composed.image.rect().bottomRight());
            }
        }
    }
    void composerMovesCallsignIndependently(){
        const auto& mode=*sstv::modeById("martin2");sstv::TextOverlay overlay;overlay.text="HELLO 73";
        CompositionDialog dialog(source(),mode,"N0CALL",overlay,{});dialog.show();QApplication::processEvents();auto* preview=dialog.findChild<CaptionPreview*>("compositionPreview");
        const auto caption=preview->captionBounds();const auto call=preview->textWidgetRect("callsign");QVERIFY(!call.isEmpty());
        drag(preview,call.center().toPoint(),preview->imageRect().center());QVERIFY(dialog.callsignPlacement().freePosition);QVERIFY(preview->textWidgetRect("callsign")!=call);QCOMPARE(preview->captionBounds(),caption);QVERIFY(!dialog.overlay().freePosition);
        const auto placed=dialog.callsignPlacement();const auto bounds=preview->textWidgetRect("callsign");dialog.resize(1100,720);QApplication::processEvents();QCOMPARE(dialog.callsignPlacement().relativePosition,placed.relativePosition);QVERIFY(preview->textWidgetRect("callsign")!=bounds);
        dialog.findChild<QPushButton*>("compositionResetCallsign")->click();QVERIFY(!dialog.callsignPlacement().freePosition);dialog.reject();QVERIFY(!overlay.freePosition);
    }
    void mainCallsignDragAndTransmission_data(){QTest::addColumn<bool>("useTemplate");QTest::newRow("ordinary callsign")<<false;QTest::newRow("template callsign")<<true;}
    void mainCallsignDragAndTransmission(){
        QFETCH(bool,useTemplate);QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-drag-device");
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));
        if(useTemplate)editTemplate(window,[](TemplatesDialog* dialog){dialog->findChild<QLineEdit*>("templateCity")->setText("Austin");dialog->findChild<QPushButton*>("templateApply")->click();});
        auto* preview=window.findChild<CaptionPreview*>("transmitImage");QVERIFY(preview);QVERIFY(!preview->textWidgetRect("callsign").isEmpty());
        const auto old=preview->textWidgetRect("callsign");drag(preview,old.center().toPoint(),preview->imageRect().center());QVERIFY(preview->textWidgetRect("callsign")!=old);
        const auto normalized=[&]{const auto box=preview->imageRect();const auto point=preview->textWidgetRect("callsign").topLeft()-box.topLeft();return QPointF(point.x()/box.width(),point.y()/box.height());};
        const auto beforeResize=normalized();window.resize(1150,780);QApplication::processEvents();const auto afterResize=normalized();QVERIFY(std::abs(beforeResize.x()-afterResize.x())<.005);QVERIFY(std::abs(beforeResize.y()-afterResize.y())<.005);
        window.findChild<QPushButton*>("transmitFullScreenButton")->click();auto* fullWindow=window.findChild<QWidget*>("transmitFullScreenWindow");auto* full=window.findChild<CaptionPreview*>("transmitFullScreenImage");QVERIFY(fullWindow->isVisible());QCOMPARE(full->cursor().shape(),Qt::BlankCursor);
        drag(full,full->textWidgetRect("callsign").center().toPoint(),full->imageRect().center()+QPoint(80,60));QVERIFY(full->cursor().shape()!=Qt::BlankCursor);QTest::keyClick(fullWindow,Qt::Key_Escape);QTRY_VERIFY(!fullWindow->isVisible());
        sstv::CallsignPlacement placement;sstv::TemplateFields fields;
        if(useTemplate)editTemplate(window,[&](TemplatesDialog* dialog){fields=dialog->fields();QVERIFY(fields.textPositions.contains("callsign"));dialog->reject();});
        else {
            const auto current=normalized();editCaption(window,[&](CompositionDialog* dialog){placement=dialog->callsignPlacement();QVERIFY(placement.freePosition);auto* edit=dialog->findChild<CaptionPreview*>("compositionPreview");drag(edit,edit->textWidgetRect("callsign").center().toPoint(),edit->imageRect().topLeft()+QPoint(10,10));dialog->reject();});
            const auto afterCancel=normalized();QVERIFY(std::abs(current.x()-afterCancel.x())<.005);QVERIFY(std::abs(current.y()-afterCancel.y())<.005);
        }
        const auto screenshot=qEnvironmentVariable("OPENSSTV_CALLSIGN_DRAG_PREVIEW");if(!useTemplate&&!screenshot.isEmpty())QVERIFY(window.grab().save(screenshot));
        const auto* selectedMode=sstv::modeById("robot36");const auto base=useTemplate?sstv::renderTemplate(fields,{320,240},"N0CALL"):sstv::testPattern();const auto expected=(sstv::composeTransmitImage(base,*selectedMode,useTemplate?QString{}:"N0CALL",{},placement).image);
        auto* send=window.findChild<QPushButton*>("playButton");send->click();QVERIFY(preview->textWidgetRect("callsign").isEmpty());
        const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto result=sstv::decode(sstv::readWav(wav));QVERIFY(result.complete);QCOMPARE(result.image,decodedReference(expected));QTRY_VERIFY(send->isEnabled());QVERIFY(!preview->textWidgetRect("callsign").isEmpty());
        selectOriginalPattern(window);
        editCaption(window,[](CompositionDialog* dialog){QVERIFY(!dialog->callsignPlacement().freePosition);dialog->reject();});window.close();
    }
    void mainCaptionDragAndTransmission_data(){
        QTest::addColumn<bool>("useTemplate");QTest::addColumn<QString>("callsign");
        QTest::newRow("picture caption")<<false<<QString("N0CALL");
        QTest::newRow("template caption")<<true<<QString("N0CALL");
        QTest::newRow("caption without callsign")<<false<<QString{};
    }
    void mainCaptionDragAndTransmission(){
        QFETCH(bool,useTemplate);QFETCH(QString,callsign);
        QSettings settings;settings.setValue("station/call",callsign);settings.setValue("audio/output","fake-drag-device");
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));
        sstv::TemplateFields fields;
        if(useTemplate)editTemplate(window,[&](TemplatesDialog* dialog){dialog->findChild<QLineEdit*>("templateCity")->setText("Texarkana");fields=dialog->fields();dialog->findChild<QPushButton*>("templateApply")->click();});
        const QString message="Hello from Texarkana, TX 😎";
        editCaption(window,[&](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText(message);dialog->accept();});
        auto* preview=window.findChild<CaptionPreview*>("transmitImage");QVERIFY(!preview->captionWidgetRect().isEmpty());
        const auto callBounds=preview->textWidgetRect("callsign");const auto oldCaption=preview->captionWidgetRect();
        drag(preview,oldCaption.center().toPoint(),oldCaption.center().toPoint()-QPoint(0,preview->imageRect().height()/4));
        QVERIFY(preview->captionWidgetRect()!=oldCaption);QCOMPARE(preview->textWidgetRect("callsign"),callBounds);
        const auto normalized=[&]{const auto box=preview->imageRect();const auto point=preview->captionWidgetRect().topLeft()-box.topLeft();return QPointF(point.x()/box.width(),point.y()/box.height());};
        const auto beforeResize=normalized();window.resize(1150,780);QApplication::processEvents();QVERIFY(QLineF(beforeResize,normalized()).length()<.005);
        window.findChild<QPushButton*>("transmitFullScreenButton")->click();auto* fullscreen=window.findChild<QWidget*>("transmitFullScreenWindow");auto* full=window.findChild<CaptionPreview*>("transmitFullScreenImage");QTRY_VERIFY(fullscreen->isVisible());QTest::qWait(30);
        const auto fullCaption=full->captionWidgetRect();const auto fullCall=full->textWidgetRect("callsign");QVERIFY(!fullCaption.isEmpty());
        drag(full,fullCaption.center().toPoint(),fullCaption.center().toPoint()+QPoint(full->imageRect().width()/10,-full->imageRect().height()/6));
        QVERIFY(full->captionWidgetRect()!=fullCaption);QCOMPARE(full->textWidgetRect("callsign"),fullCall);
        QTest::keyClick(fullscreen,Qt::Key_Escape);QTRY_VERIFY(!fullscreen->isVisible());const auto placed=normalized();
        sstv::TextOverlay overlay;
        captionMenu(preview,"caption","Edit text & emoji…",[&](CompositionDialog* dialog){overlay=dialog->overlay();QVERIFY(overlay.freePosition);QCOMPARE(overlay.text,message);QCOMPARE(dialog->findChild<CaptionPreview*>("compositionPreview")->captionBounds(),preview->captionBounds());QCOMPARE(dialog->findChild<QComboBox*>("compositionPosition")->currentIndex(),9);dialog->reject();});
        const auto* selected=sstv::modeById("robot36");const auto base=useTemplate?sstv::renderTemplate(fields,{320,240},callsign):sstv::testPattern();
        const auto expected=(sstv::composeTransmitImage(base,*selected,useTemplate?QString{}:callsign,overlay).image);
        auto* send=window.findChild<QPushButton*>("playButton");send->click();QVERIFY(preview->captionWidgetRect().isEmpty());QVERIFY(full->captionWidgetRect().isEmpty());
        const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));
        QTRY_VERIFY(send->isEnabled());QVERIFY(QLineF(placed,normalized()).length()<.005);
        selectOriginalPattern(window);QVERIFY(preview->captionWidgetRect().isEmpty());QVERIFY(full->captionWidgetRect().isEmpty());window.close();
    }
    void multipleCaptionsFitEveryMode(){
        for(const auto& mode:sstv::modes()){
            QList<sstv::TextOverlay> layers;
            for(int i=0;i<3;++i){sstv::TextOverlay text;text.text=i==0?"Hello 😎":i==1?"73 📻":"👋🏽";text.sizePercent=5;text.freePosition=true;text.relativePosition={i*.4,i*.4};layers<<text;}
            const auto composed=sstv::composeTransmitLayers(source(),mode,"N0CALL",layers);
            QCOMPARE(composed.captionRects.size(),3);QCOMPARE(composed.captionRegions().size(),3);
            for(const auto& bounds:composed.captionRects){QVERIFY(!bounds.isEmpty());QVERIFY(composed.image.rect().contains(bounds));}
            QVERIFY(composed.image!=sstv::composeTransmitImage(source(),mode,"N0CALL",layers.first()).image);
            if(mode.family==sstv::Family::Mono)for(int y=0;y<mode.height;y+=7)for(int x=0;x<mode.width;x+=7){const auto pixel=composed.image.pixel(x,y);QCOMPARE(qRed(pixel),qGreen(pixel));QCOMPARE(qGreen(pixel),qBlue(pixel));}
        }
    }
    void overlappingBlocksSelectTheTopLayer(){
        QList<sstv::TextOverlay> layers;
        for(int i=0;i<12;++i){sstv::TextOverlay text;text.text="73 📻";text.freePosition=true;text.relativePosition={.3,.4};layers<<text;}
        const auto composed=sstv::composeTransmitLayers(source(),*sstv::modeById("martin2"),{},layers);
        CaptionPreview preview;preview.setImage(composed.image);preview.setTextRegions(composed.captionRegions(),sstv::captionHitOrder(layers));preview.resize(800,600);preview.show();QApplication::processEvents();
        const auto point=preview.textWidgetRect("caption:11").center().toPoint();QCOMPARE(preview.textKeyAt(point),QString("caption:11"));
        QString moved;preview.textMoved=[&](const QString& key,QPointF){moved=key;};drag(&preview,point,point+QPoint(40,30));QCOMPARE(moved,QString("caption:11"));
    }
    void captionPaintsOverCallsignInEveryMode(){
        for(const auto& mode:sstv::modes()){
            QImage black(mode.width,mode.height,QImage::Format_RGB32);black.fill(Qt::black);QImage solid=black;solid.fill(Qt::green);const auto green=sstv::fitImage(solid,mode).pixel(0,0);
            sstv::CallsignPlacement call;call.freePosition=true;call.relativePosition={.2,.3};
            sstv::TextOverlay cover;cover.text=QString(8,QChar(0x2588));cover.color=Qt::green;cover.outline=false;cover.sizePercent=18;cover.freePosition=true;cover.relativePosition=call.relativePosition;
            const auto base=sstv::composeTransmitImage(black,mode,"N0CALL",{},call);const auto onlyText=sstv::composeTransmitImage(black,mode,{},cover);const auto both=sstv::composeTransmitImage(black,mode,"N0CALL",cover,call);
            QVERIFY(both.captionBounds.intersects(both.callsignBounds));int covered=0;
            for(int y=base.callsignBounds.top();y<=base.callsignBounds.bottom();++y)for(int x=base.callsignBounds.left();x<=base.callsignBounds.right();++x){
                const auto callPixel=base.image.pixel(x,y);
                if(qRed(callPixel)>240&&qGreen(callPixel)>240&&qBlue(callPixel)>240&&onlyText.image.pixel(x,y)==green){QCOMPARE(both.image.pixel(x,y),green);++covered;}
            }
            QVERIFY2(covered>0,qPrintable(mode.id));
        }
    }
    void dragOverTextAndCallsign_data(){QTest::addColumn<bool>("useTemplate");QTest::newRow("picture")<<false;QTest::newRow("template")<<true;}
    void dragOverTextAndCallsign(){
        QFETCH(bool,useTemplate);QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-drag-device");
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));sstv::TemplateFields fields;
        if(useTemplate)editTemplate(window,[&](TemplatesDialog* dialog){dialog->findChild<QLineEdit*>("templateCity")->setText("Texarkana");fields=dialog->fields();dialog->findChild<QPushButton*>("templateApply")->click();});
        QList<sstv::TextOverlay> layers;
        for(int i=0;i<2;++i)editCaption(window,[&](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText(i==0?"MOVE 😎":"OTHER 📻");dialog->findChild<QSpinBox*>("compositionSize")->setValue(9);dialog->findChild<QComboBox*>("compositionPosition")->setCurrentIndex(i==0?4:8);dialog->findChild<QComboBox*>("compositionColor")->setCurrentIndex(i==0?4:2);layers<<dialog->overlay();dialog->accept();});
        auto* preview=window.findChild<CaptionPreview*>("transmitImage");const auto call=preview->textWidgetRect("callsign");const auto first=preview->captionWidgetRect();const auto other=preview->textWidgetRect("caption:1");
        drag(preview,first.center().toPoint(),(first.center()+other.topLeft()-first.topLeft()).toPoint());
        auto overlap=preview->captionWidgetRect().intersected(preview->textWidgetRect("caption:1"));QVERIFY(!overlap.isEmpty());QCOMPARE(preview->textKeyAt(overlap.center()),QString("caption"));QCOMPARE(preview->textWidgetRect("callsign"),call);
        window.findChild<QPushButton*>("transmitFullScreenButton")->click();auto* fullscreen=window.findChild<QWidget*>("transmitFullScreenWindow");auto* full=window.findChild<CaptionPreview*>("transmitFullScreenImage");QTRY_VERIFY(fullscreen->isVisible());QTest::qWait(30);
        const auto fullCall=full->textWidgetRect("callsign");const auto old=full->captionWidgetRect();drag(full,old.center().toPoint(),(old.center()+fullCall.topLeft()-old.topLeft()).toPoint());
        overlap=full->captionWidgetRect().intersected(fullCall);QVERIFY(!overlap.isEmpty());QCOMPARE(full->textKeyAt(overlap.center()),QString("caption"));QCOMPARE(full->textWidgetRect("callsign"),fullCall);
        captionMenu(full,"caption","Edit text & emoji…",[&](CompositionDialog* dialog){layers[0]=dialog->overlay();dialog->reject();});QTRY_VERIFY(fullscreen->isActiveWindow());
        const auto second=full->textWidgetRect("caption:1");drag(full,second.center().toPoint(),(second.center()+fullCall.topLeft()-second.topLeft()).toPoint());
        overlap=full->captionWidgetRect().intersected(full->textWidgetRect("caption:1")).intersected(fullCall);QVERIFY(!overlap.isEmpty());QCOMPARE(full->textKeyAt(overlap.center()),QString("caption:1"));
        captionMenu(full,"caption:1","Edit text & emoji…",[&](CompositionDialog* dialog){layers[1]=dialog->overlay();dialog->reject();});QTRY_VERIFY(fullscreen->isActiveWindow());
        const auto* selected=sstv::modeById("robot36");const auto base=useTemplate?sstv::renderTemplate(fields,{320,240},"N0CALL"):sstv::testPattern();const auto expected=(sstv::composeTransmitLayers(base,*selected,useTemplate?QString{}:"N0CALL",layers).image);
        const auto screenshot=qEnvironmentVariable("OPENSSTV_OVERLAP_PREVIEW");if(!useTemplate&&!screenshot.isEmpty())QVERIFY(fullscreen->grab().save(screenshot));
        QTest::keyClick(fullscreen,Qt::Key_Space);const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));
        QTRY_VERIFY(window.findChild<QPushButton*>("playButton")->isEnabled());QTRY_VERIFY(!fullscreen->isVisible());window.close();
    }
    void addingIndependentTextBlocks_data(){QTest::addColumn<bool>("useTemplate");QTest::newRow("picture")<<false;QTest::newRow("template")<<true;}
    void addingIndependentTextBlocks(){
        QFETCH(bool,useTemplate);QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-drag-device");
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));sstv::TemplateFields fields;
        if(useTemplate)editTemplate(window,[&](TemplatesDialog* dialog){dialog->findChild<QLineEdit*>("templateCity")->setText("Texarkana");fields=dialog->fields();dialog->findChild<QPushButton*>("templateApply")->click();});
        auto* preview=window.findChild<CaptionPreview*>("transmitImage");QList<sstv::TextOverlay> layers;
        const QStringList messages={"Hello from Texarkana, TX 😎","73! 📻","👋🏽"};
        for(int i=0;i<messages.size();++i){
            const auto before=preview->captionBounds();
            editCaption(window,[&](CompositionDialog* dialog){
                auto* text=dialog->findChild<QPlainTextEdit*>("compositionText");QVERIFY(text->toPlainText().isEmpty());QVERIFY(!dialog->overlay().freePosition);QVERIFY(dialog->findChild<CaptionPreview*>("compositionPreview")->captionBounds().isEmpty());
                text->setPlainText(messages[i]);dialog->findChild<QSpinBox*>("compositionSize")->setValue(i==0?5:8);dialog->findChild<QComboBox*>("compositionPosition")->setCurrentIndex(i==0?0:i==1?8:4);layers<<dialog->overlay();dialog->accept();
            });
            QVERIFY(!preview->textWidgetRect(sstv::captionKey(i)).isEmpty());if(i>0)QCOMPARE(preview->captionBounds(),before);
        }
        const auto call=preview->textWidgetRect("callsign");const auto second=preview->textWidgetRect("caption:1");const auto third=preview->textWidgetRect("caption:2");const auto first=preview->captionWidgetRect();
        drag(preview,first.center().toPoint(),first.center().toPoint()+QPoint(0,preview->imageRect().height()/5));
        QVERIFY(preview->captionWidgetRect()!=first);QCOMPARE(preview->textWidgetRect("caption:1"),second);QCOMPARE(preview->textWidgetRect("caption:2"),third);QCOMPARE(preview->textWidgetRect("callsign"),call);
        captionMenu(preview,"caption","Edit text & emoji…",[&](CompositionDialog* dialog){QCOMPARE(dialog->overlay().text,messages[0]);layers[0]=dialog->overlay();dialog->reject();});
        // A blank Apply and a cancelled new message leave every placed block intact.
        editCaption(window,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());dialog->accept();});QVERIFY(preview->textWidgetRect("caption:3").isEmpty());
        editCaption(window,[](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("DO NOT SEND");dialog->reject();});QVERIFY(preview->textWidgetRect("caption:3").isEmpty());
        window.findChild<QPushButton*>("transmitFullScreenButton")->click();auto* fullscreen=window.findChild<QWidget*>("transmitFullScreenWindow");auto* full=window.findChild<CaptionPreview*>("transmitFullScreenImage");QTRY_VERIFY(fullscreen->isVisible());QTest::qWait(30);
        const auto fullFirst=full->captionWidgetRect();const auto fullSecond=full->textWidgetRect("caption:1");const auto fullThird=full->textWidgetRect("caption:2");
        drag(full,fullSecond.center().toPoint(),fullSecond.center().toPoint()-QPoint(full->imageRect().width()/5,full->imageRect().height()/8));
        QVERIFY(full->textWidgetRect("caption:1")!=fullSecond);QCOMPARE(full->captionWidgetRect(),fullFirst);QCOMPARE(full->textWidgetRect("caption:2"),fullThird);
        captionMenu(full,"caption:1","Edit text & emoji…",[&](CompositionDialog* dialog){QCOMPARE(dialog->overlay().text,messages[1]);layers[1]=dialog->overlay();dialog->reject();});
        QTRY_VERIFY(fullscreen->isActiveWindow());QTest::keyClick(fullscreen,Qt::Key_Escape);QTRY_VERIFY(!fullscreen->isVisible());
        // Changing just one block preserves the other two and their drawing order.
        captionMenu(preview,"caption:2","Edit text & emoji…",[&](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("👋🏽 ❤️");layers[2]=dialog->overlay();dialog->accept();});
        if(useTemplate){
            editTemplate(window,[&](TemplatesDialog* dialog){
                QCOMPARE(dialog->captionOverlays().size(),3);QCOMPARE(dialog->captionOverlays()[1].relativePosition,layers[1].relativePosition);
                auto* card=dialog->findChild<CaptionPreview*>("templatePreview");const auto old=card->textWidgetRect("caption:2");drag(card,old.center().toPoint(),old.center().toPoint()+QPoint(card->imageRect().width()/5,card->imageRect().height()/8));
                QVERIFY(card->textWidgetRect("caption:2")!=old);layers[2]=dialog->captionOverlays()[2];dialog->findChild<QPushButton*>("templateApply")->click();
            });
            editTemplate(window,[](TemplatesDialog* dialog){dialog->findChild<QPushButton*>("templateClear")->click();QVERIFY(dialog->captionOverlays().isEmpty());dialog->reject();});
        }
        const auto* selected=sstv::modeById("robot36");const auto base=useTemplate?sstv::renderTemplate(fields,{320,240},"N0CALL"):sstv::testPattern();const auto composed=sstv::composeTransmitLayers(base,*selected,useTemplate?QString{}:"N0CALL",layers);
        const auto expected=(composed.image);
        const auto screenshot=qEnvironmentVariable("OPENSSTV_MULTI_TEXT_PREVIEW");if(!useTemplate&&!screenshot.isEmpty())QVERIFY(window.grab().save(screenshot));
        auto* send=window.findChild<QPushButton*>("playButton");send->click();for(int i=0;i<3;++i)QVERIFY(preview->textWidgetRect(sstv::captionKey(i)).isEmpty());QVERIFY(!window.findChild<QPushButton*>("saveTransmitImageButton")->isEnabled());QVERIFY(!window.findChild<QAction*>("saveTransmitImageAction")->isEnabled());
        const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));QTRY_VERIFY(send->isEnabled());
        captionMenu(preview,"caption:1","Remove text block");QVERIFY(preview->textWidgetRect("caption:2").isEmpty());
        captionMenu(preview,"caption:1","Edit text & emoji…",[&](CompositionDialog* dialog){QCOMPARE(dialog->overlay().text,layers[2].text);dialog->reject();});
        captionMenu(preview,"caption:1","Edit text & emoji…",[](CompositionDialog* dialog){dialog->findChild<QPushButton*>("compositionClear")->click();dialog->accept();});
        QVERIFY(preview->textWidgetRect("caption:1").isEmpty());QVERIFY(!preview->captionWidgetRect().isEmpty());
        selectOriginalPattern(window);for(int i=0;i<3;++i)QVERIFY(preview->textWidgetRect(sstv::captionKey(i)).isEmpty());window.close();
        MainWindow reopened;reopened.show();QVERIFY(reopened.findChild<CaptionPreview*>("transmitImage")->captionWidgetRect().isEmpty());reopened.close();
    }
    void fullscreenPointerShowsForThreeSecondsAfterMovement(){
        QSettings settings;settings.setValue("station/call","N0CALL");MainWindow window;window.show();
        auto* button=window.findChild<QPushButton*>("transmitFullScreenButton");button->click();auto* fullscreen=window.findChild<QWidget*>("transmitFullScreenWindow");auto* preview=window.findChild<CaptionPreview*>("transmitFullScreenImage");
        QTRY_VERIFY(fullscreen->isVisible());QTest::qWait(30);QCOMPARE(fullscreen->cursor().shape(),Qt::BlankCursor);QCOMPARE(preview->cursor().shape(),Qt::BlankCursor);
        hover(preview,{20,20});hover(preview,preview->textWidgetRect("callsign").center().toPoint());QTRY_VERIFY_WITH_TIMEOUT(preview->cursor().shape()!=Qt::BlankCursor,1000);QCOMPARE(fullscreen->cursor().shape(),Qt::ArrowCursor);
        QTest::qWait(1600);auto* progress=window.findChild<QLabel*>("transmitProgressInfo");QVERIFY(fullscreen->isAncestorOf(progress));hover(progress,progress->rect().center());QCOMPARE(fullscreen->cursor().shape(),Qt::ArrowCursor);
        QTest::qWait(1600);QCOMPARE(fullscreen->cursor().shape(),Qt::ArrowCursor); // New movement restarted the full three seconds.
        QTRY_COMPARE_WITH_TIMEOUT(fullscreen->cursor().shape(),Qt::BlankCursor,2000);QCOMPARE(preview->cursor().shape(),Qt::BlankCursor);
        hover(preview,preview->imageRect().center());QCOMPARE(fullscreen->cursor().shape(),Qt::ArrowCursor);QTest::keyClick(fullscreen,Qt::Key_Escape);QTRY_VERIFY(!fullscreen->isVisible());
        button->click();QTRY_VERIFY(fullscreen->isVisible());QCOMPARE(fullscreen->cursor().shape(),Qt::BlankCursor);QCOMPARE(preview->cursor().shape(),Qt::BlankCursor);window.close();
    }
    void sendingWithVisiblePointerExcludesCursorAndGuide(){
        QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-drag-device");MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));
        const auto expected=(sstv::composeTransmitImage(sstv::testPattern(),*sstv::modeById("robot36"),"N0CALL").image);
        window.findChild<QPushButton*>("transmitFullScreenButton")->click();auto* fullscreen=window.findChild<QWidget*>("transmitFullScreenWindow");auto* preview=window.findChild<CaptionPreview*>("transmitFullScreenImage");QTRY_VERIFY(fullscreen->isActiveWindow());QTest::qWait(30);
        hover(preview,{20,20});hover(preview,preview->textWidgetRect("callsign").center().toPoint());QTRY_VERIFY_WITH_TIMEOUT(preview->cursor().shape()!=Qt::BlankCursor,1000); // The visible pointer and selection guide are over the callsign.
        QTest::keyClick(fullscreen,Qt::Key_Space);QVERIFY(preview->cursor().shape()!=Qt::BlankCursor);const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);
        const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));QTRY_VERIFY(window.findChild<QPushButton*>("playButton")->isEnabled());QTRY_VERIFY(!fullscreen->isVisible());window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){
    QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);DragTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "drag_tests.moc"
