#include "ui/theme.h"
#include <QApplication>
#include <QHash>
#include <QPalette>
#include <QPainter>
#include <QSettings>
#include <QStyleHints>
#include <QStyleFactory>
namespace runperch {
ThemeTokens themeTokens(bool dark) {
    if(dark) return {
        .background=QColor("#17191f"), .surface=QColor("#20232b"), .sidebar=QColor("#12141a"),
        .text=QColor("#e8ebf2"), .muted=QColor("#a8b0bf"), .border=QColor("#3b414e"),
        .hover=QColor("#2a303c"), .selection=QColor("#2b4165"), .selectedText=QColor("#eef4ff"),
        .disabled=QColor("#7d8697"), .disabledBackground=QColor("#1c1f27"),
        .accent=QColor("#8cb4ff"), .danger=QColor("#f49a8d"), .warning=QColor("#e2bb77"), .memory=QColor("#c1a0ed"),
        .chartFillAlpha=32};
    return {
        .background=QColor("#f6f7f9"), .surface=QColor("#ffffff"), .sidebar=QColor("#eff1f5"),
        .text=QColor("#202632"), .muted=QColor("#5c6575"), .border=QColor("#d3d9e2"),
        .hover=QColor("#edf2fa"), .selection=QColor("#e3edff"), .selectedText=QColor("#153963"),
        .disabled=QColor("#7b8594"), .disabledBackground=QColor("#eef0f4"),
        .accent=QColor("#3566b8"), .danger=QColor("#bd4a3a"), .warning=QColor("#95651a"), .memory=QColor("#8064b2"),
        .chartFillAlpha=24};
}

bool useDarkTheme(ThemePreference choice,Qt::ColorScheme system) {
    return choice==ThemePreference::Dark || (choice==ThemePreference::System && system==Qt::ColorScheme::Dark);
}
namespace {
QString stylesheet(const ThemeTokens& t) {
    QString css=R"(
        QWidget { background:@background@; color:@text@; font-size:@body@px; }
        QWidget:disabled { color:@disabled@; }
        QLabel { background:transparent; }
        QWidget#sidebar { background:@sidebar@; border-right:1px solid @border@; }
        QLabel#brand { font-size:@brand@px; font-weight:700; }
        QLabel#heading { font-size:@heading@px; font-weight:650; }
        QLabel#activity { font-size:@activity@px; font-weight:600; }
        QLabel#muted, QLabel#caption, QLabel#footer { color:@muted@; }
        QLabel#caption, QLabel#footer { font-size:@caption@px; }
        QLabel#metrics { border-top:1px solid @border@; border-bottom:1px solid @border@; padding:@section@px 0; font-size:@body@px; }
        QLabel#footer { border-top:1px solid @border@; padding-top:@section@px; }
        QPushButton { border:1px solid @border@; border-radius:@radius@px; padding:@gap@px @section@px; background:@surface@; }
        QPushButton:hover { background:@hover@; }
        QPushButton:focus, QComboBox:focus, QLineEdit:focus, QSpinBox:focus { border:1px solid @accent@; }
        QPushButton:disabled { color:@disabled@; background:@disabledBackground@; }
        QWidget#sidebar QPushButton { text-align:left; border:none; padding:@section@px; background:transparent; }
        QWidget#sidebar QPushButton#selectedNav { background:@selection@; color:@selectedText@; font-weight:600; }
        QTableWidget, QListWidget { border:1px solid @border@; border-radius:@radius@px; background:@surface@; selection-background-color:@selection@; selection-color:@selectedText@; }
        QHeaderView::section { background:@sidebar@; border:none; border-bottom:1px solid @border@; padding:@gap@px; text-align:left; }
        QMenu { background:@surface@; border:1px solid @border@; padding:@gap@px; }
        QMenu::item { padding:@gap@px @large@px; } QMenu::item:selected { background:@selection@; color:@selectedText@; }
        QMenu::item:disabled { color:@disabled@; }
        QComboBox, QLineEdit, QSpinBox { background:@surface@; border:1px solid @border@; border-radius:@radius@px; padding:@gap@px; selection-background-color:@selection@; selection-color:@selectedText@; }
        QComboBox:disabled, QLineEdit:disabled, QSpinBox:disabled { color:@disabled@; background:@disabledBackground@; }
        QComboBox { padding-right:36px; combobox-popup:0; }
        QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:28px; border:none; background:transparent; border-top-right-radius:@radius@px; border-bottom-right-radius:@radius@px; }
        QComboBox::drop-down:hover { background:@hover@; }
        QComboBox::drop-down:on { background:@selection@; }
        QComboBox::drop-down:disabled { background:transparent; }
        QComboBox QAbstractItemView { background:@surface@; color:@text@; border:1px solid @border@; padding:4px; outline:none; selection-background-color:@selection@; selection-color:@selectedText@; }
        QComboBox QAbstractItemView::item { min-height:28px; padding:2px 8px; border:none; }
        QSpinBox { padding-right:36px; }
        QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin:padding; width:28px; border:none; background:transparent; }
        QSpinBox::up-button { subcontrol-position:top right; border-top-right-radius:@radius@px; }
        QSpinBox::down-button { subcontrol-position:bottom right; border-bottom-right-radius:@radius@px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background:@hover@; }
        QSpinBox::up-button:pressed, QSpinBox::down-button:pressed { background:@selection@; }
        QSpinBox::up-button:disabled, QSpinBox::down-button:disabled { background:transparent; }
        QToolTip { color:@text@; background:@surface@; border:1px solid @border@; padding:@gap@px; }
        QFrame#runnerPopover { background:transparent; border:none; }
        QWidget#popoverContent, QWidget#popoverViewport, QScrollArea#popoverScroll { background:transparent; border:none; }
        QLabel#popoverBrand { font-size:@popoverBrand@px; font-weight:700; }
        QLabel#popoverState { font-size:@body@px; font-weight:650; }
        QLabel#popoverState[tone="working"] { color:@warning@; }
        QLabel#popoverState[tone="idle"] { color:@accent@; }
        QLabel#popoverState[tone="quiet"] { color:@muted@; }
        QLabel#popoverActivity { font-size:@body@px; font-weight:500; }
        QLabel#popoverLocal, QLabel#popoverTiming, QLabel#popoverObserved, QLabel#historyCaption { color:@muted@; font-size:@caption@px; }
        QLabel#popoverCpu, QLabel#popoverMemory { font-size:@body@px; font-weight:650; }
        QFrame#metricCard { background:@surface@; border:1px solid @border@; border-radius:@radius@px; }
        QWidget#cpuHistory, QWidget#memoryHistory { background:transparent; }
        QPushButton#popoverOverview { font-size:@caption@px; padding:@gap@px; }
        QPushButton#popoverStop { color:@danger@; }
        QPushButton#popoverStop:disabled { color:@disabled@; }
    )";
    const QHash<QString,QColor> colors{{"background",t.background},{"surface",t.surface},{"sidebar",t.sidebar},{"text",t.text},{"muted",t.muted},{"border",t.border},{"hover",t.hover},{"selection",t.selection},{"selectedText",t.selectedText},{"disabled",t.disabled},{"disabledBackground",t.disabledBackground},{"accent",t.accent},{"danger",t.danger},{"warning",t.warning}};
    for(auto it=colors.cbegin();it!=colors.cend();++it) css.replace("@"+it.key()+"@",it.value().name());
    const QHash<QString,int> sizes{{"body",uiMetrics.body},{"brand",uiMetrics.brand},{"heading",uiMetrics.heading},{"activity",uiMetrics.activity},{"caption",uiMetrics.caption},{"popoverBrand",uiMetrics.popoverBrand},{"gap",uiMetrics.gap},{"section",uiMetrics.section},{"large",uiMetrics.large},{"radius",uiMetrics.radius}};
    for(auto it=sizes.cbegin();it!=sizes.cend();++it) css.replace("@"+it.key()+"@",QString::number(it.value()));
    return css;
}
}
ThemeController::ThemeController(QObject* parent):QObject(parent) {
    // One portable widget base prevents native subcontrols leaking through QSS.
    // QApplication owns the style; install it once, before applying theme rules.
    baseStyle=QStyleFactory::create("Fusion"); qApp->setStyle(baseStyle);
    setObjectName("applicationTheme"); const auto saved=QSettings().value("appearance","system").toString();
    choice=saved=="dark" ? ThemePreference::Dark : saved=="light" ? ThemePreference::Light : ThemePreference::System;
    connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,this,[this] { if(choice==ThemePreference::System) apply(); });
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
    QGuiApplication::styleHints()->setColorScheme(choice==ThemePreference::System ? Qt::ColorScheme::Unknown : choice==ThemePreference::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
    apply();
}
void ThemeController::setPreference(ThemePreference preference) {
    choice=preference;
    QSettings().setValue("appearance",choice==ThemePreference::System ? "system" : choice==ThemePreference::Dark ? "dark" : "light");
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
    QGuiApplication::styleHints()->setColorScheme(choice==ThemePreference::System ? Qt::ColorScheme::Unknown : choice==ThemePreference::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
    apply();
}
void ThemeController::apply() {
    dark=useDarkTheme(choice,QGuiApplication::styleHints()->colorScheme()); current=themeTokens(dark);
    QPalette palette;
    for(auto group:{QPalette::Active,QPalette::Inactive,QPalette::Disabled}) {
        const bool disabled=group==QPalette::Disabled;
        for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText,QPalette::ToolTipText}) palette.setColor(group,role,disabled ? current.disabled : current.text);
        palette.setColor(group,QPalette::Window,current.background); palette.setColor(group,QPalette::Base,current.surface);
        palette.setColor(group,QPalette::AlternateBase,current.sidebar); palette.setColor(group,QPalette::Button,disabled ? current.disabledBackground : current.surface);
        palette.setColor(group,QPalette::ToolTipBase,current.surface); palette.setColor(group,QPalette::Highlight,current.selection);
        palette.setColor(group,QPalette::HighlightedText,current.selectedText); palette.setColor(group,QPalette::PlaceholderText,current.muted);
        palette.setColor(group,QPalette::Light,current.border); palette.setColor(group,QPalette::Midlight,current.hover); palette.setColor(group,QPalette::Dark,current.border);
        palette.setColor(group,QPalette::Mid,current.border); palette.setColor(group,QPalette::Shadow,current.sidebar);
        palette.setColor(group,QPalette::Link,current.accent); palette.setColor(group,QPalette::LinkVisited,current.memory);
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
        palette.setColor(group,QPalette::Accent,current.accent);
#endif
    }
    qApp->setPalette(palette); qApp->setStyleSheet(stylesheet(current)); emit changed();
}
ThemeController& theme() {
    auto* controller=qApp->findChild<ThemeController*>("applicationTheme",Qt::FindDirectChildrenOnly);
    if(!controller) controller=new ThemeController(qApp); return *controller;
}
QString statusTone(const QString& status) {
    return status=="Running" ? "working" : status=="Idle" ? "idle" : "quiet";
}
QPixmap themedBrandMark() {
    QPixmap mark(":/assets/logo.png");
    QPainter painter(&mark); painter.setCompositionMode(QPainter::CompositionMode_SourceIn); painter.fillRect(mark.rect(),theme().tokens().accent); painter.end();
    return mark.scaled(uiMetrics.logoSize,uiMetrics.logoSize,Qt::KeepAspectRatio,Qt::SmoothTransformation);
}
QColor statusColor(const QString& status) {
    const auto& t=theme().tokens();
    if(statusTone(status)=="working") return t.warning;
    if(statusTone(status)=="idle") return t.accent;
    return t.muted;
}
}
