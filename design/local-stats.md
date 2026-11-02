# Local service statistics

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| CPU, RSS and disk with no uptime or worker evidence | Add service uptime and exact-path worker count from a local process snapshot | Useful activity without runner-management permissions |
| Generic CPU label | CPU average label and source qualification | macOS ps reports a decaying average over up to a minute |
| No matched API job appears unavailable | Show locally observed worker activity independently | Process evidence is useful without inferring GitHub connectivity/idle |
| Metrics parsing embedded in native service adapter | One process-snapshot reader/parser behind macOS adapter | Share validation and reuse from tests/CLI/UI |

References: macOS ps(1), fields pid/ppid/pcpu/rss/etime/comm and -ww; GitHub
[JobDispatcher](https://github.com/actions/runner/blob/main/src/Runner.Listener/JobDispatcher.cs),
which launches Runner.Worker from its bin directory; GitHub's documented local
runner service and _diag monitoring practices.

| Reference step / acceptance gate | Implementation / validation |
|---|---|
| Read a process snapshot with explicit fields | Fixed ps argument list, C locale, no args/environment/credential dump |
| Scope to one service | PID/parent graph rooted at the validated launchd PID |
| Resource units | pcpu / 100 core equivalents, RSS KiB * 1024, etime to seconds |
| Elapsed forms | mm:ss, hh:mm:ss, dd-hh:mm:ss with numeric/overflow validation |
| Identify a worker | Descendant executable canonical path must match this installation's bin/Runner.Worker |
| Process root missing, invalid/truncated data, duplicate PID | Return unavailable stats; never manufacture zero workers or idle |
| Worker present | Local active observation supports Running and blocks conservative IdleOnly Stop even if GitHub still says Idle; only confirmed Stop now may interrupt |
| Worker absent | Report No active worker observed; never infer connected Idle |
| Shared consumers | ServiceObservation feeds the same CLI serializer and real Window |
| Producer contract | Reader tested with actual ps output and its own temporary child process |

CPU is a recent decaying average, not an instantaneous sample. RSS sums resident
memory and can double-count shared pages. Process observations can race exits;
these are timestamped snapshots, not an atomic GitHub health check. No logs,
command arguments, runner credentials or tokens are read by this capability.
