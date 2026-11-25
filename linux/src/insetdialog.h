#pragma once
#include "core/composition.h"
#include <QDialog>
class CaptionPreview;
class QSlider;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;

class InsetDialog:public QDialog {
    Q_OBJECT
public:
    InsetDialog(const sstv::ImageInset&,const QImage& background,const sstv::Mode&,const QString& callsign,const QList<sstv::TextOverlay>&,const sstv::CallsignPlacement&,const QImage& received,const QString& historyFolder,QWidget* parent=nullptr,bool grayscalePreview=false);
    sstv::ImageInset inset() const{return inset_;}
private:
    sstv::ImageInset inset_;
    bool grayscalePreview_;
    QImage background_,received_;
    const sstv::Mode& mode_;
    QString callsign_,historyFolder_;
    QList<sstv::TextOverlay> captions_;
    sstv::CallsignPlacement callPlacement_;
    CaptionPreview* preview_;
    QSlider* width_;
    QSpinBox* percent_;
    QCheckBox* frame_;
    QLabel* sourceInfo_;
    QPushButton *apply_,*remove_;
    void updatePreview();
};
