# F16 inspector, F04 display map and F11 running-application rules

Work date: October 16, 2026, America/Denver. This batch preserves the existing
uncommitted F02/F03/F05 implementation and its evidence. No reset, clean, broad
staging, install, deployment, publication, live startup registration or physical
DDC operation was performed. F01 remains **on hold**; F06 is outside this batch.

## Delivered surfaces

- **Display inspector:** tray → Diagnostics → Display inspector / OS-only
  rescan; also Display controls → Inspect and map → Inspect selected display.
  A native display list selects local read-only details: current friendly name,
  identity/source/OS connection and rotation, signed desktop bounds, eligibility,
  protection/reason/deadlines, saved versus effective hardware permission, cached
  helper outcome, fault/quarantine freshness and pending recovery. Local details
  can contain identities and configured application paths.
- **Display map:** tray → Visual display map; also Display controls → Map and
  inspector → Map. Monitor rectangles use signed Windows desktop **pixel** bounds
  in the existing PMv2 process. A single aspect-preserving transform retains
  negative origins, portrait orientation, gaps and overlap. Individual display
  DPI does not distort topology; the native UI scales to its window DPI. The
  legend, textual states, selected marker and parallel native list provide routes
  independent of color. Selecting only changes UI selection, never controller
  input, idle histories or Windows display layout. Stable IDs survive reorder;
  a disconnected selection remains visibly removed until explicitly changed.
- **Background rules:** Advanced settings → Profiles / application rules → Add
  background running rule / Edit selected background rule. Choose an executable,
  enablement, and either explicit global scope or one or more stable display
  targets. Disconnected assignments remain visible when editing. Apply to the
  parent rules draft, Apply rules to the main draft, then Save. Cancel discards
  the nested edit. Delete uses Remove selected rule. Capturing a profile includes
  its rules; loading a profile allows editing and recapturing them.

Full executable paths are the default. Explicit filename-only matching is broader
and includes other applications sharing that filename. No application is launched
by adding a rule. No regex, script, command line, injection or foreground-window
attribution is used. The rules inhibit DAC automatic protection only; they do
not prevent Windows sleep or grant hardware authority.

## Contracts and decisions

The UI thread remains the sole controller and accepted-catalog owner. The map,
inspector and flyout read cached state; opening, repainting and refreshing these
views does not enumerate processes/displays, touch files, call media APIs or
query hardware. Explicit export follows the existing file-picker/write convention.

The bounded display snapshot carries topology generation, monotonic sample time,
an immutable accepted catalog, a stale flag and a rescan report. After 30 seconds
it is labelled stale until refreshed. One OS rescan worker coalesces requests;
the owner rejects results after topology/settings changes or shutdown. An OS or
per-monitor/per-path read failure retains the previous snapshot visibly stale.
Unchanged-topology rescans preserve generations, presentations, leases and fault
cache. Changed topology uses the established cancellation/recovery boundaries.
Windows topology/session notifications retain their existing immediate safety
invalidation, with subsequent enumeration moved off the timer/input path.
Rescans never clear quarantine records or change hardware grants.

**Live physical DDC capability probing is deferred.** The inspector says
“not tested / unknown”; saved permission and simulated helper results never
prove compatibility. Observed hardware state remains unknown without a live
query. No capability-string query was added. “Unavailable” identifies failed or
missing observations; “stale” identifies outdated retained observations; neither
is presented as “supported.” F16 is complete only for this read-only scope; the
larger research feature remains partial in the catalog.

Schema **5** extends each rule with enablement and a bounded canonical stable-ID
target set. Rule kinds remain explicit: 0 foreground, 1 audio-ignore, 2 running.
Schemas 1–4 remain readable with their earlier defaults. The first save verifies
a byte-exact `.v1-backup` through `.v4-backup`; a mismatched existing backup blocks
replacement. Full exports include configured paths/IDs/nicknames; profile exports
omit nicknames and explicitly map all target IDs, including disconnected rule
assignments. Import disarms automation and hardware in base and profile policies.
Runtime observations and per-display leases are never serialized.

The process worker starts lazily, scans at most once per second, and retains at
most one pending request and one result. Catalogs are capped at 8,192 processes
and 32 rules. Retained process handles establish executable identity and current
user SID/session; PID reuse cannot inherit a prior match. Own PID/executable,
helpers and live owned saver-job membership are excluded. Job registry access is
brief and synchronized; jobs are registered before suspended children resume.
Exited handles are excluded. All handles have bounded RAII ownership, and shutdown
joins the worker under the existing independent process shutdown deadline.

Each observation carries a scan-start monotonic timestamp and policy/session
epoch. Positive matches dominate unrelated inaccessible processes. Enumeration or
identity failures mean unknown, never confirmed absence. Fresh absence removes
that rule promptly; an old positive can contribute for at most **five seconds**,
including repeated unknown results. Expiration visibly reports uncertainty and
releases only that rule. Effective-profile precedence stays manual > foreground
application > schedule > base. Profile/session changes revoke prior observations;
even an identical-policy profile switch changes the observation epoch.

Unchanged rules can retain their original timestamped confirmations across a
background-only edit, while late old-epoch publications are rejected. Entry and
exit of a target's combined background contribution establish fresh idle
boundaries only on that target. Other background rules, leases, global snooze,
media, disabled/session state, manual/sticky owners and spans remain independent.
Background-only/label edits retain the accepted media observation, avoiding a
spurious global stale-media inhibitor. The existing stale-media policy is unchanged.

Default diagnostics add only fixed summaries, counts, freshness and approved
reason text. They omit executable names/paths, display IDs/nicknames, titles,
raw input, command lines and recovery secrets. No identifiable diagnostic-export
mode was added.

## Verification and review

New evidence is isolated in
[`evidence/inspector-map-background-20261016/`](../evidence/inspector-map-background-20261016/).
The initial sandbox attempt could not create private desktops: 26 native tests
were **not run**, while policy/theme checks passed. Escalated execution used the
same noninteractive runner and never switched to the operator's input desktop.
Fresh pre-change baselines passed Release **28/28 in 96.28 s** and Debug
**28/28 in 102.70 s**; these are separate from historical feature-batch evidence.

The lead owned architecture, shared schema/controller/platform/settings edits,
the inspector and final integration. Two GPT-6.1 Sol workers owned only new map
and process-adapter headers/fixtures. A separate GPT-6.1 Sol high-reasoning review
read the integrated implementation without editing it. Review fixes cover media
observation preservation, partial OS-read failures, and clipped keyboard focus in
the background editor. Boundary tests also cover stable peer histories,
identical-policy profile epochs, rejected late results and stopped publications.

Final executed checks (the two full commands use the pinned repository recipe):

| Check | Actual result | Evidence |
|---|---|---|
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Release -Test` | **29/29 passed**, 102.54 s | [Summary](../evidence/inspector-map-background-20261016/final-release.log), [detail](../evidence/inspector-map-background-20261016/release-details.log), [JUnit](../evidence/inspector-map-background-20261016/release-results.xml) |
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Debug -Test` | **29/29 passed**, 109.21 s | [Summary](../evidence/inspector-map-background-20261016/final-debug.log), [detail](../evidence/inspector-map-background-20261016/debug-details.log), [JUnit](../evidence/inspector-map-background-20261016/debug-results.xml) |
| Policy, both configurations | **117,665 assertions each**, 48 input/media/mute/poll combinations, eight virtual hours | Full-suite detail above |
| `pwsh -NoProfile -File evidence/inspector-map-background-20261016/run-analysis.ps1` | Exit 0; zero warnings/findings; empty `DEFECTS`; production source hashes unchanged | [Log](../evidence/inspector-map-background-20261016/static-analysis.log), [XML](../evidence/inspector-map-background-20261016/static-analysis.xml), [hashes](../evidence/inspector-map-background-20261016/analysis-source-hashes.json) |
| `pwsh -NoProfile -File tools/verify-pe.ps1 -Executable build/release/DAC.exe` and `build/debug/DAC.exe` | Both passed: AMD64 GUI, ASLR/DEP/high-entropy VA/CFG; 12 allowlisted Windows imports | [Release](../evidence/inspector-map-background-20261016/pe-release.json), [Debug](../evidence/inspector-map-background-20261016/pe-debug.json) |
| `git diff --check`; final source/hash and result reconciliation | Passed; final implementation/tests match the verification manifest | [Result manifest](../evidence/inspector-map-background-20261016/verification.json), [source hashes](../evidence/inspector-map-background-20261016/verification-source-hashes.json), [reconciliation script](../evidence/inspector-map-background-20261016/summarize-verification.ps1) |

The new native group covers cached inspector opening/refresh without power or
owner/lease changes, unchanged/changed/failed/partial/stale OS rescans, independent
selection and scrolling, map geometry and native routes, rule drafts/migration/
failed saves/profile target mapping, active-app redaction, and bounded query-only
worker lifecycle. Injected catalogs cover PID reuse, multiple instances, denied
access, session/owned exclusions, epoch/order rejection and uncertain grace.
The existing F02/F03/F05, wake/input, media, containment, sessions and lifecycle
suites also pass. No arbitrary configured application was launched.

The original flyout checks verified each owned font/window/registration deletion,
then compared 16 warmup against 64 measured cycles. Both configurations observed
process-wide GDI peaks **52→58**, with handles **464→464 Release / 469→469 Debug**;
that run allowed +8 aggregate GDI objects and +4 handles and left the variation
unexplained. The subsequent [GDI investigation](GDI_LIFETIME_INVESTIGATION.md)
associated the delayed allocation with native input initialization after edit
focus; individual allocation ownership remains unproven. Its
[independent follow-up](GDI_REVIEW.md) corrects repeated-focus/delayed-sampling
coverage and qualifies the original attribution.
It originally replaced that allowance with settled native-input measurements
and a separate process-local IME control. The independent follow-up repeats
verified focus and checks zero GDI/settled-USER growth across 256 cycles per case,
while bounding native-input USER transients by a frozen warmup ceiling.
The original evidence remains preserved; the follow-up does not change DAC's
native input behavior or establish general leak freedom/endurance qualification.

Investigation logs remain preserved: initial fixture warnings, the failed
peer-history assertion, focus-notification behavior, GDI-count experiments and a
slow Debug run with two existing UI-dispatch timeouts. Neither timeout limits nor
wake/performance assertions were relaxed. The final full runs above pass; hidden
timing samples still do not establish visible wake or endurance reliability.
The final media-transition refinement avoids extra policy serialization on every
timer refresh, retaining the existing polling work outside actual policy changes.
See the [review record](../evidence/inspector-map-background-20261016/review.txt).

## Separate qualification checklist

Use an explicitly authorized isolated **visible** Windows session with a fresh
profile and hardware off. Hidden fixtures do not establish these outcomes:

- At 100%, 150%, 200% and mixed-DPI transitions, operate map, inspector and nested
  rule drafts with Tab/Shift+Tab, arrows, Space, mnemonics, Escape and a screen
  reader. Confirm names/order, focus visibility, scrollbar behavior and text in
  light/dark/system/high contrast, small work areas and resized windows.
- On actual negative-origin, portrait, gapped, overlapping/cloned and large
  desktops, compare map placement to Windows. Reorder/disconnect/reconnect while
  selected; controls/settings must resolve the same stable target or reject it.
  Rescan unchanged/changed topology and failed/slow OS reads while presentations
  and keep-awake timers exist. Inspect stale/unknown and quarantine explanations.
- In a disposable configuration, add/edit/disable/delete path and explicit
  filename rules, multiple instances, crash/exit, denied access, profile changes,
  lock/unlock and suspend/resume. Check five-second uncertainty grace and fresh
  target idle boundaries, with independent media, snooze, leases and manual/span
  owners on peers. Review backups, file pickers, Save failure and disarmed imports.
- Recheck the earlier F02/F03/F05 visible checklist and the intermittent
  all-covered wake report. No wake reliability or production-readiness claim is
  made. Physical display power/capability testing needs separate explicit
  authority, an operator and recovery route; no physical operation was performed.
