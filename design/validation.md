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
