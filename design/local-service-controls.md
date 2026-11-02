# Local service controls with explicit immediate stop

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Stop requires GitHub-confirmed Idle | User-confirmed Stop now can stop a locally running service | The user explicitly chose immediate stop even when busy/unknown |
| Local actions wait for GitHub rechecks | Start and confirmed Stop recheck local state only | API delay/permission failure must not block local service management |
| One refresh flag holds every control | Local and remote refresh progress are tracked separately | Fresh local evidence remains usable during remote observation |
| Stop request is followed by another full API cycle | Publish a fresh local inspection after the request | Distinguish accepted command from observed service state |
| Remote running job overrides a stopped local service badge | Fresh local Stopped takes precedence; remote job remains separate | Server job state may lag a local stop |

References: macOS launchctl bootstrap/bootout/print; GitHub's official self-hosted
runner service configuration; Qt asynchronous QProcess and QMessageBox.

| Reference step / gate | Implementation / validation |
|---|---|
| Control an existing user-owned service | Existing validated LaunchAgent adapter; no installation or plist edits |
| Admit Start only with fresh local Stopped | Shared canControl; re-inspected at shared Monitor write path |
| Admit immediate Stop only with explicit confirmation and fresh local Running | Explicit StopAuthorization; UI warning defaults to Cancel |
| Do not infer idle from local process existence | Job/connection remain independent observations |
| Recheck identity immediately before mutation | Registration checks in Monitor and plist/loaded identity in native adapter |
| No shell interpolation | Existing bounded executor invokes launchctl with an argument list |
| Record command outcome and actual state separately | Fresh local inspection, then report the command outcome |
| Preserve conservative callers | Default Stop path still requires GitHub-confirmed Idle |
| Rejected operation, stale/missing/changed identity | Admission, write-path and UI cancellation regressions |
| No production runner mutation in automated unit tests | Fake native environment for policy; live service inspection is read-only |

The user knowingly authorizes Stop now to interrupt a running job. This is not
a drain/finish-current-job operation, nor does it require GitHub administrator
access. Native Windows/Linux service adapters remain unavailable.

## Validation

- Local Release: seven CTest gates passed; core 54 QtTest entries and UI eight
  entries passed without failures or skips. New local-control regressions failed
  before the policy change. Independent focused core/UI review also passed.
- The opt-in native helper passed a real launchd cycle: initial Stopped, Start
  observed Running with a PID, Stop observed Stopped, zero GitHub requests,
  cleanup complete and no errors. Its only service is a temporary sleep fixture;
  this does not measure actual GitHub job cancellation behavior.
- Read-only inspection of the existing development runner confirmed Running,
  a local PID, available controls and process metrics. That runner was not
  started or stopped during validation.
- Source/workflow lint checks passed. Cross-platform CI validates shared code
  separately; native Windows/Linux service control remains unsupported.

The opt-in helper is built on macOS but not registered as an ordinary CTest:
`build/runperch_macos_service_smoke --run-disposable-service-test`. It exclusively
creates its UUID-named plist, uses the production guarded Stop path for cleanup,
and retains/reports the fixture if identity changes or an outcome is uncertain.
