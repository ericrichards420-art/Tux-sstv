#include "core/patterns.h"
#include "legacy_test_audio.h"
#include "core/composition.h"
#include "patternsdialog.h"
#include "templatesdialog.h"
#include "composer.h"
#include "mainwindow.h"
#include <QtTest>
#include <QtWidgets>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>

class PatternTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    void picker(MainWindow& window,const QString& id,bool accept){
        bool seen=false;QTimer poll;poll.setInterval(10);
        connect(&poll,&QTimer::timeout,&window,[&]{auto* dialog=window.findChild<PatternsDialog*>();if(!dialog)return;seen=true;poll.stop();auto* list=dialog->findChild<QListWidget*>("testPatternList");QListWidgetItem* selected=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==id)selected=list->item(i);if(!selected){dialog->reject();return;}list->scrollToItem(selected);QApplication::processEvents();QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(selected).center());QCOMPARE(dialog->selectedId(),id);if(accept)QTest::mouseClick(dialog->findChild<QPushButton*>("useTestPattern"),Qt::LeftButton);else dialog->reject();});
        poll.start();QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        auto* button=window.findChild<QPushButton*>("testPatternsButton");QVERIFY(button);QVERIFY(button->isEnabled());QVERIFY(button->isVisible());QApplication::processEvents();QTest::mouseClick(button,Qt::LeftButton);QVERIFY(seen);
    }
    void loadPhotos(MainWindow& window,const QStringList& paths){
        bool opened=false;QTimer poll;poll.setInterval(20);
        connect(&poll,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!dialog)return;poll.stop();opened=true;QCOMPARE(dialog->fileMode(),QFileDialog::ExistingFiles);dialog->setDirectory(QFileInfo(paths.front()).absolutePath());auto* edit=dialog->findChild<QLineEdit*>("fileNameEdit");if(!edit){dialog->reject();return;}QStringList names;for(const auto& path:paths)names<<'"'+QFileInfo(path).fileName()+'"';edit->setText(names.join(' '));static_cast<QDialog*>(dialog)->accept();});poll.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);window.findChild<QPushButton*>("addStockPhotosButton")->click();QVERIFY(opened);
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());QCoreApplication::setOrganizationName("OpenSSTVPatternTests");QCoreApplication::setApplicationName("OpenSSTVPatternTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);qputenv("OPENSSTV_PATTERN_WAV",directory_.filePath("tx.wav").toUtf8());
        QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write("#!/usr/bin/python3\nimport os,sys,shutil,time\nshutil.copyfile(sys.argv[-1],os.environ['OPENSSTV_PATTERN_WAV']+'.tmp')\nos.replace(os.environ['OPENSSTV_PATTERN_WAV']+'.tmp',os.environ['OPENSSTV_PATTERN_WAV'])\ntime.sleep(.1)\n");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void catalogAndOriginalCompatibility(){
        QSet<QString> ids;QSet<QByteArray> hashes;
        for(const auto& pattern:sstv::testPatterns()){
            QVERIFY(!ids.contains(pattern.id));ids.insert(pattern.id);QVERIFY(!pattern.name.isEmpty());QVERIFY(!pattern.description.isEmpty());QVERIFY(!pattern.era.isEmpty());
            const auto image=sstv::renderTestPattern(pattern.id);QVERIFY(!image.isNull());hashes.insert(QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),QCryptographicHash::Sha256));
            if(pattern.monochrome)for(int y=0;y<image.height();y+=3)for(int x=0;x<image.width();x+=3){const auto pixel=image.pixel(x,y);QCOMPARE(qRed(pixel),qGreen(pixel));QCOMPARE(qRed(pixel),qBlue(pixel));}
            for(const auto& mode:sstv::modes()){const auto fitted=sstv::fitImage(image,mode);QCOMPARE(fitted.size(),QSize(mode.width,mode.height));}
        }
        QCOMPARE(ids.size(),10);QCOMPARE(hashes.size(),10);QVERIFY(!sstv::testPatternById("missing"));QVERIFY(sstv::renderTestPattern("missing").isNull());QCOMPARE(sstv::renderTestPattern("test-pattern"),sstv::testPattern());
    }
    void referenceBarLevels(){
        const auto smpte=sstv::renderTestPattern("smpte"),ebu=sstv::renderTestPattern("ebu");
        const QRgb expected[]={qRgb(191,191,191),qRgb(191,191,0),qRgb(0,191,191),qRgb(0,191,0),qRgb(191,0,191),qRgb(191,0,0),qRgb(0,0,191)};
        for(int i=0;i<7;++i){QCOMPARE(smpte.pixel((2*i+1)*768/14,100),expected[i]);QCOMPARE(ebu.pixel((2*i+1)*48,100),i==0?qRgb(255,255,255):expected[i]);}
        QCOMPARE(ebu.pixel(767,575),qRgb(0,0,0));QCOMPARE(smpte.pixel(40,400),qRgb(0,0,191));QCOMPARE(smpte.pixel(620,500),qRgb(8,8,8));QCOMPARE(smpte.pixel(650,500),qRgb(20,20,20));
        const auto gray=sstv::renderTestPattern("grayscale");for(int i=0;i<11;++i)QCOMPARE(gray.pixel((2*i+1)*768/22,100),qRgb(255*i/10,255*i/10,255*i/10));
    }
    void pickerEveryPattern_data(){QTest::addColumn<QString>("id");for(const auto& pattern:sstv::testPatterns())QTest::newRow(qPrintable(pattern.id))<<pattern.id;}
    void pickerEveryPattern(){
        QFETCH(QString,id);PatternsDialog dialog;dialog.show();auto* list=dialog.findChild<QListWidget*>("testPatternList");QCOMPARE(list->count(),10);
        QListWidgetItem* item=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==id)item=list->item(i);QVERIFY(item);list->scrollToItem(item);QApplication::processEvents();QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(item).center());QCOMPARE(dialog.selectedId(),id);QCOMPARE(dialog.findChild<QLabel*>("testPatternName")->text(),sstv::testPatternById(id)->name);QTest::mouseClick(dialog.findChild<QPushButton*>("useTestPattern"),Qt::LeftButton);QCOMPARE(dialog.result(),int(QDialog::Accepted));
    }
    void stockSelectionAndCapturedTransmission(){
        QSettings settings;settings.clear();settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-pattern-device");settings.setValue("radio/enabled",false);
        QImage photo(60,50,QImage::Format_RGB32);photo.fill(Qt::red);const auto photoPath=directory_.filePath("photo.png");QVERIFY(photo.save(photoPath));settings.setValue("pictures/stock",QStringList{photoPath});
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));
        auto* stock=window.findChild<QListWidget*>("stockPictures");QCOMPARE(stock->count(),1);QCOMPARE(stock->item(0)->data(Qt::UserRole).toString(),photoPath);
        QTest::mouseClick(stock->viewport(),Qt::LeftButton,Qt::NoModifier,stock->visualItemRect(stock->item(0)).center());
        // A new pattern clears an applied weather card and caption, just as a new photo does.
        bool edited=false;QTimer::singleShot(20,&window,[&]{auto* dialog=qobject_cast<TemplatesDialog*>(QApplication::activeModalWidget());if(!dialog)return;edited=true;dialog->findChild<QLineEdit*>("templateCity")->setText("OLD REPORT");dialog->findChild<QPushButton*>("templateApply")->click();});window.findChild<QPushButton*>("templatesButton")->click();QVERIFY(edited);
        picker(window,"pm5544",false);bool retained=false;QTimer::singleShot(20,&window,[&]{auto* dialog=qobject_cast<TemplatesDialog*>(QApplication::activeModalWidget());if(!dialog)return;retained=dialog->fields().city=="OLD REPORT";dialog->reject();});window.findChild<QPushButton*>("templatesButton")->click();QVERIFY(retained);
        picker(window,"pm5544",true);QCOMPARE(stock->count(),1);QCOMPARE(stock->item(0)->data(Qt::UserRole).toString(),photoPath);
        const auto expected=(sstv::composeTransmitImage(sstv::renderTestPattern("pm5544"),*sstv::modeById("robot36"),"N0CALL").image);
        window.findChild<QPushButton*>("playButton")->click();const auto path=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(path),5000);
        const auto decoded=sstv::decode(sstv::readWav(path));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));QTRY_VERIFY(window.findChild<QPushButton*>("playButton")->isEnabled());window.close();
    }
    void photoLibraryImportRemoveAndPersistence(){
        QSettings settings;settings.clear();settings.setValue("audio/output","fake-pattern-device");QFile::remove(directory_.filePath("tx.wav"));
        QStringList paths;QMap<QString,QByteArray> originals;int color=0;for(const auto value:{QColor(Qt::red),QColor(Qt::blue),QColor(Qt::green)}){const auto path=directory_.filePath(QString("photo %1.png").arg(++color));QImage image(320,240,QImage::Format_RGB32);image.fill(value);QVERIFY(image.save(path));paths<<path;QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));originals[path]=file.readAll();}
        MainWindow window;window.show();auto* stock=window.findChild<QListWidget*>("stockPictures");auto* remove=window.findChild<QPushButton*>("removeStockPhotosButton");auto* add=window.findChild<QPushButton*>("addStockPhotosButton");QCOMPARE(stock->count(),0);QVERIFY(!remove->isEnabled());QVERIFY(add->isEnabled());
        picker(window,"ebu",true);QCOMPARE(stock->count(),0); // Built-in choices stay in the existing picker.
        auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));loadPhotos(window,paths);QCOMPARE(stock->count(),3);
        QSet<QString> shown;for(int i=0;i<stock->count();++i)shown<<stock->item(i)->data(Qt::UserRole).toString();QCOMPARE(shown,QSet<QString>(paths.begin(),paths.end()));
        const auto active=stock->item(0)->data(Qt::UserRole).toString(),remaining=stock->item(1)->data(Qt::UserRole).toString();
        const auto screenshot=qEnvironmentVariable("OPENSSTV_PHOTO_LIBRARY_PREVIEW");if(!screenshot.isEmpty())QVERIFY(window.grab().save(screenshot));
        stock->setCurrentRow(0);stock->item(2)->setSelected(true);QCOMPARE(stock->selectedItems().size(),2);QVERIFY(remove->isEnabled());QTest::mouseClick(remove,Qt::LeftButton);QCOMPARE(stock->count(),1);QCOMPARE(stock->item(0)->data(Qt::UserRole).toString(),remaining);QCOMPARE(settings.value("pictures/stock").toStringList(),QStringList{remaining});
        stock->setCurrentRow(0);QVERIFY(remove->isEnabled());const auto expected=(sstv::composeTransmitImage(QImage(active),*sstv::modeById("robot36"),{}).image);
        auto* send=window.findChild<QPushButton*>("playButton");send->click();QVERIFY(!add->isEnabled());QVERIFY(!remove->isEnabled());const auto wav=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(wav),5000);const auto decoded=sstv::decode(sstv::readWav(wav));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));QTRY_VERIFY(send->isEnabled());window.close();
        MainWindow reopened;reopened.show();reopened.activateWindow();QTRY_VERIFY(reopened.isActiveWindow());auto* saved=reopened.findChild<QListWidget*>("stockPictures");QCOMPARE(saved->count(),1);QCOMPARE(saved->item(0)->data(Qt::UserRole).toString(),remaining);saved->setCurrentRow(0);saved->setFocus();QTest::keyClick(saved,Qt::Key_Delete);QTRY_COMPARE(saved->count(),0);QVERIFY(!reopened.findChild<QPushButton*>("removeStockPhotosButton")->isEnabled());reopened.close();
        MainWindow empty;QCOMPARE(empty.findChild<QListWidget*>("stockPictures")->count(),0);empty.close();
        for(auto it=originals.cbegin();it!=originals.cend();++it){QFile file(it.key());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),it.value());}
    }
    void cliPatternSelection(){
        const auto binary=QCoreApplication::applicationDirPath()+"/opensstv";auto run=[&](QStringList args){QProcess process;process.start(binary,args);if(!process.waitForFinished(10000))return std::pair<int,QByteArray>{-1,{}};return std::pair<int,QByteArray>{process.exitCode(),process.readAllStandardOutput()};};
        const auto version=run({"--version"});QCOMPARE(version.first,0);QVERIFY(version.second.startsWith("Tux-sstv "));const auto help=run({"--help"});QCOMPARE(help.first,0);QVERIFY(help.second.contains("Tux-sstv Final Release"));
        QProcess current;current.start(QCoreApplication::applicationDirPath()+"/tux-sstv",{"--version"});QVERIFY(current.waitForFinished(10000));QCOMPARE(current.exitCode(),0);QCOMPARE(current.readAllStandardOutput(),version.second);
        const auto catalog=run({"--list-patterns"});QCOMPARE(catalog.first,0);QCOMPARE(QJsonDocument::fromJson(catalog.second).array().size(),10);
        const auto path=directory_.filePath("ebu.png");QCOMPARE(run({"--pattern",path,"--pattern-id","ebu"}).first,0);QCOMPARE(QImage(path),sstv::renderTestPattern("ebu"));QCOMPARE(run({"--pattern-id","ebu"}).first,1);QCOMPARE(run({"--pattern",path,"--pattern-id","unknown"}).first,1);QCOMPARE(QImage(path),sstv::renderTestPattern("ebu"));
    }
    void exportPreviews(){
        const auto path=qEnvironmentVariable("OPENSSTV_PATTERN_ARTIFACTS");if(path.isEmpty())return;QVERIFY(QDir().mkpath(path));QImage sheet(960,5*388,QImage::Format_RGB32);sheet.fill(QColor("#171c26"));QPainter painter(&sheet);int i=0;
        for(const auto& pattern:sstv::testPatterns()){const auto image=sstv::renderTestPattern(pattern.id);QVERIFY(image.save(path+"/"+pattern.id+".png"));const int x=(i%2)*480,y=(i/2)*388;const auto thumb=image.scaled(456,342,Qt::KeepAspectRatio,Qt::SmoothTransformation);painter.drawImage(QPoint(x+(480-thumb.width())/2,y+8),thumb);painter.setPen(Qt::white);QFont font("DejaVu Sans");font.setPixelSize(17);painter.setFont(font);painter.drawText(QRect(x+12,y+354,456,30),Qt::AlignCenter,pattern.name);++i;}painter.end();QVERIFY(sheet.save(path+"/classic-patterns.png"));
        PatternsDialog dialog("pm5544");dialog.show();QApplication::processEvents();QVERIFY(dialog.grab().save(path+"/pattern-picker.png"));
    }
    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);PatternTests tests;return QTest::qExec(&tests,argc,argv);}
#include "pattern_tests.moc"
