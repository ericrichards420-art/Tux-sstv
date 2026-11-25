#pragma once
#include <QObject>
#include <QEvent>

// Observe visible UI churn while exercising the real refresh/remote path.
class UiRefreshProbe final:public QObject {
public:
    int enabledChanges=0,styleChanges=0,paints=0;
    void reset(){enabledChanges=styleChanges=paints=0;}
protected:
    bool eventFilter(QObject*,QEvent* event) override {
        if(event->type()==QEvent::EnabledChange)++enabledChanges;
        if(event->type()==QEvent::StyleChange)++styleChanges;
        if(event->type()==QEvent::Paint)++paints;
        return false;
    }
};
