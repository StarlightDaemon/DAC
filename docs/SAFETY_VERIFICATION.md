# DAC-SAFETY-01 local verification

Candidate source: `afcabcfc34a851dbbf400a98cc390e1fb3845f22` (clean tree).
Execution date: 2026-11-24. These are development receipts, not live installation
or display qualification.

Read [REAL_HARDWARE_SAFETY](REAL_HARDWARE_SAFETY.md) for the trust boundaries,
tool interfaces, recovery procedure, installation steps, progressive field matrix
and machine-specific authorization checklist.

## What this change contains

Only new files: the runbook and this note, `Safety.Common.ps1` (pinned candidate
identity, safe local reads, state inventory, 28-file comparison and redacted
evidence helpers), `safety-preflight.ps1`, `safety-verify-installed.ps1`,
`safety-observe.ps1`, `test-safety.ps1` and `test-safety-observer.ps1`. No
production source, installer, package manifest, existing test inventory or CI
control changes. The safety tooling is outside the 28-file shipping allowlist.

The tools replace an earlier harness that pinned a pre-rebase 19-file installer.
They now pin the current candidate (identities in the runbook) and take the
payload count from one pinned value instead of repeating it.

## Commands and outcomes

| Check | Result and practical limit |
| --- | --- |
| CI `Release` phase on the candidate commit | Configure, build and the reviewed 30-test suite passed with zero failures, errors or skips. Private-desktop and simulated-power fixtures only. |
| CI `Analyze` phase | Production analysis passed with zero defects. |
| CI `Package` phase | Independent Release builds had identical bytes; PE properties, package regression, package creation and normalized archive verification passed. |
| CI `Installer` phase | Pinned Inno Setup 7.1.0 verified; eight privacy-boundary checks passed; two independent Setup builds were byte-identical; both verified with all 28 decoded payload files; runtime privacy regression and the 48-check integrity regression passed. Fixture Setup only; no installation. |
| `test-safety.ps1` against the candidate Setup and ZIP | 33 checks passed: canonical acceptance; wrong Setup, receipt, checksum and ZIP rejection; all 28 payloads; installed-tamper and extra-file preservation; path and reparse rejection; redaction; permission failure; unchanged host state; foreign process survival; incomplete baseline rejection; syntax and inactive-tool surface. No product launched. |
| `test-safety-observer.ps1` on the existing private-desktop runner | 4 checks passed: wrong start time and injected foreign owner rejected; identity-matched host exit observed through a retained handle; production state unchanged. Fresh test profile, bounded 12-second runtime, no presentation and no DDC command. |
| CI `Audit` phase on the final worktree | Publication-privacy audit passed with zero findings, including history from the publication root, the worktree, the portable package and the shipping Setup. |

The CI `Debug` phase and the CI-driver regression (`test-ci.ps1`) were not rerun
for this change; hosted CI covers them on the pull request. Earlier October results
for the old candidate are historical and are not carried forward.

## Evidence and remaining gates

Build outputs and private fixtures stay in ignored folders and are not committed.
The candidate Setup, ZIP and receipts are kept outside the repository by the
owner's workspace; the runbook's re-pinning section says how to refresh them.

The tests ran on the development host, not on the three-display qualification
machine. Preflight was exercised only through the regression script's read-only
host observations; it was not run with `-Capture` against any real machine.

No shipping Setup execution, physical screen obstruction, login registration,
production preference modification, physical DDC, restart, external recovery
configuration or automatic main-PC termination was performed. Inno fixture
success does not establish physical Installed Apps, Explorer or tray behavior.
No automatic independent recovery channel is demonstrated. Manual/off-device
recovery and operator-confirmed visible outcomes remain mandatory; all-covered
wake and physical monitor power remain unqualified.
