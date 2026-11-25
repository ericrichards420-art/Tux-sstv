#pragma once
#include <QString>
#include <QList>

namespace audio {
struct MixerValue {int id=0;QString values,control;};
struct MixerSetup {
    QString card,error,description,identity;
    QList<MixerValue> original;
    bool optimized=false;
};
// Runs on the wizard's worker. Commands always address one explicit USB card.
QString mixerCard(const QString& pcm);
MixerSetup optimizePlayback(const QString& pcm);
QString restorePlayback(const MixerSetup&);
struct CaptureAdjustment {QString pcm,message;bool changed=false;};
// Reduces one capture-only volume on an explicitly selected card. No playback
// controls, mute switches or ambiguous default routes are changed.
CaptureAdjustment reduceCapture(const QString& pcm);
}
