#include "fskid.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sstv {
bool validCallsign(const QString& call) {
    static const QRegularExpression syntax("^[A-Z0-9]+(?:/[A-Z0-9]+)*$");
    return call.size()>=3 && call.size()<=12 && syntax.match(call).hasMatch() && call.contains(QRegularExpression("[A-Z]")) && call.contains(QRegularExpression("[0-9]"));
}
std::optional<FskId> detectFskId(const std::vector<float>& f,double beginMs,double offset,double clockPpm,Cancel cancel) {
    if(!std::isfinite(beginMs) || beginMs<0 || !std::isfinite(offset) || std::abs(offset)>200 || !std::isfinite(clockPpm) || std::abs(clockPpm)>10000)return {};
    const double scale=1+clockPpm*1e-6,bit=264*scale;
    const auto first=std::min(f.size(),size_t(beginMs*12)),last=std::min(f.size(),first+size_t(5*12000));
    auto near=[&](double position,double hz) {
        int good=0;
        for(int k=-4;k<=4;++k){const auto i=qint64(std::llround(position+k*6));if(i>=qint64(first) && i<qint64(last) && std::abs(f[size_t(i)]-offset-hz)<55)++good;}
        return good>=7;
    };
    for(size_t i=first+size_t(3500*scale);i+size_t(bit*19)<last;++i) {
        if((i&4095)==0 && cancel && cancel->load())throw std::runtime_error("Cancelled.");
        if(std::abs(f[i]-offset-1900)>45 || std::abs(f[i-1]-offset-1900)<=45)continue;
        // At the 2100-to-start-bit transition, require a 100 ms second
        // leader preceded by a sustained 1500 Hz (or narrow 1900 Hz) leader.
        if(!near(i-600*scale,2100) || !near(i-1080*scale,2100) || !near(i+bit*.5,1900))continue;
        bool leader=false;
        for(const double hz:{1500.,1900.})if(near(i-1800*scale,hz) && near(i-3000*scale,hz) && near(i-4200*scale,hz))leader=true;
        if(!leader)continue;
        double position=i+bit;bool good=true;
        auto symbol=[&]() {
            int value=0;
            for(int b=0;b<6;++b){const auto center=position+(b+.5)*bit;if(near(center,1900))value|=1<<b;else if(!near(center,2100))good=false;}
            position+=6*bit;return value;
        };
        int marker=symbol();if(marker==0x20)marker=symbol(); // optional initial idle symbol
        if(!good || marker!=0x2a)continue;
        QString call;int checksum=0;
        for(int count=0;count<=12 && good;++count) {
            const int value=symbol();if(!good)break;
            if(value==1){const int received=symbol();if(good && received==checksum && validCallsign(call))return FskId{call,position/12};break;}
            checksum^=value;call+=QChar(value+0x20);
        }
    }
    return {};
}
}
