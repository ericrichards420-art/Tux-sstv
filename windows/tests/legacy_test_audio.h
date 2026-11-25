#pragma once
#include "core/codec.h"
#include <QTemporaryDir>
inline QImage decodedReference(const QImage& image,const sstv::Mode& mode=*sstv::modeById("robot36")){
    QTemporaryDir directory;const auto path=directory.filePath("expected.wav");
    sstv::writeWav(path,sstv::encode(image,mode,48000));
    return sstv::decode(sstv::readWav(path)).image;
}
