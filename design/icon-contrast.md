# Application icon contrast

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Transparent dark mark used as Dock icon | Dedicated light-tile app icon with a larger green mark | The dark Dock hid the mark in the reported screenshot |
| The same source supplies app, sidebar and template roles | App/window and bundle use app-icon.png; sidebar/menu template retain logo.png | A template icon needs transparency; a Dock icon needs contrast |

Reference: [Qt application icons](https://doc.qt.io/qt-6/appicon.html).

| Reference step / acceptance | Implementation / evidence |
|---|---|
| Runtime window icon is set explicitly | Main application and real Window use the dedicated icon resource |
| Finder and launch icon use packaged ICNS | Existing Apple iconutil adapter now consumes the same app-icon.png |
| Bundle metadata points to an installed icon | Existing native bundle test checks metadata, resource and ICNS header |
| Reproduce the reported visibility defect | Actual Window's 32px icon fails the light-backing check before the change |
| Preserve menu-bar template behavior | Its transparent logo resource and isMask setting remain separate |

This is a visual asset correction, not a runner or service behavior change.
The application version advances to 0.2.1 for the delivered app build.

Local Release validation: all six CTest gates passed with the new icon. The
32px real-window regression passed after failing against the old transparent
asset. The bundle reports application version 0.2.1. Visual review of the new
asset confirms a light tile around the green mark; the running pre-update
process retains its old icon until the user quits and reopens it.
