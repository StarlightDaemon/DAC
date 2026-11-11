# Independent GDI review — 2026-10-17

The subsequent [resource attribution follow-up](GDI_ATTRIBUTION.md) found that
the ordinary native resource case could re-enable media observation while its
control kept media disabled. That comparison cannot isolate IME as the cause.
The separate native-only probe below remains separate evidence; no historical
six-object owner or unique cause has been established.

A fresh-context `gpt-6.1-sol` reviewer, reasoning high, independently inspected
the GDI evidence, native-resource tests, production cleanup and private-desktop
runner without editing, building or launching runtime fixtures. The lead implemented the fixes
and ran verification. The [review findings](../evidence/gdi-review-20261017/independent-review.txt)
identify three actionable issues; no production leak was demonstrated.

The evidence **associates** the six-object variation with native-input startup;
it does not identify the six objects or prove caching instead of native-system
or one-time application retention. No accumulating DAC flyout leak was observed
within the measured intervals. That qualified conclusion supersedes the earlier
categorical attribution. Production source and native text-entry behavior remain
unchanged.

## Findings and corrections

1. **Repeated focus coverage.** Native-input measured cycles previously skipped
   focus, and an old assertion could accept null focus. Both modes now require
   an enabled, nonnull Edit and `GetFocus()==edit` before destruction. Each
   64-cycle wave contains 63 rapid lifetimes and one focused lifetime held across
   one second of normal UI message pumping. The wait is on the caller thread;
   the UI owner continues dispatching messages. Its two-second dispatch bound
   remains unchanged.
2. **Delayed retention.** Every wave now includes at least one second of
   closed-state sampling, requiring 500 ms unchanged within a five-second
   observation window. The original baseline is retained. GDI growth fails at
   any sample, including growth later released. Settled USER growth fails, and
   native-input transient USER peaks have a ceiling frozen from warmup. The
   IME-disabled control requires zero USER growth at every closed sample.
   An owned UI timer deliberately retains a memory DC after message pumping;
   the actual observer must see increased GDI and reject it, then verify timer
   consumption, DC deletion and restored resources before ordinary measurement.
   Invalid zero GUI queries and failed GDI flushes now fail the test.
3. **Evidence provenance.** The old summarizer could combine historical logs
   with current file hashes. It is now validation-only, checks the original saved
   source/binary hashes, and never rewrites historical proof. A negative control
   [rejects the changed tests](../evidence/gdi-review-20261017/historical-validator-negative-control.log)
   while preserving both original manifests byte-for-byte. New verification
   captures inputs before/after each completed build and test run, captures all
   six tested binaries before/after the full recipe, checks exact executed test
   names/statuses and resource-wave accounting, and refuses changed artifacts.
   Files are created once in distinct run directories. The reviewed test
   translation unit is recompiled from frozen inputs so exploratory object
   timestamps cannot obscure an intervening source edit.

## Transient USER activity exposed by stronger coverage

The first strengthened test correctly failed on a closed-state USER peak one
above its settled baseline, with GDI and handles flat:
[initial result](../evidence/gdi-review-20261017/fixed-targeted-release-01.log).
A private-desktop investigation repeated four waves: warmup USER 60→59, then
each measured wave 60→59 with GDI 58 throughout. The process-owned top-level and
child-window inventory stayed at 13 windows during those transitions:
[trace](../evidence/gdi-review-20261017/user-transient-trace-01.log).
That demonstrates temporary process USER activity, not its exact type/owner;
USER totals also contain non-window objects. Diagnostic mode explicitly refused
qualification and was removed before final verification.

A separate native-only probe uses a dedicated UI thread, matching DAC's early
process-local IME control ordering. Across six held Edit lifetimes its closed
counts plateau at GDI 38 / USER 38 after startup; with that control they remain
GDI 2 / USER 2: [source](../evidence/gdi-review-20261017/native-focus-probe.cpp),
[native input](../evidence/gdi-review-20261017/native-focus-probe-v4.log),
[IME control](../evidence/gdi-review-20261017/native-focus-probe-v4-ime-disabled.log).
This confirms substantial native-input-associated retention without DAC code;
it does not identify either the original six objects or the transient USER item.
Earlier calling-thread and burst variants are retained separately and do not
establish equivalent control conditions or passing qualification.

The final oracle therefore separates **transient peak** from **settled retention**.
It freezes the native-input USER ceiling from closed-state warmup samples,
before measured waves; measured results never enlarge it. The settled baseline
is never raised. Both modes keep zero GDI growth and zero settled USER growth;
the IME-disabled case additionally keeps zero instantaneous closed-state USER
growth. The previous +4 process-handle bound is unchanged. Logs expose baseline,
warmup ceiling, peaks and settled counts separately.

Warmup has one held and 16 rapid lifetimes, separately counted. Each mode then
measures four 64-cycle waves. One-time retention absorbed by warmup and behavior
after the bounded observation interval remain outside the claim.

## Fresh executed verification

The [independent re-review](../evidence/gdi-review-20261017/independent-rereview.txt)
found no remaining material issue in the corrected fixture, timer/DC ownership,
oracle, capture scripts or parser before these final runs. It is separate from
the lead's executed verification below.

| Check | Actual result | Evidence |
|---|---|---|
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Release -Test` | **30/30 passed**, 126.17 s | [Summary](../evidence/gdi-review-20261017/release-final-01/full.log), [detail](../evidence/gdi-review-20261017/release-final-01/details.log), [JUnit](../evidence/gdi-review-20261017/release-final-01/results.xml) |
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Debug -Test` | **30/30 passed**, 134.85 s | [Summary](../evidence/gdi-review-20261017/debug-final-01/full.log), [detail](../evidence/gdi-review-20261017/debug-final-01/details.log), [JUnit](../evidence/gdi-review-20261017/debug-final-01/results.xml) |
| Policy, both configurations | **117,665 assertions each**, 48 combinations and eight virtual hours | Full-suite details above |
| Deferred-retention controls, both configurations | Actual observer saw and rejected **GDI 20→21**, then deletion restored **20** | Full-suite details above |
| `pwsh -NoProfile -File evidence/gdi-review-20261017/run-analysis.ps1` | Exit 0; zero warnings/findings; empty `DEFECTS`; hashes unchanged | [Log](../evidence/gdi-review-20261017/static-analysis.log), [XML](../evidence/gdi-review-20261017/static-analysis.xml), [source hashes](../evidence/gdi-review-20261017/analysis-source-hashes.json) |
| `pwsh -NoProfile -File tools/verify-pe.ps1 -Executable build/release/DAC.exe` and `build/debug/DAC.exe` | Both passed: AMD64 GUI, ASLR/DEP/high-entropy VA/CFG and 12 allowlisted imports | [Release](../evidence/gdi-review-20261017/pe-release.json), [Debug](../evidence/gdi-review-20261017/pe-debug.json) |
| Capture/reconciliation and five corrupted-copy guard cases | Passed; mismatched artifact content, skipped cases, wrong cycle counts, changed source and changed binaries all rejected; original captures unchanged | [Proof](../evidence/gdi-review-20261017/verification.json), [guard log](../evidence/gdi-review-20261017/evidence-guards.log), [guard results](../evidence/gdi-review-20261017/guard-cases-01/results.json) |
| `git diff --check`; final preservation reconciliation | Passed; all twelve production source files unchanged | [Diff check](../evidence/gdi-review-20261017/diff-check.log), [production hashes](../evidence/gdi-review-20261017/production-preservation.json) |

The [capture wrapper](../evidence/gdi-review-20261017/run-verification.ps1) first
performs the pinned build and records its inputs/binaries, then invokes the exact
`-Test` recipe above. The rebuild and full test run must leave them unchanged.
Both configurations passed those comparisons. The
[reconciler](../evidence/gdi-review-20261017/summarize-verification.ps1) validates
the sealed captures instead of taking new hashes as evidence of past execution.

Each case measured **256 focused lifetimes** (1,024 total across both modes and
configurations), including four held/pumped windows, plus separate warmups:

| Configuration/mode | GDI baseline / closed peak / settled | USER baseline / frozen peak ceiling / observed peak / settled | Handles baseline / peak / settled |
|---|---|---|---|
| Release, native input | 58 / 58 / 58 | 57 / 58 / 58 / 57 | 495 / 495 / 495 |
| Debug, native input | 58 / 58 / 58 | 58 / 59 / 59 / 58 | 499 / 499 / 499 |
| Release, process-local IME control | 22 / 22 / 22 | 8 / 8 / 8 / 8 | 239 / 239 / 239 |
| Debug, process-local IME control | 22 / 22 / 22 | 8 / 8 / 8 / 8 | 240 / 240 / 240 |

All four waves in each case had these bounds, with 1,000–1,062 ms of post-wave
observation. There was zero GDI growth and zero settled USER growth. The native
USER transient is explicitly reported, not mislabeled as zero peak growth.
No existing UI-dispatch or wake/performance timeout was relaxed. Production
source and both shipping executable hashes remain unchanged; only tests,
evidence tooling and documentation were corrected.

## Scope and qualification

All GUI/runtime work uses private noninteractive desktops through the existing
job-contained runner. There is no input-desktop switch, physical DDC operation,
live startup/power-plan change, installation or publishing. Existing tracked and
untracked implementation and earlier successful/failed evidence are preserved.
Exploratory compiler failures, the stricter USER failure and timed-out native
burst probes remain investigation evidence rather than passing qualification.

Visible IME composition, accessibility, actual topology/mixed DPI, physical wake
and long-duration endurance still require separate qualification. The earlier
intermittent wake issue remains open. No product workaround disables native input.

The subsequent [extended resource record](GDI_ENDURANCE.md) measures at least
thirty minutes per configuration/mode. It preserves transient resource failures
and Release final-generation assertions, with no continuing GDI/USER increase
observed. It does not turn this earlier short passing record into a claim of
passing endurance or identify the six original objects.
