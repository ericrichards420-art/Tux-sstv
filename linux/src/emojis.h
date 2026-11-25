#pragma once
#include <QString>
#include <QStringList>
#include <vector>
#include <QTextOption>
class QTextDocument;
namespace ui {
struct Emoji {QString symbol,name,category,keywords;bool skinToneVariant=false;};
const std::vector<Emoji>& emojis();
const QStringList& emojiCategories();
const Emoji* findEmoji(const QString& symbol);
// Tones 0..5: default or one of five tones. Tone 6: all exact RGI variants.
std::vector<const Emoji*> emojiChoices(int tone=0);
QString emojiWithSkinTone(const Emoji& emoji,int tone);
bool emojiMatches(const Emoji& emoji,const QString& query);
QTextOption emojiTextOption();
QList<QPair<int,int>> emojiSpans(const QString& text);
void formatEmojiText(QTextDocument& document);
}
