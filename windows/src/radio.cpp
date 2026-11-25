#include "radio.h"
#include "hamlib_api.h"
#include "audio_process.h"
#include <QProcess>
#include <QRegularExpression>
#include <QPointer>
#include <QDir>
#include <QFileInfo>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <vector>
#include <cwchar>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace radio {
namespace {
[[noreturn]] void fail(const QString& message){throw std::runtime_error(message.toStdString());}
QString failure(const char* operation,int code){return QString::fromLatin1(operation)+": "+QString::fromLocal8Bit(rigerror2(code)).trimmed()+QString(" (Hamlib %1)").arg(code);}
void checked(const char* operation,int code){if(code<0)fail(failure(operation,code));}
bool communicationError(int code){return code==-5 || code==-6 || code==-8 || code==-13 || code==-22;}
void validNumber(double value,double low,double high){if(!std::isfinite(value) || value<low || value>high)fail("Radio value is outside the supported input range.");}
const std::array<const char*,6> dataModes={"USB-D1","USB-D2","USB-D3","LSB-D1","LSB-D2","LSB-D3"};
QString modeName(rmode_t mode){
    for(size_t i=0;i<dataModes.size();++i)if(mode==(rmode_t(1)<<(40+i)))return dataModes[i];
    return QString::fromLatin1(rig_strrmode(mode));
}
rmode_t parseMode(const QString& text,int model){
    const auto name=text.trimmed().toUpper();
    if(model==2039)for(size_t i=0;i<dataModes.size();++i)if(name==dataModes[i])return rmode_t(1)<<(40+i);
    return rig_parse_mode(name.toLatin1().constData());
}
QString comparableMode(QString mode){mode=mode.trimmed().toUpper();if(mode=="PKTUSB")return "USB-D1";if(mode=="PKTLSB")return "LSB-D1";return mode;}
QString nativeSerialPath(const QString& path){
#ifdef Q_OS_WIN
    if(QRegularExpression("^COM[0-9]+$",QRegularExpression::CaseInsensitiveOption).match(path).hasMatch())return "\\\\.\\"+path;
#endif
    return path;
}
class HamlibBackend:public Backend {
    RIG* rig_=nullptr;
    Config config_;
    vfo_t target_=0;
    bool opened_=false,mayBeKeyed_=false;
    void conf(const QString& key,const QString& value){
        const auto name=key.toUtf8(),data=value.toUtf8();const auto token=rig_token_lookup(rig_,name.constData());
        if(!token)fail("Unknown Hamlib setting: "+key);
        checked(name.constData(),rig_set_conf(rig_,token,data.constData()));
    }
    bool available(const char* name,int code){if(communicationError(code))fail(failure(name,code));return code>=0;}
public:
    ~HamlibBackend() override{close();}
    void open(const Config& config) override{
        const auto error=config.validate();if(!error.isEmpty())fail(error);
        if(!close().isEmpty())fail("Could not release the previous radio's PTT.");
        static std::once_flag initialized;std::call_once(initialized,[]{rig_set_debug(RIG_DEBUG_NONE);});
        config_=config;rig_=rig_init(config.network?2:config.model);if(!rig_)fail("Hamlib could not initialize this radio model.");
        try {
            if(config.network){
                const auto host=config.host.contains(':')&&!config.host.startsWith('[')?"["+config.host+"]":config.host;
                conf("rig_pathname",host+":"+QString::number(config.port));
            }else if(!config.device.isEmpty())conf("rig_pathname",nativeSerialPath(config.device));
            if(!config.network && config.baud)conf("serial_speed",QString::number(config.baud));
            for(const auto& item:config.extra.split(',',Qt::SkipEmptyParts)){
                const auto equals=item.indexOf('=');conf(item.left(equals).trimmed(),item.mid(equals+1).trimmed());
            }
            // Bound retries even when an advanced setting requested otherwise.
            conf("timeout","500");conf("retry","0");
            const bool cat=config.network||config.pttType=="RIG"||config.pttType=="RIGMICDATA";
            conf("ptt_type",cat?(config.pttValue>1||config.pttType=="RIGMICDATA"?"RIGMICDATA":"RIG"):config.pttType);
            if(!config.network && !config.pttDevice.isEmpty())conf("ptt_pathname",nativeSerialPath(config.pttDevice));
            target_=rig_parse_vfo(config.targetVfo.toLatin1().constData());if(!target_)fail("Invalid target VFO.");
            checked("Open radio",rig_open(rig_));opened_=true;
        }catch(...){close();throw;}
    }
    QVariantMap read(bool pttOnly=false) override{
        if(!opened_)fail("Radio is disconnected.");
        // rig_open's background poll can restore cache defaults after opening.
        // Read actual hardware state here, including after a successful setter.
        conf("cache_timeout","0");
        QVariantMap result;
        ptt_t ptt=RIG_PTT_OFF;if(available("Read PTT",rig_get_ptt(rig_,target_,&ptt)))result["ptt"]=int(ptt);
        if(pttOnly)return result;
        freq_t frequency=0;if(available("Read frequency",rig_get_freq(rig_,target_,&frequency)) && std::isfinite(frequency))result["frequencyHz"]=frequency;
        rmode_t mode=0;pbwidth_t width=0;if(available("Read mode",rig_get_mode(rig_,target_,&mode,&width))){result["mode"]=modeName(mode);result["passband"]=width>0?QVariant(qlonglong(width)):QVariant{};}
        vfo_t vfo=0;if(available("Read VFO",rig_get_vfo(rig_,&vfo)))result["vfo"]=QString::fromLatin1(rig_strvfo(vfo));
        value_t power{};if(available("Read power",rig_get_level(rig_,target_,rig_parse_level("RFPOWER"),&power)) && std::isfinite(power.f))result["powerPercent"]=double(power.f)*100;
        value_t strength{};result["strengthDb"]=QVariant{};if(available("Read signal strength",rig_get_level(rig_,target_,rig_parse_level("STRENGTH"),&strength)))result["strengthDb"]=strength.i;
        shortfreq_t rit=0;if(available("Read RIT",rig_get_rit(rig_,target_,&rit)))result["ritHz"]=qlonglong(rit);
        split_t split=RIG_SPLIT_OFF;vfo_t tx=0;if(available("Read split",rig_get_split_vfo(rig_,target_,&split,&tx))){result["split"]=int(split);result["txVfo"]=QString::fromLatin1(rig_strvfo(tx));}
        freq_t txFrequency=0;if(available("Read TX frequency",rig_get_split_freq(rig_,target_,&txFrequency)) && std::isfinite(txFrequency))result["txFrequencyHz"]=txFrequency;
        rptr_shift_t shift=RIG_RPT_SHIFT_NONE;if(available("Read repeater shift",rig_get_rptr_shift(rig_,target_,&shift)))result["repeaterShift"]=int(shift);
        shortfreq_t offset=0;if(available("Read repeater offset",rig_get_rptr_offs(rig_,target_,&offset)))result["repeaterOffsetHz"]=qlonglong(offset);
        // Hamlib 4.6.5 sends TO (without a band) for TS-990S tone state,
        // but Kenwood requires TO0/TO1. Do not let this optional, invalid
        // query disconnect an otherwise working radio or claim tone enable.
        if(config_.model==2039)result["ctcssToneHz"]=QVariant{};
        else {
            tone_t tone=0;if(available("Read tone",rig_get_ctcss_tone(rig_,target_,&tone)))result["ctcssToneHz"]=double(tone)/10;
            int toneOn=0;if(available("Read tone encoder",rig_get_func(rig_,target_,rig_parse_func("TONE"),&toneOn))){result["toneEnabled"]=bool(toneOn);if(!toneOn)result["ctcssToneHz"]=0.;}
        }
        return result;
    }
    void setPtt(bool on) override{
        if(!opened_)fail("Radio is disconnected; PTT OFF cannot be confirmed.");
        if(on)mayBeKeyed_=true; // Include ambiguous errors after a radio accepted a command.
        const int code=rig_set_ptt(rig_,target_,on?static_cast<ptt_t>(config_.pttValue):RIG_PTT_OFF);
        if(code<0)fail(failure(on?"PTT ON":"PTT OFF",code));
        mayBeKeyed_=on;
    }
    void apply(const Command& c) override{
        if(!opened_)fail("Radio is disconnected.");
        if(mayBeKeyed_)fail("Return to receive before changing radio settings.");
        switch(c.control){
        case Control::Frequency:validNumber(c.value,1,1e11);checked("Set frequency",rig_set_freq(rig_,target_,c.value));break;
        case Control::Mode:{validNumber(c.value,0,1000000);const auto mode=parseMode(c.text,config_.model);if(!mode)fail("Unknown radio mode. Select a complete mode name, such as USB or USB-D1.");checked("Set mode",rig_set_mode(rig_,target_,mode,long(c.value)));break;}
        case Control::Vfo:{const auto vfo=rig_parse_vfo(c.text.toLatin1().constData());if(!vfo)fail("Unknown VFO.");checked("Set VFO",rig_set_vfo(rig_,vfo));break;}
        case Control::Power:{validNumber(c.value,0,100);value_t value{};value.f=float(c.value/100);checked("Set RF power",rig_set_level(rig_,target_,rig_parse_level("RFPOWER"),value));break;}
        case Control::Rit:validNumber(c.value,-1000000,1000000);checked("Set RIT",rig_set_rit(rig_,target_,long(c.value)));break;
        case Control::Split:{const auto vfo=rig_parse_vfo(c.text.toLatin1().constData());if(!vfo || (c.option!=0 && c.option!=1))fail("Invalid split configuration.");checked("Set split",rig_set_split_vfo(rig_,target_,static_cast<split_t>(c.option),vfo));break;}
        case Control::SplitFrequency:validNumber(c.value,1,1e11);checked("Set TX frequency",rig_set_split_freq(rig_,target_,c.value));break;
        case Control::RepeaterShift:if(c.option<0 || c.option>2)fail("Invalid repeater shift.");checked("Set repeater shift",rig_set_rptr_shift(rig_,target_,static_cast<rptr_shift_t>(c.option)));break;
        case Control::RepeaterOffset:validNumber(c.value,0,100000000);checked("Set repeater offset",rig_set_rptr_offs(rig_,target_,long(c.value)));break;
        case Control::Tone:if(config_.model==2039)fail("Set CTCSS on the radio; this Hamlib TS-990S driver cannot confirm tone-enable state.");
            validNumber(c.value,0,300);
            if(c.value>0)checked("Set CTCSS tone",rig_set_ctcss_tone(rig_,target_,unsigned(std::lround(c.value*10))));
            checked("Set tone encoder",rig_set_func(rig_,target_,rig_parse_func("TONE"),c.value>0?1:0));break;
        }
    }
    QString close() override{
        QString error;
        if(rig_){if(opened_ && mayBeKeyed_){const int code=rig_set_ptt(rig_,target_,RIG_PTT_OFF);if(code<0)error=failure("PTT OFF not confirmed",code);}
            if(opened_)rig_close(rig_);
            rig_cleanup(rig_);}
        rig_=nullptr;opened_=false;mayBeKeyed_=false;return error;
    }
};
}
QStringList modeNames(int model){
    if(model==2039)return {"USB","LSB","USB-D1","USB-D2","USB-D3","LSB-D1","LSB-D2","LSB-D3","FM","FMN","AM","CW","CWR","RTTY","RTTYR","PSK","PSKR"};
    return {"USB","LSB","FM","PKTFM","PKTUSB","PKTLSB","AM","CW","CWR","RTTY","RTTYR","WFM"};
}
QString verifyReadback(const Command& c,const QVariantMap& state){
    const auto number=[&](const QString& key,double expected,double tolerance,const QString& name){
        if(!state.value(key).isValid()||state.value(key).isNull())return name+" was not confirmed: readback is unavailable.";
        const double actual=state.value(key).toDouble();if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance)return QString("Radio reports %1 %2; requested %3.").arg(name).arg(actual,0,'g',12).arg(expected,0,'g',12);return QString{};
    };
    switch(c.control){
    case Control::Frequency:return number("frequencyHz",c.value,1,"frequency (Hz)");
    case Control::Mode:{if(comparableMode(state.value("mode").toString())!=comparableMode(c.text))return "Radio did not confirm mode "+c.text+"; readback: "+state.value("mode","unavailable").toString();return c.value>0?number("passband",c.value,1,"filter width (Hz)"):QString{};}
    case Control::Vfo:if(state.value("vfo").toString()!=c.text)return "Radio did not confirm VFO "+c.text+"; readback: "+state.value("vfo","unavailable").toString();return {};
    case Control::Power:return number("powerPercent",c.value,1,"RF power (%)");
    case Control::Rit:return number("ritHz",c.value,1,"RIT (Hz)");
    case Control::Split:{auto error=number("split",c.option,0,"split");if(!error.isEmpty())return error;if(c.option&&state.value("txVfo").toString()!=c.text)return "Radio did not confirm split TX VFO "+c.text;return {};}
    case Control::SplitFrequency:return number("txFrequencyHz",c.value,1,"TX frequency (Hz)");
    case Control::RepeaterShift:return number("repeaterShift",c.option,0,"repeater shift");
    case Control::RepeaterOffset:return number("repeaterOffsetHz",c.value,1,"repeater offset (Hz)");
    case Control::Tone:return number("ctcssToneHz",c.value,.1,"CTCSS (Hz)");
    }
    return {};
}
QString Config::validate() const{
    if(model<=0 || model>1000000 || baud<0 || baud>4000000 || port<1 || port>65535 || pttValue<1 || pttValue>3 || leadMs<0 || leadMs>5000 || tailMs<0 || tailMs>3000)return "Invalid Hamlib connection or PTT timing settings.";
    if(!QStringList{"RIG","RIGMICDATA","RTS","DTR","None","CM108","GPIO","GPION"}.contains(pttType))return "Select a supported PTT method.";
    if(automaticPtt && pttType=="None" && !network)return "Automatic PTT requires a PTT method.";
    if(network && (host.trimmed().isEmpty() || host.contains(QRegularExpression("[\\s/]"))))return "Enter a rigctld host name or IP address.";
    if(!network && model!=1 && device.trimmed().isEmpty())return "Enter the radio's serial, USB or network device path.";
    if(device.size()>500 || pttDevice.size()>500 || host.size()>255 || extra.size()>2048 || extra.contains('\n') || extra.contains('\r'))return "Radio connection setting is too long or contains a newline.";
    if(!QStringList{"currVFO","VFOA","VFOB","Main","Sub","MainA","MainB","SubA","SubB"}.contains(targetVfo))return "Select a valid target VFO.";
    for(const auto& item:extra.split(',',Qt::SkipEmptyParts))if(!QRegularExpression("^\\s*[A-Za-z0-9_]+\\s*=.+$").match(item).hasMatch())return "Extra Hamlib settings use name=value pairs separated by commas.";
    return {};
}
Config Config::load(QSettings& s){
    Config c;s.beginGroup("radio");c.enabled=s.value("enabled",false).toBool();c.network=s.value("network",false).toBool();c.automaticPtt=s.value("automaticPtt",true).toBool();
    c.model=s.value("model",1).toInt();c.baud=s.value("baud",0).toInt();c.port=s.value("port",4532).toInt();c.pttValue=s.value("pttValue",1).toInt();c.leadMs=s.value("leadMs",300).toInt();c.tailMs=s.value("tailMs",100).toInt();
    c.device=s.value("device").toString();c.host=s.value("host","127.0.0.1").toString();c.pttType=s.value("pttType","RIG").toString();c.pttDevice=s.value("pttDevice").toString();c.targetVfo=s.value("targetVfo","currVFO").toString();c.extra=s.value("extra").toString();s.endGroup();return c;
}
void Config::save(QSettings& s) const{
    s.beginGroup("radio");s.setValue("enabled",enabled);s.setValue("network",network);s.setValue("automaticPtt",automaticPtt);s.setValue("model",model);s.setValue("baud",baud);s.setValue("port",port);s.setValue("pttValue",pttValue);s.setValue("leadMs",leadMs);s.setValue("tailMs",tailMs);s.setValue("device",device);s.setValue("host",host);s.setValue("pttType",pttType);s.setValue("pttDevice",pttDevice);s.setValue("targetVfo",targetVfo);s.setValue("extra",extra);s.endGroup();
}
QJsonObject Config::toJson() const{
    return {{"enabled",enabled},{"network",network},{"automaticPtt",automaticPtt},
        {"model",model},{"baud",baud},{"port",port},{"pttValue",pttValue},{"leadMs",leadMs},{"tailMs",tailMs},
        {"device",device},{"host",host},{"pttType",pttType},{"pttDevice",pttDevice},{"targetVfo",targetVfo},{"extra",extra}};
}
bool Config::fromJson(const QJsonObject& json,Config& result,QString& error){
    Config c;
    const auto boolean=[&](const char* key,bool& target){const auto v=json.value(key);if(!v.isBool())return false;target=v.toBool();return true;};
    const auto integer=[&](const char* key,int& target,int low,int high){const auto v=json.value(key);const auto n=v.toDouble(low-1.);if(!v.isDouble()||!std::isfinite(n)||n<low||n>high||n!=int(n))return false;target=int(n);return true;};
    const auto string=[&](const char* key,QString& target,int max){const auto v=json.value(key);if(!v.isString()||v.toString().size()>max)return false;target=v.toString();return true;};
    if(!boolean("enabled",c.enabled)||!boolean("network",c.network)||!boolean("automaticPtt",c.automaticPtt)||
       !integer("model",c.model,1,1000000)||!integer("baud",c.baud,0,4000000)||!integer("port",c.port,1,65535)||
       !integer("pttValue",c.pttValue,1,3)||!integer("leadMs",c.leadMs,0,5000)||!integer("tailMs",c.tailMs,0,3000)||
       !string("device",c.device,500)||!string("host",c.host,255)||!string("pttType",c.pttType,32)||
       !string("pttDevice",c.pttDevice,500)||!string("targetVfo",c.targetVfo,32)||!string("extra",c.extra,2048)){
        error="Invalid Hamlib setup fields.";return false;
    }
    error=c.enabled?c.validate():QString{};if(!error.isEmpty())return false;
    result=c;return true;
}
std::unique_ptr<Backend> hamlibBackend(){return std::make_unique<HamlibBackend>();}
QStringList serialDevices(){
    QStringList ports;
#ifdef Q_OS_WIN
    std::vector<wchar_t> names(65536);
    const auto size=QueryDosDeviceW(nullptr,names.data(),DWORD(names.size()));
    if(size)for(const wchar_t* name=names.data();*name;name+=wcslen(name)+1){const auto port=QString::fromWCharArray(name);if(QRegularExpression("^COM[0-9]+$").match(port).hasMatch())ports<<port;}
    std::sort(ports.begin(),ports.end(),[](const auto& a,const auto& b){return a.mid(3).toInt()<b.mid(3).toInt();});
#else
    for(const auto& pattern:QStringList{"/dev/serial/by-id/*","/dev/ttyUSB*","/dev/ttyACM*","/dev/ttyS*"}){
        const QFileInfo path(pattern);for(const auto& name:QDir(path.path()).entryList({path.fileName()},QDir::System|QDir::Files))ports<<path.path()+"/"+name;
    }
#endif
    ports.removeDuplicates();return ports;
}
QList<QPair<int,QString>> models(QString* error){
    QProcess process;process.start(audio::bundledProgram("rigctl"),{"-l"});if(!process.waitForFinished(4000) || process.exitCode()!=0){if(error)*error="Cannot list radio models. Check the bundled Hamlib tools or install libhamlib-utils on Linux.";return {{1,"Hamlib Dummy (test radio)"},{2,"NET rigctl"}};}
    const auto lines=process.readAllStandardOutput().split('\n');if(lines.isEmpty())return {};
    const auto header=lines.first();const int manufacturer=header.indexOf("Mfg"),model=header.indexOf("Model"),version=header.indexOf("Version");QList<QPair<int,QString>> result;
    if(manufacturer<0 || model<0 || version<0)return {{1,"Hamlib Dummy (test radio)"}};
    for(const auto& line:lines){bool valid=false;const int id=line.left(manufacturer).trimmed().toInt(&valid);if(valid && id>0)result.append({id,QString::fromUtf8(line.mid(manufacturer,model-manufacturer).trimmed())+" "+QString::fromUtf8(line.mid(model,version-model).trimmed())+QString(" (%1)").arg(id)});}
    return result;
}
Controller::Controller(QObject* parent,std::unique_ptr<Backend> backend):QObject(parent),worker_(new QObject),backend_(backend?std::move(backend):hamlibBackend()){
    worker_->moveToThread(&thread_);connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater);thread_.start();poll_.setInterval(1000);connect(&poll_,&QTimer::timeout,this,&Controller::refresh);poll_.start();
}
Controller::~Controller(){shutdown();}
void Controller::run(std::function<QVariantMap()> work,Done done,bool communicationFault){
    if(stopped_){if(done)done("Radio controller is closed.");return;}++pending_;emit changed();
    QMetaObject::invokeMethod(worker_,[this,work=std::move(work),done=std::move(done),communicationFault]{
        QVariantMap values;QString error;try{values=work();}catch(const std::exception& e){error=QString::fromUtf8(e.what());}
        QMetaObject::invokeMethod(this,[this,values,error,done,communicationFault]{
            --pending_;if(stopped_)return;
            for(auto it=values.begin();it!=values.end();++it)state_[it.key()]=it.value();
            if(!error.isEmpty())error_=error;
            if(communicationFault && !error.isEmpty()){connected_=false;wanted_->store(false);emit fault(error);}
            if(done)done(error);
            emit changed();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void Controller::connectRadio(const Config& config,Done done){
    if(remote_){remoteRun("radio.connect",{},std::move(done));return;}
    if(busy() || wanted_->load() || held_){if(done)done("Radio is busy.");return;}
    const auto validation=config.validate();if(!validation.isEmpty()){if(done)done(validation);return;}
    ++pttGeneration_;error_.clear();state_.clear();connected_=false;
    run([backend=backend_,unconfirmed=unconfirmedClose_,config]{
        try{backend->open(config);const auto state=backend->read();if(state.contains("ptt")&&state["ptt"].toInt()==0)unconfirmed->clear();return state;}
        catch(...){const auto error=backend->close();if(!error.isEmpty())*unconfirmed=error;throw;}
    },[this,done](QString error){connected_=error.isEmpty();if(connected_ && state_.contains("ptt") && state_["ptt"].toInt()==0)uncertain_=false;if(done)done(error);});
}
void Controller::disconnectRadio(Done done){
    ++pttGeneration_;
    if(remote_){remoteRun("radio.disconnect",{},std::move(done));return;}
    wanted_->store(false);run([backend=backend_,unconfirmed=unconfirmedClose_]{
        // After a failed close the connection is gone. A later no-op close
        // cannot prove PTT OFF; only a successful release or RX readback can.
        const auto error=backend->close();if(!error.isEmpty())*unconfirmed=error;if(!unconfirmed->isEmpty())fail(*unconfirmed);return QVariantMap{};
    },[this,done](QString error){connected_=false;held_=false;uncertain_=!error.isEmpty();state_.clear();if(done)done(error);});
}
void Controller::testConnection(const Config& config,Done done){
    if(remote_){done("Configure and test USB radio settings on the shack PC.");return;}
    if(connected_ || busy() || pttActive()){done("Disconnect the active radio before testing another connection.");return;}
    run([config]{auto test=hamlibBackend();test->open(config);const auto state=test->read();test->close();return state;},std::move(done));
}
void Controller::refresh(){
    if(remote_)return; // The shack sends its actual radio readback continuously.
    if(!connected_ || busy())return;
    const bool transmitting=pttActive();
    run([backend=backend_,transmitting]{return backend->read(transmitting);},[this,transmitting](QString error){
        if(error.isEmpty() && transmitting && wanted_->load() && held_ && state_.contains("ptt") && state_["ptt"].toInt()==0){wanted_->store(false);emit fault("Radio returned to receive during transmission.");}
    },true);
}
void Controller::apply(Command command,Done done){
    if(!connected_ || pttActive() || state_.value("ptt",0).toInt()!=0){if(done)done("Radio must be connected, idle and receiving.");return;}
    error_.clear();
    if(remote_){remoteRun("radio.apply",{{"control",int(command.control)},{"value",command.value},{"text",command.text},{"option",command.option}},std::move(done));return;}
    auto readback=std::make_shared<QVariantMap>();
    run([backend=backend_,command,readback]{backend->apply(command);*readback=backend->read();return *readback;},[this,command,readback,done=std::move(done)](QString error){if(error.isEmpty()){state_=*readback;error=verifyReadback(command,*readback);}if(!error.isEmpty())error_=error;if(done)done(error);});
}
void Controller::key(Done done){
    if(!connected_ || pttActive() || state_.value("ptt",0).toInt()!=0){done("Radio is disconnected, already transmitting, or awaiting PTT release.");return;}
    ++pttGeneration_;
    if(remote_){
        const auto generation=pttGeneration_;remoteKeyActivation_.clear();remoteOwnedReleasePending_=0;remoteKeyPending_=true;
        remoteRun("radio.key",{},[this,done,generation](QString error){
            if(generation!=pttGeneration_){if(done)done(error.isEmpty()?QString("PTT activation was cancelled."):error);return;}
            remoteKeyPending_=false;
            // The client applies the reply's snapshot before completing key().
            // An error reply may describe somebody else's TX, so never claim it.
            if(error.isEmpty())remoteKeyActivation_=remotePttActivation_;
            const bool cancelled=remoteOwnedReleasePending_==generation;remoteOwnedReleasePending_=0;
            if(cancelled&&error.isEmpty())releaseIfCurrent(generation);
            if(done)done(cancelled&&error.isEmpty()?QString("PTT test cancelled."):error);
        });return;
    }
    wanted_->store(true);uncertain_=true;
    run([backend=backend_,wanted=wanted_]{
        if(!wanted->load())fail("Transmission cancelled before PTT.");
        try{backend->setPtt(true);}catch(...){try{backend->setPtt(false);}catch(...){}throw;}
        if(!wanted->load()){backend->setPtt(false);fail("Transmission cancelled while keying PTT.");}
        return QVariantMap{{"ptt",1}};
    },[this,done](QString error){held_=error.isEmpty();if(error.isEmpty())uncertain_=false;else wanted_->store(false);done(error);});
}
void Controller::release(Done done){
    ++pttGeneration_;remoteKeyActivation_.clear();remoteOwnedReleasePending_=0;
    if(remote_){remoteKeyPending_=false;remoteRun("radio.release",{},std::move(done));return;}
    wanted_->store(false);run([backend=backend_,unconfirmed=unconfirmedClose_]{backend->setPtt(false);unconfirmed->clear();return QVariantMap{{"ptt",0}};},[this,done](QString error){held_=false;uncertain_=!error.isEmpty();if(error.isEmpty())error_.clear();if(done)done(error);});
}
void Controller::releaseIfCurrent(quint64 generation){
    if(generation!=pttGeneration_)return;
    if(!remote_){release();return;}
    // A dialog may close before the key reply supplies its authoritative host
    // activation. Defer only this owned release; explicit Stop stays immediate.
    if(remoteKeyPending_){remoteOwnedReleasePending_=generation;return;}
    if(remoteKeyActivation_.isEmpty())return;
    const auto activation=remoteKeyActivation_;++pttGeneration_;remoteKeyActivation_.clear();remoteOwnedReleasePending_=0;
    remoteRun("radio.release",{{"expectedPttActivation",activation}},{});
}
QString Controller::shutdown(){
    if(stopped_)return uncertain_?error_:QString{};
    if(remote_)remote_("radio.release",{},{});
    stopped_=true;++remoteGeneration_;++pttGeneration_;poll_.stop();wanted_->store(false);QString error;
    QMetaObject::invokeMethod(worker_,[&]{error=backend_->close();if(!error.isEmpty())*unconfirmedClose_=error;else error=*unconfirmedClose_;},Qt::BlockingQueuedConnection);thread_.quit();thread_.wait();
    connected_=false;held_=false;uncertain_=!error.isEmpty();if(!error.isEmpty())error_=error;return error;
}
void Controller::setRemote(RemoteRequest request){
    ++remoteGeneration_;++pttGeneration_;remote_=std::move(request);remoteKeyPending_=remotePttActive_=false;remotePttActivation_.clear();remoteKeyActivation_.clear();remoteOwnedReleasePending_=0;connected_=false;held_=uncertain_=false;wanted_->store(false);state_.clear();error_.clear();emit changed();
}
void Controller::setRemoteState(bool connected,const QVariantMap& state,const QString& error,bool pttActive,const QString& pttActivation){
    if(!remote_)return;
    // Losing the peer must invalidate an old test even if a restarted host
    // later happens to reuse the same numeric controller generation.
    if(!remotePttActivation_.isEmpty()&&pttActivation.isEmpty()){++pttGeneration_;remoteKeyActivation_.clear();remoteOwnedReleasePending_=0;}
    remotePttActivation_=pttActivation;
    connected_=connected;state_=state;error_=error;remotePttActive_=pttActive;if(!connected)remoteKeyPending_=false;emit changed();
}
void Controller::remoteRun(const QString& operation,const QVariantMap& arguments,Done done){
    if(stopped_){if(done)done("Radio controller is closed.");return;}++pending_;emit changed();const auto generation=remoteGeneration_;QPointer<Controller> self(this);
    remote_(operation,arguments,[self,generation,done](QString error){if(!self)return;--self->pending_;if(self->stopped_||generation!=self->remoteGeneration_)return;self->error_=error;if(done)done(error);emit self->changed();});
}
}
