#pragma once
#include <QIODevice>
#include <QPointer>
#include <QTimer>
#include <cstring>

// A reliable duplex byte stream. Delivery is queued, as with real socket events.
// Exercises sessions even in environments that prohibit AF_INET sockets.
class RemoteTestDevice:public QIODevice {
public:
    QPointer<RemoteTestDevice> peer;
    QByteArray sent;
    explicit RemoteTestDevice(){open(QIODevice::ReadWrite);}
    bool isSequential() const override{return true;}
    qint64 bytesAvailable() const override{return incoming_.size()+QIODevice::bytesAvailable();}
    void close() override{
        if(!isOpen())return;
        QIODevice::close();
        if(peer){QPointer<RemoteTestDevice> other=peer;QTimer::singleShot(0,peer,[other]{if(other)emit other->readChannelFinished();});}
    }
protected:
    qint64 readData(char* data,qint64 max) override{const auto size=std::min(max,qint64(incoming_.size()));std::memcpy(data,incoming_.constData(),size_t(size));incoming_.remove(0,size);return size;}
    qint64 writeData(const char* data,qint64 size) override{
        if(!peer||!peer->isOpen())return -1;
        sent.append(data,size);peer->incoming_.append(data,size);
        QPointer<RemoteTestDevice> other=peer,self=this;
        QTimer::singleShot(0,peer,[other,self,size]{if(other)emit other->readyRead();if(self)emit self->bytesWritten(size);});return size;
    }
private:
    QByteArray incoming_;
};
inline std::pair<RemoteTestDevice*,RemoteTestDevice*> remoteTestPair(){auto* first=new RemoteTestDevice;auto* second=new RemoteTestDevice;first->peer=second;second->peer=first;return {first,second};}
