#include "captionpreview.h"
#include <QMouseEvent>
#include <QPainter>

CaptionPreview::CaptionPreview(QWidget* parent):ImageView({},parent){setMouseTracking(true);setToolTip("Drag the text on this preview to position it.");}
void CaptionPreview::setCaptionBounds(const QRect& bounds){captionBounds_=bounds;setTextRegions(bounds.isEmpty()?QMap<QString,QRect>{}:QMap<QString,QRect>{{"caption",bounds}});if(!bounds.isEmpty())activeKey_="caption";}
void CaptionPreview::setTextRegions(const QMap<QString,QRect>& regions,const QStringList& topFirst){if(regions_==regions&&topFirst_==topFirst)return;regions_=regions;topFirst_=topFirst;captionBounds_=regions.value("caption");if(!regions_.contains(activeKey_)){const bool wasDragging=dragging_;activeKey_.clear();dragging_=false;setPreviewCursor(Qt::ArrowCursor);if(wasDragging)emit editFinished();}update();}
QRectF CaptionPreview::captionWidgetRect() const{
    return textWidgetRect("caption");
}
QRectF CaptionPreview::textWidgetRect(const QString& key) const{
    const auto target=imageRect();const auto size=imageSize();if(target.isEmpty()||size.isEmpty())return {};
    const auto region=regions_.value(key);if(region.isEmpty())return {};
    const double sx=double(target.width())/size.width(),sy=double(target.height())/size.height();
    return {target.x()+region.x()*sx,target.y()+region.y()*sy,region.width()*sx,region.height()*sy};
}
QPointF CaptionPreview::imagePoint(QPointF point) const{
    const auto target=imageRect();if(target.isEmpty())return {};
    return {(point.x()-target.x())*imageSize().width()/target.width(),(point.y()-target.y())*imageSize().height()/target.height()};
}
QString CaptionPreview::keyAt(QPointF position) const{
    if(!imageRect().contains(position.toPoint()))return {};
    for(const auto& key:topFirst_)if(regions_.contains(key)&&textWidgetRect(key).contains(position))return key;
    QString result;int area=0;
    for(auto it=regions_.cbegin();it!=regions_.cend();++it)if(textWidgetRect(it.key()).adjusted(-5,-5,5,5).contains(position)){
        const int candidate=it.value().width()*it.value().height();if(result.isEmpty()||candidate<area){result=it.key();area=candidate;}
    }return result;
}
bool CaptionPreview::hit(QPointF position) const{return !keyAt(position).isEmpty();}
void CaptionPreview::paintEvent(QPaintEvent* event){
    ImageView::paintEvent(event);if(pointerHidden_)return;const auto active=textWidgetRect(activeKey_);if(active.isEmpty())return;
    QPainter p(this);p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(0,0,0,180),3));p.drawRect(active.adjusted(-2,-2,2,2));p.setPen(QPen(QColor("#64dcff"),1,Qt::DashLine));p.drawRect(active.adjusted(-2,-2,2,2));
}
void CaptionPreview::mousePressEvent(QMouseEvent* event){
    const auto key=keyAt(event->position());
    if(event->button()==Qt::LeftButton&&!key.isEmpty()){activeKey_=key;dragging_=true;moved_=false;pressPosition_=event->position();grabOffset_=imagePoint(event->position())-regions_[key].topLeft();setPreviewCursor(Qt::ClosedHandCursor);emit editStarted(key);event->accept();return;}
    ImageView::mousePressEvent(event);
}
void CaptionPreview::moveCaption(QPointF position){
    if(imageSize().isEmpty())return;
    const auto point=imagePoint(position)-grabOffset_;const QPointF relative(point.x()/imageSize().width(),point.y()/imageSize().height());
    if(textMoved)textMoved(activeKey_,relative);else if(captionMoved)captionMoved(relative);
}
void CaptionPreview::mouseMoveEvent(QMouseEvent* event){
    if(dragging_){moved_=moved_||event->position()!=pressPosition_;if(moved_)moveCaption(event->position());event->accept();return;}
    activeKey_=keyAt(event->position());setPreviewCursor(activeKey_.isEmpty()?Qt::ArrowCursor:Qt::OpenHandCursor);update();ImageView::mouseMoveEvent(event);
}
void CaptionPreview::mouseReleaseEvent(QMouseEvent* event){
    if(event->button()==Qt::LeftButton&&dragging_){if(moved_||event->position()!=pressPosition_)moveCaption(event->position());dragging_=false;emit editFinished();setPreviewCursor(hit(event->position())?Qt::OpenHandCursor:Qt::ArrowCursor);event->accept();return;}
    ImageView::mouseReleaseEvent(event);
}
void CaptionPreview::leaveEvent(QEvent* event){if(!dragging_)setPreviewCursor(Qt::ArrowCursor);ImageView::leaveEvent(event);}

void CaptionPreview::setPointerHidden(bool hidden){pointerHidden_=hidden;QWidget::setCursor(pointerHidden_?Qt::BlankCursor:pointerShape_);update();}
void CaptionPreview::setPreviewCursor(Qt::CursorShape shape){pointerShape_=shape;QWidget::setCursor(pointerHidden_?Qt::BlankCursor:pointerShape_);}

bool CaptionPreview::event(QEvent* event){
    if((event->type()==QEvent::Hide||event->type()==QEvent::UngrabMouse)&&dragging_){dragging_=false;setPreviewCursor(Qt::ArrowCursor);emit editFinished();}
    return ImageView::event(event);
}
