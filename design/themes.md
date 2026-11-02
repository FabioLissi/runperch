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
