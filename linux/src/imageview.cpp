#include "imageview.h"
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

ImageView::ImageView(const QString& text,QWidget* parent):QLabel(text,parent) {
    setAlignment(Qt::AlignCenter);
    setMinimumSize(320,256);
    setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    setStyleSheet("background:#101010;color:#b8b8b8;border:2px inset #888888;");
}
void ImageView::setImage(const QImage& image) { image_=image;update(); }
QRect ImageView::imageRect() const {
    if(image_.isNull())return {};
    const auto bounds=rect().adjusted(3,3,-3,-3);const auto size=image_.size().scaled(bounds.size(),Qt::KeepAspectRatio);
    QRect target(QPoint(0,0),size);target.moveCenter(bounds.center());return target;
}
void ImageView::paintEvent(QPaintEvent* event) {
    if(image_.isNull()) {QLabel::paintEvent(event);return;}
    QStyleOption option;option.initFrom(this);
    QPainter painter(this);style()->drawPrimitive(QStyle::PE_Widget,&option,&painter,this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.drawImage(imageRect(),image_);
}
