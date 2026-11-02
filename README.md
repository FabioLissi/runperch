<p align="center"><img src="assets/app-icon.png" width="100" height="100" alt="Runperch bird and R logo"></p>

<h1 align="center">Runperch</h1>

<p align="center"><strong>Your runners, in view.</strong></p>

<p align="center">
  <a href="https://github.com/FabioLissi/runperch/actions/workflows/ci.yml"><img src="https://github.com/FabioLissi/runperch/actions/workflows/ci.yml/badge.svg" alt="Build and test status"></a>
  &nbsp; <a href="LICENSE">Apache 2.0</a>
  &nbsp; · &nbsp; <a href="#screenshots">Screenshots</a>
  &nbsp; · &nbsp; <a href="#build-and-run">Build and run</a>
</p>

Runperch is a small desktop monitor for existing GitHub Actions self-hosted
runners. Attach a configured runner, see its local service and GitHub job state,
and open the current job without hunting through terminals.

## Features

- **Appearance:** System, Light and Dark, with shared neutral color, typography
  and spacing tokens across panels, controls, menus and charts.
- **Runner overview:** one row per installation, details for the selected runner.
- **Current work:** workflow, job, reported step, elapsed time and GitHub link.
- **Local observations:** process-tree CPU average, aggregate RSS, free disk,
  service uptime and worker activity, without GitHub sign-in or admin access.
- **Menu bar / tray:** compact runner picker, local status and uptime, current
  work, Start/Stop and Open current job, with an overview/action-menu fallback.
- **Resource graphs:** bounded 15-minute CPU-average and RSS history; missing
  observations and service restarts remain visible gaps.
- **Local history:** a bounded list of jobs observed while Runperch runs.
- **Connect GitHub:** browser/device sign-in through GitHub CLI, with account identity and no token copying.
- **Recent results:** latest matched completed jobs and their server-reported outcomes.
- **Explicit local controls:** start a stopped service or confirm **Stop now**,
  using fresh local checks without requiring GitHub administration access.
- **JSON CLI:** the same monitor, validation and policy for scripts.
- **Honest status:** unknown or expired evidence stays Unknown. No invented
  completion percentages or finish-current-job guarantees.

## Screenshots

Click a thumbnail to see the full image.

<table>
  <tr>
    <td width="50%" align="center">
      <a href="assets/screenshots/macos-light-popover.png"><img src="assets/screenshots/macos-light-popover.png" width="300" alt="Runperch light compact panel with blue CPU and violet memory graphs"></a>
      <br><strong>Light · compact runner panel</strong>
    </td>
    <td width="50%" align="center">
      <a href="assets/screenshots/macos-dark-popover.png"><img src="assets/screenshots/macos-dark-popover.png" width="300" alt="Runperch graphite dark compact panel"></a>
      <br><strong>Dark · compact runner panel</strong>
    </td>
  </tr>
</table>

Actual version 0.7.0 Qt interface, captured with scripted example data.
[Overview](assets/screenshots/macos-dark-overview.png) and
[appearance settings](assets/screenshots/macos-dark-settings.png) use the same
shared tokens. Public screenshots contain no personal runner configuration or
credentials.

## Status

macOS is the first native adapter. The core, GitHub adapter, UI and CLI are
platform independent. Windows/Linux currently build with an explicit unsupported
native-service adapter; native service controls are **not implemented** there.

| Platform | Shared app and CLI | Native service integration |
|---|---|---|
| macOS | Built and tested locally | Existing user LaunchAgents through launchd |
| Windows | Included in the CI build matrix | Not implemented yet |
| Linux | Included in the CI build matrix | Not implemented yet |

Runperch does not install/register runners, move working directories, modify
LaunchAgents, change signing/keychain sessions, upload logs or collect telemetry.
It never reads the runner's `.credentials` files.

## Build and run

Requires C++20, CMake 3.24+, and the open-source Qt 6.5+ Core/Gui/Widgets/Test
modules. GitHub observations use the official `gh` CLI and its existing login.

Clone the public repository:

```sh
git clone https://github.com/FabioLissi/runperch.git
cd runperch
```

With the dependencies installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On macOS, install Qt with `brew install qtbase` and, if CMake needs it, add
`-DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"` to configuration. Use
`brew install gh` for GitHub access. Use **Connect GitHub** in the app or
`gh auth login` in Terminal. This builds a development
app, not a self-contained signed/notarized distribution.

```sh
open build/runperch.app
build/runperchctl --runner /path/to/actions-runner --local-only
build/runperchctl --runner /path/to/actions-runner
```

Use **Add existing runner** to select a configured installation. macOS suggests
top-level `~/actions-runner*` directories on first launch, asking before saving
them. Other locations are supported through the folder picker or `--runner`.
Normal monitoring uses local service observations and readable GitHub job APIs;
it does not request runner-administration access. Active runs and recent
completed runs are inspected independently, each bounded to three workflows
and the first 100 jobs per workflow. Jobs must match runner ID and name.
No match is **Unknown**, never proof that the runner is idle.

The runner-management status endpoint is optional, disabled by default, and
available through Settings or CLI `--runner-status`. It needs additional role
and token permissions. Organization-scoped registrations have no repository
for job lookup, so their default view provides local service observations.
Enterprise hosts are not supported yet.

Browser authorization uses GitHub CLI's standard repo/read:org/gist OAuth scopes;
these are distinct from repository administrator roles. Signing in can change
the active gh account shared with other applications. GitHub CLI normally uses
OS secure storage, but may fall back to its own plaintext configuration when
secure storage is unavailable. Runperch does not receive or persist the token.

### CLI examples

```sh
# Discover platform-suggested installations.
build/runperchctl --discover

# Monitor several installations in one read-only invocation.
build/runperchctl --runner /path/to/runner-one --runner /path/to/runner-two
```

JSON includes observation timestamps and only the metrics available. A
successful query is not a health verdict: inspect the returned states.

## Controls and status

Choose **Settings → Appearance → System / Light / Dark**, then **Save**.
System follows the platform's appearance; explicit Light/Dark remains your
choice. Theme-only changes do not query GitHub or change services. Qt6.8+ also
requests matching native chrome; Qt6.5–6.7 retain OS-native chrome while the
shared Qt interface uses the selected theme.

- Click the menu-bar/tray icon for a compact runner panel. Choose a runner to
  see its live state and recent CPU/memory samples. Right-click for the action
  menu; **Open overview** restores the full window and Dock icon.
- Graphs keep at most 180 samples over 15 minutes, only in memory while the app
  is open. They use actual timestamps and gaps, not fabricated intermediate data.
- **Open current job** requires a fresh active job observation; local worker
  presence alone provides no job URL. Completed jobs remain in History.
- Service, GitHub connection and job state are separate observations.
- Expired observations are unknown; process existence does not prove online.
- Start/stop use the existing user LaunchAgent, with identity checked again.
- Start requires a fresh local Stopped state. **Stop now** requires a fresh
  local Running state, an explicit confirmation with Cancel selected by default,
  and another local identity/status check before acting. It can interrupt a job
  and does not wait for that job to finish.
- Local controls do not wait for GitHub requests or require a GitHub administrator
  role. The service PID and local state are displayed separately from GitHub job
  observations. Local process existence never proves GitHub connectivity or idle.
- A service request is followed by a local inspection; a rejected command is
  reported as an error, rather than being assumed successful.
- No controls automatically run when the app launches, closes or refreshes.
- On macOS, the red close button leaves Runperch in the menu bar and hides its
  Dock icon. Choose **Open Runperch** in the menu-bar menu to restore both the
  window and Dock icon. Disable **Hide from Dock when the window is closed** in
  Settings if you prefer to keep the Dock icon.
- Without an available tray, closing the window exits the monitor normally.
  **Quit Runperch** always closes the monitor and leaves runners untouched.
- CPU is a recent process-tree average in core equivalents; RSS is a sum,
  including shared pages. Service uptime is the local service process age.
- A matching local worker reports active work. No active worker observed does
  not prove GitHub connectivity or idle; job names and steps use optional
  read-only GitHub observations. A locally observed worker blocks conservative
  stop even if GitHub still reports Idle.
- History contains only jobs observed by this app, not the complete GitHub audit.

Settings and observed-job history use Qt's per-user application settings. Raw
runner logs, subprocess stderr, tokens and credentials are not persisted.
History retains at most 200 observed jobs. **Open logs folder** opens the local
runner diagnostics directory; Runperch does not upload its contents.

## Development

See [architecture and reference conformance](design/architecture.md).
The UI and CLI share the same monitor, validation and control policy. OS calls
belong exclusively behind `src/platform/platform.h`. New native adapters must
pass shared contracts plus real service integration checks on their own OS.
Never exercise stop/start tests against someone else's active runner.

`runperch --screenshot /path/window.png --runner /path/runner` captures only the
app's own window after its first observation and exits, for UI smoke checks.

Screenshot mode uses isolated settings. Public images should use an empty
first-launch state or sample configuration; inspect images for account names,
local paths and job details before publishing them.

## License

Runperch is [Apache-2.0](LICENSE). See [NOTICE](NOTICE) and
[third-party notices](THIRD_PARTY_NOTICES.md). Qt retains its own licenses;
Runperch uses the open-source/community build with dynamic linking. No upstream
runner-manager source code is included.
