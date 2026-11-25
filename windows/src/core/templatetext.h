#pragma once
#include "templates.h"
#include <QPainter>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace sstv::detail {
inline void templateText(QPainter& p,const TemplateFields& fields,const QString& key,QMap<QString,QRect>* bounds,QRectF box,const QString& value,int pixels,QColor color,bool bold,int alignment,int minimum,std::vector<std::function<void()>>* deferred){
    if(fields.textColor.isValid())color=fields.textColor;
    QFont font;font.setFamilies(QStringList{"DejaVu Sans"}+QFontDatabase::systemFont(QFontDatabase::GeneralFont).families());font.setBold(bold);
    for(;pixels>minimum;--pixels){font.setPixelSize(pixels);if(QFontMetricsF(font).horizontalAdvance(value)<=box.width()&&QFontMetricsF(font).height()<=box.height())break;}
    font.setPixelSize(pixels);p.setFont(font);p.setPen(color);const auto displayed=QFontMetricsF(font).elidedText(value,Qt::ElideRight,box.width());
    QRectF ink=displayed.trimmed().isEmpty()?QRectF{}:p.boundingRect(box,alignment,displayed).adjusted(-2,-2,2,2);QPointF delta;
    const auto position=fields.textPositions.constFind(key);
    if(!ink.isEmpty() && position!=fields.textPositions.cend() && std::isfinite(position->x())&&std::isfinite(position->y())){
        const QPointF topLeft(std::clamp(std::clamp(position->x(),0.,1.)*640.,4.,std::max(4.,636.-ink.width())),std::clamp(std::clamp(position->y(),0.,1.)*480.,4.,std::max(4.,476.-ink.height())));
        delta=topLeft-ink.topLeft();
    }
    if(bounds&&!ink.isEmpty())(*bounds)[key]=p.worldTransform().mapRect(ink.translated(delta)).toAlignedRect();
    const auto transform=p.worldTransform();
    const auto draw=[&p,font,color,transform,delta,box,alignment,displayed]{p.save();p.setWorldTransform(transform);p.setFont(font);p.setPen(color);p.translate(delta);p.drawText(box,alignment,displayed);p.restore();};
    if(deferred&&position!=fields.textPositions.cend())deferred->push_back(draw);else draw();
}
}
