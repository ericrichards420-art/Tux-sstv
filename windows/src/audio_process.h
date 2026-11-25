#pragma once
#include <QProcess>
#include <QStringList>

namespace audio {
// Linux keeps its existing ALSA helpers; Windows uses the bundled native helper.
void startCapture(QProcess& process,const QStringList& arguments);
void startPlayback(QProcess& process,const QStringList& arguments);
QString backendName();
QString bundledProgram(const QString& name);
}
