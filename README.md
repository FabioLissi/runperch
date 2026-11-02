# Runperch

**Your runners, in view.**

Runperch is a small desktop monitor for existing GitHub Actions self-hosted
runners. Attach a configured runner, see its local service and GitHub job state,
and open the current job without hunting through terminals.

![Runperch interface concept](assets/ui-concept.png)

The image above is the design concept with sample data. The first implementation
provides the runner table, selected-runner details, process metrics, observed-job
history, settings, tray menu and read-only JSON CLI. It does not invent progress
percentages or job completion when GitHub has not reported them.

## Status

macOS is the first native adapter. The core, GitHub adapter, UI and CLI are
platform independent. Windows/Linux currently build with an explicit unsupported
native-service adapter; native service controls are **not implemented** there.

Runperch does not install/register runners, move working directories, modify
LaunchAgents, change signing/keychain sessions, upload logs or collect telemetry.
It never reads the runner's `.credentials` files.

## Build

Requires C++20, CMake 3.24+, and the open-source Qt 6.5+ Core/Gui/Widgets/Test
modules. GitHub observations use the official `gh` CLI and its existing login.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On macOS, install Qt with `brew install qtbase` and, if CMake needs it, add
`-DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"` to configuration. Use
`brew install gh` and `gh auth login` for GitHub access. This builds a development
app, not a self-contained signed/notarized distribution.

```sh
open build/runperch.app
build/runperchctl --runner /path/to/actions-runner --local-only
build/runperchctl --runner /path/to/actions-runner
```

Use **Add existing runner** to select a configured installation. macOS suggests
top-level `~/actions-runner*` directories on first launch, asking before saving
them. Other locations are supported through the folder picker or `--runner`.
Monitoring does not require runner administration privileges; GitHub's runner
availability API does require additional permissions. If unavailable, Runperch
can still find an active repository job by exact runner ID/name, but absence of
a matching job is **Unknown**, never proof that the runner is idle. The fallback
examines up to three recent active workflows and the first 100 jobs in each.
Organization-scoped registrations need runner-read permissions; repository-job
fallback is not available for them. Enterprise hosts are not supported yet.

## Controls and status

- Service, GitHub connection and job state are separate observations.
- Expired observations are unknown; process existence does not prove online.
- Start/stop use the existing user LaunchAgent, with identity checked again.
- Stop is enabled only after a fresh GitHub Idle observation, requires explicit
  confirmation and rechecks status. A new job can race the check: this is
  **service stop**, not guaranteed finish-current-job draining.
- No controls automatically run when the app launches, closes or refreshes.
- Closing the window leaves its tray entry running where available. **Quit
  Runperch** closes the monitor and leaves runners untouched.
- CPU is process-tree core equivalents; RSS is a sum, including shared pages.
- History contains only jobs observed by this app, not the complete GitHub audit.

Settings and observed-job history use Qt's per-user application settings. Raw
runner logs, subprocess stderr, tokens and credentials are not persisted.

## Development

See [architecture and reference conformance](design/architecture.md).
The UI and CLI share the same monitor, validation and control policy. OS calls
belong exclusively behind `src/platform/platform.h`. New native adapters must
pass shared contracts plus real service integration checks on their own OS.
Never exercise stop/start tests against someone else's active runner.

`runperch --screenshot /path/window.png --runner /path/runner` captures only the
app's own window after its first observation and exits, for UI smoke checks.

## License

Runperch is [Apache-2.0](LICENSE). See [NOTICE](NOTICE) and
[third-party notices](THIRD_PARTY_NOTICES.md). Qt retains its own licenses;
Runperch uses the open-source/community build with dynamic linking. No upstream
runner-manager source code is included.
