# Third-party software

Runperch source is licensed under Apache-2.0. Dependencies retain their own
licenses; this notice does not relicense them.

## Qt

Runperch uses the open-source/community edition of Qt, dynamically linked:
Qt Core, Qt Gui and Qt Widgets; Qt Test is used for development tests only.
The applicable open-source choice for these Qt libraries is LGPL-3.0, with
additional bundled third-party notices as specified by the exact Qt release.
No GPL-only Qt add-on module is required by Runperch.

- [Qt license overview](https://doc.qt.io/qt-6/licensing.html)
- [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
- [Qt sources](https://download.qt.io/official_releases/qt/)
- [Qt third-party components](https://doc.qt.io/qt-6/licenses-used-in-qt.html)

Development builds use the user's installed Qt. Binary distributors must ship
the applicable Qt/LGPL/GPL and third-party license texts, corresponding source
or a compliant source offer for the exact distributed Qt build, and the
information needed to rebuild, replace/relink and run the modified libraries.
Do not treat a link to this page alone as a complete binary compliance package.
Runperch's CMake sources are provided to enable rebuilding with another Qt.
No proprietary restriction may prevent exercise of Qt's LGPL rights.

## GitHub CLI and GitHub Actions runner

`gh` and the GitHub Actions runner are separately installed tools, not vendored
into Runperch. Their trademarks, licenses, authentication and configuration stay
with their respective upstream projects. Runperch invokes only read-only `gh api`
requests. It does not copy or export authentication material.

## Runperch identity

The Runperch logo and interface artwork included in this repository are part of
this project and available under its Apache-2.0 license. No third-party runner
manager code or artwork is included.
