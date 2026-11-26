#include "mainwindow.h"
#include "audio_wizard.h"
#include "audio_levels_panel.h"
#include "branding.h"
#include "radio_ui.h"
#include <QtWidgets>
#include <QJsonArray>
#include <QScopeGuard>

namespace {
// Keep asynchronous station changes on the page until their acknowledgement.
class SetupPage final : public QWizardPage {
public:
    std::function<void()> initialize;
    std::function<bool()> validate;
    void initializePage() override { if(initialize)initialize(); }
    bool validatePage() override { return !validate || validate(); }
};
QLabel* paragraph(QVBoxLayout* layout,const QString& text,const char* name=nullptr) {
    auto* label=new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    if(name)label->setObjectName(name);
    layout->addWidget(label);
    return label;
}
bool sameConnection(const remote::Config& a,const remote::Config& b) {
    return a.role==b.role && a.host==b.host && a.port==b.port && a.key==b.key && a.reconnect==b.reconnect;
}
}

void MainWindow::showSetupWizard() {
    if(setupWizard_){setupWizard_->raise();setupWizard_->activateWindow();return;}
    if(!prepareSetup("Setup wizard",[this]{showSetupWizard();}))return;
    audioLevels_->stopChecks();
    QWizard wizard(this);
    setupWizard_=&wizard;
    const auto clear=qScopeGuard([this]{setupWizard_.clear();});
    wizard.setObjectName("setupWizard");
    wizard.setWindowTitle("Tux-sstv setup wizard");
    wizard.setWizardStyle(QWizard::ModernStyle);
    wizard.setPixmap(QWizard::LogoPixmap,branding::tux(40));
    wizard.setMinimumSize(740,650);
    wizard.setOption(QWizard::NoBackButtonOnStartPage);

    auto* connection=new SetupPage;
    connection->setTitle("1. Station and remote connection");
    connection->setSubTitle("Choose where the radio is connected, then set up audio and CAT.");
    auto* connectionLayout=new QVBoxLayout(connection);
    auto* form=new QFormLayout;
    connectionLayout->addLayout(form);
    auto* role=new QComboBox;
    role->setObjectName("setupRole");
    role->addItems({"Local — radio connected to this computer","Shack PC — share my radio over Wi-Fi","Remote laptop — connect to the shack PC"});
    role->setCurrentIndex(int(remoteConfig_.role));
    form->addRow("Use this computer as",role);
    auto* host=new QLineEdit(remoteConfig_.host);
    host->setObjectName("setupHost");
    host->setPlaceholderText("Shack PC address, for example 192.168.1.50");
    form->addRow("Shack PC address",host);
    auto* port=new QSpinBox;
    port->setObjectName("setupPort");port->setRange(1,65535);port->setValue(remoteConfig_.port);
    form->addRow("Connection port",port);
    auto* key=new QLineEdit(remoteConfig_.key);
    key->setObjectName("setupKey");key->setMaxLength(128);
    form->addRow("Connection key",key);
    auto* generate=new QPushButton("Generate new connection key");
    generate->setObjectName("setupNewKey");form->addRow("",generate);
    auto* retry=new QCheckBox("Reconnect automatically when Wi-Fi returns");
    retry->setObjectName("setupReconnect");retry->setChecked(remoteConfig_.reconnect);
    connectionLayout->addWidget(retry);
    auto* connectionHelp=paragraph(connectionLayout,{},"setupConnectionHelp");
    auto* connectionError=paragraph(connectionLayout,{},"setupConnectionError");
    paragraph(connectionLayout,"Next saves the station connection. Audio and CAT changes save in their own steps; closing this wizard keeps settings already saved.");
    connectionLayout->addStretch();
    wizard.addPage(connection);

    auto* audioPage=new SetupPage;
    audioPage->setTitle("2. Audio devices and levels");
    audioPage->setSubTitle("Choose SignaLink, radio USB audio, FlexRadio DAX, or separate input/output devices.");
    auto* audioLayout=new QVBoxLayout(audioPage);
    auto* target=paragraph(audioLayout,{},"setupAudioTarget");
    auto* audioSummary=paragraph(audioLayout,{},"setupAudioSummary");
    auto* audioButton=new QPushButton("Choose audio interface, devices and levels…");
    audioButton->setObjectName("setupConfigureAudio");audioLayout->addWidget(audioButton);
    paragraph(audioLayout,"Choose the receive and transmit sound devices, then check their levels. A SignaLink test tone can key the radio through audio. FlexRadio DAX uses separate RX and TX audio choices; computer PTT is selected in the next step.");
    paragraph(audioLayout,"Audio and radio control are separate. Keep working audio choices, or open the audio setup above. Next lets you choose CAT, a separate PTT port, SignaLink / VOX, or manual keying.");
    audioLayout->addStretch();wizard.addPage(audioPage);

    auto* catPage=new SetupPage;
    catPage->setTitle("3. Radio control and transmit keying");
    catPage->setSubTitle("Choose your connection and how the radio should start transmitting.");
    auto* catLayout=new QVBoxLayout(catPage);
    auto* catTarget=paragraph(catLayout,{},"setupCatTarget");
    auto* catScroll=new QScrollArea;
    catScroll->setWidgetResizable(true);catLayout->addWidget(catScroll,1);
    auto* catError=paragraph(catLayout,{},"setupCatError");
    paragraph(catLayout,"Next saves these settings and closes the previous radio connection. On the final page you can connect without transmitting, then optionally test PTT for one second.");
    wizard.addPage(catPage);

    auto* review=new SetupPage;
    review->setTitle("4. Setup summary");
    review->setSubTitle("Review the saved audio and keying choices, then check the radio connection.");
    auto* reviewLayout=new QVBoxLayout(review);
    auto* summary=paragraph(reviewLayout,{},"setupSummary");
    paragraph(reviewLayout,"Live reception starts automatically when an audio input is available. Use Radio → Connect for CAT control, then check the radio's frequency, mode and audio levels before transmitting.");
    auto* checkRow=new QHBoxLayout;
    auto* connectCheck=new QPushButton("Connect and check radio");connectCheck->setObjectName("setupConnectRadio");checkRow->addWidget(connectCheck);
    auto* pttCheck=new QPushButton("Test PTT for 1 second");pttCheck->setObjectName("setupPttTest");checkRow->addWidget(pttCheck);reviewLayout->addLayout(checkRow);
    auto* checkStatus=paragraph(reviewLayout,"Connection and PTT have not been tested here.","setupCheckStatus");
    reviewLayout->addStretch();wizard.addPage(review);

    const auto configuration=[&]{
        remote::Config c;c.role=remote::Role(role->currentIndex());c.host=host->text().trimmed();
        c.port=quint16(port->value());c.key=key->text().trimmed();c.reconnect=retry->isChecked();return c;
    };
    const auto targetText=[this]{return remoteClientMode()?"Shack PC over Wi-Fi ("+remoteConfig_.host+")":remoteConfig_.role==remote::Role::Host?QString("This shack PC — shared over Wi-Fi"):QString("This computer — local USB / sound card");};
    const auto audioText=[this]{
        if(remoteClientMode()){
            if(!remoteClient_->connected())return "Shack disconnected: "+remoteClient_->status()+"\nGo Back to check its address and key. Audio devices can be configured after it connects.";
            const auto state=remoteClient_->snapshot();
            return QString("Receive: %1\nTransmit: %2").arg(state.value("input").toString(),state.value("output").toString());
        }
        return QString("Receive: %1\nTransmit: %2").arg(inputDevice_->text(),outputDevice_->text());
    };
    const auto refreshAudio=[&]{
        target->setText("Audio hardware: "+targetText());audioSummary->setText(audioText());
        audioButton->setEnabled(!audioSetupBusy_&&(!remoteClientMode()||(remoteClient_->connected()&&remoteClient_->snapshot().value("audioWizard").toBool())));
    };
    const auto refreshConnection=[&]{
        const auto mode=configuration().role;
        host->setEnabled(mode==remote::Role::Client);port->setEnabled(mode!=remote::Role::Local);
        key->setEnabled(mode!=remote::Role::Local);generate->setEnabled(mode==remote::Role::Host);retry->setEnabled(mode==remote::Role::Client);
        connectionHelp->setText(mode==remote::Role::Host?
            "Leave Tux-sstv running on this shack PC. On the laptop, choose Remote laptop and copy this port and key. Shack addresses: "+remote::localAddresses().join(", "):
            mode==remote::Role::Client?QString("Copy the address, port and key from the shack PC. The next steps configure the shack's audio and CAT; laptop microphone and speaker settings are kept. Use your home Wi-Fi/LAN without router port forwarding."):
            QString("The next steps configure audio and CAT for the radio connected to this computer. Your saved remote address and key are kept for later."));
        connectionError->setText(configuration().validate());
    };
    connect(role,&QComboBox::currentIndexChanged,&wizard,[&]{if(role->currentIndex()==int(remote::Role::Host)&&key->text().isEmpty())key->setText(remote::newKey());refreshConnection();});
    connect(generate,&QPushButton::clicked,&wizard,[key]{key->setText(remote::newKey());});
    connect(host,&QLineEdit::textChanged,&wizard,refreshConnection);connect(key,&QLineEdit::textChanged,&wizard,refreshConnection);
    connect(port,&QSpinBox::valueChanged,&wizard,refreshConnection);
    refreshConnection();

    bool changingConnection=false;
    remote::Config pendingConnection;
    QElapsedTimer connectionClock;
    QTimer connectionWait;
    connectionWait.setInterval(100);
    connect(&connectionWait,&QTimer::timeout,&wizard,[&]{
        if(sameConnection(remoteConfig_,pendingConnection)){
            connectionWait.stop();changingConnection=false;wizard.setEnabled(true);wizard.next();
        }else if(connectionClock.elapsed()>10000){
            connectionWait.stop();changingConnection=false;wizard.setEnabled(true);
            connectionError->setText("The station connection could not be changed. "+status_->text());
        }
    });
    connection->validate=[&]{
        if(changingConnection)return false;
        const auto config=configuration();const auto error=config.validate();
        if(!error.isEmpty()){connectionError->setText(error);return false;}
        if(sameConnection(config,remoteConfig_))return true;
        if(!prepareSetup("Setup wizard",[this]{showSetupWizard();}))return false;
        setRemoteConfig(config);
        if(sameConnection(config,remoteConfig_))return true;
        changingConnection=true;pendingConnection=config;connectionClock.start();connectionWait.start();
        connectionError->setText("Closing the previous radio connection…");wizard.setEnabled(false);return false;
    };
    audioPage->initialize=refreshAudio;
    QTimer audioWait;
    audioWait.setInterval(100);
    connect(&audioWait,&QTimer::timeout,&wizard,[&]{if(!audioSetupBusy_){audioWait.stop();wizard.setEnabled(true);refreshAudio();}});
    connect(audioButton,&QPushButton::clicked,&wizard,[&]{
        showAudioWizard();refreshAudio();
        if(audioSetupBusy_){wizard.setEnabled(false);audioWait.start();}
    });
    connect(remoteClient_,&remote::Client::snapshotChanged,&wizard,refreshAudio);
    connect(remoteClient_,&remote::Client::changed,&wizard,refreshAudio);

    RadioSettingsPage* cat=nullptr;
    radio::Config original;QString originalProfile;
    bool catAvailable=false,catRemote=false,savingCat=false,catSaved=false;
    quint64 catSession=0,connectionRevision=0;
    bool connected=remoteClient_->connected();
    connect(remoteClient_,&remote::Client::changed,&wizard,[&]{
        const bool now=remoteClient_->connected();if(now!=connected){connected=now;++connectionRevision;}
    });
    catPage->initialize=[&]{
        catRemote=remoteClientMode();original=radioConfig_;QString error;
        const auto state=remoteClient_->snapshot();
        originalProfile=state.value("profiles").toObject().value("active").toString();
        catAvailable=!catRemote||(remoteClient_->connected()&&radio::Config::fromJson(state.value("radioConfig").toObject(),original,error));
        catSession=connectionRevision;
        cat=new RadioSettingsPage(original,radio_);
        cat->setAudioConnection(catRemote?state.value("audioConnection").toString():QSettings().value("audio/setupConnection","other").toString());
        if(auto* old=catScroll->takeWidget())delete old;
        catScroll->setWidget(cat);catScroll->setVisible(catAvailable);
        catTarget->setText("CAT hardware: "+targetText());
        catError->setText(catAvailable?QString{}:"Shack CAT settings are unavailable. Go Back to connect, or choose Next to keep the current CAT setup.");
        // All connection checks go through Radio → Connect after saving. This
        // also prevents a remote setup page from probing laptop serial ports.
        cat->findChild<QPushButton*>("radioCheckConnection")->hide();
        if(catRemote){QStringList devices;for(const auto& value:state.value("radioDevices").toArray())if(value.isString())devices<<value.toString();cat->setDeviceChoices(devices);}
        catSaved=false;
    };
    catPage->validate=[&]{
        if(savingCat)return false;
        if(catSaved){catSaved=false;return true;}
        if(!catAvailable)return true;
        const auto config=cat->config();
        if(config.enabled){const auto error=cat->validationError();if(!error.isEmpty()){catError->setText(error);return false;}}
        if(catRemote&&(!remoteClient_->connected()||catSession!=connectionRevision)){catError->setText("Shack connection changed. Go Back and reopen this step before saving.");return false;}
        if(config==original)return true;
        if(!catRemote&&original!=radioConfig_){catError->setText("CAT settings changed elsewhere. Go Back and reopen this step before saving.");return false;}
        savingCat=true;wizard.setEnabled(false);catError->setText("Saving CAT setup…");
        const QPointer<QWizard> guard(&wizard);
        const auto done=[&,guard,config](QString error){
            if(!guard)return;
            QTimer::singleShot(0,guard,[&,guard,config,error]{
                if(!guard)return;
                savingCat=false;wizard.setEnabled(true);
                if(!error.isEmpty()){catError->setText(error);return;}
                original=config;catSaved=true;wizard.next();
            });
        };
        if(catRemote)remoteClient_->command("radio.configure",{{"config",config.toJson()},{"expected",original.toJson()},{"expectedProfile",originalProfile}},done);
        else applyRadioSetup(config,done);
        return false;
    };
    bool testOwned=false;quint64 testGeneration=0;
    const auto releaseTest=qScopeGuard([&]{if(testOwned)radio_->releaseIfCurrent(testGeneration);});
    const auto refreshReview=[&]{
        radio::Config config=radioConfig_;QString error;
        const bool available=!remoteClientMode()||(remoteClient_->connected()&&radio::Config::fromJson(remoteClient_->snapshot().value("radioConfig").toObject(),config,error));
        const auto* model=cat?cat->findChild<QComboBox*>("radioModel"):nullptr;
        const auto modelName=model&&model->currentData().toInt()==config.model?model->currentText():QString("Hamlib model %1").arg(config.model);
        QString catText=!available?"Unavailable — reconnect to check the shack setup":!config.enabled?"Disabled — audio-only operation":config.network?QString("rigctld · %1:%2").arg(config.host).arg(config.port):modelName+" · "+config.device;
        const auto keying=!available?QString("Unavailable — reconnect to the shack"):
            !config.enabled?QString("External / manual keying — computer radio control is disabled"):
            !config.automaticPtt?QString("SignaLink / VOX or manual keying; CAT remains independent"):
            config.network?QString("Automatic transmit control through rigctld"):
            config.pttType=="RTS"||config.pttType=="DTR"?QString("Automatic %1 PTT on %2").arg(config.pttType,config.pttDevice.isEmpty()?config.device:config.pttDevice):
            config.pttType=="None"?QString("Not configured — select a PTT method"):
            QString("Automatic CAT PTT (%1)").arg(config.pttValue==3?"data / USB input":config.pttValue==2?"microphone input":"default transmit audio");
        connectCheck->setEnabled(available&&config.enabled&&!radio_->busy()&&!radio_->pttActive());
        pttCheck->setEnabled(available&&config.enabled&&config.automaticPtt&&radio_->connected()&&!radio_->busy()&&!radio_->pttActive()&&radio_->supportsOwnedPtt());
        pttCheck->setToolTip("This explicit test keys the radio for one second. Verify the radio enters TX and returns to RX.");
        if(!testOwned&&!radio_->error().isEmpty())checkStatus->setText("Radio check: "+radio_->error());
        summary->setText("Station: "+targetText()+"\n\n"+audioText()+"\n\nCAT: "+catText+"\nPTT: "+keying+"\nRadio connection: "+(!available?QString("unavailable"):!config.enabled?QString("not used"):radio_->connected()?QString("connected"):QString("not connected — use the check below"))+"\n\n"+remoteStatus_->text());
    };
    review->initialize=refreshReview;
    connect(radio_,&radio::Controller::changed,&wizard,[&]{if(wizard.currentId()==3)refreshReview();});
    connect(connectCheck,&QPushButton::clicked,&wizard,[&]{checkStatus->setText("Checking the radio connection; no transmit command is requested.");connectRadio();});
    connect(radio_,&radio::Controller::changed,&wizard,[&]{if(wizard.currentId()==3&&!testOwned&&radio_->connected()&&!radio_->busy()&&radio_->error().isEmpty()&&!checkStatus->text().startsWith("PTT"))checkStatus->setText("Radio connected. Audio levels and actual PTT operation still need to be checked.");});
    connect(pttCheck,&QPushButton::clicked,&wizard,[&]{
        testOwned=true;testGeneration=radio_->pttGeneration()+1;const auto generation=testGeneration;const QPointer<QWizard> guard(&wizard);
        checkStatus->setText("PTT test: requesting transmit for one second…");
        radio_->key([&,guard,generation](QString error){
            if(!guard||!testOwned||testGeneration!=generation)return;
            if(!error.isEmpty()){testOwned=false;radio_->releaseIfCurrent(generation);checkStatus->setText("PTT test failed: "+error);refreshReview();return;}
            QTimer::singleShot(1000,guard,[&,guard,generation]{
                if(!guard||!testOwned||testGeneration!=generation)return;
                if(radio_->pttGeneration()!=generation){testOwned=false;checkStatus->setText("PTT test ended because radio control changed.");refreshReview();return;}
                testOwned=false;radio_->release([&,guard](QString error){if(!guard)return;checkStatus->setText(error.isEmpty()?"PTT commands completed. Confirm the radio entered TX and returned to RX.":"PTT OFF was not confirmed: "+error);refreshReview();});
            });
        });
    });
    connect(&wizard,&QWizard::currentIdChanged,&wizard,[&](int page){if(page!=3&&testOwned){testOwned=false;radio_->releaseIfCurrent(testGeneration);}});
    wizard.exec();
}
