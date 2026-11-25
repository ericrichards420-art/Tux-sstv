#include "insetdialog.h"
#include "captionpreview.h"
#include <QtWidgets>
#include <algorithm>
#include <cmath>

InsetDialog::InsetDialog(const sstv::ImageInset& inset,const QImage& background,const sstv::Mode& mode,const QString& callsign,const QList<sstv::TextOverlay>& captions,const sstv::CallsignPlacement& callPlacement,const QImage& received,const QString& historyFolder,QWidget* parent,bool grayscalePreview)
    :QDialog(parent),inset_(inset),grayscalePreview_(grayscalePreview),background_(background),received_(received),mode_(mode),callsign_(callsign),historyFolder_(historyFolder),captions_(captions),callPlacement_(callPlacement){
    setObjectName("pictureInPictureDialog");setWindowTitle("Picture in picture · Received image");resize(900,620);setCursor(Qt::ArrowCursor);if(parent)setPalette(parent->palette());
    auto* root=new QVBoxLayout(this);auto* columns=new QHBoxLayout;root->addLayout(columns,1);
    preview_=new CaptionPreview;preview_->setObjectName("insetPreview");preview_->setToolTip("Drag the received picture to position it.");columns->addWidget(preview_,1);
    auto* tools=new QVBoxLayout;columns->addLayout(tools);auto* title=new QLabel("Show their picture as received");title->setStyleSheet("font-size:17px;font-weight:bold;");tools->addWidget(title);
    auto* help=new QLabel("Choose a received picture, adjust its size, then drag it into place on your TX image.");help->setWordWrap(true);help->setMaximumWidth(275);tools->addWidget(help);
    auto* current=new QPushButton("Use current RX image");current->setObjectName("insetUseCurrentRx");current->setEnabled(!received_.isNull());tools->addWidget(current);
    auto* choose=new QPushButton("Choose saved image…");choose->setObjectName("insetChooseImage");tools->addWidget(choose);
    sourceInfo_=new QLabel;sourceInfo_->setObjectName("insetSourceInfo");sourceInfo_->setWordWrap(true);tools->addWidget(sourceInfo_);
    tools->addWidget(new QLabel("Width of TX picture"));auto* sizeRow=new QHBoxLayout;width_=new QSlider(Qt::Horizontal);width_->setObjectName("insetWidthSlider");width_->setRange(10,100);percent_=new QSpinBox;percent_->setObjectName("insetWidthPercent");percent_->setRange(10,100);percent_->setSuffix(" %");
    const int percent=std::isfinite(inset_.widthFraction)?int(std::lround(std::clamp(inset_.widthFraction,.1,1.)*100)):35;inset_.widthFraction=percent/100.;width_->setValue(percent);percent_->setValue(percent);sizeRow->addWidget(width_,1);sizeRow->addWidget(percent_);tools->addLayout(sizeRow);
    frame_=new QCheckBox("White frame");frame_->setObjectName("insetFrame");frame_->setChecked(inset_.frame);tools->addWidget(frame_);
    auto* reset=new QPushButton("Place at bottom right");reset->setObjectName("insetResetPosition");tools->addWidget(reset);tools->addStretch();
    auto* resolution=new QLabel(QString("%1 × %2 · %3").arg(mode.width).arg(mode.height).arg(mode.name));tools->addWidget(resolution);
    auto* footer=new QHBoxLayout;remove_=new QPushButton("Remove inset");remove_->setObjectName("insetRemove");footer->addWidget(remove_);footer->addStretch();auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);apply_=buttons->button(QDialogButtonBox::Apply);apply_->setObjectName("insetApply");apply_->setDefault(true);footer->addWidget(buttons);root->addLayout(footer);
    connect(current,&QPushButton::clicked,this,[this]{inset_.image=received_.copy();updatePreview();});
    connect(choose,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"Choose a received image",historyFolder_,"Images (*.png *.jpg *.jpeg *.bmp *.webp)");if(path.isEmpty())return;QImageReader reader(path);reader.setAutoTransform(true);const auto image=reader.read();if(image.isNull()){sourceInfo_->setText("Cannot load picture: "+reader.errorString());return;}inset_.image=image;updatePreview();});
    connect(width_,&QSlider::valueChanged,percent_,&QSpinBox::setValue);connect(percent_,&QSpinBox::valueChanged,width_,&QSlider::setValue);connect(percent_,&QSpinBox::valueChanged,this,[this](int percent){inset_.widthFraction=percent/100.;updatePreview();});
    connect(frame_,&QCheckBox::toggled,this,[this](bool enabled){inset_.frame=enabled;updatePreview();});
    connect(reset,&QPushButton::clicked,this,[this]{inset_.relativePosition={1.,1.};updatePreview();});
    preview_->textMoved=[this](const QString& key,QPointF position){if(key=="inset"){inset_.relativePosition=position;updatePreview();}};
    connect(remove_,&QPushButton::clicked,this,[this]{inset_={};accept();});connect(apply_,&QPushButton::clicked,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);updatePreview();
}
void InsetDialog::updatePreview(){
    const auto composed=sstv::composeTransmitLayers(background_,mode_,callsign_,captions_,callPlacement_,inset_);preview_->setImage(grayscalePreview_?composed.image.convertToFormat(QImage::Format_Grayscale8):composed.image);
    QMap<QString,QRect> regions;if(!composed.insetBounds.isEmpty())regions["inset"]=composed.insetBounds;preview_->setTextRegions(regions);
    const bool present=!inset_.image.isNull();apply_->setEnabled(present);remove_->setEnabled(present);width_->setEnabled(present);percent_->setEnabled(present);frame_->setEnabled(present);
    sourceInfo_->setText(present?QString("Received image · %1 × %2\nThe complete picture keeps its proportions.").arg(inset_.image.width()).arg(inset_.image.height()):"No received image selected.");
}
