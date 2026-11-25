#include "remote_station.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageAuthenticationCode>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QtEndian>
#include <algorithm>
#include <stdexcept>

namespace remote {
namespace {
[[noreturn]] void fail(const char* error){throw std::runtime_error(error);}
QByteArray mac(const QByteArray& message,const QByteArray& key){return QMessageAuthenticationCode::hash(message,key,QCryptographicHash::Sha256);}
bool equal(const QByteArray& a,const QByteArray& b){if(a.size()!=b.size())return false;unsigned difference=0;for(qsizetype n=0;n<a.size();++n)difference|=static_cast<unsigned char>(a[n])^static_cast<unsigned char>(b[n]);return difference==0;}
QByteArray sessionKey(const QByteArray& nonce,const QString& key){return mac("OpenSSTV session v1:"+nonce,key.toUtf8());}
QByteArray proof(const QByteArray& nonce,const QString& key){return mac("OpenSSTV auth v1:"+nonce,key.toUtf8()).toHex();}
bool validKey(const QString& key){return QRegularExpression("^[A-Za-z0-9-]{16,128}$").match(key).hasMatch();}
QString profileMismatch(const QJsonObject& request,const QJsonObject& snapshot){
    const auto profiles=snapshot.value("profiles").toObject();const auto active=profiles.value("active").toString(),expected=request.value("profile").toString();
    if(active.isEmpty()||expected==active||(expected.isEmpty()&&profiles.value("items").toArray().size()<=1))return {};
    return "Radio profile changed. Review the selected radio and try again using Tux-sstv 1.13 or newer.";
}
qint64 waveDuration(const QString& path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))fail("Cannot read uploaded audio.");
    const auto h=file.read(44);const auto n=file.size();
    if(h.size()!=44||h.first(4)!="RIFF"||h.mid(8,8)!="WAVEfmt "||h.mid(36,4)!="data"||
       qFromLittleEndian<quint32>(h.constData()+4)!=n-8||qFromLittleEndian<quint32>(h.constData()+16)!=16||
       qFromLittleEndian<quint16>(h.constData()+20)!=1||qFromLittleEndian<quint16>(h.constData()+22)!=1||
       qFromLittleEndian<quint32>(h.constData()+24)!=48000||qFromLittleEndian<quint32>(h.constData()+28)!=96000||
       qFromLittleEndian<quint16>(h.constData()+32)!=2||qFromLittleEndian<quint16>(h.constData()+34)!=16||
       qFromLittleEndian<quint32>(h.constData()+40)!=n-44||n<=44||n>MaxWaveBytes||(n-44)%2||n-44>600*96000)
        fail("Remote TX requires complete mono 48000 Hz 16-bit PCM audio, at most ten minutes.");
    return std::max(qint64(1),(n-44)*1000/96000);
}
}
QString newKey(){QByteArray bytes(16,Qt::Uninitialized);for(int n=0;n<16;n+=4)qToBigEndian(QRandomGenerator::system()->generate(),bytes.data()+n);return QString::fromLatin1(bytes.toHex());}
QStringList localAddresses(){QStringList result;for(const auto& address:QNetworkInterface::allAddresses())if(address.protocol()==QAbstractSocket::IPv4Protocol&&!address.isLoopback())result<<address.toString();result.removeDuplicates();return result;}
Config Config::load(QSettings& s){Config c;const int role=s.value("remote/role",0).toInt();c.role=role>=0&&role<=2?Role(role):Role::Local;c.host=s.value("remote/host").toString();c.key=s.value("remote/key").toString();const int port=s.value("remote/port",48765).toInt();c.port=port>0&&port<=65535?quint16(port):48765;c.reconnect=s.value("remote/reconnect",true).toBool();return c;}
void Config::save(QSettings& s) const{s.setValue("remote/role",int(role));s.setValue("remote/host",host);s.setValue("remote/key",key);s.setValue("remote/port",port);s.setValue("remote/reconnect",reconnect);}
QString Config::validate() const{if(role==Role::Local)return {};if(role!=Role::Host&&role!=Role::Client)return "Choose a remote station role.";if(!port)return "Choose a port from 1 to 65535.";if(!validKey(key))return "Copy the connection key from the shack PC (16–128 letters, digits or dashes).";if(role==Role::Client&&(host.trimmed().isEmpty()||host.size()>255||host.contains(QRegularExpression("[\\s/]"))))return "Enter the shack PC's address or host name.";return {};}

QByteArray frame(const QJsonObject& message,const QByteArray& data){const auto json=QJsonDocument(message).toJson(QJsonDocument::Compact);if(json.size()>8192||json.size()+data.size()+4>MaxFrameBytes)fail("Remote frame is too large.");QByteArray bytes(8,Qt::Uninitialized);qToBigEndian<quint32>(quint32(json.size()+data.size()+4),bytes.data());qToBigEndian<quint32>(quint32(json.size()),bytes.data()+4);bytes+=json;bytes+=data;return bytes;}
std::vector<Packet> Decoder::feed(const QByteArray& bytes){
    std::vector<Packet> packets;qsizetype position=0;
    while(position<bytes.size()){
        const auto target=expected_?qsizetype(expected_+4):4;
        const auto count=std::min(target-buffer_.size(),bytes.size()-position);buffer_.append(bytes.constData()+position,count);position+=count;
        if(!expected_&&buffer_.size()==4){expected_=qFromBigEndian<quint32>(buffer_.constData());if(expected_<6||expected_>MaxFrameBytes)fail("Invalid remote frame length.");}
        if(expected_&&buffer_.size()==expected_+4){
            const auto jsonBytes=qFromBigEndian<quint32>(buffer_.constData()+4);if(jsonBytes<2||jsonBytes>8192||jsonBytes>expected_-4)fail("Invalid remote JSON length.");
            QJsonParseError error;const auto json=QJsonDocument::fromJson(buffer_.mid(8,jsonBytes),&error);
            if(error.error!=QJsonParseError::NoError||!json.isObject())fail("Invalid remote message.");
            packets.push_back({json.object(),buffer_.mid(8+jsonBytes)});buffer_.clear();expected_=0;
        }
    }return packets;
}
Channel::Channel(QIODevice* device,QObject* parent):QObject(parent),device_(device){
    device->setParent(this);connect(device,&QIODevice::readyRead,this,&Channel::read);connect(device,&QIODevice::bytesWritten,this,[this]{emit writable();});
    connect(device,&QIODevice::aboutToClose,this,[this]{close("Connection closed");});connect(device,&QIODevice::readChannelFinished,this,[this]{close("Connection lost");});
    if(auto* socket=qobject_cast<QTcpSocket*>(device)){socket->setReadBufferSize(MaxFrameBytes*2);connect(socket,&QTcpSocket::disconnected,this,[this]{close("Shack connection lost");});connect(socket,&QTcpSocket::errorOccurred,this,[this,socket]{close(socket->errorString());});}
    QTimer::singleShot(0,this,&Channel::read);
}
void Channel::secure(const QByteArray& key){key_=key;sent_=received_=0;}
qint64 Channel::queuedBytes() const{return device_?device_->bytesToWrite():0;}
bool Channel::send(QJsonObject message,const QByteArray& data){
    if(closed_||!device_)return false;
    if(queuedBytes()>256*1024){close("Wi-Fi connection is too slow; reconnect to resume receiving.");return false;}
    if(!key_.isEmpty()){message["_sequence"]=double(++sent_);message["_mac"]=QString::fromLatin1(mac(QJsonDocument(message).toJson(QJsonDocument::Compact)+data,key_).toHex());}
    try{const auto bytes=frame(message,data);if(device_->write(bytes)!=bytes.size()){close("Could not send remote data.");return false;}}
    catch(const std::exception& e){close(QString::fromUtf8(e.what()));return false;}return true;
}
void Channel::read(){
    if(closed_||!device_)return;
    try{while(!closed_&&device_->bytesAvailable())for(auto packet:decoder_.feed(device_->read(65536))){
        if(closed_)break;
        if(!key_.isEmpty()){
            const auto signature=packet.message.take("_mac").toString().toLatin1();const auto sequence=packet.message.value("_sequence").toDouble(-1);
            if(sequence!=received_+1||!equal(signature,mac(QJsonDocument(packet.message).toJson(QJsonDocument::Compact)+packet.data,key_).toHex()))fail("Remote authentication failed.");
            ++received_;packet.message.remove("_sequence");
        }
        emit this->packet(packet.message,packet.data);
    }}catch(const std::exception& e){close(QString::fromUtf8(e.what()));}
}
void Channel::close(const QString& reason){if(closed_)return;closed_=true;if(device_){if(auto* socket=qobject_cast<QTcpSocket*>(device_.data()))socket->abort();else device_->close();}emit ended(reason);}

Server::Server(QObject* parent,int leaseMs):QObject(parent),leaseMs_(leaseMs){
    setObjectName("remoteServer");connect(&listener_,&QTcpServer::newConnection,this,[this]{while(listener_.hasPendingConnections())acceptConnection(listener_.nextPendingConnection());});
    watchdog_.setInterval(std::max(50,std::min(500,leaseMs/3)));connect(&watchdog_,&QTimer::timeout,this,[this]{if(owner_&&heartbeat_.elapsed()>leaseMs_)owner_->close("Laptop heartbeat expired");});watchdog_.start();
}
Server::~Server(){stop();}
bool Server::start(quint16 port,const QString& key,const QHostAddress& address){stop();key_=key;error_.clear();if(!validKey(key)){error_="Generate a connection key first.";return false;}if(!listener_.listen(address,port)){error_=listener_.errorString();return false;}return true;}
void Server::stop(){finishLocalMonitor(false);releaseLocalTransmit();listener_.close();const auto connections=connections_;for(auto* channel:connections)channel->close("Shack sharing stopped");cancelUpload();}
void Server::acceptConnection(QIODevice* device){
    if(!device)return;
    if(connections_.size()>=4||!validKey(key_)){device->close();device->deleteLater();return;}
    auto* channel=new Channel(device,this);connections_.insert(channel);const auto nonce=newKey().toLatin1();
    connect(channel,&Channel::ended,this,[this,channel](const QString&){connections_.remove(channel);lostOwner(channel);channel->deleteLater();});
    auto authenticated=std::make_shared<bool>(false);
    connect(channel,&Channel::packet,this,[this,channel,nonce,authenticated](const QJsonObject& message,const QByteArray& data){
        if(!*authenticated){
            if(message.value("type")!="auth"||message.value("version").toInt()!=1||!data.isEmpty()||!equal(message.value("proof").toString().toLatin1(),proof(nonce,key_))){channel->close("Connection key was rejected");return;}
            channel->secure(sessionKey(nonce,key_));if(owner_){channel->send({{"type","rejected"},{"error","Another laptop already controls this shack PC."}});QTimer::singleShot(100,channel,[channel]{channel->close("Station already in use");});return;}
            *authenticated=true;owner_=channel;++session_;heartbeat_.start();pcmSequence_=0;monitorSupported_=false;if(channel->send({{"type","welcome"},{"snapshot",snapshot_},{"duplexMonitor",true},{"voice",true}})&&owner_==channel)emit clientChanged(true);return;
        }
        if(channel!=owner_)return;
        handle(message,data);
    });
    QTimer::singleShot(5000,channel,[channel,authenticated]{if(!*authenticated)channel->close("Pairing timed out");});
    channel->send({{"type","challenge"},{"version",1},{"nonce",QString::fromLatin1(nonce)}});
}
void Server::lostOwner(Channel* channel){if(owner_!=channel)return;owner_.clear();++session_;finishVoice("Connection lost");transmitting_=false;cancelUpload();emit emergencyStop();emit clientChanged(false);}
void Server::setSnapshot(const QJsonObject& state){if(snapshot_==state)return;snapshot_=state;if(owner_)owner_->send({{"type","snapshot"},{"snapshot",state}});}
void Server::publishPcm(const QByteArray& pcm){if(!owner_)return;for(qsizetype offset=0;offset<pcm.size()&&owner_;offset+=32768){const auto block=pcm.mid(offset,32768);owner_->send({{"type","pcm"},{"offset",double(pcmSequence_)},{"rate",12000}},block);pcmSequence_+=block.size();}}
QString Server::reserveLocalTransmit(){if(localReserved_||remoteTransmitBusy())return "The station is preparing or transmitting. Stop that transmission first.";localReserved_=true;return {};}
void Server::releaseLocalTransmit(){localReserved_=false;}
void Server::startLocalMonitor(){finishLocalMonitor(false);monitorActive_=true;monitorSequence_=0;++monitorId_;if(owner_&&monitorSupported_)owner_->send({{"type","tx.monitor.begin"},{"stream",double(monitorId_)}});}
void Server::publishMonitorPcm(const QByteArray& pcm){
    if(!monitorActive_||!owner_||!monitorSupported_)return;
    for(qsizetype offset=0;offset<pcm.size()&&owner_;offset+=32768){const auto block=pcm.mid(offset,32768);owner_->send({{"type","pcm"},{"source","tx"},{"stream",double(monitorId_)},{"offset",double(monitorSequence_)},{"rate",12000}},block);monitorSequence_+=block.size();}
}
void Server::finishLocalMonitor(bool success){if(!monitorActive_)return;monitorActive_=false;if(owner_&&monitorSupported_)owner_->send({{"type","tx.monitor.end"},{"stream",double(monitorId_)},{"success",success}});}
void Server::reply(qint64 id,const QString& error,const QJsonObject& result,quint64 session,bool includeSnapshot){
    if(!owner_||(session&&session!=session_))return;
    if(includeSnapshot)emit activityChanged();
    QJsonObject message{{"type","reply"},{"id",double(id)},{"error",error},{"result",result}};if(includeSnapshot)message["snapshot"]=snapshot_;
    if(owner_&&(!session||session==session_))owner_->send(message);
}
void Server::finishTransmit(bool success,const QString& message){if(!transmitting_)return;transmitting_=false;if(owner_)owner_->send({{"type","tx.finished"},{"tx",double(transmitToken_)},{"success",success},{"message",message}});transmitToken_=0;}
void Server::abortTransmit(const QString& message){const auto token=upload_.isOpen()?uploadToken_:transmitToken_;const bool active=upload_.isOpen()||transmitting_;cancelUpload();transmitting_=false;transmitToken_=0;if(active&&owner_)owner_->send({{"type","tx.finished"},{"tx",double(token)},{"success",false},{"message",message}});}
void Server::finishVoice(const QString& message){
    if(!voiceToken_)return;
    const auto token=voiceToken_,request=voiceRequest_;voiceToken_=voiceSequence_=voiceRequest_=0;voiceReady_=false;
    emit voiceStopped();
    if(request)reply(request,message.isEmpty()?"Voice PTT released.":message);
    if(owner_)owner_->send({{"type","voice.finished"},{"stream",double(token)},{"message",message}});
    emit activityChanged();
}
void Server::cancelUpload(){upload_.close();uploadSize_=uploadPosition_=uploadToken_=0;expectedHash_.clear();uploadHash_.reset();}
void Server::handle(const QJsonObject& message,const QByteArray& data){
    const auto type=message.value("type").toString();
    if(type=="ping"){heartbeat_.restart();owner_->send({{"type","pong"}});return;}
    if(type=="capabilities"){
        monitorSupported_=message.value("duplexMonitor").toBool();
        if(monitorSupported_&&monitorActive_){monitorSequence_=0;owner_->send({{"type","tx.monitor.begin"},{"stream",double(monitorId_)}});}return;
    }
    if(type=="voice.pcm"){
        if(!voiceToken_||message.value("stream").toDouble(-1)!=voiceToken_)return;
        if(!voiceReady_||message.value("rate").toInt()!=48000||message.value("offset").toDouble(-1)!=voiceSequence_||data.isEmpty()||data.size()>9600||data.size()%2){finishVoice("Invalid or interrupted voice audio.");return;}
        voiceSequence_+=data.size();if(voicePcm)voicePcm(data);return;
    }
    const auto number=message.value("id").toDouble(-1);if(number<1||number>1e12||number!=qint64(number)){owner_->close("Invalid remote request ID");return;}const qint64 id=qint64(number);
    if(type=="stop"){finishVoice("Voice stopped at shack PC");cancelUpload();emit emergencyStop();finishTransmit(false,"Stopped at shack PC");reply(id);return;}
    if(type=="voice.end"){if(voiceToken_&&message.value("stream").toDouble(-1)==voiceToken_)finishVoice();return;}
    if(type=="voice.begin"){
        const auto profileError=profileMismatch(message,snapshot_);if(!profileError.isEmpty()){reply(id,profileError);return;}
        const auto token=message.value("stream").toDouble(-1);
        const auto busy=canTransmit?canTransmit():QString("Remote voice unavailable.");
        if(localReserved_||remoteTransmitBusy()||!busy.isEmpty()){reply(id,busy.isEmpty()?"Shack PC is preparing or transmitting.":busy);return;}
        if(!data.isEmpty()||token<1||token>1e12||token!=qint64(token)||!beginVoice){reply(id,"Voice is unavailable. Update Tux-sstv on both computers.");return;}
        voiceToken_=qint64(token);voiceSequence_=0;voiceRequest_=id;voiceReady_=false;
        const auto session=session_;QPointer<Server> guard=this;emit activityChanged();
        // Publishing status can synchronously close the owner and cancel voice.
        if(!guard||!owner_||session!=session_||voiceToken_!=token)return;
        beginVoice([this,guard,session,token,id](QString error){
            if(!guard||session!=session_||voiceToken_!=token)return;
            if(!error.isEmpty()){finishVoice(error);return;}
            voiceReady_=true;voiceRequest_=0;reply(id);
        });return;
    }
    if(type=="tx.begin"){
        const auto profileError=profileMismatch(message,snapshot_);if(!profileError.isEmpty()){reply(id,profileError);return;}
        const auto busy=canTransmit?canTransmit():QString("Remote transmission is unavailable.");if(localReserved_||remoteTransmitBusy()||!busy.isEmpty()){reply(id,busy.isEmpty()?"Shack PC is preparing or transmitting a picture.":busy);return;}
        const auto size=message.value("bytes").toDouble(-1);const auto hash=message.value("sha256").toString().toLatin1();
        const auto morse=message.value("morse").toDouble(-1);
        const auto token=message.value("tx").toDouble(-1);
        if(size<46||size>MaxWaveBytes||size!=qint64(size)||token<1||token>1e12||token!=qint64(token)||!QRegularExpression("^[a-f0-9]{64}$").match(QString::fromLatin1(hash)).hasMatch()||morse< -1||morse>600000||morse!=qint64(morse)){reply(id,"Invalid remote audio upload.");return;}
        if(!directory_.isValid()){reply(id,"Cannot create shack audio directory.");return;}upload_.setFileName(directory_.filePath("remote-tx.wav"));if(!upload_.open(QIODevice::WriteOnly|QIODevice::Truncate)){reply(id,upload_.errorString());return;}
        uploadToken_=qint64(token);uploadSize_=qint64(size);uploadPosition_=0;expectedHash_=hash;morseStart_=qint64(morse);uploadHash_.reset();reply(id);return;
    }
    if(type=="tx.chunk"){
        if(!upload_.isOpen()||data.isEmpty()||data.size()>65536||message.value("offset").toDouble(-1)!=uploadPosition_||data.size()>uploadSize_-uploadPosition_){cancelUpload();reply(id,"Invalid audio chunk; transmission cancelled.");return;}
        if(upload_.write(data)!=data.size()){const auto error=upload_.errorString();cancelUpload();reply(id,error);return;}uploadHash_.addData(data);uploadPosition_+=data.size();return;
    }
    if(type=="tx.commit"){
        const auto profileError=profileMismatch(message,snapshot_);if(!profileError.isEmpty()){cancelUpload();reply(id,profileError);return;}
        if(!upload_.isOpen()||uploadPosition_!=uploadSize_||!equal(uploadHash_.result().toHex(),expectedHash_)){cancelUpload();reply(id,"Incomplete or damaged audio; nothing was transmitted.");return;}
        upload_.close();const auto path=upload_.fileName();const auto token=uploadToken_;cancelUpload();
        try{
            const auto duration=waveDuration(path);if(morseStart_>=duration)fail("Invalid Morse ID timing.");const auto busy=canTransmit?canTransmit():QString("Remote transmission unavailable.");if(!busy.isEmpty()){reply(id,busy);return;}
            transmitting_=true;transmitToken_=token;const auto error=transmit?transmit(path,duration,morseStart_):QString("Remote transmission unavailable.");if(!error.isEmpty()){transmitting_=false;transmitToken_=0;reply(id,error);return;}reply(id,{},{{"durationMs",double(duration)},{"morse",double(morseStart_)}});
        }catch(const std::exception& e){reply(id,QString::fromUtf8(e.what()));}return;
    }
    if(type=="command"&&data.isEmpty()){emit request(id,message.value("operation").toString(),message.value("arguments").toObject());return;}
    reply(id,"Unsupported remote request.");
}

Client::Client(QObject* parent,int leaseMs):QObject(parent),leaseMs_(leaseMs){
    setObjectName("remoteClient");clock_.start();tick_.setInterval(std::max(50,std::min(1000,leaseMs/3)));
    connect(&tick_,&QTimer::timeout,this,[this]{
        if(channel_){if(heartbeat_.isValid()&&heartbeat_.elapsed()>leaseMs_){channel_->close("Shack PC stopped responding");return;}if(authenticated_)channel_->send({{"type","ping"}});pump();}
        const auto ids=deadlines_.keys();for(auto id:ids)if(deadlines_.value(id)<clock_.elapsed()){auto done=pending_.take(id);deadlines_.remove(id);if(done)done("Shack request timed out.");auto result=resultPending_.take(id);if(result)result("Shack request timed out.",{});}
    });tick_.start();retry_.setSingleShot(true);retry_.setInterval(2000);connect(&retry_,&QTimer::timeout,this,[this]{if(retryEnabled_)connectToHost(config_);});
}
Client::~Client(){disconnectFromHost();}
void Client::connectToHost(const Config& config){disconnectFromHost();config_=config;retryEnabled_=config.reconnect;const auto error=config.validate();if(!error.isEmpty()){status_=error;emit changed();return;}auto* socket=new QTcpSocket;attach(socket,config.key);status_="Connecting to shack PC · "+config.host;emit changed();socket->connectToHost(config.host,config.port);}
void Client::connectDevice(QIODevice* device,const QString& key){disconnectFromHost();retryEnabled_=false;attach(device,key);}
void Client::attach(QIODevice* device,const QString& key){
    config_.key=key;challengeReceived_=false;monitorId_=monitorSequence_=0;auto* channel=new Channel(device,this);channel_=channel;heartbeat_.start();pcmSequence_=0;
    connect(channel,&Channel::packet,this,[this](const QJsonObject& message,const QByteArray& data){handle(message,data);});
    connect(channel,&Channel::writable,this,[this]{QTimer::singleShot(0,this,&Client::pump);});
    connect(channel,&Channel::ended,this,[this,channel](const QString& reason){if(channel_==channel){channel_.clear();lost(reason);}channel->deleteLater();});
}
void Client::disconnectFromHost(){retryEnabled_=false;retry_.stop();if(channel_){if(authenticated_)channel_->send({{"type","stop"},{"id",double(++requestId_)}});if(channel_)channel_->close("Disconnected from shack PC");}else if(authenticated_||txActive_)lost("Disconnected from shack PC");}
void Client::lost(const QString& reason){const bool monitor=monitorId_!=0;monitorId_=monitorSequence_=0;authenticated_=false;voiceSupported_=false;const bool voice=voiceToken_!=0;voiceToken_=voiceSequence_=0;voiceReady_=false;upload_.close();uploadReady_=false;++txGeneration_;const bool wasTx=txActive_;txActive_=false;snapshot_={};auto pending=std::move(pending_);pending_.clear();auto results=std::move(resultPending_);resultPending_.clear();deadlines_.clear();status_=reason;for(auto done:pending)if(done)done(reason);for(auto done:results)if(done)done(reason,{});if(voice)emit voiceFinished(reason);if(wasTx)emit transmitFinished(false,reason);if(monitor)emit monitorFinished(false);emit snapshotChanged();emit changed();if(retryEnabled_)retry_.start();}
void Client::handle(const QJsonObject& message,const QByteArray& data){
    const auto type=message.value("type").toString();
    if(type=="challenge"&&!authenticated_&&!challengeReceived_){const auto nonce=message.value("nonce").toString().toLatin1();if(message.value("version").toInt()!=1||nonce.size()!=32){channel_->close("Incompatible remote station version");return;}challengeReceived_=true;if(channel_->send({{"type","auth"},{"version",1},{"proof",QString::fromLatin1(proof(nonce,config_.key))}})&&channel_)channel_->secure(sessionKey(nonce,config_.key));return;}
    if(type=="welcome"){
        if(!challengeReceived_||authenticated_){channel_->close("Shack PC did not authenticate the connection");return;}
        authenticated_=true;voiceSupported_=message.value("voice").toBool();heartbeat_.restart();status_="Connected to shack PC";snapshot_=message.value("snapshot").toObject();
        if(message.value("duplexMonitor").toBool()&&!channel_->send({{"type","capabilities"},{"duplexMonitor",true}}))return;
        emit snapshotChanged();emit changed();return;
    }
    if(type=="rejected"){status_=message.value("error").toString();retryEnabled_=false;channel_->close(status_);return;}
    if(!authenticated_){channel_->close("Unexpected pairing response");return;}
    if(type=="pong"){heartbeat_.restart();return;}
    if(type=="snapshot"||(type=="reply"&&message.value("snapshot").isObject())){const auto state=message.value("snapshot").toObject();if(state!=snapshot_){snapshot_=state;emit snapshotChanged();}}
    if(type=="pcm"){
        if(message.value("source")=="tx"){
            if(message.value("stream").toDouble(-1)!=monitorId_)return;
            if(message.value("rate").toInt()!=12000||message.value("offset").toDouble(-1)!=monitorSequence_){channel_->close("Shack transmit audio continuity was lost");return;}monitorSequence_+=data.size();emit monitorPcm(data);return;
        }
        if(message.value("rate").toInt()!=12000||message.value("offset").toDouble(-1)!=pcmSequence_){channel_->close("Remote audio continuity was lost; reconnecting");return;}pcmSequence_+=data.size();emit pcm(data);return;
    }
    if(type=="tx.monitor.begin"){monitorId_=qint64(message.value("stream").toDouble());monitorSequence_=0;emit monitorStarted();return;}
    if(type=="tx.monitor.end"){if(message.value("stream").toDouble(-1)==monitorId_){monitorId_=0;emit monitorFinished(message.value("success").toBool());}return;}
    if(type=="reply"){const auto id=qint64(message.value("id").toDouble());auto done=pending_.take(id);deadlines_.remove(id);if(done)done(message.value("error").toString());auto result=resultPending_.take(id);if(result)result(message.value("error").toString(),message.value("result").toObject());return;}
    if(type=="voice.finished"){
        if(voiceToken_&&message.value("stream").toDouble(-1)==voiceToken_){voiceToken_=voiceSequence_=0;voiceReady_=false;emit voiceFinished(message.value("message").toString());emit changed();}return;
    }
    if(type=="tx.finished"){if(!txActive_||message.value("tx").toDouble(-1)!=double(txGeneration_))return;txActive_=false;uploadReady_=false;upload_.close();++txGeneration_;emit transmitFinished(message.value("success").toBool(),message.value("message").toString());emit changed();}
}
void Client::command(const QString& operation,const QJsonObject& arguments,Done done){
    if(!authenticated_||!channel_){if(done)done("Connect to the shack PC first.");return;}if(pending_.size()+resultPending_.size()>=32){if(done)done("Too many pending shack requests.");return;}
    const auto id=++requestId_;pending_[id]=std::move(done);deadlines_[id]=clock_.elapsed()+10000;
    channel_->send({{"type","command"},{"id",double(id)},{"operation",operation},{"arguments",arguments}});
}
void Client::commandResult(const QString& operation,const QJsonObject& arguments,ResultDone done){
    if(!authenticated_||!channel_){if(done)done("Connect to the shack PC first.",{});return;}
    if(pending_.size()+resultPending_.size()>=32){if(done)done("Too many pending shack requests.",{});return;}
    const auto id=++requestId_;resultPending_[id]=std::move(done);deadlines_[id]=clock_.elapsed()+10000;
    channel_->send({{"type","command"},{"id",double(id)},{"operation",operation},{"arguments",arguments}});
}
bool Client::sendWave(const QString& path,qint64 morse,QString* error){
    const auto reject=[error](const QString& text){if(error)*error=text;return false;};
    if(!authenticated_||!channel_)return reject("Connect to the shack PC first.");
    if(txActive_||voiceActive())return reject("Remote transmission is already active.");
    try{waveDuration(path);}catch(const std::exception& e){return reject(QString::fromUtf8(e.what()));}
    upload_.setFileName(path);if(!upload_.open(QIODevice::ReadOnly))return reject(upload_.errorString());
    QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&upload_)||!upload_.seek(0)){upload_.close();return reject("Cannot read TX audio.");}
    uploadProfile_=snapshot_.value("profiles").toObject().value("active").toString();txActive_=true;uploadReady_=false;uploadPosition_=0;uploadMorse_=morse;const auto generation=++txGeneration_;const auto id=++requestId_;
    pending_[id]=[this,generation](QString error){if(generation!=txGeneration_)return;if(!error.isEmpty()){txActive_=false;upload_.close();emit transmitFinished(false,error);emit changed();return;}uploadReady_=true;pump();};deadlines_[id]=clock_.elapsed()+10000;
    if(!channel_->send({{"type","tx.begin"},{"profile",uploadProfile_},{"id",double(id)},{"tx",double(generation)},{"bytes",double(upload_.size())},{"sha256",QString::fromLatin1(hash.result().toHex())},{"morse",double(morse)}}))return reject(status_);
    emit changed();return true;
}
void Client::pump(){
    if(!authenticated_||!channel_||!uploadReady_||!upload_.isOpen())return;
    int chunks=0;
    while(channel_&&channel_->queuedBytes()<128*1024&&chunks++<4){
        const auto data=upload_.read(65536);if(data.isEmpty()){
            if(upload_.error()!=QFile::NoError){stopTransmit();emit transmitFinished(false,"Cannot read transmit audio.");return;}
            upload_.close();uploadReady_=false;const auto id=++requestId_;const auto generation=txGeneration_;
            pending_[id]=[this,generation](QString error){if(generation!=txGeneration_)return;if(!error.isEmpty()){txActive_=false;emit transmitFinished(false,error);emit changed();return;}emit transmitStarted(snapshot_.value("txDurationMs").toInteger(),uploadMorse_);};deadlines_[id]=clock_.elapsed()+10000;
            channel_->send({{"type","tx.commit"},{"profile",uploadProfile_},{"id",double(id)}});return;
        }
        if(!channel_->send({{"type","tx.chunk"},{"id",double(++requestId_)},{"offset",double(uploadPosition_)}},data))return;
        uploadPosition_+=data.size();
    }
    if(uploadReady_)QTimer::singleShot(10,this,&Client::pump);
}
void Client::startVoice(Done done){
    if(!authenticated_||!channel_||!voiceSupported_){if(done)done("Voice requires Tux-sstv or OpenSSTV 1.3 or later on both computers and a shack connection.");return;}
    if(voiceActive()||txActive_||pending_.size()+resultPending_.size()>=32){if(done)done("A transmission or request is already active.");return;}
    voiceToken_=++voiceGeneration_;voiceSequence_=0;voiceReady_=false;
    const auto token=voiceToken_,id=++requestId_;
    pending_[id]=[this,token,done=std::move(done)](QString error){
        if(voiceToken_!=token)return;
        if(!error.isEmpty()){stopVoice();if(done)done(error);return;}
        voiceReady_=true;if(done)done({});
    };deadlines_[id]=clock_.elapsed()+10000;
    channel_->send({{"type","voice.begin"},{"profile",snapshot_.value("profiles").toObject().value("active").toString()},{"id",double(id)},{"stream",double(token)}});emit changed();
}
void Client::sendVoicePcm(const QByteArray& pcm){
    if(!voiceReady_||!voiceToken_||!channel_||!authenticated_)return;
    if(pcm.isEmpty()||pcm.size()>9600||pcm.size()%2||channel_->queuedBytes()>48000){stopVoice();emit voiceFinished("Wi-Fi cannot keep up with voice audio.");return;}
    const auto offset=voiceSequence_;voiceSequence_+=pcm.size();
    channel_->send({{"type","voice.pcm"},{"stream",double(voiceToken_)},{"offset",double(offset)},{"rate",48000}},pcm);
}
void Client::stopVoice(){
    if(!voiceToken_)return;
    const auto token=voiceToken_;voiceToken_=voiceSequence_=0;voiceReady_=false;
    if(authenticated_&&channel_)channel_->send({{"type","voice.end"},{"id",double(++requestId_)},{"stream",double(token)}});
    emit changed();
}
void Client::stopTransmit(){
    stopVoice();
    uploadReady_=false;upload_.close();++txGeneration_;const bool active=txActive_;txActive_=false;
    if(authenticated_&&channel_)channel_->send({{"type","stop"},{"id",double(++requestId_)}});
    if(active)emit transmitFinished(false,"Remote transmission stopped");
    emit changed();
}
}
