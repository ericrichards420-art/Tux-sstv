#pragma once
#include "codec.h"
#include <QColor>
#include <QPointF>
#include <QRect>
#include <QList>
#include <QMap>
#include <QStringList>

namespace sstv {
struct TextOverlay {
    QString text;
    QColor color=Qt::white;
    int sizePercent=7;
    int position=7; // Nine anchors: top/center/bottom, then left/center/right.
    bool bold=true,outline=true,backing=false;
    bool freePosition=false;
    QPointF relativePosition; // Caption's top-left as a fraction of image dimensions.
    bool operator==(const TextOverlay&) const=default;
    qint64 stackOrder=0; // Current-picture stacking; later entries break ties.
};
struct CallsignPlacement {
    bool freePosition=false;
    QPointF relativePosition; // Top-left of the outlined callsign, in image fractions.
    bool operator==(const CallsignPlacement&) const=default;
};
struct ImageInset {
    QImage image; // Frozen received pixels, kept separately from the TX background.
    QPointF relativePosition={.62,.62};
    double widthFraction=.35;
    bool frame=true;
    bool operator==(const ImageInset&) const=default;
};
struct Composition {
    QImage image;
    bool reduced=false;
    QRect callsignBounds; // Tight bounds of the station callsign including its outline.
    QRect captionBounds; // Draggable text block in final image coordinates.
    QRect placementBounds;
    QRect insetBounds;
    QList<QRect> captionRects; // One entry per input layer, including empty layers.
    QMap<QString,QRect> captionRegions() const;
};
Composition composeTransmitImage(const QImage& source,const Mode& mode,const QString& callsign,const TextOverlay& overlay={},const CallsignPlacement& callsignPlacement={},const ImageInset& inset={});
Composition composeTransmitLayers(const QImage& source,const Mode& mode,const QString& callsign,const QList<TextOverlay>& overlays,const CallsignPlacement& callsignPlacement={},const ImageInset& inset={});
QString captionKey(int index);
int captionIndex(const QString& key);
QStringList captionHitOrder(const QList<TextOverlay>& overlays);
void bringCaptionToFront(QList<TextOverlay>& overlays,int index);
}
