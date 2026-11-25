#pragma once
#include "radio.h"
#include <QMutex>
#include <QStringList>
#include <chrono>
#include <thread>
#include <stdexcept>

struct FakeRadioState {
    QMutex mutex;
    QStringList events;
    std::atomic_bool keyed=false,failOn=false,failOff=false,failRead=false;
    std::atomic_int keyDelayMs=0;
    void event(const QString& text){QMutexLocker lock(&mutex);events<<text;}
    QStringList history(){QMutexLocker lock(&mutex);return events;}
};
class FakeRadio:public radio::Backend {
    std::shared_ptr<FakeRadioState> state_;
    bool open_=false;
public:
    explicit FakeRadio(std::shared_ptr<FakeRadioState> state):state_(std::move(state)){}
    void open(const radio::Config&) override{open_=true;state_->event("open");}
    QVariantMap read(bool =false) override{
        if(state_->failRead)throw std::runtime_error("Simulated cable disconnected");
        return {{"frequencyHz",145550000.},{"mode","FM"},{"ptt",state_->keyed?1:0}};
    }
    void setPtt(bool on) override{
        state_->event(on?"on-start":"off-start");
        if(on)std::this_thread::sleep_for(std::chrono::milliseconds(state_->keyDelayMs.load()));
        if((on&&state_->failOn)||(!on&&state_->failOff))throw std::runtime_error(on?"Simulated PTT ON timeout":"Simulated PTT OFF timeout");
        if(!open_)throw std::runtime_error("Radio is closed");
        state_->keyed=on;state_->event(on?"on":"off");
    }
    void apply(const radio::Command&) override{state_->event("apply");}
    QString close() override{
        if(open_&&state_->keyed){try{setPtt(false);}catch(const std::exception& e){open_=false;return e.what();}}
        open_=false;state_->event("close");return {};
    }
};
