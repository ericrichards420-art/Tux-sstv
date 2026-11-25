#include "wav.h"
#include <QFile>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sstv {
namespace {
[[noreturn]] void fail(const QString& why) { throw std::runtime_error(why.toStdString()); }
quint16 u16(const char* p) { return qFromLittleEndian<quint16>(p); }
quint32 u32(const char* p) { return qFromLittleEndian<quint32>(p); }
void put16(QByteArray& b,quint16 x) { char p[2]; qToLittleEndian(x,p); b.append(p,2); }
void put32(QByteArray& b,quint32 x) { char p[4]; qToLittleEndian(x,p); b.append(p,4); }
}
Audio readWav(const QString& path,int channel) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) fail(file.errorString());
    if (file.size()>240*1024*1024) fail("WAV is too large (240 MiB limit).");
    const auto data = file.readAll();
    if (data.size()<12 || data.first(4)!="RIFF" || data.mid(8,4)!="WAVE") fail("Expected a RIFF WAVE file.");
    const qint64 end = qint64(u32(data.constData()+4))+8;
    if (end>data.size() || end<12) fail("Truncated or invalid RIFF container.");
    int format=0, channels=0, rate=0, bits=0, align=0;
    QByteArray pcm;
    bool haveFormat=false,haveData=false;
    for (qint64 p=12;p+8<=end;) {
        const auto tag=data.mid(p,4);
        const qint64 size=u32(data.constData()+p+4);
        p+=8;
        if (size>end-p) fail("Truncated WAV chunk.");
        if (tag=="fmt ") {
            if (size<16 || haveFormat) fail("Invalid WAV format chunk.");
            haveFormat=true;
            format=u16(data.constData()+p); channels=u16(data.constData()+p+2);
            rate=int(u32(data.constData()+p+4)); align=u16(data.constData()+p+12); bits=u16(data.constData()+p+14);
        } else if (tag=="data") {
            if(haveData) fail("Multiple WAV data chunks are unsupported.");
            haveData=true; pcm=data.mid(p,size);
        }
        p+=size+(size&1);
        if(p>end) fail("Missing WAV chunk padding.");
    }
    if (!haveFormat || !haveData || format!=1 || bits!=16 || (channels!=1 && channels!=2) || rate<8000 || rate>96000 || align!=channels*2)
        fail("Use 16-bit PCM WAV, mono or stereo, at 8000–96000 Hz.");
    if(pcm.isEmpty() || pcm.size()%align) fail("Empty or incomplete PCM frames.");
    if(channel<0 || channel>2 || (channel==1 && channels==1)) fail("Selected audio channel is unavailable.");
    Audio audio; audio.sampleRate=rate;
    audio.samples.resize(size_t(pcm.size()/align));
    if(audio.seconds()>600) fail("Recording exceeds the ten-minute limit.");
    for(size_t i=0;i<audio.samples.size();++i) {
        const char* p=pcm.constData()+i*align;
        double v=qFromLittleEndian<qint16>(p);
        if(channels==2 && channel==1) v=qFromLittleEndian<qint16>(p+2);
        else if(channels==2 && channel==2) v=(v+qFromLittleEndian<qint16>(p+2))*.5;
        audio.samples[i]=float(v/32768);
    }
    return audio;
}
void writeWav(const QString& path,const Audio& audio) {
    if(audio.sampleRate<8000 || audio.sampleRate>96000 || audio.samples.empty() || audio.seconds()>600) fail("Invalid audio buffer.");
    QByteArray header("RIFF");
    const auto bytes=quint32(audio.samples.size()*2);
    put32(header,36+bytes); header+="WAVEfmt "; put32(header,16);
    put16(header,1); put16(header,1); put32(header,audio.sampleRate); put32(header,audio.sampleRate*2);
    put16(header,2); put16(header,16); header+="data"; put32(header,bytes);
    QSaveFile out(path);
    if(!out.open(QIODevice::WriteOnly)) fail(out.errorString());
    if(out.write(header)!=header.size()) fail(out.errorString());
    QByteArray block; block.reserve(65536);
    for(float sample:audio.samples) {
        if(!std::isfinite(sample)) fail("Non-finite audio sample.");
        put16(block,quint16(qint16(std::lround(std::clamp(double(sample),-1.,1.)*32767))));
        if(block.size()==65536) {
            if(out.write(block)!=block.size()) fail(out.errorString());
            block.clear();
        }
    }
    if(out.write(block)!=block.size() || !out.commit()) fail(out.errorString());
}
}
