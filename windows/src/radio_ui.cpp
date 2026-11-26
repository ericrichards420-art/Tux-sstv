#include "radio_ui.h"
#include <QtWidgets>
#include <QStandardItemModel>

RadioSettingsPage::RadioSettingsPage(const radio::Config& config,radio::Controller* controller,QWidget* parent):QWidget(parent){
    auto* layout=new QVBoxLayout(this);layout->setSpacing(9);
    auto* title=new QLabel("Choose the radio connection, then choose how transmission starts.");title->setWordWrap(true);title->setObjectName("radioWiringHelp");layout->addWidget(title);
    auto* form=new QFormLayout;form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);layout->addLayout(form);
    preset_=new QComboBox;preset_->setObjectName("radioSetupPreset");
    for(const auto& item:QList<QPair<QString,QString>>{{"Custom / existing settings","saved"},{"Radio USB or serial CAT cable","cat"},{"SignaLink USB with a separate CAT cable","signalink"},{"FlexRadio: SmartSDR CAT virtual COM port","flex-cat"},{"FlexRadio: direct network / SmartSDR slice","flex-network"},{"Existing Hamlib rigctld server","rigctld"},{"Audio only / receive only / manual PTT","manual"}})preset_->addItem(item.first,item.second);
    form->addRow("Connection setup",preset_);
    setupHelp_=new QLabel;setupHelp_->setObjectName("radioSetupHelp");setupHelp_->setWordWrap(true);form->addRow(setupHelp_);
    enabled_=new QCheckBox("Use computer radio control");enabled_->setObjectName("radioEnabled");enabled_->setChecked(config.enabled);form->addRow(enabled_);
    model_=new QComboBox;model_->setObjectName("radioModel");QString modelError;
    for(const auto& model:radio::models(&modelError))model_->addItem(model.second,model.first);
    if(model_->findData(config.model)<0)model_->addItem(QString("Model %1").arg(config.model),config.model);
    model_->setCurrentIndex(model_->findData(config.model));model_->setEditable(true);model_->setInsertPolicy(QComboBox::NoInsert);model_->completer()->setFilterMode(Qt::MatchContains);model_->completer()->setCaseSensitivity(Qt::CaseInsensitive);modelLabel_=new QLabel("Radio model (search)");form->addRow(modelLabel_,model_);
    device_=new QComboBox;device_->setEditable(true);device_->setObjectName("radioDevice");device_->addItems(radio::serialDevices());device_->setEditText(config.device);
    auto* deviceRow=new QHBoxLayout;deviceRow->addWidget(device_,1);refreshDevices_=new QPushButton("Refresh ports");refreshDevices_->setObjectName("radioRefreshDevices");deviceRow->addWidget(refreshDevices_);deviceLabel_=new QLabel("CAT port / device");form->addRow(deviceLabel_,deviceRow);
    connect(refreshDevices_,&QPushButton::clicked,this,[this]{const QSignalBlocker blocker(device_);const auto current=device_->currentText();device_->clear();device_->addItems(radio::serialDevices());device_->setEditText(current);});
    baud_=new QComboBox;baud_->setObjectName("radioBaud");for(int baud:{0,1200,2400,4800,9600,19200,38400,57600,115200,230400})baud_->addItem(baud?QString::number(baud):"Driver default",baud);
    if(baud_->findData(config.baud)<0){baud_->addItem(QString::number(config.baud),config.baud);}
    baud_->setCurrentIndex(baud_->findData(config.baud));baudLabel_=new QLabel("CAT baud rate");form->addRow(baudLabel_,baud_);
    host_=new QLineEdit(config.host);host_->setObjectName("radioHost");port_=new QSpinBox;port_->setObjectName("radioPort");port_->setRange(1,65535);port_->setValue(config.port);auto* server=new QHBoxLayout;server->addWidget(host_);server->addWidget(port_);serverLabel_=new QLabel("rigctld host / port");form->addRow(serverLabel_,server);
    keying_=new QComboBox;keying_->setObjectName("radioKeyingChoice");
    for(const auto& item:QList<QPair<QString,QString>>{{"Automatic: CAT transmit command","cat"},{"Automatic: serial RTS PTT","rts"},{"Automatic: serial DTR PTT","dtr"},{"Audio-operated: SignaLink / radio VOX","external"},{"Manual: I key the radio myself","manual"},{"Advanced / current PTT settings","advanced"}})keying_->addItem(item.first,item.second);
    form->addRow("Start transmission with",keying_);
    pttDevice_=new QLineEdit(config.pttDevice);pttDevice_->setObjectName("radioPttDevice");pttDevice_->setPlaceholderText("PTT COM port / device; blank uses the CAT port");pttDeviceLabel_=new QLabel("PTT port / device");form->addRow(pttDeviceLabel_,pttDevice_);
    keyingHelp_=new QLabel;keyingHelp_->setObjectName("radioKeyingHelp");keyingHelp_->setWordWrap(true);form->addRow(keyingHelp_);
    auto* usbPtt=new QPushButton("Use radio USB / DATA transmit input");usbPtt->setObjectName("radioUsbDataPtt");form->addRow(usbPtt);
    auto* advanced=new QGroupBox("Advanced radio and PTT settings");advanced->setObjectName("radioAdvancedSettings");advanced->setCheckable(true);advanced->setChecked(false);layout->addWidget(advanced);
    auto* advancedLayout=new QVBoxLayout(advanced);auto* advancedBody=new QWidget;advancedBody->setObjectName("radioAdvancedBody");advancedLayout->addWidget(advancedBody);auto* details=new QFormLayout(advancedBody);details->setContentsMargins(0,0,0,0);details->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);advancedBody->hide();connect(advanced,&QGroupBox::toggled,advancedBody,&QWidget::setVisible);
    connection_=new QComboBox;connection_->setObjectName("radioConnection");connection_->addItems({"Radio serial / USB / native network driver","Existing rigctld server"});connection_->setCurrentIndex(config.network?1:0);details->addRow("Control protocol",connection_);
    method_=new QComboBox;method_->setObjectName("radioPttMethod");for(const auto& pair:QList<QPair<QString,QString>>{{"CAT command","RIG"},{"CAT mic/data selection","RIGMICDATA"},{"Serial RTS","RTS"},{"Serial DTR","DTR"},{"CM108 GPIO","CM108"},{"GPIO","GPIO"},{"GPIO inverted","GPION"},{"Audio interface / VOX / manual","None"}})method_->addItem(pair.first,pair.second);
    method_->setCurrentIndex(std::max(0,method_->findData(config.pttType=="RIG"&&config.pttValue>1?QString("RIGMICDATA"):config.pttType)));details->addRow("PTT method",method_);
    pttValue_=new QComboBox;pttValue_->setObjectName("radioPttValue");pttValue_->addItem("Default transmit audio",1);pttValue_->addItem("Microphone input",2);pttValue_->addItem("Data / USB / rear input",3);pttValue_->setCurrentIndex(std::max(0,pttValue_->findData(config.pttValue)));details->addRow("CAT transmit input",pttValue_);
    vfo_=new QComboBox;vfo_->setObjectName("radioTargetVfo");vfo_->addItems({"currVFO","VFOA","VFOB","Main","Sub","MainA","MainB","SubA","SubB"});vfo_->setCurrentText(config.targetVfo);details->addRow("Control target VFO",vfo_);
    automatic_=new QCheckBox("Key PTT through CAT / PTT control for TX audio");automatic_->setObjectName("radioAutoPtt");automatic_->setChecked(config.automaticPtt);details->addRow(automatic_);
    lead_=new QSpinBox;lead_->setObjectName("radioLeadMs");lead_->setRange(0,5000);lead_->setSuffix(" ms");lead_->setValue(config.leadMs);tail_=new QSpinBox;tail_->setObjectName("radioTailMs");tail_->setRange(0,3000);tail_->setSuffix(" ms");tail_->setValue(config.tailMs);auto* delays=new QHBoxLayout;delays->addWidget(new QLabel("Before audio"));delays->addWidget(lead_);delays->addWidget(new QLabel("After audio"));delays->addWidget(tail_);details->addRow("PTT timing",delays);
    extra_=new QLineEdit(config.extra);extra_->setObjectName("radioExtra");extra_->setPlaceholderText("Optional Hamlib name=value settings");details->addRow("Extra Hamlib settings",extra_);
    auto* combined=new QPushButton("Use SignaLink / VOX keying with a separate CAT cable");combined->setObjectName("radioSeparateCat");details->addRow(combined);connect(combined,&QPushButton::clicked,this,[this]{applyPreset("signalink");});
    connect(usbPtt,&QPushButton::clicked,this,[this]{selectKeying("cat");method_->setCurrentIndex(method_->findData("RIGMICDATA"));pttValue_->setCurrentIndex(pttValue_->findData(3));});
    auto* check=new QPushButton("Check connection (no PTT ON)");check->setObjectName("radioCheckConnection");auto* result=new QLabel(modelError);result->setObjectName("radioConnectionResult");result->setWordWrap(true);layout->addWidget(check);layout->addWidget(result);
    auto* note=new QLabel("Save these choices, then connect and check the radio on the final wizard page. Selecting a setup never connects or transmits by itself.");note->setWordWrap(true);layout->addWidget(note);layout->addStretch();
    connect(preset_,&QComboBox::currentIndexChanged,this,[this]{applyPreset(preset_->currentData().toString());});
    connect(keying_,&QComboBox::currentIndexChanged,this,[this]{if(!guiding_)selectKeying(keying_->currentData().toString());});
    const auto changed=[this,usbPtt]{if(guiding_)return;usbPtt->setVisible(model_->currentData().toInt()==2039);usbPtt->setEnabled(enabled_->isChecked());updateGuidance();emit edited();};
    for(auto* box:{enabled_,automatic_})connect(box,&QCheckBox::toggled,this,changed);
    for(auto* combo:{connection_,model_,device_,baud_,method_,pttValue_,vfo_})connect(combo,&QComboBox::currentTextChanged,this,changed);
    for(auto* edit:{host_,pttDevice_,extra_})connect(edit,&QLineEdit::textChanged,this,changed);
    for(auto* spin:{port_,lead_,tail_})connect(spin,&QSpinBox::valueChanged,this,changed);
    connect(pttValue_,qOverload<int>(&QComboBox::activated),this,[this](int){if(pttValue_->currentData().toInt()>1&&method_->currentData()=="RIG")method_->setCurrentIndex(method_->findData("RIGMICDATA"));});
    changed();
    connect(check,&QPushButton::clicked,this,[this,controller,check,result]{const auto configuration=this->config();const auto error=validationError();if(!error.isEmpty()){result->setText(error);return;}if(!configuration.enabled){result->setText("Computer radio control is disabled. Choose a radio connection to check CAT.");return;}check->setEnabled(false);result->setText("Checking radio…");const QPointer<RadioSettingsPage> guard(this);controller->testConnection(configuration,[guard,check,result,controller](const QString& error){if(!guard)return;check->setEnabled(true);const auto state=controller->state();result->setText(error.isEmpty()?QString("Connection OK · %1 Hz · %2").arg(state.value("frequencyHz").toString(),state.value("mode").toString()):error);});});
}
void RadioSettingsPage::setAudioConnection(const QString& connection){audioConnection_=connection;updateGuidance();}
void RadioSettingsPage::selectKeying(const QString& choice){
    if(guiding_)return;
    guiding_=true;
    if(connection_->currentIndex()==1&&(choice=="rts"||choice=="dtr")){guiding_=false;updateGuidance();return;}
    if(choice=="advanced"){findChild<QGroupBox*>("radioAdvancedSettings")->setChecked(true);}
    if(choice=="cat"||choice=="rts"||choice=="dtr"){
        if(choice!="cat")pttValue_->setCurrentIndex(0);
        enabled_->setChecked(true);automatic_->setChecked(true);
        method_->setCurrentIndex(method_->findData(choice=="cat"?(pttValue_->currentData().toInt()>1?QString("RIGMICDATA"):QString("RIG")):choice.toUpper()));
    }else if(choice=="external"||choice=="manual"){automatic_->setChecked(false);method_->setCurrentIndex(method_->findData("None"));}
    guiding_=false;updateGuidance();emit edited();
}
void RadioSettingsPage::applyPreset(const QString& preset){
    if(guiding_||preset=="saved")return;
    guiding_=true;
    const bool daemon=preset=="rigctld",manual=preset=="manual";
    enabled_->setChecked(!manual);connection_->setCurrentIndex(daemon?1:0);
    if(preset=="flex-cat"){
        const int index=model_->findData(2014);if(index>=0)model_->setCurrentIndex(index);
        if(device_->currentText().contains(':'))device_->setEditText({});
        extra_->clear();pttDevice_->clear();baud_->setCurrentIndex(baud_->findData(9600));vfo_->setCurrentText("currVFO");pttValue_->setCurrentIndex(0);
    }else if(preset=="flex-network"){
        const int id=model_->currentData().toInt();if(id<23005||id>23012){const int index=model_->findData(23005);if(index>=0)model_->setCurrentIndex(index);}
        if(!device_->currentText().contains(':'))device_->setEditText({});
        extra_->clear();pttDevice_->clear();baud_->setCurrentIndex(baud_->findData(0));vfo_->setCurrentText("currVFO");pttValue_->setCurrentIndex(0);
    }
    const bool external=preset=="signalink"||manual;
    automatic_->setChecked(!external);method_->setCurrentIndex(method_->findData(external?"None":"RIG"));
    if(preset!="cat")pttValue_->setCurrentIndex(0);
    guiding_=false;{const QSignalBlocker block(preset_);preset_->setCurrentIndex(preset_->findData(preset));}updateGuidance();emit edited();
}
void RadioSettingsPage::updateGuidance(){
    if(auto* usb=findChild<QPushButton*>("radioUsbDataPtt")){usb->setVisible(model_->currentData().toInt()==2039);usb->setEnabled(enabled_->isChecked());}
    const bool on=enabled_->isChecked(),network=connection_->currentIndex()==1;
    const int id=model_->currentData().toInt();const bool flexDirect=!network&&id>=23005&&id<=23012;
    const auto preset=preset_->currentData().toString(),method=method_->currentData().toString();
    for(auto* widget:{model_,device_,baud_,method_})widget->setEnabled(on&&!network);
    baud_->setEnabled(on&&!network&&!flexDirect);host_->setEnabled(on&&network);port_->setEnabled(on&&network);
    automatic_->setEnabled(on);pttValue_->setEnabled(on&&(network||method=="RIG"||method=="RIGMICDATA"));vfo_->setEnabled(on);lead_->setEnabled(on&&automatic_->isChecked());tail_->setEnabled(on&&automatic_->isChecked());extra_->setEnabled(on);
    const bool externalPtt=automatic_->isChecked()&&!network&&QStringList{"RTS","DTR","GPIO","GPION","CM108"}.contains(method);pttDevice_->setEnabled(on&&externalPtt);
    model_->setVisible(on&&!network);modelLabel_->setVisible(on&&!network);
    device_->setVisible(on&&!network);deviceLabel_->setVisible(on&&!network);refreshDevices_->setVisible(on&&!network&&!flexDirect);
    const bool showBaud=on&&!network&&!flexDirect&&preset!="flex-cat";baud_->setVisible(showBaud);baudLabel_->setVisible(showBaud);
    host_->setVisible(on&&network);port_->setVisible(on&&network);serverLabel_->setVisible(on&&network);
    pttDevice_->setVisible(on&&externalPtt);pttDeviceLabel_->setVisible(on&&externalPtt);
    if(auto* choices=qobject_cast<QStandardItemModel*>(keying_->model()))for(const auto& name:{"rts","dtr"})if(auto* item=choices->item(keying_->findData(name)))item->setEnabled(!network);
    deviceLabel_->setText(flexDirect?"Radio IP address / port":"CAT port / device");device_->setToolTip(flexDirect?"Enter the Flex radio's IP address and SmartSDR API port, for example 192.168.1.50:4992. This is not a rigctld server.":"Choose the CAT port assigned to this application. Audio devices are selected separately.");
    if(device_->lineEdit())device_->lineEdit()->setPlaceholderText(flexDirect?"Radio IP address:4992":"Choose a COM port or enter a device path");
    QString help;
    if(preset=="flex-cat")help="In SmartSDR CAT, create a CAT virtual serial port for the intended slice, then choose that COM port below. The TS-2000 model is SmartSDR's documented Kenwood compatibility mode. For RTS/DTR keying, create a separate PTT port in SmartSDR CAT and enter it below.";
    else if(flexDirect||preset=="flex-network")help="Choose the SmartSDR slice you operate and enter the radio's IP address with port 4992. Automatic CAT PTT controls that slice through Hamlib. Do not put the radio's API address into the rigctld fields. Audio still needs a separate DAX / compatible audio connection.";
    else if(network)help="Enter the address of a running Hamlib rigctld server (usually port 4532). The server owns the radio connection and PTT. This is different from a Flex radio's native network API.";
    else if(preset=="signalink")help="Keep SignaLink selected in audio setup. CAT supplies frequency and mode control; SignaLink keys the radio when transmit audio starts. Choose your actual radio model and CAT cable below.";
    else if(!on)help="Audio remains available. Use this for receive-only operation or a radio you key yourself. Select a connection setup above to enable computer radio control.";
    else help="Choose your radio model and its control port. Select how transmission starts below; using a CAT cable does not change your audio or disable automatic PTT.";
    if(audioConnection_=="flex-dax")help+=" For audio, select your slice's DAX RX channel and DAX TX, then enable DAX TX in SmartSDR.";
    setupHelp_->setText(help);
    QString choice=network?"cat":method=="RTS"?"rts":method=="DTR"?"dtr":method=="RIG"||method=="RIGMICDATA"||network?"cat":"advanced";
    if(!automatic_->isChecked())choice=keying_->currentData()=="external"||audioConnection_=="signalink"||preset=="signalink"?"external":"manual";
    {const QSignalBlocker block(keying_);keying_->setCurrentIndex(keying_->findData(choice));}
    QString keying=!on?"Computer PTT is off. Transmission requires an audio-operated interface, VOX, or manual keying.":!automatic_->isChecked()?"Automatic computer PTT is off. SignaLink / VOX or manual keying starts transmission; CAT remains independent.":network?"Automatic PTT: the rigctld server sends the transmit command.":externalPtt?"Automatic PTT: "+method+" on "+(pttDevice_->text().trimmed().isEmpty()?QString("the CAT port"):pttDevice_->text().trimmed()):method=="None"?"Automatic PTT needs a PTT method. Choose CAT, serial PTT, or an advanced device.":"Automatic PTT: CAT transmit command; audio starts after the radio acknowledges it.";
    if((flexDirect||preset=="flex-cat"||audioConnection_=="flex-dax")&&(!on||!automatic_->isChecked()))keying+=" Flex / DAX does not itself provide automatic keying. Choose automatic CAT or a configured PTT port unless you intend to key manually.";
    keyingHelp_->setText(keying);
}
QString RadioSettingsPage::validationError() const{
    const auto c=config();if(!c.enabled)return {};
    if(c.model==1&&!c.network&&(preset_->currentData()=="cat"||preset_->currentData()=="signalink"))return "Choose your actual radio model. Hamlib Dummy is only a simulation; choose Audio only if you do not use CAT.";
    if(c.model<1)return "Choose a complete radio model from the list.";
    const auto error=c.validate();if(!error.isEmpty())return error;
    const auto preset=preset_->currentData().toString();
    if(preset=="flex-cat"&&c.device.contains(':'))return "Select the virtual CAT COM port created by SmartSDR CAT. For a radio IP address, choose the direct network setup.";
    if(preset=="flex-cat"&&c.model!=2014)return "For the SmartSDR CAT virtual COM setup, use TS-2000 compatibility mode or choose Custom for a different driver.";
    if(preset=="flex-network"&&(c.model<23005||c.model>23012))return "Your Hamlib installation does not list a SmartSDR slice driver. Choose SmartSDR CAT virtual COM or install a Hamlib version with SmartSDR support.";
    if(!c.network&&c.model>=23005&&c.model<=23012){
        const auto path=c.device.trimmed();const auto colon=path.lastIndexOf(':');bool valid=false;const int port=path.mid(colon+1).toInt(&valid);
        if(colon<1||!valid||port<1||port>65535||path.contains(QRegularExpression("[\\s/]")))return "Enter the Flex radio's address with its API port, for example 192.168.1.50:4992.";
    }
    if(preset=="flex-cat"&&c.automaticPtt&&(c.pttType=="RTS"||c.pttType=="DTR")&&(c.pttDevice.trimmed().isEmpty()||c.pttDevice.compare(c.device,Qt::CaseInsensitive)==0))return "Select the separate PTT COM port created in SmartSDR CAT. The CAT port and RTS/DTR PTT port must be different.";
    return {};
}
void RadioSettingsPage::setDeviceChoices(const QStringList& devices){
    const QSignalBlocker blocker(device_);const auto current=device_->currentText();device_->clear();device_->addItems(devices);device_->setEditText(current);
    device_->setToolTip("Serial/USB devices on the shack PC. You can also type the path or network address used by that radio backend on the shack.");
    refreshDevices_->setEnabled(false);refreshDevices_->setToolTip("These are the shack PC's ports. Reopen this CAT setup step to reload its current port list.");
}
radio::Config RadioSettingsPage::config() const{
    radio::Config c;c.enabled=enabled_->isChecked();c.network=connection_->currentIndex()==1;c.automaticPtt=automatic_->isChecked();const int selected=model_->findText(model_->currentText(),Qt::MatchExactly);c.model=selected<0?-1:model_->itemData(selected).toInt();
    c.device=device_->currentText().trimmed();c.baud=baud_->currentData().toInt();c.host=host_->text().trimmed();c.port=port_->value();c.pttType=method_->currentData().toString();c.pttDevice=pttDevice_->text().trimmed();c.pttValue=pttValue_->currentData().toInt();c.targetVfo=vfo_->currentText();c.leadMs=lead_->value();c.tailMs=tail_->value();c.extra=extra_->text().trimmed();return c;
}
RadioControls::RadioControls(radio::Controller* controller,std::function<bool()> idle,std::function<void()> emergency,QWidget* parent,int model):QDialog(parent),controller_(controller),idle_(std::move(idle)){
    setAttribute(Qt::WA_DeleteOnClose);setObjectName("radioControlsDialog");setWindowTitle("Tux-sstv · Radio controls");resize(550,510);auto* layout=new QVBoxLayout(this);status_=new QLabel;status_->setObjectName("radioControlsStatus");status_->setWordWrap(true);layout->addWidget(status_);auto* form=new QFormLayout;layout->addLayout(form);
    const auto row=[&](const QString& label,QWidget* field,const QString& object,std::function<radio::Command()> command){auto* line=new QHBoxLayout;auto* apply=new QPushButton("Set");apply->setObjectName(object);line->addWidget(field,1);line->addWidget(apply);form->addRow(label,line);const QStringList keys={"frequencyHz","mode","vfo","powerPercent","ritHz","split","txFrequencyHz","repeaterShift","repeaterOffsetHz","ctcssToneHz"};const auto key=keys.at(int(command().control));field->setProperty("radioReadbackKey",key);apply->setProperty("radioReadbackKey",key);controls_<<field<<apply;connect(apply,&QPushButton::clicked,this,[this,command]{const QPointer<RadioControls> guard(this);controller_->apply(command(),[guard](const QString& error){if(guard && !error.isEmpty())guard->status_->setText(error);});});};
    const auto frequency=[&](const char* name){auto* field=new QDoubleSpinBox;field->setObjectName(name);field->setDecimals(6);field->setRange(.000001,100000);field->setSuffix(" MHz");return field;};
    auto* frequencyField=frequency("radioFrequency");row("Frequency",frequencyField,"radioSetFrequency",[frequencyField]{return radio::Command{radio::Control::Frequency,frequencyField->value()*1e6,{}};});
    auto* mode=new QComboBox;mode->setObjectName("radioOperatingMode");mode->setEditable(true);mode->addItems(radio::modeNames(model));auto* passband=new QSpinBox;passband->setObjectName("radioPassband");passband->setRange(0,1000000);passband->setSuffix(" Hz");passband->setSpecialValueText("Driver bandwidth");if(model==2039){passband->setEnabled(false);passband->setToolTip("Set filter width on the radio; this Hamlib TS-990S driver does not set or report it");}auto* modeRow=new QWidget;auto* modeLayout=new QHBoxLayout(modeRow);modeLayout->setContentsMargins(0,0,0,0);modeLayout->addWidget(mode);modeLayout->addWidget(passband);row("Mode / bandwidth",modeRow,"radioSetMode",[mode,passband,model]{return radio::Command{radio::Control::Mode,model==2039?0.:double(passband->value()),mode->currentText()};});
    auto* vfo=new QComboBox;vfo->setObjectName("radioVfo");vfo->addItems(model==2039?QStringList{"Main","Sub"}:QStringList{"VFOA","VFOB","Main","Sub","MainA","MainB","SubA","SubB"});row("Current VFO",vfo,"radioSetVfo",[vfo]{return radio::Command{radio::Control::Vfo,0,vfo->currentText()};});
    auto* power=new QDoubleSpinBox;power->setObjectName("radioPower");power->setRange(0,100);power->setSuffix(" %");row("RF power",power,"radioSetPower",[power]{return radio::Command{radio::Control::Power,power->value(),{}};});
    auto* rit=new QSpinBox;rit->setObjectName("radioRit");rit->setRange(-1000000,1000000);rit->setSuffix(" Hz");row("RIT offset",rit,"radioSetRit",[rit]{return radio::Command{radio::Control::Rit,double(rit->value()),{}};});
    auto* split=new QComboBox;split->setObjectName("radioSplit");split->addItems(model==2039?QStringList{"Off","TX on Sub"}:QStringList{"Off","TX on VFOA","TX on VFOB","TX on Main","TX on Sub"});row("Split",split,"radioSetSplit",[split,model]{const QStringList targets=model==2039?QStringList{"Sub","Sub"}:QStringList{"VFOB","VFOA","VFOB","Main","Sub"};return radio::Command{radio::Control::Split,0,targets[split->currentIndex()],split->currentIndex()?1:0};});
    auto* txFrequency=frequency("radioTxFrequency");row("Split TX frequency",txFrequency,"radioSetTxFrequency",[txFrequency]{return radio::Command{radio::Control::SplitFrequency,txFrequency->value()*1e6,{}};});
    auto* shift=new QComboBox;shift->setObjectName("radioRepeaterShift");shift->addItems({"Simplex","Minus","Plus"});row("Repeater shift",shift,"radioSetRepeaterShift",[shift]{return radio::Command{radio::Control::RepeaterShift,0,{},shift->currentIndex()};});
    auto* offset=new QSpinBox;offset->setObjectName("radioRepeaterOffset");offset->setRange(0,100000000);offset->setSuffix(" Hz");row("Repeater offset",offset,"radioSetRepeaterOffset",[offset]{return radio::Command{radio::Control::RepeaterOffset,double(offset->value()),{}};});
    auto* tone=new QDoubleSpinBox;tone->setObjectName("radioCtcss");tone->setDecimals(1);tone->setRange(0,300);tone->setSuffix(" Hz");tone->setSpecialValueText("Tone off");row("CTCSS transmit tone",tone,"radioSetTone",[tone]{return radio::Command{radio::Control::Tone,tone->value(),{}};});
    const auto copy=[this,frequencyField,mode,passband,vfo,power,rit,split,txFrequency,shift,offset,tone,model]{const auto state=controller_->state();if(state.contains("frequencyHz"))frequencyField->setValue(state["frequencyHz"].toDouble()/1e6);if(state.contains("mode"))mode->setCurrentText(state["mode"].toString());if(state.contains("passband"))passband->setValue(state["passband"].toInt());if(state.contains("vfo"))vfo->setCurrentText(state["vfo"].toString());if(state.contains("powerPercent"))power->setValue(state["powerPercent"].toDouble());if(state.contains("ritHz"))rit->setValue(state["ritHz"].toInt());if(state.value("split").isValid()){const QString target=state.value("txVfo").toString();split->setCurrentIndex(!state.value("split").toInt()?0:model==2039?1:target=="VFOA"?1:target=="VFOB"?2:target=="Main"?3:4);}if(state.contains("txFrequencyHz"))txFrequency->setValue(state["txFrequencyHz"].toDouble()/1e6);if(state.contains("repeaterShift"))shift->setCurrentIndex(state["repeaterShift"].toInt());if(state.contains("repeaterOffsetHz"))offset->setValue(state["repeaterOffsetHz"].toInt());if(state.contains("ctcssToneHz"))tone->setValue(state["ctcssToneHz"].toDouble());};
    auto* buttons=new QHBoxLayout;auto* read=new QPushButton("Copy radio values");read->setObjectName("radioCopyValues");buttons->addWidget(read);connect(read,&QPushButton::clicked,this,copy);
    test_=new QPushButton("PTT test (1 s)");test_->setObjectName("radioPttTest");buttons->addWidget(test_);auto* off=new QPushButton("PTT OFF / Stop TX");off->setObjectName("radioPttOff");buttons->addWidget(off);connect(off,&QPushButton::clicked,this,[emergency]{emergency();});layout->addLayout(buttons);
    auto* note=new QLabel("Readback appears above; Copy radio values fills the edit controls. Set changes only its own row. Power is a percentage, not watts. Unsupported operations return the radio's Hamlib error.");note->setWordWrap(true);layout->addWidget(note);auto* close=new QDialogButtonBox(QDialogButtonBox::Close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::close);layout->addWidget(close);
    connect(test_,&QPushButton::clicked,this,[this]{
        testOwned_=true;testGeneration_=controller_->pttGeneration()+1;const auto generation=testGeneration_;const QPointer<RadioControls> guard(this);
        controller_->key([guard,generation](const QString& error){
            if(!guard||guard->testGeneration_!=generation)return;
            const bool owned=guard->testOwned_&&guard->controller_->pttGeneration()==generation;
            if(!error.isEmpty()){guard->status_->setText(error);guard->testOwned_=false;if(owned)guard->controller_->releaseIfCurrent(generation);return;}
            if(!owned)return;
            QTimer::singleShot(1000,guard,[guard,generation]{if(guard&&guard->testOwned_&&guard->testGeneration_==generation&&guard->controller_->pttGeneration()==generation){guard->testOwned_=false;guard->controller_->releaseIfCurrent(generation);}});
        });
    });
    connect(controller_,&radio::Controller::changed,this,&RadioControls::updateState);auto* refresh=new QTimer(this);refresh->setInterval(200);connect(refresh,&QTimer::timeout,this,&RadioControls::updateState);refresh->start();copy();updateState();
}
void RadioControls::updateState(){
    if(testOwned_&&controller_->pttGeneration()!=testGeneration_)testOwned_=false;
    const auto state=controller_->state();status_->setText(controller_->connected()?QString("Connected · %1 MHz · %2 · %3 · PTT %4%5").arg(state.contains("frequencyHz")?QString::number(state["frequencyHz"].toDouble()/1e6,'f',6):"unknown",state.value("mode").toString(),state.value("vfo").toString(),state.contains("ptt")?(state["ptt"].toInt()?"TX":"RX"):"unknown",controller_->error().isEmpty()?"":"\n"+controller_->error()):"Disconnected · "+controller_->error());
    const bool idle=controller_->connected()&&!controller_->pttActive()&&state.value("ptt",0).toInt()==0&&idle_();for(auto* widget:controls_){const auto value=state.value(widget->property("radioReadbackKey").toString());const bool available=value.isValid()&&!value.isNull();widget->setEnabled(idle&&available&&(!qobject_cast<QPushButton*>(widget)||!controller_->busy()));widget->setToolTip(available?QString{}:QString("Readback is unavailable for this control on the connected radio"));}test_->setEnabled(idle&&!controller_->busy()&&controller_->supportsOwnedPtt());
    test_->setToolTip(controller_->supportsOwnedPtt()?QString("Key this radio for one second. Closing this dialog stops only its own test."):QString("Remote PTT tests require Tux-sstv 1.13.1 or newer on the shack PC. Use matching versions; PTT OFF / Stop TX remains available."));
}
void RadioControls::closeEvent(QCloseEvent* event){if(testOwned_){testOwned_=false;controller_->releaseIfCurrent(testGeneration_);}QDialog::closeEvent(event);}
