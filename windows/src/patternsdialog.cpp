#include "patternsdialog.h"
#include "core/patterns.h"
#include "imageview.h"
#include <QtWidgets>

PatternsDialog::PatternsDialog(const QString& selected,QWidget* parent):QDialog(parent){
    setObjectName("patternsDialog");setWindowTitle("Classic SSTV test patterns");resize(1040,700);
    auto* root=new QVBoxLayout(this);auto* body=new QHBoxLayout;root->addLayout(body,1);
    patterns_=new QListWidget;patterns_->setObjectName("testPatternList");patterns_->setViewMode(QListView::IconMode);patterns_->setMovement(QListView::Static);patterns_->setResizeMode(QListView::Adjust);patterns_->setWrapping(true);patterns_->setWordWrap(true);patterns_->setIconSize({120,90});patterns_->setGridSize({140,132});patterns_->setMinimumWidth(440);patterns_->setMaximumWidth(480);body->addWidget(patterns_);
    auto* details=new QVBoxLayout;body->addLayout(details,1);name_=new QLabel;name_->setObjectName("testPatternName");name_->setStyleSheet("font-size:20px;font-weight:bold;");name_->setWordWrap(true);details->addWidget(name_);
    era_=new QLabel;era_->setObjectName("testPatternEra");era_->setWordWrap(true);details->addWidget(era_);
    preview_=new ImageView;preview_->setObjectName("testPatternPreview");preview_->setMinimumHeight(280);details->addWidget(preview_,1);
    description_=new QLabel;description_->setObjectName("testPatternDescription");description_->setWordWrap(true);description_->setMinimumHeight(80);details->addWidget(description_);
    auto* hint=new QLabel("Choose a pattern, then Use pattern. It becomes your TX picture; add your callsign or text and send it using the usual TX controls.");hint->setWordWrap(true);root->addWidget(hint);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("Use pattern");buttons->button(QDialogButtonBox::Ok)->setObjectName("useTestPattern");root->addWidget(buttons);
    int row=0,current=0;for(const auto& pattern:sstv::testPatterns()){
        auto* item=new QListWidgetItem(QIcon(QPixmap::fromImage(sstv::renderTestPattern(pattern.id).scaled(120,90,Qt::KeepAspectRatio,Qt::SmoothTransformation))),pattern.name,patterns_);item->setData(Qt::UserRole,pattern.id);item->setToolTip(pattern.era+"\n"+pattern.description);if(pattern.id==selected)current=row;++row;
    }
    connect(patterns_,&QListWidget::currentRowChanged,this,&PatternsDialog::updatePreview);patterns_->setCurrentRow(current);patterns_->scrollToItem(patterns_->currentItem());
    connect(patterns_,&QListWidget::itemDoubleClicked,this,[this]{accept();});connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
}
QString PatternsDialog::selectedId() const{return patterns_->currentItem()?patterns_->currentItem()->data(Qt::UserRole).toString():QString{};}
void PatternsDialog::updatePreview(){const auto* pattern=sstv::testPatternById(selectedId());if(!pattern)return;name_->setText(pattern->name);era_->setText(pattern->era);description_->setText(pattern->description);preview_->setImage(sstv::renderTestPattern(pattern->id));}
