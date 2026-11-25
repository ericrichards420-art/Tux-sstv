#include "mainwindow.h"
#include "branding.h"
#include "core/patterns.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <algorithm>

int main(int argc,char** argv) {
    bool cli=false;
    for(int i=1;i<argc;++i) if(QByteArray(argv[i])=="--list-patterns" || QByteArray(argv[i])=="--list-modes" || QByteArray(argv[i])=="--encode" || QByteArray(argv[i])=="--decode" || QByteArray(argv[i])=="--pattern")cli=true;
    if(cli && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    app.setApplicationName(branding::StorageName);app.setOrganizationName(branding::StorageName);app.setApplicationDisplayName(branding::Name);app.setApplicationVersion(TUXSSTV_VERSION);
    app.setWindowIcon(branding::icon());
    app.setDesktopFileName(branding::DesktopId);
    QCommandLineParser p;p.setApplicationDescription("Tux-sstv Final Release — send and receive SSTV pictures over amateur radio");p.addHelpOption();p.addVersionOption();
    p.addOptions({
        {"list-modes","Print the complete compatibility catalog as JSON."},
        {"encode","Encode an image to PCM WAV.","image"},
        {"decode","Decode a PCM WAV to PNG.","wav"},
        {"pattern","Write a built-in test pattern to PNG.","png"},
        {"list-patterns","List built-in classic test patterns as JSON."},
        {"pattern-id","Pattern ID for --pattern (see --list-patterns).","id","test-pattern"},
        {"mode","Mode ID or name. Required for encode; defaults to auto for decode.","id"},
        {"output","Output filename.","path"},
        {"rate","Transmit sample rate.","hz","48000"},
        {"start-ms","Manual image start in WAV (requires --mode).","ms","-1"},
        {"auto-clock","Automatically measure legacy receive clock drift."},
        {"auto-afc","Automatically measure legacy receive tone offset."},
        {"clock-ppm","RX sample clock correction.","ppm","0"},
        {"offset-hz","Received audio tone offset.","hz","0"},
        {"channel","WAV input channel: left, right, or mix.","channel","left"},
        {"partial","Allow incomplete received images."},
        {"screenshot","Save the application window and exit (UI check).","png"},
        {"smoke-test","Open the window briefly and exit (UI check)."}
    });
    // Qt's standard help/version/error paths use applicationName. No profile
    // access happens until the legacy storage identity is restored below.
    app.setApplicationName(branding::Name);p.process(app);app.setApplicationName(branding::StorageName);
    QTextStream out(stdout),err(stderr);
    try {
        const int commands=int(p.isSet("list-patterns"))+int(p.isSet("list-modes"))+int(p.isSet("encode"))+int(p.isSet("decode"))+int(p.isSet("pattern"));
        if(commands>1)throw std::runtime_error("Choose one of --list-modes, --list-patterns, --encode, --decode, --pattern.");
        if(p.isSet("pattern-id") && !p.isSet("pattern"))throw std::runtime_error("--pattern-id requires --pattern.");
        if(p.isSet("list-patterns")) {
            QJsonArray patterns;for(const auto& pattern:sstv::testPatterns())patterns.append(QJsonObject{{"id",pattern.id},{"name",pattern.name},{"era",pattern.era},{"description",pattern.description},{"monochrome",pattern.monochrome}});
            out<<QJsonDocument(patterns).toJson();return 0;
        }
        if(p.isSet("list-modes")) {
            QJsonArray a;
            for(const auto& m:sstv::modes()) {
                auto entry=QJsonObject{{"id",m.id},{"name",m.name},{"family",sstv::familyName(m.family)},{"width",m.width},{"height",m.height},{"vis",m.vis<0?QJsonValue(QJsonValue::Null):QJsonValue(m.vis)},{"header",m.narrow()?"N-VIS":m.vis>127?"Extended VIS":"VIS"},{"implemented",m.implemented},{"interop_verified",false},{"image_seconds",m.implemented?QJsonValue(m.imageMs()/1000):QJsonValue(QJsonValue::Null)},{"note",m.note}};
                a.append(entry);
            }
            out<<QJsonDocument(a).toJson();return 0;
        }
        if(p.isSet("pattern")){const auto image=sstv::renderTestPattern(p.value("pattern-id"));if(image.isNull())throw std::runtime_error("Unknown test pattern ID; see --list-patterns.");if(!image.save(p.value("pattern"),"PNG"))throw std::runtime_error("Cannot save test pattern.");return 0;}
        if(p.isSet("encode") || p.isSet("decode")) {
            if(!p.isSet("output"))throw std::runtime_error("Provide --output.");
            bool valid=false;
            if(p.isSet("encode")) {
                const auto* m=sstv::modeById(p.value("mode"));if(!m)throw std::runtime_error("Provide a known --mode; see --list-modes.");
                const int rate=p.value("rate").toInt(&valid);if(!valid)throw std::runtime_error("Invalid --rate.");
                auto audio=sstv::encode(QImage(p.value("encode")),*m,rate);
                sstv::writeWav(p.value("output"),audio);out<<m->name<<": "<<audio.seconds()<<" seconds written\n";
            } else {
                sstv::DecodeOptions options;options.forceMode=p.value("mode");options.autoClock=p.isSet("auto-clock");options.autoFrequency=p.isSet("auto-afc");
                options.manualStartMs=p.value("start-ms").toDouble(&valid);if(!valid)throw std::runtime_error("Invalid --start-ms.");
                options.clockPpm=p.value("clock-ppm").toDouble(&valid);if(!valid)throw std::runtime_error("Invalid --clock-ppm.");
                options.frequencyOffset=p.value("offset-hz").toDouble(&valid);if(!valid)throw std::runtime_error("Invalid --offset-hz.");
                options.allowPartial=p.isSet("partial");const auto channel=p.value("channel");
                if(channel!="left" && channel!="right" && channel!="mix")throw std::runtime_error("Channel must be left, right, or mix.");
                const auto audio=sstv::readWav(p.value("decode"),channel=="right"?1:channel=="mix"?2:0);
                const auto result=sstv::decode(audio,options);
                if(!result.image.save(p.value("output"),"PNG"))throw std::runtime_error("Cannot save decoded image.");
                out<<result.message<<"\n";
            }
            return 0;
        }
        MainWindow window;window.show();
        if(p.isSet("screenshot"))QTimer::singleShot(400,&app,[&]{window.saveScreenshot(p.value("screenshot"));app.quit();});
        else if(p.isSet("smoke-test"))QTimer::singleShot(400,&app,&QApplication::quit);
        return app.exec();
    } catch(const std::exception& e) {err<<"Tux-sstv: "<<e.what()<<"\n";return 1;}
}
