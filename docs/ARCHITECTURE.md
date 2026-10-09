# Native standalone architecture

DAC is a C++23 Win32 executable with no mandatory service, elevated host, browser,
network account, injection, or Windhawk dependency.

`src/dac.cpp` is the same-source assembly point for production and tests.
`src/policy.hpp` owns validated configuration, serialization, profile resolution,
monitor policy state, input boundaries, and diagnostic transition accounting.
`src/platform.hpp` owns Win32 adapters, presentation windows, contained children,
media observation, settings/tray UI, persistence, and helper recovery.
`src/standalone.cpp` owns executable dispatch and bounded process shutdown.
The generated `generated/fujin.hpp` is included inside the DAC namespace and
contains typed palette/typography/metric values from the pinned dependency.

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
