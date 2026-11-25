#include "branding.h"
#include <QApplication>
#include <QResource>

static void ensureBrandResources(){static const bool ready=[] {Q_INIT_RESOURCE(skin_icons);return true;}();Q_UNUSED(ready);}
QPixmap branding::tux(int size){ensureBrandResources();return QIcon(":/branding/tux.svg").pixmap(QSize(size,size),qApp?qApp->devicePixelRatio():1.);}
QIcon branding::icon(){ensureBrandResources();return QIcon(":/branding/tux-sstv.svg");}
