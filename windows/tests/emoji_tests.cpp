#include "emojis.h"
#include "composer.h"
#include "core/modes.h"
#include "core/composition.h"
#include "core/templates.h"
#include <QtTest>
#include <QtWidgets>
#include <QCryptographicHash>
#include <QGlyphRun>
#include <algorithm>

class EmojiTests:public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
    static QImage picture(){QImage image(320,256,QImage::Format_RGB32);image.fill(Qt::black);return image;}
private slots:
    void initTestCase(){
        QVERIFY(settings_.isValid());
        QCoreApplication::setOrganizationName("TuxSstvEmojiTests");QCoreApplication::setApplicationName("TuxSstvEmojiTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
        if(qEnvironmentVariableIsSet("TUX_TEST_NO_SYSTEM_EMOJI"))QVERIFY(!QFontDatabase::families().contains("Noto Color Emoji"));
        QVERIFY(ui::emojiFontFamilies().contains("Noto Color Emoji"));
    }
    void everyFlagRendersInColor(){
        const auto& mode=*sstv::modeById("martin2");int flags=0;
        for(const auto& emoji:ui::emojis())if(emoji.category=="Flags"){
            ++flags;sstv::TextOverlay overlay;overlay.text=emoji.symbol;overlay.sizePercent=18;overlay.position=4;overlay.outline=false;
            const auto image=sstv::composeTransmitImage(picture(),mode,{},overlay).image;int colored=0,visible=0;
            for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto p=image.pixel(x,y);if(std::max({qRed(p),qGreen(p),qBlue(p)})>30)++visible;if(std::max({qRed(p),qGreen(p),qBlue(p)})-std::min({qRed(p),qGreen(p),qBlue(p)})>30)++colored;}
            QVERIFY2(visible>30,qPrintable(emoji.name));
            // These flag symbols are intentionally monochrome.
            static const QSet<QString> monochrome={"🏳️","🏴","🏁","🏴‍☠️"};
            if(monochrome.contains(emoji.symbol))continue;
            QVERIFY2(colored>30,qPrintable(emoji.name+" "+emoji.symbol));
        }
        QCOMPARE(flags,270);
    }
    void catalogMatchesCompleteUnicode17RgiSet(){
        QCOMPARE(ui::emojis().size(),size_t(3953));QList<QByteArray> symbols;QSet<QString> unique;
        for(const auto& emoji:ui::emojis()){QVERIFY(!emoji.name.isEmpty());QVERIFY(ui::emojiCategories().contains(emoji.category));QVERIFY(!unique.contains(emoji.symbol));unique.insert(emoji.symbol);symbols<<emoji.symbol.toUtf8();}
        std::sort(symbols.begin(),symbols.end());QByteArray joined;for(const auto& symbol:symbols){if(!joined.isEmpty())joined+='\n';joined+=symbol;}
        // Independently extracted from ICU 78.2's normative RGI_Emoji set.
        QCOMPARE(QCryptographicHash::hash(joined,QCryptographicHash::Sha256).toHex(),QByteArray("e59a1ad6cad887d2c1ac15cb04859ec50aad8b0c1ef53d9714a586bd5c2ff400"));
        for(const auto* symbol:{"🫪","🫍","🫩","🫆","🧑‍🩰","🇨🇶","🇿🇦","🫱🏻‍🫲🏿","👩🏽‍🐰‍👩🏻"})QVERIFY2(ui::findEmoji(symbol),symbol);
    }
    void skinTonesUseValidSequencesInsteadOfCharacterInsertion(){
        for(const auto& pair:QList<QPair<QString,QString>>{{"👋","👋🏽"},{"☝️","☝🏽"},{"✌️","✌🏽"},{"👩‍🚀","👩🏽‍🚀"},{"🏄‍♀️","🏄🏽‍♀️"},{"🤝","🤝🏽"}}){const auto* entry=ui::findEmoji(pair.first);QVERIFY(entry);QCOMPARE(ui::emojiWithSkinTone(*entry,3),pair.second);QVERIFY(ui::findEmoji(pair.second));}
        for(int tone=0;tone<=6;++tone){QSet<QString> displayed;for(const auto* entry:ui::emojiChoices(tone)){QVERIFY(ui::findEmoji(entry->symbol));QVERIFY(!displayed.contains(entry->symbol));displayed.insert(entry->symbol);}if(tone==6)QCOMPARE(displayed.size(),3953);}
        const auto* flag=ui::findEmoji("🇨🇦");QVERIFY(flag);QCOMPARE(ui::emojiWithSkinTone(*flag,4),QString("🇨🇦"));
    }
    void searchIncludesNamesKeywordsAndRadioAliases(){
        for(const auto& pair:QList<QPair<QString,QString>>{{"👋","hello"},{"📻","ham radio"},{"📡","amateur antenna"},{"🇺🇸","USA"},{"🇬🇧","UK"},{"🫍","orca"},{"🫱🏻‍🫲🏿","handshake"}}){const auto* entry=ui::findEmoji(pair.first);QVERIFY(entry);QVERIFY2(ui::emojiMatches(*entry,pair.second),qPrintable(pair.second));}
    }
    void everyCatalogEntryShapesAsOneEmoji(){
        QFont font("Noto Color Emoji");font.setPixelSize(32);QStringList failures;
        for(const auto& emoji:ui::emojis()){
            QTextLayout layout(emoji.symbol,font);layout.setTextOption(ui::emojiTextOption());layout.beginLayout();layout.createLine();layout.endLayout();int glyphs=0;bool missing=false;
            for(const auto& run:layout.glyphRuns())for(const auto glyph:run.glyphIndexes()){++glyphs;if(!glyph)missing=true;}
            if(missing||glyphs!=1)failures<<emoji.name+QString(" (%1 glyphs)").arg(glyphs);
        }
        QVERIFY2(failures.isEmpty(),qPrintable(QString("%1 unsupported sequences: ").arg(failures.size())+failures.mid(0,20).join("; ")));
    }
    void fullPickerFiltersAndInsertsJoinedEmoji(){
        CompositionDialog dialog(picture(),*sstv::modeById("martin2"),{},{},{},6);dialog.show();QApplication::processEvents();
        auto* grid=dialog.findChild<QListWidget*>("emojiGrid");auto* search=dialog.findChild<QLineEdit*>("emojiSearch");auto* category=dialog.findChild<QComboBox*>("emojiCategory");auto* tone=dialog.findChild<QComboBox*>("emojiSkinTone");auto* text=dialog.findChild<QPlainTextEdit*>("compositionText");
        QCOMPARE(tone->currentIndex(),6);QCOMPARE(grid->count(),3953);QCOMPARE(dialog.emojiTone(),6);QCOMPARE(category->count(),12);
        category->setCurrentText("Flags");QCOMPARE(grid->count(),270);search->setText("Canada");QCOMPARE(grid->count(),1);QCOMPARE(grid->item(0)->data(Qt::UserRole).toString(),QString("🇨🇦"));
        search->clear();category->setCurrentIndex(0);search->setText("🫱🏻‍🫲🏿");QCOMPARE(grid->count(),1);grid->setCurrentRow(0);grid->setFocus();QTest::keyClick(grid,Qt::Key_Return);QCOMPARE(text->toPlainText(),QString("🫱🏻‍🫲🏿"));
        search->clear();category->setCurrentText("Recent");QCOMPARE(grid->count(),1);search->setText("handshake");QCOMPARE(grid->count(),1);
        category->setCurrentIndex(0);search->clear();tone->setCurrentIndex(0);QVERIFY(grid->count()>1800);QVERIFY(grid->count()<3953);
        const auto preview=qEnvironmentVariable("OPENSSTV_EMOJI_PREVIEW");if(!preview.isEmpty()){text->setPlainText("73! 📻 🫪 🫍 👩🏽‍🚀");QApplication::processEvents();QVERIFY(dialog.grab().save(preview));}
        dialog.reject();
    }
    void captionPreservesWordSpaces_data(){
        QTest::addColumn<bool>("emoji");QTest::newRow("plain text")<<false;QTest::newRow("text with emoji")<<true;
    }
    void captionPreservesWordSpaces(){
        QFETCH(bool,emoji);
        if(qEnvironmentVariableIsSet("TUX_TEST_NO_LINUX_TEXT_FONTS")){
            QVERIFY(!QFontDatabase::families().contains("Noto Sans"));QVERIFY(!QFontDatabase::families().contains("DejaVu Sans"));
        }
        const auto& mode=*sstv::modeById("martin2");sstv::TextOverlay overlay;overlay.sizePercent=10;overlay.outline=false;overlay.position=4;
        QList<int> widths;
        for(int spaces:{0,1,3}){const auto gap=QString(spaces,' ');overlay.text=emoji?"H"+gap+"😀"+gap+"H":"H"+gap+"H";const auto composed=sstv::composeTransmitImage(picture(),mode,{},overlay);widths<<composed.captionBounds.width();}
        QVERIFY2(widths[1]>widths[0]+2,qPrintable(QString("Word spaces disappeared: widths %1, %2").arg(widths[0]).arg(widths[1])));
        QVERIFY2(widths[2]>widths[1]+3,qPrintable(QString("Repeated spaces collapsed: widths %1, %2").arg(widths[1]).arg(widths[2])));
        if(qEnvironmentVariableIsSet("TUX_TEST_NO_LINUX_TEXT_FONTS")){
            QFont font=QFontDatabase::systemFont(QFontDatabase::GeneralFont);font.setPixelSize(32);font.setBold(true);
            const auto expected=QFontMetricsF(font).horizontalAdvance(' ')*(emoji?2:1);
            QVERIFY2(widths[1]-widths[0]<=expected*1.75+2,"Spaces must use the regular text font, not a color emoji font's metrics");
        }
        const QString text="CQ  CQ\n73   😀  de KI5BZO";CompositionDialog dialog(picture(),mode,{},{},{},6);dialog.findChild<QPlainTextEdit*>("compositionText")->setPlainText(text);QCOMPARE(dialog.overlay().text,text);
    }
    void templateFieldsPreserveWordSpaces_data(){QTest::addColumn<QString>("design");QTest::newRow("weather")<<QString("weather");QTest::newRow("CQ")<<QString("cq");QTest::newRow("news")<<QString("news");}
    void templateFieldsPreserveWordSpaces(){
        QFETCH(QString,design);sstv::TemplateFields fields;fields.design=design;QList<int> widths;
        for(int spaces:{0,1,3}){fields.message="H"+QString(spaces,' ')+"H";QMap<QString,QRect> bounds;const auto image=sstv::renderTemplate(fields,{640,480},{},{},&bounds,false);QVERIFY(!image.isNull());QVERIFY(bounds.contains("message"));widths<<bounds["message"].width();}
        QVERIFY(widths[1]>widths[0]+2);QVERIFY(widths[2]>widths[1]+3);
    }
    void pickerGlyphsFitInsideCells_data(){
        QTest::addColumn<QString>("symbol");QTest::addColumn<int>("pixels");QTest::addColumn<double>("scale");
        for(const auto& symbol:QStringList{"😀","🇨🇦","📻","👩🏽‍🚀"})for(int pixels:{26,72})for(double scale:{1.,1.25,1.5,2.})QTest::newRow(qPrintable(symbol+QString("-%1-%2").arg(pixels).arg(scale)))<<symbol<<pixels<<scale;
    }
    void pickerGlyphsFitInsideCells(){
        QFETCH(QString,symbol);QFETCH(int,pixels);QFETCH(double,scale);
        CompositionDialog dialog(picture(),*sstv::modeById("martin2"),{},{},{},6);auto* grid=dialog.findChild<QListWidget*>("emojiGrid");dialog.findChild<QLineEdit*>("emojiSearch")->setText(symbol);QVERIFY(grid->count()>0);
        auto* item=grid->item(0);QFont font=item->font();font.setPixelSize(pixels);item->setFont(font);
        QImage image(QSize(66,66)*scale,QImage::Format_ARGB32_Premultiplied);image.setDevicePixelRatio(scale);image.fill(Qt::black);
        QStyleOptionViewItem option;option.rect=QRect(10,10,46,46);option.widget=grid;option.state=QStyle::State_Enabled;option.palette.setColor(QPalette::Base,Qt::black);option.palette.setColor(QPalette::Text,Qt::white);
        {QPainter painter(&image);grid->itemDelegate()->paint(&painter,option,grid->model()->index(0,0));}
        QRect ink;int visible=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto color=image.pixelColor(x,y);if(std::max({color.red(),color.green(),color.blue()})>40){ink=ink.united(QRect(x,y,1,1));++visible;}}
        QVERIFY(visible>30);QVERIFY2(ink.left()>=int(12*scale)&&ink.top()>=int(12*scale)&&ink.right()<int(54*scale)&&ink.bottom()<int(54*scale),qPrintable(QString("Clipped picker glyph: ink %1,%2 %3x%4 at scale %5").arg(ink.x()).arg(ink.y()).arg(ink.width()).arg(ink.height()).arg(scale)));
    }
    void newEmojiRenderInTransmittedImage(){
        const auto& mode=*sstv::modeById("martin2");
        for(const auto* symbol:{"🫪","🫍","🫱🏻‍🫲🏿","👩🏽‍🐰‍👩🏻","🧑🏻‍🫯‍🧑🏼"}){sstv::TextOverlay overlay;overlay.text=symbol;overlay.sizePercent=18;overlay.position=4;overlay.outline=false;const auto image=sstv::composeTransmitImage(picture(),mode,{},overlay).image;int visible=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto p=image.pixel(x,y);if(qRed(p)>30||qGreen(p)>30||qBlue(p)>30)++visible;}QVERIFY2(visible>30,symbol);}
    }
};
QTEST_MAIN(EmojiTests)
#include "emoji_tests.moc"
