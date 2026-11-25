#include "emojis.h"
#include "composition.h"
#include <QAbstractTextDocumentLayout>
#include <QFontDatabase>
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QCache>
#include <QDataStream>
#include <algorithm>
#include <cmath>

namespace sstv {
namespace {
constexpr int textScale=4;
struct CaptionRaster {QImage fill,stroke;QRect crop;bool reduced=false;};
QRect inkBounds(const QImage& layer){
    int left=layer.width(),right=-1,top=layer.height(),bottom=-1;
    for(int y=0;y<layer.height();++y){const auto* row=reinterpret_cast<const QRgb*>(layer.constScanLine(y));for(int x=0;x<layer.width();++x)if(qAlpha(row[x])){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}
    return right<left?QRect{}:QRect(QPoint(left,top),QPoint(right,bottom));
}
QImage outlineMask(const QImage& layer,int radius) {
    const int w=layer.width(),h=layer.height();std::vector<uchar> horizontal(size_t(w)*h);
    for(int y=0;y<h;++y){const auto* row=reinterpret_cast<const QRgb*>(layer.constScanLine(y));for(int x=0;x<w;++x) {
        int alpha=0;for(int dx=std::max(0,x-radius);dx<=std::min(w-1,x+radius);++dx)alpha=std::max(alpha,qAlpha(row[dx]));
        horizontal[size_t(y)*w+x]=uchar(alpha);
    }
    }
    QImage mask(layer.size(),QImage::Format_ARGB32_Premultiplied);mask.fill(Qt::transparent);
    for(int y=0;y<h;++y){auto* row=reinterpret_cast<QRgb*>(mask.scanLine(y));for(int x=0;x<w;++x) {
        int alpha=0;for(int dy=std::max(0,y-radius);dy<=std::min(h-1,y+radius);++dy)alpha=std::max(alpha,int(horizontal[size_t(dy)*w+x]));
        row[x]=qRgba(0,0,0,alpha);
    }
    }
    return mask;
}
}
QString captionKey(int index){return index==0?QStringLiteral("caption"):QString("caption:%1").arg(index);}
int captionIndex(const QString& key){
    if(key=="caption")return 0;
    if(!key.startsWith("caption:"))return -1;
    bool ok=false;const int index=key.mid(8).toInt(&ok);return ok&&index>0&&captionKey(index)==key?index:-1;
}
namespace {
QList<int> captionDrawOrder(const QList<TextOverlay>& overlays){
    QList<int> order;for(int i=0;i<overlays.size();++i)order<<i;
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return overlays[a].stackOrder<overlays[b].stackOrder;});return order;
}
}
QStringList captionHitOrder(const QList<TextOverlay>& overlays){
    const auto drawing=captionDrawOrder(overlays);QStringList order;for(auto it=drawing.crbegin();it!=drawing.crend();++it)order<<captionKey(*it);return order;
}
void bringCaptionToFront(QList<TextOverlay>& overlays,int index){
    if(index<0||index>=overlays.size())return;
    const auto order=captionDrawOrder(overlays);if(order.last()!=index)overlays[index].stackOrder=overlays[order.last()].stackOrder+1;
}
QMap<QString,QRect> Composition::captionRegions() const {
    QMap<QString,QRect> regions;for(qsizetype i=0;i<captionRects.size();++i)if(!captionRects[i].isEmpty())regions[captionKey(int(i))]=captionRects[i];return regions;
}
Composition composeTransmitImage(const QImage& source,const Mode& mode,const QString& callsign,const TextOverlay& overlay,const CallsignPlacement& callsignPlacement,const ImageInset& inset) {
    return composeTransmitLayers(source,mode,callsign,{overlay},callsignPlacement,inset);
}
Composition composeTransmitLayers(const QImage& source,const Mode& mode,const QString& callsign,const QList<TextOverlay>& overlays,const CallsignPlacement& callsignPlacement,const ImageInset& inset) {
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    static const bool emojiFont=[] {QFontDatabase::setApplicationEmojiFontFamilies({"Noto Color Emoji"});return true;}();
    Q_UNUSED(emojiFont);
#endif
    // Position changes do not change the fitted photo. QImage's cache key also
    // changes when source pixels are edited, so a changed photo cannot go stale.
    QByteArray backgroundKey;{QDataStream key(&backgroundKey,QIODevice::WriteOnly);key<<source.cacheKey()<<mode.width<<mode.height<<(mode.family==Family::Mono);}
    static thread_local QCache<QByteArray,QImage> backgrounds(16*1024);
    Composition result;
    if(const auto* cached=backgrounds.object(backgroundKey))result.image=*cached;
    else {result.image=fitImage(source,mode);backgrounds.insert(backgroundKey,new QImage(result.image),int(result.image.sizeInBytes()/1024)+1);}
    if(!inset.image.isNull()){
        const double fraction=std::isfinite(inset.widthFraction)?std::clamp(inset.widthFraction,.1,1.):.35;
        const int width=std::clamp(int(std::lround(mode.width*fraction)),1,mode.width);
        const auto size=inset.image.size().scaled({width,mode.height},Qt::KeepAspectRatio).expandedTo({1,1});
        const double px=std::isfinite(inset.relativePosition.x())?std::clamp(inset.relativePosition.x(),0.,1.):.62;
        const double py=std::isfinite(inset.relativePosition.y())?std::clamp(inset.relativePosition.y(),0.,1.):.62;
        const int x=std::clamp(int(std::lround(px*mode.width)),0,mode.width-size.width()),y=std::clamp(int(std::lround(py*mode.height)),0,mode.height-size.height());
        result.insetBounds=QRect(QPoint(x,y),size);
        QPainter p(&result.image);p.setRenderHint(QPainter::SmoothPixmapTransform);
        auto content=result.insetBounds;
        if(inset.frame&&std::min(size.width(),size.height())>4){
            const int border=std::min(std::max(1,int(std::lround(mode.width/320.))),(std::min(size.width(),size.height())-1)/2);p.fillRect(content,Qt::white);content.adjust(border,border,-border,-border);p.fillRect(content,Qt::black);
        }
        const auto fitted=inset.image.size().scaled(content.size(),Qt::KeepAspectRatio).expandedTo({1,1});QRect target(QPoint(0,0),fitted);target.moveCenter(content.center());p.drawImage(target,inset.image);
    }
    int callHeight=0,callRadius=1;
    QImage letters,callOutline;QRect callCrop;QPoint callOrigin;
    if(!callsign.trimmed().isEmpty()) {
        QFont font=QFontDatabase::systemFont(QFontDatabase::FixedFont);font.setBold(true);font.setPixelSize(std::max(12,mode.width/20));
        font.setHintingPreference(QFont::PreferFullHinting);font.setStyleStrategy(QFont::PreferAntialias);
        callHeight=font.pixelSize()+8;
        QByteArray callKey;{QDataStream key(&callKey,QIODevice::WriteOnly);key<<callsign.trimmed().toUpper()<<mode.width<<font.toString();}
        static thread_local QCache<QByteArray,CaptionRaster> callsigns(4*1024);
        CaptionRaster raster;
        if(const auto* cached=callsigns.object(callKey))raster=*cached;
        else {
        const QSize nativeSize(mode.width,callHeight);QImage high(nativeSize*textScale,QImage::Format_ARGB32_Premultiplied);high.fill(Qt::transparent);
        {
            auto highFont=font;highFont.setPixelSize(font.pixelSize()*textScale);
            QPainter p(&high);p.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);p.setFont(highFont);p.setPen(Qt::white);
            p.drawText(QRect(6*textScale,2*textScale,(mode.width-12)*textScale,(font.pixelSize()+4)*textScale),Qt::AlignLeft|Qt::AlignVCenter,callsign.trimmed().toUpper());
        }
        callRadius=std::max(1,int(std::lround(mode.width/320.)));
        raster.fill=high.scaled(nativeSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        raster.stroke=outlineMask(high,callRadius*textScale).scaled(nativeSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        const auto ink=inkBounds(raster.fill);
        raster.crop=ink.isEmpty()?QRect{}:ink.adjusted(-callRadius,-callRadius,callRadius,callRadius).intersected(raster.fill.rect());
        callsigns.insert(callKey,new CaptionRaster(raster),int((raster.fill.sizeInBytes()+raster.stroke.sizeInBytes())/1024)+1);
        }
        letters=raster.fill;callOutline=raster.stroke;callCrop=raster.crop;
        if(!callCrop.isEmpty()) {
            const auto position=callsignPlacement.relativePosition;
            if(callsignPlacement.freePosition && std::isfinite(position.x()) && std::isfinite(position.y())) {
                const int x=std::clamp(int(std::lround(std::clamp(position.x(),0.,1.)*mode.width)),0,mode.width-callCrop.width());
                const int y=std::clamp(int(std::lround(std::clamp(position.y(),0.,1.)*mode.height)),0,mode.height-callCrop.height());
                callOrigin=QPoint(x,y)-callCrop.topLeft();
            }
            result.callsignBounds=callCrop.translated(callOrigin);
        }
    }
    if(!callCrop.isEmpty()) {
        QPainter p(&result.image);p.drawImage(result.callsignBounds,callOutline,callCrop);p.drawImage(result.callsignBounds,letters,callCrop);
    }
    result.captionRects.resize(overlays.size());
    for(const int index:captionDrawOrder(overlays)){
      const auto& overlay=overlays[index];
      QRect captionBounds,placementBounds;
      if(!overlay.text.trimmed().isEmpty()) {
        const int margin=std::max(3,mode.width/40),radius=std::max(1,int(std::lround(mode.width/320.))),padding=radius+1;
        const int width=mode.width-2*margin,top=margin,available=mode.height-top-margin;
        placementBounds=result.image.rect();
        const int desired=std::max(4,int(std::lround(mode.width*std::clamp(overlay.sizePercent,3,18)/100.)));
        const int anchor=std::clamp(overlay.position,0,8);
        const auto textFamilies=QStringList{"Noto Sans","DejaVu Sans"}+QFontDatabase::systemFont(QFontDatabase::GeneralFont).families();
        QByteArray rasterKey;QDataStream key(&rasterKey,QIODevice::WriteOnly);
        key<<overlay.text<<quint64(overlay.color.rgba64())<<overlay.bold<<overlay.outline<<width<<available<<padding<<radius<<desired<<anchor%3<<textFamilies<<QFont().toString();
        // Keep appearance independent of font-cache warming by other editors.
        // Cached rasters also avoid rebuilding glyphs/outlines on every drag.
        static thread_local QCache<QByteArray,CaptionRaster> rasters(32*1024);
        CaptionRaster raster;
        if(const auto* cached=rasters.object(rasterKey))raster=*cached;
        else {
        // Lay out against an image device too: screen/font-cache metrics can
        // otherwise change antialiased glyphs between preview and recomposition.
        QImage textDevice(1,1,QImage::Format_ARGB32_Premultiplied);
        QTextDocument doc;doc.documentLayout()->setPaintDevice(&textDevice);doc.setDocumentMargin(0);doc.setPlainText(overlay.text);
        QTextOption option=ui::emojiTextOption();option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        option.setAlignment(anchor%3==0?Qt::AlignLeft:anchor%3==2?Qt::AlignRight:Qt::AlignHCenter);doc.setDefaultTextOption(option);
        QTextCursor cursor(&doc);cursor.select(QTextCursor::Document);QTextCharFormat format;format.setForeground(overlay.color.isValid()?overlay.color:QColor(Qt::white));cursor.mergeCharFormat(format);ui::formatEmojiText(doc);
        int pixels=desired;
        for(;pixels>=1;--pixels) {
            QFont font;font.setFamilies(textFamilies);font.setPixelSize(pixels*textScale);font.setBold(overlay.bold);
            font.setHintingPreference(QFont::PreferFullHinting);font.setStyleStrategy(QFont::PreferAntialias);doc.setDefaultFont(font);
            doc.setTextWidth((width-2*padding)*textScale);
            if(doc.size().height()/textScale+2*padding<=available)break;
        }
        raster.reduced=pixels<desired;
        const int height=std::min(available,int(std::ceil(doc.size().height()/textScale))+2*padding);
        const QSize nativeSize(width,height);QImage high(nativeSize*textScale,QImage::Format_ARGB32_Premultiplied);high.fill(Qt::transparent);
        {QPainter p(&high);p.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);p.translate(padding*textScale,padding*textScale);doc.drawContents(&p);}
        raster.fill=high.scaled(nativeSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        raster.stroke=overlay.outline?outlineMask(high,radius*textScale).scaled(nativeSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation):QImage{};
        const auto ink=inkBounds(raster.fill);
        raster.crop=ink.isEmpty()?QRect{}:ink.adjusted(-padding,-padding,padding,padding).intersected(raster.fill.rect());
        const int cost=int((raster.fill.sizeInBytes()+raster.stroke.sizeInBytes())/1024)+1;
        rasters.insert(rasterKey,new CaptionRaster(raster),cost);
        }
        result.reduced=result.reduced||raster.reduced;
        const auto& layer=raster.fill;const auto& stroke=raster.stroke;const int height=layer.height();
        const int y=anchor/3==0?top:anchor/3==1?top+(available-height)/2:mode.height-margin-height;
        const auto crop=raster.crop;
        const bool free=overlay.freePosition && std::isfinite(overlay.relativePosition.x()) && std::isfinite(overlay.relativePosition.y()) && !crop.isEmpty();
        QPainter painter(&result.image);
        if(free){
            const int x=std::clamp(int(std::lround(std::clamp(overlay.relativePosition.x(),0.,1.)*mode.width)),0,mode.width-crop.width());
            const int placedY=std::clamp(int(std::lround(std::clamp(overlay.relativePosition.y(),0.,1.)*mode.height)),0,mode.height-crop.height());
            captionBounds=QRect(QPoint(x,placedY),crop.size());
            if(overlay.backing)painter.fillRect(captionBounds,QColor(0,0,0,170));
            if(overlay.outline)painter.drawImage(captionBounds,stroke,crop);
            painter.drawImage(captionBounds,layer,crop);
        }else{
            if(!crop.isEmpty())captionBounds=crop.translated(margin,y);
            if(overlay.backing)painter.fillRect(QRect(margin-2,y-2,width+4,height+4),QColor(0,0,0,170));
            if(overlay.outline)painter.drawImage(margin,y,stroke);
            painter.drawImage(margin,y,layer);
        }
      }
      if(index==0){result.captionBounds=captionBounds;result.placementBounds=placementBounds;}
      result.captionRects[index]=captionBounds;
    }
    // Grayscale modes must preview the same final pixels that the encoder sends.
    if(mode.family==Family::Mono)result.image=fitImage(result.image,mode);
    return result;
}
}
