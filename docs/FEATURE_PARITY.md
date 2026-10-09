# Standalone feature parity and qualification

This matrix accounts for the original standalone assignment and the verified
DAC/R01C functionality selected in [SOURCE_INVENTORY.md](SOURCE_INVENTORY.md).
It describes the current source and available test coverage. Execution results
are recorded separately.

[VERIFICATION](VERIFICATION.md) is the authority for current build, test,
static-analysis, package, and reproducibility results. Historical development
runs are not counted as fresh publication validation.

Test names below identify the fixtures that exercise a feature within the stated
scope. They do not imply a current pass or that every associated visible or
physical behavior was exercised. Historical Windhawk/R01C results do not qualify
this executable.

All physical display, real multi-monitor input, visible desktop, and DDC/CI
qualification remains **untested**. Hidden/private-desktop observations do not
establish compositor disappearance or input-to-photon latency.

Status meanings:

- **Implemented:** a source implementation exists within the stated scope;
  verification is recorded separately.
- **Incomplete:** part of the requested behavior or compatibility is missing.
- **Deferred:** intentionally requires another engineering or authorization step.
- **Unsupported:** deliberately outside this candidate's supported behavior.
- **Untested:** the behavior or environment has not been qualified; this may apply
  to an implemented feature.

## Product, host, and lifecycle

| Feature | Implementation status | Source / test coverage and limitations |
|---|---|---|
| Native standalone executable | Implemented | `src/standalone.cpp`, `wWinMain`; C++23/Win32 application. `standalone-cli-version`, `standalone-cli-help`, `standalone-cli-reject`, `standalone-host`. |
| Independent application startup/shutdown | Implemented | `Initialize`, `Shutdown`, standalone supervisor; `lifecycle`, `emergency`, `standalone-host`, `standalone-lifecycle`. Eight-second shutdown and thirty-second startup watchdogs request process termination if graceful work stalls. |
| One controller per user/session | Implemented | User SID/session object scope and authoritative named-mutex creation. `host-preferences`, `lifecycle`, `standalone-lifecycle`; the shipping executable's second-instance no-op and scoped stop are exercised on a private desktop. Real sign-in behavior remains untested. |
| Reliable ownership and cleanup | Implemented | UI-owned windows, worker-owned children/jobs, retained process handles, COM/GDI/input teardown. `containment`, `host-death`, `sessions`, `session-soak`, `lifecycle`. |
| Independent system-tray icon/menu | Implemented | Native generated DAC icon, grouped menus, explicit start/stop/exit, status and notices. `advanced`, `nextbeta`, `standalone-host`; real Explorer behavior untested. |
| Configurable primary tray click | Implemented | Start/stop, Quick Setup, or Advanced Settings; native `.host` preferences. `nextbeta`, `host-preferences`. |
| Emergency stop and ordinary exit | Implemented | Ctrl+Alt+Shift+F12, tray Exit, same-user/session `DAC.exe --stop`; cancellation and owned-job termination. `emergency`, `lifecycle`, `standalone-lifecycle`. The shipping executable's scoped stop is exercised privately; the emergency shortcut is simulated in harness tests. |
| Optional ordinary control hotkeys | Implemented | Per-monitor/current/all controls, configurable function-key assignments, conflict reporting. `advanced`, `nextbeta`; actual desktop hotkey registration untested. |
| Optional run at login | Implemented | Explicit HKCU Run registration, exact executable/profile command, foreign-value protection. `host-preferences` substitutes the registry; live registration and sign-in are untested and were not authorized. |
| Portable configuration-directory override | Implemented | `--config-directory` selects an existing fixed-local directory below the drive root; forwarded to startup and helpers. `host-preferences`, `standalone-host`. Root, UNC, mapped-drive, and reparse paths are unsupported. |
| Bounded explicitly requested run | Implemented | `--run-for-seconds 1..86400`; `standalone-host` uses an isolated profile and desktop. |
| No mandatory privileged host/service | Implemented | As-invoker manifest; ordinary user process. `SECURITY.md` and final PE checks. |
| No Windhawk/runtime browser/JS dependency | Implemented | No Windhawk callbacks/imports/injection; no Electron, Chromium, Node, React, or Mantine runtime. Build-time PowerShell/Git generate the Fujin adapter. Final import/package checks belong in `VERIFICATION.md`. |
| Windows 11 x64 qualification target | Implemented target; untested physical qualification | CMake rejects non-Windows/non-x64 targets. Windows ARM64, 32-bit product builds, and other operating systems are unsupported for this candidate. |

## Monitors, input, and wake reliability

| Feature | Implementation status | Source / test coverage and limitations |
|---|---|---|
| Monitor enumeration and stable identity | Implemented | `Catalog`, topology catalog and stable device identities; `catalog`, `advanced`. Physical connection/adapter identity stability untested. |
| Per-monitor enablement and preferences | Implemented | `Preference`, monitor mapping, settings drafts. `policy`, `advanced`, `nextbeta`. Disconnected preferences are retained. |
| Unidentified/ambiguous/clone display safety | Implemented | Ineligible targets are suppressed instead of guessing an independent hardware/presentation target. `catalog`, `policy`, `nativewake`. |
| Independent idle timers | Implemented | Per-node activity/generation/timestamp state. `policy`, `sessions`, `nativewake`. |
| Independent wake | Implemented; physical behavior untested | Targeted activity attribution and cancellation preserve unrelated display sessions and idle history. `policy`, `queued-wake-probe`, `nativewake`. This is not proof that the reported intermittent wake defect is fixed. |
| Explicit shared/global input | Implemented | Global and per-monitor input policies; shared behavior is intentional. `policy`, `nativewake`. Unknown input does not become unconditional wake-all for independent outputs. |
| Pointer-only and foreground-based input modes | Implemented | Per-monitor input modes and conservative foreground attribution. `policy`, `nativewake`; focus/media attribution is heuristic. |
| Mouse and keyboard attribution | Implemented | `WM_INPUT`, queued event position/time, raw-read fallback. `queued-wake-probe`, `nativewake`; physical devices and RDP routing untested. |
| Delayed/queued input treatment | Implemented | Historical pointer coordinates and event timestamps; stale current-focus data is not treated as historical evidence. `queued-wake-probe`, `nativewake`, `wakeperf`. |
| Activation, manual-start, and topology boundaries | Implemented | Reject input older than ownership/activation boundaries; generation-check late results. `policy`, `nativewake`. |
| Manual monitor/all activation | Implemented | Tray/manual controller paths and saved assignments. `policy`, `sessions`, `advanced`. |
| Sticky manual presentations | Implemented | Sticky state ignores normal activity until explicit stop/emergency recovery. `policy`, `nativewake`, `advanced`. |
| Spanning presentations | Implemented | Single spanning owner, only when all participating outputs are identified/enabled; no spanning DDC authority. `advanced`, `nativewake`. Per-saver spanning compatibility untested. |
| Stop/wake one display without stopping peers | Implemented | `StopMonitor`, targeted reconciliation; `sessions`, `nativewake`. Physical monitor wake is a separate opt-in DDC operation. |
| Display connection/topology recovery | Implemented | Display notifications invalidate owners, refresh the catalog, and establish fresh input boundaries. `catalog`, `sessions`, `nativewake`; real docking/unplug/replug untested. Topology reset is not advertised as preserving every active session. |
| High-DPI and negative-coordinate desktops | Implemented | Per-monitor-V2 manifest, settings relayout, signed queued positions, monitor bounds. `advanced`, `nextbeta`, `nativewake`; mixed-DPI physical viewing untested. |
| Session lock/disconnect/suspend handling | Implemented | WTS/session and power notifications block or reset owners; `sessions`, `nativewake`. Real lock/unlock, secure desktop, RDP handoff, suspend/resume, and Windows restart are untested. |
| R01C wake observability | Implemented | Bounded counters/histograms for raw reads, attribution, active-to-desktop transitions, cancellation, hide observations, coverage, and reset causes. `policy`, `nativewake`, `wakeperf`. |
| Emergency visibility recovery | Implemented; physical outcome untested | Independent stop watcher hides owned windows and cancels owned work. `emergency`, `faults`, `host-death`. A hung driver or powered-off display may still require physical recovery. |
| Resolution of intermittent all-covered wake report | Incomplete / untested | The original report was intermittent. Instrumentation and preserved invariants are present; reproducible physical qualification and field confirmation remain open. |

## Presentations and external content

| Feature | Implementation status | Source / test coverage and limitations |
|---|---|---|
| Native Black | Implemented | Opaque native cover and idle steady-state repaint behavior. `sessions`, `nextbeta`, `nativewake`, `wakeperf`. Software black is not a physical power-off or Windows lock. |
| Moving Clock | Implemented | Internally rendered low-luminance clock with moving position. `nextbeta` native-render fixtures. Real appearance/HDR/luminance untested. |
| Constellation | Implemented | Internally rendered drifting sparse scene; no `.scr` conversion required. `nextbeta`. |
| Native scene themes and rotation | Implemented | Existing dim neutral/violet/teal content palettes, optional clock/constellation rotation, scene duration. `policy`, `nextbeta`. Scene content preserves the legacy renderer; it is distinct from the Fujin settings-UI palette. |
| Dim/fade warning stage | Implemented | Layered nonactivating click-through dim cover, configured percent/fade/duration, immediate input reversal. `policy`, `nextbeta`. |
| Timed scene/saver-to-black transition | Implemented | Configurable black deadline and scene maximum; generation-safe retirement. `policy`, `sessions`, `nextbeta`. |
| Failure fallback to black | Implemented | Missing/hung/exited child and unusable image paths fall back to native black with notices. `faults`, `advanced`, `nextbeta`. |
| Multiple simultaneous per-monitor presentations | Implemented | Independent owners/workers and per-monitor selection; `sessions`, `session-soak`, `nativewake`. Real multi-monitor compositor behavior untested. |
| Independent local photo slideshow | Implemented | GDI+ worker, local folder selection, JPEG/PNG/BMP/GIF catalog. `slides`, `nextbeta`. Images remain in-process decoder inputs. |
| Slideshow order, timing, and recursive traversal | Implemented | Interval, optional shuffle, optional bounded recursion and entry count. `slides`, `policy`; large real libraries untested. |
| Slideshow placement/background | Implemented | Fit, fill/crop, stretch, original-size center, tile, fit without enlargement, configured background. `slides`, `advanced`. |
| Slideshow resource bounds/cancellation | Implemented | File catalog filtering, reparse rejection, pixel/output bounds, cancellation and black fallback. `slides`, `nextbeta`; decoder allocation/blocking behavior is a documented residual risk. |
| Installed Windows screensavers | Implemented hosting; compatibility untested | Fixed Windows system paths for Bubbles, Mystify, Ribbons, 3D Text, Photos, and Blank where installed. Actual Windows installations may omit choices. No installed saver was assumed compatible from a fixture result. |
| Explicit custom `.scr` selection | Implemented with trust boundary | Existing local `.scr` selection, path validation, exact executable invocation, owned job. `host-preferences` checks paths without executing the copied `.scr` fixture; `containment` uses reviewed test executables. A `.scr` runs with the user's permissions. |
| Per-monitor saver hosting and preview protocol | Implemented with compatibility limits | DAC hosts a monitor-sized preview child and invokes `/p HWND`. Savers that implement only `/s`, ignore the parent, or refuse preview hosting are unsupported by this host and fall back. There is no claim of arbitrary `.scr` compatibility. |
| Saver configuration dialogs | Implemented; actual saver settings untested | Explicit `/c:HWND` launch through owned containment. These dialogs may change shared Windows/saver preferences; automated tests use fixtures and do not configure installed savers. |
| Unsaved draft preview | Implemented | Separate resizable preview owner, no controller/hardware authority, automatic two-minute expiry, cancellation when settings close. `nextbeta`, `faults`. |
| Hung/failed child and host-death containment | Implemented | Suspended creation, assignment-before-resume, kill-on-close job, health check, bounded teardown. `containment`, `host-death`, `faults`. Jobs are lifecycle controls, not security sandboxes. |
| Network/mapped/reparse photo and saver sources | Unsupported | UNC shares, mapped drives, alternate streams, symlinks, and junctions are rejected by local-path validation. Copy trusted content to an ordinary local directory first. |
| Windows secure lock or OS screensaver registration | Unsupported | DAC owns presentations; it does not lock Windows, replace Windows lock policy, or register itself as the system screensaver. |

## Activity policies and automation

| Feature | Implementation status | Source / test coverage and limitations |
|---|---|---|
| Audio/media activity | Implemented heuristic | Core Audio endpoint/session observation, process/window attribution, ambiguity handling and stale-observation grace. `media`, `policy`, `nextbeta`; real media applications/devices untested. |
| Independent/global media policies | Implemented | Global and per-monitor opt-in/override behavior; `policy`, `media`. Ambiguous process/window relationships are handled conservatively. |
| Muted-media behavior and display requests | Implemented | Configurable muted-session treatment plus applicable Windows display-request observation. `policy`, `media`; this is not semantic video-playback detection. |
| Foreground application keep-awake rules | Implemented | Full executable path or explicitly less-precise filename matching, per-attributed-output/global scope. `policy`, `nextbeta`. |
| Application audio-ignore rules | Implemented | Scoped ignore rules in the policy/UI; `policy`, `nextbeta`, `media`. |
| Fullscreen foreground handling | Implemented heuristic | Window/monitor overlap, per-monitor policy; `policy`, `advanced`. Exclusive fullscreen, protected content, and unusual window managers untested. |
| Optional controller input | Implemented | System-only XInput loading, buttons/sticks/triggers/deadzones, stale-observation safeguards. `policy`, `nextbeta` injected adapter checks. Actual controller models/connections untested. |
| Named profiles | Implemented | Capture/load/delete policies, retained disconnected preferences, local base permission caps. `policy`, `nextbeta`. |
| Manual/app/schedule precedence | Implemented | Explicit manual profile, then foreground-app match, then schedule, then base; first matching entry and one-second automatic-selection debounce. `policy`, `nextbeta`. |
| Daily local-time schedules | Implemented | HH:MM intervals, overnight and exclusive-end handling, reevaluation on current local time. `policy`, `nextbeta`. No broader calendar or cloud scheduling claim. |
| Session pause | Implemented | Pause/reset without a saved global OS setting; `policy`, `sessions`, `advanced`. |
| Snooze and cancel snooze | Implemented | 5/15/30/60-minute tray choices, cancellation, and fresh automatic idle boundary after expiry. `policy`, `nextbeta`. |
| Battery-sensitive presentations | Implemented | Optional transition to black on DC power while preserving saved assignment and ownership boundaries. `policy`, `nextbeta` injected AC/DC checks. Physical battery/resume behavior untested. |
| Safe automation defaults and coexistence | Implemented | New configuration starts with automatic activation/DDC off. Read-only predecessor controller/OLED Aegis startup conflict checks cap automation. `policy`, `host-preferences`, `nextbeta`; no predecessor installation is modified. |

## Settings, design, and persistence

| Feature | Implementation status | Source / test coverage and limitations |
|---|---|---|
| First-run experience and Quick Setup | Implemented | Native welcome/setup with monitor selection and explicit automatic/manual decision. `nextbeta`; live usability untested. |
| Advanced Settings and per-monitor options | Implemented | Native draft controls for all ported monitor policies, presentations, activity rules, stages, hardware choices and profiles. `advanced`, `nextbeta`. |
| Display identification | Implemented | Temporary native labeled overlays and stable settings mapping; `advanced`, `nextbeta`; physical placement untested. |
| Validated transactional preferences | Implemented | Bounded schema/UTF-8 parsing, draft validation, canonical commit, atomic replacement, save failure retention. `policy`, `storage`, `nextbeta`, `host-preferences`. |
| Corruption/future-schema recovery | Implemented | Automatic activation withheld for invalid/unreadable preferences; explicit reset and preserved originals/backups. `storage`, `nextbeta`. |
| Legacy settings import | Implemented with deliberate safety change | Read-only INI import, current display-alias mapping, unknown/disconnected preference retention. Imported automatic/hardware authority is revoked across base/profiles. `policy`, `nextbeta`. |
| Portable profile/policy import/export | Implemented | Explicit monitor mapping and unsaved review; automatic/DDC grants removed. `nextbeta`, `policy`. Exported paths/identities are not anonymized diagnostics. |
| Full protection-configuration import/export | Implemented | `ExportSettings` / `ImportSettings` exchange monitor preferences, profiles, rules, schedules and base protection policy. `policy` exercises complete configuration roundtrip and `DisarmImportedConfig` checks, including retained rules/schedules and revoked base/profile authority. Import uses explicit confirmation and the backup/atomic save path. Saved identities are retained; use profile import for explicit remapping. File-picker/confirmation interaction is source-reviewed, not visible user acceptance. |
| Transfer of host appearance/tray settings, login registration and recovery tickets | Unsupported by design | These remain local to the application instance/machine and are not part of protection-configuration exchange. Export may contain configured paths and saved flags; import never treats those flags as renewed automation or hardware authority. Hardware recovery tickets must not be transplanted. |
| Reset selected/all preferences and backups | Implemented | Explicit confirmation, original-byte preservation for schema/reset operations, hardware recovery markers retained. `storage`, `nextbeta`. |
| Fujin pinned native token integration | Implemented | Actual `v0.1.0` tagged checkout, Git/SHA/schema verification, generated typed C++ header at configure and every build. `fujin-integrity`; [FUJIN.md](FUJIN.md). |
| Runtime light/dark/system appearance | Implemented | Native UI settings and Windows preference notifications. Persisted host preferences load before initial palette/icon creation. `nextbeta`, `host-preferences`; real visual/accessibility review untested. |
| Windows high-contrast behavior | Implemented | System colors/fonts override custom painting; state/draft retained. `advanced`, `nextbeta`; assistive-technology qualification untested. |
| Accent choices | Implemented at build time only | `DAC_FUJIN_ACCENT`: violet, indigo, blue, cyan, teal, green, orange. `fujin-integrity` generates all presets. Runtime accent selection is unsupported; the native UI offers system/light/dark appearance, not an accent picker. |
| Native layout continuity and DPI changes | Implemented | Ported Quick Setup/Advanced/profiles layout, DPI relayout, scroll bounds, focus/draft retention are exercised by hidden checks in `advanced` and `nextbeta`. Offscreen snapshots omit some native Edit painting; they do not establish that visible text fields render correctly. Physical mixed-DPI, keyboard and screen-reader acceptance remains untested. |
| Fujin consumer-ledger registration | Deferred | Proposed row is in `FUJIN.md`; writing `Fujin/CONSUMERS.md` requires separate authority. |

## Hardware, diagnostics, performance, and delivery

| Feature | Implementation status | Source / test coverage and limitations |
|---|---|---|
| Per-monitor DDC/CI opt-in | Implemented; hardware untested | Saved target-specific grant, identifiable single physical monitor requirement, off/wake VCP D6 cycle. `power`, `policy`, `nextbeta` use simulations only. Unsupported/ambiguous targets fall back. |
| Delayed/manual hardware-off request | Implemented; hardware untested | Configured deadline or explicit opt-in tray action, native black fallback. `policy`, `power`. |
| Nonce-bound helper ownership | Implemented | Matching ticket/event/parent/session/executable and rechecked permission. `power`, `hardware-helper-reject`; malformed CLI rejection is not a real hardware cycle. |
| Cancellation and attempted physical wake | Implemented; hardware untested | Owned helper cycle, cancellation and acknowledgement, bounded waits. `power`, `nextbeta`; physical power recovery is best effort. |
| Persistent fault quarantine/reset | Implemented | Ambiguous failures retain markers and disable retries until explicit recovery/reset; inaccessible marker is not treated as absent. `power`, `nativewake`, `nextbeta`. |
| Physical DDC monitor/adapter compatibility | Untested / deferred | No DDC query/off/on qualification was authorized. Required separate monitor/adapter matrix and physical recovery procedure. |
| Redacted local diagnostics | Implemented | Bounded events, controlled descriptions, path/identity redaction, local export. `nextbeta`, `nativewake`. Configuration/profile backups remain identifiable. |
| Conflict and recovery notices | Implemented | Read-only coexistence/screensaver queries, save/child/hardware faults, state explanations and countdowns. `advanced`, `nextbeta`, `faults`. |
| Input/paint/UI timing measurements | Instrumented; current results in verification report | `wakeperf`, `idleperf` and native scene fixtures in `nextbeta` measure synthetic dispatch, hidden painting and UI round trips. [VERIFICATION](VERIFICATION.md) records executed samples. Hidden timings are not input-to-photon measurements. |
| Idle CPU/memory/handle/GDI measurement | Instrumented; current results in verification report | `idleperf` samples CPU, working set, private bytes, handles and GDI objects on a private desktop. [VERIFICATION](VERIFICATION.md) records executed samples. Short harness measurements do not establish production-desktop efficiency or endurance. |
| Startup/shutdown and failed-operation cleanup measurement | Instrumented; current results in verification report | `idleperf`, `lifecycle`, `faults`, `containment`, `host-death`, `standalone-host` and `standalone-lifecycle` exercise startup, shutdown and cleanup. The shipping-process fixture uses a private desktop. [VERIFICATION](VERIFICATION.md) records executed samples and their scope. |
| Debug/Release native build and static analysis | Build and analysis paths implemented | Both native configurations and the production MSVC `/analyze` command are available. Windows SDK/toolchain headers alone are treated as external. Release PE inspection checks AMD64/Windows GUI, ASLR, DEP, high-entropy VA and CFG. Current outcomes belong in [VERIFICATION](VERIFICATION.md). |
| Portable ZIP, notices, checksums, package integrity | Final artifact record | [VERIFICATION.md](VERIFICATION.md) is the authority for the final portable ZIP, included notices/assets, hashes, package checks and reproducibility result. Build/test success is not used as a substitute for package verification. |
| Independent product version/history | Implemented | New DAC `0.1.0` line and standalone changelog, rooted in the new repository's initial commit. Historical Windhawk `v0.3.0` is provenance, not this product's previous release. |
| Selective source/asset reuse and licensing | Implemented inventory | Verified DAC/R01C files and original DAC geometry selectively ported under MIT; [SOURCE_INVENTORY.md](SOURCE_INVENTORY.md), third-party notices. No whole historical repository, Windhawk Git history, or unresolved-rights OLED Aegis source/artwork is imported. |
| Local development commits/review | Publication history and review | Publication work belongs to `feature/standalone-publication`, based directly on the independent repository history. [VERIFICATION](VERIFICATION.md) records the publication validation. No remote operation is implied. |
| Push/PR/merge, signing, release, deployment | Deferred | No remote mutation, release publication, live installation or startup activation is authorized. [GITHUB_INTEGRATION.md](GITHUB_INTEGRATION.md) describes the subsequent review sequence. |

This candidate must not be described as production-ready or as having achieved
physical feature parity solely because the source is implemented or automated
tests pass. Unavailable, skipped, and unexecuted checks remain visibly distinct
from successful checks in the final verification record.
