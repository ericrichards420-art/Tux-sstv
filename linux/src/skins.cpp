#include "skins.h"
#include <algorithm>
#include <utility>
#include <QResource>

static void ensureSkinResources() {
    static const bool initialized=[] {Q_INIT_RESOURCE(skin_icons);return true;}();
    Q_UNUSED(initialized);
}

namespace ui {
const std::vector<Skin>& skins() {
    static const std::vector<Skin> catalog={
        {"classic","Classic","The familiar gray radio desk, with beveled controls.",
         "#d4d0c8","#e5e2da","#f3f1ec","#ffffff","#181818","#72716e","#96938d","#316ac5","#28603a","#a52222","#a5e94d",0},
        {"midnight","Midnight","Deep navy panels with ice-blue controls and a cyan spectrum.",
         "#0b1320","#121f31","#1b2d44","#091420","#e9f2ff","#95abc4","#334a64","#50c4f2","#6cdea1","#ff8a94","#4ce5d3",6},
        {"aurora","Aurora","Violet instruments, lilac accents and a mint-green spectrum.",
         "#18142b","#241d3b","#32264d","#110d21","#f3eaff","#b2a1cb","#524469","#c59bff","#70e4c7","#ff8aa4","#5fe6c8",7},
        {"ember","Ember","Warm charcoal and copper, with amber controls and scope trace.",
         "#1d1715","#2c211c","#3b2c23","#15100e","#fff0dc","#c4ad97","#65503f","#ffb45b","#b2d88b","#ff9181","#ffb45b",5},
        {"phosphor","Phosphor","A green CRT-inspired station with crisp monospace lettering.",
         "#061009","#0b1b10","#102817","#030b05","#a5f5a9","#76b786","#2b5836","#78ed8a","#9df098","#ffac75","#9dff76",0,true},
        {"arctic","Arctic","Bright porcelain panels, slate lettering and cobalt-blue controls.",
         "#e7edf4","#f7faff","#ffffff","#ffffff","#172b43","#61758a","#a5b6c8","#2363d1","#287954","#ad3143","#56c4ff",6},
        {"neon-circuit","Neon Circuit","Carbon-black instruments, electric cyan edges and acid-lime highlights.",
         "#080f13","#111f27","#19313b","#050c11","#e4fbff","#9dbbc4","#365a68","#39e6ed","#94efb3","#ff8e9c","#a9f95e",2,true,"#a9f95e",SkinFinish::Neon},
        {"synthwave","Synthwave","An after-dark station in ultraviolet, hot pink and luminous turquoise.",
         "#140e25","#241634","#362247","#0d0919","#fff0ff","#c8aed8","#654778","#ff8bdc","#72e8c9","#ff9c9c","#54e9ee",9,false,"#54e9ee",SkinFinish::Neon},
        {"solar-flare","Solar Flare","Anodized graphite, warm orange instrument lights and gold highlights.",
         "#171514","#292521","#3b342b","#100e0c","#fff3e2","#cab9a2","#685948","#ffc06a","#b5e397","#ff9490","#ffd668",4,false,"#ffe49b",SkinFinish::Metal},
        {"ocean-glass","Ocean Glass","Deep ocean-blue glass, rounded panels and seafoam controls.",
         "#081b25","#10313e","#1c4653","#06151f","#e5fbff","#9fc5d0","#3d6c7b","#6ee7d1","#9af0b5","#ff9da8","#7edfff",12,false,"#7dbfff",SkinFinish::Glass},
        {"titanium","Titanium","Brushed silver instruments, crisp graphite text and precision blue details.",
         "#d4dce5","#e8eef4","#f8fbff","#ffffff","#182d44","#50657b","#8196ac","#195ba8","#216440","#a1273a","#75ceff",3,false,"#187b82",SkinFinish::Metal},
        {"crimson-ops","Crimson Ops","Angular obsidian panels with ruby-red lighting and cool silver type.",
         "#140d13","#241a23","#352632","#0c080d","#fff0f5","#c4abb9","#654759","#ff94ad","#a8e6b3","#ffb46d","#ff9fbd",0,true,"#c9c4ef",SkinFinish::Neon}
    };
    return catalog;
}
const Skin& skinById(const QString& id) {
    const auto& catalog=skins();const auto it=std::find_if(catalog.begin(),catalog.end(),[&](const Skin& skin){return skin.id==id;});
    return it==catalog.end()?catalog.front():*it;
}
namespace {
QColor selectedText(const Skin& s) {
    return .2126*s.accent.red()+.7152*s.accent.green()+.0722*s.accent.blue()>160?QColor("#06111b"):QColor("#ffffff");
}
}
QPalette skinPalette(const Skin& s) {
    QPalette p;
    p.setColor(QPalette::Window,s.background);p.setColor(QPalette::WindowText,s.text);
    p.setColor(QPalette::Base,s.field);p.setColor(QPalette::AlternateBase,s.surface);p.setColor(QPalette::Text,s.text);
    p.setColor(QPalette::Button,s.raised);p.setColor(QPalette::ButtonText,s.text);
    p.setColor(QPalette::Highlight,s.accent);p.setColor(QPalette::HighlightedText,selectedText(s));p.setColor(QPalette::BrightText,s.error);
    p.setColor(QPalette::Light,s.raised.lighter(120));p.setColor(QPalette::Midlight,s.raised);
    p.setColor(QPalette::Mid,s.border);p.setColor(QPalette::Dark,s.background.darker(125));p.setColor(QPalette::Shadow,s.border.darker(160));
    p.setColor(QPalette::PlaceholderText,s.muted);
    p.setColor(QPalette::Disabled,QPalette::WindowText,s.muted);p.setColor(QPalette::Disabled,QPalette::Text,s.muted);p.setColor(QPalette::Disabled,QPalette::ButtonText,s.muted);
    return p;
}
QString skinStyleSheet(const Skin& s) {
    ensureSkinResources();
    QString sheet=R"(
        QWidget {background:@bg@;color:@text@;font-size:12px;font-family:@font@;}
        QMainWindow,QDialog {background:@bg@;}
        QLabel,QCheckBox,QRadioButton {background:transparent;}
        QWidget#receivePage,QWidget#transmitPage,QWidget#syncPage {background:@panel@;}
        QGroupBox {background:@panel@;border:1px solid @border@;border-radius:@radius@px;margin-top:10px;padding:6px;}
        QGroupBox::title {subcontrol-origin:margin;left:8px;padding:0 5px;color:@muted@;}
        QPushButton {background:@button@;border:1px solid @border@;border-radius:@radius@px;padding:3px 8px;min-height:17px;}
        QPushButton:hover {border-color:@accent@;background:@hover@;}
        QPushButton:pressed,QPushButton:checked {background:@accent@;border-color:@accent@;color:@selectedText@;}
        QPushButton:disabled {color:@muted@;background:@panel@;border-color:@border@;}
        QPushButton:focus {border-color:@accent@;}
        QPushButton#playButton,QPushButton#stopButton {font-weight:bold;color:@error@;}
        QLineEdit,QComboBox,QAbstractSpinBox,QPlainTextEdit {background:@field@;color:@text@;border:1px solid @border@;border-radius:@radius@px;padding:3px;min-height:17px;selection-background-color:@accent@;selection-color:@selectedText@;}
        QLineEdit:focus,QComboBox:focus,QAbstractSpinBox:focus {border-color:@accent@;}
        QLineEdit:disabled,QComboBox:disabled,QAbstractSpinBox:disabled {color:@muted@;background:@panel@;}
        QAbstractSpinBox {padding-right:22px;}
        QSpinBox::up-button,QDoubleSpinBox::up-button {subcontrol-origin:border;subcontrol-position:top right;width:18px;height:12px;background:@raised@;border-left:1px solid @border@;}
        QSpinBox::down-button,QDoubleSpinBox::down-button {subcontrol-origin:border;subcontrol-position:bottom right;width:18px;height:12px;background:@raised@;border-left:1px solid @border@;}
        QSpinBox::up-arrow,QDoubleSpinBox::up-arrow {image:url(:/skins/arrow-up-@arrowInk@.xpm);width:9px;height:5px;}
        QSpinBox::down-arrow,QDoubleSpinBox::down-arrow {image:url(:/skins/arrow-down-@arrowInk@.xpm);width:9px;height:5px;}
        QSpinBox::up-arrow:disabled,QSpinBox::down-arrow:disabled,QDoubleSpinBox::up-arrow:disabled,QDoubleSpinBox::down-arrow:disabled {image:none;}
        QCheckBox::indicator {width:14px;height:14px;border:1px solid @border@;border-radius:2px;background:@field@;}
        QCheckBox::indicator:hover {border-color:@accent@;}
        QCheckBox::indicator:checked {background:@accent@;border-color:@accent@;image:url(:/skins/@checkIcon@);}
        QComboBox {padding-right:22px;}
        QComboBox QAbstractItemView {background:@field@;color:@text@;selection-background-color:@accent@;selection-color:@selectedText@;}
        QTabWidget::pane {border:1px solid @border@;background:@panel@;}
        QTabBar::tab {background:@bg@;color:@muted@;border:1px solid @border@;padding:5px 13px;margin-right:2px;}
        QTabBar::tab:selected {background:@panel@;color:@text@;border-top:2px solid @accent@;padding-top:4px;}
        QTabBar::tab:hover {color:@text@;}
        QListWidget,QTableWidget {background:@field@;color:@text@;border:1px solid @border@;border-radius:@radius@px;gridline-color:@border@;selection-background-color:@accent@;selection-color:@selectedText@;}
        QListWidget::item {padding:3px;}
        QHeaderView::section {background:@raised@;color:@text@;padding:4px;border:1px solid @border@;}
        QProgressBar {border:1px solid @border@;background:@field@;color:@text@;height:8px;}
        QProgressBar::chunk {background:@accent@;}
        QProgressBar#transmitProgress {border:1px solid @border@;background:@field@;color:@text@;font-weight:bold;font-size:13px;}
        QProgressBar#transmitProgress::chunk {background:@progress@;}
        QLabel#transmitProgressInfo {color:@success@;}
        QLabel#detectedReceiveMode {font-weight:bold;font-size:14px;color:@accent@;}
        QLabel#settingsMorseError {color:@error@;}
        QLabel#statusLabel {background:@panel@;border:1px solid @border@;border-radius:@radius@px;padding:3px;}
        QLabel#skinDescription {color:@muted@;}
        QMenuBar,QMenu {background:@panel@;color:@text@;}
        QMenu::item:selected,QMenuBar::item:selected {background:@accent@;color:@selectedText@;}
        QToolTip {background:@raised@;color:@text@;border:1px solid @accent@;padding:4px;}
        QScrollBar:vertical {background:@panel@;width:12px;margin:0;}
        QScrollBar:horizontal {background:@panel@;height:12px;margin:0;}
        QScrollBar::handle {background:@border@;min-height:18px;min-width:18px;border-radius:4px;}
        QScrollBar::handle:hover {background:@accent@;}
        QScrollBar::add-line,QScrollBar::sub-line {height:0;width:0;}
        QScrollBar::add-page,QScrollBar::sub-page {background:transparent;}
    )";
    if(s.finish!=SkinFinish::Flat) {
        sheet+=R"(
          QGroupBox {background:@material@;border-top:2px solid @rim@;}
          QGroupBox::title {color:@accent@;font-weight:bold;}
          QPushButton {background:@materialButton@;border-top-color:@rim@;}
          QPushButton:hover {background:@hover@;border-color:@accent@;}
          QPushButton:pressed,QPushButton:checked {background:@accent@;color:@selectedText@;}
          QPushButton:disabled {background:@panel@;border-color:@border@;color:@muted@;}
          QTabBar::tab {border-top-left-radius:@radius@px;border-top-right-radius:@radius@px;}
          QTabBar::tab:selected {background:@material@;color:@accent@;border-top-color:@secondary@;font-weight:bold;}
          QProgressBar::chunk {background:@accentGradient@;}
          QProgressBar#transmitProgress::chunk {background:@progressGradient@;}
          QSlider::groove:horizontal {background:@field@;height:5px;border:1px solid @border@;border-radius:3px;}
          QSlider::sub-page:horizontal {background:@accent@;border-radius:3px;}
          QSlider::handle:horizontal {background:@secondary@;border:1px solid @accent@;width:12px;margin:-5px 0;border-radius:6px;}
          QSplitter::handle {background:@border@;}
        )";
    }
    const auto gradient=[](const QColor& a,const QColor& b){return QString("qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2)").arg(a.name(),b.name());};
    const auto secondary=s.secondary.isValid()?s.secondary:s.accent;
    const QColor rim=s.finish==SkinFinish::Neon?s.accent.darker(150):s.border.lighter(125);
    const QString button=s.id=="classic"?"qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #faf9f6,stop:1 #d4d0c8)":s.raised.name();
    const auto progress=s.id=="classic"?QColor("#83bb79"):s.background.lightness()<128?s.accent.darker(220):s.accent.lighter(170);
    const std::vector<std::pair<QString,QString>> tokens={
        {"@shell@",gradient(s.background,s.surface)},
        {"@material@",gradient(s.surface.lighter(s.finish==SkinFinish::Glass?125:110),s.surface)},
        {"@materialButton@",gradient(s.raised.lighter(s.finish==SkinFinish::Metal?112:105),s.raised)},
        {"@accentGradient@",gradient(s.accent,secondary)},
        {"@progressGradient@",gradient(progress,progress.lighter(120))},
        {"@secondary@",secondary.name()},{"@rim@",rim.name()},
        {"@bg@",s.background.name()},{"@panel@",s.surface.name()},{"@raised@",s.raised.name()},
        {"@field@",s.field.name()},{"@text@",s.text.name()},{"@muted@",s.muted.name()},
        {"@border@",s.border.name()},{"@accent@",s.accent.name()},{"@success@",s.success.name()},
        {"@error@",s.error.name()},{"@selectedText@",selectedText(s).name()},
        {"@button@",button},{"@hover@",s.raised.lighter(115).name()},{"@progress@",progress.name()},
        {"@radius@",QString::number(s.radius)},{"@font@",s.monospace?"Monospace":"Sans Serif"},
        {"@checkIcon@",selectedText(s).lightness()<128?"check-dark.xpm":"check-light.xpm"},
        {"@arrowInk@",s.background.lightness()<128?"light":"dark"}
    };
    for(const auto& [token,value]:tokens)sheet.replace(token,value);
    return sheet;
}
Skin radioSkin(const Skin& skin) {
    if(skin.id!="classic")return skin;
    return {"radio-classic","Radio","Tux-sstv radio console",
        "#0c141e","#142231","#203649","#060e17","#edf5fc","#9bb1c7","#30495e","#72f1d5","#50e2b8","#ff8d99","#72f1d5",8};
}
QString radioStyleSheet(const Skin& s) {
    ensureSkinResources();
    QString sheet=R"(
      QWidget#radioFrontend, QWidget#radioConsoleBody, QWidget#radioControlDeck {background:@bg@;color:@text@;}
      QWidget#radioFrontend QLabel {background:transparent;color:@muted@;}
      QWidget#radioFrontend QLabel#radioConsoleTitle {font-size:21px;font-weight:bold;color:@text@;}
      QWidget#radioFrontend QLabel#radioConsoleSubtitle {font-size:9px;letter-spacing:1px;color:@muted@;}
      QWidget#radioFrontend QLabel#radioFrontendConnection {font-size:10px;color:@muted@;}
      QWidget#radioFrontend QLabel#radioFrontendFrequency {font-family:"DejaVu Sans Mono";font-size:56px;font-weight:bold;color:@accent@;}
      QWidget#radioFrontend QLabel#radioFrequencyUnit {font-size:14px;font-weight:bold;color:@muted@;padding-bottom:12px;}
      QWidget#radioFrontend QLabel#radioFrontendBadge {background:@raised@;border:1px solid @border@;border-radius:5px;padding:4px 12px;font-size:12px;font-weight:bold;}
      QWidget#radioFrontend QLabel#radioFrontendModeReadout,QWidget#radioFrontend QLabel#radioFrontendVfoReadout {background:@raised@;border:1px solid @border@;border-radius:5px;padding:4px 10px;font-family:"DejaVu Sans Mono";font-size:11px;color:@text@;}
      QWidget#radioFrontend QLabel#radioConsoleSection,QWidget#radioFrontend QLabel#radioStationCaption {font-size:10px;font-weight:bold;letter-spacing:1px;color:@muted@;}
      QWidget#radioFrontend QLabel[instrumentReadout="true"] {background:@panel@;border:1px solid @border@;border-radius:4px;padding:4px 10px;font-family:"DejaVu Sans Mono";font-size:10px;color:@text@;}
      QWidget#radioFrontend QFrame#radioConsoleDisplay {background:@display@;border:1px solid @border@;border-top:2px solid @accent@;border-radius:10px;}
      QWidget#radioFrontend QFrame#radioStationBar {background:@panel@;border:1px solid @border@;border-radius:7px;}
      QWidget#radioFrontend QLabel#remoteStationStatus {font-size:11px;color:@text@;}
      QWidget#radioFrontend QLabel#radioFrontendSignal {font-size:10px;color:@muted@;padding:0 8px;}
      QWidget#radioFrontend QFrame#radioStationBar QPushButton,QWidget#radioFrontend QFrame#radioStationBar QToolButton {font-size:11px;padding:4px 8px;}
      QWidget#radioFrontend QFrame#radioVoiceDeck {background:@panel@;border:1px solid @border@;border-left:3px solid @accent@;border-radius:7px;}
      QWidget#radioFrontend QLabel#voiceStatus,QWidget#radioFrontend QLabel#radioFrontendStatus,QWidget#radioFrontend QLabel#radioConsoleHint {font-size:11px;color:@muted@;}
      QWidget#radioFrontend QGroupBox {background:@panel@;border:1px solid @border@;border-radius:8px;margin-top:12px;padding:10px;}
      QWidget#radioFrontend QGroupBox#radioTuningCard,QWidget#radioFrontend QGroupBox#radioOperatingCard {padding:0;}
      QWidget#radioFrontend QGroupBox::title {color:@accent@;font-size:10px;font-weight:bold;letter-spacing:1px;subcontrol-origin:margin;left:12px;padding:0 6px;}
      QWidget#radioFrontend QPushButton,QWidget#radioFrontend QToolButton {background:@button@;color:@text@;border:1px solid @border@;border-radius:5px;padding:6px 10px;}
      QWidget#radioFrontend QPushButton:hover {background:@hover@;border-color:@accent@;}
      QWidget#radioFrontend QPushButton:pressed,QWidget#radioFrontend QPushButton:checked {background:@accent@;color:@selected@;}
      QWidget#radioFrontend QPushButton:disabled {background:@panel@;color:@muted@;border-color:@border@;}
      QWidget#radioFrontend QPushButton#radioFrontendStop {background:@stopBg@;color:@stopText@;border-color:@error@;font-weight:bold;}
      QWidget#radioFrontend QPushButton#radioFrontendConnect,QWidget#radioFrontend QPushButton#radioFrontendTune {border-color:@accent@;font-weight:bold;}
      QWidget#radioFrontend QPushButton#voicePttButton {background:@button@;border:1px solid @accent@;font-size:13px;font-weight:bold;}
      QWidget#radioFrontend QPushButton#voicePttButton:disabled {border-color:@border@;color:@muted@;}
      QWidget#radioFrontend QPushButton#radioFrontendUp,QWidget#radioFrontend QPushButton#radioFrontendDown {font-size:20px;font-weight:bold;padding:0;}
      QWidget#radioFrontend QProgressBar {background:@field@;color:@text@;border:1px solid @border@;border-radius:4px;text-align:center;font-size:10px;}
      QWidget#radioFrontend QProgressBar::chunk {background:@accent@;border-radius:3px;}
      QWidget#radioFrontend QAbstractSpinBox, QWidget#radioFrontend QComboBox {background:@field@;color:@text@;border:1px solid @border@;border-radius:4px;padding:5px;}
      QWidget#radioFrontend QAbstractSpinBox:disabled, QWidget#radioFrontend QComboBox:disabled {color:@muted@;border-color:@border@;}
      QWidget#radioFrontend QAbstractSpinBox {padding-right:24px;}
      QWidget#radioFrontend QSpinBox::up-button, QWidget#radioFrontend QDoubleSpinBox::up-button, QWidget#radioFrontend QSpinBox::down-button, QWidget#radioFrontend QDoubleSpinBox::down-button {background:@button@;border-left:1px solid @border@;}
      QWidget#radioFrontend QSpinBox::up-arrow, QWidget#radioFrontend QDoubleSpinBox::up-arrow {image:url(:/skins/arrow-up-@ink@.xpm);}
      QWidget#radioFrontend QSpinBox::down-arrow, QWidget#radioFrontend QDoubleSpinBox::down-arrow, QWidget#radioFrontend QComboBox::down-arrow {image:url(:/skins/arrow-down-@ink@.xpm);}
      QWidget#radioFrontend QComboBox QAbstractItemView {background:@field@;color:@text@;selection-background-color:@accent@;selection-color:@selected@;}
      QWidget#radioFrontend QSlider::groove:horizontal {background:@field@;height:5px;border-radius:2px;}
      QWidget#radioFrontend QSlider {background:transparent;}
      QWidget#radioFrontend QWidget#radioSpeakerControls {background:transparent;}
      QWidget#radioFrontend QCheckBox {background:transparent;color:@text@;spacing:7px;}
      QWidget#radioFrontend QCheckBox::indicator {width:14px;height:14px;border:1px solid @border@;border-radius:3px;background:@field@;}
      QWidget#radioFrontend QCheckBox::indicator:checked {background:@accent@;border-color:@accent@;image:url(:/skins/@checkIcon@);}
      QWidget#radioFrontend QSlider::sub-page:horizontal {background:@accent@;border-radius:2px;}
      QWidget#radioFrontend QSlider::handle:horizontal {background:@accent@;border:1px solid @accent@;border-radius:6px;width:12px;margin:-4px 0;}
      QWidget#radioFrontend QScrollArea {border:0;background:transparent;}
      QWidget#radioFrontend QScrollBar:vertical {background:@bg@;width:8px;margin:0;}
      QWidget#radioFrontend QScrollBar::handle:vertical {background:@border@;min-height:32px;border-radius:4px;}
      QWidget#radioFrontend QScrollBar::add-line:vertical,QWidget#radioFrontend QScrollBar::sub-line:vertical {height:0;}
      QWidget#radioFrontend QScrollBar::add-page:vertical,QWidget#radioFrontend QScrollBar::sub-page:vertical {background:transparent;}
    )";

    const auto gradient=[](QColor a,QColor b){return QString("qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2)").arg(a.name(),b.name());};
    const bool decorated=s.finish!=SkinFinish::Flat;
    if(decorated)sheet+="QWidget#radioFrontend QGroupBox {border-top:2px solid @accent@;border-radius:@radius@px;} QWidget#radioFrontend QFrame#radioConsoleDisplay {border-color:@accent@;}";
    const std::vector<std::pair<QString,QString>> tokens={
        {"@bg@",s.background.name()},{"@text@",s.text.name()},{"@muted@",s.muted.name()},
        {"@field@",s.field.name()},{"@border@",s.border.name()},{"@accent@",s.accent.name()},
        {"@raised@",s.raised.name()},{"@display@",gradient(s.field.lighter(130),s.field)},
        {"@error@",s.error.name()},{"@selected@",selectedText(s).name()},
        {"@panel@",decorated?gradient(s.surface.lighter(115),s.surface):s.surface.name()},
        {"@button@",decorated?gradient(s.raised.lighter(110),s.raised):s.raised.name()},
        {"@hover@",s.raised.lighter(120).name()},
        {"@stopBg@",s.background.lightness()<128?s.error.darker(300).name():s.error.lighter(190).name()},
        {"@stopText@",s.background.lightness()<128?s.error.lighter(125).name():s.error.darker(120).name()},
        {"@ink@",s.background.lightness()<128?"light":"dark"},{"@radius@",QString::number(s.radius)}
        ,{"@checkIcon@",selectedText(s).lightness()<128?"check-dark.xpm":"check-light.xpm"}
    };
    for(const auto& [token,value]:tokens)sheet.replace(token,value);
    return sheet;
}

}
