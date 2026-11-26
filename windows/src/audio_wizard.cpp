#include "audio_process.h"
#include "audio_wizard.h"
#include "branding.h"
#include "core/wav.h"
#include <QtWidgets>
#include <QDesktopServices>
#include <QUrl>
#include <QtEndian>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <QtConcurrentRun>

namespace {
QString selectedAudioDevice(const QComboBox* box){
    const int index=box->currentIndex();
    if(index>=0&&box->currentText()==box->itemText(index)&&box->itemData(index).canConvert<QString>()&&!box->itemData(index).toString().isEmpty())return box->itemData(index).toString();
    return box->currentText().trimmed();
}
void selectAudioDevice(QComboBox* box,const QString& name){const int index=box->findData(name);if(index>=0)box->setCurrentIndex(index);else box->setEditText(name);}
}

QList<AudioWizard::PcmDevice> AudioWizard::pcmDevices(const QByteArray& listing){
    QList<PcmDevice> devices;
    for(const auto& line:QString::fromLocal8Bit(listing).split('\n')){
        if(line.trimmed().isEmpty()||line.startsWith('#'))continue;
        if(!line.front().isSpace()){
            const auto name=line.trimmed();
            if(name.size()<256)devices.append({name,{}});
        }else if(!devices.isEmpty()&&devices.last().description.isEmpty())devices.last().description=line.trimmed();
    }
    return devices;
}

QStringList AudioWizard::pcmNames(const QByteArray& listing) {
    QStringList names;
    for(const auto& device:pcmDevices(listing))if(!names.contains(device.name))names<<device.name;
    return names;
}
AudioWizard::AudioWizard(const QString& input,const QString& output,QWidget* parent,bool relay):QWizard(parent),relay_(relay) {
    setObjectName("audioWizard");setWindowTitle("Tux-sstv audio setup");setPixmap(QWizard::LogoPixmap,branding::tux(36));resize(740,580);
    auto* devices=new QWizardPage;devices->setTitle("1. Choose the sound device for your radio");auto* form=new QFormLayout(devices);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    connection_=new QComboBox;connection_->setObjectName("wizardConnection");
    connection_->addItem("SignaLink USB","signalink");connection_->addItem("Radio with built-in USB audio","radio-usb");connection_->addItem("Other USB audio interface","usb");connection_->addItem("FlexRadio / SmartSDR DAX","flex-dax");connection_->addItem("Computer sound card / other device","other");
    const int savedConnection=connection_->findData(QSettings().value("audio/setupConnection","other").toString());connection_->setCurrentIndex(savedConnection<0?connection_->findData("other"):savedConnection);form->addRow("Audio interface",connection_);
    connectionHelp_=new QLabel;connectionHelp_->setObjectName("wizardConnectionHelp");connectionHelp_->setWordWrap(true);connectionHelp_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);connectionHelp_->setAlignment(Qt::AlignLeft|Qt::AlignTop);form->addRow(connectionHelp_);
    device_=new QComboBox;device_->setObjectName("wizardDevice");device_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);device_->setMinimumContentsLength(26);
    refresh_=new QPushButton("Refresh devices");refresh_->setObjectName("wizardRefresh");auto* row=new QHBoxLayout;row->addWidget(device_,1);row->addWidget(refresh_);deviceLabel_=new QLabel("Sound device");deviceLabel_->setObjectName("wizardDeviceLabel");form->addRow(deviceLabel_,row);
    selection_=new QLabel;selection_->setObjectName("wizardSelection");selection_->setWordWrap(true);form->addRow(selection_);
    discoveryStatus_=new QLabel;discoveryStatus_->setObjectName("wizardDiscoveryStatus");discoveryStatus_->setWordWrap(true);form->addRow(discoveryStatus_);
    advanced_=new QCheckBox("Choose separate receive and transmit devices");advanced_->setObjectName("wizardAdvanced");form->addRow(advanced_);
    auto* advancedPanel=new QWidget;advancedPanel->setObjectName("wizardAdvancedDevices");auto* advancedForm=new QFormLayout(advancedPanel);advancedForm->setContentsMargins(0,0,0,0);
    input_=new QComboBox;output_=new QComboBox;input_->setEditable(true);output_->setEditable(true);
    input_->setInsertPolicy(QComboBox::NoInsert);output_->setInsertPolicy(QComboBox::NoInsert);
    input_->setObjectName("wizardInput");output_->setObjectName("wizardOutput");
    input_->addItem(input.isEmpty()?"default":input);output_->addItem(output.isEmpty()?"default":output);
    advancedForm->addRow("Receive input",input_);advancedForm->addRow("Transmit output",output_);form->addRow(advancedPanel);advancedPanel->hide();connect(advanced_,&QCheckBox::toggled,advancedPanel,&QWidget::setVisible);addPage(devices);
    auto* receive=new QWizardPage;receive->setTitle("2. Check receive audio");auto* rx=new QVBoxLayout(receive);
    receiveDevice_=new QLabel;receiveDevice_->setObjectName("wizardReceiveDevice");receiveDevice_->setWordWrap(true);rx->addWidget(receiveDevice_);
    rxHint_=new QLabel;rxHint_->setObjectName("wizardReceiveHelp");rxHint_->setWordWrap(true);rx->addWidget(rxHint_);
    level_=new QProgressBar;level_->setObjectName("wizardInputLevel");level_->setRange(0,100);level_->setFormat("Peak %p%");rx->addWidget(level_);
    inputStatus_=new QLabel("Input check stopped");inputStatus_->setObjectName("wizardInputStatus");inputStatus_->setWordWrap(true);rx->addWidget(inputStatus_);
    listen_=new QPushButton("Start input meter");listen_->setObjectName("wizardListen");rx->addWidget(listen_);rx->addStretch();addPage(receive);
    auto* transmit=new QWizardPage;transmit->setTitle("3. Set levels and check transmit audio");auto* tx=new QVBoxLayout(transmit);
    transmitDevice_=new QLabel;transmitDevice_->setObjectName("wizardTransmitDevice");transmitDevice_->setWordWrap(true);tx->addWidget(transmitDevice_);
    txHint_=new QLabel;txHint_->setObjectName("wizardTransmitHelp");txHint_->setWordWrap(true);tx->addWidget(txHint_);
    auto* levelRow=new QHBoxLayout;levelRow->addWidget(new QLabel("Tux-sstv TX level"));txLevel_=new QSpinBox;txLevel_->setObjectName("wizardTxLevel");txLevel_->setRange(0,100);txLevel_->setSuffix(" %");txLevel_->setValue(std::clamp(QSettings().value("audio/txLevelPercent",100).toInt(),0,100));levelRow->addWidget(txLevel_);levelRow->addStretch();tx->addLayout(levelRow);
    optimize_=new QPushButton("Optimize SignaLink audio levels");optimize_->setObjectName("wizardOptimizeLevels");tx->addWidget(optimize_);
#ifdef Q_OS_WIN
    if(!relay_)optimize_->setText("Windows sound settings…");
#endif
    mixerStatus_=new QLabel("Optimize unmutes and raises only the selected USB sound device's playback level, and sets Tux-sstv TX to 100%. No tone plays automatically. Finish keeps the changes; Cancel restores the previous levels.");mixerStatus_->setObjectName("wizardMixerStatus");mixerStatus_->setWordWrap(true);tx->addWidget(mixerStatus_);
    tone_=new QPushButton("Play test tone");tone_->setObjectName("wizardTone");tx->addWidget(tone_);
    outputStatus_=new QLabel("Output check stopped");outputStatus_->setObjectName("wizardOutputStatus");outputStatus_->setWordWrap(true);tx->addWidget(outputStatus_);tx->addStretch();addPage(transmit);
    auto* finish=new QWizardPage;finish->setTitle("4. Review and save");auto* end=new QVBoxLayout(finish);
    review_=new QLabel;review_->setObjectName("wizardReview");review_->setWordWrap(true);end->addWidget(review_);
    interfacePtt_=new QCheckBox("Use SignaLink PTT; keep CAT radio control available");interfacePtt_->setObjectName("wizardInterfacePtt");interfacePtt_->setChecked(!QSettings().value("radio/automaticPtt",true).toBool());end->addWidget(interfacePtt_);
    radioHint_=new QLabel;radioHint_->setObjectName("wizardRadioHelp");radioHint_->setWordWrap(true);end->addWidget(radioHint_);
    radio_=new QCheckBox("Open radio controls after Finish");radio_->setObjectName("wizardOpenRadio");end->addWidget(radio_);end->addStretch();addPage(finish);
    connect(this,&QWizard::currentIdChanged,this,[this]{stopChecks();updateGuidance();});
    connect(txLevel_,&QSpinBox::valueChanged,this,[this]{stopChecks();updateGuidance();});
    connect(optimize_,&QPushButton::clicked,this,&AudioWizard::optimizeLevels);
    connect(&mixerJob_,&QFutureWatcher<audio::MixerSetup>::finished,this,&AudioWizard::finishOptimization);
    connect(connection_,&QComboBox::currentIndexChanged,this,[this]{if(connectionType()=="signalink")interfacePtt_->setChecked(true);if(!relay_)rebuildDevices();updateGuidance();});
    connect(device_,&QComboBox::currentIndexChanged,this,[this]{
        const auto pair=device_->currentData().toMap();if(pair.isEmpty()){advanced_->setChecked(true);return;}
        stopChecks();{const QSignalBlocker in(input_),out(output_);selectAudioDevice(input_,pair["input"].toString());selectAudioDevice(output_,pair["output"].toString());}updateGuidance();
    });
    for(auto* box:{input_,output_})connect(box,&QComboBox::currentTextChanged,this,[this]{stopChecks();synchronizeDevice();updateGuidance();});
    connect(refresh_,&QPushButton::clicked,this,&AudioWizard::discoverDevices);
    connect(&inputList_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){discoveryFinished(inputList_,true,code,status);});
    connect(&outputList_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){discoveryFinished(outputList_,false,code,status);});
    for(auto* process:{&inputList_,&outputList_})connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){if(error==QProcess::FailedToStart)discoveryFinished(*process,process==&inputList_,-1,QProcess::CrashExit);});
    discoveryLimit_.setSingleShot(true);connect(&discoveryLimit_,&QTimer::timeout,this,[this]{if(inputList_.state()!=QProcess::NotRunning)inputList_.kill();if(outputList_.state()!=QProcess::NotRunning)outputList_.kill();});
    connect(listen_,&QPushButton::clicked,this,[this]{
        if(relay_){emit relayRequest("listen",{});return;}
        if(capture_.state()!=QProcess::NotRunning){stopChecks();return;}
        oddByte_.clear();stopping_=false;level_->setValue(0);inputStatus_->setText("Opening input…");
        audio::startCapture(capture_,{"-q","-D",inputDevice(),"-t","raw","-f","S16_LE","-c","1","-r","12000"});limit_.start(30000);updateButtons();
    });
    connect(tone_,&QPushButton::clicked,this,[this]{
        if(relay_){emit relayRequest("tone",{});return;}
        if(playback_.state()!=QProcess::NotRunning){stopChecks();return;}
        try {
            const bool signalink=connectionType()=="signalink";
            const int txPercent=transmitPercent();
            if(signalink&&txPercent==0){outputStatus_->setText("TX level is 0%. Raise TX level above before checking SignaLink PTT.");return;}
            const double amplitude=signalink?.65*txPercent/100.:.1;
            sstv::Audio audio;audio.sampleRate=48000;audio.samples.resize(48000);
            for(int n=0;n<48000;++n){const double ramp=std::min({1.,n/240.,(47999-n)/240.});audio.samples[size_t(n)]=float(amplitude*ramp*std::sin(2*std::numbers::pi*1900*n/48000));}
            if(!temporary_.isValid())throw std::runtime_error("Cannot create test audio file.");
            sstv::writeWav(temporary_.filePath("tone.wav"),audio);stopping_=false;outputStatus_->setText("Playing test tone…");
            audio::startPlayback(playback_,{"-q","-D",outputDevice(),temporary_.filePath("tone.wav")});limit_.start(5000);updateButtons();
        }catch(const std::exception& e){outputStatus_->setText(QString::fromUtf8(e.what()));}
    });
    connect(&capture_,&QProcess::readyReadStandardOutput,this,&AudioWizard::measure);
    auto watch=[this](QProcess& process,QLabel* status,const QString& success) {
        connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,&process,status,success](int code,QProcess::ExitStatus exit){
            const auto error=QString::fromLocal8Bit(process.readAllStandardError()).left(500);
            const auto finished=&process==&playback_&&connectionType()=="signalink"?QString("Tone finished — playback succeeded; check whether the SignaLink's red PTT light came on. Keying is not monitored."):success;
            status->setText(stopping_?"Check stopped":code==0 && exit==QProcess::NormalExit?finished:"Audio check failed: "+error);limit_.stop();updateButtons();
        });
        connect(&process,&QProcess::errorOccurred,this,[this,&process,status](QProcess::ProcessError error){if(error==QProcess::FailedToStart){status->setText("Cannot open audio helper: "+process.errorString());limit_.stop();updateButtons();}});
    };
    watch(capture_,inputStatus_,"Input check finished");watch(playback_,outputStatus_,"Tone finished — confirm you heard it and checked the level.");
    limit_.setSingleShot(true);connect(&limit_,&QTimer::timeout,this,[this]{stopChecks();});
    if(relay_){
        for(auto* combo:findChildren<QComboBox*>())if(combo->objectName().startsWith("wizard")){
            if(combo->isEditable())connect(combo,&QComboBox::currentTextChanged,this,[this,combo](const QString& value){if(!relayApplying_)emit relayRequest("edit",{{"name",combo->objectName()},{"value",combo==input_?inputDevice():combo==output_?outputDevice():value}});});
            else connect(combo,&QComboBox::currentIndexChanged,this,[this,combo](int index){if(!relayApplying_)emit relayRequest("edit",{{"name",combo->objectName()},{"value",combo->itemData(index,Qt::UserRole+1).toInt()}});});
        }
        for(auto* box:findChildren<QCheckBox*>())if(box->objectName().startsWith("wizard"))connect(box,&QCheckBox::toggled,this,[this,box](bool value){if(!relayApplying_)emit relayRequest("edit",{{"name",box->objectName()},{"value",value}});});
        connect(txLevel_,&QSpinBox::valueChanged,this,[this](int value){if(!relayApplying_)emit relayRequest("edit",{{"name",txLevel_->objectName()},{"value",value}});});
        connect(this,&QWizard::currentIdChanged,this,[this](int id){if(!relayApplying_)emit relayRequest("step",{{"page",id}});});
        updateGuidance();
    }else {rebuildDevices();updateGuidance();discoverDevices();}
}
AudioWizard::~AudioWizard(){if(relay_){relayClosing_=true;return;}mixerJob_.disconnect(this);finishMixer(false);discoveryLimit_.stop();inputList_.disconnect(this);outputList_.disconnect(this);stopChecks();inputList_.kill();outputList_.kill();inputList_.waitForFinished(500);outputList_.waitForFinished(500);}
QString AudioWizard::inputDevice() const{return selectedAudioDevice(input_);}
QString AudioWizard::outputDevice() const{return selectedAudioDevice(output_);}
bool AudioWizard::openRadioControls() const{return radio_->isChecked();}
QString AudioWizard::connectionType() const{return connection_->currentData().toString();}
bool AudioWizard::useInterfacePtt() const{return connectionType()=="signalink"&&interfacePtt_->isChecked();}
int AudioWizard::transmitPercent() const{return txLevel_->value();}
void AudioWizard::optimizeLevels(){
    if(relay_){emit relayRequest("optimize",{});return;}
    if(optimizationPending_||connectionType()!="signalink")return;
#ifdef Q_OS_WIN
    stopChecks();QDesktopServices::openUrl(QUrl("ms-settings:sound"));mixerStatus_->setText("Select this USB output in Windows Sound settings, unmute it and raise its level. Then use the TX level and Play test tone to check SignaLink keying. This preview does not change Windows hardware mixer levels automatically.");return;
#endif
    stopChecks();optimizationPending_=true;mixerStatus_->setText("Checking and adjusting the selected USB playback mixer…");updateButtons();
    const auto output=outputDevice();mixerJob_.setFuture(QtConcurrent::run([output]{return audio::optimizePlayback(output);}));
}
void AudioWizard::finishOptimization(){
    if(!optimizationPending_)return;
    const auto result=mixerJob_.result();optimizationPending_=false;
    mixerStatusCard_=result.card;
    if(!result.original.isEmpty()&&!mixerOriginals_.contains(result.card))mixerOriginals_[result.card]=result;
    if(mixerOriginals_.contains(result.card))mixerOriginals_[result.card].optimized=result.optimized;
    if(result.optimized){txLevel_->setValue(100);mixerStatus_->setText(result.description+" Use Play test tone to check the red PTT light. Finish keeps these levels; Cancel restores the previous mixer settings.");}
    else mixerStatus_->setText(result.error);
    updateButtons();
}
void AudioWizard::finishMixer(bool keep){
    if(mixerClosed_)return;
    if(optimizationPending_){mixerJob_.waitForFinished();finishOptimization();}
    const auto selected=audio::mixerCard(outputDevice());
    for(const auto& original:mixerOriginals_)if(!keep||connectionType()!="signalink"||original.card!=selected||!original.optimized){
        const auto error=audio::restorePlayback(original);if(!error.isEmpty()){if(!restoreError_.isEmpty())restoreError_+=' ';restoreError_+=original.card+": "+error;}
    }
    mixerClosed_=true;
}
namespace {
QString windowsDeviceKey(const QString& description){
    const auto name=description.trimmed();
    static const QRegularExpression parenthesized("\\(([^()]*)\\)\\s*$");
    static const QRegularExpression direction("^(?:microphone|line(?: in)?|speakers?|headphones?|input|output|recording|playback)\\s*[-:]\\s*(.+)$",QRegularExpression::CaseInsensitiveOption);
    const auto parentheses=parenthesized.match(name),prefix=direction.match(name);
    return (parentheses.hasMatch()?parentheses.captured(1):prefix.hasMatch()?prefix.captured(1):name).simplified().toCaseFolded();
}
QString interfaceLabel(const AudioWizard::PcmDevice& device,bool signalink){
    const auto label=device.description.isEmpty()?device.name:device.description;
    static const QRegularExpression codec("USB[\\s_-]*AUDIO[\\s_-]*CODEC",QRegularExpression::CaseInsensitiveOption);
    if(signalink&&!label.contains("SignaLink",Qt::CaseInsensitive)&&codec.match(label).hasMatch())return "SignaLink USB — "+label;
    return label;
}
QString physicalKey(const QString& name){
    static const QRegularExpression pattern("^(?:plughw|hw|sysdefault|front):CARD=([^,]+)(?:,DEV=([0-9]+))?$");
    const auto match=pattern.match(name);return match.hasMatch()?match.captured(1)+":"+(match.captured(2).isEmpty()?QString("0"):match.captured(2)):QString{};
}
QMap<QString,AudioWizard::PcmDevice> physicalDevices(const QList<AudioWizard::PcmDevice>& devices){
    QMap<QString,AudioWizard::PcmDevice> result;
    QSet<QString> ambiguous;
    for(const auto& device:devices){
        if(device.name.startsWith("winmm:")){
            const auto key="windows:"+windowsDeviceKey(device.description);
            if(key=="windows:"||ambiguous.contains(key))continue;
            if(result.contains(key)){result.remove(key);ambiguous.insert(key);}else result[key]=device;
            continue;
        }
        const auto key=physicalKey(device.name);
        if(key.isEmpty()||(!device.name.startsWith("plughw:")&&!device.name.startsWith("sysdefault:")))continue;
        if(!result.contains(key)||device.name.startsWith("plughw:"))result[key]=device;
    }
    return result;
}
}
void AudioWizard::discoverDevices(){
    if(relay_){emit relayRequest("refresh",{});return;}
    if(inputList_.state()!=QProcess::NotRunning||outputList_.state()!=QProcess::NotRunning)return;
    inputListed_=outputListed_=false;discoveryErrors_.clear();refresh_->setEnabled(false);discoveryStatus_->setText("Looking for sound devices…");
    for(auto* process:{&inputList_,&outputList_})if(process->isOpen()){process->readAllStandardOutput();process->readAllStandardError();}
    discoveryLimit_.start(2500);audio::startCapture(inputList_,{"-L"});audio::startPlayback(outputList_,{"-L"});
}
void AudioWizard::discoveryFinished(QProcess& process,bool input,int code,QProcess::ExitStatus status){
    bool& listed=input?inputListed_:outputListed_;if(listed)return;listed=true;
    auto& choices=input?inputs_:outputs_;
    if(!code&&status==QProcess::NormalExit)choices=pcmDevices(process.readAllStandardOutput());
    else {choices.clear();discoveryErrors_<<QString("Could not scan %1 devices.").arg(input?"receive":"transmit");}
    if(!inputListed_||!outputListed_)return;
    discoveryLimit_.stop();refresh_->setEnabled(true);rebuildDevices();
    const auto receive=physicalDevices(inputs_),transmit=physicalDevices(outputs_);
    int common=0;for(auto it=receive.begin();it!=receive.end();++it)if(transmit.contains(it.key()))++common;
    discoveryStatus_->setText(!discoveryErrors_.isEmpty()?discoveryErrors_.join(' ')+" Refresh to retry, or use Advanced.":
        common?QString("%1 sound device%2 found for both receive and transmit. Select one above.").arg(common).arg(common==1?"":"s"):
        "No sound device found for both directions. Connect your USB device, wait a moment, then Refresh. Separate input/output choices are under Advanced.");
    if(connectionType()=="flex-dax")discoveryStatus_->setText("DAX receive and transmit use separate devices. Select the RX channel for your slice and the DAX TX output below.");
    updateGuidance();
}
void AudioWizard::rebuildDevices(){
    for(const auto& entry:{qMakePair(input_,inputs_),qMakePair(output_,outputs_)}){
        const QSignalBlocker block(entry.first);const auto current=selectedAudioDevice(entry.first);entry.first->clear();entry.first->addItem(current,current);
        for(const auto& device:entry.second){const int found=entry.first->findData(device.name);if(found<0)entry.first->addItem(device.name.startsWith("winmm:")?interfaceLabel(device,false):device.name,device.name);if(device.name.startsWith("winmm:"))entry.first->setItemText(entry.first->findData(device.name),interfaceLabel(device,false));entry.first->setItemData(entry.first->findData(device.name),device.description,Qt::ToolTipRole);}
        selectAudioDevice(entry.first,current);
    }
    const QSignalBlocker block(device_);device_->clear();device_->addItem("Separate devices / saved custom setup…",QVariantMap{});
    device_->addItem("Computer's default sound device",QVariantMap{{"input","default"},{"output","default"}});
    const auto receive=physicalDevices(inputs_),transmit=physicalDevices(outputs_);
    QList<QString> keys;for(auto it=receive.begin();it!=receive.end();++it)if(transmit.contains(it.key()))keys<<it.key();
    std::stable_sort(keys.begin(),keys.end(),[&](const auto& a,const auto& b){const auto usb=[&](const auto& key){const auto& d=receive[key];return (d.description+key).contains("USB",Qt::CaseInsensitive)||(d.description+key).contains("CODEC",Qt::CaseInsensitive);};return usb(a)>usb(b);});
    for(const auto& key:keys){
        const auto& in=receive[key];const auto& out=transmit[key];QString label=interfaceLabel(in,connectionType()=="signalink");
        int duplicates=0;for(const auto& other:keys)if(receive[other].description==in.description)++duplicates;
        if(duplicates>1)label+=" ("+key+")";
        device_->addItem(label,QVariantMap{{"input",in.name},{"output",out.name}});device_->setItemData(device_->count()-1,QString("Receive: %1\nTransmit: %2").arg(in.name,out.name),Qt::ToolTipRole);
    }
    if(connectionType()=="flex-dax"){
        const auto choose=[&](QComboBox* box,const QList<PcmDevice>& devices,const QString& direction){
            if(selectedAudioDevice(box)!="default")return;
            QStringList matches;
            for(const auto& d:devices){const auto text=d.description+" "+d.name;if(text.contains("DAX",Qt::CaseInsensitive)&&text.contains(QRegularExpression("\\b"+direction+"\\b",QRegularExpression::CaseInsensitiveOption))&&!text.contains("IQ",Qt::CaseInsensitive)&&!text.contains("reserved",Qt::CaseInsensitive))matches<<d.name;}
            if(matches.size()==1)selectAudioDevice(box,matches.first());
        };
        choose(input_,inputs_,"RX");choose(output_,outputs_,"TX");
    }
    synchronizeDevice();
}
void AudioWizard::synchronizeDevice(){
    const QSignalBlocker block(device_);int match=0;
    for(int i=1;i<device_->count();++i){
        const auto pair=device_->itemData(i).toMap();const auto in=pair["input"].toString(),out=pair["output"].toString();
        if((inputDevice()==in&&outputDevice()==out)||(!physicalKey(inputDevice()).isEmpty()&&physicalKey(inputDevice())==physicalKey(in)&&!physicalKey(outputDevice()).isEmpty()&&physicalKey(outputDevice())==physicalKey(out))){match=i;break;}
    }
    device_->setCurrentIndex(match);if(!match&&inputListed_&&outputListed_)advanced_->setChecked(true);
}
QString AudioWizard::deviceLabel(const QString& name,const QList<PcmDevice>& devices) const{
    if(name=="default")return "Computer's default sound device";
    for(const auto& device:devices)if(device.name==name)return interfaceLabel(device,connectionType()=="signalink");
    return name;
}
void AudioWizard::updateGuidance(){
    const bool signalink=connectionType()=="signalink",usb=connectionType()=="radio-usb",dax=connectionType()=="flex-dax";
    if(dax){advanced_->setChecked(true);}
    advanced_->setVisible(!dax);device_->setVisible(!dax);deviceLabel_->setText(dax?"DAX devices":"Sound device");device_->setEnabled(!dax&&!optimizationPending_);
    const int txPercent=transmitPercent();
    connectionHelp_->setText(dax?
        "Select your slice's DAX RX channel for receive and DAX TX for transmit below. Enable that RX channel and DAX TX in SmartSDR. CAT / PTT is configured in the next station setup step; DAX audio alone does not key the radio.":signalink?
        "Choose SignaLink USB — USB AUDIO CODEC for receive and transmit. It works with any compatible radio; CAT is separate. To identify your interface among several USB codecs, unplug it and Refresh, then reconnect and Refresh.":usb?
        "Choose your radio's USB sound device for both receive and transmit. It may appear as USB Audio CODEC instead of the radio model. A USB cable for CAT / frequency control alone does not provide audio.":
        "Choose the sound device connected to your radio. One selection sets both receive and transmit. For a microphone and speakers on different devices, use Advanced below.");
    const auto input=deviceLabel(inputDevice(),inputs_),output=deviceLabel(outputDevice(),outputs_);
    selection_->setText(QString("Receive from: %1\nSend audio to: %2").arg(input,output)+((signalink||usb)&&inputDevice()=="default"?"\nChoose your USB device above so audio does not follow the computer's default microphone and speakers.":QString{}));
    receiveDevice_->setText("Listening to: "+input);transmitDevice_->setText("Sending audio to: "+output);
    rxHint_->setText(QString("Tune your radio to an active signal, then start the input meter. %1 Adjust until the meter responds without clipping. The check stops after 30 seconds; it does not play audio through your speakers.").arg(signalink?"Use the SignaLink RX knob to adjust receive audio.":usb?"Adjust the radio's USB receive-audio level.":"Adjust the radio or interface receive-audio level."));
    if(signalink){
        txHint_->setText(QString("Click Optimize SignaLink audio levels, then play the one-second tone and watch the red PTT light. No radio is needed to check this light. The test uses your TX setting (%1%). The SignaLink can key a connected radio when the tone plays; use its TX knob to adjust audio sent to that radio. The wizard cannot read the PTT light.").arg(txPercent));
    }else txHint_->setText(QString("Play a one-second 1900 Hz tone at a low test level (10%). %1 The check does not request Hamlib PTT; a radio or interface using VOX can transmit when audio plays.").arg(usb?"Select USB audio as the radio's transmit-audio source if its menu requires it.":"Check the interface output and your radio's audio level."));
    if(dax){
        rxHint_->setText("Choose the DAX RX channel assigned to your active SmartSDR slice. Start the input meter and adjust the DAX RX level without clipping.");
        txHint_->setText("Choose DAX TX and enable DAX TX in SmartSDR. This tone checks audio only; it does not request CAT PTT. Configure keying in the next station setup step.");
    }
    interfacePtt_->setVisible(signalink);
    optimize_->setVisible(signalink);mixerStatus_->setVisible(signalink);
    radioHint_->setText(signalink?"SignaLink carries receive/transmit audio and handles PTT. You can also connect a separate CAT cable for frequency and mode control. Keep SignaLink selected here and enable CAT in the next setup step. The checkbox keeps CAT settings and turns off automatic CAT PTT.":"Audio and radio control are separate. If your radio needs computer-controlled PTT, configure it in Radio setup and Connect. Some radios require USB audio to be selected in their own menus.");
    const auto card=audio::mixerCard(outputDevice());const bool optimized=signalink&&mixerOriginals_.contains(card)&&mixerOriginals_[card].optimized;
    if(card!=mixerStatusCard_&&!optimizationPending_){mixerStatusCard_=card;mixerStatus_->setText(optimized?mixerOriginals_[card].description:"Optimize unmutes and raises only the selected USB sound device's playback level, and sets Tux-sstv TX to 100%. No tone plays automatically. Finish keeps the changes; Cancel restores the previous levels.");}
#ifdef Q_OS_WIN
    if(!relay_){
        mixerStatus_->setText("Open Windows sound settings to unmute and set the selected USB playback level. Hardware mixer changes are manual in this preview; use the TX level and test tone to check SignaLink keying.");
        if(signalink)txHint_->setText(QString("Open Windows sound settings and set the USB playback level, then play the one-second tone and check the red PTT light. The test uses the TX setting (%1%). The wizard cannot read the PTT light.").arg(txPercent));
    }
#endif
    review_->setText(QString("Receive from: %1\nTransmit through: %2\nTux-sstv TX level: %3%%4\n\nFinish saves these choices. Cancel restores your previous setup.").arg(input,output).arg(transmitPercent()).arg(optimized?"\nUSB playback levels optimized and verified.":""));
}
bool AudioWizard::validateCurrentPage(){
    if(relay_)return true; // The shack validates its own devices before advancing.
    if(currentId()==0&&connectionType()=="flex-dax"&&(inputDevice().isEmpty()||outputDevice().isEmpty()||inputDevice()=="default"||outputDevice()=="default")){
        discoveryStatus_->setText("Choose both a receive input and a transmit output for DAX. The computer's default microphone and speakers are not DAX devices.");return false;
    }
    return QWizard::validateCurrentPage();
}
void AudioWizard::done(int result){if(relay_){if(!relayApplying_&&!relayClosing_)emit relayRequest(result==Accepted?"finish":"cancel",{});if(result!=Accepted)finishRelay(result);return;}stopChecks();if(result==Accepted && (inputDevice().isEmpty() || outputDevice().isEmpty())){QMessageBox::information(this,"Audio devices","Enter both input and output device names.");return;}finishMixer(result==Accepted);QWizard::done(result);}
void AudioWizard::stopChecks(){if(relay_){if(!relayApplying_&&!relayClosing_)emit relayRequest("stop",{});return;}stopping_=true;limit_.stop();capture_.kill();playback_.kill();capture_.waitForFinished(500);playback_.waitForFinished(500);level_->setValue(0);updateButtons();}
void AudioWizard::updateButtons(){
    const bool checking=capture_.state()!=QProcess::NotRunning||playback_.state()!=QProcess::NotRunning;
    listen_->setText(capture_.state()==QProcess::NotRunning?"Start input meter":"Stop input meter");tone_->setText(playback_.state()==QProcess::NotRunning?"Play test tone":"Stop test tone");
    optimize_->setEnabled(!optimizationPending_&&!checking);listen_->setEnabled(!optimizationPending_);tone_->setEnabled(!optimizationPending_);txLevel_->setEnabled(!optimizationPending_&&!checking);
    for(auto* box:{connection_,device_,input_,output_}){box->setEnabled(!optimizationPending_);}
    device_->setEnabled(!optimizationPending_&&connectionType()!="flex-dax");
    if(currentPage()){
        const bool final=currentPage()->isFinalPage()||nextId()<0;
        button(QWizard::BackButton)->setEnabled(!optimizationPending_&&visitedIds().size()>1);
        button(QWizard::NextButton)->setEnabled(!optimizationPending_&&!final&&currentPage()->isComplete());
        button(QWizard::FinishButton)->setEnabled(!optimizationPending_&&final&&currentPage()->isComplete());
    }
}
void AudioWizard::measure(){
    auto bytes=oddByte_+capture_.readAllStandardOutput();oddByte_=bytes.size()%2?bytes.right(1):QByteArray{};
    double peak=0,sum=0;const int count=bytes.size()/2;
    for(int i=0;i<count;++i){const double sample=qFromLittleEndian<qint16>(bytes.constData()+i*2)/32768.;peak=std::max(peak,std::abs(sample));sum+=sample*sample;}
    if(!count)return;
    level_->setValue(int(std::lround(peak*100)));const double rms=std::sqrt(sum/count);
    inputStatus_->setText(peak>.98?"Clipping — reduce input level":peak<.003?"Very low input — check the radio and selected device":QString("Signal present · RMS %1 dBFS · adjust level on the radio/interface").arg(20*std::log10(std::max(rms,1e-6)),0,'f',1));
}
