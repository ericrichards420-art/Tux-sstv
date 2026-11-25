#pragma once
#include <QImage>
#include <QLabel>

class ImageView : public QLabel {
public:
    explicit ImageView(const QString& text = {},QWidget* parent=nullptr);
    void setImage(const QImage& image);
    QRect imageRect() const;
    QSize imageSize() const{return image_.size();}
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QImage image_;
};
