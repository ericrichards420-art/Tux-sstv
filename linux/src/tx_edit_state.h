#pragma once
#include "core/composition.h"
#include "core/templates.h"

// One TX drawing. Station identity, Morse/radio/audio settings, received pictures
// and files saved on disk are independent of this document's editing history.
struct TxEditState {
    QImage source;
    sstv::ImageInset inset;
    sstv::CallsignPlacement callsignPlacement;
    QList<sstv::TextOverlay> captions;
    sstv::TemplateFields templateFields;
    bool templateEnabled=false,showCallsign=true;
    QString selectedPattern;
    bool operator==(const TxEditState&) const=default;
};
