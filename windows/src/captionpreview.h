#pragma once
#include "imageview.h"
#include <functional>
#include <QMap>
#include <QStringList>

class CaptionPreview:public ImageView {
    Q_OBJECT
public:
    explicit CaptionPreview(QWidget* parent=nullptr);
    void setCaptionBounds(const QRect& bounds);
    QRect captionBounds() const{return captionBounds_;}
    QRectF captionWidgetRect() const;
    std::function<void(QPointF)> captionMoved;
    void setTextRegions(const QMap<QString,QRect>& regions,const QStringList& topFirst={});
    void setPointerHidden(bool hidden);
    QRectF textWidgetRect(const QString& key) const;
    QString textKeyAt(QPointF position) const{return keyAt(position);}
    std::function<void(const QString&,QPointF)> textMoved;
signals:
    void editStarted(const QString& key);
    void editFinished();
protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    QRect captionBounds_;
    bool dragging_=false,pointerHidden_=false;
    Qt::CursorShape pointerShape_=Qt::ArrowCursor;
    void setPreviewCursor(Qt::CursorShape shape);
    QPointF grabOffset_,pressPosition_;
    bool moved_=false;
    QMap<QString,QRect> regions_;
    QStringList topFirst_;
    QString activeKey_;
    QPointF imagePoint(QPointF position) const;
    bool hit(QPointF position) const;
    QString keyAt(QPointF position) const;
    void moveCaption(QPointF position);
};
