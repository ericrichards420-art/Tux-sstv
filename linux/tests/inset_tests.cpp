#include "mainwindow.h"
#include "insetdialog.h"
#include "captionpreview.h"
#include "composer.h"
#include "templatesdialog.h"
#include <QtTest>
#include <QtWidgets>
#include <cmath>
#include <limits>
#include <QScopeGuard>

class InsetTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    QString receivedPath_,otherPath_;
    QImage received_,other_;
    static QByteArray bytes(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};}
    static void drag(CaptionPreview* view,QPoint from,QPoint to){QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(view,to,10);QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,to);QApplication::processEvents();}
    static void modal(MainWindow& window,const std::function<void()>& start,const std::function<void(QDialog*)>& edit,const QString& menuAction={}){
        bool seen=false;QTimer editor,menu;editor.setInterval(20);menu.setInterval(15);
        connect(&editor,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog||qobject_cast<QMenu*>(dialog))return;editor.stop();seen=true;const auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});edit(dialog);});
        connect(&menu,&QTimer::timeout,&window,[&]{auto* popup=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!popup)return;for(auto* action:popup->actions())if(action->text()==menuAction){menu.stop();popup->setActiveAction(action);QTest::keyClick(popup,Qt::Key_Return);return;}});
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();if(auto* popup=QApplication::activePopupWidget())popup->close();});deadline.start(5000);editor.start();if(!menuAction.isEmpty())menu.start();start();QVERIFY(seen);
    }
    static void editInset(MainWindow& window,const std::function<void(InsetDialog*)>& edit){modal(window,[&]{window.findChild<QAction*>("pictureInPictureAction")->trigger();},[&](QDialog* dialog){auto* inset=qobject_cast<InsetDialog*>(dialog);QVERIFY(inset);edit(inset);});}
    static void editCaption(MainWindow& window,const std::function<void(CompositionDialog*)>& edit){modal(window,[&]{window.findChild<QPushButton*>("compositionButton")->click();},[&](QDialog* dialog){auto* caption=qobject_cast<CompositionDialog*>(dialog);QVERIFY(caption);edit(caption);});}
    static void showReceived(MainWindow& window,const QString& path){
        auto* history=window.findChild<QListWidget*>("receiveHistory");QListWidgetItem* selected=nullptr;for(int i=0;i<history->count();++i)if(history->item(i)->data(Qt::UserRole).toString()==path)selected=history->item(i);QVERIFY(selected);QVERIFY(QMetaObject::invokeMethod(history,"itemDoubleClicked",Q_ARG(QListWidgetItem*,selected)));QVERIFY(window.findChild<QPushButton*>("addReceivedInsetButton")->isEnabled());
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVInsetTests");QCoreApplication::setApplicationName("OpenSSTVInsetTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        const auto history=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)+"/OpenSSTV";QVERIFY(QDir().mkpath(history));receivedPath_=history+"/received-test.png";otherPath_=history+"/other-test.png";
        received_=sstv::testPattern().scaled({160,120},Qt::IgnoreAspectRatio,Qt::SmoothTransformation);{QPainter p(&received_);p.setPen(Qt::black);for(int y:{22,24,60,97})p.drawLine(0,y,159,y);p.fillRect(QRect(110,30,50,10),QColor("#80765a"));p.setPen(Qt::white);p.setFont(QFont("DejaVu Sans",12,QFont::Bold));p.drawText(QRect(5,4,150,20),"K1ABC");}received_.setText("Mode","martin2");received_.setText("Callsign","K1ABC");received_.setText("ReceivedUTC","2026-11-21T12:34:56.000Z");QVERIFY(received_.save(receivedPath_));
        other_=QImage(160,120,QImage::Format_RGB32);other_.fill(Qt::blue);QVERIFY(other_.save(otherPath_));
        oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_INSET_WAV",directory_.filePath("tx.wav").toUtf8());QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write("#!/usr/bin/python3\nimport os,sys,shutil,time\np=os.environ['OPENSSTV_INSET_WAV']\nshutil.copyfile(sys.argv[-1],p+'.tmp')\nos.replace(p+'.tmp',p)\ntime.sleep(.2)\n");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings settings;settings.clear();QFile::remove(directory_.filePath("tx.wav"));}
    void insetFitsEveryModeAndPreservesSources(){
        const auto original=received_;
        for(const auto& mode:sstv::modes()){
            const auto source=sstv::testPattern();const auto base=sstv::composeTransmitImage(source,mode,"N0CALL").image;
            for(const bool frame:{false,true})for(const double fraction:{.1,.35,1.})for(const auto point:{QPointF(0,0),QPointF(1,1)}){
                sstv::ImageInset inset;inset.image=received_;inset.frame=frame;inset.widthFraction=fraction;inset.relativePosition=point;
                const auto composed=sstv::composeTransmitLayers(source,mode,"N0CALL",{},{},inset);QVERIFY(!composed.insetBounds.isEmpty());QVERIFY(composed.image.rect().contains(composed.insetBounds));
                QVERIFY(std::abs(composed.insetBounds.width()/double(received_.width())-composed.insetBounds.height()/double(received_.height()))<.02);
                if(point==QPointF(0,0))QCOMPARE(composed.insetBounds.topLeft(),QPoint(0,0));else QCOMPARE(composed.insetBounds.bottomRight(),composed.image.rect().bottomRight());
                auto restored=composed.image;{QPainter p(&restored);p.drawImage(composed.insetBounds,base,composed.insetBounds);}QCOMPARE(restored,base);QCOMPARE(inset.image,original);QCOMPARE(source,sstv::testPattern());
            }
            sstv::ImageInset portrait;portrait.image=QImage(1,1000,QImage::Format_RGB32);portrait.image.fill(Qt::green);portrait.widthFraction=std::numeric_limits<double>::quiet_NaN();portrait.relativePosition={std::numeric_limits<double>::infinity(),-100};const auto image=sstv::composeTransmitLayers(source,mode,{}, {},{},portrait);QVERIFY(!image.insetBounds.isEmpty());QVERIFY(image.image.rect().contains(image.insetBounds));
        }
    }
    void insetKeepsAllFourSourceCorners(){
        for(const auto* id:{"martin2","bw8","robot36"})for(const bool frame:{false,true}){
            const auto& mode=*sstv::modeById(id);QImage background(mode.width,mode.height,QImage::Format_RGB32);background.fill(Qt::black);QImage image(120,80,QImage::Format_RGB32);{QPainter p(&image);p.fillRect(QRect(0,0,60,40),Qt::red);p.fillRect(QRect(60,0,60,40),Qt::green);p.fillRect(QRect(0,40,60,40),Qt::blue);p.fillRect(QRect(60,40,60,40),Qt::yellow);}
            sstv::ImageInset inset;inset.image=image;inset.widthFraction=.5;inset.frame=frame;const auto composed=sstv::composeTransmitLayers(background,mode,{},{},{},inset);int i=0;
            for(const auto color:{Qt::red,Qt::green,Qt::blue,Qt::yellow}){QImage solid(mode.width,mode.height,QImage::Format_RGB32);solid.fill(color);const auto expected=sstv::fitImage(solid,mode).pixel(0,0);const auto r=composed.insetBounds;const QPoint point(r.x()+r.width()*(i%2?3:1)/4,r.y()+r.height()*(i/2?3:1)/4);QCOMPARE(composed.image.pixel(point),expected);++i;}
        }
    }
    void receivedInsetThroughSaveAndTransmission_data(){
        QTest::addColumn<QString>("modeId");QTest::addColumn<QString>("profile");QTest::addColumn<bool>("useTemplate");
        QTest::newRow("robot-color")<<QString("robot36")<<QString("standard")<<false;
        QTest::newRow("robot-template")<<QString("robot36")<<QString("default")<<true;
        QTest::newRow("martin-color")<<QString("martin2")<<QString("default")<<false;
        QTest::newRow("weather-card")<<QString("robot36")<<QString("default")<<true;
        QTest::newRow("legacy-mono")<<QString("bw8")<<QString("default")<<false;
    }
    void receivedInsetThroughSaveAndTransmission(){
        QFETCH(QString,modeId);QFETCH(QString,profile);QFETCH(bool,useTemplate);const auto receivedBytes=bytes(receivedPath_);
        QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-inset-device");MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData(modeId));sstv::TemplateFields fields;
        if(useTemplate)modal(window,[&]{window.findChild<QPushButton*>("templatesButton")->click();},[&](QDialog* dialog){auto* card=qobject_cast<TemplatesDialog*>(dialog);QVERIFY(card);card->findChild<QLineEdit*>("templateCity")->setText("Texarkana");card->findChild<QLineEdit*>("templateState")->setText("TX");card->findChild<QLineEdit*>("templateTemperature")->setText("72");fields=card->fields();card->findChild<QPushButton*>("templateApply")->click();});
        sstv::TextOverlay caption;editCaption(window,[&](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("Your picture as received 📻");dialog->findChild<QSpinBox*>("compositionSize")->setValue(5);dialog->findChild<QComboBox*>("compositionPosition")->setCurrentIndex(7);caption=dialog->overlay();dialog->accept();});
        QVERIFY(!window.findChild<QPushButton*>("addReceivedInsetButton")->isEnabled());showReceived(window,receivedPath_);sstv::ImageInset inset;
        modal(window,[&]{window.findChild<QPushButton*>("addReceivedInsetButton")->click();},[&](QDialog* dialog){auto* editor=qobject_cast<InsetDialog*>(dialog);QVERIFY(editor);QCOMPARE(editor->inset().image,received_);editor->findChild<QSpinBox*>("insetWidthPercent")->setValue(45);QCOMPARE(editor->findChild<QSlider*>("insetWidthSlider")->value(),45);editor->findChild<QCheckBox*>("insetFrame")->setChecked(false);inset=editor->inset();const auto shot=qEnvironmentVariable("OPENSSTV_INSET_EDITOR_PREVIEW");if(useTemplate&&!shot.isEmpty())QVERIFY(editor->grab().save(shot));editor->findChild<QPushButton*>("insetApply")->click();});
        auto* preview=window.findChild<CaptionPreview*>("transmitImage");const auto captionBounds=preview->captionBounds();const auto old=preview->textWidgetRect("inset");QVERIFY(!old.isEmpty());drag(preview,old.center().toPoint(),old.center().toPoint()-QPoint(preview->imageRect().width()/5,preview->imageRect().height()/5));QVERIFY(preview->textWidgetRect("inset")!=old);QCOMPARE(preview->captionBounds(),captionBounds);
        editInset(window,[&](InsetDialog* editor){inset=editor->inset();QCOMPARE(inset.image,received_);editor->findChild<QSpinBox*>("insetWidthPercent")->setValue(90);editor->reject();});
        // A new decoded RX frame must not replace the already placed snapshot.
        const auto incoming=directory_.filePath("incoming.wav");sstv::writeWav(incoming,sstv::encode((other_),*sstv::modeById("robot36"),12000));window.openReceiveWav(incoming);QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton*>("decodeFileButton")->isEnabled(),5000);
        editInset(window,[&](InsetDialog* editor){QCOMPARE(editor->inset().image,received_);QCOMPARE(editor->inset().widthFraction,.45);editor->findChild<QPushButton*>("insetUseCurrentRx")->click();QVERIFY(editor->inset().image!=received_);editor->reject();});
        window.findChild<QTabWidget*>("imageTabs")->setCurrentIndex(1);window.findChild<QPushButton*>("transmitFullScreenButton")->click();auto* fullWindow=window.findChild<QWidget*>("transmitFullScreenWindow");auto* full=window.findChild<CaptionPreview*>("transmitFullScreenImage");QTRY_VERIFY(fullWindow->isActiveWindow());QTest::qWait(30);const auto fullRect=full->textWidgetRect("inset");QVERIFY(!fullRect.isEmpty());drag(full,fullRect.center().toPoint(),fullRect.center().toPoint()-QPoint(full->imageRect().width()/10,full->imageRect().height()/12));
        modal(window,[&]{QVERIFY(QMetaObject::invokeMethod(full,"customContextMenuRequested",Q_ARG(QPoint,full->textWidgetRect("inset").center().toPoint())));},[&](QDialog* dialog){auto* editor=qobject_cast<InsetDialog*>(dialog);QVERIFY(editor);inset=editor->inset();QCOMPARE(inset.image,received_);QVERIFY(!inset.frame);editor->reject();},"Picture in picture…");QTRY_VERIFY(fullWindow->isActiveWindow());
        const auto* selected=sstv::modeById(modeId);const auto base=useTemplate?sstv::renderTemplate(fields,{selected->width,selected->height},"N0CALL"):sstv::testPattern();const auto composed=sstv::composeTransmitLayers(base,*selected,useTemplate?QString{}:"N0CALL",{caption},{},inset);const auto expected=composed.image;
        const auto png=directory_.filePath(QString("saved-%1-%2-%3.png").arg(modeId,profile,useTemplate?"card":"photo"));
        modal(window,[&]{QTest::keyClick(fullWindow,Qt::Key_S,Qt::ControlModifier|Qt::ShiftModifier);},[&](QDialog* dialog){auto* file=qobject_cast<QFileDialog*>(dialog);QVERIFY(file);file->selectFile(png);file->findChild<QLineEdit*>("fileNameEdit")->setText(png);QVERIFY(QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection));});QCOMPARE(QImage(png),expected);QTRY_VERIFY(fullWindow->isActiveWindow());
        const auto artifact=qEnvironmentVariable("OPENSSTV_INSET_PREVIEW");if(useTemplate&&!artifact.isEmpty())QVERIFY(fullWindow->grab().save(artifact));
        QTest::keyClick(fullWindow,Qt::Key_Space);QVERIFY(full->textWidgetRect("inset").isEmpty());QVERIFY(!window.findChild<QAction*>("pictureInPictureAction")->isEnabled());QVERIFY(!window.findChild<QPushButton*>("addReceivedInsetButton")->isEnabled());
        const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);
        {const auto without=sstv::composeTransmitLayers(base,*selected,"N0CALL",{caption}).image;double withInset=0,withoutInset=0;for(int y=composed.insetBounds.top();y<=composed.insetBounds.bottom();++y)for(int x=composed.insetBounds.left();x<=composed.insetBounds.right();++x){const auto p=decoded.image.pixel(x,y);withInset+=std::abs(qRed(p)-qRed(expected.pixel(x,y)));withoutInset+=std::abs(qRed(p)-qRed(without.pixel(x,y)));}QVERIFY(withInset<withoutInset*.5);}
        QTRY_VERIFY(window.findChild<QPushButton*>("playButton")->isEnabled());QTRY_VERIFY(!fullWindow->isVisible());QVERIFY(!preview->textWidgetRect("inset").isEmpty());QCOMPARE(bytes(receivedPath_),receivedBytes);
        editInset(window,[](InsetDialog* editor){editor->findChild<QPushButton*>("insetRemove")->click();});QVERIFY(preview->textWidgetRect("inset").isEmpty());QCOMPARE(preview->captionBounds(),captionBounds);window.close();
    }
    void historySelectionCancelClearAndNewPicture(){
        MainWindow window;window.show();showReceived(window,otherPath_);auto* history=window.findChild<QListWidget*>("receiveHistory");auto* galleries=window.findChild<QTabWidget*>("galleryTabs");galleries->setCurrentIndex(1);QListWidgetItem* old=nullptr;for(int i=0;i<history->count();++i)if(history->item(i)->data(Qt::UserRole).toString()==receivedPath_)old=history->item(i);QVERIFY(old);history->scrollToItem(old);
        const auto start=[&]{QVERIFY(QMetaObject::invokeMethod(history,"customContextMenuRequested",Q_ARG(QPoint,history->visualItemRect(old).center())));};
        modal(window,start,[&](QDialog* dialog){auto* editor=qobject_cast<InsetDialog*>(dialog);QVERIFY(editor);QCOMPARE(editor->inset().image,received_);editor->reject();},"Add to TX as inset…");auto* preview=window.findChild<CaptionPreview*>("transmitImage");QVERIFY(preview->textWidgetRect("inset").isEmpty());
        modal(window,start,[&](QDialog* dialog){auto* editor=qobject_cast<InsetDialog*>(dialog);QVERIFY(editor);QCOMPARE(editor->inset().image,received_);editor->findChild<QPushButton*>("insetApply")->click();},"Add to TX as inset…");QVERIFY(!preview->textWidgetRect("inset").isEmpty());
        modal(window,[&]{window.findChild<QPushButton*>("templatesButton")->click();},[&](QDialog* dialog){auto* editor=qobject_cast<TemplatesDialog*>(dialog);QVERIFY(editor);QCOMPARE(editor->imageInset().image,received_);editor->findChild<QPushButton*>("templateClear")->click();QVERIFY(editor->imageInset().image.isNull());editor->reject();});QVERIFY(!preview->textWidgetRect("inset").isEmpty());
        modal(window,[&]{window.findChild<QPushButton*>("testPatternsButton")->click();},[&](QDialog* dialog){auto* button=dialog->findChild<QPushButton*>("useTestPattern");QVERIFY(button);button->click();});QVERIFY(preview->textWidgetRect("inset").isEmpty());window.close();MainWindow reopened;QVERIFY(reopened.findChild<CaptionPreview*>("transmitImage")->textWidgetRect("inset").isEmpty());reopened.close();
    }
    void emptyEditorCanLoadSavedImage(){
        MainWindow window;window.show();editInset(window,[&](InsetDialog* editor){QVERIFY(editor->inset().image.isNull());QVERIFY(!editor->findChild<QPushButton*>("insetApply")->isEnabled());QTimer choose;choose.setInterval(20);connect(&choose,&QTimer::timeout,editor,[&]{auto* file=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!file)return;choose.stop();file->selectFile(receivedPath_);file->findChild<QLineEdit*>("fileNameEdit")->setText(receivedPath_);QVERIFY(QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection));});choose.start();editor->findChild<QPushButton*>("insetChooseImage")->click();QCOMPARE(editor->inset().image,received_);QVERIFY(editor->findChild<QPushButton*>("insetApply")->isEnabled());editor->findChild<QPushButton*>("insetApply")->click();});QVERIFY(!window.findChild<CaptionPreview*>("transmitImage")->textWidgetRect("inset").isEmpty());window.close();
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);InsetTests tests;return QTest::qExec(&tests,argc,argv);}
#include "inset_tests.moc"
