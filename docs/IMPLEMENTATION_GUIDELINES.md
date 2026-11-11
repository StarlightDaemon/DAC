# DAC feature implementation guidelines

Implementation status update, October 16: F02/F03/F05 are delivered in
[FEATURE_IMPLEMENTATION.md](FEATURE_IMPLEMENTATION.md); scoped F16/F04/F11 are
delivered in [INSPECTOR_MAP_BACKGROUND.md](INSPECTOR_MAP_BACKGROUND.md).
The research plans below are retained as proposals/history, not claims that these
surfaces are absent. New physical DDC probing remains deferred. F01 stays on hold;
original research scores and estimates are unchanged. Schema 5 extends rule kinds
explicitly; future code must not convert kinds with a boolean/ternary shortcut.

Review date: October 15, 2026, America/Denver. Baseline: `3ef15bdd7947f331dd33c1a7e85fc9802f5a2f3e`.

These are conditional implementation plans for the ranked [comparative review](COMPARATIVE_REVIEW.md). The canonical scores, tags, dependencies and proposed acceptance criteria are in [FEATURE_CATALOG.json](FEATURE_CATALOG.json). The October 15 review itself implemented no runtime features; the implementation updates above describe subsequent work. Roadmap update on October 16, 2026: F01 is deferred by user decision and excluded from current ranks and implementation waves; its original plan is retained below.

## Shared implementation rules

Keep the UI thread as the sole owner of controller state. Workers publish bounded immutable observations or requests; only the owner resolves target identity and generation. New code should use the existing `Controller`, `Explanation`, `ReconcileWake` and asynchronous cleanup paths. Preserve independent input attribution, activation boundaries, explicit shared/sticky/span behavior and an emergency exit that does not wait for media, filesystem or hardware work.

For each persistent extension, migrate the current schema deliberately, preserve old defaults, validate bounded UTF-8 input, and roundtrip base and profile policies. Extend import disarming for every new permission or activation trigger. Host preferences, runtime leases and hardware recovery tickets have distinct lifetimes. Never transplant recovery state or make a profile/import an implicit grant.

Start new adapters as narrow interfaces around current functions; avoid a broad source split before a feature needs it. Keep same-source fixtures working. New presentation owners need cancellation, generation checks, containment and black fallback before new visuals. Hardware operations require independent typed grants, a bounded helper protocol and a physical compatibility gate. A killed helper does not establish physical recovery.

Use existing policy fixtures for deterministic transitions, platform fixtures for Win32 integration, and the private-desktop runner for host behavior. A meaningful regression test should exercise a boundary or failure, rather than mirror a new implementation. Physical and visible outcomes require the separate qualification records below.

## Delivery sequence

| Wave | Work | Dependencies and exit criterion |
| --- | --- | --- |
| 0 | G01, G02, G04 and G05; G03 before hardware expansion | Reproduce current baseline evidence in clean CI and resolve central wake/UI failures. Physical checks use the existing qualification procedure. |
| 1 | F03, F05 and F08 as small policy/settings slices; F02 and F04 as control surfaces | Reuse existing in-process controller actions for flyout/map; no full CLI or external IPC is needed. Each slice retains independent-peer state and import disarming. |
| 2 | F16 read-only inspector, F06, F10, F11, F23; F12 as a measured prototype | F06 depends on F05 and uses a native process picker while F01 is on hold. Hardware probing remains explicitly separate from opening the inspector. Localize after visible accessibility fixes. |
| 3 | F13, then F14 and F15 | Require G03 and F16, per-operation grants and restoration conflict handling. If physical recovery fails, keep these deferred. |
| 4 | Select from F07, F09, F17-F22 and F24-F25/F27 according to observed demand | Each item has its own scope decision. F24 can move earlier once G01/G04 are satisfied. F17 begins read-only. No commitment to build the entire wave. |
| On hold | F01 full command interface; F21 blocked by that dependency | F01 is excluded by user decision. Retain plans for reconsideration without scheduling CLI/IPC work. |
| Defer | F26 pointer routing | Prefer compatibility with LittleBigMouse; input interception adds little to DAC's central purpose. |

Wave order reflects dependencies and product scope, so it can override numerical rank. Feature estimates are initial engineering ranges, not calendar commitments. They include initial local tests and exclude hardware/visible qualification, external review, signing procurement, translation labor and package-manager acceptance. Reestimate after the first two small slices; do not sum ranges as a project budget because shared adapters overlap.

## Qualification gates

### G01 Physical wake and topology qualification

Value 10/10; effort 7/10; risk 9/10. Dependencies: none. Establishes whether the central per-monitor protection behavior works on real desktops.

Local anchors: [docs/QUALIFICATION.md](../docs/QUALIFICATION.md), [tests/nativewake-negative.cpp](../tests/nativewake-negative.cpp), [tests/host-lifecycle.cpp](../tests/host-lifecycle.cpp).

Proposed work:

- Run the existing qualification matrix in an isolated visible Windows session across one, two and three displays, all coverage states, queued/high-rate input, hot-plug, mixed DPI, HDR and resume.
- Retain executable hash, redacted timing counters, exact topology and a repeatable all-covered failure recipe. Fix defects before expanding owner/input behavior.

Acceptance:

- Only the attributed independent target dismisses; peer generation/idle history stays intact.
- Recovery is visibly reliable across repeated all-covered trials; unresolved failures remain recorded.

### G02 Visible settings and accessibility acceptance

Value 9/10; effort 5/10; risk 5/10. Dependencies: none. Verifies that existing settings and new controls can actually be read and operated.

Local anchors: [docs/QUALIFICATION.md](../docs/QUALIFICATION.md), [src/platform.hpp LayoutSettings](../src/platform.hpp), [tests/platform.cpp ExerciseSettingsDpi](../tests/platform.cpp).

Proposed work:

- Check visible Edit painting, keyboard order, focus, screen-reader names, high contrast, light/dark and DPI changes.
- Fix concrete failures using the current native controls before adding flyout/localization surfaces.

Acceptance:

- Setup and recovery can be completed by keyboard and screen reader.
- No hidden snapshot is counted as visible UI acceptance.

### G03 Monitor and adapter hardware qualification

Value 9/10; effort 8/10; risk 10/10. Dependencies: G01. Establishes the narrow hardware combinations on which power and later brightness features are recoverable.

Local anchors: [docs/QUALIFICATION.md](../docs/QUALIFICATION.md), [src/platform.hpp PowerGuardian](../src/platform.hpp), [tests/platform.cpp PowerTests](../tests/platform.cpp).

Proposed work:

- Use separately authorized, operator-present tests for exact monitor/cable/dock/GPU combinations with physical recovery.
- Record off, cancel, wake, timeout and quarantine behavior; preserve uncertain recovery tickets and never widen an unsupported target.

Acceptance:

- Publish a qualified/unsupported/unknown matrix per operation and connection.
- A successful simulated cycle is never substituted for physical wake evidence.

### G04 Clean CI and trusted release preparation

Value 9/10; effort 6/10; risk 6/10. Dependencies: none. Makes delivered artifacts reproducible and reviewable outside the developer machine.

Local anchors: [CMakeLists.txt](../CMakeLists.txt), [tools/build.ps1](../tools/build.ps1), [tools/verify-pe.ps1](../tools/verify-pe.ps1), [tools/verify-package.ps1](../tools/verify-package.ps1), [docs/GITHUB_INTEGRATION.md](../docs/GITHUB_INTEGRATION.md).

Proposed work:

- Add clean Windows CI using pinned toolchain/Fujin inputs and the existing Release/Debug, static-analysis, PE and ZIP checks.
- Keep runtime tests on private desktops without startup registration or DDC. Design signing and immutable release artifacts separately; verify a final signed artifact as well as unsigned reproducibility.

Acceptance:

- Clean CI reproduces the existing suites and retains exact source/tool identities.
- Release version, resource version, archive root, manifest, signature and checksums agree.

### G05 Real activity and endurance qualification

Value 9/10; effort 7/10; risk 7/10. Dependencies: G01. Validates that common real applications and long sessions do not cause false suppression, missed wake or leaks.

Local anchors: [docs/QUALIFICATION.md](../docs/QUALIFICATION.md), [src/platform.hpp ObserveMedia](../src/platform.hpp), [tests/platform.cpp Sessions](../tests/platform.cpp).

Proposed work:

- Exercise browsers/native media, mute, multiple endpoints, fullscreen games, actual controllers and AC/DC transitions, then run hours/days of desktop use.
- Measure resource growth and dismissal responsiveness; retain content/codec failure results. Isolate decoder work before expanding untrusted media inputs.

Acceptance:

- Ambiguous media does not acquire unwarranted exact-tab attribution.
- No sustained resource growth or stranded owned processes appears in representative runs; failures include reproducible evidence.

## Feature plans in priority order

### F02 Compact per display control flyout

**Implementation update (2026-10-16):** Implemented in the combined native feature slice. The following delta and steps preserve the earlier research baseline; current contracts, verification and outstanding visible/physical qualification are in [FEATURE_IMPLEMENTATION.md](FEATURE_IMPLEMENTATION.md). F01 remains on hold.

Rank 1; priority 8.5/10; usefulness 9/10; popularity proxy 6/10; fit 10/10; effort 5/10; risk 3/10. Wave 1; initial engineering estimate 5-10 days. Dependencies: none. Tags: daily-use, visibility, native.

**Research baseline delta (historical):** Tray menus and selected-monitor explanations exist; there is no compact all-display surface with simultaneous status, reason, countdown and direct actions.

**Value to DAC:** Reduces the number of interactions needed to understand and control several monitors.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Monitorian](https://github.com/emoacht/Monitorian). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp Menu](../src/platform.hpp), [src/platform.hpp ExplanationText](../src/platform.hpp), [src/policy.hpp Explanation](../src/policy.hpp).

Implementation:

1. Build a native keyboard-accessible flyout showing one row per connected target: name, presentation, inhibition reason, next deadline and start/dismiss controls.
2. Read the existing explanation snapshot on the UI thread; reuse current commands and permission checks rather than creating another controller.
3. Keep a stable selection across topology updates and expose equivalent text to accessibility tools.

Acceptance:

- Each action affects the chosen stable target while peers retain state.
- Rows remain usable at 100-200% DPI, high contrast and with keyboard/screen reader navigation.
- No shell, media or DDC work runs synchronously while opening or refreshing the flyout.

### F03 Persistent friendly display names

**Implementation update (2026-10-16):** Implemented in the combined native feature slice. The following delta and steps preserve the earlier research baseline; current contracts, verification and outstanding visible/physical qualification are in [FEATURE_IMPLEMENTATION.md](FEATURE_IMPLEMENTATION.md). F01 remains on hold.

Rank 2; priority 8.5/10; usefulness 9/10; popularity proxy 4/10; fit 10/10; effort 2/10; risk 2/10. Wave 1; initial engineering estimate 2-4 days. Dependencies: none. Tags: daily-use, multi-monitor, settings.

**Research baseline delta (historical):** OS labels and temporary Display N overlays exist, but users cannot save their own names.

**Value to DAC:** Makes similar monitors recognizable in settings, tray actions and scripts.

Comparable evidence: [Monitorian](https://github.com/emoacht/Monitorian). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp Display](../src/platform.hpp), [src/platform.hpp Catalog](../src/platform.hpp), [src/policy.hpp Preference](../src/policy.hpp), [src/platform.hpp SafeDiagnosticAlias](../src/platform.hpp).

Implementation:

1. Persist a bounded UTF-8 display nickname keyed by stable identity in a versioned schema; preserve disconnected names.
2. Use the nickname only for display text. Keep IDs authoritative and permit duplicate names without ambiguous command resolution.
3. Redact nicknames in diagnostics because a name may contain personal information.

Acceptance:

- Names survive reorder, reconnect and settings roundtrip without transferring hardware authority.
- Empty, duplicate and non-Latin names render safely; invalid lengths/encoding are rejected.
- Diagnostics still contain only approved Display N aliases.

### F05 Per display temporary keep awake leases

**Implementation update (2026-10-16):** Implemented in the combined native feature slice. The following delta and steps preserve the earlier research baseline; current contracts, verification and outstanding visible/physical qualification are in [FEATURE_IMPLEMENTATION.md](FEATURE_IMPLEMENTATION.md). F01 remains on hold.

Rank 3; priority 8.3/10; usefulness 9/10; popularity proxy 4/10; fit 10/10; effort 4/10; risk 4/10. Wave 1; initial engineering estimate 5-10 days. Dependencies: none. Tags: daily-use, multi-monitor, timers.

**Research baseline delta (historical):** Current snooze is global with fixed tray durations. Per-target leases, custom durations and an absolute expiry are missing.

**Value to DAC:** Lets one screen stay available for a meeting while unused peers continue protecting themselves.

Comparable evidence: [PowerToys Awake](https://learn.microsoft.com/en-us/windows/powertoys/awake). Evidence class: adapted-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp Controller](../src/policy.hpp), [src/platform.hpp Menu](../src/platform.hpp), [src/policy.hpp Controller](../src/policy.hpp).

Implementation:

1. Add runtime leases with stable target sets, monotonic deadlines and explicit cancel controls; keep them separate from persisted monitor enablement.
2. Treat an absolute local expiry as an input resolved to a deadline with stated clock-change behavior. On expiry establish fresh idle boundaries only for affected nodes.
3. Show active leases in explanations and the flyout; session exit clears them. This inhibits DAC activation and does not by itself inhibit Windows sleep.

Acceptance:

- A lease for A neither wakes nor resets B, including under shared/media policies.
- Expiry, cancellation, suspend/resume and clock changes follow documented rules.
- Imported configurations cannot silently create runtime leases.

### F16 Display capability inspector and rescan report

**Implementation update (2026-10-16):** Scoped cached inspector / OS-only rescan delivered; new physical DDC probing remains deferred. See [INSPECTOR_MAP_BACKGROUND.md](INSPECTOR_MAP_BACKGROUND.md). The following research steps retain the earlier proposal.

Rank 4; priority 7.9/10; usefulness 8/10; popularity proxy 6/10; fit 10/10; effort 5/10; risk 6/10. Wave 2; initial engineering estimate 5-10 days. Dependencies: none. Tags: support, multi-monitor, ddc.

**Research baseline delta (historical):** Catalog refresh, faults and redacted diagnostics exist; there is no structured user-facing capability/probe view.

**Value to DAC:** Explains why a target is ineligible or quarantined and prepares hardware expansion without guesswork.

Comparable evidence: [Monitorian](https://github.com/emoacht/Monitorian), [LittleBigMouse](https://github.com/mgth/LittleBigMouse). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp Catalog](../src/platform.hpp), [src/platform.hpp DiagnosticText](../src/platform.hpp), [src/platform.hpp PowerPending](../src/platform.hpp), [src/platform.hpp RefreshTopology](../src/platform.hpp).

Implementation:

1. First expose cached topology, identity eligibility, fault state and observation age without issuing hardware queries.
2. Add a separate explicit capability probe on the contained worker/helper path; cache by device/connection epoch and preserve unknown/timeout states.
3. Offer a redacted summary by default and an explicit identifiable local export for support. Read queries can still stall a driver.

Acceptance:

- Opening diagnostics never sends DDC commands.
- Canceled, hung and malformed capability responses are bounded and shown as unknown rather than supported.
- Reconnect invalidates the right cache; clone targets and quarantine cannot become eligible through rescan.

### F04 Visual display map for target selection

**Implementation update (2026-10-16):** Implemented; visible and physical qualification remains separate. See [INSPECTOR_MAP_BACKGROUND.md](INSPECTOR_MAP_BACKGROUND.md). The following research steps retain the earlier proposal.

Rank 5; priority 7.8/10; usefulness 8/10; popularity proxy 5/10; fit 10/10; effort 5/10; risk 3/10. Wave 1; initial engineering estimate 5-10 days. Dependencies: none. Tags: multi-monitor, onboarding, visibility.

**Research baseline delta (historical):** DAC provides a dropdown and identify overlays, but no spatial map of the current desktop.

**Value to DAC:** Helps users choose the correct physical screen before changing a presentation or policy.

Comparable evidence: [LittleBigMouse](https://github.com/mgth/LittleBigMouse). Evidence class: adapted-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp Catalog](../src/platform.hpp), [src/platform.hpp IdentifyDisplays](../src/platform.hpp), [src/platform.hpp ShowSetup](../src/platform.hpp).

Implementation:

1. Render scaled rectangles from the catalog with names, enabled state and a selected target; provide a parallel accessible list.
2. Use signed coordinates and preserve aspect ratios across rotated and negative-coordinate outputs. Mark clone/unknown targets as ineligible.
3. Keep this map read-only with respect to Windows layout; opening it must not activate presentations.

Acceptance:

- Fixtures cover mixed DPI, portrait layouts, negative coordinates and clone targets.
- Selection remains attached to identity across reordering; stale map actions are rejected.
- Visible acceptance confirms that labels and keyboard focus match physical displays.

### F11 Rules for background running applications

**Implementation update (2026-10-16):** Implemented; visible and physical qualification remains separate. See [INSPECTOR_MAP_BACKGROUND.md](INSPECTOR_MAP_BACKGROUND.md). The following research steps retain the earlier proposal.

Rank 6; priority 7.6/10; usefulness 8/10; popularity proxy 6/10; fit 9/10; effort 6/10; risk 5/10. Wave 2; initial engineering estimate 5-10 days. Dependencies: none. Tags: automation, media, rules.

**Research baseline delta (historical):** Rules currently match foreground apps or ignore audio; they do not match a bounded snapshot of background processes.

**Value to DAC:** Supports long tasks or applications that should inhibit protection even after losing focus.

Comparable evidence: [Lively Wallpaper](https://github.com/lively-community/lively). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp AppRule](../src/policy.hpp), [src/policy.hpp Controller](../src/policy.hpp), [src/platform.hpp ObserveForeground](../src/platform.hpp), [src/platform.hpp MediaMain](../src/platform.hpp).

Implementation:

1. Add a distinct running-process rule kind; scan on a worker at a bounded interval and match full paths by default.
2. Require explicit global or selected-monitor scope because a background process has no reliable foreground display.
3. Publish timestamped observations with a stale-state policy and explain which rule blocks a target without logging paths by default.

Acceptance:

- Foreground-only rules keep existing semantics; process rules cannot accidentally become global.
- Stale/denied process observations, exited tasks and owned saver processes behave predictably.
- Measure scan overhead with many processes and confirm no UI/input-path blocking.

### F06 Process bound temporary keep awake leases

Rank 7; priority 7.3/10; usefulness 8/10; popularity proxy 4/10; fit 9/10; effort 6/10; risk 5/10. Wave 2; initial engineering estimate 5-10 days. Dependencies: F05. Tags: integration, process-lifetime, timers.

**Current delta:** Foreground executable rules exist; there is no explicit temporary lease attached to one live process instance.

**Value to DAC:** Keeps selected displays available while a specific task runs and removes the exception automatically when it ends.

Comparable evidence: [PowerToys Awake](https://learn.microsoft.com/en-us/windows/powertoys/awake). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp AppRule](../src/policy.hpp), [src/platform.hpp ProcessIdentity](../src/platform.hpp), [src/policy.hpp Controller](../src/policy.hpp).

Implementation:

1. Bind a lease to a retained process handle and creation identity, with a maximum timeout and an explicit target set.
2. Observe process exit away from the input path, then enqueue lease removal on the UI thread. A reused PID must never inherit the lease.
3. Use a native process picker for the current roadmap; expose access-denied and exited-process outcomes without widening scope. A CLI option can be reconsidered only if F01 is restored.

Acceptance:

- Process exit, crash, PID reuse and denied handle access cannot leave an indefinite inhibitor.
- Only selected monitors receive a new idle boundary on lease removal.
- Owned DAC saver/helper processes cannot create recursive self-inhibition.

### F08 Weekday schedules and date exceptions

Rank 8; priority 7.3/10; usefulness 8/10; popularity proxy 1/10; fit 10/10; effort 4/10; risk 4/10. Wave 1; initial engineering estimate 5-10 days. Dependencies: none. Tags: calendar, daily-use, dac-extension.

**Current delta:** Profiles support daily HH:MM windows, including overnight periods, but no weekdays or explicit date exclusions. This is a DAC-specific extension, not an observed donor feature.

**Value to DAC:** Allows different weekday/weekend routines without editing daily profiles.

Comparable evidence: DAC-specific inference; no matched comparable. Evidence class: dac-inference. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp Profile](../src/policy.hpp), [src/policy.hpp InSchedule](../src/policy.hpp), [src/policy.hpp ResolveProfile](../src/policy.hpp), [src/platform.hpp WorkspaceProc](../src/platform.hpp).

Implementation:

1. Introduce a weekday mask and bounded local-date exception list with a schema migration preserving existing every-day behavior.
2. Define an overnight interval as owned by its starting day. Keep manual > foreground-app > scheduled > base precedence and show why a profile won.
3. Use an injected civil clock to reevaluate DST, timezone changes and resume; avoid replaying missed activations.

Acceptance:

- Test Friday-to-Saturday overnight, exclusive ends, DST gaps/repeats and overlapping profiles.
- Old settings retain their meaning; import continues to disarm automatic and hardware authority.
- The schedule editor previews the next matching interval and reports conflicts deterministically.

### F23 Localization infrastructure and translated UI

Rank 9; priority 7.3/10; usefulness 7/10; popularity proxy 8/10; fit 9/10; effort 7/10; risk 3/10. Wave 2; initial engineering estimate 11-20 days. Dependencies: G02. Tags: accessibility, international, native.

**Current delta:** UI strings are predominantly inline English and the resources declare an English version-information language.

**Value to DAC:** Makes setup and recovery understandable to a broader audience.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Monitorian](https://github.com/emoacht/Monitorian), [Lively Wallpaper](https://github.com/lively-community/lively), [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp ShowSetup](../src/platform.hpp), [src/platform.hpp ShowSettings](../src/platform.hpp), [src/platform.hpp ReasonText](../src/platform.hpp), [resources/dac.rc](../resources/dac.rc).

Implementation:

1. Extract strings into stable resource IDs, keep persisted enums/keys language-neutral and retain English fallback.
2. Start with one pilot locale and longer-string pseudo-localization; use locale-aware date/time formatting and inspect native layout sizing.
3. Keep diagnostics machine fields stable and resource updates separately reviewable; plan RTL support explicitly rather than assuming it.

Acceptance:

- Missing translations fall back without losing emergency/recovery text.
- Long strings, Unicode, keyboard accelerators and 100-200% DPI do not clip controls.
- Configuration roundtrips unchanged across language selection and translated UI remains accessible.

### F13 Actual monitor brightness control

Rank 10; priority 7.2/10; usefulness 8/10; popularity proxy 7/10; fit 7/10; effort 8/10; risk 9/10. Wave 3; initial engineering estimate 11-20 days. Dependencies: F16, G03. Tags: optional, ddc, brightness.

**Current delta:** Dim is software overlay opacity; current DDC support implements power off/wake only, not brightness.

**Value to DAC:** Adds true brightness reduction before covering a monitor and manual adjustment from DAC.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Monitorian](https://github.com/emoacht/Monitorian), [LittleBigMouse](https://github.com/mgth/LittleBigMouse). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp PowerChild](../src/platform.hpp), [src/platform.hpp PowerGuardian](../src/platform.hpp), [src/policy.hpp Preference](../src/policy.hpp).

Implementation:

1. Introduce a separate brightness grant and capability model, independent of the current hardware-power flag. Use DDC for eligible external monitors and investigate WMI for built-in panels.
2. Extend a separately contained helper protocol with typed brightness operations, rate limits, cancellation and bounded target identity validation.
3. Record prior value and the last applied value. Restore only if DAC still owns that value, so another controller or the user is not overwritten.

Acceptance:

- Simulation covers timeout, wrong target, permission revocation, concurrent controllers and restoration conflicts.
- Physical checks cover exact monitor/adapter combinations with recovery available.
- Never describe overlay dimming as hardware brightness or infer energy savings from a percentage.

### F10 Battery and AC profile triggers

Rank 11; priority 7.1/10; usefulness 7/10; popularity proxy 5/10; fit 9/10; effort 4/10; risk 4/10. Wave 2; initial engineering estimate 5-10 days. Dependencies: none. Tags: power, laptop, profiles.

**Current delta:** The existing batteryBlack flag changes expensive content to black; it cannot select a full battery-specific profile.

**Value to DAC:** Changes complete protection policies when a laptop moves between power sources.

Comparable evidence: [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode). Evidence class: adapted-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp PowerSource](../src/policy.hpp), [src/policy.hpp Controller](../src/policy.hpp), [src/policy.hpp ResolveProfile](../src/policy.hpp), [src/platform.hpp ReadPowerSource](../src/platform.hpp).

Implementation:

1. Add an explicit AC/DC trigger to profiles and document where it sits relative to app and scheduled matches.
2. Reuse the existing power-source adapter and debounce; Unknown must preserve a conservative, explainable policy.
3. Continue applying base hardware grants and enablement caps after profile selection.

Acceptance:

- Rapid source changes and Unknown readings do not thrash ownership or enable DDC.
- Manual profiles retain precedence and old batteryBlack behavior remains available.
- Injected AC/DC transitions and visible laptop resume tests agree.

### F24 Package manager distribution

Rank 12; priority 7.0/10; usefulness 7/10; popularity proxy 6/10; fit 8/10; effort 5/10; risk 5/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: G04, G01. Tags: adoption, installation, optional.

**Current delta:** DAC has a verified local portable ZIP workflow, but no maintained public package-manager manifest.

**Value to DAC:** Makes finding, installing and updating qualified releases easier.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [tools/package.ps1](../tools/package.ps1), [tools/verify-package.ps1](../tools/verify-package.ps1), [docs/GITHUB_INTEGRATION.md](../docs/GITHUB_INTEGRATION.md), [resources/dac.rc](../resources/dac.rc).

Implementation:

1. After a qualified signed release exists, prepare a versioned WinGet or Scoop manifest tied to immutable artifact hashes.
2. Define portable extraction, user data retention, startup path handling and update/uninstall behavior before publishing.
3. Validate manifests locally; package-manager submission and release publication remain separate delivery actions.

Acceptance:

- Clean-machine install/update/remove preserve preferences and never enable startup or DDC implicitly.
- Every manifest references the expected architecture, version and verified artifact hash.
- Moving the executable cannot leave an unnoticed stale login entry.

### F12 Coverage aware application inhibition

Rank 13; priority 6.8/10; usefulness 7/10; popularity proxy 6/10; fit 8/10; effort 7/10; risk 6/10. Wave 2; initial engineering estimate 11-20 days. Dependencies: none. Tags: fullscreen, gaming, heuristics.

**Current delta:** Foreground/fullscreen overlap exists; DAC does not calculate collective visible coverage as an optional inhibitor.

**Value to DAC:** Reduces false activation when several visible windows collectively cover a monitor.

Comparable evidence: [Lively Wallpaper](https://github.com/lively-community/lively). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp ObserveForeground](../src/platform.hpp), [src/platform.hpp ForegroundDisplays](../src/platform.hpp), [src/policy.hpp Controller](../src/policy.hpp).

Implementation:

1. Prototype bounded window coverage on a worker with a configurable threshold and hysteresis; start with opaque ordinary windows.
2. Exclude DAC-owned covers and previews, hidden/cloaked/minimized windows and irrelevant desktops. Keep the current simple policy as default.
3. Present the result as a heuristic; uncertainty must not invent precise media or tab attribution.

Acceptance:

- Fixtures cover tiled windows, overlays, transparent windows, moving windows and multiple outputs.
- A covered A does not inhibit B; DAC's own cover cannot sustain its own inhibitor.
- Visible gaming tests check exclusive/fullscreen behavior and CPU cost before enabling by default.

### F07 Keep Windows awake while DAC manages displays

Rank 14; priority 6.4/10; usefulness 7/10; popularity proxy 4/10; fit 7/10; effort 6/10; risk 6/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: F05. Tags: optional, system-power, integration.

**Current delta:** DAC observes display requests but does not create a user-requested system keep-awake lifetime.

**Value to DAC:** Supports downloads or compute jobs while DAC continues using black or other presentations on idle screens.

Comparable evidence: [PowerToys Awake](https://learn.microsoft.com/en-us/windows/powertoys/awake). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp ObserveMedia](../src/platform.hpp), [src/platform.hpp SetBlocked](../src/platform.hpp), [src/platform.hpp Shutdown](../src/platform.hpp).

Implementation:

1. Add a separate opt-in system-power lease using a Windows power request with a visible reason and clear/close ownership on every exit path.
2. Default to a system-only request. Any screen-on request is global Windows behavior and must be labeled separately from DAC per-display inhibition.
3. Respect explicit sleep/session policy and document Modern Standby and DC limitations; do not modify persistent power plans.

Acceptance:

- Injected API failures and every cancellation path release request ownership.
- Windows sleep, lock and battery behavior are checked in an isolated visible session.
- The UI never implies that Windows display requests provide per-monitor power authority.

### F15 Scheduled and idle hardware brightness stages

Rank 15; priority 6.0/10; usefulness 6/10; popularity proxy 5/10; fit 7/10; effort 6/10; risk 8/10. Wave 3; initial engineering estimate 5-10 days. Dependencies: F13, F08. Tags: optional, automation, brightness.

**Current delta:** DAC has dim/fade and timed black/power stages; those stages do not drive hardware brightness.

**Value to DAC:** Can reduce brightness gradually before native black or select different brightness targets by time.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp Controller](../src/policy.hpp), [src/policy.hpp DimAlpha](../src/policy.hpp), [src/policy.hpp ResolveProfile](../src/policy.hpp).

Implementation:

1. Model brightness as a separately owned stage with explicit entry, cancellation and restoration rules. Coalesce intermediate values to limit DDC traffic.
2. Reuse profile scheduling and never let schedules grant hardware authority.
3. Resolve manual brightness, time profile and idle-stage precedence explicitly; topology/session transitions invalidate pending writes.

Acceptance:

- Input during a ramp cancels stale operations and restores only owned values.
- Schedule overlap, wake and helper timeout do not repeat writes indefinitely.
- Physical acceptance measures actual responsiveness separately from synthetic timing.

### F22 Explicit Windows workstation lock action

Rank 16; priority 5.9/10; usefulness 7/10; popularity proxy 1/10; fit 6/10; effort 4/10; risk 6/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: G01. Tags: optional, windows, dac-adaptation.

**Current delta:** DAC deliberately does not lock Windows. ScreenBlind's password overlay motivates this adaptation, but is not evidence of secure workstation locking.

**Value to DAC:** Offers an unambiguous security action for users who might otherwise confuse black presentations with locking.

Comparable evidence: [ScreenBlind](https://github.com/hemigapp/ScreenBlind). Evidence class: adapted-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp SetBlocked](../src/platform.hpp), [src/platform.hpp WindowProc](../src/platform.hpp), [src/platform.hpp Menu](../src/platform.hpp).

Implementation:

1. Add a clearly named explicit Lock Windows command using the OS lock API; the first version should be manual only.
2. Retire/hide owned presentations and let existing session notifications handle lock/unlock. Check the asynchronous result through session state.
3. Defer automatic locking until whole-session idle semantics are specified; never lock solely because one secondary display is idle.

Acceptance:

- An API fixture verifies request handling without locking the operator's session.
- Visible tests in an isolated session confirm lock/unlock and cover cleanup.
- No DAC password database, cursor trapping or claim of per-monitor OS locking is introduced.

### F14 Brightness calibration and synchronized groups

Rank 17; priority 5.9/10; usefulness 6/10; popularity proxy 6/10; fit 6/10; effort 6/10; risk 7/10. Wave 3; initial engineering estimate 5-10 days. Dependencies: F13. Tags: optional, multi-monitor, brightness.

**Current delta:** DAC has no hardware brightness ranges, calibration curves or linked adjustment groups.

**Value to DAC:** Makes one user-facing brightness adjustment behave more consistently across unlike panels.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Monitorian](https://github.com/emoacht/Monitorian). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp Preference](../src/policy.hpp), [src/platform.hpp Catalog](../src/platform.hpp).

Implementation:

1. Store per-target user min/max and a monotonic normalized mapping; retain native values for diagnostics only with user choice.
2. Apply a group intent as independent bounded requests. Report partial success without claiming an atomic physical update.
3. Let users adjust perceived equality; avoid presenting percentage matching as measured luminance calibration.

Acceptance:

- Mappings are monotonic and clamped; invalid ranges cannot send out-of-range values.
- A disconnected or failed group member does not redirect changes to another display.
- Restoration ownership and explicit grants remain per device.

### F25 Opt in update notifications

Rank 18; priority 5.6/10; usefulness 6/10; popularity proxy 5/10; fit 5/10; effort 6/10; risk 6/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: G04, F24. Tags: optional, network, maintenance.

**Current delta:** DAC is offline and has no updater; this is a deliberate change to its product contract.

**Value to DAC:** Helps users notice fixes once qualified releases exist.

Comparable evidence: [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/standalone.cpp Main](../src/standalone.cpp), [src/platform.hpp Maintain](../src/platform.hpp), [docs/SECURITY.md](../docs/SECURITY.md).

Implementation:

1. Prefer package-manager updates first. If demand remains, add manual or opt-in metadata checks in a worker with a timeout and no device/configuration upload.
2. Show a release link and verified version information; avoid self-replacement while presentations or helpers are active.
3. Keep offline launches fast and default behavior unchanged. A full automatic updater requires a separate rollback/signature design.

Acceptance:

- Offline, malformed metadata, proxy failure and version rollback responses cannot block startup.
- Disabled checking produces no network requests.
- A notification cannot execute downloaded code or mutate hardware/startup permissions.

### F09 Sunrise and sunset profile triggers

Rank 19; priority 5.4/10; usefulness 5/10; popularity proxy 5/10; fit 7/10; effort 6/10; risk 4/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: F08. Tags: optional, offline, calendar.

**Current delta:** DAC has fixed local times only; it has no solar calculation or location setting.

**Value to DAC:** Can align presentation and timeout preferences with daylight without daily manual adjustment.

Comparable evidence: [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/policy.hpp Profile](../src/policy.hpp), [src/policy.hpp ResolveProfile](../src/policy.hpp), [src/platform.hpp ShowWorkspace](../src/platform.hpp).

Implementation:

1. Use a deterministic local solar calculation with manually supplied approximate coordinates and offsets; avoid a mandatory location/network service.
2. Represent polar-day/no-event outcomes explicitly, use timezone-aware civil dates and offer a fixed-time fallback.
3. Do not add geolocation to exported diagnostics. Keep solar triggers below manual/application precedence.

Acceptance:

- Reference vectors cover hemispheres, DST and polar cases.
- Missing coordinates or an unavailable event produces a visible fallback instead of an arbitrary activation.
- Offline operation and configuration disarming remain intact.

### F20 Additional configurable native scenes

Rank 20; priority 5.1/10; usefulness 5/10; popularity proxy 1/10; fit 8/10; effort 5/10; risk 3/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: none. Tags: optional, native, content.

**Current delta:** DAC already has moving clock, constellation, themes and rotation; new scene types and per-scene density/speed/clock format are missing.

**Value to DAC:** Adds variety at lower runtime cost than embedding a browser or video engine.

Comparable evidence: [ScreenBlind](https://github.com/hemigapp/ScreenBlind). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp PaintScene](../src/platform.hpp), [src/policy.hpp Preference](../src/policy.hpp), [src/platform.hpp StartDraftPreview](../src/platform.hpp).

Implementation:

1. Add one low-luminance native scene and bounded speed/density/clock-format options rather than an executable plugin system.
2. Reuse draft preview, scene deadlines, palettes and battery fallback. Keep motion independent of media/input attribution.
3. Validate each parameter and use a deterministic test clock/random seed for rendering fixtures.

Acceptance:

- Worst-case settings stay within measured paint/resource budgets.
- Input dismisses the new scene through the existing path; low-power and black transitions still work.
- Visible acceptance checks high contrast settings UI, mixed DPI and repeated static elements.

### F27 Runtime accent selection

Rank 21; priority 5.1/10; usefulness 4/10; popularity proxy 5/10; fit 7/10; effort 4/10; risk 2/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: none. Tags: optional, native, fujin.

**Current delta:** System/light/dark and high contrast exist; seven Fujin accent presets are build-time choices only.

**Value to DAC:** Lets users personalize the settings UI without rebuilding DAC.

Comparable evidence: [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode). Evidence class: adapted-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [cmake/Fujin.cmake](../cmake/Fujin.cmake), [tools/generate-theme.ps1](../tools/generate-theme.ps1), [src/platform.hpp RefreshSettingsTheme](../src/platform.hpp), [src/policy.hpp IntegrationSettings](../src/policy.hpp).

Implementation:

1. Generate an allowlisted table of all pinned Fujin palettes at build time and choose an index in host preferences.
2. Refresh existing controls in place while preserving drafts/focus. High contrast must continue to override custom palettes.
3. Do not change low-luminance scene colors when a settings accent changes.

Acceptance:

- Each generated palette passes integrity validation and contrast review.
- Old host preferences migrate to the current default; unknown accent values fail safely.
- Changing accents retains drafts, focus and high-contrast behavior.

### F17 HDR awareness and SDR content brightness investigation

Rank 22; priority 4.9/10; usefulness 5/10; popularity proxy 4/10; fit 6/10; effort 8/10; risk 8/10. Wave 4; initial engineering estimate 11-20 days. Dependencies: F16, G01. Tags: optional, hdr, research.

**Current delta:** Physical HDR appearance is unqualified, and DAC has no HDR capability status or SDR content brightness setting.

**Value to DAC:** Makes display state clearer and could improve appearance consistency on HDR desktops.

Comparable evidence: [Monitorian](https://github.com/emoacht/Monitorian). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp Catalog](../src/platform.hpp), [src/platform.hpp PaintScene](../src/platform.hpp), [resources/dac.manifest](../resources/dac.manifest).

Implementation:

1. Start with read-only HDR state and visible qualification of black/scenes/slides; keep that deliverable independent of a brightness setter.
2. Investigate a supported SDR brightness setter for the pinned SDK/OS baseline. Monitorian's product history establishes the feature, not API suitability for DAC.
3. If only undocumented operations are available, retain read-only status and defer writing. Keep hardware brightness, SDR content brightness and overlay opacity distinct.

Acceptance:

- HDR state changes invalidate cached presentation assumptions safely.
- A prototype must establish API support, restoration and exact-target behavior before any setter is scheduled.
- Document visible black-level/clock results on representative HDR and SDR displays.

### F19 Local video and animated media presentations

Rank 23; priority 4.9/10; usefulness 5/10; popularity proxy 6/10; fit 5/10; effort 9/10; risk 7/10. Wave 4; initial engineering estimate 21-40 days. Dependencies: G05. Tags: optional, media, content.

**Current delta:** Photo extensions include GIF, but SlideFrame decodes one image frame; there is no timed animated GIF/video playback pipeline.

**Value to DAC:** Adds visual variety for users who prefer moving content to black or the existing scenes.

Comparable evidence: [Lively Wallpaper](https://github.com/lively-community/lively). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp Run](../src/platform.hpp), [src/platform.hpp SlideFrame](../src/platform.hpp), [src/platform.hpp SlideWorker](../src/platform.hpp), [src/platform.hpp AddRun](../src/platform.hpp).

Implementation:

1. Prototype a local-only media owner in a contained process using a Windows media pipeline, with bounded files and no browser/URL support.
2. Mute by default, enforce resource/frame limits, obey batteryBlack and the existing timed-to-black policy, and cancel by generation.
3. Fall back to native black on decoder/device failure. Preserve instant hide/cancel before asynchronous cleanup.

Acceptance:

- Malformed media, decoder hangs, device loss and rapid input cannot strand a cover or helper.
- Measure CPU/GPU/memory and dismissal responsiveness against native black and scenes.
- Confirm each output retains independent ownership; no burn-in effectiveness claim follows from motion.

### F18 Monitor contrast controls

Rank 24; priority 4.4/10; usefulness 4/10; popularity proxy 6/10; fit 4/10; effort 6/10; risk 8/10. Wave 4; initial engineering estimate 5-10 days. Dependencies: F13. Tags: optional, ddc, adjacent.

**Current delta:** DAC has neither contrast read/write nor generalized monitor settings.

**Value to DAC:** Offers a small convenience for users who already want DAC to act as a broader monitor controller.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Monitorian](https://github.com/emoacht/Monitorian). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp PowerChild](../src/platform.hpp), [src/policy.hpp Preference](../src/policy.hpp).

Implementation:

1. If the brightness foundation proves reliable, add a typed contrast operation with its own advertised capability and permission.
2. Keep raw arbitrary VCP writes out of ordinary controls; avoid adding input-source switching to this item because it can remove the recovery display.
3. Reuse normalization and ownership rules where applicable and provide a manual reset path.

Acceptance:

- Unsupported capabilities remain disabled and values are clamped to reported ranges.
- Timeouts cannot block input dismissal or reuse a stale monitor handle.
- Physical tests confirm recoverability without changing saved power grants.

### F21 Optional Windows screensaver bridge

Blocked while F01 is deferred; this dependent feature is retained for research only.

Rank 25; priority 4.2/10; usefulness 4/10; popularity proxy 6/10; fit 4/10; effort 8/10; risk 6/10. Wave 4; initial engineering estimate 11-20 days. Dependencies: F01, G01. Tags: optional, windows, screensaver.

**Current delta:** Hosting other savers through /p exists; DAC itself is not a registered Windows screensaver.

**Value to DAC:** Lets Windows launch DAC content for users who want an OS-managed screensaver entry.

Comparable evidence: [Lively Wallpaper](https://github.com/lively-community/lively). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/standalone.cpp Main](../src/standalone.cpp), [src/platform.hpp AddRun](../src/platform.hpp), [src/platform.hpp UserSessionScope](../src/platform.hpp).

Implementation:

1. Treat this as a deliberate scope decision. Prototype a separate bridge handling /s, /p and /c with bounded IPC and no hardware grants.
2. Define who owns idle detection when Windows launches it and prevent duplicate controllers or recursion into the bridge.
3. Make registration and removal explicit and reversible; do not silently replace the user's existing screensaver policy.

Acceptance:

- Fixtures cover protocol parsing, parent-window lifetime and missing controller.
- An isolated visible session confirms Windows invocation, stop behavior and restoration of prior settings.
- Secure Windows lock behavior is tested separately and is never inferred from a cover.

### F26 Pointer border resistance and routing

Rank 26; priority 2.9/10; usefulness 3/10; popularity proxy 5/10; fit 2/10; effort 10/10; risk 9/10. Wave defer; initial engineering estimate 21-40 days. Dependencies: G01. Tags: defer, coexistence, adjacent.

**Current delta:** DAC observes input; it does not redirect or constrain the cursor.

**Value to DAC:** Could reduce accidental crossover into idle screens, but duplicates a specialist tool and adds input risk.

Comparable evidence: [LittleBigMouse](https://github.com/mgth/LittleBigMouse). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/platform.hpp DeliverInput](../src/platform.hpp), [src/policy.hpp AttributeInput](../src/policy.hpp), [tests/nativewake-input-fixture.h](../tests/nativewake-input-fixture.h).

Implementation:

1. Prefer a documented coexistence recipe and compatibility tests with LittleBigMouse before considering a built-in router.
2. If a future fork explores routing, isolate it behind an opt-in adapter with a separate recovery shortcut and evaluate historical-event attribution.
3. Do not reuse current pointer position for queued events or let synthetic routing credit unrelated displays.

Acceptance:

- Coexistence tests cover high-rate input, boundary crossing, mixed DPI and all-covered wake.
- Disabling routing restores normal cursor behavior immediately.
- No input interception extension proceeds without proving that the core wake invariant and recovery remain reliable.

## Feature held by user decision

### F01 Full command interface and local IPC

Current rank: excluded by user decision; original research rank 1; priority 8.6/10; usefulness 9/10; popularity proxy 7/10; fit 10/10; effort 5/10; risk 5/10. Wave on hold; initial engineering estimate 5-10 days. Dependencies: none. Tags: automation, power-user, native.

**Current delta:** The public CLI can launch, stop, report version/help and select a config directory; it cannot list displays, query state, start/stop one presentation, choose a profile or snooze.

**Value to DAC:** Makes DAC usable from shortcuts, scripts and external controllers without editing live settings.

Comparable evidence: [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray), [Lively Wallpaper](https://github.com/lively-community/lively), [PowerToys Awake](https://learn.microsoft.com/en-us/windows/powertoys/awake). Evidence class: observed-comparable. The steps below adapt the idea to DAC; they do not assert implementation parity with the comparable.

Local anchors: [src/standalone.cpp Main](../src/standalone.cpp), [src/platform.hpp WindowProc](../src/platform.hpp), [src/platform.hpp UserSessionScope](../src/platform.hpp).

Implementation:

1. Define versioned request/reply structs and typed actions shared by tray and CLI. Add list, status, start, dismiss, snooze and profile actions; distinguish dismiss from existing --stop shutdown.
2. Use bounded local IPC with an explicit logon-SID ACL, remote-client rejection and user/session validation. Dispatch immutable requests to the UI owner; return snapshots with exit codes and a JSON output option.
3. Resolve stable IDs against a catalog generation. Keep the singleton scoped per user/session, reject unknown targets and require a separate hardware action that cannot grant permission.

Acceptance:

- Concurrent clients cannot create a second controller or race topology changes into a different target.
- Malformed/oversized requests, cross-session access, disconnected targets and busy/shutting-down owners fail predictably.
- Existing --stop semantics, emergency recovery and independent-peer idle histories remain intact.

## Platform references for prototypes

The command transport design should follow Microsoft's [named pipe access-control guidance](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights), including explicit access restrictions. Naming a pipe after a user/session is insufficient access control.

The system-awake proposal uses [PowerCreateRequest](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powercreaterequest), [PowerSetRequest](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powersetrequest), and corresponding clear/close cleanup. Windows documents limits on DC Modern Standby and user-initiated sleep; a system request is not a guarantee of indefinite execution.

Brightness adapters should start from [SetMonitorBrightness](https://learn.microsoft.com/en-us/windows/win32/api/highlevelmonitorconfigurationapi/nf-highlevelmonitorconfigurationapi-setmonitorbrightness) and the built-in-panel [WmiSetBrightness](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmisetbrightness-method-in-class-wmimonitorbrightnessmethods) documentation. Hardware support and responsiveness remain per-device questions. F17 deliberately requires a separate supported-API investigation for SDR content brightness.

The manual locking adaptation uses [LockWorkStation](https://learn.microsoft.com/en-us/windows/desktop/api/Winuser/nf-winuser-lockworkstation). It operates on the interactive workstation, not an independently lockable physical monitor; successful return requests an asynchronous transition rather than proving that locking completed.

All counterpart code and artwork remain external to this plan. Any later source reuse needs a file-level provenance and license review under [SOURCE_INVENTORY.md](SOURCE_INVENTORY.md); the GPL projects here supply product examples, not selected implementation inputs.
