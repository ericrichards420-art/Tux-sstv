#pragma once
#include "templates.h"
#include "composition.h"
namespace sstv {
struct TemplateLayout {
    QString id,name;
    TemplateFields fields; // Layout/style only; content is always blank.
    TextOverlay caption;
    QImage picture;
};
class LayoutStore {
public:
    explicit LayoutStore(QString directory):directory_(std::move(directory)){}
    QList<TemplateLayout> list(QString& error) const;
    bool save(const QString& name,const TemplateFields&,const TextOverlay&,const QImage&,QString& error) const;
private:
    QString directory_;
};
}
