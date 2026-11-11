# Resource attribution and fixture corrections — 2026-10-18

> Paths under `evidence/` name local development evidence that is not tracked
> in this repository, so they appear as plain paths rather than links.

This follows the [30-minute endurance investigation](GDI_ENDURANCE.md). All
earlier captures, including the four nonpassing cases and the unknown historical
Release generation resets, remain evidence. New captures are in a separate
directory (`evidence/gdi-attribution-20261018/`). Runtime checks use owned,
job-contained processes on private noninteractive desktops. No desktop is
switched and no Windows session is locked by these tests.

The final Release and Debug suites both passed 30/30. Four thirty-minute
endurance repeats completed 30,720 measured lifetimes with zero resource-violating
waves and verified graceful shutdown. **None of the four endurance cases passed
overall:** received session notifications changed all three target generations,
and the unchanged generation postconditions correctly failed. This is evidence
against accumulating resource growth in this measured workload, not a waiver of
generation checks or complete endurance qualification.

## Changes and attribution limits

The pre-fix owned tray probe (`evidence/gdi-attribution-20261018/tray-ownership.cpp`)
reproduced these counts in all eight rounds:

| Phase | GDI | USER |
|---|---:|---:|
| Original closed baseline | 20 | 7 |
| Worker owns one copied tray icon | 23 | 8 |
| Worker plus queued request own two copies | 26 | 9 |
| After 32 pending replacements | 26 | 9 |
| After completion | 20 | 7 |

The pre-fix log (`evidence/gdi-attribution-20261018/release-legacy/legacy-02/runtime.log`)
demonstrates bounded asynchronous ownership, including complete shutdown. It
does not identify the objects in any earlier process, or the six original
native-input-associated objects. No accumulating icon leak was demonstrated.

`Tray()` now borrows the host's immutable normal/active icons. They are created
before maintenance starts, remain unchanged during theme refreshes, and are
destroyed only after `StopMaintenance()` joins the worker. Replacing pending
requests drops borrowed values; it never destroys the host's icons. This removes
the unnecessary per-refresh `CopyIcon` allocations without changing shell-call
ownership, coalescing, epochs, retry/recovery or controller policy. The
[regression](../tests/flyout-fixture.hpp) keeps the original strict resource bounds
while exercising worker entry, queued replacements and result convergence.

The first fresh Release suite failed an existing `NativeWake` wait. That fixture
disables its normal timer, but the worker admits another request only after the
UI consumes the preceding result. `WaitMaintenance()` now calls the real
UI-owner `Maintain()` while waiting, retaining its four-second nominal deadline
and two-second dispatch bound. A new regression obtains a real completed result,
proves that another tray request is gated behind it, then requires progress.
The original timeout did not identify its exact wait stage; this independently
demonstrates and corrects a polling-only progress defect, without inventing a
unique retrospective cause.

A later Debug suite exposed another genuine test failure: GDI stayed at 58,
but USER peaked at 58 against a warmup ceiling of 57, settled at 57 against a
baseline of 56, and handles reached 493 against 487 + 4. The
failed capture (`evidence/gdi-attribution-20261018/regression-debug-02/full.log`)
is preserved. Review found that the native 64-display setup restored default
`media=true`; its control used `media=false`. Real audio/COM work could therefore
overlap the native measurement. This is a concrete fixture mismatch, not proof
that media owned the extra USER object or six handles.

Both ordinary resource modes now establish the same media-disabled policy and
64-display catalog. A harness-only acknowledgment is published under
`observationLock` after actual media work completes for the current epoch.
Before warmup, tests verify configured, effective and copied worker policy,
including per-monitor force-on flags, then require acknowledgment of the disabled
epoch. Old in-flight work cannot satisfy it. The bounded resource observer also
requires handle counts to be stable alongside GDI/USER; its one-second minimum,
500 ms stable interval and five-second observation limit remain unchanged.
There is no new resource allowance, rebaselining, skipped native focus, disabled
shipping IME or suppression of session/power/topology notifications. The media
acknowledgment does not prove all Windows COM/TSF asynchronous activity has ended.

## Generation failure handling

The separate attributed fixture (`evidence/gdi-attribution-20261018/endurance.cpp`)
subclasses only its owned private controller HWND and calls the original window
procedure. It logs generation/serial changes, triggering message type/code,
reset-counter deltas and bounded state counts. It omits raw input, handles,
window titles, application paths and configured names. Absolute tick timestamps
share the baseline's logged starting tick. At most 128 transition records are
emitted; overflow is a failure, rather than silent missing attribution.

Final policy/target/generation mismatches remain failing postconditions, but the
fixture now proceeds to `Shutdown()` and verifies worker/window cleanup before
emitting its failing footer. Earlier assertions and dispatch failures still fail
immediately. An explicit negative-control mode sends a synthetic WTS lock
notification only to the owned private HWND; it never invokes Windows locking or
power APIs. Resource and generation checks remain strict.

The new endurance cases retain three injected targets, matching the preceding
endurance workload; both modes acknowledge their identical media-disabled
policy before warmup. The ordinary full-suite resource cases retain the separate
maximum 64-target coverage. Neither view changes policy merely by selection.

The first attributed attempt received the same sequence in all four processes:
`WM_WTSSESSION_CHANGE` / `WTS_REMOTE_CONNECT` at tick 260848000, then
`WTS_SESSION_UNLOCK` at 260848796. Session resets advanced the three generations
from 4/5/6 to 7/8/9, then 10/11/12. At tick 260849093 the ordinary UI timer accepted
the OS rescan, advancing the topology counter and replacing the three injected
targets with one OS target. The stable selection was no longer eligible; the
next focused-lifetime assertion correctly rejected its disabled Edit.

Each case therefore stopped after four completed measured waves (256 completed
lifetimes, roughly 60 seconds), with a resource-violating fourth wave and no
verified graceful-shutdown footer. These are **failed short trajectories**, not
four thirty-minute runs. The
abort reconciliation (`evidence/gdi-attribution-20261018/aborted-attempts.json`)
binds their actual source, binaries, logs and ordered events. Resource changes
coincided with the session/catalog boundary; it does not identify their object
types. The received notifications attribute these new generation changes, not
the older Release failures. No test action sent those notifications in the
ordinary modes, and no session handler was suppressed or made conditional.
Private desktops isolate these input/window surfaces; they do not virtualize
Windows session notifications or the OS display catalog. The observed replacement
of injected targets is not evidence that physical monitors were moved or changed.

One unchanged repeat was explicitly selected as attempt `02` in a frozen
case map (`evidence/gdi-attribution-20261018/case-map.json`). The selection is
independent of its eventual pass/fail outcome; there is no automatic search for
a passing attempt. The earlier `01` failures remain intact.

All four selected `02` logs received `WM_WTSSESSION_CHANGE` (689), first
`WTS_REMOTE_DISCONNECT` (4) at tick 262091515, then `WTS_SESSION_LOCK` (7) at
262091640. The original handler advanced Session resets from 0 to 1 to 2 and
target generations from 4/5/6 to 7/8/9 to 10/11/12. No catalog replacement was
recorded in these repeats. These received boundaries explain the new final
generation mismatches; their originating Windows activity and the historical
Release resets are not established. No event was filtered and no controller
reset was removed to obtain a passing result.

| Selected case | Measured ms | Waves / lifetimes | GDI baseline / peak / final | USER baseline / peak / final | Resource-violating waves | Overall |
|---|---:|---:|---|---|---:|---|
| Release native (`evidence/gdi-attribution-20261018/release-endurance-v3/endurance-native-02/runtime.log`) | 1,813,531 | 120 / 7,680 | 57 / 57 / 57 | 57 / 58 / 52 | 0 | FAIL: 3 generation mismatches |
| Release control (`evidence/gdi-attribution-20261018/release-endurance-v3/endurance-control-02/runtime.log`) | 1,814,781 | 120 / 7,680 | 22 / 22 / 22 | 8 / 8 / 8 | 0 | FAIL: 3 generation mismatches |
| Debug native (`evidence/gdi-attribution-20261018/debug-endurance-v3/endurance-native-02/runtime.log`) | 1,812,500 | 120 / 7,680 | 57 / 57 / 57 | 56 / 57 / 52 | 0 | FAIL: 3 generation mismatches |
| Debug control (`evidence/gdi-attribution-20261018/debug-endurance-v3/endurance-control-02/runtime.log`) | 1,812,828 | 120 / 7,680 | 22 / 22 / 22 | 8 / 8 / 8 | 0 | FAIL: 3 generation mismatches |

Each case has zero policy failures, two attributed events, no trace overflow and
verified shutdown. Native USER peaks are one above baseline, within the original
warmup ceiling; they are not flat at every sample. Aggregate measured time is
7,253,640 ms across four concurrent processes, not two hours of elapsed testing.
The canonical reconciliation (`evidence/gdi-attribution-20261018/outcome-audit-final.json`)
retains all four `passed: false` outcomes.

## Executed verification

The final full recipes ran serially, with no endurance/probe runtime overlap:

| Check | Actual result | Evidence |
|---|---|---|
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Release -Test` | **30/30 passed**, 128.03 seconds, **117,665 policy assertions** | Log (`evidence/gdi-attribution-20261018/regression-release-03/full.log`), details (`evidence/gdi-attribution-20261018/regression-release-03/details.log`), JUnit (`evidence/gdi-attribution-20261018/regression-release-03/results.xml`) |
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Debug -Test` | **30/30 passed**, 135.83 seconds, **117,665 policy assertions** | Log (`evidence/gdi-attribution-20261018/regression-debug-03/full.log`), details (`evidence/gdi-attribution-20261018/regression-debug-03/details.log`), JUnit (`evidence/gdi-attribution-20261018/regression-debug-03/results.xml`) |
| Owned tray lifecycle probe | Eight rounds per configuration with flat GDI/USER through active/queued replacement and restored baseline; shutdown with pending work verified | Release (`evidence/gdi-attribution-20261018/release-fixed/fixed-01/runtime.log`), current Debug (`evidence/gdi-attribution-20261018/debug-fixed-v3/fixed-01/runtime.log`) |
| Explicit generation failure controls | Both configurations: one attributed Session event, **three generation failures**, zero resource violations, **FAIL / exit 1**, followed by verified complete shutdown | Release (`evidence/gdi-attribution-20261018/release-endurance-v3/attribution-control-01/runtime.log`), Debug (`evidence/gdi-attribution-20261018/debug-endurance-v3/attribution-control-01/runtime.log`) |
| Pinned production static analysis | Exit 0, zero warnings, empty `DEFECTS`, unchanged inputs | Recipe (`evidence/gdi-attribution-20261018/tools/analysis.ps1`), log (`evidence/gdi-attribution-20261018/analysis-02/analysis.log`), XML (`evidence/gdi-attribution-20261018/analysis-02/standalone.xml`) |
| PE hardening/dependency checks | Both shipping executables passed | Release (`evidence/gdi-attribution-20261018/pe-v3-release.json`), Debug (`evidence/gdi-attribution-20261018/pe-v3-debug.json`) |

Release `01` (mailbox wait), Debug `02` (native resource bounds), older build
versions, sandbox desktop-unavailable attempts and their receipts are preserved.
The earlier Release `02` passed, but the source-current table above uses `03`
for both configurations after the media-fixture correction. A later PASS is not
a reclassification of any earlier failure. JUnit's root duration is rounded;
the precise times above come from the sealed CTest summary logs.

The focused patch (`evidence/gdi-attribution-20261018/implementation.patch`)
compares only this task's three implementation/test files with their exact
starting contents. The V3 source archive (`evidence/gdi-attribution-20261018/tested-source-v3-snapshot.zip`)
and entry hashes (`evidence/gdi-attribution-20261018/tested-source-v3-snapshot.json`)
preserve the actual code/tooling and generated headers used by the final builds.

The separate outcome auditor (`evidence/gdi-attribution-20261018/tools/audit.ps1`)
requires exact seal inventories and binary sets, mandatory unique numeric
fields, at least 100 waves and thirty measured minutes within the outer deadline,
actual suite/endurance UTC non-overlap, explicit case identity, source/binary
bindings and consistent failing/passing footers. Missing failure counters never
become zero. Saved derived results must match a fresh reconciliation. These
reporting tools live outside the fixture's frozen input set; they do not alter
runtime observations or the resource oracle.

The final disposable-copy guards (`evidence/gdi-attribution-20261018/guards-02/results.json`)
passed all 12 checks: two valid controls were accepted and ten corrupted controls
were rejected. These cover incomplete seals, empty binary sets, missing or
duplicate counters, missing exit status, forged PASS/derived qualification,
insufficient waves, missing UTC zones and overlapping suite/endurance receipts.
Acceptance of valid evidence means successful reconciliation, not a passing
endurance case.

The first guard attempt exposed a reporting-tool date defect: automatic JSON
date conversion followed by string conversion lost timezone/fraction precision,
so its forged overlap did not represent the intended UTC overlap. The auditor
now reads dates as strings, requires explicit producer UTC timestamps and parses
with `DateTimeOffset`; the guard constructs explicit UTC values. The
failed first guard log (`evidence/gdi-attribution-20261018/guards-01.log`), earlier
scripts and reports remain intact. This correction changed no runtime source,
measurement or threshold.

The final reconciliation verifies all 1,077 earlier evidence files unchanged.
Preexisting implementation work was preserved; only the three focused source/test
files and six existing qualification/architecture records changed in this task.
Shipping build outputs were intentionally rebuilt. No unrelated work was staged,
discarded or committed.

## Qualification boundary

This work does not establish visible accessibility/IME behavior, live
mixed-DPI/topology/session transitions, physical wake, hours/days of operation,
media-enabled whole-host endurance, or production readiness. The intermittent wake report remains open; F01 remains
on hold. Historical reset causes and the individual objects in historical peaks
cannot be recovered from aggregate counts alone.
