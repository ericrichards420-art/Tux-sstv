#pragma once
#include "core/composition.h"
#include <QDialog>
class QPlainTextEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QListWidget;
class QLabel;
class QDialogButtonBox;
class CaptionPreview;

class CompositionDialog:public QDialog {
    Q_OBJECT
public:
    CompositionDialog(const QImage& source,const sstv::Mode& mode,const QString& callsign,const sstv::TextOverlay& overlay,const QStringList& recent,int emojiTone=0,QWidget* parent=nullptr,const sstv::CallsignPlacement& callsignPlacement={},const QList<sstv::TextOverlay>& placedCaptions={},int insertIndex=-1,const sstv::ImageInset& inset={},bool grayscalePreview=false);
    sstv::TextOverlay overlay() const {return overlay_;}
    sstv::CallsignPlacement callsignPlacement() const{return callsignPlacement_;}
    QStringList recentEmojis() const {return recent_;}
    int emojiTone() const {return selectedEmojiTone_;}
public slots:
    void accept() override;
private:
    QImage source_;
    const sstv::Mode* mode_;
    QString callsign_;
    sstv::TextOverlay overlay_;
    sstv::ImageInset inset_;
    bool grayscalePreview_;
    QList<sstv::TextOverlay> placedCaptions_;
    int insertIndex_;
    sstv::CallsignPlacement callsignPlacement_;
    QStringList recent_;
    QPlainTextEdit* text_;
    QSpinBox* size_;
    QComboBox *color_,*position_,*category_,*tone_;
    QCheckBox *bold_,*outline_,*backing_;
    QLineEdit* search_;
    QListWidget* emojiGrid_;
    QLabel *status_,*emojiHint_;
    QDialogButtonBox* buttons_;
    CaptionPreview* preview_;
    bool valid_=true;
    int selectedEmojiTone_=0;
    void updatePreview();
    void populateEmojis();
    void insertEmoji(const QString& emoji);
};
