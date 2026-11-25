#include "audio_process.h"
#include <QCoreApplication>
#include <QDir>

namespace audio {
QString bundledProgram(const QString& name){
#ifdef Q_OS_WIN
    return QDir(QCoreApplication::applicationDirPath()).filePath(name+".exe");
#else
    return name;
#endif
}
void startCapture(QProcess& process,const QStringList& arguments){
#ifdef Q_OS_WIN
    process.start(bundledProgram("tux-sstv-audio"),QStringList{"--capture"}+arguments);
#else
    process.start("arecord",arguments);
#endif
}
void startPlayback(QProcess& process,const QStringList& arguments){
#ifdef Q_OS_WIN
    process.start(bundledProgram("tux-sstv-audio"),QStringList{"--playback"}+arguments);
#else
    process.start("aplay",arguments);
#endif
}
QString backendName(){
#ifdef Q_OS_WIN
    return "Windows audio";
#else
    return "ALSA";
#endif
}
}
