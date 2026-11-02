# Shared themes and compact formatting

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Light-only colors duplicated in Window, popup and chart | Semantic light/dark tokens and one application theme controller | Consistent, maintainable styling for every entry point |
| Green-tinted surfaces and text | Neutral graphite/soft-white surfaces, restrained blue accents, amber activity and violet memory | User requested a calmer visual direction |
| No appearance preference | Persisted System / Light / Dark setting, System default | Respect platform appearance and explicit choice |
| Local styles do not cover dialogs/menus | Global token stylesheet plus QPalette active/inactive/disabled roles | Controls and dialogs inherit the same theme |
| Fixed-height popup stretches sparse content | Top-aligned content with natural measured height and bounded scroll fallback | Remove artificial vertical whitespace |
| Repeated large status headings | Compact status row, quieter activity, grouped metric cards and clear graph axes | Improve scan order without changing monitoring behavior |
| Green application icon | Same bird/R identity in blue on a cool-white tile | Match the user-approved neutral/blue palette and preserve Dock readability |
| Rounded border with opaque square corners | Translucent frameless popup with a token-painted rounded shell | Remove the corner artifacts in the supplied screenshot |

References: Qt QStyleHints::colorScheme/colorSchemeChanged (since 6.5),
setColorScheme (since 6.8), QPalette roles/groups, stylesheet inheritance,
QWidget translucent frameless windows, QLayout heightForWidth.

- https://doc.qt.io/qt-6/qstylehints.html#colorScheme-prop
- https://doc.qt.io/qt-6/qpalette.html
- https://doc.qt.io/qt-6/stylesheet-syntax.html
- https://doc.qt.io/qt-6/qwidget.html#creating-translucent-windows
- https://doc.qt.io/qt-6/qlayout.html#totalHeightForWidth

| Reference step / acceptance gate | Implementation / verification |
|---|---|
| Resolve explicit/System preference | One resolver; Unknown system scheme falls back to Light; explicit choice wins |
| Follow OS appearance changes | QStyleHints signal updates only System preference; no polling or OS-specific shared code |
| Native appearance | Qt6.8+ setColorScheme reflects explicit preference; Unknown resets System. Qt6.5–6.7 use shared palette/styles while native chrome follows OS |
| Persist user choice | Settings Save updates one controller and QSettings; Cancel preserves it |
| Own all visual tokens | Colors/typography/spacing/radii in theme module; Window, popup and custom-painted charts consume it |
| Cover widget states | Active/inactive/disabled palette and menu/input/tooltip/selection/focus styles; no separate inline theme overrides |
| Update all views live | Controller signal repaints charts and refreshes presentation without touching Monitor/source/services |
| Bound layout | Natural heightForWidth, top alignment, screen clamp/scroll for short displays; long runner/job names wrap/elide within available width |
| Transparent shell | WA_TranslucentBackground before native handle creation, rounded painting, Cocoa lifecycle gates retained |
| Visual validation | Actual light/dark overview, panel and Settings captures; check sparse history, long names, empty state and disabled controls |
| Regression gates | Missing appearance-setting test fails before implementation; real Settings Save/Cancel and visible consumers tested, native presentation retained |

No resource sampling, history retention, service-control policy, authentication or
GitHub requests change. Screenshots use explicit scripted sample data. Native
Windows/Linux appearance and physical OS theme switching require their own checks;
local Mac captures are not evidence of those platforms.

## 0.7.1 control rendering correction (planned before implementation)

| Before | After | Reason |
|---|---|---|
| Native platform subcontrols inside custom rounded fields | Built-in Qt Fusion base with one shared token stylesheet covering combo and spin subcontrols | Avoid mixed native/QSS edges; keep the same implementation across platforms |
| Combo menu overlaps field with native checkmark styling | Qt list popup below the field (`combobox-popup: 0`) with themed rows | Keep the field readable while selecting, including in the menu-bar panel |
| Tiny native spin buttons with square patches | Inset, padded spin buttons with matching corner radii and visible hover/pressed/disabled states | Consistent geometry and readable controls |
| Exact-color disabled-text raster assertion | Check rendered text against alpha-composited foreground/background colors | Antialiased glyphs need not contain any fully covered pixel |
| Windows CTest failure lacks Qt UI details | Persistent UI report printed in CI | Diagnose native platform failures rather than guessing |

References: Qt's documented merged QComboBox and QSpinBox stylesheet examples,
QStyleFactory's portable built-in Fusion style, and the Qt 6.8 reference
implementation QStyleSheetStyle::styleHint (`combobox-popup`) and
QFusionStyle::drawPrimitive for standard arrows. Shared thin widget paint adapters
call Qt's standard arrow primitive after the styled base field, because QSS custom
subcontrols suppress the inherited arrow. No custom dropdown behavior,
painting algorithm or platform-specific shared logic is introduced.

- https://doc.qt.io/qt-6/stylesheet-examples.html#customizing-qcombobox
- https://doc.qt.io/qt-6/stylesheet-examples.html#customizing-qspinbox
- https://doc.qt.io/qt-6/qstylefactory.html
- https://github.com/qt/qtbase/blob/6.8/src/widgets/styles/qstylesheetstyle.cpp

| Reference / acceptance gate | Implementation and verification |
|---|---|
| Fully own field/subcontrol geometry | Shared stylesheet; assert separated text/arrow rectangles on real Settings controls |
| Preserve standard behavior | Real popup selection/Escape and spin clicks/keyboard/bounds tests |
| Use themed popup view | Actual runner chooser survives refresh; list geometry below field and theme colors verified |
| Preserve Cocoa panel lifecycle | Existing native nonactivating panel journey retained |
| Render both themes and states | Cocoa light/dark Settings and open chooser captures; focus, disabled, bounds covered |
| Defect regression fails first | Settings geometry/list-mode assertions run against unchanged production code |
| Portable raster acceptance | Antialias-aware text assertion plus wrong-foreground mutation; no platform skips |
| Independent review and native CI | Review before publication; Mac local results do not certify Windows/Linux |

Deviation: Fusion deliberately replaces the OS-specific widget base for consistent
themed interiors. Window chrome, Dock/menu-bar integration and native lifecycle
remain in the platform adapter.

Arrow conformance: real fields must match the Qt Fusion primitive raster in both
themes (including disabled/bound states). The popup content viewport must start
below the field; Qt intentionally shares the one-pixel outer border.
