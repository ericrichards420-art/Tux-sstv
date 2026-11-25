#include "templatesdialog.h"
#include "captionpreview.h"
#include <QtWidgets>
#include <QTimeZone>

TemplatesDialog::TemplatesDialog(const sstv::TemplateFields& fields,bool enabled,const sstv::Mode& mode,const QString& callsign,const sstv::TextOverlay& overlay,QWidget* parent,const QImage& sourcePicture,const QList<sstv::TextOverlay>& captions,const sstv::ImageInset& inset,bool showCallsign,bool grayscalePreview)
    :QDialog(parent),fields_(fields),original_(fields),enabled_(enabled),mode_(mode),callsign_(callsign),showCallsign_(showCallsign),grayscalePreview_(grayscalePreview),overlay_(overlay),inset_(inset),captions_(captions),picture_(sourcePicture){
    if(captions_.isEmpty()&&!overlay_.text.trimmed().isEmpty())captions_<<overlay_;
    overlay_.text.clear();overlay_.freePosition=false;overlay_.relativePosition={};
    setObjectName("templatesDialog");setWindowTitle("SSTV image templates");resize(1000,680);if(parent)setPalette(parent->palette());
    auto* root=new QVBoxLayout(this);auto* columns=new QHBoxLayout;root->addLayout(columns,1);
    auto* left=new QVBoxLayout;columns->addLayout(left,1);preview_=new CaptionPreview;preview_->setObjectName("templatePreview");left->addWidget(preview_,1);
    auto* size=new QLabel(QString("%1 × %2 · %3 · Transmit preview").arg(mode.width).arg(mode.height).arg(mode.name));left->addWidget(size);
    left->addWidget(new QLabel("Drag any text on the preview to move it."));
    auto* savedRow=new QHBoxLayout;savedRow->addWidget(new QLabel("My layouts"));savedLayouts_=new QComboBox;savedLayouts_->setObjectName("savedLayouts");savedRow->addWidget(savedLayouts_,1);
    auto* load=new QPushButton("Load");load->setObjectName("loadLayout");auto* save=new QPushButton("Save layout…");save->setObjectName("saveLayout");savedRow->addWidget(load);savedRow->addWidget(save);left->addLayout(savedRow);
    connect(load,&QPushButton::clicked,this,&TemplatesDialog::loadSavedLayout);connect(save,&QPushButton::clicked,this,&TemplatesDialog::saveLayout);
    auto* colors=new QHBoxLayout;left->addLayout(colors);
    const auto addColor=[this,colors](const QString& label,const QString& name,QColor sstv::TemplateFields::* member){auto* button=new QPushButton(label);button->setObjectName(name);colors->addWidget(button);connect(button,&QPushButton::clicked,this,[this,member,label]{const auto chosen=QColorDialog::getColor((fields_.*member).isValid()?fields_.*member:QColor(Qt::white),this,label);if(chosen.isValid()){fields_.*member=chosen;updatePreview();}});};
    addColor("Accent color","templateAccentColor",&sstv::TemplateFields::accentColor);addColor("Text color","templateTextColor",&sstv::TemplateFields::textColor);addColor("Background color","templateBackgroundColor",&sstv::TemplateFields::backgroundColor);
    auto* defaultColors=new QPushButton("Default colors");defaultColors->setObjectName("templateDefaultColors");colors->addWidget(defaultColors);connect(defaultColors,&QPushButton::clicked,this,[this]{fields_.accentColor=QColor{};fields_.textColor=QColor{};fields_.backgroundColor=QColor{};updatePreview();});
    auto* browse=new QHBoxLayout;browse->addWidget(new QLabel("Designs"));auto* category=new QComboBox;category_=category;category->setObjectName("templateCategory");category->addItems({"All templates","Weather cards","SSTV exchanges","TV Studio"});browse->addWidget(category);browse->addStretch();left->addLayout(browse);
    designs_=new QListWidget;designs_->setObjectName("templateDesigns");designs_->setViewMode(QListView::IconMode);designs_->setFlow(QListView::LeftToRight);designs_->setWrapping(true);designs_->setResizeMode(QListView::Adjust);designs_->setMovement(QListView::Static);designs_->setIconSize({96,64});designs_->setGridSize({110,91});designs_->setWordWrap(false);designs_->setFixedHeight(194);left->addWidget(designs_);
    int selected=0;for(const auto& design:sstv::imageTemplates()){
        auto* item=new QListWidgetItem(design.name,designs_);item->setData(Qt::UserRole,design.id);item->setToolTip(design.description);if(design.id==fields.design)selected=designs_->count()-1;
    }designs_->setCurrentRow(selected);
    auto* rightPanel=new QWidget;auto* right=new QVBoxLayout(rightPanel);auto* scroll=new QScrollArea;scroll->setObjectName("templateFieldsScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setMinimumWidth(370);scroll->setWidget(rightPanel);columns->addWidget(scroll);title_=new QLabel;title_->setStyleSheet("font-size:19px;font-weight:bold;");right->addWidget(title_);
    description_=new QLabel;description_->setWordWrap(true);description_->setMaximumWidth(320);right->addWidget(description_);
    auto* form=new QFormLayout;form_=form;right->addLayout(form);
    toCall_=new QLineEdit(fields.toCall);toCall_->setObjectName("templateToCall");toCall_->setMaxLength(20);toCall_->setPlaceholderText("Other station (optional)");form->addRow("To callsign",toCall_);
    report_=new QLineEdit(fields.report);report_->setObjectName("templateReport");report_->setMaxLength(32);report_->setPlaceholderText("Your picture report");form->addRow("Report",report_);
    pictureRow_=new QWidget;auto* pictureLayout=new QHBoxLayout(pictureRow_);pictureLayout->setContentsMargins(0,0,0,0);
    usePicture_=new QCheckBox("Use picture");usePicture_->setObjectName("templateUsePicture");usePicture_->setChecked(fields.usePicture);usePicture_->setEnabled(!picture_.isNull());
    auto* choosePicture=new QPushButton("Choose…");choosePicture->setObjectName("templateChoosePicture");choosePicture->setToolTip("Choose your own photo for the card background.");pictureLayout->addWidget(usePicture_);pictureLayout->addWidget(choosePicture);form->addRow("Background",pictureRow_);
    city_=new QLineEdit(fields.city);city_->setObjectName("templateCity");city_->setMaxLength(48);city_->setPlaceholderText("City");form->addRow("City",city_);
    state_=new QLineEdit(fields.state);state_->setObjectName("templateState");state_->setMaxLength(32);state_->setPlaceholderText("State / region");form->addRow("State / region",state_);
    condition_=new QComboBox;condition_->setObjectName("templateWeather");condition_->setEditable(true);condition_->addItems({"","Sunny","Clear","Partly cloudy","Cloudy","Overcast","Rain","Thunderstorms","Snow","Fog","Windy"});condition_->setCurrentText(fields.weather);weather_=condition_->lineEdit();weather_->setMaxLength(64);weather_->setPlaceholderText("Choose or type conditions");form->addRow("Weather",condition_);
    temperature_=new QLineEdit(fields.temperature);temperature_->setObjectName("templateTemperature");temperature_->setPlaceholderText("e.g. 72");temperature_->setMaxLength(12);
    auto* validator=new QDoubleValidator(-238,302,2,temperature_);validator->setNotation(QDoubleValidator::StandardNotation);validator->setLocale(QLocale::c());temperature_->setValidator(validator);
    unit_=new QComboBox;unit_->setObjectName("templateTemperatureUnit");unit_->addItem("°F","F");unit_->addItem("°C","C");unit_->setCurrentIndex(fields.unit=="C"?1:0);
    temperatureRow_=new QWidget;auto* tempRow=new QHBoxLayout(temperatureRow_);tempRow->setContentsMargins(0,0,0,0);tempRow->addWidget(temperature_,1);tempRow->addWidget(unit_);form->addRow("Temperature",temperatureRow_);
    const auto measurement=[&](const QString& label,const char* name,const char* unitName,const QString& value,const QList<QPair<QString,QString>>& units,const QString& selected,QLineEdit*& edit,QComboBox*& choice){
        auto* row=new QWidget;auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);
        edit=new QLineEdit(value);edit->setObjectName(name);edit->setMaxLength(12);edit->setPlaceholderText("Optional");
        auto* validator=new QDoubleValidator(0,1000000,3,edit);validator->setNotation(QDoubleValidator::StandardNotation);validator->setLocale(QLocale::c());edit->setValidator(validator);
        choice=new QComboBox;choice->setObjectName(unitName);for(const auto& unit:units)choice->addItem(unit.first,unit.second);choice->setCurrentIndex(std::max(0,choice->findData(selected)));
        layout->addWidget(edit,1);layout->addWidget(choice);form->addRow(label,row);return row;
    };
    windRow_=measurement("Wind speed","templateWindSpeed","templateWindUnit",fields.windSpeed,{{"mph","mph"},{"km/h","km/h"},{"knots","kn"},{"m/s","m/s"}},fields.windUnit,windSpeed_,windUnit_);
    rainRow_=measurement("Rainfall","templateRainfall","templateRainUnit",fields.rainfall,{{"in","in"},{"mm","mm"}},fields.rainUnit,rainfall_,rainUnit_);
    humidityRow_=new QWidget;auto* humidityLayout=new QHBoxLayout(humidityRow_);humidityLayout->setContentsMargins(0,0,0,0);
    humidity_=new QLineEdit(fields.humidity);humidity_->setObjectName("templateHumidity");humidity_->setPlaceholderText("0–100 (optional)");humidity_->setMaxLength(12);
    auto* humidityValidator=new QDoubleValidator(0,100,3,humidity_);humidityValidator->setNotation(QDoubleValidator::StandardNotation);humidityValidator->setLocale(QLocale::c());humidity_->setValidator(humidityValidator);
    humidityLayout->addWidget(humidity_,1);humidityLayout->addWidget(new QLabel("%"));form->addRow("Humidity",humidityRow_);
    pressureRow_=measurement("BP / pressure","templatePressure","templatePressureUnit",fields.pressure,{{"inHg","inHg"},{"hPa (mb)","hPa"}},fields.pressureUnit,pressure_,pressureUnit_);
    pressure_->setToolTip("Barometric pressure. Enter the reading from your weather station.");
    const auto tvField=[&](const char* name,const QString& label,const QString& placeholder,const QString& value,int maximum){
        auto* edit=new QLineEdit(value);edit->setObjectName(name);edit->setMaxLength(maximum);edit->setPlaceholderText(placeholder);form->addRow(label,edit);return edit;
    };
    headline_=tvField("templateHeadline","Headline","Your headline or show title",fields.headline,72);
    subheadline_=tvField("templateSubheadline","Story details","Add a short second line",fields.subheadline,100);
    programInfo_=tvField("templateProgramInfo","Time / channel","e.g. Tonight · 8:00 PM",fields.programInfo,64);
    homeTeam_=tvField("templateHomeTeam","Home team","Team name",fields.homeTeam,24);
    awayTeam_=tvField("templateAwayTeam","Away team","Team name",fields.awayTeam,24);
    homeScore_=tvField("templateHomeScore","Home score","Score",fields.homeScore,8);
    awayScore_=tvField("templateAwayScore","Away score","Score",fields.awayScore,8);
    ticker_=tvField("templateTicker","Ticker","Your update along the bottom",fields.ticker,100);
    date_=new QLineEdit(fields.date.toString(Qt::ISODate));date_->setObjectName("templateDate");date_->setPlaceholderText("YYYY-MM-DD");date_->setMaxLength(10);form->addRow("Date",date_);
    time_=new QLineEdit(fields.time.toString("HH:mm"));time_->setObjectName("templateTime");time_->setPlaceholderText("HH:MM (24-hour)");time_->setMaxLength(5);form->addRow("Time",time_);
    zone_=new QComboBox;zone_->setObjectName("templateTimeZone");zone_->addItems({"Local time","UTC"});zone_->setCurrentIndex(fields.utc?1:0);form->addRow("Time zone",zone_);
    auto* now=new QPushButton("Use current date & time");now->setObjectName("templateNow");form->addRow(now);
    message_=new QLineEdit(fields.message);message_->setObjectName("templateMessage");message_->setMaxLength(80);message_->setPlaceholderText("CQ SSTV · 73!");form->addRow("Greeting",message_);
    quickReply_=new QComboBox;quickReply_->setObjectName("templateQuickReply");quickReply_->addItems({"Choose a reply…","Nice picture!","Your turn!","Send another one!","One more before 73?","Thanks for the picture!","Back to you!"});form->addRow("Quick reply",quickReply_);
    note_=new QLabel;note_->setWordWrap(true);note_->setMaximumWidth(320);right->addWidget(note_);
    status_=new QLabel;status_->setObjectName("templateStatus");status_->setWordWrap(true);status_->setMaximumWidth(320);right->addWidget(status_);right->addStretch();
    auto* footer=new QHBoxLayout;auto* picture=new QPushButton("Use picture only");picture->setObjectName("templatePictureOnly");picture->setEnabled(enabled);footer->addWidget(picture);
    auto* clear=new QPushButton("Clear all fields");clear->setObjectName("templateClear");footer->addWidget(clear);
    auto* resetLayout=new QPushButton("Reset text positions");resetLayout->setObjectName("templateResetPositions");footer->addWidget(resetLayout);
    footer->addStretch();auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);apply_=buttons->button(QDialogButtonBox::Apply);apply_->setText("Use template");apply_->setObjectName("templateApply");apply_->setDefault(true);footer->addWidget(buttons);root->addLayout(footer);
    connect(apply_,&QPushButton::clicked,this,[this]{if(fields_.validate().isEmpty()){enabled_=true;accept();}});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(picture,&QPushButton::clicked,this,[this]{enabled_=false;fields_=original_;accept();});
    connect(clear,&QPushButton::clicked,this,[this]{
        captions_.clear();inset_={};fields_.textPositions.clear();
        for(auto* edit:{toCall_,report_,city_,state_,temperature_,windSpeed_,rainfall_,humidity_,pressure_,headline_,subheadline_,ticker_,programInfo_,homeTeam_,awayTeam_,homeScore_,awayScore_,date_,time_,message_}){const QSignalBlocker block(edit);edit->clear();}
        {const QSignalBlocker block(condition_);condition_->setCurrentIndex(0);}
        {const QSignalBlocker block(usePicture_);usePicture_->setChecked(false);}
        updatePreview();
    });
    connect(resetLayout,&QPushButton::clicked,this,[this]{fields_.textPositions.clear();for(auto& caption:captions_){caption.freePosition=false;caption.relativePosition={};}updatePreview();});
    preview_->textMoved=[this](const QString& key,QPointF point){
        const int index=sstv::captionIndex(key);
        if(index>=0&&index<captions_.size()){captions_[index].freePosition=true;captions_[index].relativePosition=point;sstv::bringCaptionToFront(captions_,index);}
        else fields_.textPositions[key]=point;
        updatePreview();
    };
    connect(choosePicture,&QPushButton::clicked,this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,"Choose card picture",{},"Images (*.png *.jpg *.jpeg *.bmp *.webp)");if(path.isEmpty())return;
        QImageReader reader(path);reader.setAutoTransform(true);const auto selected=reader.read();
        if(selected.isNull()){QMessageBox::warning(this,"Picture error",reader.errorString());return;}
        picture_=selected;usePicture_->setEnabled(true);usePicture_->setChecked(true);updatePreview();
    });
    connect(usePicture_,&QCheckBox::toggled,this,&TemplatesDialog::updatePreview);
    connect(quickReply_,&QComboBox::currentIndexChanged,this,[this](int index){if(index>0){message_->setText(quickReply_->currentText());const QSignalBlocker block(quickReply_);quickReply_->setCurrentIndex(0);}});
    connect(now,&QPushButton::clicked,this,[this]{
        const QSignalBlocker dateBlock(date_),timeBlock(time_);
        fields_.utc=zone_->currentIndex()==1;fields_.useCurrentTime();date_->setText(fields_.date.toString(Qt::ISODate));time_->setText(fields_.time.toString("HH:mm"));updatePreview();
    });
    connect(zone_,&QComboBox::currentIndexChanged,this,[this]{
        // Keep the same instant when switching between local time and UTC.
        if(fields_.date.isValid() && fields_.time.isValid()){
            const QDateTime previous(fields_.date,fields_.time,fields_.utc?QTimeZone::utc():QTimeZone::systemTimeZone());
            const auto converted=zone_->currentIndex()?previous.toUTC():previous.toLocalTime();
            const QSignalBlocker dateBlock(date_),timeBlock(time_);date_->setText(converted.date().toString(Qt::ISODate));time_->setText(converted.time().toString("HH:mm"));
        }
        updatePreview();
    });
    connect(unit_,&QComboBox::currentIndexChanged,this,[this]{
        bool ok=false;double value=temperature_->text().toDouble(&ok);if(ok){value=unit_->currentData().toString()=="C"?(value-32)*5/9:value*9/5+32;const QSignalBlocker block(temperature_);temperature_->setText(QString::number(value,'f',1));}updatePreview();
    });
    const auto convert=[this](QLineEdit* edit,double factor,int decimals){
        bool ok=false;const double value=edit->text().toDouble(&ok);
        if(ok){const QSignalBlocker block(edit);edit->setText(QString::number(value*factor,'f',decimals));}
        updatePreview();
    };
    connect(windUnit_,&QComboBox::currentIndexChanged,this,[this,convert]{
        const auto kilometers=[](const QString& unit){return unit=="mph"?1.609344:unit=="kn"?1.852:unit=="m/s"?3.6:1.;};
        convert(windSpeed_,kilometers(fields_.windUnit)/kilometers(windUnit_->currentData().toString()),3);
    });
    connect(rainUnit_,&QComboBox::currentIndexChanged,this,[this,convert]{convert(rainfall_,fields_.rainUnit=="in"?25.4:1/25.4,3);});
    connect(pressureUnit_,&QComboBox::currentIndexChanged,this,[this,convert]{convert(pressure_,fields_.pressureUnit=="inHg"?33.8639:1/33.8639,3);});
    connect(category,&QComboBox::currentIndexChanged,this,[this,category]{
        // Rebuild the filtered model: hiding/revealing icon-mode rows can leave
        // Qt's hit-testing out of sync with the visible thumbnail positions.
        const auto previous=fields_.design;
        {
            const QSignalBlocker block(designs_);designs_->clear();int selected=0;
            for(const auto& design:sstv::imageTemplates()){
                const int kind=design.tvCard?3:design.weatherCard?1:2;
                if(category->currentIndex()!=0 && category->currentIndex()!=kind)continue;
                auto* item=new QListWidgetItem(design.name,designs_);item->setData(Qt::UserRole,design.id);item->setToolTip(design.description);
                if(design.id==previous)selected=designs_->count()-1;
            }
            designs_->setCurrentRow(selected);
        }
        updatePreview();designs_->doItemsLayout();
        designs_->scrollToItem(designs_->currentItem());
    });
    for(auto* edit:{toCall_,report_,city_,state_,temperature_,windSpeed_,rainfall_,humidity_,pressure_,headline_,subheadline_,ticker_,programInfo_,homeTeam_,awayTeam_,homeScore_,awayScore_,date_,time_,message_})connect(edit,&QLineEdit::textChanged,this,&TemplatesDialog::updatePreview);
    connect(condition_,&QComboBox::currentTextChanged,this,&TemplatesDialog::updatePreview);connect(designs_,&QListWidget::currentRowChanged,this,&TemplatesDialog::updatePreview);reloadLayouts();updatePreview();
}
void TemplatesDialog::updatePreview(){
    fields_.design=designs_->currentItem()->data(Qt::UserRole).toString();fields_.city=city_->text().trimmed();fields_.state=state_->text().trimmed();fields_.weather=condition_->currentText().trimmed();fields_.temperature=temperature_->text().trimmed();fields_.unit=unit_->currentData().toString();fields_.date=QDate::fromString(date_->text().trimmed(),Qt::ISODate);fields_.time=QTime::fromString(time_->text().trimmed(),"HH:mm");fields_.utc=zone_->currentIndex()==1;fields_.message=message_->text();
    fields_.toCall=toCall_->text().trimmed().toUpper();fields_.report=report_->text().trimmed();fields_.usePicture=usePicture_->isChecked();
    fields_.windSpeed=windSpeed_->text().trimmed();fields_.windUnit=windUnit_->currentData().toString();fields_.rainfall=rainfall_->text().trimmed();fields_.rainUnit=rainUnit_->currentData().toString();fields_.humidity=humidity_->text().trimmed();fields_.pressure=pressure_->text().trimmed();fields_.pressureUnit=pressureUnit_->currentData().toString();
    fields_.headline=headline_->text().trimmed();fields_.subheadline=subheadline_->text().trimmed();fields_.ticker=ticker_->text().trimmed();fields_.programInfo=programInfo_->text().trimmed();fields_.homeTeam=homeTeam_->text().trimmed();fields_.awayTeam=awayTeam_->text().trimmed();fields_.homeScore=homeScore_->text().trimmed();fields_.awayScore=awayScore_->text().trimmed();
    const auto* selected=sstv::imageTemplateById(fields_.design);const bool weatherCard=selected->weatherCard,tv=selected->tvCard,sports=fields_.design=="scoreboard";
    description_->setText(selected->description);
    title_->setText(weatherCard?"Your station. Your weather.":tv?"Your own TV studio.":"Keep the QSO going.");
    const auto visible=[&](QWidget* field,bool shown){field->setVisible(shown);if(auto* label=form_->labelForField(field))label->setVisible(shown);};
    for(auto* field:std::initializer_list<QWidget*>{toCall_,quickReply_})visible(field,!weatherCard&&!tv);
    visible(pictureRow_,!weatherCard);visible(message_,!tv);
    for(auto* field:{headline_,subheadline_})visible(field,tv&&!sports);
    visible(ticker_,tv);visible(programInfo_,tv&&(fields_.design=="now-showing"||fields_.design=="on-air"));
    for(auto* field:{homeTeam_,awayTeam_,homeScore_,awayScore_})visible(field,tv&&sports);
    qobject_cast<QLabel*>(form_->labelForField(headline_))->setText(fields_.design=="now-showing"?"Program / movie":fields_.design=="on-air"?"Show / segment":"Headline");
    qobject_cast<QLabel*>(form_->labelForField(subheadline_))->setText(fields_.design=="now-showing"?"Details":fields_.design=="on-air"?"Guest / topic":"Story details");
    const bool showReport=fields_.design=="report";report_->setVisible(showReport);form_->labelForField(report_)->setVisible(showReport);
    condition_->setVisible(weatherCard);form_->labelForField(condition_)->setVisible(weatherCard);temperatureRow_->setVisible(weatherCard);form_->labelForField(temperatureRow_)->setVisible(weatherCard);
    for(auto* row:{windRow_,rainRow_,humidityRow_,pressureRow_}){row->setVisible(weatherCard);form_->labelForField(row)->setVisible(weatherCard);}
    message_->setPlaceholderText(weatherCard?"CQ SSTV · 73!":"Add your message (optional)");
    note_->setText(weatherCard?"Start with blank fields for each new image. Fill in your weather, or use the button above for the current date and time. Clear all fields also removes the caption; Cancel keeps the current image.":tv?"Write your own story, show information or scores. An optional photo sets the scene; the ticker appears along the bottom. Drag text on the preview to move it. Saved layouts keep the design, not old stories or scores.":"Add the other station's callsign, a message and an optional photo. Picture Report has a report field. All image fields start blank; Cancel keeps the current picture.");
    auto error=fields_.validate();
    if(!date_->text().trimmed().isEmpty() && !fields_.date.isValid())error="Enter the date as YYYY-MM-DD, or leave it blank.";
    if(!time_->text().trimmed().isEmpty() && !fields_.time.isValid())error="Enter the time as HH:MM (24-hour), or leave it blank.";
    apply_->setEnabled(error.isEmpty());status_->setText(error);if(!error.isEmpty()){preview_->setTextRegions({});return;}
    QMap<QString,QRect> regions;
    const auto base=sstv::renderTemplate(fields_,{mode_.width,mode_.height},callsign_,picture_,&regions,showCallsign_);
    const auto composed=sstv::composeTransmitLayers(base,mode_,{},captions_,{},inset_);image_=grayscalePreview_?composed.image.convertToFormat(QImage::Format_Grayscale8).convertToFormat(QImage::Format_RGB32):composed.image;
    regions.insert(composed.captionRegions());
    preview_->setImage(image_);preview_->setTextRegions(regions,sstv::captionHitOrder(captions_));
    for(int i=0;i<designs_->count();++i){auto f=fields_;f.design=designs_->item(i)->data(Qt::UserRole).toString();auto icon=sstv::renderTemplate(f,{160,120},callsign_,picture_,nullptr,showCallsign_);if(grayscalePreview_)icon=icon.convertToFormat(QImage::Format_Grayscale8);designs_->item(i)->setIcon(QIcon(QPixmap::fromImage(icon)));}
}

namespace {
QString layoutDirectory(){return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/layouts";}
}
void TemplatesDialog::reloadLayouts(){
    QString error;layouts_=sstv::LayoutStore(layoutDirectory()).list(error);savedLayouts_->clear();savedLayouts_->addItem("Choose a saved layout…");
    for(const auto& layout:layouts_)savedLayouts_->addItem(layout.name,layout.id);
    if(!error.isEmpty())status_->setText(error);
}
void TemplatesDialog::saveLayout(){
    QInputDialog prompt(this);prompt.setObjectName("layoutNameDialog");prompt.setWindowTitle("Save template layout");prompt.setLabelText("Layout name (same name replaces that layout). Artwork, colors and positions are saved. Message and report fields are excluded.");prompt.setInputMode(QInputDialog::TextInput);if(savedLayouts_->currentIndex()>0)prompt.setTextValue(savedLayouts_->currentText());
    if(prompt.exec()!=QDialog::Accepted)return;
    QString error;if(!sstv::LayoutStore(layoutDirectory()).save(prompt.textValue(),fields_,captionOverlay(),picture_,error)){status_->setText(error);return;}
    reloadLayouts();savedLayouts_->setCurrentIndex(savedLayouts_->findText(prompt.textValue().trimmed()));status_->setText("Layout saved · message and report fields excluded");
}
void TemplatesDialog::loadSavedLayout(){
    const int index=savedLayouts_->currentIndex()-1;if(index<0 || index>=layouts_.size())return;const auto layout=layouts_[index];
    fields_=layout.fields;overlay_=layout.caption;captions_.clear();inset_={};picture_=layout.picture; // Content was never stored.
    for(auto* edit:{toCall_,report_,city_,state_,temperature_,windSpeed_,rainfall_,humidity_,pressure_,headline_,subheadline_,ticker_,programInfo_,homeTeam_,awayTeam_,homeScore_,awayScore_,date_,time_,message_}){const QSignalBlocker block(edit);edit->clear();}
    {const QSignalBlocker block(condition_);condition_->setCurrentIndex(0);}
    {const QSignalBlocker block(unit_);unit_->setCurrentIndex(fields_.unit=="C"?1:0);}
    {const QSignalBlocker block(windUnit_);windUnit_->setCurrentIndex(windUnit_->findData(fields_.windUnit));}
    {const QSignalBlocker block(rainUnit_);rainUnit_->setCurrentIndex(rainUnit_->findData(fields_.rainUnit));}
    {const QSignalBlocker block(pressureUnit_);pressureUnit_->setCurrentIndex(pressureUnit_->findData(fields_.pressureUnit));}
    {const QSignalBlocker block(zone_);zone_->setCurrentIndex(fields_.utc?1:0);}
    {const QSignalBlocker block(usePicture_);usePicture_->setEnabled(!picture_.isNull());usePicture_->setChecked(fields_.usePicture);}
    {const QSignalBlocker block(quickReply_);quickReply_->setCurrentIndex(0);}
    {const QSignalBlocker block(category_);category_->setCurrentIndex(0);}
    {
        const QSignalBlocker block(designs_);designs_->clear();int selected=0;
        for(const auto& design:sstv::imageTemplates()){auto* item=new QListWidgetItem(design.name,designs_);item->setData(Qt::UserRole,design.id);if(design.id==fields_.design)selected=designs_->count()-1;}
        designs_->setCurrentRow(selected);
    }
    updatePreview();designs_->doItemsLayout();designs_->scrollToItem(designs_->currentItem());status_->setText("Layout loaded · fill in fresh fields for this picture");
}
