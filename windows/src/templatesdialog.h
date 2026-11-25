#pragma once
#include "core/templates.h"
#include "core/layouts.h"
#include "core/composition.h"
#include <QDialog>
class CaptionPreview;
class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QFormLayout;
class QCheckBox;

class TemplatesDialog:public QDialog {
    Q_OBJECT
public:
    TemplatesDialog(const sstv::TemplateFields&,bool enabled,const sstv::Mode&,const QString& callsign,const sstv::TextOverlay&,QWidget* parent=nullptr,const QImage& picture={},const QList<sstv::TextOverlay>& captions={},const sstv::ImageInset& inset={},bool showCallsign=true,bool grayscalePreview=false);
    sstv::TemplateFields fields() const{return fields_;}
    bool enabled() const{return enabled_;}
    QImage image() const{return image_;}
    QString captionText() const{return captionOverlay().text;}
    sstv::TextOverlay captionOverlay() const{return captions_.isEmpty()?overlay_:captions_.first();}
    QList<sstv::TextOverlay> captionOverlays() const{return captions_;}
    QImage picture() const{return picture_;}
    sstv::ImageInset imageInset() const{return inset_;}
private:
    sstv::TemplateFields fields_,original_;
    bool enabled_;
    const sstv::Mode& mode_;
    QString callsign_;
    bool showCallsign_;
    bool grayscalePreview_;
    sstv::TextOverlay overlay_;
    sstv::ImageInset inset_;
    QList<sstv::TextOverlay> captions_;
    QImage image_,picture_;
    CaptionPreview* preview_;
    QListWidget* designs_;
    QFormLayout* form_;
    QWidget* temperatureRow_;
    QWidget *windRow_,*rainRow_,*humidityRow_,*pressureRow_;
    QWidget* pictureRow_;
    QCheckBox* usePicture_;
    QLabel *title_,*note_;
    QLineEdit *city_,*state_,*weather_,*temperature_,*message_;
    QLineEdit *windSpeed_,*rainfall_,*humidity_,*pressure_;
    QLineEdit *headline_,*subheadline_,*ticker_,*programInfo_,*homeTeam_,*awayTeam_,*homeScore_,*awayScore_;
    QComboBox *windUnit_,*rainUnit_,*pressureUnit_;
    QComboBox *condition_,*unit_,*zone_,*savedLayouts_,*category_;
    QList<sstv::TemplateLayout> layouts_;
    void reloadLayouts();
    void loadSavedLayout();
    void saveLayout();
    QLineEdit *date_,*time_;
    QLineEdit *toCall_,*report_;
    QComboBox* quickReply_;
    QLabel *status_,*description_;
    QPushButton* apply_;
    void updatePreview();
};
