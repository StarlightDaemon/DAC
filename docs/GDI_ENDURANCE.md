# Private-desktop GDI/USER endurance — 2026-10-17

The subsequent [attribution and correction record](GDI_ATTRIBUTION.md) removes
tray-icon copy churn and strengthens fixture synchronization and media isolation.
It preserves all four failed cases below; later results do not retroactively
change their resource, generation or shutdown outcomes.

This extends the [independent GDI review](GDI_REVIEW.md) with four continuous,
30-minute **wall-clock** cases: Release/native input, Release/process-local
IME-disabled control, Debug/native input and Debug/control. Each case retains
one DAC host for the entire measurement. The cases run concurrently on four
separate private noninteractive desktops; aggregate process time is not elapsed
wall time. Nothing switches to or operates DAC on the user's input desktop.

No continuing GDI/USER increase was observed, but **all four cases remain
nonpassing**: three crossed strict resource limits, and both Release cases failed
their final generation check before verified graceful shutdown. The separate
fresh full regression suites each passed 30/30. The measured failures are retained
and are not replaced by those shorter passing suites.

## Method and fixed bounds

The evidence-only [fixture](../evidence/gdi-endurance-20261017/endurance-v2.cpp)
includes the current production integration fixtures. It does not change shipping
DAC, ordinary tests or their timeouts. The dedicated
[runner](../evidence/gdi-endurance-20261017/endurance-runner.cpp) differs from the
reviewed ordinary runner only in its outer 1,980-second endurance deadline and
timeout message. It retains private-desktop creation, hidden startup, suspended
launch, inherited-handle restriction, kill-on-close job containment, exit checks
and desktop cleanup. UI commands retain their ordinary two-second bound.

Each case warms up with one held and sixteen rapid focused flyout lifetimes,
then freezes the original settled GDI/USER/handle baseline and native-input USER
transient ceiling. Warmup and measured rapid lifetimes use one lifetime per
synchronous UI command. Each measured wave opens/closes 64 focused flyouts, including
one held for at least one second while the real UI thread pumps messages. Every
tenth wave holds that focused flyout for at least ten seconds. The caller thread
waits; the UI owner does not sleep or run a nested message pump. Holds are logged
as their requested minimum, not an exact stopwatch measurement.

After each wave, the reviewed observer samples for at least one second and
requires 500 ms stable within its nominal five-second bound. Closed-state
sampling then continues every 250 ms through the remainder of a wave interval
of at least fifteen seconds. Sample scheduling includes synchronous UI dispatch
time. Every open/close verifies the enabled Edit's actual focus, window/font
destruction, empty flyout/control ownership and absence of unexpected DAC panels.

The original baseline is never raised. Every closed GDI sample must stay at or below
that baseline; settled USER must stay at or below its original baseline. Native USER
transients may only reach the ceiling established in closed warmup samples.
Control USER is strict at every closed sample. Handles retain the reviewed +4
bound. Violating waves are logged without rebasing, and any violation makes the
case fail. The control first verifies that a deliberately deferred memory DC is
detected, rejected and deleted. Configuration, target generations, empty leases,
disarmed automatic protection and worker shutdown are also checked.

Private bytes and working set are supplementary samples, not an allocator-leak
proof or a new memory-performance threshold. Resource counts are process-local;
these cases do not inventory the system's native object owners.

## Evidence integrity and independent review

All new artifacts are under a separate
[evidence directory](../evidence/gdi-endurance-20261017/). Earlier passing,
failed and investigation evidence is preserved. Builds use the pinned MSVC/SDK
and repository compiler hardening conventions, with optimization/static release
CRT for Release and unoptimized/runtime-check/static debug CRT for Debug. These
are separate evidence executables that include the current DAC source, not
modified shipping binaries.

The capture scripts freeze source/script hashes before/after completed builds,
and source plus executable hashes before/after each complete run. They refuse
overwriting run directories. Completed logs, receipts and manifests are sealed.
The frozen reconciler checks actual modes, baseline/ceiling derivation, all
waves, focus-cycle counts, passing bounds, elapsed duration and completed
shutdown. A separate supplemental timing audit checks actual fifteen-second
intervals, a final measured sample at or beyond thirty minutes, the tightly
bounded final-sample/footer gap, and UTC receipts covering the monotonic measured
interval. Resealed corrupted-copy guards exercise those timing checks. The
final-sample thirty-minute bar is an acceptance check of these actual captures;
it is not a claim that adjacent clock reads in the fixture cannot straddle a
millisecond boundary.

A fresh-context read-only reviewer checked fixture ownership/isolation, resource
oracles, build/run provenance and reporting. The reviewer identified the timing
reconciliation gap above; the independent supplemental audit closes it without
changing running inputs. The final outcome audit also compares the saved derived
report with a freshly reconciled canonical representation before using aggregate
counts; hashing a stale summary alone would not establish that association.

## Actual measured outcomes

All four trajectories contain at least thirty minutes of wall-clock sampling:
**30,528 measured focused lifetimes**, plus 68 separate warmup lifetimes, and
**7,227,733 ms** of aggregate measured process time (120.462 minutes). These are
four concurrent/staggered process intervals, not two hours of elapsed desktop use.
**None of the four cases is a fully passing endurance qualification.**

| Configuration / mode | Measured seconds | Waves / lifetimes | GDI baseline / peak / final | USER baseline / peak / final | Resource-violating waves | Final lifecycle outcome |
|---|---:|---:|---|---|---:|---|
| Release / native input | 1,803.312 | 119 / 7,616 | 57 / 60 / 57 | 57 / 58 / 54 | 1 | Final generation assertion failed; graceful shutdown unverified |
| Release / IME control | 1,804.984 | 119 / 7,616 | 22 / 22 / 22 | 8 / 8 / 8 | 0 | Final generation assertion failed; graceful shutdown unverified |
| Debug / native input | 1,814.687 | 120 / 7,680 | 57 / 60 / 57 | 58 / 59 / 53 | 1 | Completed strict `FAIL`; graceful shutdown verified |
| Debug / IME control | 1,804.750 | 119 / 7,616 | 22 / 25 / 22 | 8 / 9 / 8 | 2 | Completed strict `FAIL`; graceful shutdown verified |

Final GDI/USER counts returned to or below their original baselines. No continuing
increase was observed over these intervals. This is not zero growth at every
sample: Debug/control retained the extra 3 GDI / 1 USER at one final wave boundary
before releasing them by the next, and the native cases had temporary GDI peaks.
All process-handle samples stayed within the unchanged +4 bound. Their
baseline/peak/final values were Release/native 452/453/442, Release/control
238/240/234, Debug/native 456/456/437 and Debug/control 233/235/229. Private bytes
and working-set samples remain supplementary; they do not prove memory-leak freedom.

The Release generation assertions failed after their final measured waves and
before `Shutdown`. The runner contained their process exits, but their owned UI
and workers did not reach the fixture's verified graceful-join footer. The actual
reset causes are unknown. Source review found no generation mutation in flyout
creation, refresh, focus or destruction, and no ordering/race defect in the saved
generation map. Injected observations do not suppress real session, power,
topology or tray notifications, whose normal handlers can reset generations.
That is a possible mechanism, not an attribution of these two failures.

A separately owned [reset probe](../evidence/gdi-endurance-20261017/reset-attribution-probe.cpp)
verified unchanged generations across eight ordinary focused lifetimes. A
synthetic session notification delivered only to its private HWND then produced
Session counter 0→1 and serial 6→9 for three unchanged targets, with unchanged
configuration, no leases/presentations and verified graceful shutdown. No Windows
session lock or power-setting API was called. Its
[log](../evidence/gdi-endurance-20261017/release-reset-probe/runtime.log)
demonstrates attribution for the injected mechanism only.

The [bound outcome report](../evidence/gdi-endurance-20261017/observed-outcomes-bound.json),
[timing audit](../evidence/gdi-endurance-20261017/observed-timing-audit.json) and
[case map](../evidence/gdi-endurance-20261017/final-case-map.json) identify the
actual source/binaries and distinguish measured coverage, completed lifecycle,
graceful shutdown and passing qualification. Failed Release postconditions do not
acquire invented footers or idle-sample totals. Original strict/completed-only
validators remain preserved and reject these failures.

## Preserved failures and transient investigation

Initial scheduling overlapped the two full regression recipes with the first
endurance workloads. That was unsuitable for the shipping host tests: DAC's
singleton and emergency event are SID/session scoped across builds, profiles and
private desktops. One Debug startup returned as a second-instance no-op, followed
by a missing-host lifecycle failure. Those recipes are failed evidence, not
qualification. The final regression driver serializes its invocations, refuses
an existing DAC controller and requires all endurance measurements to finish.
Its guard does not coordinate arbitrary direct CTest runs or a controller
launched later; the lead also explicitly excludes overlapping workloads.

Seven initial/retry endurance attempts with 16-lifetime UI batches hit the
unchanged two-second generic UI dispatch check. Some failures also occurred after
the regression recipes ended. Their sampled resources remained within bounds
before abort. The exact blocking operation was not identified, and contention
is a hypothesis. An abort does not establish a leak or a deadlocked controller.
All those logs, receipts, executables and source snapshots are retained. The
separate V2 fixture keeps the deadline and resource checks while dispatching one
lifetime per command, allowing normal message dispatch between lifetimes and
recording per-lifetime command timing. This is a test workload refinement; it
does not claim to fix a product or explain the earlier batch failures.

Release/native V2 wave 6 and Debug/native wave 64 observed process GDI
**57→60→57**. Debug/control waves 75–76 observed **22→25→22 GDI** and
**8→9→8 USER**, with the increase present at one final wave boundary and released
by the next. The strict test records those waves as failures and continues
without rebasing. A separate
[owned tray probe](../evidence/gdi-endurance-20261017/tray-resource-probe.cpp)
reproduced **20→23→20 GDI** and **8→9→8 USER** in all eight rounds, both for direct
`CopyIcon`/`DestroyIcon` and the real maintenance worker's icon handoff with a
bounded injected delay. Its [log](../evidence/gdi-endurance-20261017/release-tray-probe/runtime.log)
and source/binary snapshots are separately sealed. DAC normally copies an icon
for periodic tray work and destroys it asynchronously; process-wide resource
samples and a UI-thread GDI flush can overlap that ownership. The probe establishes
a matching transient mechanism, not the identity of the objects in wave 6 or
the six original native-input-associated objects. The new control observation
also shows that the new transient pattern is not exclusive to enabled IME.
No permanent allowance is
added, and the original strict qualification validator remains unchanged.

Measured-outcome reporting is separate from passing qualification. It requires
sealed unchanged inputs and full measured intervals, with exact source-bound
outcome classification. A completed `FAIL` requires matching footer, exit code,
receipt and violation counts. The specifically recorded Release final assertion
can establish measured coverage while leaving completed/graceful flags false.
Other missing-footer aborts are rejected. Re-sealed corrupted-copy guards reject
false `PASS`, incorrect violation counts, missing shutdown and an appended fake
graceful footer that tries to conceal the assertion. The original capture
wrapper's `incomplete` status on nonzero exits remains intact.

The first serialized Release configure attempt failed before CTest because the
local pinned Fujin checkout's empty `.git/refs` directory was missing. Git then
discovered the parent DAC repository despite correct Fujin HEAD/config/packed
refs/index files. Only the empty directory was restored. The pinned origin,
commit/tag and clean checkout verified afterward, with those metadata hashes
unchanged. No reset, clean, fetch, ref replacement, reclone or pruning occurred.
The directory's disappearance remains unexplained. The failed configure record
is preserved; copied prior CTest artifacts in that failed wrapper are explicitly
identified as historical in the [note](../evidence/gdi-endurance-20261017/configure-failure-note.txt).

## Remaining qualification

This is bounded hidden flyout-resource endurance. It cannot establish visible
IME composition/accessibility, real input-desktop use, mixed-DPI/topology/session
transitions, physical wake, arbitrary saver behavior or hours/days of operation.
The six original GDI objects' exact type/owner and caching versus other one-time
retention remain unproven. No native-input workaround is added. The earlier
intermittent wake issue remains open, and F01 remains on hold.

## Fresh regression and evidence checks

Both full recipes below ran serially after all endurance/probe processes ended.
The driver refused an existing SID/session DAC controller and held a verification
mutex. Source and all six ordinary binaries per configuration remained unchanged
across the recipes. Shipping host durations (2.30 s Release, 2.17 s Debug) and the
owned-controller lifecycle checks exclude the earlier second-instance no-op.

| Executed check | Actual result | Evidence |
|---|---|---|
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Release -Test` | **30/30 passed**, 125.55 s; 117,665 policy assertions | [Summary](../evidence/gdi-endurance-20261017/regression-final-release-02/full.log), [detail](../evidence/gdi-endurance-20261017/regression-final-release-02/details.log), [JUnit](../evidence/gdi-endurance-20261017/regression-final-release-02/results.xml) |
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Debug -Test` | **30/30 passed**, 132.93 s; 117,665 policy assertions | [Summary](../evidence/gdi-endurance-20261017/regression-final-debug-02/full.log), [detail](../evidence/gdi-endurance-20261017/regression-final-debug-02/details.log), [JUnit](../evidence/gdi-endurance-20261017/regression-final-debug-02/results.xml) |
| `tools/verify-pe.ps1` for both shipping executables | Both passed AMD64 GUI/hardening/import checks; shipping hashes unchanged | [Release](../evidence/gdi-endurance-20261017/pe-release.json), [Debug](../evidence/gdi-endurance-20261017/pe-debug.json) |
| Eight distinct evidence guards | Short waves, unmeasured tail, short receipt, false PASS, wrong violation count, missing shutdown, false graceful footer and changed derived qualification fields all rejected; valid derived control accepted | [Timing](../evidence/gdi-endurance-20261017/timing-guards-01/), [outcome](../evidence/gdi-endurance-20261017/observed-guards-03/results.json), [derived report](../evidence/gdi-endurance-20261017/derived-report-guard-01/result.json) |
| Final reconciliation / preservation | Four measured trajectories, two verified graceful endurance shutdowns, **zero fully passing endurance cases**; **358 previous evidence files** and **12 ordinary binaries** unchanged; existing production/tests/tools unchanged | [Audit](../evidence/gdi-endurance-20261017/final-audit.json), [log](../evidence/gdi-endurance-20261017/final-audit.log), [source archive](../evidence/gdi-endurance-20261017/tested-source-snapshot.json) |
| `git diff --check` | Passed | [Log](../evidence/gdi-endurance-20261017/diff-check.log) |

The [capture/build scripts](../evidence/gdi-endurance-20261017/build-endurance-v2.ps1)
and [private-desktop driver](../evidence/gdi-endurance-20261017/run-endurance-v2.ps1)
are reproducible evidence tools. The final case map selects native/control
`*-01` runs under `release-v2` and `debug-v2`. The driver labels nonzero exits
`incomplete`; the bound report preserves that receipt and separately classifies
the exact recorded outcome. No passing count is borrowed from previous evidence.
Production static analysis was not rerun for this evidence-only extension;
production source remained byte-identical to the separately recorded analyzed
version. PE and full suites above were executed afresh.

Before claiming passing endurance, use an attributed repeat to identify the
generation resets and correlate resource samples with pending/in-flight tray
ownership. Preserve all current failures and strict bounds. Separate real-session
hours/days, visible accessibility/IME, topology/mixed-DPI and physical wake
qualification remain required.
