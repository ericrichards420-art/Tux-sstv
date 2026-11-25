#include "emojis.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <array>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QFontDatabase>

static void initEmojiResources(){Q_INIT_RESOURCE(emoji_catalog);}
static void initEmojiFontResources(){Q_INIT_RESOURCE(emoji_font);}
namespace ui {
namespace {
bool modifier(char32_t cp){return cp>=0x1f3fb&&cp<=0x1f3ff;}
QString toneFamily(const QString& symbol){
    QString result;for(const auto cp:symbol.toUcs4())if(!modifier(cp)&&cp!=0xfe0f){const auto value=char32_t(cp);result+=QString::fromUcs4(&value,1);}return result;
}
struct Catalog {
    std::vector<Emoji> entries;
    QStringList categories;
    QHash<QString,size_t> index;
    QHash<QString,std::array<QString,5>> tones;
    QHash<QString,QList<QString>> prefixes;
    Catalog(){
        initEmojiResources();QFile file(":/emoji/catalog.json");
        if(!file.open(QIODevice::ReadOnly))qFatal("Embedded emoji catalog is missing");
        const auto document=QJsonDocument::fromJson(file.readAll());const auto root=document.object();
        const auto rows=root.value("entries").toArray();if(rows.size()!=3953||root.value("unicodeVersion").toString()!="17.0")qFatal("Invalid embedded Emoji 17.0 catalog");
        for(const auto category:root.value("categories").toArray())categories<<category.toString();
        entries.reserve(rows.size());
        for(const auto row:rows){const auto object=row.toObject();Emoji entry{object.value("symbol").toString(),object.value("name").toString(),object.value("category").toString(),object.value("keywords").toString()};
            int tone=0;bool mixed=false;
            for(const auto cp:entry.symbol.toUcs4())if(modifier(cp)){const int next=int(cp)-0x1f3fa;if(tone&&tone!=next)mixed=true;tone=next;}
            entry.skinToneVariant=tone&&entry.category!="Components";
            // Look up only real RGI sequences. Inserting a modifier after the
            // first UTF-16 character breaks variation selectors and couples.
            if(entry.skinToneVariant&&!mixed)tones[toneFamily(entry.symbol)][tone-1]=entry.symbol;
            index.insert(entry.symbol,entries.size());prefixes[entry.symbol.left(entry.symbol.front().isHighSurrogate()?2:1)]<<entry.symbol;entries.push_back(std::move(entry));
        }
    }
};
const Catalog& catalog(){static const Catalog value;return value;}
}
const std::vector<Emoji>& emojis(){return catalog().entries;}
const QStringList& emojiCategories(){return catalog().categories;}
const Emoji* findEmoji(const QString& symbol){const auto& c=catalog();const auto found=c.index.constFind(symbol);return found==c.index.cend()?nullptr:&c.entries[*found];}
QString emojiWithSkinTone(const Emoji& emoji,int tone){
    if(tone<1||tone>5)return emoji.symbol;
    const auto& families=catalog().tones;const auto found=families.constFind(toneFamily(emoji.symbol));
    if(found==families.cend()||(*found)[tone-1].isEmpty())return emoji.symbol;
    return (*found)[tone-1];
}
std::vector<const Emoji*> emojiChoices(int tone){
    std::vector<const Emoji*> choices;choices.reserve(emojis().size());
    for(const auto& entry:emojis()){
        if(tone==6){choices.push_back(&entry);continue;}
        if(entry.skinToneVariant)continue;
        const auto* selected=tone>0?findEmoji(emojiWithSkinTone(entry,tone)):nullptr;choices.push_back(selected?selected:&entry);
    }
    return choices;
}
QStringList emojiFontFamilies(){
    static const QStringList families=[] {
        initEmojiFontResources();
        const int id=QFontDatabase::addApplicationFont(":/emoji/fonts/NotoColorEmoji.ttf");
        const auto loaded=id<0?QStringList{}:QFontDatabase::applicationFontFamilies(id);
        if(loaded.isEmpty())qFatal("Cannot load Tux-sstv's bundled Noto Color Emoji font. Use Qt 6.9 or newer on Windows.");
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
        QFontDatabase::setApplicationEmojiFontFamilies(loaded);
#endif
        return loaded;
    }();
    return families;
}
QTextOption emojiTextOption(){
    QTextOption option;
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    // Qt's built-in emoji parser predates some Emoji 17 sequences and splits
    // valid ligatures. Explicit emoji fonts let HarfBuzz shape the full text.
    option.setFlags(QTextOption::DisableEmojiParsing);
#endif
    return option;
}
QList<QPair<int,int>> emojiSpans(const QString& text){
    QList<QPair<int,int>> spans;const auto& prefixes=catalog().prefixes;
    for(qsizetype position=0;position<text.size();){
        const int units=text[position].isHighSurrogate()?2:1;const auto found=prefixes.constFind(text.mid(position,units));qsizetype length=0;
        if(found!=prefixes.cend())for(const auto& symbol:*found)if(symbol.size()>length&&QStringView(text).mid(position).startsWith(symbol))length=symbol.size();
        if(length){spans<<qMakePair(int(position),int(length));position+=length;}else position+=units;
    }
    return spans;
}
void formatEmojiText(QTextDocument& document){
    QTextCharFormat format;format.setFontFamilies(emojiFontFamilies());
    for(auto block=document.begin();block.isValid();block=block.next())for(const auto& span:emojiSpans(block.text())){QTextCursor cursor(&document);cursor.setPosition(block.position()+span.first);cursor.setPosition(block.position()+span.first+span.second,QTextCursor::KeepAnchor);cursor.mergeCharFormat(format);}
}
bool emojiMatches(const Emoji& emoji,const QString& query){
    if(query.isEmpty()||emoji.symbol.contains(query))return true;
    const auto text=emoji.name+" "+emoji.keywords+" "+emoji.category;
    for(const auto& word:query.simplified().split(' ',Qt::SkipEmptyParts))if(!text.contains(word,Qt::CaseInsensitive))return false;
    return true;
}
}
