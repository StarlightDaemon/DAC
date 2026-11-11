# Qualification remaining after the local development candidate

The [resource attribution follow-up](GDI_ATTRIBUTION.md) records the removal of
tray-icon allocation churn and corrections to timer-disabled mailbox progress
and media-policy isolation in resource fixtures. Earlier failed captures remain
preserved; passing repeats do not identify their historical object owners or
generation-reset causes. Final full suites passed 30/30 in Release and Debug.
Four new thirty-minute cases completed 30,720 lifetimes with zero resource
violations and verified cleanup, but all four remain **FAIL** after received
session notifications changed the target generations. A private desktop does
not isolate Windows session events; stable-session endurance remains pending.

For the subsequent F16 inspector/OS-only rescan, F04 native map and F11 background
rules, use the concrete [separate visible-session checklist](INSPECTOR_MAP_BACKGROUND.md#separate-qualification-checklist).
Hidden native fixtures do not establish keyboard/screen-reader acceptance, actual
mixed-DPI/topology/session behavior or physical reliability. Live DDC capability
probing stays deferred. The earlier intermittent wake issue remains open.

The [GDI lifetime follow-up](GDI_LIFETIME_INVESTIGATION.md) associates the delayed
six-object variation with native input initialization. Its
[independent review](GDI_REVIEW.md) adds verified repeated focus, delayed sampling
and strict GDI/settled-USER retention checks; object ownership remains unproven.
It does not replace visible IME/accessibility or long-duration
qualification, and does not disable native input in shipping DAC.

The subsequent [extended resource record](GDI_ENDURANCE.md) contains four
30-minute private-desktop trajectories and 30,528 focused lifetimes. No continuing
GDI/USER increase was observed, but none is fully passing qualification: three
cases crossed strict transient limits, and both Release cases failed their final
generation assertion before verified graceful shutdown. A separate tray probe
reproduced matching temporary allocations with cleanup; the original sample
owners and Release reset causes remain unknown. Preserve these failures when
planning attributed repeats. This bounded hidden work does not qualify real
desktop hours/days, visible accessibility or physical behavior.

Target: Windows 11 x64. The automated suite uses private desktops, generated
fixtures, and simulated hardware/input observations. It never switches to the
operator's input desktop. These results do not qualify a deployed application.

The combined F02/F03/F05 implementation and its development verification are
recorded in [FEATURE_IMPLEMENTATION.md](FEATURE_IMPLEMENTATION.md). That record's
development Release/Debug suites each passed 28/28 tests with zero production
static-analysis findings; this is not publication validation.
No new visible or physical authority was
granted by implementing these features. Use this concrete acceptance checklist in
a separately authorized isolated visible session:

- At 100%, 150% and 200%, and moving between different-DPI displays, open Display
  controls from the tray and through the configurable click action. Check every
  row/action/help/error with one, three and many displays, scrolling, focus rings,
  Tab/Shift+Tab, arrow selection, Escape, screen-reader names/order, light/dark,
  system changes and high contrast. Verify selected-row identity and that timers
  update without moving edit caret or focus.
- Save/Cancel empty, duplicate, long, non-Latin, astral and ampersand names in
  Advanced settings. Compare setup, tray, overlays and flyout. Switch profiles,
  reorder/unplug/replug, and retain disconnected names. Test failed Save and
  explicit import review/rollback using disposable local files. Inspect redacted
  diagnostics for absence of the chosen personal names.
- On each eligible output use all presets, custom minimum/maximum and until;
  verify resolved expiry, invalid date/range errors and DST gap/fold errors.
  Replace/cancel/expire one timer while peers protect, including shared input,
  media, global snooze, app/profile changes, manual/sticky starts and active spans.
  Check immediate selected automatic dismissal and fresh idle after final
  inhibition. Disconnect/ineligible/reconnect must not resurrect a timer.
- In that isolated session test lock/unlock, suspend/resume and clock/time-zone
  changes. Timers count suspended time and retain their accepted deadlines;
  restart clears them. Recheck emergency exit and existing `--stop`, resource use
  across repeated panel opens and the separate intermittent all-covered wake case.
- Physical monitor-power/DDC acceptance still requires separate explicit approval,
  an operator and a recovery route. Keep-awake does not change system sleep or
  guarantee that hardware remains on. No physical wake fix is claimed.

## Automated verification scope

[VERIFICATION](VERIFICATION.md) records fresh publication validation, including
source identities, Debug/Release suite outcomes, static analysis, package checks,
and reproducibility. Historical development runs are not reused as current passes.

The automated fixtures cover:

- Policy validation and transitions, input/media/mute/poll combinations, and an
  eight-hour **virtual-time** soak. Protection-configuration roundtrip and
  `DisarmImportedConfig` checks retain rules/schedules while revoking imported
  base/profile automatic and hardware authority.
- Queued-input wake attribution and R01C accounting, independent peers, activation
  boundaries, partial/full/empty coverage, native hide observations and synthetic
  timing measurements.
- Storage failure/recovery, catalog handling, process and child-tree containment,
  host death, cancellation, repeated lifecycle/session ownership, simulated power
  tickets/quarantine, slideshow placement and malformed-image fallback.
- Hidden native settings, drafts, profiles/rules, DPI relayout,
  light/dark/high-contrast state, previews, scenes, controller and battery
  observations, and redacted diagnostics. Full settings file-picker/confirmation
  behavior remains source-reviewed; these fixtures are not visible UI acceptance.
- Shipping-executable version/help/rejection paths, invalid hardware-helper
  rejection, bounded private-desktop startup, second-instance no-op, retained
  native ownership and scoped `--stop`, with build-local test profiles.
- Fujin integrity/schema/generation checks. Startup registration uses an injected
  store; hardware tests use simulated operations.

Timing, CPU, memory, handle and GDI samples are bounded private-desktop
observations. They do not establish live compositor performance, physical wake,
or endurance. Development measurements left cold-process resource retention
unattributed; long-duration resource qualification remains open.

## Remaining desktop and hardware qualification

| Separate qualification | Required observations |
| --- | --- |
| Core physical wake | One, two, and three monitors; each native style; every target; empty/partial/all-covered presentations; mouse movement/click/wheel/keyboard and boundary crossing. Only intended independent display wakes; peers retain idle history and generation. |
| Earlier intermittent incident | Repeat all-covered scenarios with real high-rate input, focus restoration, background load, delayed queues, and long idle intervals. Capture redacted counters and exact executable hash if dismissal fails. The incident is not declared fixed by removing Windhawk. |
| Shared/sticky/span | Confirm explicit shared scopes, sticky manual control and spanning behavior; verify emergency exit regardless of presentation state. |
| Display lifecycle | Hot plug, reorder, resolution/orientation change, multi-GPU, mixed DPI, HDR, remote/console handoff, and reconnect. Check stable identities, boundaries, preserved preferences and safe fallback. |
| Native settings accessibility | Visual native edit/control rendering, keyboard navigation, screen reader names/order, focus indication, high contrast, light/dark startup persistence, 100–200% DPI and multiple-monitor transitions. Development snapshots omitted some native Edit painting despite successful hidden control state/value checks. Snapshots also omit nonclient behavior and cannot qualify visible rendering or accessibility. |
| Real tray and lifecycle | Explorer restart, duplicate launch, first run, graceful/emergency exit and tray interaction on the input desktop, login startup enable/disable, moving a portable installation, user sign-out, Windows restart/update, suspend/resume and lock/unlock. The shipping executable's duplicate-owner and scoped-stop fixtures use a private desktop; they do not replace this visible-session qualification. No live startup registration was performed. |
| Real media and activity | Browser/native audio, muted sessions, fullscreen games/apps, multiple endpoints, app rules, controller input and AC/battery transitions. Heuristic attribution needs observed field behavior. |
| Content and .scr compatibility | Trusted installed Windows saver animation, `/p HWND` preview protocol, concurrent per-display hosting, saver configuration behavior, representative local photographs/placement/order and graceful fallback. Automated fixtures cover six slideshow placements, a malformed image and contained test executables. They do not qualify each installed/custom saver or the full supported image corpus. No arbitrary `.scr` executable was run. Savers that implement only `/s` or ignore the preview parent are outside this host's supported protocol. |
| Settings transfer and visible review | Exercise full protection-configuration export/import, file pickers, default-No review, successful backup, rejected input, stale open drafts and subsequent explicit enablement in an authorized visible session. Roundtrip/disarming/atomic save fixtures cover the underlying mechanisms; appearance/tray preferences, login registration and recovery tickets deliberately remain local. |
| Hardware (separate explicit authority) | Exact monitor/adapter DDC capability, off, cancellation, wake, timeout and fault quarantine, with a physical recovery method and operator present. A killed helper does not guarantee physical wake. |
| Long duration | Hours/days of real desktop use and repeated topology/session changes; working set/handles/GDI/CPU and responsiveness under representative content. The virtual eight-hour policy test and short hidden cycles are not endurance qualification. |

Before any visible test, authorize an isolated non-production user session or
machine, retain a known-working recovery route, and use a fresh DAC-specific
profile with automation and hardware off. Approve physical display-power testing
separately. This checklist does not itself grant execution authority.

Independent Windows x64 CI has now run successfully on a clean GitHub-hosted
runner: [branch push](https://github.com/StarlightDaemon/DAC/actions/runs/37877951396)
and [PR synthetic-merge validation](https://github.com/StarlightDaemon/DAC/actions/runs/37877951558).
Both runs completed successfully, including Release and Debug 27/27 suites,
static analysis, pinned-input checks, reproducibility, packaging and privacy audit.
These automated results do not qualify visible desktops or physical monitors.
Required-check/branch-protection configuration remains a separate decision.

Distribution decisions still pending: Authenticode signing, public prerelease
naming and publication, Fujin consumer-ledger registration, and broader OS/
architecture support. No `1.0.0` claim is made.
