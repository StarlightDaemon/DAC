# F03, F05 and F02 implementation and verification

> Paths under `evidence/` name local development evidence that is not tracked
> in this repository, so they appear as plain paths rather than links.

Subsequent work: [F16/F04/F11](INSPECTOR_MAP_BACKGROUND.md) retains this feature
batch and its evidence while migrating persistence to schema 5. The schema 4
and 28-test results below describe this earlier batch, not a fresh current run.

Work date: 2026-10-16, America/Denver. Starting HEAD:
`3ef15bdd7947f331dd33c1a7e85fc9802f5a2f3e`. The initial tree contained a modified
README and untracked comparative review, catalog and implementation guidelines.
Those changes were preserved. Current implementation is an uncommitted working
tree addition; exact final source hashes and verification are retained in
`evidence/features-20261016/`.

F01 remains on hold. No external command protocol, new public CLI commands,
network service, power-plan/system-awake request, brightness control, deployment,
installation, publication, login registration or physical DDC action was added
or performed. Work used one lead and two focused GPT-6.1 Sol subagents, with
shared policy/platform/integration edits reserved to the lead and isolated
flyout/test header ownership for parallel work.

## Milestones and contracts

1. **F03 — names:** schema 4 adds global `Config::nicknames`, keyed by canonical
   stable display identity, outside profile policy and permission keys. Advanced
   settings edits participate in the normal draft/Save/Close flow. Empty clears
   the stored name; OS labels remain fallback. Disconnected labels persist.
   Duplicate labels are allowed and numbered at presentation time. One formatter
   supplies setup, settings, identification, tray and flyout text. The parser
   rejects malformed UTF-8, C0/C1 controls, line/paragraph separators, directional
   overrides/isolates, more than 64 Unicode scalars or 256 bytes, and more than
   64 named identities. Native menu ampersands/tabs are escaped. Redacted
   diagnostics omit nicknames. Names alone do not reset policy/session state.

   Schemas 1–3 remain readable with empty name metadata. Saving an older schema
   requires a verified byte-exact `.v1-backup`, `.v2-backup` or `.v3-backup`;
   mismatched existing backups fail closed. Full exports retain labels and stable
   IDs; imported configurations always disarm automatic/hardware authority in
   base and profile policies. Profile-only transfers omit device labels and use
   the existing explicit target mapping. Timers never enter either transfer.

2. **F05 — temporary inhibition:** at most one runtime lease per display.
   Presets are 5/15/30/60 minutes, custom range 1–1440 minutes, and future local
   date/time within 24 hours. A new request replaces the old deadline. Absolute
   input is round-tripped against both Windows standard/daylight offsets; gaps
   and folds are rejected. The accepted local/UTC expiry is shown. Native
   `GetTickCount64` deadlines count suspend/hibernate time and ignore later wall
   clock/time-zone changes ([Windows time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/windows-time)).

   Leases survive valid same-process lock/suspend/profile transitions, clear on
   exit/restart and are discarded on disconnect/new ineligibility. Reconnection
   does not resurrect them. Grant/replacement/expiry/cancel target only their
   display, never synthesize shared input, and retain peers' generations/history.
   Automatic owners are cancelled/hidden immediately through `ReconcileWake`;
   launch and worker cleanup stay on the normal deferred path. Expiry/cancel
   gives the affected eligible automatic display a fresh idle boundary. Existing
   global snooze independently inhibits activation. Manual/sticky owners reject
   new leases; a later explicit manual Start overrides but retains an existing
   lease until it ends. Cancellation/expiry does not stop that manual owner.
   Active spanning presentations reject individual controls. No power authority
   is created. This feature inhibits DAC automatic protection only; it neither
   prevents Windows sleep nor guarantees physical monitor power.

3. **F02 — native control panel:** tray **Display controls…**, plus an optional
   persisted tray-click choice (default and earlier choices retained). A bounded
   scrolling native list shows connected display labels, presentation type,
   explanation and countdown. Selected-display controls provide Start, Stop,
   lease presets/custom/until/cancel and corresponding Advanced settings.
   Stable ID plus topology epoch rejects pending/stale actions. Removed selection
   remains explicit instead of redirecting to another display. Unknown/clone,
   disabled and span targets have appropriate disabled actions and reasons.
   Refresh retains HWNDs, custom inputs, list selection/scroll and focus. Native
   keyboard navigation, Escape, focus scrolling, complete accessible list strings,
   Fujin light/dark/system and Windows high contrast use the existing UI owner.
   No synchronous filesystem/media/shell/hardware probing occurs on flyout open,
   refresh or input cancellation. No new controller or transport exists.

Material choices: names are global device metadata, not profile properties;
the compact surface uses an all-display list with selected-display controls;
manual starts retain dormant timers; ambiguous DST times require a duration or
another time rather than guessing an offset. These choices implement the handoff
without depending on held F01.

## Verification record

The baseline Release build succeeded. Inside the sandbox, 25 runtime cases could
not create a private desktop (error 5); these were **not run**, not regressions.
With the approved runner authority the unchanged Release baseline passed
27/27 in 95.56 seconds. Logs: `baseline-release.log` and
`baseline-release-isolated.log`. No DAC UI was operated on the input desktop.

New policy/native fixture coverage lives in `tests/features-policy.hpp`,
`tests/features-platform.hpp` and `tests/flyout-fixture.hpp`, integrated as the
existing policy suite and a new private-runner `features` CTest. It covers
migration, malformed/Unicode/duplicate names, save failures, import disarming,
redaction, profile/reorder/reconnect retention, replacement/cancellation/expiry,
snooze/input/manual/sticky/span/session interactions, complete peer snapshots,
deterministic DST gaps/folds and calendar bounds, changing topology and guarded
actions, native drafts/focus, 64 displays, themes, DPI and repeated cleanup.

Review/verification found and resolved:

- Cosmetic-save optimization initially skipped explicit import/reset shutdown
  when only labels differed; explicit reset authority now forces cancellation.
  The failing regression and final test remain in the evidence.
- Absolute expiry sampled its monotonic origin before time-zone lookup, which
  could shorten a lease by lookup latency. It now pairs the origin with the wall
  sample after lookup.
- Cross-monitor initial flyout placement could replace an already-created font;
  the initial assignment now releases that font first.
- An initial cleanup assertion queried a deleted GDI font handle. Deletion and
  window destruction succeeded; Microsoft documents that `GetObjectType` cannot
  validate deleted cached objects. The fixture checks actual deletion results,
  ownership teardown and bounded repeated resources instead
  ([Microsoft GDI guidance](https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/march/resource-leaks-detecting-locating-and-repairing-your-leaky-gdi-code)).
- Hidden-fixture reopen/settings navigation is guarded against inadvertently
  showing a window. All runtime checks retain the private-desktop boundary.
- Offscreen inspection found narrow-client clipping after a DPI transition.
  Flyout controls now scale from their original logical bounds to available
  client width, and the window is clamped to the monitor work area. The fixture
  asserts that every control remains inside the client width at every tested DPI.
  The 200% image is a scrolling viewport, not a claim that all content fits
  vertically on a short desktop.

Final source is implementation-complete within the authorized local scope.
Both full applicable suites pass; independent integrated review has no remaining
actionable findings. Earlier failed/investigation logs are retained, not replaced.

| Executed check | Final outcome | Evidence in `evidence/features-20261016/` |
|---|---|---|
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Release -Test` | 28/28 passed, zero failed/skipped, 95.49 s | `verified-release.log`, `release-results.xml`, `release-details.log` |
| `pwsh -NoProfile -File tools/build.ps1 -Configuration Debug -Test` | 28/28 passed, zero failed/skipped, 102.24 s | `verified-debug.log`, `debug-results.xml`, `debug-details.log` |
| Policy, both configurations | 117,613 assertions; 48 input/media/mute/poll combinations; eight virtual hours | Full suite detail logs |
| Final targeted native features/layout | Passed; hidden 96/144/192 DPI, drafts/focus, stable targets, themes, 64-display scrolling and 16 resource cycles | `layout-test.log`, `flyout-100.png`, `flyout-150.png`, `flyout-200.png` |
| `pwsh -NoProfile -File evidence/features-20261016/run-analysis.ps1` | Exit 0; zero warnings; empty `DEFECTS`; source unchanged during/after analysis | `static-analysis.log`, `static-analysis.xml`, `analysis-source-hashes.json` |
| `pwsh -NoProfile -File tools/verify-pe.ps1 -Executable build/release/DAC.exe` (also Debug) | Both passed: AMD64 GUI, ASLR/DEP/high-entropy VA/CFG, 12 allowlisted Windows imports | `pe-release.json`, `pe-debug.json` |
| `git diff --check` and final independent source review | Passed; no actionable remaining findings | Source diff and this review record |

Toolchain/dependency setup uses the repository's pinned `tools/use-toolchain.ps1`
and existing Fujin verification on every build. Both suites include Fujin's 17
integrity/schema/generation cases, emergency, independent-wake, containment,
sessions, shutdown and actual private-desktop shipping-executable lifecycle.
`source-identity.json` identifies final source/test/build inputs against the
starting commit. No historical evidence was overwritten.

Final Release executable: 727,552 bytes, SHA-256
`1e96ebfc16f54d983f8c2c386aa3747938217c7ef1bf4213afe567511a70fb19`.
Debug: 2,906,624 bytes, SHA-256
`9294b4a003da464c9a734ba33aadf0815c85d0e9b7fa456cd550e19f5e737d63`.
These are build-tree binaries, not a release or installed artifact. The offscreen
images establish bounded native paint/layout observations; some native Edit
painting is omitted by hidden Windows controls, so they do not qualify visible
text entry or accessibility.

## Remaining qualification

Implementation completion is separate from visible/physical qualification.
Hidden control bounds, offscreen paints and synthetic DPI changes do not prove
visible layout, native Edit rendering, screen-reader usability or physical
mixed-DPI behavior. Actual lock/suspend/clock changes, hot-plug, Explorer tray,
physical input and long-duration resources require the separately authorized
isolated visible session checklist in [QUALIFICATION.md](QUALIFICATION.md).
Physical DDC/power acceptance needs additional explicit authority and a recovery
route. No production-readiness claim or fix for the existing intermittent
all-covered physical wake incident follows from these tests.
