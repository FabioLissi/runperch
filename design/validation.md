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
