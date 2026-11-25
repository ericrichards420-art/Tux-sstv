#include "legacy_test_audio.h"
#include "core/templates.h"
#include "mainwindow.h"
#include "templatesdialog.h"
#include "composer.h"
#include "captionpreview.h"
#include "skins.h"
#include <QtTest>
#include <QtWidgets>
#include <QTimeZone>
#include <cmath>

class TemplateTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    QByteArray oldPath_;
    static sstv::TemplateFields report(){
        sstv::TemplateFields f;f.city="Austin";f.state="Texas";f.weather="Partly cloudy";f.temperature="72";f.date={2026,11,20};f.time={19,45};f.utc=true;return f;
    }
    static void editTemplate(MainWindow& window,const std::function<void(TemplatesDialog*)>& edit){
        bool edited=false;QTimer timer;timer.setInterval(25);
        connect(&timer,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<TemplatesDialog*>(QApplication::activeModalWidget());if(!dialog)return;timer.stop();edited=true;edit(dialog);});timer.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        window.findChild<QPushButton*>("templatesButton")->click();QVERIFY(edited);
    }
    static void editCaption(MainWindow& window,const std::function<void(CompositionDialog*)>& edit){
        bool edited=false;QTimer timer;timer.setInterval(25);connect(&timer,&QTimer::timeout,&window,[&]{auto* dialog=qobject_cast<CompositionDialog*>(QApplication::activeModalWidget());if(!dialog)return;timer.stop();edited=true;edit(dialog);});timer.start();
        QTimer deadline;deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,&window,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});deadline.start(5000);
        window.findChild<QPushButton*>("compositionButton")->click();QVERIFY(edited);
    }
private slots:
    void initTestCase(){
        QVERIFY(directory_.isValid());oldPath_=qgetenv("PATH");qputenv("PATH",directory_.path().toUtf8()+":"+oldPath_);
        QCoreApplication::setOrganizationName("OpenSSTVTemplateTests");QCoreApplication::setApplicationName("OpenSSTVTemplateTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory_.filePath("settings"));
        qputenv("OPENSSTV_TEMPLATE_TX_COPY",directory_.filePath("tx.wav").toUtf8());
        QFile player(directory_.filePath("aplay"));QVERIFY(player.open(QIODevice::WriteOnly));player.write(R"(#!/usr/bin/python3
import os, shutil, sys, time
target=os.environ['OPENSSTV_TEMPLATE_TX_COPY']
shutil.copyfile(sys.argv[-1],target+'.tmp')
os.replace(target+'.tmp',target)
time.sleep(.3)
)");player.close();QVERIFY(player.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void init(){QSettings settings;settings.clear();QFile::remove(directory_.filePath("tx.wav"));}
    void persistenceValidationAndTime(){
        QSettings settings;auto fields=report();fields.design="sunset";fields.message="73 from Austin 📻";fields.save(settings);
        const auto loaded=sstv::TemplateFields::load(settings);QCOMPARE(loaded.design,fields.design);QCOMPARE(loaded.unit,fields.unit);QCOMPARE(loaded.utc,fields.utc);QVERIFY(loaded.city.isEmpty());QVERIFY(loaded.message.isEmpty());QVERIFY(loaded.date.isNull());QVERIFY(loaded.time.isNull());QVERIFY(!settings.contains("tx/template/city"));
        for(const auto& invalid:QStringList{"-","NaN","infinity","303","-239"}){auto f=fields;f.temperature=invalid;QVERIFY(!f.validate().isEmpty());}
        fields.temperature.clear();QVERIFY(fields.validate().isEmpty());fields.unit="C";fields.temperature="150";QVERIFY(fields.validate().isEmpty());fields.temperature="150.1";QVERIFY(!fields.validate().isEmpty());
        fields.temperature.clear();fields.utc=true;fields.useCurrentTime();const auto instant=QDateTime(fields.date,fields.time,QTimeZone::utc());QVERIFY(std::abs(instant.msecsTo(QDateTime::currentDateTimeUtc()))<1000);
        settings.setValue("tx/template/design","missing");settings.setValue("tx/template/temperature","NaN");settings.setValue("tx/template/city","City\nState");const auto recovered=sstv::TemplateFields::load(settings);QVERIFY(recovered.validate().isEmpty());QCOMPARE(recovered.design,QString("weather"));QVERIFY(recovered.temperature.isEmpty());
    }
    void renderEveryDesignAndMode(){
        QSet<QByteArray> designs;
        for(const auto& design:sstv::imageTemplates()){
            auto fields=report();fields.design=design.id;if(!design.weatherCard&&!design.tvCard){fields.toCall="K1ABC";fields.report="GOOD COPY";}const auto preview=sstv::renderTemplate(fields,{640,480},"N0CALL");QVERIFY(!preview.isNull());
            designs.insert(QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(preview.constBits()),preview.sizeInBytes()),QCryptographicHash::Sha256));
            const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()){QDir().mkpath(artifacts);QVERIFY(preview.save(artifacts+"/"+design.id+".png"));}
            for(const auto& mode:sstv::modes()){
                const auto image=sstv::composeTransmitImage(sstv::renderTemplate(fields,{mode.width,mode.height},"N0CALL"),mode,{}).image;QCOMPARE(image.size(),QSize(mode.width,mode.height));
                if(mode.family==sstv::Family::Mono)for(int y=0;y<image.height();y+=7)for(int x=0;x<image.width();x+=7){const auto pixel=image.pixel(x,y);QCOMPARE(qRed(pixel),qGreen(pixel));QCOMPARE(qGreen(pixel),qBlue(pixel));}
            }
            for(const auto& condition:QStringList{"Sunny","Clear","Partly cloudy","Rain","Thunderstorms","Snow","Fog","Windy","Custom conditions",""}){fields.weather=condition;QVERIFY(!sstv::renderTemplate(fields,{320,240},{}).isNull());}
        }QCOMPARE(designs.size(),sstv::imageTemplates().size());QCOMPARE(designs.size(),16);
        QVERIFY(sstv::renderTemplate(report(),{0,240},{}).isNull());QVERIFY(sstv::renderTemplate(report(),{50000,50000},{}).isNull());
        auto fields=report();const auto first=sstv::renderTemplate(fields,{320,240},"N0CALL");fields.temperature="-12.5";QVERIFY(sstv::renderTemplate(fields,{320,240},"N0CALL")!=first);
    }
    void televisionCardsRenderAndUseOwnFields(){
        auto fields=report();fields.headline="PICTURES FROM THE SHACK";fields.subheadline="A new SSTV show with friends";fields.ticker="Next: another picture exchange";fields.programInfo="Tonight · 8:00 PM";fields.homeTeam="TUX";fields.awayTeam="PENGUINS";fields.homeScore="7";fields.awayScore="3";
        int count=0;
        for(const auto& design:sstv::imageTemplates())if(design.tvCard){
            ++count;fields.design=design.id;QMap<QString,QRect> bounds;
            // Prime large glyphs before comparing independently rendered frames.
            sstv::renderTemplate(fields,{640,480},"KI5BZO");
            const auto image=sstv::renderTemplate(fields,{640,480},"KI5BZO",{},&bounds);QVERIFY(!image.isNull());
            const QStringList keys=design.id=="scoreboard"?QStringList{"homeTeam","awayTeam","homeScore","awayScore","ticker","location","stamp"}:QStringList{"headline","subheadline","ticker","location","stamp"};
            for(int i=0;i<keys.size();++i){QVERIFY2(bounds.contains(keys[i]),qPrintable(design.id+" / "+keys[i]));QVERIFY(image.rect().contains(bounds[keys[i]]));for(int j=i+1;j<keys.size();++j)QVERIFY2(!bounds[keys[i]].intersects(bounds[keys[j]]),qPrintable(keys[i]+" / "+keys[j]));}
            const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()){QDir().mkpath(artifacts);QVERIFY(image.save(artifacts+"/tv-"+design.id+".png"));}
            auto changed=fields;changed.weather="Snow";changed.temperature="bad";changed.windSpeed="bad";changed.humidity="999";changed.message="Hidden greeting";changed.toCall="K1OTHER";QVERIFY(changed.validate().isEmpty());const auto ignored=sstv::renderTemplate(changed,{640,480},"KI5BZO");if(ignored!=image&&!artifacts.isEmpty())QVERIFY(ignored.save(artifacts+"/tv-difference-"+design.id+".png"));QVERIFY2(ignored==image,qPrintable(design.id));
            if(design.id=="scoreboard")changed.homeScore="8";else changed.headline="A NEW HEADLINE";QVERIFY(sstv::renderTemplate(changed,{640,480},"KI5BZO")!=image);
            const QString key=design.id=="scoreboard"?"homeTeam":"headline";changed=fields;changed.textPositions[key]={.1,.35};QMap<QString,QRect> moved;sstv::renderTemplate(changed,{640,480},"KI5BZO",{},&moved);QVERIFY(std::abs(moved[key].left()-64)<=2);
            QImage photo(640,480,QImage::Format_RGB32);photo.fill(QColor("#9a7134"));changed=fields;changed.usePicture=true;QVERIFY(sstv::renderTemplate(changed,{640,480},"KI5BZO",photo)!=image);
            sstv::renderTemplate(fields,{320,240},"KI5BZO",{},&bounds,false);QVERIFY(!bounds.contains("callsign"));
        }QCOMPARE(count,5);
        fields.design="newsroom";fields.headline=QString(129,'X');QVERIFY(!fields.validate().isEmpty());fields.headline="Line one\nLine two";QVERIFY(!fields.validate().isEmpty());
    }
    void televisionEditorCategoryClearAndFreshReports(){
        sstv::TemplateFields fields;TemplatesDialog dialog(fields,false,*sstv::modeById("robot36"),"KI5BZO",{});dialog.show();
        auto* category=dialog.findChild<QComboBox*>("templateCategory");QCOMPARE(category->itemText(3),QString("TV Studio"));category->setCurrentIndex(3);auto* designs=dialog.findChild<QListWidget*>("templateDesigns");QCOMPARE(designs->count(),5);
        const auto select=[&](const QString& id){for(int i=0;i<designs->count();++i)if(designs->item(i)->data(Qt::UserRole).toString()==id){designs->setCurrentRow(i);return;}QFAIL("Missing TV design");};
        select("newsroom");QVERIFY(dialog.findChild<QLineEdit*>("templateHeadline")->isVisible());QVERIFY(!dialog.findChild<QComboBox*>("templateWeather")->isVisible());QVERIFY(!dialog.findChild<QLineEdit*>("templateToCall")->isVisible());QVERIFY(!dialog.findChild<QLineEdit*>("templateHomeTeam")->isVisible());
        dialog.findChild<QLineEdit*>("templateHeadline")->setText("PICTURES FROM THE SHACK");dialog.findChild<QLineEdit*>("templateSubheadline")->setText("Your station, your story");dialog.findChild<QLineEdit*>("templateTicker")->setText("Next: another picture exchange");
        select("scoreboard");QVERIFY(!dialog.findChild<QLineEdit*>("templateHeadline")->isVisible());QVERIFY(dialog.findChild<QLineEdit*>("templateHomeTeam")->isVisible());dialog.findChild<QLineEdit*>("templateHomeTeam")->setText("TUX");dialog.findChild<QLineEdit*>("templateAwayTeam")->setText("PENGUINS");dialog.findChild<QLineEdit*>("templateHomeScore")->setText("7");dialog.findChild<QLineEdit*>("templateAwayScore")->setText("3");
        select("now-showing");QVERIFY(dialog.findChild<QLineEdit*>("templateProgramInfo")->isVisible());dialog.findChild<QLineEdit*>("templateProgramInfo")->setText("Tonight · 8:00 PM");
        select("breaking-news");QCOMPARE(dialog.fields().headline,QString("PICTURES FROM THE SHACK"));QVERIFY(!dialog.findChild<QLineEdit*>("templateProgramInfo")->isVisible());
        const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()){QApplication::processEvents();QVERIFY(dialog.grab().save(artifacts+"/tv-editor.png"));}
        auto* preview=dialog.findChild<CaptionPreview*>("templatePreview");QVERIFY(!preview->textWidgetRect("headline").isEmpty());preview->textMoved("headline",{.1,.45});QCOMPARE(dialog.fields().textPositions.value("headline"),QPointF(.1,.45));
        const auto entered=dialog.fields();QSettings settings;entered.save(settings);const auto fresh=sstv::TemplateFields::load(settings);QCOMPARE(fresh.design,QString("breaking-news"));QVERIFY(fresh.headline.isEmpty());QVERIFY(fresh.ticker.isEmpty());QVERIFY(fresh.homeScore.isEmpty());QVERIFY(fresh.programInfo.isEmpty());
        dialog.findChild<QPushButton*>("templateClear")->click();for(const auto* name:{"templateHeadline","templateSubheadline","templateTicker","templateProgramInfo","templateHomeTeam","templateAwayTeam","templateHomeScore","templateAwayScore"})QVERIFY(dialog.findChild<QLineEdit*>(name)->text().isEmpty());QVERIFY(dialog.fields().textPositions.isEmpty());QVERIFY(dialog.findChild<QPushButton*>("templateApply")->isEnabled());dialog.reject();QVERIFY(fields.headline.isEmpty());
        auto cleared=entered;cleared.clearFields();QVERIFY(cleared.headline.isEmpty());QVERIFY(cleared.subheadline.isEmpty());QVERIFY(cleared.ticker.isEmpty());QVERIFY(cleared.programInfo.isEmpty());QVERIFY(cleared.homeTeam.isEmpty());QVERIFY(cleared.awayTeam.isEmpty());QVERIFY(cleared.homeScore.isEmpty());QVERIFY(cleared.awayScore.isEmpty());
    }
    void televisionApplyCancelUndoAndTransmission_data(){
        QTest::addColumn<QString>("design");for(const auto& design:sstv::imageTemplates())if(design.tvCard)QTest::newRow(design.id.toUtf8().constData())<<design.id;
    }
    void televisionApplyCancelUndoAndTransmission(){
        QFETCH(QString,design);QSettings().setValue("station/call","KI5BZO");MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));sstv::TemplateFields entered;
        editTemplate(window,[&](TemplatesDialog* dialog){auto* designs=dialog->findChild<QListWidget*>("templateDesigns");for(int i=0;i<designs->count();++i)if(designs->item(i)->data(Qt::UserRole).toString()==design)designs->setCurrentRow(i);dialog->findChild<QLineEdit*>("templateHeadline")->setText("PICTURES FROM THE SHACK");dialog->findChild<QLineEdit*>("templateSubheadline")->setText("A new SSTV show");dialog->findChild<QLineEdit*>("templateTicker")->setText("73 from Texas");dialog->findChild<QLineEdit*>("templateProgramInfo")->setText("Tonight · 8:00 PM");dialog->findChild<QLineEdit*>("templateHomeTeam")->setText("TUX");dialog->findChild<QLineEdit*>("templateAwayTeam")->setText("PENGUINS");dialog->findChild<QLineEdit*>("templateHomeScore")->setText("7");dialog->findChild<QLineEdit*>("templateAwayScore")->setText("3");entered=dialog->fields();dialog->findChild<QPushButton*>("templateApply")->click();});
        const auto expected=sstv::composeTransmitImage(sstv::renderTemplate(entered,{320,240},"KI5BZO"),*sstv::modeById("robot36"),{}).image;
        editTemplate(window,[&](TemplatesDialog* dialog){QCOMPARE(dialog->image(),expected);dialog->findChild<QLineEdit*>("templateTicker")->setText("Discard this update");dialog->reject();});
        window.findChild<QAction*>("undoTransmitAction")->trigger();editTemplate(window,[](TemplatesDialog* dialog){QVERIFY(dialog->fields().ticker.isEmpty());dialog->reject();});window.findChild<QAction*>("redoTransmitAction")->trigger();
        editTemplate(window,[&](TemplatesDialog* dialog){QCOMPARE(dialog->image(),expected);QCOMPARE(dialog->fields().ticker,QString("73 from Texas"));dialog->reject();});
        window.findChild<QPushButton*>("playButton")->click();const auto copy=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(copy),5000);const auto decoded=sstv::decode(sstv::readWav(copy));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));QTRY_VERIFY(window.findChild<QPushButton*>("templatesButton")->isEnabled());window.close();
    }
    void weatherReadingsValidationAndPreferences(){
        auto fields=report();fields.windSpeed="12";fields.rainfall="0.25";fields.humidity="65";fields.pressure="1013.2";
        fields.windUnit="km/h";fields.rainUnit="mm";fields.pressureUnit="hPa";QVERIFY(fields.validate().isEmpty());
        QSettings settings;fields.save(settings);const auto fresh=sstv::TemplateFields::load(settings);
        QCOMPARE(fresh.windUnit,fields.windUnit);QCOMPARE(fresh.rainUnit,fields.rainUnit);QCOMPARE(fresh.pressureUnit,fields.pressureUnit);
        QVERIFY(fresh.windSpeed.isEmpty());QVERIFY(fresh.rainfall.isEmpty());QVERIFY(fresh.humidity.isEmpty());QVERIFY(fresh.pressure.isEmpty());
        for(auto member:{&sstv::TemplateFields::windSpeed,&sstv::TemplateFields::rainfall,&sstv::TemplateFields::humidity,&sstv::TemplateFields::pressure}){
            for(const auto& invalid:QStringList{"-1","NaN","inf","bad","1000001"}){auto f=fields;f.*member=invalid;QVERIFY2(!f.validate().isEmpty(),qPrintable(invalid));}
            auto f=fields;f.*member="";QVERIFY(f.validate().isEmpty());
        }
        fields.humidity="100.1";QVERIFY(!fields.validate().isEmpty());fields.humidity="100";QVERIFY(fields.validate().isEmpty());
        fields.pressure="0";QVERIFY(!fields.validate().isEmpty());fields.pressure="29.92";fields.windSpeed="0";fields.rainfall="0";fields.humidity="0";QVERIFY(fields.validate().isEmpty());
        fields.clearFields();QVERIFY(fields.windSpeed.isEmpty());QVERIFY(fields.rainfall.isEmpty());QVERIFY(fields.humidity.isEmpty());QVERIFY(fields.pressure.isEmpty());QCOMPARE(fields.pressureUnit,QString("hPa"));
        settings.setValue("tx/template/windUnit","invalid");settings.setValue("tx/template/rainUnit","invalid");settings.setValue("tx/template/pressureUnit","invalid");
        const auto recovered=sstv::TemplateFields::load(settings);QVERIFY(recovered.validate().isEmpty());QCOMPARE(recovered.windUnit,QString("mph"));QCOMPARE(recovered.rainUnit,QString("in"));QCOMPARE(recovered.pressureUnit,QString("inHg"));
    }
    void weatherReadingsRenderWithoutOverlap(){
        auto fields=report();fields.message="73 from Texas";fields.windSpeed="12";fields.rainfall="0.25";fields.humidity="65";fields.pressure="29.92";
        for(const auto& design:sstv::imageTemplates())if(design.weatherCard){
            fields.design=design.id;
            for(const auto size:{QSize(640,480),QSize(320,240),QSize(320,256)}){
                QMap<QString,QRect> bounds;const auto image=sstv::renderTemplate(fields,size,"KI5BZO",{},&bounds);QVERIFY(!image.isNull());
                const QStringList keys={"temperature","windSpeed","humidity","rainfall","pressure","date","time","message"};
                for(int i=0;i<keys.size();++i){QVERIFY2(bounds.contains(keys[i]),qPrintable(keys[i]));QVERIFY(image.rect().contains(bounds.value(keys[i])));
                    for(int j=i+1;j<keys.size();++j)QVERIFY2(!bounds.value(keys[i]).intersects(bounds.value(keys[j])),qPrintable(keys[i]+" / "+keys[j]));}
                const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()&&size==QSize(640,480)){QDir().mkpath(artifacts);QVERIFY(image.save(artifacts+"/weather-readings-"+design.id+".png"));}
            }
        }
        fields.design="weather";fields.textPositions["humidity"]={.08,.72};QMap<QString,QRect> moved;sstv::renderTemplate(fields,{640,480},"KI5BZO",{},&moved);QVERIFY(std::abs(moved["humidity"].left()-51)<=2);
        fields.pressure.clear();sstv::renderTemplate(fields,{320,240},"KI5BZO",{},&moved);QVERIFY(!moved.contains("pressure"));QVERIFY(moved.contains("windSpeed"));
        fields.rainfall="0";sstv::renderTemplate(fields,{320,240},"KI5BZO",{},&moved);QVERIFY(moved.contains("rainfall"));
        fields.design="reply";const auto exchange=sstv::renderTemplate(fields,{320,240},"KI5BZO");fields.windSpeed="-";fields.rainfall="bad";fields.humidity="101";fields.pressure="0";QVERIFY(fields.validate().isEmpty());QCOMPARE(sstv::renderTemplate(fields,{320,240},"KI5BZO"),exchange);
    }
    void weatherReadingsEditorConversionsAndClear(){
        auto fields=report();fields.windSpeed="12";fields.rainfall="0.25";fields.humidity="65";fields.pressure="29.92";
        TemplatesDialog dialog(fields,true,*sstv::modeById("robot36"),"KI5BZO",{});dialog.show();
        const auto value=[&](const char* name){return dialog.findChild<QLineEdit*>(name)->text().toDouble();};
        auto* wind=dialog.findChild<QComboBox*>("templateWindUnit");wind->setCurrentIndex(wind->findData("km/h"));QVERIFY(std::abs(value("templateWindSpeed")-19.312128)<.001);
        wind->setCurrentIndex(wind->findData("kn"));QVERIFY(std::abs(value("templateWindSpeed")-10.4277)<.002);wind->setCurrentIndex(wind->findData("m/s"));QVERIFY(std::abs(value("templateWindSpeed")-5.36448)<.002);wind->setCurrentIndex(0);QVERIFY(std::abs(value("templateWindSpeed")-12)<.003);
        auto* rain=dialog.findChild<QComboBox*>("templateRainUnit");rain->setCurrentIndex(1);QVERIFY(std::abs(value("templateRainfall")-6.35)<.001);rain->setCurrentIndex(0);QVERIFY(std::abs(value("templateRainfall")-.25)<.001);
        auto* pressure=dialog.findChild<QComboBox*>("templatePressureUnit");pressure->setCurrentIndex(1);QVERIFY(std::abs(value("templatePressure")-1013.2079)<.002);pressure->setCurrentIndex(0);QVERIFY(std::abs(value("templatePressure")-29.92)<.001);
        auto* humidity=dialog.findChild<QLineEdit*>("templateHumidity");auto* apply=dialog.findChild<QPushButton*>("templateApply");humidity->setText("101");QVERIFY(!apply->isEnabled());humidity->setText("65");QVERIFY(apply->isEnabled());
        const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()){QApplication::processEvents();QVERIFY(dialog.grab().save(artifacts+"/weather-readings-editor.png"));}
        auto* category=dialog.findChild<QComboBox*>("templateCategory");category->setCurrentIndex(2);
        for(const auto* name:{"templateWindSpeed","templateRainfall","templateHumidity","templatePressure"})QVERIFY(!dialog.findChild<QLineEdit*>(name)->isVisible());
        category->setCurrentIndex(1);QVERIFY(humidity->isVisible());dialog.findChild<QPushButton*>("templateClear")->click();
        for(const auto* name:{"templateWindSpeed","templateRainfall","templateHumidity","templatePressure"})QVERIFY(dialog.findChild<QLineEdit*>(name)->text().isEmpty());QVERIFY(apply->isEnabled());dialog.reject();QCOMPARE(fields.humidity,QString("65"));
    }
    void weatherReadingsApplyAndCancel(){
        QSettings().setValue("station/call","KI5BZO");MainWindow window;window.show();window.findChild<QComboBox*>("transmitMode")->setCurrentIndex(window.findChild<QComboBox*>("transmitMode")->findData("robot36"));sstv::TemplateFields entered;
        editTemplate(window,[&](TemplatesDialog* dialog){dialog->findChild<QLineEdit*>("templateWindSpeed")->setText("15");dialog->findChild<QLineEdit*>("templateRainfall")->setText("0.75");dialog->findChild<QLineEdit*>("templateHumidity")->setText("82");dialog->findChild<QLineEdit*>("templatePressure")->setText("29.85");entered=dialog->fields();dialog->findChild<QPushButton*>("templateApply")->click();});
        const auto expected=sstv::composeTransmitImage(sstv::renderTemplate(entered,{320,240},"KI5BZO"),*sstv::modeById("robot36"),{}).image;
        editTemplate(window,[&](TemplatesDialog* dialog){QCOMPARE(dialog->image(),expected);QCOMPARE(dialog->fields().windSpeed,QString("15"));QCOMPARE(dialog->fields().rainfall,QString("0.75"));QCOMPARE(dialog->fields().humidity,QString("82"));QCOMPARE(dialog->fields().pressure,QString("29.85"));dialog->findChild<QLineEdit*>("templateHumidity")->setText("10");dialog->reject();});editTemplate(window,[&](TemplatesDialog* dialog){QCOMPARE(dialog->fields().humidity,QString("82"));QCOMPARE(dialog->image(),expected);dialog->reject();});
        window.findChild<QAction*>("undoTransmitAction")->trigger();editTemplate(window,[](TemplatesDialog* dialog){QVERIFY(dialog->fields().humidity.isEmpty());dialog->reject();});
        window.findChild<QAction*>("redoTransmitAction")->trigger();editTemplate(window,[&](TemplatesDialog* dialog){QCOMPARE(dialog->fields().humidity,QString("82"));QCOMPARE(dialog->image(),expected);dialog->reject();});
        window.findChild<QPushButton*>("playButton")->click();const auto copy=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(copy),5000);const auto decoded=sstv::decode(sstv::readWav(copy));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));QTRY_VERIFY(window.findChild<QPushButton*>("templatesButton")->isEnabled());window.close();
    }
    void unitsDateTimeValidationAndPreview(){
        auto fields=report();TemplatesDialog dialog(fields,false,*sstv::modeById("robot36"),"N0CALL",{});dialog.show();
        auto* temperature=dialog.findChild<QLineEdit*>("templateTemperature");auto* unit=dialog.findChild<QComboBox*>("templateTemperatureUnit");auto* apply=dialog.findChild<QPushButton*>("templateApply");
        temperature->setText("32");unit->setCurrentIndex(1);QCOMPARE(temperature->text().toDouble(),0.);unit->setCurrentIndex(0);QCOMPARE(temperature->text().toDouble(),32.);
        temperature->setText("-");QVERIFY(!apply->isEnabled());QVERIFY(!dialog.findChild<QLabel*>("templateStatus")->text().isEmpty());temperature->clear();QVERIFY(apply->isEnabled());
        const auto utc=QDateTime(fields.date,fields.time,QTimeZone::utc());dialog.findChild<QComboBox*>("templateTimeZone")->setCurrentIndex(0);
        QCOMPARE(dialog.fields().date,utc.toLocalTime().date());QCOMPARE(dialog.fields().time,utc.toLocalTime().time());dialog.findChild<QComboBox*>("templateTimeZone")->setCurrentIndex(1);QCOMPARE(dialog.fields().date,fields.date);QCOMPARE(dialog.fields().time,fields.time);
        dialog.findChild<QPushButton*>("templateNow")->click();QCOMPARE(dialog.fields().date,QDateTime::currentDateTimeUtc().date());QVERIFY(std::abs(QDateTime(dialog.fields().date,dialog.fields().time,QTimeZone::utc()).msecsTo(QDateTime::currentDateTimeUtc()))<60000);
        auto* designs=dialog.findChild<QListWidget*>("templateDesigns");const auto before=dialog.image();designs->setCurrentRow(2);QVERIFY(dialog.image()!=before);
        const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()){temperature->setText("72");dialog.findChild<QLineEdit*>("templateDate")->setText(fields.date.toString(Qt::ISODate));dialog.findChild<QLineEdit*>("templateTime")->setText(fields.time.toString("HH:mm"));QApplication::processEvents();QVERIFY(dialog.grab().save(artifacts+"/editor.png"));}dialog.reject();
    }
    void exchangeCardsBrowseAndKeepFieldsBlank(){
        sstv::TemplateFields fields;TemplatesDialog dialog(fields,false,*sstv::modeById("robot36"),"N0CALL",{});dialog.show();
        auto* category=dialog.findChild<QComboBox*>("templateCategory");auto* designs=dialog.findChild<QListWidget*>("templateDesigns");auto* weather=dialog.findChild<QComboBox*>("templateWeather");
        auto* apply=dialog.findChild<QPushButton*>("templateApply");QVERIFY(category&&designs&&weather&&apply);
        category->setCurrentIndex(2);int visible=0;
        auto* quick=dialog.findChild<QComboBox*>("templateQuickReply");QCOMPARE(quick->currentIndex(),0);QVERIFY(dialog.fields().message.isEmpty());
        for(int i=0;i<designs->count();++i)if(!designs->item(i)->isHidden()){
            ++visible;designs->setCurrentRow(i);QVERIFY(!sstv::imageTemplateById(dialog.fields().design)->weatherCard);QVERIFY(!weather->isVisible());QVERIFY(!dialog.findChild<QLineEdit*>("templateTemperature")->isVisible());
            for(const auto* name:{"templateToCall","templateReport","templateCity","templateState","templateTemperature","templateDate","templateTime","templateMessage"})QVERIFY(dialog.findChild<QLineEdit*>(name)->text().isEmpty());
            QVERIFY(dialog.fields().weather.isEmpty());QVERIFY(apply->isEnabled());
        }QCOMPARE(visible,7);
        category->setCurrentIndex(1);QVERIFY(weather->isVisible());dialog.findChild<QLineEdit*>("templateTemperature")->setText("-");QVERIFY(!apply->isEnabled());
        category->setCurrentIndex(2);QVERIFY(apply->isEnabled());QVERIFY(!weather->isVisible());dialog.findChild<QPushButton*>("templateClear")->click();
        for(int i=0;i<designs->count();++i)if(designs->item(i)->data(Qt::UserRole).toString()=="reply")designs->setCurrentRow(i);
        dialog.findChild<QLineEdit*>("templateToCall")->setText("K1ABC");quick->setCurrentIndex(5);QCOMPARE(dialog.fields().message,QString("Thanks for the picture!"));QCOMPARE(quick->currentIndex(),0);dialog.findChild<QLineEdit*>("templateCity")->setText("Austin");dialog.findChild<QLineEdit*>("templateState")->setText("Texas");
        const auto artifacts=qEnvironmentVariable("OPENSSTV_TEMPLATE_ARTIFACTS");if(!artifacts.isEmpty()){QApplication::processEvents();QVERIFY(dialog.grab().save(artifacts+"/exchange-editor.png"));}
        category->setCurrentIndex(0);for(int i=0;i<designs->count();++i)QVERIFY(!designs->item(i)->isHidden());dialog.reject();
        QVERIFY(fields.city.isEmpty());QVERIFY(fields.message.isEmpty());QVERIFY(!sstv::imageTemplateById("missing"));
    }
    void mouseCanChooseWeatherStation_data(){
        QTest::addColumn<QString>("skin");for(const auto& skin:ui::skins())QTest::newRow(skin.id.toUtf8().constData())<<skin.id;
    }
    void mouseCanChooseWeatherStation(){
        QFETCH(QString,skin);QSettings settings;settings.setValue("ui/skin",skin);
        auto fields=report();fields.design="reply";MainWindow parent;parent.show();
        TemplatesDialog dialog(fields,false,*sstv::modeById("robot36"),"N0CALL",{},&parent);dialog.show();
        auto* designs=dialog.findChild<QListWidget*>("templateDesigns");auto* category=dialog.findChild<QComboBox*>("templateCategory");
        const auto click=[&](const QString& id,bool label=false){
            QListWidgetItem* target=nullptr;for(int i=0;i<designs->count();++i)if(designs->item(i)->data(Qt::UserRole).toString()==id)target=designs->item(i);
            QVERIFY(target);designs->scrollToItem(target);QApplication::processEvents();const auto bounds=designs->visualItemRect(target);const auto position=label?QPoint(bounds.center().x(),bounds.bottom()-8):bounds.center();QVERIFY(designs->viewport()->rect().contains(position));QVERIFY2(designs->itemAt(position)==target,qPrintable(QString("%1: filter %2, rect %3,%4 %5x%6, current %7").arg(id).arg(category->currentIndex()).arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height()).arg(dialog.fields().design)));
            QTest::mouseClick(designs->viewport(),Qt::LeftButton,Qt::NoModifier,position);QTRY_COMPARE(dialog.fields().design,id);
        };
        click("weather");QVERIFY(dialog.findChild<QComboBox*>("templateWeather")->isVisible());QVERIFY(dialog.findChild<QLineEdit*>("templateTemperature")->isVisible());
        click("reply");click("weather");category->setCurrentIndex(2);category->setCurrentIndex(0);click("weather");
        category->setCurrentIndex(1);click("sunset");click("weather");click("sunset",true);click("weather",true);
        auto expected=fields;expected.design="weather";QCOMPARE(dialog.image(),sstv::composeTransmitImage(sstv::renderTemplate(expected,{320,240},"N0CALL"),*sstv::modeById("robot36"),{}).image);
        dialog.findChild<QPushButton*>("templateApply")->click();QCOMPARE(dialog.result(),int(QDialog::Accepted));QCOMPARE(dialog.fields().design,QString("weather"));parent.close();
    }
    void exchangeCardsIgnoreWeatherAndDecode(){
        for(const auto& design:sstv::imageTemplates())if(!design.weatherCard&&!design.tvCard){
            auto fields=report();fields.design=design.id;fields.message="Thanks for the QSO!";const auto image=sstv::renderTemplate(fields,{320,240},"N0CALL");
            fields.weather="Snow";fields.temperature="-20";QCOMPARE(sstv::renderTemplate(fields,{320,240},"N0CALL"),image);
            const auto prepared=(image);const auto decoded=sstv::decode(sstv::encode(prepared,*sstv::modeById("robot36"),12000));QVERIFY(decoded.complete);QCOMPARE(decoded.image.size(),prepared.size());
        }
    }
    void recipientReportAndPicture(){
        auto fields=report();fields.design="report";fields.toCall="K1ABC";fields.report="GOOD COPY";
        const auto first=sstv::renderTemplate(fields,{320,240},"N0CALL");fields.toCall="K2XYZ";QVERIFY(sstv::renderTemplate(fields,{320,240},"N0CALL")!=first);
        fields.toCall="K1ABC";fields.report="WEAK PICTURE";QVERIFY(sstv::renderTemplate(fields,{320,240},"N0CALL")!=first);
        QImage photo(640,480,QImage::Format_RGB32);photo.fill(QColor("#964a37"));fields.design="photo";
        const auto plain=sstv::renderTemplate(fields,{320,240},"N0CALL",photo);fields.usePicture=true;const auto withPhoto=sstv::renderTemplate(fields,{320,240},"N0CALL",photo);QVERIFY(withPhoto!=plain);QCOMPARE(withPhoto.pixelColor(160,120),photo.pixelColor(320,240));
        const auto prepared=(withPhoto);QVERIFY(sstv::decode(sstv::encode(prepared,*sstv::modeById("robot36"),12000)).complete);
        TemplatesDialog dialog(fields,true,*sstv::modeById("robot36"),"N0CALL",{},nullptr,photo);dialog.show();QVERIFY(dialog.findChild<QCheckBox*>("templateUsePicture")->isChecked());
        dialog.findChild<QPushButton*>("templateClear")->click();QVERIFY(dialog.fields().toCall.isEmpty());QVERIFY(dialog.fields().report.isEmpty());QVERIFY(!dialog.fields().usePicture);dialog.reject();QVERIFY(fields.usePicture);
        QSettings settings;fields.save(settings);const auto loaded=sstv::TemplateFields::load(settings);QVERIFY(loaded.toCall.isEmpty());QVERIFY(loaded.report.isEmpty());QVERIFY(!loaded.usePicture);
        QVERIFY(!settings.contains("tx/template/toCall"));QVERIFY(!settings.contains("tx/template/report"));
        for(const auto& design:sstv::imageTemplates())if(design.weatherCard){
            fields.design=design.id;const auto weather=sstv::renderTemplate(fields,{320,240},"N0CALL");QCOMPARE(sstv::renderTemplate(fields,{320,240},"N0CALL",photo),weather);
        }
        for(const auto* removed:{"cosmic","arcade","tropical","robot","campfire","vibes"})QVERIFY(!sstv::imageTemplateById(removed));
    }
    void applyCancelAndPictureRestore(){
        QSettings settings;settings.setValue("station/call","N0CALL");MainWindow window;window.show();QVERIFY(window.findChild<QPushButton*>("templatesButton"));
        editTemplate(window,[](TemplatesDialog* dialog){dialog->findChild<QLineEdit*>("templateCity")->setText("Austin");dialog->findChild<QLineEdit*>("templateState")->setText("Texas");dialog->findChild<QLineEdit*>("templateTemperature")->setText("72");dialog->findChild<QComboBox*>("templateWeather")->setCurrentText("Partly cloudy");dialog->findChild<QPushButton*>("templateApply")->click();});
        QVERIFY(!settings.contains("tx/template/enabled"));QVERIFY(!settings.contains("tx/template/city"));
        editTemplate(window,[](TemplatesDialog* dialog){QVERIFY(dialog->enabled());QCOMPARE(dialog->fields().city,QString("Austin"));dialog->findChild<QLineEdit*>("templateCity")->setText("Discard this change");dialog->reject();});
        editTemplate(window,[](TemplatesDialog* dialog){QCOMPARE(dialog->fields().city,QString("Austin"));dialog->findChild<QPushButton*>("templatePictureOnly")->click();});
        editTemplate(window,[](TemplatesDialog* dialog){QVERIFY(!dialog->enabled());QVERIFY(dialog->fields().city.isEmpty());dialog->reject();});
        window.close();MainWindow reopened;reopened.show();editTemplate(reopened,[](TemplatesDialog* dialog){QVERIFY(!dialog->enabled());QVERIFY(dialog->fields().city.isEmpty());QVERIFY(dialog->fields().message.isEmpty());QVERIFY(dialog->fields().date.isNull());QVERIFY(dialog->fields().time.isNull());dialog->reject();});reopened.close();
    }
    void templateCaptionAndTransmission_data(){
        QTest::addColumn<QString>("design");QTest::newRow("weather-card")<<QString("aurora");QTest::newRow("exchange-card")<<QString("reply");QTest::newRow("photo-card")<<QString("photo");
    }
    void templateCaptionAndTransmission(){
        QFETCH(QString,design);
        QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("audio/output","fake-template-device");
        auto fields=report();fields.design=design;
        if(design!="aurora")fields.toCall="K1ABC";
        QImage photo;const auto photoPath=directory_.filePath("background.png");
        if(design=="photo"){fields.usePicture=true;photo=QImage(640,480,QImage::Format_RGB32);photo.fill(QColor("#964a37"));QVERIFY(photo.save(photoPath));settings.setValue("pictures/stock",QStringList{photoPath});}
        sstv::TextOverlay overlay;overlay.text="73 📻";overlay.position=8;overlay.sizePercent=5;settings.setValue("tx/caption/position",overlay.position);settings.setValue("tx/caption/size",overlay.sizePercent);
        MainWindow window;window.show();auto* mode=window.findChild<QComboBox*>("transmitMode");mode->setCurrentIndex(mode->findData("robot36"));
        if(design=="photo"){auto* stock=window.findChild<QListWidget*>("stockPictures");QMetaObject::invokeMethod(stock,"itemClicked",Q_ARG(QListWidgetItem*,stock->item(0)));}
        editTemplate(window,[&](TemplatesDialog* dialog){
            auto* designs=dialog->findChild<QListWidget*>("templateDesigns");for(int i=0;i<designs->count();++i)if(designs->item(i)->data(Qt::UserRole).toString()==design)designs->setCurrentRow(i);
            dialog->findChild<QLineEdit*>("templateToCall")->setText(fields.toCall);
            dialog->findChild<QCheckBox*>("templateUsePicture")->setChecked(fields.usePicture);
            dialog->findChild<QLineEdit*>("templateCity")->setText(fields.city);dialog->findChild<QLineEdit*>("templateState")->setText(fields.state);dialog->findChild<QComboBox*>("templateWeather")->setCurrentText(fields.weather);dialog->findChild<QLineEdit*>("templateTemperature")->setText(fields.temperature);dialog->findChild<QComboBox*>("templateTimeZone")->setCurrentIndex(1);dialog->findChild<QLineEdit*>("templateDate")->setText(fields.date.toString(Qt::ISODate));dialog->findChild<QLineEdit*>("templateTime")->setText(fields.time.toString("HH:mm"));dialog->findChild<QPushButton*>("templateApply")->click();
        });
        const auto expected=(sstv::composeTransmitImage(sstv::renderTemplate(fields,{320,240},"N0CALL",photo),*sstv::modeById("robot36"),{},overlay).image);
        editCaption(window,[&](CompositionDialog* dialog){dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText(overlay.text);dialog->accept();});
        window.findChild<QPushButton*>("playButton")->click();QVERIFY(!window.findChild<QPushButton*>("templatesButton")->isEnabled());
        const auto copy=directory_.filePath("tx.wav");QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(copy),5000);const auto decoded=sstv::decode(sstv::readWav(copy));QVERIFY(decoded.complete);QCOMPARE(decoded.image,decodedReference(expected));
        QTRY_VERIFY(window.findChild<QPushButton*>("templatesButton")->isEnabled());window.close();if(design=="photo")QCOMPARE(QImage(photoPath),photo);
    }
    void staleContentAndNewPictureStartBlank(){
        QSettings settings;settings.setValue("station/call","N0CALL");settings.setValue("tx/caption/text","OLD CAPTION");settings.setValue("tx/template/enabled",true);settings.setValue("tx/template/city","OLD CITY");settings.setValue("tx/template/message","OLD GREETING");settings.setValue("tx/template/date",QDate(2000,1,1));
        QImage photo(320,240,QImage::Format_RGB32);photo.fill(Qt::blue);const auto path=directory_.filePath("photo.png");QVERIFY(photo.save(path));settings.setValue("pictures/stock",QStringList{path});
        MainWindow window;window.show();QCOMPARE(window.findChild<QLineEdit*>("stationCall")->text(),QString("N0CALL"));QVERIFY(!settings.contains("tx/caption/text"));QVERIFY(!settings.contains("tx/template/city"));
        editCaption(window,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());dialog->findChild<QPlainTextEdit*>("compositionText")->setPlainText("CURRENT CAPTION");dialog->accept();});
        editTemplate(window,[](TemplatesDialog* dialog){QVERIFY(dialog->fields().city.isEmpty());QVERIFY(dialog->fields().message.isEmpty());QVERIFY(dialog->fields().date.isNull());QVERIFY(dialog->fields().time.isNull());QVERIFY(dialog->captionText().isEmpty());dialog->reject();});
        editCaption(window,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());dialog->reject();});
        QVERIFY(!window.findChild<CaptionPreview*>("transmitImage")->captionWidgetRect().isEmpty());
        auto* stock=window.findChild<QListWidget*>("stockPictures");QMetaObject::invokeMethod(stock,"itemClicked",Q_ARG(QListWidgetItem*,stock->item(0)));
        editCaption(window,[](CompositionDialog* dialog){QVERIFY(dialog->overlay().text.isEmpty());dialog->reject();});
        editTemplate(window,[](TemplatesDialog* dialog){QVERIFY(dialog->fields().city.isEmpty());QVERIFY(dialog->fields().message.isEmpty());QVERIFY(dialog->fields().date.isNull());QVERIFY(dialog->fields().time.isNull());dialog->reject();});window.close();
    }
    void clearAllAndCancel(){
        sstv::TextOverlay caption;caption.text="OLD CAPTION";TemplatesDialog dialog(report(),true,*sstv::modeById("robot36"),"N0CALL",caption);dialog.show();
        auto* date=dialog.findChild<QLineEdit*>("templateDate");date->setText("2026-02-30");QVERIFY(!dialog.findChild<QPushButton*>("templateApply")->isEnabled());date->clear();QVERIFY(dialog.findChild<QPushButton*>("templateApply")->isEnabled());
        auto* time=dialog.findChild<QLineEdit*>("templateTime");time->setText("99:99");QVERIFY(!dialog.findChild<QPushButton*>("templateApply")->isEnabled());
        dialog.findChild<QPushButton*>("templateClear")->click();
        for(const auto* name:{"templateToCall","templateReport","templateCity","templateState","templateTemperature","templateDate","templateTime","templateMessage"})QVERIFY(dialog.findChild<QLineEdit*>(name)->text().isEmpty());
        QVERIFY(dialog.findChild<QComboBox*>("templateWeather")->currentText().isEmpty());QVERIFY(dialog.captionText().isEmpty());QVERIFY(dialog.findChild<QPushButton*>("templateApply")->isEnabled());QVERIFY(dialog.fields().validate().isEmpty());dialog.reject();QCOMPARE(caption.text,QString("OLD CAPTION"));
    }

    void cleanupTestCase(){qputenv("PATH",oldPath_);}
};
int main(int argc,char** argv){
    QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QDir().mkpath(env.filePath("config"));QFile dirs(env.filePath("config/user-dirs.dirs"));if(!dirs.open(QIODevice::WriteOnly))return 1;dirs.write("XDG_PICTURES_DIR=\""+env.filePath("pictures").toUtf8()+"\"\n");dirs.close();QApplication app(argc,argv);TemplateTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "template_tests.moc"
