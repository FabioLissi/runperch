# Runperch architecture

The 0.2 GitHub connection, recent-activity and icon additions have their complete
reference conformance table in [authorization.md](authorization.md). Version 0.4
local controls are specified in [local-service-controls.md](local-service-controls.md).
Version 0.5 local metrics are specified in [local-stats.md](local-stats.md).

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Runner health lives in terminals and GitHub | Tray, window and JSON CLI share one monitor | Make existing runners observable |
| Process existence can be confused with availability | Service, connection and job state remain separate | Report only evidence we possess |
| Each platform needs different service commands | One Platform interface; launchd is the first adapter | Keep shared policy portable |
| A pause can silently terminate work | Explicit start/stop; busy/unknown stop needs an immediate-stop confirmation | Avoid promising unsupported drain behavior |
| Brand exists only in mockups | Original Runperch mark and a working desktop UI | Deliver a recognizable independent utility |

## Production practices and conformance

References:

- [GitHub runner monitoring](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/monitor-and-troubleshoot)
- [GitHub service configuration](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/configure-the-application)
- [Runner REST API](https://docs.github.com/en/rest/actions/self-hosted-runners)
- [Workflow jobs REST API](https://docs.github.com/en/rest/actions/workflow-jobs)
- [Qt asynchronous QProcess](https://doc.qt.io/qt-6/qprocess.html)
- [Qt system tray](https://doc.qt.io/qt-6/qsystemtrayicon.html)
- macOS `man launchctl`: `print`, `bootstrap`, `bootout`, GUI service domain.

Explicit deviation: `launchctl print` is documented as diagnostic output, not a
stable API. The macOS adapter conservatively checks its loaded plist path,
program, working directory and PID against the already validated installation.
Missing, duplicate or changed identity fields leave state Unknown and disable
controls. This dependency is covered by a fixture taken from actual macOS 26.5.1
output with names/paths replaced, plus a read-only live adapter check. It must be
revalidated on new macOS versions; it is not advertised as a stable launchd API.

| Reference step / acceptance gate | Implementation | Verification / deviation |
|---|---|---|
| Attach to an already configured official runner | Read only bounded `.runner` and `.service` metadata | BOM, missing, malformed, invalid URL and symlink cases |
| Preserve service user/session and configuration | Native adapter inspects existing LaunchAgent plist | Verify label, directory and exact runsvc.sh before enabling controls |
| Separate local service state from remote connection | Independent fields and timestamps | State contract tests; CLI real local inspection |
| Report online/busy from runner API only when explicitly enabled | Optional management-status setting; off by default | Normal monitoring tests forbid runner API requests; permission failure stays Unknown |
| Identify current job / step from workflow API | Bounded in-progress workflow lookup, exact runner ID matching | Absence of a job never implies Idle; repository-scoped fallback only |
| Query lifecycle asynchronously | Bounded QProcess, no shell and no blocking waits | Failed start, nonzero, timeout and output-cap tests |
| Show icon and menu when tray exists | QSystemTrayIcon with full window fallback | Native macOS smoke; Windows/Linux initially compile-only targets |
| Service stop is explicit | UI Stop now requires confirmation and fresh local Running; conservative API callers retain IdleOnly | No atomic drain claim; local identity/status is rechecked at the write path |
| Avoid changing monitored service on app shutdown | Only terminate Runperch-owned query subprocesses | No runner kill, stop or uninstall on close |

This is not a replacement runner or fleet orchestrator. No installation,
registration, updates, credential export, remote control server, automatic
draining, telemetry, log upload or background service is introduced.

## Layout

`src/core` owns types, state policy, registration validation, subprocess lifecycle,
GitHub observation, catalog persistence and refresh coordination. It uses Qt Core
but no native APIs. `src/ui` and `src/cli` consume the same Monitor. Platform
implementations alone know launchd/systemd/Windows Services and executable search
paths. Windows/Linux adapters report unsupported native inspection until built;
they do not pretend to inspect macOS services.

UI updates are event-driven. Refresh has an overlap guard; each external command
has a deadline and output cap. Poll interval defaults to 15 seconds, configurable
from 5 to 3600 seconds. Command deadline defaults to 8 seconds, command output to
2 MiB, registration metadata to 64 KiB, and workflow detail lookup to 3 recent
active plus 3 recent completed runs. These are operational bounds, not tracking/algorithm thresholds.
An incomplete lookup leaves unknowns explicit.
Observations expire after 30 seconds (`observationFreshnessSeconds` owns that
policy); table labels and control admission share the policy. Service changes
re-read registration before checks and again immediately before the native
request. A per-attachment instance ID rejects callbacks from removed runners.

No subprocess command is interpreted by a shell. Config paths and API fields are
untrusted. Only github.com repository/org URLs are supported in 0.1; enterprise
hosts require a future explicit host adapter. Authentication stays with `gh`.
Permission errors show an access diagnostic, not raw stderr or tokens.

CPU is aggregate process-tree CPU core equivalents (100% = one core). RSS is a
sum over those processes, not unique physical memory. Free disk is the volume
containing the runner installation; no cache cleanup is performed. Job history
records observed job identities locally, not an invented complete account audit.
