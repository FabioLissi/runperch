# GitHub connection, recent activity and app identity

## Before / after / reason

| Before | After | Reason |
|---|---|---|
| Repeated generic authentication warning | Account identity and repository permission explanation | Authentication is separate from runner-admin authorization |
| No sign-in action in the app | Connect GitHub dialog using the official gh browser/device flow | Avoid manual token copying or inventing OAuth |
| Empty screen once active jobs disappear | Latest server-reported completed job, result and completion time | Preserve useful context without inferring idle or success |
| No Finder/Dock bundle icon | Generated ICNS and bundle metadata, plus runtime application icon | Make the installed development app identifiable |

## Reference conformance

References: [gh auth login](https://cli.github.com/manual/gh_auth_login),
GitHub CLI 2.102 `pkg/cmd/auth/shared/login_flow.go:Login` and
`internal/authflow/flow.go:AuthFlow`, GitHub runner/job REST documentation,
Apple `CFBundleIconFile` bundle convention, `sips`/`iconutil`, and
Qt `QGuiApplication::setWindowIcon`.

| Reference step / acceptance | Implementation / gate |
|---|---|
| Reuse existing gh account; don't read tokens | Read-only `gh api user` identity check; parse login only |
| User explicitly starts browser sign-in | Button starts `gh auth login --web --hostname github.com --skip-ssh-key --clipboard=false` |
| Preserve Git protocol and SSH keys | Do not specify a protocol, request SSH upload or configure Git helpers |
| Device code is temporary and approval belongs to user | Extract only the one-time code in memory; user opens fixed `https://github.com/login/device` |
| No secret CLI output is persisted | Parse a bounded buffer; display fixed status messages and code only, never raw stderr |
| Handle cancel, timeout, failed executable, rejected authorization | Bounded query lifetime and auth-specific cancellation, fake-process/reference transcript tests |
| Verify successful account before claiming connected | Generation-tagged `gh api user` after successful login; pre-login callbacks cannot verify a new login |
| Don't pretend login grants repository roles | Diagnose non-admin repository access; runner availability remains Unknown |
| Use actual completed job records | Exact runner identity and safe URL checks; latest completion timestamp wins |
| Bundle uses a real native icon | Native CTest fails if CFBundleIconFile/resource/ICNS header is missing |

The UI states that it uses GitHub CLI authorization and can change the active
account shared with other gh users. No Runperch OAuth client identity is implied.
GitHub CLI normally uses OS secure storage but may fall back to its own plaintext
configuration if secure storage is unavailable; Runperch does not claim otherwise.
The dialog discloses the official CLI flow's standard repo/read:org/gist scopes;
these are broader than the read-only API operations used for monitoring.
Do not request `admin:org` or attempt to grant a repository role automatically.
When GH_TOKEN/GITHUB_TOKEN is provided by the environment, browser authorization
is unavailable until the user resolves that override.

Normal monitoring never calls the runner-management API. That optional path is
disabled by default and has a separate setting/CLI flag. Account-check generations
prevent a pre-login reply verifying a new login. Active-run lookup is independent
of recent-completed lookup, so new completed workflows do not bury active work.
Workflow listing alone is not claimed as job-detail readability.
An active job is published immediately after its identity matches; it does not
wait for completed-job history or further run queries. Its workflow name comes
from the same matched run. The regression fails on the former four-request path
and passes with two requests for a first-run match.

Native icon conversion belongs under `cmake/macos`, never in shared runtime code.
The current cross-platform UI uses the same PNG resource for its runtime icon.
