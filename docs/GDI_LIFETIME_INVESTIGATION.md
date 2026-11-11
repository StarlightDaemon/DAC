# Flyout GDI lifetime investigation — 2026-10-16

> Paths under `evidence/` name local development evidence that is not tracked
> in this repository, so they appear as plain paths rather than links.

The evidence associates the six-object variation with native-input initialization
after focusing an Edit control. No accumulating DAC flyout leak was observed;
the six objects' ownership and whether they are caching or retained allocations
were not established. The correction is to the resource-test measurement:
observe native-input initialization before taking the baseline, then require
zero GDI and USER growth during repeated flyout lifetimes. A separate fixture
process disables IME locally to isolate DAC allocations. Shipping DAC and the
ordinary focus, draft, theme and DPI tests retain native input behavior.

The subsequent [independent review](GDI_REVIEW.md) found gaps in repeated focus,
post-wave observation and evidence reconciliation. It corrects those tests and
qualifies this attribution. The executed results below describe the original
test version; use the review record for fresh verification and its distinction
between transient USER peaks and settled retention.

All GUI/runtime experiments used `dac-test-runner.exe` on private noninteractive
desktops. No input-desktop switching, physical DDC, startup registration, power
plan changes, arbitrary application execution, installation or deployment was
performed. Existing tracked/untracked work and earlier evidence were preserved.
All twelve production source files are byte-identical to the investigation's
starting point: preservation hashes (`evidence/gdi-lifetime-20261016/production-preservation.json`).

## Cause and evidence

1. Both existing binaries reproduced **52→58 GDI objects**, with no process-handle
   growth: Release baseline (`evidence/gdi-lifetime-20261016/baseline-release-features.log`),
   Debug baseline (`evidence/gdi-lifetime-20261016/baseline-debug-features.log`).
   The per-cycle trace showed one rise followed by a plateau, rather than a
   retained object on each flyout open/close:
   cycle trace (`evidence/gdi-lifetime-20261016/cycle-trace-release.log`).
2. The exact six-object rise also occurred while the UI owner **slept without
   recreating a flyout**: 53→59, followed by 58 after deleting the flyout font.
   Both subsequent resource waves stayed at 58:
   idle experiment (`evidence/gdi-lifetime-20261016/idle-before-cycles-release.log`).
   A plain first flyout without the focus exercise remained at 23 throughout its
   idle interval: initial control (`evidence/gdi-lifetime-20261016/initial-idle-release.log`).
3. With the normal native-input path, focusing the hidden Edit triggered delayed
   GDI/USER initialization. The owner thread had `IME` and `MSCTFIME UI` windows;
   no settings, map or inspector window had opened accidentally:
   native-input stages (`evidence/gdi-lifetime-20261016/stage-ime-enabled-release.log`).
   Calling `ImmDisableIME(-1)` **before initialization in the fixture process**
   removed those windows and the delayed allocation. Its idle stages stayed flat:
   process-local IME control (`evidence/gdi-lifetime-20261016/stage-ime-disabled-release.log`).
   Microsoft documents the process/thread scope and required early call timing
   for [ImmDisableIME](https://learn.microsoft.com/en-us/windows/win32/api/imm/nf-imm-immdisableime).
4. A minimal program containing only hidden native windows, a focused Edit,
   normal message pumping and COM initialization independently exhibited delayed
   native-input allocation followed by a repeatable plateau. Its counts differ
   from DAC's (33→40 during first focus, then 40 on subsequent focused windows):
   owned native probe source (`evidence/gdi-lifetime-20261016/edit-focus-probe.cpp`),
   probe result (`evidence/gdi-lifetime-20261016/edit-focus-probe.log`).
5. Temporary read-only handle inventories found no six new retained public
   font/bitmap/DC handles across the exact rise:
   inventory (`evidence/gdi-lifetime-20261016/handle-inventory-v4-complete-release.log`).
   This Windows-build-specific diagnostic is supporting evidence, not a portable
   test oracle. The first legacy query returned null; guarded retries used the
   available read-only exported table. Diagnostic timeouts are preserved. None
   of that instrumentation, its undocumented APIs, environment switches or
   diagnostic command modes remains in production or the final test source.

Together the idle experiment, native-input intervention, native-only reproduction
and sustained lifetime tests strongly associate the variation with native-input
startup/retention. They do not prove the ownership of the six objects or exclude
one-time DAC retention triggered by native input.
The private Windows allocation stacks and individual internal objects were not
identified. This is not a claim that every Windows input component is leak-free,
nor that aggregate process counts prove general application leak freedom.
[GetGuiResources](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguiresources)
reports process totals; those totals include native subsystem activity.

## Regression changes

- `tests/flyout-fixture.hpp` replaces the +8 GDI allowance with a bounded settling
  phase (at least one second, 500 ms unchanged, maximum five seconds), then four
  measured 64-cycle waves after 16 warmup cycles. Each wave must retain **zero**
  GDI/USER objects. The existing +4 process-handle bound remains.
- Every cycle still checks successful window/font destruction, hidden state,
  repeated-show reuse and empty ownership registrations. It now also rejects
  accidentally opened settings/map/inspector/background-editor surfaces.
- The `flyout-resource-control` CTest group uses a fresh fixture process, disables
  IME only there, and focuses the Edit on each of its 256 measured cycles. This
  tests DAC allocation/deletion independently of native input startup.
- UI work is split into 16-cycle batches. The existing two-second UI dispatch
  timeout and existing wake/performance assertions are unchanged.
- `imm32` is linked only to `platform-tests`; DAC production source/imports and
  native text-entry support are unchanged. No test uses the live input desktop.

The exploratory logs also preserve an unpumped native-edit experiment terminated
by the private runner's 120-second deadline and unsuccessful inventory attempts.
These are investigation outcomes, not passing qualification. Older labeled
inventory runs could execute the preceding diagnostic binary while a build was
finishing; `handle-inventory-v4-complete-release.log` is the reconciled completed
build. Final test evidence below uses completed builds exclusively.

## Final executed verification

| Check | Actual result | Evidence |
|---|---|---|
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Release -Test` | **30/30 passed**, 104.66 s | Summary (`evidence/gdi-lifetime-20261016/release-full-01.log`), detail (`evidence/gdi-lifetime-20261016/release-details-01.log`), JUnit (`evidence/gdi-lifetime-20261016/release-results-01.xml`) |
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Debug -Test` | **30/30 passed**, 113.03 s | Summary (`evidence/gdi-lifetime-20261016/debug-full-01.log`), detail (`evidence/gdi-lifetime-20261016/debug-details-01.log`), JUnit (`evidence/gdi-lifetime-20261016/debug-results-01.xml`) |
| Policy, both configurations | **117,665 assertions each**; 48 input/media/mute/poll combinations and eight virtual hours | Full-suite detail above |
| `pwsh -NoProfile -File evidence/gdi-lifetime-20261016/run-analysis.ps1` | Exit 0; zero warnings/findings; empty `DEFECTS`; production hashes unchanged | Log (`evidence/gdi-lifetime-20261016/static-analysis.log`), XML (`evidence/gdi-lifetime-20261016/static-analysis.xml`), hashes (`evidence/gdi-lifetime-20261016/analysis-source-hashes.json`) |
| `pwsh -NoProfile -File tools/verify-pe.ps1 -Executable build/release/DAC.exe` and `build/debug/DAC.exe` | Both passed: AMD64 GUI, ASLR/DEP/high-entropy VA/CFG and 12 allowlisted Windows imports | Release (`evidence/gdi-lifetime-20261016/pe-release.json`), Debug (`evidence/gdi-lifetime-20261016/pe-debug.json`) |
| `git diff --check`; source/evidence reconciliation | Passed; only the intended two existing test source files changed; all production sources preserved | Diff check (`evidence/gdi-lifetime-20261016/diff-check.log`), manifest (`evidence/gdi-lifetime-20261016/verification.json`), changed test hashes (`evidence/gdi-lifetime-20261016/changed-existing-source-hashes.json`) |

Each row below contains **four 64-cycle measured waves**, after 16 warmup cycles
and bounded settling. Every measured wave had these same closed-state peaks:

| Configuration and mode | GDI baseline → peaks | USER baseline → peaks | Handles baseline → peaks |
|---|---|---|---|
| Release, native input | 58→58 | 57→57 | 469→469 |
| Debug, native input | 58→58 | 56→56 | 467→467 |
| Release, process-local IME control with focus each cycle | 22→22 | 8→8 | 238→238 |
| Debug, process-local IME control with focus each cycle | 22→22 | 8→8 | 239→239 |

There were **1,024 measured cycles** across these four cases, plus warmups. No
test failure or timeout limit was relaxed to obtain the final passes. The
misleading baseline/+8 allowance is removed, accidental-surface ownership checks
are added, and every temporary diagnostic hook was removed. No production fix
was justified by the investigation. The shipping executable hashes remain
`16d8b0553e624270a009d6db9d7b00d451f210b4e69996dc3fc48baead634bb7`
(Release) and
`5b2b4c8b914a44bc51d60bfe2ec600fdcb8d8febc3631d345fec41cf09ec697e`
(Debug), matching the preceding feature batch.

## Qualification limits

This explains the observed timing discrepancy on Windows 10.0.26300 with the
installed native input stack; precise allocation ownership remains unproven.
It does not establish visible IME composition,
screen-reader acceptance, mixed-DPI/topology behavior, physical wake reliability,
long-duration endurance or production readiness. The existing
[qualification checklist](QUALIFICATION.md) and intermittent wake investigation
remain applicable. No product input method was disabled as a workaround.
