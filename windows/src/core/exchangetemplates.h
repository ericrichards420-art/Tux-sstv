#pragma once
#include "templates.h"
class QPainter;
namespace sstv::detail {
void drawExchangeTemplate(QPainter&,const TemplateFields&,const QString& callsign,const QImage& picture,QMap<QString,QRect>* textBounds,bool showCallsign);
}
