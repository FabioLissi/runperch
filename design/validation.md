# Initial macOS validation

Version: 0.1.0. Host: macOS 26.5.1 arm64, Qt 6.11.2 open-source dynamic build.

## Evidence

- Core tests: 41 QtTest entries passed, including data rows and init/cleanup;
  no skips on the Mac validation host.
- UI: expiry updates the actual window's table and details without a network
  refresh; isolated Qt settings prevent the tests reading personal catalogs.
- CLI help, successful application-window capture and forced capture failure
  are separate CTest gates.
- Four focused regressions failed before their fixes: stale Idle labeling,
  changed registration at service write, removed/re-added attachment callback,
  and query callbacks during executor shutdown. All then passed.
- Loaded-service mismatched-path regression failed before adding loaded
  plist/program/working-directory validation, then passed.
- AddressSanitizer reproduced stack-use-after-scope in the initial screenshot
  callback. The fixed app produced its window image and exited successfully
  under AddressSanitizer (leak checking disabled for this UI check).
- Read-only live inspection verified an already running LaunchAgent and its
  process tree, plus an active job and current workflow step matched using the
  runner's ID and name. Missing runner-API permissions remained Unknown rather
  than being inferred from job existence.
- Workflow YAML passes actionlint 1.7.12. External actions are pinned to exact
  upstream commits. Public CI builds the shared application and contracts on
  macOS, Windows and Linux; remote results are separate from these local checks.
- Source and filenames were checked for unwanted attribution and instruction
  files; none are included. Assets have no textual attribution metadata.

An independent source review checked the state policy, subprocess lifetimes,
loaded service identity, write-path rechecks, attachment generation, status
expiry, UI fallback behavior and platform boundaries. It also independently
ran the focused regressions and UI expiry test.

## Limits

No actual start/stop was performed on the live runner. Native mutation coverage
is limited to policy/admission tests and command construction/source review;
a disposable runner is needed for a full lifecycle integration test.
Windows/Linux native service adapters remain unimplemented. Cross-platform CI
does not certify native lifecycle behavior on those systems. Symlink rejection
is tested with native filesystem symlinks; hosts without symlink privileges
explicitly skip that one case. launchctl diagnostic parsing is version-qualified.
No signed/notarized or self-contained binary distribution is claimed by this
initial development build.

## 0.2.0 connection and activity update

- Local Release build: 6/6 CTest gates passed; core reports 48 passed, zero
  failures or skips. The added native bundle gate checks the icon metadata,
  resource and ICNS header. Native Cocoa window capture and normal exit passed.
- Independent review ran the focused auth/monitoring regressions: 9 QtTest
  entries passed, zero failures or skips. Source review cleared the changes.
- A new active-job regression failed on the previous four-request path and
  passes on the two-request immediate-publication path. The account-generation
  regression covers delayed pre-login replies, and fake-process authorization
  tests exercise the exact CLI flags without performing real sign-in.
- Read-only live observation with a non-admin account returned the running
  local service, readable job details and the latest successful matched job.
  Default code-path and request-contract tests prove management requests are
  disabled; the live receipt itself does not record every requested endpoint.
- Workflow lint passes. Earlier public macOS/Linux jobs passed, while the
  Windows core gate failed without test output. A persistent QtTest report is
  now printed separately in CI to expose that failure; this is diagnostic
  work, not a claim that Windows is fixed.

Real OAuth approval and live runner service changes remain untested. The
development app still requires locally installed Qt and gh.

## Windows newline correction

Public run 41377009387 passed macOS, Linux and workflow lint. Windows reported
46 passing core entries and one exact-output mismatch: its QTextStream stdout
producer emits CRLF, but the test expected LF. The test now expects the native
Windows bytes under Q_OS_WIN and retains the LF comparison elsewhere; no
production output is normalized or changed. Independent review confirmed the
receipt and ran the focused Mac regression (3 entries passed). Native Windows
confirmation remains pending the next CI run.

## 0.5.0 local service statistics

- macOS Release build: all 7 CTest gates passed. Core reports 67 passed;
  UI reports 9 passed; neither suite skipped or failed. Native Cocoa presence,
  bundle icon, CLI and application capture/failure gates also passed.
- A real ps producer/consumer contract launches an owned temporary child,
  reads its process tree and verifies exact executable-path worker matching.
  It does not start or stop a real GitHub runner.
- Parser coverage includes malformed/truncated snapshots, duplicate PID,
  rooted cycles, invalid elapsed time, overflow, missing root and unrelated
  workers. Missing data stays unavailable.
- The UI regression failed before adding the local activity display. Independent
  review then identified a conflicting GitHub Idle/local-worker Stop admission.
  Its regression failed before the shared guard and passes afterward, including
  the explicit immediate-stop exception and zero-worker/remote-Idle case.
- Independent source review and focused parser, actual ps, Window and control
  tests passed after that correction. Review checked units, privacy, platform
  boundaries and that no observed worker never becomes proof of Idle.
- Live read-only local-only inspection reported a running service, roughly
  2 days 6 hours uptime, 153 MiB aggregate RSS and zero observed workers.
  No GitHub request or monitored-service mutation was performed.
- Previous 0.4.0 source passed all three public platform jobs and workflow lint
  in run 41389246461. Those results do not validate 0.5.0.

The new native statistics adapter is macOS-only; Windows/Linux still have the
explicit unsupported native-service adapter. No real job was launched solely
for this validation. Local worker presence is process evidence, not a GitHub
connectivity check or a source of workflow names/results. CPU is a decaying
average, RSS can double-count shared pages, and service uptime is not job age.
The development bundle still depends on local Qt and optional gh installations.

## 0.6.0 compact panel and resource history

- All 7 local Release CTest gates passed. Core: 69 passed; UI: 15 passed;
  no failures or skips. The UI suite also passed using real Cocoa presentation.
- The initial missing-popover regression failed on 0.5. Independent review
  then found two additional defects: routine refresh mutated the open chooser,
  and the job URL admitted a different job ID. Each received a failing test
  before its fix; both now pass.
- Monitor completion contracts verify one local observation per timestamp, no
  samples from remote callbacks/UI refresh, validated units, bounded retention
  and cleared history on remove/re-add. Chart tests pin timestamp positions,
  zero scale, missing observations, long gaps and process restarts.
- Real Window/popover tests verify runner selection, cancel/accept controls,
  stale-state disabling, attachment identity across confirmation and fresh
  same-job URL opening through the actual URL-handler consumer.
- Native Cocoa caught Qt::Popup dismissal during application deactivation.
  macOS now uses a non-activating NSPanel behind the presentation adapter.
  The gate checks the actual panel type/style, hidden overview/Accessory mode,
  Escape, overview/Dock restore, duplicate foreground notifications and a
  different-app notification. Notification and key events are injected; no
  claim of human physical-click or keyboard acceptance is made.
- Native Cocoa handle access initially crashed the offscreen capture gates.
  A Cocoa backend capability guard fixes both without removing those tests.
- Independent source review and an independent native journey passed after
  those corrections. Native observers only exist while the panel is visible,
  use main-thread delivery and detach on hide/destruction. No live service
  start/stop or new GitHub authorization was performed.
- The README image captures the real Qt panel with scripted example data.
  Workflow lint and whitespace checks pass. Previous 0.5.0 public CI passed
  all three platforms; new 0.6.0 CI results must be verified separately.

Windows/Linux native service metrics remain unimplemented. The shared GUI and
contracts are cross-platform; macOS presentation validation does not certify
those platforms or a self-contained/signed distribution.

## 0.7.0 neutral themes, compact formatting and blue icon

- Final local Release gate: all 7 CTest gates passed. Core: 69 and UI: 20 passed
  without failures/skips; the full 20 UI entries also passed on Cocoa.
- Missing appearance-setting regression failed before implementation. Settings
  Save/Cancel and persistence, System resolution, live existing Window/panel/menu
  and chart updates, long-text scrolling and transparent corners now pass.
- Native rendering caught generic QSS overriding disabled checkbox text and
  the Running color despite its palette reporting the expected color. Failing
  regressions precede fixes; actual rendered pixels now pin status, disabled
  text, chart and theme-tinted sidebar mark in both themes.
- System transition uses the actual QStyleHints producer. Offscreen platforms
  can report Unknown and cannot establish physical OS switching; the resolver
  separately covers Dark/Light/Unknown and explicit-choice precedence.
- Natural-height/top-aligned panel removes fixed-height whitespace. Long work
  names stay within bounded geometry with vertical scrolling and no horizontal
  overflow. Cocoa captures were visually inspected in light and dark modes.
- The actual bundled/runtime icon changed to blue. The old-green regression
  failed before replacement; both blue-family and light-backing checks pass.
  ICNS was regenerated; its actual 64/128 pixel resources were visually checked.
- Independent theme/visual/Settings source checks and final bundled-icon
  review passed. No monitored service or GitHub authorization changed.
- Previous 0.6 public CI passed macOS, Windows, Linux and workflow lint. The
  new 0.7 three-platform results must be checked separately after publication.

The Mac uses Qt 6.11.2. Qt 6.5–6.7 have no setColorScheme API: the shared theme
works through palette/styles, with native chrome following OS appearance. Native
Windows/Linux appearance and human physical theme switching remain unverified.
The app remains a development bundle requiring local Qt, not a signed portable
distribution.

## 0.7.1 shared control rendering and portable test diagnostics

- The Cocoa regression failed on the original overlapping combo menu hint. A
  separate raster regression failed when the styled control lacked its standard
  arrow. Shared paint adapters restore Qt Fusion's existing arrow primitive;
  geometry and actual arrow raster now pass in light/dark, disabled and min/max
  states. No custom input, dropdown, spin-step or accessibility behavior is used.
- Real Settings controls verify arrow clicks, keyboard steps, numeric bounds,
  keyboard and mouse selection, Escape and popup content below the field. The
  real runner chooser stays open without model changes during refresh.
- Local Release CTest: 7/7 passed; core 69 and UI 21 passed without failures or
  skips. Full Cocoa UI: 21 passed. Native nonactivating panel and bundle gates
  remain intact. Light/dark captures of the real widgets were visually checked.
- Previous public 0.7 CI passed macOS/lint and failed UI on Linux/Windows. Linux
  reports an exact-color disabled-text assertion; Windows did not expose its UI
  detail. The raster gate now checks alpha-composited text rather than requiring
  a fully covered glyph pixel, and deliberately rendering magenta must fail it.
  The UI report is now persisted and printed separately, including on Windows.
- Workflow lint 1.7.12 and whitespace checks pass. Native Windows/Linux results
  for this correction must be checked separately after publication; their prior
  UI failure is not silently treated as fixed by a Mac pass.

Independent source/reference/visual review cleared this correction and reran
focused tests on both offscreen and Cocoa backends (5 entries each, no skips).
No live runner service, authentication or stored runner catalog was changed.
The local app is still a development bundle requiring installed Qt.

## 0.8.0 immediate preferences and completed local Linux reproduction

- Public 0.7.1 run 41403721099 passed Mac/lint, failed one UI entry on each
  other platform: Windows had 10 pixels of hidden horizontal overflow; Linux
  rejected disabled text because its LCD rasterizer uses per-channel coverage.
- Rebuilt the exact failing commit in an isolated Ubuntu 24.04 x86_64 container
  with Qt 6.8.3 (matching CI). Its Linux disabled-text failure reproduced. Actual
  evidence showed per-channel LCD edges; both the single-alpha approximation and
  a standalone-QImage surrogate were rejected. The final reference is a real Qt
  checkbox with independently specified expected color and identical widget
  raster context. The actual wrong-magenta consumer remains a rejected mutation.
- The new long-token layout case failed before its fix with 903/1279 pixels of
  overflow at the two tested font metrics. Width-only minimumSizeHint override
  retains natural label width and height-for-width while allowing text to wrap.
  Ignored sizing was rejected because it collapsed labels beside stretches.
- Immediate-preference regression failed before wiring the actual controls to
  their existing owners. Theme, interval and Dock settings persist before Close;
  reopening retains them. Closing never rolls back. Only an actual optional
  management-choice change refreshes GitHub; no-op selections, opening Settings,
  theme/interval/Dock changes do not query or control a runner.
- Final local Mac: 7/7 CTest, 69 core and 23 UI entries, full Cocoa UI 23; no
  failures/skips. Actual light/dark Settings/panel screenshots refreshed.
- Final local Linux container: 5/5 CTest, 57 core and 23 UI entries; no failures
  or skips, using CI's exact Qt version. This validates shared Linux behavior,
  not native Linux service control (still unsupported) or a desktop display.
- Workflow lint 1.7.12 and whitespace checks passed. Windows requires its native
  CI result; the container/Mac passes do not certify Windows. No live runner,
  credentials or account settings were changed by these isolated tests.

Independent final source/reference review cleared 0.8.0 and reran seven focused
entries covering persistence before dismissal/reopen, attached-runner management
query counts and no-ops, long-token wrapping and the real-widget raster reference
with its actual wrong-color mutation. All seven passed without skips.
