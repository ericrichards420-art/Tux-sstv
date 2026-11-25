#include "composer.h"
#include "emojis.h"
#include "captionpreview.h"
#include <QtWidgets>
#include <QTextBoundaryFinder>
#include <QGlyphRun>
#include <algorithm>

namespace {
class EmojiDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override{
        QStyleOptionViewItem panel(option);initStyleOption(&panel,index);const auto symbol=panel.text;panel.text.clear();
        const auto* style=panel.widget?panel.widget->style():QApplication::style();style->drawControl(QStyle::CE_ItemViewItem,&panel,painter,panel.widget);
        QTextLayout layout(symbol,index.data(Qt::FontRole).value<QFont>(),painter->device());layout.setTextOption(ui::emojiTextOption());layout.beginLayout();const auto line=layout.createLine();layout.endLayout();
        if(!line.isValid())return;
        QRectF bounds(0,0,line.naturalTextWidth(),line.height());
        // Bitmap color glyphs can extend beyond the line's typographic box,
        // especially with Windows display scaling. Fit their actual ink too.
        for(const auto& run:layout.glyphRuns()){
            const auto glyphs=run.glyphIndexes();const auto positions=run.positions();const auto font=run.rawFont();
            for(qsizetype n=0;n<std::min(glyphs.size(),positions.size());++n)bounds=bounds.united(font.boundingRect(glyphs[n]).translated(positions[n]));
        }
        const auto area=QRectF(option.rect).adjusted(4,4,-4,-4);if(bounds.isEmpty()||area.isEmpty())return;
        const auto scale=std::min({1.,area.width()/bounds.width(),area.height()/bounds.height()});
        painter->save();painter->setClipRect(option.rect,Qt::IntersectClip);painter->setPen(option.palette.color(option.state&QStyle::State_Selected?QPalette::HighlightedText:QPalette::Text));
        painter->setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing|QPainter::SmoothPixmapTransform);
        painter->translate(area.center());painter->scale(scale,scale);painter->translate(-bounds.center());layout.draw(painter,QPointF{});painter->restore();
    }
};
class EmojiHighlighter:public QSyntaxHighlighter {
public:
    explicit EmojiHighlighter(QTextDocument* document):QSyntaxHighlighter(document){}
    void highlightBlock(const QString& text) override{QTextCharFormat format;format.setFontFamilies({"Noto Color Emoji"});for(const auto& span:ui::emojiSpans(text))setFormat(span.first,span.second,format);}
};
}

CompositionDialog::CompositionDialog(const QImage& source,const sstv::Mode& mode,const QString& callsign,const sstv::TextOverlay& overlay,const QStringList& recent,int emojiTone,QWidget* parent,const sstv::CallsignPlacement& callsignPlacement,const QList<sstv::TextOverlay>& placedCaptions,int insertIndex,const sstv::ImageInset& inset,bool grayscalePreview)
    :QDialog(parent),source_(source),mode_(&mode),callsign_(callsign),overlay_(overlay),inset_(inset),grayscalePreview_(grayscalePreview),placedCaptions_(placedCaptions),insertIndex_(insertIndex<0?int(placedCaptions.size()):std::clamp(insertIndex,0,int(placedCaptions.size()))),callsignPlacement_(callsignPlacement),recent_(recent) {
    if(insertIndex<0){auto layers=placedCaptions_;layers<<overlay_;sstv::bringCaptionToFront(layers,insertIndex_);overlay_=layers[insertIndex_];}
    setObjectName("compositionDialog");setWindowTitle("Text & emoji · "+mode.name);resize(900,650);if(parent)setPalette(parent->palette());
    auto* root=new QVBoxLayout(this);auto* columns=new QHBoxLayout;root->addLayout(columns,1);
    auto* left=new QVBoxLayout;preview_=new CaptionPreview;preview_->setObjectName("compositionPreview");preview_->setAccessibleName("Draggable caption preview");left->addWidget(preview_,1);
    auto* resolution=new QLabel(QString("%1 × %2 · %3 · Transmit preview").arg(mode.width).arg(mode.height).arg(mode.name));left->addWidget(resolution);
    auto* dragHint=new QLabel(callsign.trimmed().isEmpty()?"Drag your text on the preview to move it.":"Drag your caption or callsign anywhere inside the preview.");dragHint->setWordWrap(true);left->addWidget(dragHint);
    status_=new QLabel;status_->setObjectName("compositionStatus");status_->setWordWrap(true);left->addWidget(status_);columns->addLayout(left,1);
    auto* right=new QVBoxLayout;columns->addLayout(right,1);
    right->addWidget(new QLabel("Your message"));text_=new QPlainTextEdit;text_->setObjectName("compositionText");{auto option=text_->document()->defaultTextOption();option.setFlags(option.flags()|ui::emojiTextOption().flags());text_->document()->setDefaultTextOption(option);new EmojiHighlighter(text_->document());}
    text_->setAccessibleName("Caption text");text_->setPlaceholderText("Add your message… 73! 📻");text_->setPlainText(overlay.text);text_->setFixedHeight(90);text_->setStyleSheet("font-size:16px;");right->addWidget(text_);
    auto* options=new QFormLayout;size_=new QSpinBox;size_->setObjectName("compositionSize");size_->setRange(3,18);size_->setSuffix(" %");size_->setValue(overlay.sizePercent);
    size_->setToolTip("Text size as a percentage of the image width. Text shrinks to fit if needed.");options->addRow("Size",size_);
    position_=new QComboBox;position_->setObjectName("compositionPosition");position_->addItems({"Top left","Top center","Top right","Middle left","Middle center","Middle right","Bottom left","Bottom center","Bottom right","Free position (drag)"});position_->setCurrentIndex(overlay.freePosition?9:std::clamp(overlay.position,0,8));options->addRow("Position",position_);
    auto* colors=new QHBoxLayout;color_=new QComboBox;color_->setObjectName("compositionColor");
    const QList<QPair<QString,QColor>> presets={{"White",Qt::white},{"Yellow",QColor("#ffeb3b")},{"Cyan",QColor("#40e8ff")},{"Orange",QColor("#ff9f43")},{"Red",QColor("#ff5067")},{"Green",QColor("#74f78f")},{"Violet",QColor("#cb9bff")},{"Black",Qt::black}};
    int selected=-1;for(const auto& [name,value]:presets){QPixmap swatch(14,14);swatch.fill(value);color_->addItem(QIcon(swatch),name,value);if(value==overlay.color)selected=color_->count()-1;}
    if(selected<0){QPixmap swatch(14,14);swatch.fill(overlay.color);color_->addItem(QIcon(swatch),"Custom",overlay.color);selected=color_->count()-1;}color_->setCurrentIndex(selected);
    auto* custom=new QPushButton("More…");colors->addWidget(color_,1);colors->addWidget(custom);options->addRow("Text color",colors);
    auto* styles=new QHBoxLayout;bold_=new QCheckBox("Bold");outline_=new QCheckBox("Outline");backing_=new QCheckBox("Dark background");
    bold_->setObjectName("compositionBold");outline_->setObjectName("compositionOutline");backing_->setObjectName("compositionBacking");
    bold_->setChecked(overlay.bold);outline_->setChecked(overlay.outline);backing_->setChecked(overlay.backing);styles->addWidget(bold_);styles->addWidget(outline_);styles->addWidget(backing_);options->addRow(styles);right->addLayout(options);
    auto* picker=new QGroupBox("Emoji");auto* pickerLayout=new QVBoxLayout(picker);search_=new QLineEdit;search_->setObjectName("emojiSearch");search_->setPlaceholderText("Search emoji… radio, smile, heart");pickerLayout->addWidget(search_);
    auto* filters=new QHBoxLayout;category_=new QComboBox;category_->setObjectName("emojiCategory");category_->addItems({"All","Recent"});category_->addItems(ui::emojiCategories());
    tone_=new QComboBox;tone_->setObjectName("emojiSkinTone");tone_->addItems({"Default tone","Light","Medium-light","Medium","Medium-dark","Dark","All skin tones"});tone_->setCurrentIndex(std::clamp(emojiTone,0,6));tone_->setToolTip("Choose a preferred tone, or All skin tones to browse every variant, including mixed-tone couples and handshakes.");filters->addWidget(category_,1);filters->addWidget(tone_);pickerLayout->addLayout(filters);
    emojiGrid_=new QListWidget;emojiGrid_->setObjectName("emojiGrid");emojiGrid_->setItemDelegate(new EmojiDelegate(emojiGrid_));emojiGrid_->setViewMode(QListView::IconMode);emojiGrid_->setResizeMode(QListView::Adjust);emojiGrid_->setMovement(QListView::Static);emojiGrid_->setLayoutMode(QListView::Batched);emojiGrid_->setBatchSize(120);emojiGrid_->setUniformItemSizes(true);emojiGrid_->setWordWrap(false);emojiGrid_->setGridSize(QSize(46,46));emojiGrid_->setMinimumHeight(140);pickerLayout->addWidget(emojiGrid_,1);
    emojiHint_=new QLabel("Click an emoji to insert at the text cursor.");emojiHint_->setWordWrap(true);pickerLayout->addWidget(emojiHint_);right->addWidget(picker,1);
    auto* footer=new QHBoxLayout;auto* clear=new QPushButton("Clear caption");clear->setObjectName("compositionClear");footer->addWidget(clear);
    if(!callsign.trimmed().isEmpty()){auto* resetCall=new QPushButton("Reset callsign position");resetCall->setObjectName("compositionResetCallsign");footer->addWidget(resetCall);connect(resetCall,&QPushButton::clicked,this,[this]{callsignPlacement_={};updatePreview();});}
    footer->addStretch();
    buttons_=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons_->button(QDialogButtonBox::Apply)->setDefault(true);footer->addWidget(buttons_);root->addLayout(footer);
    connect(buttons_->button(QDialogButtonBox::Apply),&QPushButton::clicked,this,&CompositionDialog::accept);connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(clear,&QPushButton::clicked,this,[this]{overlay_.freePosition=false;overlay_.relativePosition={};const QSignalBlocker block(position_);position_->setCurrentIndex(std::clamp(overlay_.position,0,8));text_->clear();updatePreview();});connect(text_,&QPlainTextEdit::textChanged,this,&CompositionDialog::updatePreview);
    connect(size_,&QSpinBox::valueChanged,this,&CompositionDialog::updatePreview);connect(color_,&QComboBox::currentIndexChanged,this,&CompositionDialog::updatePreview);
    connect(position_,&QComboBox::currentIndexChanged,this,[this](int index){
        if(index==9&&!overlay_.freePosition){const auto bounds=preview_->captionBounds();overlay_.relativePosition={double(bounds.x())/mode_->width,double(bounds.y())/mode_->height};}
        overlay_.freePosition=index==9;updatePreview();
    });
    preview_->textMoved=[this](const QString& key,QPointF position){
        if(key=="callsign"){callsignPlacement_.freePosition=true;callsignPlacement_.relativePosition=position;}
        else {overlay_.freePosition=true;overlay_.relativePosition=position;auto layers=placedCaptions_;layers.insert(insertIndex_,overlay_);sstv::bringCaptionToFront(layers,insertIndex_);overlay_.stackOrder=layers[insertIndex_].stackOrder;const QSignalBlocker block(position_);position_->setCurrentIndex(9);}
        updatePreview();
    };
    for(auto* option:{bold_,outline_,backing_})connect(option,&QCheckBox::toggled,this,&CompositionDialog::updatePreview);
    connect(custom,&QPushButton::clicked,this,[this] {
        const auto chosen=QColorDialog::getColor(overlay_.color,this,"Caption color");if(!chosen.isValid())return;
        QPixmap swatch(14,14);swatch.fill(chosen);color_->addItem(QIcon(swatch),"Custom",chosen);color_->setCurrentIndex(color_->count()-1);
    });
    connect(search_,&QLineEdit::textChanged,this,&CompositionDialog::populateEmojis);connect(category_,&QComboBox::currentIndexChanged,this,&CompositionDialog::populateEmojis);connect(tone_,&QComboBox::currentIndexChanged,this,&CompositionDialog::populateEmojis);
    connect(emojiGrid_,&QListWidget::itemClicked,this,[this](QListWidgetItem* item){insertEmoji(item->data(Qt::UserRole).toString());});
    for(const auto key:{Qt::Key_Return,Qt::Key_Enter}) {
        auto* insert=new QShortcut(QKeySequence(key),emojiGrid_);insert->setContext(Qt::WidgetWithChildrenShortcut);insert->setAutoRepeat(false);
        connect(insert,&QShortcut::activated,this,[this]{if(auto* item=emojiGrid_->currentItem())insertEmoji(item->data(Qt::UserRole).toString());});
    }
    updatePreview();populateEmojis();
}
void CompositionDialog::updatePreview() {
    overlay_.text=text_->toPlainText();overlay_.sizePercent=size_->value();if(position_->currentIndex()<9)overlay_.position=position_->currentIndex();overlay_.color=color_->currentData().value<QColor>();
    overlay_.bold=bold_->isChecked();overlay_.outline=outline_->isChecked();overlay_.backing=backing_->isChecked();
    int count=0;QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,overlay_.text);while(boundary.toNextBoundary()>=0)++count;
    valid_=count<=500 && overlay_.text.count('\n')<6 && overlay_.text.size()<=16384;buttons_->button(QDialogButtonBox::Apply)->setEnabled(valid_);
    if(!valid_){preview_->setCaptionBounds({});status_->setText("Keep your message to 500 characters and 6 lines.");return;}
    auto layers=placedCaptions_;layers.insert(insertIndex_,overlay_);
    const auto composed=sstv::composeTransmitLayers(source_,*mode_,callsign_,layers,callsignPlacement_,inset_);preview_->setImage(grayscalePreview_?composed.image.convertToFormat(QImage::Format_Grayscale8):composed.image);
    QMap<QString,QRect> regions;const auto bounds=composed.captionRects.value(insertIndex_);if(!bounds.isEmpty())regions["caption"]=bounds;if(!composed.callsignBounds.isEmpty())regions["callsign"]=composed.callsignBounds;preview_->setTextRegions(regions,{"caption","callsign"});
    status_->setText(QString("%1 / 500 characters%2").arg(count).arg(composed.reduced?" · Text reduced to fit":""));
}
void CompositionDialog::populateEmojis() {
    selectedEmojiTone_=tone_->currentIndex();
    emojiGrid_->setUpdatesEnabled(false);emojiGrid_->clear();const auto query=search_->text().trimmed(),category=category_->currentText();
    QFont font;font.setFamilies({"Noto Color Emoji","Noto Sans"});font.setPixelSize(26);
    auto add=[&](const QString& symbol,const QString& name) {
        auto* item=new QListWidgetItem(symbol,emojiGrid_);item->setData(Qt::UserRole,symbol);item->setData(Qt::AccessibleTextRole,name);item->setToolTip(name);item->setFont(font);item->setTextAlignment(Qt::AlignCenter);item->setSizeHint(QSize(46,46));
    };
    if(category=="Recent") {
        for(const auto& symbol:recent_) {
            if(const auto* entry=ui::findEmoji(symbol)){if(ui::emojiMatches(*entry,query))add(symbol,entry->name);}
            else if(query.isEmpty()||symbol.contains(query))add(symbol,"Recently used emoji");
        }
    }else for(const auto* entry:ui::emojiChoices(tone_->currentIndex()))if((category=="All"||entry->category==category)&&ui::emojiMatches(*entry,query))add(entry->symbol,entry->name);
    emojiGrid_->setUpdatesEnabled(true);
    emojiHint_->setText(emojiGrid_->count()?QString("%1 emoji · Click or press Enter to insert.").arg(emojiGrid_->count()):category=="Recent"?"Your recently used emoji will appear here.":"No matching emoji. Try another search.");
}
void CompositionDialog::insertEmoji(const QString& emoji) {
    auto cursor=text_->textCursor();cursor.insertText(emoji);text_->setTextCursor(cursor);
    recent_.removeAll(emoji);recent_.prepend(emoji);while(recent_.size()>16)recent_.removeLast();
    if(category_->currentText()=="Recent")populateEmojis();
}
void CompositionDialog::accept() {if(valid_)QDialog::accept();}
