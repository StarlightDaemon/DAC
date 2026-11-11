# Native standalone architecture

Tray maintenance borrows immutable host icons, initialized before the worker and
deleted only after it joins. Requests never copy or destroy them; stale-result
and pending-request ownership remain bounded. See the
[resource attribution record](GDI_ATTRIBUTION.md).

DAC is a C++23 Win32 executable with no mandatory service, elevated host, browser,
network account, injection, or Windhawk dependency.

`src/dac.cpp` is the same-source assembly point for production and tests.
`src/policy.hpp` owns validated configuration, serialization, profile resolution,
monitor policy state, input boundaries, and diagnostic transition accounting.
`src/platform.hpp` owns Win32 adapters, presentation windows, contained children,
media observation, settings/tray UI, persistence, and helper recovery.
`src/standalone.cpp` owns executable dispatch and bounded process shutdown.
The [F16/F04/F11 record](INSPECTOR_MAP_BACKGROUND.md) describes the subsequent
schema 5 extension. `display-snapshot.hpp` owns bounded immutable OS scan results;
the UI accepts them against topology/configuration epochs. `inspector.hpp` and
`display-map.hpp` read the accepted catalog and cached controller observations.
`background-policy.hpp` owns timestamp/epoch-aware rule matching and limited
grace; `background-process.hpp` owns the lazy one-Hz user/session worker and RAII
query handles. `background-settings.hpp` adds the nested native rule draft.
Background-only edits preserve independent peer state and accepted media samples.
Physical DDC probing remains deferred; cached state never proves compatibility.
The generated `generated/fujin.hpp` is included inside the DAC namespace and
contains typed palette/typography/metric values from the pinned dependency.

Schema 4 adds `Config::nicknames`, a bounded stable-ID map outside `Policy` and
profiles. `nickname.<hex UTF-8 identity>=<hex UTF-8 name>` avoids delimiter and
native-menu interpretation. Schemas 1–3 remain readable with empty metadata;
nickname keys require schema 4 and are forbidden in nested profile bodies.
`DisplayLabel` formats labels for all display surfaces, with numbered visual
disambiguation. Identity, grants, recovery and diagnostics continue using their
existing keys/approved aliases. Cosmetic saves do not reset effective policy or
presentation generations. Explicit reset/import still cancels owners. Migration
verifies the original bytes in a schema-specific backup before replacement.

`Controller::leases` owns at most one temporary automatic inhibitor per display.
It is runtime state, retained through session resets and eligible profile changes,
pruned on disconnect/ineligibility/expiry, and cleared at process shutdown.
Grant/replacement/cancellation/expiry mutate only the selected node. Cancelling
or expiring an eligible nonmanual target starts a fresh idle interval. Existing
shared input attribution is unchanged; applying a lease never fabricates input.
Manual/sticky owners remain authoritative, and an active span rejects per-display
actions. A manual start may run through a retained lease. Global snooze and leases
are independently required to end before automatic activation can resume.

The Windows adapter uses native `GetTickCount64`, including suspended time, for
lease deadlines. Absolute local input is resolved once using the applicable
Windows time-zone rules, checking both standard/daylight offsets by UTC-to-local
round trip. Gaps and folds fail explicitly. The wall-clock sample is paired with
the monotonic origin after time-zone lookup; later clock changes do not retime it.
See Microsoft's [Windows time documentation](https://learn.microsoft.com/en-us/windows/win32/sysinfo/windows-time).

`src/display-controls.hpp` contains only guarded in-process UI-owner actions and
the civil-time resolver; it introduces no transport or external command surface.
Actions capture stable identity plus a topology epoch and reject pending/stale
targets. Cancellation calls `ReconcileWake` immediately to hide/cancel owners;
the existing timer performs deferred launch and cleanup. `src/flyout.hpp` reads
cached catalog/controller state, retains native controls and edit drafts during
countdown refresh, and uses Fujin themes, system high contrast, DPI layout and
native scrolling. It does no synchronous filesystem/media/shell/hardware probing
when opening or refreshing. F01 remains on hold.

The UI thread owns the controller and native windows. Raw input uses the queued
message timestamp and signed pointer position, rather than current pointer
coordinates. Unknown scope never becomes implicit wake-all. Wake reconciliation
only cancels stale owners and hides their shells; file, shell, media and child
cleanup happen away from the input dismissal path. Fixed-size aggregate counters
describe those stages without retaining input values or window titles.

Media and shell/filesystem maintenance use workers. Screensaver processes start
suspended, enter a kill-on-close job, and only then run. Hardware commands require
explicit saved opt-in and nonce-bound tickets; a separate helper owns off/wake
and records uncertain results for quarantine. No test issues physical DDC calls.

Application object names are scoped to the current Windows user SID and session.
The primary owns the stop event and singleton; command clients can request stop
without injecting code. Graceful shutdown cancels work, hides presentations, and
joins owners. The executable supervisor imposes a final process-exit deadline
when an OS call does not return. Hardware recovery remains best effort when a
driver is stuck; quarantine records must not be silently discarded.

The same-source `DAC_POLICY_ONLY` path excludes Win32 presentation/runtime code.
`DAC_HARNESS` uses isolated named objects, settings next to its test executable,
hidden native windows, and simulated hardware/input boundaries. These fixtures
do not establish physical desktop, compositor, monitor, or driver qualification.

The shipping executable also accepts `--config-directory` for a separate portable
profile and `--run-for-seconds` for an explicitly bounded session. The directory
must already exist on a fixed local drive, below its root, without symlinks or
junctions. It is never created implicitly. Default launches use
`%LOCALAPPDATA%\DAC`. The override travels with an explicitly enabled login entry
and any hardware helper, avoiding a silent return to the default profile.
The validation runner may use these options on its private, noninteractive
desktop, with a fresh build-local directory and no hardware opt-in.

Local custom savers and slideshow paths reject UNC/mapped drives, alternate data
streams, and reparse points in every component. The opened target's final path
and attributes must still match. This reduces accidental remote content loading;
it is not a sandbox for deliberately selected executable `.scr` files or image
codecs. Source paths can still be replaced by their owner after validation.
Legacy import copies preferences but always revokes automatic activation and
hardware permissions across base and profile policies. Review and explicit local
enablement are required after import.

Windows may stall inside a filesystem, codec, COM, shell, or monitor driver call.
Graceful shutdown has eight seconds before the independent supervisor requests
termination of the application process. Application startup has thirty seconds.
Owned child jobs terminate on handle closure; this is not an absolute guarantee
that a stuck kernel driver returns or that a physical monitor wakes. DDC recovery
uses nonce-bound tickets and preserves uncertain state for quarantine. Forced
process termination intentionally skips normal in-process destructors.
