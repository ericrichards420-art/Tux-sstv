#include "signalview.h"
#include <QPainter>
#include <QPainterPath>
#include <QtEndian>
#include <algorithm>
#include <cmath>

SignalView::SignalView(QWidget* parent):QWidget(parent){setMinimumHeight(105);setAccessibleName("Receiver audio waterfall and spectrum");makeColors();}
void SignalView::makeColors(){
    const std::array<QColor,4> stops={background_,trace_.darker(260),trace_,QColor("#fff0b3")};
    for(size_t i=0;i<colors_.size();++i){const double at=i/255.*3;const int stop=std::min(2,int(at));const double mix=at-stop;const auto a=stops[size_t(stop)],b=stops[size_t(stop+1)];colors_[i]=qRgb(int(a.red()*(1-mix)+b.red()*mix),int(a.green()*(1-mix)+b.green()*mix),int(a.blue()*(1-mix)+b.blue()*mix));}
}
void SignalView::setColors(QColor background,QColor grid,QColor text,QColor trace,QColor warning){background_=background;grid_=grid;text_=text;trace_=trace;warning_=warning;makeColors();update();}
void SignalView::setWaterfall(bool enabled){waterfall_=enabled;update();}
void SignalView::setFrozen(bool frozen){
    if(frozen_==frozen)return;
    frozen_=frozen;if(frozen_){held_=history_;heldSpectrum_=preview_;}else{held_.clear();heldSpectrum_.reset();}emit freezeChanged(frozen_);update();
}
void SignalView::clearHistory(){analyzer_.reset();history_.clear();held_.clear();preview_.reset();heldSpectrum_.reset();oddByte_.clear();setProperty("inputSilent",false);update();}
void SignalView::beginInput(){setFrozen(false);clearHistory();active_=true;update();}
void SignalView::endInput(){active_=false;oddByte_.clear();update();}
void SignalView::pushPcm(const QByteArray& pcm,int rate){
    const auto bytes=oddByte_+pcm;const auto count=bytes.size()/2;oddByte_=bytes.mid(count*2);std::vector<float> samples(size_t(count),0);
    for(qsizetype i=0;i<count;++i)samples[size_t(i)]=qFromLittleEndian<qint16>(bytes.constData()+i*2)/32768.f;
    analyzer_.push(samples,rate,[this](const auto& frame){history_.push_back(frame);if(history_.size()>maxRows)history_.pop_front();});
    if(count){setProperty("inputSilent",std::all_of(samples.begin(),samples.end(),[](float value){return value==0;}));preview_.reset();update();}
}
void SignalView::setSamples(const std::vector<float>& samples,int rate){preview_=sstv::SpectrumAnalyzer::tail(samples,rate);update();}
void SignalView::paintEvent(QPaintEvent*){
    QPainter p(this);p.fillRect(rect(),background_);p.setRenderHint(QPainter::Antialiasing);QFont font("Sans Serif");font.setPixelSize(10);p.setFont(font);
    const QRect plot(39,23,std::max(1,width()-53),std::max(1,height()-46));const auto& history=frozen_?held_:history_;
    const auto& preview=frozen_?heldSpectrum_:preview_;const sstv::SpectrumFrame* latest=preview?&*preview:history.empty()?nullptr:&history.back();
    QString state=frozen_?"FROZEN":active_?(property("inputSilent").toBool()?"Input open · silence":"RX live"):history.empty()?"Input closed":"RX stopped · last audio";
    if(!waterfall_&&preview&&!frozen_)state="Audio preview";
    p.setPen(text_);p.drawText(QRect(5,1,width()-10,19),Qt::AlignLeft|Qt::AlignVCenter,state);
    p.setPen(latest&&latest->peak>=.99?warning_:text_);p.drawText(QRect(5,1,width()-10,19),Qt::AlignRight|Qt::AlignVCenter,latest&&latest->peak>=.99?"CLIPPING":waterfall_?"−90 → 0 dBFS":"dBFS");
    if(waterfall_){
        QImage raster(sstv::spectrumBins,plot.height(),QImage::Format_RGB32);raster.fill(background_);
        for(int y=0;y<raster.height();++y){
            const size_t first=size_t(y)*maxRows/size_t(raster.height()),last=std::max(first+1,size_t(y+1)*maxRows/size_t(raster.height()));
            if(first>=history.size())break;
            auto* line=reinterpret_cast<QRgb*>(raster.scanLine(y));
            for(int bin=0;bin<sstv::spectrumBins;++bin){float db=-90;for(size_t row=first;row<std::min(last,history.size());++row)db=std::max(db,history[history.size()-1-row].db[size_t(bin)]);line[bin]=colors_[size_t(std::clamp(int((db+90)/90*255),0,255))];}
        }
        p.drawImage(plot,raster);
        p.setPen(text_);for(int seconds:{0,10,20,30}){const double y=plot.top()+seconds/30.*(plot.height()-1);p.drawText(QRectF(0,y-7,34,15),Qt::AlignRight,QString::number(seconds)+" s");}
        if(history.empty()){p.setPen(text_);p.drawText(plot,Qt::AlignCenter,active_?"Waiting for input samples…":"Receiver stopped");}
    }else{
        for(int db=-80;db<=0;db+=20){const double y=plot.top()-db/90.*plot.height();p.setPen(grid_);p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));p.setPen(text_);p.drawText(QRectF(0,y-7,34,15),Qt::AlignRight,QString::number(db));}
        if(latest){QPainterPath path;for(int bin=0;bin<sstv::spectrumBins;++bin){const QPointF point(plot.left()+double(bin)/(sstv::spectrumBins-1)*(plot.width()-1),plot.bottom()-std::clamp((latest->db[size_t(bin)]+90)/90.,0.,1.)*(plot.height()-1));if(!bin)path.moveTo(point);else path.lineTo(point);}p.setPen(QPen(trace_,1.5));p.drawPath(path);}else{p.setPen(text_);p.drawText(plot,Qt::AlignCenter,"No audio");}
    }
    const QList<int> ticks=width()<440?QList<int>{500,1200,1900,2300,3000}:QList<int>{500,1200,1500,1900,2300,3000};
    for(int hz:ticks){const double x=plot.left()+(hz-sstv::spectrumLowHz)/(sstv::spectrumHighHz-sstv::spectrumLowHz)*(plot.width()-1);p.setPen(grid_);p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));p.setPen(text_);p.drawText(QRectF(std::clamp(x-20.,0.,std::max(0.,width()-40.)),plot.bottom()+3,40,18),Qt::AlignCenter,QString::number(hz));}
}
