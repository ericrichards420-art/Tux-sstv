#include "core/layouts.h"
#include "templatesdialog.h"
#include <QtTest>
#include <QtWidgets>

class LayoutTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    sstv::TemplateFields fields() const {sstv::TemplateFields f;f.design="reply";f.city="PRIVATE-CITY";f.state="PRIVATE-STATE";f.weather="PRIVATE-WEATHER";f.temperature="42";f.windSpeed="12.345";f.rainfall="0.678";f.humidity="67.89";f.pressure="987.65";f.windUnit="kn";f.rainUnit="mm";f.pressureUnit="hPa";f.textPositions["humidity"]={.4,.5};f.message="PRIVATE-MESSAGE";f.toCall="PRIVATE-CALL";f.report="PRIVATE-REPORT";f.headline="PRIVATE-HEADLINE";f.subheadline="PRIVATE-STORY";f.ticker="PRIVATE-TICKER";f.programInfo="PRIVATE-PROGRAM";f.homeTeam="PRIVATE-HOME";f.awayTeam="PRIVATE-AWAY";f.homeScore="9";f.awayScore="7";f.textPositions["headline"]={.1,.4};f.date={2042,7,13};f.time={17,43};f.usePicture=true;f.textPositions["callsign"]={.2,.25};f.textPositions["toCall"]={.3,.45};f.accentColor=QColor("#f39a56");f.textColor=QColor("#e0ffee");f.backgroundColor=QColor("#292341");return f;}
    sstv::TextOverlay caption() const {sstv::TextOverlay o;o.text="PRIVATE-CAPTION";o.color=Qt::yellow;o.sizePercent=9;o.freePosition=true;o.relativePosition={.4,.5};return o;}
private slots:
    void contentIsExcludedButStyleAndArtworkSurvive(){
        sstv::LayoutStore store(directory_.filePath("model"));QImage picture(80,60,QImage::Format_ARGB32);picture.fill(QColor(30,150,80,100));picture.setText("Callsign","PRIVATE-METADATA");QString error;
        QVERIFY(store.save("My exchange",fields(),caption(),picture,error));const auto layouts=store.list(error);QVERIFY(error.isEmpty());QCOMPARE(layouts.size(),1);const auto loaded=layouts.front();
        QCOMPARE(loaded.fields.design,QString("reply"));QVERIFY(loaded.fields.city.isEmpty());QVERIFY(loaded.fields.state.isEmpty());QVERIFY(loaded.fields.weather.isEmpty());QVERIFY(loaded.fields.temperature.isEmpty());QVERIFY(loaded.fields.windSpeed.isEmpty());QVERIFY(loaded.fields.rainfall.isEmpty());QVERIFY(loaded.fields.humidity.isEmpty());QVERIFY(loaded.fields.pressure.isEmpty());QCOMPARE(loaded.fields.windUnit,QString("kn"));QCOMPARE(loaded.fields.rainUnit,QString("mm"));QCOMPARE(loaded.fields.pressureUnit,QString("hPa"));QVERIFY(loaded.fields.message.isEmpty());QVERIFY(loaded.fields.toCall.isEmpty());QVERIFY(loaded.fields.report.isEmpty());QVERIFY(loaded.fields.headline.isEmpty());QVERIFY(loaded.fields.subheadline.isEmpty());QVERIFY(loaded.fields.ticker.isEmpty());QVERIFY(loaded.fields.programInfo.isEmpty());QVERIFY(loaded.fields.homeTeam.isEmpty());QVERIFY(loaded.fields.awayTeam.isEmpty());QVERIFY(loaded.fields.homeScore.isEmpty());QVERIFY(loaded.fields.awayScore.isEmpty());QVERIFY(!loaded.fields.date.isValid());QVERIFY(!loaded.fields.time.isValid());QVERIFY(loaded.caption.text.isEmpty());
        QCOMPARE(loaded.fields.textPositions,fields().textPositions);QCOMPARE(loaded.fields.accentColor,fields().accentColor);QCOMPARE(loaded.fields.textColor,fields().textColor);QCOMPARE(loaded.fields.backgroundColor,fields().backgroundColor);QCOMPARE(loaded.caption.color,caption().color);QCOMPARE(loaded.caption.relativePosition,caption().relativePosition);QVERIFY(loaded.caption.freePosition);QVERIFY(!loaded.picture.isNull());QVERIFY(loaded.picture.textKeys().isEmpty());QVERIFY(qAlpha(loaded.picture.pixel(0,0))<255);
        for(const auto& file:QDir(directory_.filePath("model")).entryList({"*.json"},QDir::Files)){QFile source(directory_.filePath("model/")+file);QVERIFY(source.open(QIODevice::ReadOnly));const auto data=source.readAll();QVERIFY(!data.contains("PRIVATE-"));for(const auto value:{"12.345","0.678","67.89","987.65"})QVERIFY(!data.contains(value));QVERIFY(!data.contains("2042-07-13"));QVERIFY(!data.contains("17:43"));}
        QVERIFY(store.save("My exchange",fields(),caption(),picture,error));QCOMPARE(store.list(error).size(),1);
    }
    void defaultsAndCustomColors(){
        const auto* mode=sstv::modeById("martin2");sstv::TemplateFields f;f.city="COLOR TEST";
        const auto original=sstv::renderTemplate(f,{mode->width,mode->height},"K9TEST");f.accentColor=Qt::red;f.textColor=Qt::yellow;f.backgroundColor=Qt::darkGreen;
        QVERIFY(sstv::renderTemplate(f,{mode->width,mode->height},"K9TEST")!=original);f.accentColor=QColor{};f.textColor=QColor{};f.backgroundColor=QColor{};QCOMPARE(sstv::renderTemplate(f,{mode->width,mode->height},"K9TEST"),original);
        sstv::LayoutStore store(directory_.filePath("invalid"));QString error;QVERIFY(!store.save("",f,{}, {},error));QVERIFY(!error.isEmpty());QDir().mkpath(directory_.filePath("invalid"));QFile file(directory_.filePath("invalid/broken.json"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("broken");file.close();QVERIFY(store.list(error).isEmpty());QVERIFY(!error.isEmpty());
    }
    void saveAndLoadThroughEditor_data(){QTest::addColumn<QString>("design");QTest::newRow("weather")<<QString("weather");QTest::newRow("newsroom")<<QString("newsroom");QTest::newRow("scoreboard")<<QString("scoreboard");}
    void saveAndLoadThroughEditor(){
        QFETCH(QString,design);auto f=fields();f.design=design;f.usePicture=false;const auto* mode=sstv::modeById("martin2");TemplatesDialog dialog(f,true,*mode,"K9TEST",caption());dialog.show();
        bool prompted=false;QTimer::singleShot(20,&dialog,[&]{auto* prompt=dialog.findChild<QInputDialog*>("layoutNameDialog");if(!prompt)return;prompted=true;prompt->setTextValue("My weather");prompt->accept();});
        QTest::mouseClick(dialog.findChild<QPushButton*>("saveLayout"),Qt::LeftButton);QVERIFY(prompted);
        auto* saved=dialog.findChild<QComboBox*>("savedLayouts");QCOMPARE(saved->currentText(),QString("My weather"));dialog.findChild<QLineEdit*>("templateCity")->setText("ANOTHER-CITY");dialog.findChild<QComboBox*>("templateCategory")->setCurrentIndex(2);
        QTest::mouseClick(dialog.findChild<QPushButton*>("loadLayout"),Qt::LeftButton);
        QVERIFY(dialog.findChild<QLineEdit*>("templateCity")->text().isEmpty());for(const auto* name:{"templateWindSpeed","templateRainfall","templateHumidity","templatePressure"})QVERIFY(dialog.findChild<QLineEdit*>(name)->text().isEmpty());QCOMPARE(dialog.fields().windUnit,QString("kn"));QCOMPARE(dialog.fields().pressureUnit,QString("hPa"));QVERIFY(dialog.findChild<QLineEdit*>("templateState")->text().isEmpty());QVERIFY(dialog.findChild<QLineEdit*>("templateDate")->text().isEmpty());QVERIFY(dialog.findChild<QLineEdit*>("templateTime")->text().isEmpty());QVERIFY(dialog.findChild<QLineEdit*>("templateMessage")->text().isEmpty());for(const auto* name:{"templateHeadline","templateSubheadline","templateTicker","templateProgramInfo","templateHomeTeam","templateAwayTeam","templateHomeScore","templateAwayScore"})QVERIFY(dialog.findChild<QLineEdit*>(name)->text().isEmpty());QVERIFY(dialog.findChild<QLineEdit*>("templateToCall")->text().isEmpty());QVERIFY(dialog.captionText().isEmpty());QCOMPARE(dialog.fields().textPositions,f.textPositions);QCOMPARE(dialog.fields().accentColor,f.accentColor);
        auto* designs=dialog.findChild<QListWidget*>("templateDesigns");QCOMPARE(designs->currentItem()->data(Qt::UserRole).toString(),design);QCOMPARE(dialog.findChild<QComboBox*>("templateCategory")->currentIndex(),0);
        QTest::mouseClick(dialog.findChild<QPushButton*>("templateDefaultColors"),Qt::LeftButton);QVERIFY(!dialog.fields().accentColor.isValid());QVERIFY(!dialog.fields().textColor.isValid());QVERIFY(!dialog.fields().backgroundColor.isValid());
        QTest::mouseClick(dialog.findChild<QPushButton*>("templateApply"),Qt::LeftButton);QCOMPARE(dialog.result(),int(QDialog::Accepted));
        TemplatesDialog fresh({},false,*mode,"K9TEST",{});QCOMPARE(fresh.findChild<QComboBox*>("savedLayouts")->currentIndex(),0);QVERIFY(fresh.fields().city.isEmpty());
    }
};
int main(int argc,char** argv){QTemporaryDir env;qputenv("XDG_CONFIG_HOME",env.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",env.filePath("data").toUtf8());QApplication app(argc,argv);app.setOrganizationName("OpenSSTVLayoutTests");app.setApplicationName("OpenSSTVLayoutTests");LayoutTests tests;return QTest::qExec(&tests,argc,argv);}
#include "layout_tests.moc"
