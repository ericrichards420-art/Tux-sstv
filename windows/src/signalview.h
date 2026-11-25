#pragma once
#include "core/spectrum.h"
#include <QWidget>
#include <QColor>
#include <QByteArray>
#include <deque>
#include <optional>
class SignalView : public QWidget {
    Q_OBJECT
public:
    explicit SignalView(QWidget* parent=nullptr);
    void setSamples(const std::vector<float>& samples,int rate);
    void setColors(QColor background,QColor grid,QColor text,QColor trace,QColor warning);
    void beginInput();
    void endInput();
    void pushPcm(const QByteArray& pcm,int rate=12000);
    void setWaterfall(bool enabled);
    void setFrozen(bool frozen);
    void clearHistory();
    bool frozen() const{return frozen_;}
    bool inputActive() const{return active_;}
    size_t historyRows() const{return history_.size();}
    bool waterfall() const{return waterfall_;}
    static constexpr size_t maxRows=600;
signals:
    void freezeChanged(bool frozen);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    sstv::SpectrumAnalyzer analyzer_;
    std::deque<sstv::SpectrumFrame> history_,held_;
    std::optional<sstv::SpectrumFrame> preview_,heldSpectrum_;
    QByteArray oddByte_;
    bool active_=false,frozen_=false,waterfall_=true;
    QColor background_{"#10120f"},grid_{"#31402c"},text_{"#a2b08b"},trace_{"#a5e94d"},warning_{"#f18d82"};
    std::array<QRgb,256> colors_{};
    void makeColors();
};
