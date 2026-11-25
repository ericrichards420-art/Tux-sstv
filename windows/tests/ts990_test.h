#pragma once
#include "radio.h"
#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>

class Ts990Emulator {
public:
    QTemporaryDir directory;
    QProcess process;
    radio::Config config;
    ~Ts990Emulator(){process.kill();process.waitForFinished(1000);}
    QString start(int input=3,const QString& method="RIG"){
        const auto script=QFINDTESTDATA("ts990_emulator.py");if(script.isEmpty())return "Missing TS-990 emulator";
        process.start("python3",{script,directory.path()});if(!process.waitForStarted(1000)||!process.waitForReadyRead(2000))return "Cannot start TS-990 emulator: "+process.errorString();
        const auto path=QString::fromUtf8(process.readLine()).trimmed();if(!path.startsWith("/dev/pts/"))return "Expected a private pseudo-terminal";
        config.enabled=true;config.model=2039;config.device=path;config.baud=115200;config.pttType=method;config.pttValue=input;config.leadMs=20;config.tailMs=20;return {};
    }
    QByteArray log(){QFile file(directory.filePath("cat.log"));return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};}
};
