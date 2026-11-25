#include "playback_mixer.h"
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QSet>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace audio {
namespace {
QString command(const QString& card,const QStringList& arguments){
    QProcess process;process.start("amixer",QStringList{"-c",card,"--"}+arguments);
    if(!process.waitForFinished(1500)){process.kill();process.waitForFinished(300);throw std::runtime_error("The sound-device mixer did not respond. Check the USB connection and try again.");}
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode())throw std::runtime_error(("Cannot adjust this device: "+QString::fromLocal8Bit(process.readAllStandardError()).trimmed().left(300)).toStdString());
    return QString::fromLocal8Bit(process.readAllStandardOutput());
}
QList<MixerValue> playbackValues(const QString& text){
    QList<MixerValue> result;int id=0;QString control;bool playback=false,writable=false;
    const QRegularExpression header("^numid=([0-9]+),iface=MIXER,name='(PCM|Speaker|Master|Headphone) Playback (Volume|Switch)'(?:,index=([0-9]+))?(?:,|$)");
    for(const auto& raw:text.split('\n')){
        const auto line=raw.trimmed();
        if(line.startsWith("numid=")){const auto m=header.match(line);playback=m.hasMatch();id=m.captured(1).toInt();control=m.captured(2)+","+(m.captured(4).isEmpty()?QString("0"):m.captured(4));writable=false;}
        else if(playback&&line.startsWith("; type="))writable=line.contains("access=rw");
        else if(playback&&writable&&line.startsWith(": values=")){
            const auto values=line.mid(9).trimmed();
            if(!values.isEmpty())result.append({id,values,control});
        }
    }
    return result;
}
struct SimpleControl {QString name;bool volume=false,capture=false,switchable=false,db=false;};
QList<SimpleControl> simpleControls(const QString& text){
    QList<SimpleControl> result;SimpleControl current;
    const QRegularExpression header("^Simple mixer control '(PCM|Speaker|Master|Headphone)',([0-9]+)$");
    const auto append=[&]{if(!current.name.isEmpty()&&(current.volume||current.switchable))result<<current;};
    for(const auto& raw:text.split('\n')){
        const auto line=raw.trimmed();
        if(line.startsWith("Simple mixer control ")){append();current={};const auto m=header.match(line);if(m.hasMatch())current.name=m.captured(1)+","+m.captured(2);}
        else if(!current.name.isEmpty()&&line.startsWith("Capabilities:")){
            const auto flags=line.mid(13).split(' ',Qt::SkipEmptyParts);current.volume=flags.contains("pvolume");current.capture=flags.contains("cvolume");current.switchable=flags.contains("pswitch");
            // Leave controls that also expose capture settings untouched.
            if(current.capture||flags.contains("cswitch")){current.volume=false;current.switchable=false;current.name.clear();}
        }else if(!current.name.isEmpty()&&line.contains("Playback")&&line.contains("dB]"))current.db=true;
    }
    append();return result;
}
void verify(const QString& text,const QList<SimpleControl>& controls){
    const QRegularExpression header("^Simple mixer control '([^']+)',([0-9]+)$"),percent("\\[([0-9]+)%\\]"),db("\\[(-?[0-9]+(?:\\.[0-9]+)?)dB\\]");
    QString name;QSet<QString> seen;
    for(const auto& raw:text.split('\n')){
        const auto line=raw.trimmed();const auto h=header.match(line);if(h.hasMatch()){name=h.captured(1)+","+h.captured(2);continue;}
        for(const auto& control:controls)if(control.name==name&&line.contains(": Playback ")&&!line.startsWith("Limits:")){
            const auto p=percent.match(line),d=db.match(line);
            if(control.switchable&&(!line.contains("[on]")||line.contains("[off]")))throw std::runtime_error("The selected sound device is still muted after adjustment.");
            if(control.volume){
                if(!p.hasMatch()||((!d.hasMatch()||d.captured(1).toDouble()<-.5)&&p.captured(1).toInt()<100))throw std::runtime_error("The selected sound device did not reach the requested playback level.");
                if(d.hasMatch()&&d.captured(1).toDouble()>.5)throw std::runtime_error("The selected sound device applied extra gain; its previous level will be restored.");
            }
            seen.insert(name);
        }
    }
    if(seen.size()!=controls.size())throw std::runtime_error("Could not verify the sound-device playback levels.");
}
}
QString mixerCard(const QString& pcm){
    static const QRegularExpression named("^(?:plughw|hw|sysdefault|front):CARD=([A-Za-z0-9_-]+)(?:,DEV=[0-9]+)?$"),shortName("^(?:plughw|hw):([A-Za-z0-9_-]+)(?:,[0-9]+)?$");
    auto match=named.match(pcm);if(!match.hasMatch())match=shortName.match(pcm);return match.hasMatch()?match.captured(1):QString{};
}
CaptureAdjustment reduceCapture(const QString& pcm){
    CaptureAdjustment result;result.pcm=pcm;
    try{
        const auto card=mixerCard(pcm);
        if(card.isEmpty())throw std::runtime_error("Select a named input in Audio devices for automatic device-volume adjustment, or lower the input/RX knob.");
        const auto identity=command(card,{"info"});
        const auto contents=command(card,{"contents"});
        const QRegularExpression header("^numid=([0-9]+),iface=MIXER,name='(?:[^']+ )?Capture Volume'(?:,|$)"),limits("min=(-?[0-9]+),max=(-?[0-9]+)");
        int id=0,minimum=0,maximum=0;bool supported=false;
        for(const auto& raw:contents.split('\n')){
            const auto line=raw.trimmed();
            if(line.startsWith("numid=")){const auto match=header.match(line);id=match.hasMatch()?match.captured(1).toInt():0;supported=false;}
            else if(id&&line.startsWith("; type=INTEGER,")&&line.contains("access=rw")){
                const auto match=limits.match(line);if(match.hasMatch()){minimum=match.captured(1).toInt();maximum=match.captured(2).toInt();supported=maximum>minimum;}
            }else if(supported&&line.startsWith(": values=")){
                QStringList values;bool changed=false;
                const qint64 step=std::max(qint64(1),(qint64(maximum)-minimum)/10);
                for(const auto& text:line.mid(9).split(',')){
                    bool ok=false;const int value=text.trimmed().toInt(&ok);
                    if(!ok||value<minimum||value>maximum)throw std::runtime_error("Unsupported input-volume values; lower the input/RX knob.");
                    const int next=int(std::max(qint64(minimum),qint64(value)-step));values<<QString::number(next);changed|=next<value;
                }
                if(!changed)continue;
                if(command(card,{"info"})!=identity)throw std::runtime_error("The input device changed; no volume adjustment was applied.");
                const auto verify=command(card,{"cset",QString("numid=%1").arg(id),values.join(',')});
                const QRegularExpression actual(": values=([^\\n]+)");const auto match=actual.match(verify);
                if(!match.hasMatch()||match.captured(1).trimmed()!=values.join(','))throw std::runtime_error("Could not verify the input volume; lower the input/RX knob.");
                result.changed=true;result.message="Auto RX lowered the device input volume";return result;
            }
        }
        throw std::runtime_error("No adjustable capture volume remains; lower the input/RX knob.");
    }catch(const std::exception& e){result.message=QString::fromUtf8(e.what());}
    return result;
}
QString restorePlayback(const MixerSetup& setup){
    QStringList errors;
    if(!setup.original.isEmpty())try{
        if(command(setup.card,{"info"})!=setup.identity)return "The USB sound device changed; previous levels were not applied to a different device.";
    }catch(const std::exception& e){return QString::fromUtf8(e.what());}
    for(const auto& value:setup.original)try{command(setup.card,{"cset",QString("numid=%1").arg(value.id),value.values});}catch(const std::exception& e){errors<<QString::fromUtf8(e.what());}
    if(!setup.original.isEmpty())try{
        const auto current=playbackValues(command(setup.card,{"contents"}));
        for(const auto& value:setup.original)if(std::none_of(current.begin(),current.end(),[&](const auto& item){return item.id==value.id&&item.values==value.values;}))errors<<"The previous playback level could not be verified.";
    }catch(const std::exception& e){errors<<QString::fromUtf8(e.what());}
    return errors.join(" ");
}
MixerSetup optimizePlayback(const QString& pcm){
    MixerSetup result;result.card=mixerCard(pcm);
    try{
        if(result.card.isEmpty())throw std::runtime_error("Select the SignaLink's named USB sound device first. Automatic adjustment does not change the computer's default speakers or custom audio routes.");
        result.identity=command(result.card,{"info"});
        if(!result.identity.contains("USB",Qt::CaseInsensitive))throw std::runtime_error("Automatic SignaLink adjustment requires a USB sound card. Choose its USB AUDIO CODEC device.");
        const auto initial=playbackValues(command(result.card,{"contents"}));
        const auto controls=simpleControls(command(result.card,{"scontents"}));
        if(initial.isEmpty()||initial.size()>16||controls.isEmpty())throw std::runtime_error("This USB device does not expose supported playback controls. Adjust its output in your computer's sound settings, then use the test tone.");
        QList<MixerValue> selected;
        for(const auto& value:initial)for(const auto& control:controls)if(control.name==value.control)selected<<value;
        for(const auto& control:controls)if(std::none_of(selected.begin(),selected.end(),[&](const auto& value){return value.control==control.name;}))throw std::runtime_error("Could not save this device's previous playback levels; no adjustment was applied.");
        result.original=selected;
        for(const auto& control:controls){
            QStringList args{"sset",control.name,"playback"};if(control.volume)args<<(control.db?"0dB":"100%");if(control.switchable)args<<"unmute";command(result.card,args);
        }
        verify(command(result.card,{"scontents"}),controls);
        result.optimized=true;result.description="USB playback unmuted and raised to the device's normal output level. Tux-sstv TX: 100%.";
    }catch(const std::exception& e){
        result.error=QString::fromUtf8(e.what());
        if(!result.original.isEmpty()){const auto error=restorePlayback(result);if(error.isEmpty())result.original.clear();else result.error+=" Could not restore the previous level: "+error;}
    }
    return result;
}
}
