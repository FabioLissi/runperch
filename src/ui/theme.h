#pragma once
#include <QObject>
#include <QColor>
#include <QPixmap>
#include <Qt>
class QStyle;
namespace runperch {
enum class ThemePreference { System, Light, Dark };
struct ThemeTokens {
    QColor background,surface,sidebar,text,muted,border,hover,selection,selectedText,disabled,disabledBackground,accent,danger,warning,memory;
    int chartFillAlpha=24;
};
struct UiMetrics {
    int body=13,caption=11,heading=22,brand=24,popoverBrand=16,activity=17;
    int gap=8,section=12,inset=16,large=24,radius=8,panelRadius=12,chartHeight=76,logoSize=48,popoverWidth=384,popoverMaxHeight=560;
};
inline constexpr UiMetrics uiMetrics;
ThemeTokens themeTokens(bool dark);
bool useDarkTheme(ThemePreference,Qt::ColorScheme);
class ThemeController final : public QObject {
    Q_OBJECT
public:
    explicit ThemeController(QObject* parent=nullptr);
    ThemePreference preference() const { return choice; }
    bool isDark() const { return dark; }
    const ThemeTokens& tokens() const { return current; }
    QStyle& widgetBaseStyle() const { return *baseStyle; }
    void setPreference(ThemePreference);
signals:
    void changed();
private:
    void apply();
    ThemePreference choice=ThemePreference::System;
    bool dark=false;
    ThemeTokens current;
    QStyle* baseStyle=nullptr; // Owned by QApplication, beneath the stylesheet wrapper.
};
ThemeController& theme();
QString statusTone(const QString& status);
QColor statusColor(const QString& status);
QPixmap themedBrandMark();
}
