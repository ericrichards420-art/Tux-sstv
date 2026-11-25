#pragma once
#include <QColor>
#include <QPalette>
#include <QString>
#include <vector>

namespace ui {
enum class SkinFinish { Flat, Neon, Glass, Metal };
struct Skin {
    QString id,name,description;
    QColor background,surface,raised,field,text,muted,border,accent,success,error,trace;
    int radius;
    bool monospace=false;
    QColor secondary={};
    SkinFinish finish=SkinFinish::Flat;
};
const std::vector<Skin>& skins();
const Skin& skinById(const QString& id);
QString skinStyleSheet(const Skin& skin);
QPalette skinPalette(const Skin& skin);
Skin radioSkin(const Skin& skin);
QString radioStyleSheet(const Skin& skin);
}
