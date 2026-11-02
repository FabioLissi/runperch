# Window close and menu-bar presence

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Close hides the window but leaves a Dock icon | macOS can hide the Dock icon while closed, default enabled | Keep a long-running monitor in the menu bar |
| No Dock preference | Settings can retain the Dock icon | Let users choose their preferred navigation |
| AppKit would leak into shared UI | Dedicated ApplicationPresentation adapter | Keep Windows/Linux builds independent |

References: Apple NSApplication.setActivationPolicy, NSApplicationActivationPolicyAccessory
and NSApplicationActivationPolicyRegular; Qt QSystemTrayIcon and QWidget close/show/hide events.

| Reference step / gate | Implementation and validation |
|---|---|
| Accessory policy removes Dock presence but permits windows/status items | macOS adapter calls setActivationPolicy(Accessory) on the main thread |
| Regular policy restores normal application presence | Same adapter restores Regular when main window shows |
| Only hide when a status-area route exists | Shared Window policy checks visible tray and current tray availability |
| No tray means a normal close, never an unreachable background app | Restore regular presence, enable quit-on-last-window, then accept close |
| Preference is persistent and optional | One Qt settings key; exposed only for a capable native adapter |
| OS can reject a policy transition | Adapter returns result; failed transitions are not recorded as successful |
| Independent platform builds | macOS Objective-C++ implementation; explicit unsupported Dock adapter elsewhere |
| Actual caller and reference checks | Real Window close/menu-reopen tests with injected environment; native adapter smoke on macOS |

No runner service or GitHub authentication state changes with window visibility.

## Validation

Three close/reopen, no-tray and failed-request tests failed against the prior
Window implementation, then passed with the shared visibility policy.
The native macOS test uses the actual Window and the real native adapter,
checks Accessory after close, and triggers the real menu's Open Runperch action
before checking visible Window plus Regular policy. It uses temporary settings,
no runner registrations and no network/service actions. This native test passed
on the development Mac. All seven local CTest gates passed; cross-platform CI
results are reported separately. A human has not yet confirmed the new behavior
in their normally running app.
