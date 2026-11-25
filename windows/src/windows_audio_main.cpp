// Native Windows PCM helper. Its pipes preserve the existing capture/playback
// lifecycle, cancellation and PTT watchdogs in the station application.
#include "core/pcm_converter.h"
#include "core/wav.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QStringList>
#include <QtEndian>
#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include <io.h>
#include <fcntl.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
[[noreturn]] void fail(const QString& text){throw std::runtime_error(text.toLocal8Bit().constData());}
void checked(MMRESULT result,const char* operation){
    if(result==MMSYSERR_NOERROR)return;
    wchar_t detail[MAXERRORLENGTH]{};waveOutGetErrorTextW(result,detail,MAXERRORLENGTH);
    fail(QString::fromLatin1(operation)+": "+QString::fromWCharArray(detail));
}
struct Device {UINT index;QString id,name;};
QList<Device> devices(bool capture){
    QList<Device> result;const UINT count=capture?waveInGetNumDevs():waveOutGetNumDevs();
    for(UINT index=0;index<count;++index){
        QString name;
        if(capture){WAVEINCAPSW caps{};if(waveInGetDevCapsW(index,&caps,sizeof caps))continue;name=QString::fromWCharArray(caps.szPname);}
        else{WAVEOUTCAPSW caps{};if(waveOutGetDevCapsW(index,&caps,sizeof caps))continue;name=QString::fromWCharArray(caps.szPname);}
        const auto message=[&](UINT command,DWORD_PTR first,DWORD_PTR second){return capture?waveInMessage(reinterpret_cast<HWAVEIN>(UINT_PTR(index)),command,first,second):waveOutMessage(reinterpret_cast<HWAVEOUT>(UINT_PTR(index)),command,first,second);};
        DWORD size=0;
        if(message(DRV_QUERYDEVICEINTERFACESIZE,reinterpret_cast<DWORD_PTR>(&size),0)!=MMSYSERR_NOERROR||size<sizeof(wchar_t)||size>65536)continue;
        std::vector<wchar_t> path(size/sizeof(wchar_t)+1);
        if(message(DRV_QUERYDEVICEINTERFACE,reinterpret_cast<DWORD_PTR>(path.data()),size)!=MMSYSERR_NOERROR)continue;
        const auto identity=QString::fromWCharArray(path.data()).toUtf8();if(identity.isEmpty())continue;
        result.append({index,"winmm:"+QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex()),name});
    }
    return result;
}
UINT selectDevice(bool capture,const QString& id){
    if(id=="default")return WAVE_MAPPER;
    for(const auto& device:devices(capture))if(device.id==id)return device.index;
    fail("The selected Windows audio device is unavailable. Reconnect it and refresh Setup. A different device was not selected automatically.");
}
WAVEFORMATEX format(bool capture,UINT device){
    for(DWORD rate:{48000UL,44100UL,96000UL,32000UL,16000UL,12000UL})for(WORD channels:{WORD(1),WORD(2)}){
        WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_PCM;f.nChannels=channels;f.nSamplesPerSec=rate;f.wBitsPerSample=16;f.nBlockAlign=WORD(channels*2);f.nAvgBytesPerSec=rate*f.nBlockAlign;
        const auto error=capture?waveInOpen(nullptr,device,&f,0,0,WAVE_FORMAT_QUERY):waveOutOpen(nullptr,device,&f,0,0,WAVE_FORMAT_QUERY);
        if(error==MMSYSERR_NOERROR)return f;
    }
    fail("This Windows audio device does not offer a supported 16-bit PCM format.");
}
struct Event {
    HANDLE handle=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    Event(){if(!handle)fail("Cannot create the Windows audio event.");}
    ~Event(){CloseHandle(handle);}
    void wait(){if(WaitForSingleObject(handle,2000)!=WAIT_OBJECT_0)fail("Windows audio stopped responding.");}
};
struct Buffer {WAVEHDR header{};QByteArray bytes;bool prepared=false;};
class Capture {
public:
    Event event;HWAVEIN handle=nullptr;std::array<Buffer,8> buffers;
    ~Capture(){if(handle){waveInReset(handle);for(auto& b:buffers)if(b.prepared)waveInUnprepareHeader(handle,&b.header,sizeof b.header);waveInClose(handle);}}
    void run(UINT device,int requestedRate){
        const auto f=format(true,device);audio::PcmConverter converter(int(f.nSamplesPerSec),f.nChannels,requestedRate);
        checked(waveInOpen(&handle,device,&f,reinterpret_cast<DWORD_PTR>(event.handle),0,CALLBACK_EVENT),"Open microphone/USB input (check Windows microphone privacy permission)");
        for(auto& b:buffers){b.bytes.resize(int(f.nSamplesPerSec/50*f.nBlockAlign));b.header.lpData=b.bytes.data();b.header.dwBufferLength=DWORD(b.bytes.size());checked(waveInPrepareHeader(handle,&b.header,sizeof b.header),"Prepare receive buffer");b.prepared=true;checked(waveInAddBuffer(handle,&b.header,sizeof b.header),"Queue receive buffer");}
        _setmode(_fileno(stdout),_O_BINARY);checked(waveInStart(handle),"Start audio input");
        size_t next=0;
        for(;;){
            event.wait();
            while(buffers[next].header.dwFlags&WHDR_DONE){
                auto& b=buffers[next];
                const auto output=converter.push(b.bytes.first(qsizetype(b.header.dwBytesRecorded)));
                qsizetype offset=0;while(offset<output.size()){const int count=_write(_fileno(stdout),output.constData()+offset,unsigned(output.size()-offset));if(count<=0)return;offset+=count;}
                b.header.dwBytesRecorded=0;checked(waveInAddBuffer(handle,&b.header,sizeof b.header),"Continue audio input");
                next=(next+1)%buffers.size();
            }
        }
    }
};
class Playback {
    Event event_;HWAVEOUT handle_=nullptr;std::array<Buffer,6> buffers_;size_t next_=0;QByteArray pending_;qsizetype blockBytes_=0;
    void submit(QByteArray bytes){
        auto& b=buffers_[next_];
        while(b.prepared&&!(b.header.dwFlags&WHDR_DONE))event_.wait();
        if(b.prepared){checked(waveOutUnprepareHeader(handle_,&b.header,sizeof b.header),"Release output buffer");b.prepared=false;}
        b.bytes=std::move(bytes);b.header={};b.header.lpData=b.bytes.data();b.header.dwBufferLength=DWORD(b.bytes.size());checked(waveOutPrepareHeader(handle_,&b.header,sizeof b.header),"Prepare output buffer");b.prepared=true;checked(waveOutWrite(handle_,&b.header,sizeof b.header),"Play audio");next_=(next_+1)%buffers_.size();
    }
    void push(const QByteArray& bytes){pending_+=bytes;while(pending_.size()>=blockBytes_){auto block=pending_.first(blockBytes_);pending_.remove(0,blockBytes_);submit(std::move(block));}}
public:
    ~Playback(){if(handle_){waveOutReset(handle_);for(auto& b:buffers_)if(b.prepared)waveOutUnprepareHeader(handle_,&b.header,sizeof b.header);waveOutClose(handle_);}}
    void run(UINT device,int requestedRate,const QString& file){
        sstv::Audio wav;if(!file.isEmpty()){wav=sstv::readWav(file,2);requestedRate=wav.sampleRate;}
        const auto f=format(false,device);audio::PcmConverter converter(requestedRate,1,int(f.nSamplesPerSec),f.nChannels);blockBytes_=f.nSamplesPerSec/50*f.nBlockAlign;
        checked(waveOutOpen(&handle_,device,&f,reinterpret_cast<DWORD_PTR>(event_.handle),0,CALLBACK_EVENT),"Open speakers/USB output");
        if(file.isEmpty()){
            _setmode(_fileno(stdin),_O_BINARY);std::array<char,4096> input;
            for(;;){const int count=_read(_fileno(stdin),input.data(),unsigned(input.size()));if(count<0)fail("Cannot read playback audio.");if(!count)break;push(converter.push(QByteArray(input.data(),count)));}
        }else{
            for(size_t start=0;start<wav.samples.size();start+=2048){const size_t count=std::min(size_t(2048),wav.samples.size()-start);QByteArray input(qsizetype(count*2),Qt::Uninitialized);for(size_t n=0;n<count;++n)qToLittleEndian<qint16>(qint16(std::lround(std::clamp(double(wav.samples[start+n]),-1.,32767./32768)*32768)),input.data()+n*2);push(converter.push(input));}
        }
        push(converter.finish());if(!pending_.isEmpty())submit(std::move(pending_));
        for(auto& b:buffers_)while(b.prepared&&!(b.header.dwFlags&WHDR_DONE))event_.wait();
    }
};
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        const auto args=app.arguments();if(args.size()<2||(args[1]!="--capture"&&args[1]!="--playback"))fail("Choose --capture or --playback.");
        const bool capture=args[1]=="--capture";QString selected="default",file;int rate=48000;
        if(args.contains("-L")){
            QByteArray listing="default\n    Windows default audio device\n";
            for(const auto& device:devices(capture))listing+=(device.id+"\n    "+device.name+"\n").toLocal8Bit();
            return std::fwrite(listing.constData(),1,size_t(listing.size()),stdout)==size_t(listing.size())?0:1;
        }
        for(qsizetype n=2;n<args.size();++n){
            const auto& arg=args[n];if(arg=="-q"||arg.startsWith("--buffer-time=")||arg.startsWith("--period-time="))continue;
            if(arg=="-D"||arg=="-r"||arg=="-t"||arg=="-f"||arg=="-c"){
                if(++n>=args.size())fail("Missing audio option value.");const auto& value=args[n];
                if(arg=="-D")selected=value;
                else if(arg=="-r"){bool valid=false;rate=value.toInt(&valid);if(!valid||rate<8000||rate>96000)fail("Unsupported sample rate.");}
                else if((arg=="-t"&&value!="raw")||(arg=="-f"&&value!="S16_LE")||(arg=="-c"&&value!="1"))fail("Only mono signed 16-bit PCM is accepted.");
            }else if(arg.startsWith('-')||!file.isEmpty()||capture)fail("Unrecognized audio option.");else file=arg;
        }
        if(capture){Capture input;input.run(selectDevice(true,selected),rate);}
        else{Playback output;output.run(selectDevice(false,selected),rate,file);}
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
