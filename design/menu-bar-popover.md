# Compact runner popover and resource history

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Tray activation opens the whole window | Primary activation toggles a compact panel, secondary activation retains the action menu | Fast local status without a Dock window |
| Latest resource snapshot only | Bounded 15-minute in-memory sample history per attachment | See trends without extra process queries or stored telemetry |
| Service controls only in overview | Popover emits requests to the same confirmation and Monitor write path | Identical admission and explicit Stop now warning |
| Overview job link can fall back to old jobs | Popover's Open current job only enables for fresh active job evidence | Do not present history as current work |
| Confirmation resolves a root after interaction | Pin attachment instance as well as root for both views | Removing/re-adding a folder must not retarget a pending confirmation |

## Reference conformance

Production practice: time-series charts use recorded timestamps and units, gaps
for missing observations, and bounded retention. No interpolation or smoothing is
introduced. Qt's documented QSystemTrayIcon activation/geometry, Qt::Popup,
QScreen::availableGeometry and QPainter supply the GUI. Shared Qt content in a separate panel
avoids QWidgetAction's documented native macOS menu focus/reparenting limitations.

- https://doc.qt.io/qt-6/qsystemtrayicon.html
- https://doc.qt.io/qt-6/qt.html#WindowType-enum
- https://doc.qt.io/qt-6/qscreen.html#availableGeometry-prop
- https://doc.qt.io/qt-6/qwidgetaction.html
- https://doc.qt.io/qt-6/qpainter.html
- https://developer.apple.com/documentation/appkit/nswindow/stylemask-swift.struct/nonactivatingpanel
- https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/MonitoringEvents/MonitoringEvents.html

| Step / acceptance gate | Implementation / verification |
|---|---|
| Record only completed local observations | One history helper called by Monitor's two local-publication paths; remote refresh/UI ticks add no samples |
| Reject future, stale, duplicate and out-of-order timestamps | Shared freshness contract; deterministic history tests |
| Validate units and missing data | Finite nonnegative core equivalents and RSS bytes; nulls remain gaps, never zeros |
| Bound history | 900-second horizon and 180 samples per attached runner; constants are one owner; removal drops attachment history |
| Show accurate trend | Timestamp X axis, zero-based adaptive Y axis with labeled peak, real points, breaks at unavailable values/restart/over-30s gap; CPU average/RSS units explicit |
| Preserve existing status rules | Popover calls status/localWorkState/isFresh/canControl; lack of worker never establishes connected Idle |
| Mac native presentation | Non-activating NSPanel/Qt::Tool behind ApplicationPresentation; Accessory/Dock policy retained. Plain Qt::Popup elsewhere. Qt::Popup was rejected on Mac because pending application deactivation dismisses it |
| Native handle capability | Configure NSPanel only for Qt Cocoa; offscreen/other Qt backends retain Qt::Popup and never reinterpret their handles as NSView |
| GUI thread | NSEvent handlers follow Apple’s main-thread contract; NSWorkspace observer explicitly delivers on NSOperationQueue.mainQueue |
| Dismissal observation | Mac observes outside mouse clicks and real foreground-app changes only while visible; ignore duplicate activation of the same foreground app. Local Qt mouse filter and Escape share dismissal; remove native monitors on hide/destruction |
| Popup lifecycle | Tray Trigger toggles popup, outside click/Escape closes it; Open overview restores window/Dock; no tray keeps full-window fallback |
| Fit display | Anchor to tray geometry with cursor fallback, clamp to available screen including negative screen origins; scroll content on short displays |
| Stable selection | Runner root and attachment instance, refreshed choices; removed/re-added selection never authorizes an old control request |
| One control owner | Main window and popover call same confirmation handler and Monitor::control; Cancel default, fresh rechecks at write time |
| Current job action | Fresh Running remote job and validated same-repository HTTPS URL; re-resolve identity immediately on click |
| Test real consumers | Monitor-produced histories drive real popover/widgets; UI activation, selection, cancellation, accepted controls and URL handler tested |
| Native presentation | Cocoa tray activation/popup/Escape/overview round-trip test; source reviewed independently before publishing |

History resets on app exit and is not persisted, uploaded or claimed as complete.
The default 15-second observation interval is unchanged. Native Windows/Linux
runner metrics remain unsupported; shared UI builds on all three platforms.
