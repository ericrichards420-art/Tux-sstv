#include "mainwindow.h"
#include "captionpreview.h"
#include "composer.h"
#include "templatesdialog.h"
#include <QtTest>
#include <QtWidgets>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>

class TxImageTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    static QByteArray bytes(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};}
    static void drag(CaptionPreview* view,QPoint from,QPoint to){QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(view,to,10);QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,to);QApplication::processEvents();}
    static void compose(MainWindow& window,const std::function<void(CompositionDialog*)>& edit){
        bool seen=false;QTimer poll;poll.setInterval(20);connect(&poll,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;poll.stop();seen=true;edit(dialog);});poll.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        window.findChild<QPushButton*>("compositionButton")->click();QVERIFY(seen);
    }
    static void save(MainWindow& window,const QString& path,int trigger=0,bool cancel=false,QMessageBox::StandardButton replace=QMessageBox::No,bool expectReplace=false){
        bool seen=false,confirmed=false;QTimer poll;poll.setInterval(20);
        connect(&poll,&QTimer::timeout,&window,[&]{
            if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){
                if(seen)return;seen=true;QCOMPARE(dialog->objectName(),QString("saveTransmitImageDialog"));QCOMPARE(dialog->acceptMode(),QFileDialog::AcceptSave);QCOMPARE(dialog->defaultSuffix(),QString("png"));QVERIFY(dialog->selectedFiles().value(0).endsWith(".png"));
                if(cancel)dialog->reject();else {dialog->selectFile(path);dialog->findChild<QLineEdit*>("fileNameEdit")->setText(path);static_cast<QDialog*>(dialog)->accept();}
            }else if(auto* question=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){confirmed=true;QTest::mouseClick(question->button(replace),Qt::LeftButton);}
        });poll.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        if(trigger==1)window.findChild<QAction*>("saveTransmitImageAction")->trigger();
        else if(trigger==2){window.activateWindow();QTRY_VERIFY(window.isActiveWindow());QTest::keyClick(&window,Qt::Key_S,Qt::ControlModifier|Qt::ShiftModifier);}
        else if(trigger==3){auto* full=window.findChild<QWidget*>("transmitFullScreenWindow");if(!full->isVisible())window.findChild<QPushButton*>("transmitFullScreenButton")->click();QTRY_VERIFY(full->isActiveWindow());QTest::keyClick(full,Qt::Key_S,Qt::ControlModifier|Qt::ShiftModifier);}
        else QTest::mouseClick(window.findChild<QPushButton*>("saveTransmitImageButton"),Qt::LeftButton);
        QVERIFY(seen);QCOMPARE(confirmed,expectReplace);
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVTxImageTests");QCoreApplication::setApplicationName("OpenSSTVTxImageTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
    }
    void init(){QSettings settings;settings.clear();}
    void builtInOriginalsRemainAvailableAcrossModes_data(){
        QTest::addColumn<QString>("modeId");QTest::addColumn<QString>("galleryObject");
        for(const auto& gallery:QStringList{"tuxPictures","holidayPictures"})
            for(const auto& mode:QStringList{"bw8","scottie2","pd120"})
                QTest::newRow(qPrintable(gallery+"-"+mode))<<mode<<gallery;
    }
    void builtInOriginalsRemainAvailableAcrossModes(){
        QFETCH(QString,modeId);QFETCH(QString,galleryObject);
        const bool holiday=galleryObject=="holidayPictures";const int count=holiday?13:14;
        MainWindow window;window.show();
        auto* gallery=window.findChild<QListWidget*>(galleryObject);
        auto* tabs=window.findChild<QTabWidget*>("galleryTabs");
        auto* personal=window.findChild<QListWidget*>("stockPictures");
        auto* modes=window.findChild<QComboBox*>("transmitMode");
        QVERIFY(gallery&&tabs&&personal&&modes);QCOMPARE(gallery->count(),count);
        tabs->setCurrentWidget(gallery);QCOMPARE(tabs->tabText(tabs->indexOf(gallery)),holiday?QString("Holidays"):QString("Tux pictures"));QCOMPARE(personal->count(),0);
        QVERIFY(!window.findChild<QPushButton*>("removeStockPhotosButton")->isVisible());
        QVERIFY(window.findChild<QPushButton*>("addStockPhotosButton")->isVisible());
        QFile catalog(holiday?":/pictures/holidays/catalog.json":":/pictures/tux/catalog.json");QVERIFY(catalog.open(QIODevice::ReadOnly));
        const auto entries=QJsonDocument::fromJson(catalog.readAll()).array();QCOMPARE(entries.size(),count);
        if(holiday){
            // Nationwide annual federal holidays, checked against OPM's list.
            // Easter and Halloween are retained as two additional seasonal cards.
            const QSet<QString> expected={"New Year's Day","Birthday of Martin Luther King, Jr.","Washington's Birthday","Memorial Day","Juneteenth National Independence Day","Independence Day","Labor Day","Columbus Day","Veterans Day","Thanksgiving Day","Christmas Day"};
            QSet<QString> actual;int extras=0;
            for(const auto& entry:entries){
                const auto name=entry.toObject().value("federalHoliday").toString();
                if(name.isEmpty()){++extras;continue;}
                QVERIFY(!actual.contains(name));actual.insert(name);
            }
            QCOMPARE(actual,expected);QCOMPARE(extras,2);
        }
        QSet<QString> paths;
        for(int i=0;i<gallery->count();++i){
            auto* item=gallery->item(i);const auto path=item->data(Qt::UserRole).toString();
            QVERIFY(!paths.contains(path));paths.insert(path);QVERIFY(!item->icon().isNull());
            QFile original(path);QVERIFY(original.open(QIODevice::ReadOnly));
            QCOMPARE(QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256).toHex(),entries[i].toObject().value("sha256").toString().toLatin1());
            QCOMPARE(QImage(path).size(),QSize(1402,1122));
        }
        // Select at a low resolution, then increase it without reloading the
        // picture. Saving must use the full original rather than the thumbnail
        // or the previous mode's resized pixels.
        modes->setCurrentIndex(modes->findData("bw8"));
        auto* selected=gallery->item(holiday?8:gallery->count()-1);gallery->scrollToItem(selected);QApplication::processEvents();
        QTest::mouseClick(gallery->viewport(),Qt::LeftButton,Qt::NoModifier,gallery->visualItemRect(selected).center());
        modes->setCurrentIndex(modes->findData(modeId));
        const auto* mode=sstv::modeById(modeId);QVERIFY(mode);
        const auto expected=sstv::fitImage(QImage(selected->data(Qt::UserRole).toString()),*mode);
        const auto path=directory_.filePath("original-"+galleryObject+"-"+modeId+".png");save(window,path);
        QCOMPARE(QImage(path),expected);QCOMPARE(QImage(path).size(),QSize(mode->width,mode->height));
        QCOMPARE(gallery->count(),count);QCOMPARE(personal->count(),1);
        personal->setCurrentRow(0);window.findChild<QPushButton*>("removeStockPhotosButton")->click();
        QCOMPARE(personal->count(),0);QVERIFY(QFile::exists(path));QCOMPARE(gallery->count(),count);
        if(modeId=="pd120"){
            tabs->setCurrentWidget(gallery);gallery->scrollToItem(gallery->item(0));QApplication::processEvents();
            const auto preview=qEnvironmentVariable(holiday?"OPENSSTV_HOLIDAY_GALLERY_PREVIEW":"OPENSSTV_TUX_GALLERY_PREVIEW");if(!preview.isEmpty())QVERIFY(window.grab().save(preview));
        }
        window.close();MainWindow reopened;QCOMPARE(reopened.findChild<QListWidget*>(galleryObject)->count(),count);
        QCOMPARE(reopened.findChild<QListWidget*>("stockPictures")->count(),0);reopened.close();
    }
    void savesFinishedImage_data(){
        QTest::addColumn<QString>("modeId");QTest::addColumn<QString>("profile");QTest::addColumn<bool>("useTemplate");QTest::addColumn<int>("trigger");
        QTest::newRow("legacy-button")<<QString("martin2")<<QString("standard")<<false<<0;
        QTest::newRow("mono-menu")<<QString("bw8")<<QString("standard")<<false<<1;
        QTest::newRow("robot-color")<<QString("robot36")<<QString("standard")<<false<<0;
        QTest::newRow("robot-shortcut")<<QString("robot36")<<QString("default")<<false<<2;
        QTest::newRow("martin-fullscreen")<<QString("martin2")<<QString("default")<<true<<3;
        QTest::newRow("template-fullscreen")<<QString("robot36")<<QString("default")<<true<<3;
    }
    void savesFinishedImage(){
        QFETCH(QString,modeId);QFETCH(QString,profile);QFETCH(bool,useTemplate);QFETCH(int,trigger);
        QSettings settings;settings.setValue("station/call","N0CALL");MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData(modeId));
        sstv::TemplateFields fields;
        if(useTemplate){
            bool seen=false;QTimer::singleShot(25,&window,[&]{auto* dialog=qobject_cast<TemplatesDialog*>(QApplication::activeModalWidget());if(!dialog)return;seen=true;dialog->findChild<QLineEdit*>("templateCity")->setText("Texarkana");dialog->findChild<QLineEdit*>("templateState")->setText("TX");dialog->findChild<QLineEdit*>("templateTemperature")->setText("72");fields=dialog->fields();dialog->findChild<QPushButton*>("templateApply")->click();});window.findChild<QPushButton*>("templatesButton")->click();QVERIFY(seen);
        }
        QList<sstv::TextOverlay> layers;sstv::CallsignPlacement call;
        for(int i=0;i<2;++i)compose(window,[&](CompositionDialog* dialog){
            dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText(i==0?"Hello from Texarkana, TX 😎":"73! 📻 👋🏽");dialog->findChild<QSpinBox*>("compositionSize")->setValue(6);dialog->findChild<QComboBox*>("compositionPosition")->setCurrentIndex(i==0?4:8);
            auto* view=dialog->findChild<CaptionPreview*>("compositionPreview");
            if(i==0&&!useTemplate){const auto box=view->textWidgetRect("callsign");drag(view,box.center().toPoint(),view->imageRect().topRight()-QPoint(40,-20));call=dialog->callsignPlacement();QVERIFY(call.freePosition);}
            const auto text=view->captionWidgetRect();drag(view,text.center().toPoint(),text.center().toPoint()-QPoint(20,view->imageRect().height()/10));layers<<dialog->overlay();QVERIFY(layers.last().freePosition);dialog->accept();
        });
        const auto* selected=sstv::modeById(modeId);const auto base=useTemplate?sstv::renderTemplate(fields,{selected->width,selected->height},"N0CALL"):sstv::testPattern();const auto composed=sstv::composeTransmitLayers(base,*selected,useTemplate?QString{}:"N0CALL",layers,call);
        const auto expected=composed.image;
        auto* view=window.findChild<CaptionPreview*>("transmitImage");const auto first=view->captionBounds();
        auto* button=window.findChild<QPushButton*>("saveTransmitImageButton");QVERIFY(button->isVisible());QVERIFY(button->isEnabled());window.resize(900,680);QApplication::processEvents();QVERIFY(window.rect().contains(QRect(button->mapTo(&window,QPoint(0,0)),button->size())));
        const auto name=directory_.filePath(QString("finished-%1-%2-%3").arg(modeId,profile,useTemplate?"template":"picture"));const auto path=name+".png";save(window,name,trigger);
        QVERIFY(QFile::exists(path));const QImage saved(path);QCOMPARE(saved,expected);QCOMPARE(saved.text("Mode"),modeId);QCOMPARE(saved.text("Callsign"),QString("N0CALL"));QVERIFY(QDateTime::fromString(saved.text("SavedUTC"),Qt::ISODateWithMs).isValid());
        QCOMPARE(view->captionBounds(),first);QVERIFY(!view->textWidgetRect("caption:1").isEmpty());QVERIFY(!view->textWidgetRect("callsign").isEmpty());
        auto* stock=window.findChild<QListWidget*>("stockPictures");QCOMPARE(stock->count(),1);QCOMPARE(stock->item(0)->data(Qt::UserRole).toString(),path);QCOMPARE(settings.value("tx/imageSaveDirectory").toString(),directory_.path());
        const auto before=bytes(path);save(window,path,trigger==3?3:0,true);QCOMPARE(bytes(path),before);QCOMPARE(stock->count(),1);
        if(trigger==3){auto* full=window.findChild<QWidget*>("transmitFullScreenWindow");QVERIFY(full->isVisible());QTRY_VERIFY(full->isActiveWindow());QTest::keyClick(full,Qt::Key_Escape);QTRY_VERIFY(!full->isVisible());}
        compose(window,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());dialog->reject();});QCOMPARE(view->captionBounds(),first);
        const auto screenshot=qEnvironmentVariable("OPENSSTV_TX_SAVE_PREVIEW");if(!useTemplate&&modeId=="robot36"&&profile=="default"&&!screenshot.isEmpty())QVERIFY(window.grab().save(screenshot));
        window.close();MainWindow reopened;reopened.show();auto* remembered=reopened.findChild<QListWidget*>("stockPictures");QCOMPARE(remembered->count(),1);QCOMPARE(remembered->item(0)->data(Qt::UserRole).toString(),path);remembered->setCurrentRow(0);reopened.findChild<QPushButton*>("removeStockPhotosButton")->click();QCOMPARE(remembered->count(),0);QCOMPARE(bytes(path),before);reopened.close();
    }
    void overwriteConfirmsTheFinalPngName(){
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("martin2"));const auto raw=directory_.filePath("protected-photo.jpg");const auto normalized=raw+".png";QFile photo(raw);QVERIFY(photo.open(QIODevice::WriteOnly));photo.write("original photo bytes");photo.close();QImage old(32,24,QImage::Format_RGB32);old.fill(Qt::red);QVERIFY(old.save(normalized));const auto original=bytes(normalized);
        save(window,raw,0,false,QMessageBox::No,true);QCOMPARE(bytes(normalized),original);QCOMPARE(bytes(raw),QByteArray("original photo bytes"));QCOMPARE(window.findChild<QListWidget*>("stockPictures")->count(),0);
        save(window,raw,1,false,QMessageBox::Yes,true);QCOMPARE(QImage(normalized),sstv::fitImage(sstv::testPattern(),*sstv::modeById("martin2")));QCOMPARE(bytes(raw),QByteArray("original photo bytes"));QCOMPARE(window.findChild<QListWidget*>("stockPictures")->count(),1);
        save(window,normalized,0,false,QMessageBox::Yes,true);QCOMPARE(window.findChild<QListWidget*>("stockPictures")->count(),1);window.close();
    }
    void saveFailureKeepsThePictureAndLibrary(){
        MainWindow window;window.show();const auto invalid=directory_.filePath("missing-parent/image.png");QVERIFY(!QDir(QFileInfo(invalid).absolutePath()).exists());auto* view=window.findChild<CaptionPreview*>("transmitImage");const auto before=view->grab().toImage();
        save(window,invalid);QVERIFY(!QFile::exists(invalid));QVERIFY(window.findChild<QLabel*>("statusLabel")->text().startsWith("Could not save TX image:"));QCOMPARE(window.findChild<QListWidget*>("stockPictures")->count(),0);QCOMPARE(view->grab().toImage(),before);QVERIFY(!QSettings().contains("tx/imageSaveDirectory"));window.close();
    }
};
int main(int argc,char** argv){
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);TxImageTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "tx_image_tests.moc"
