# DAC — Display Activity Controls

DAC is a lightweight native Windows application for protecting idle displays
independently. It has its own executable, tray icon, settings, and lifecycle.
It requires no Windhawk, browser runtime, service, administrator rights, account,
internet connection, or telemetry.

**0.1.0 development candidate; Windows 11 x64 qualification target.** Build and test
results and remaining limits are in [verification](docs/VERIFICATION.md)
and the [feature-parity matrix](docs/FEATURE_PARITY.md). Synthetic/private-desktop
tests do not establish physical multi-monitor reliability or production readiness.

DAC succeeds [Display Activity Controls for Windhawk](https://github.com/StarlightDaemon/dac-windhawk).
This repository has independent history and versioning and does not depend on
the predecessor repository or its release artifacts. MIT source was selectively
ported from verified historical sources; see [source provenance](docs/SOURCE_INVENTORY.md).
The old Windhawk v0.3.0 release belongs to that separate product line.

## Use

Extract the portable package to a directory you control and run `DAC.exe`.
Automatic protection and hardware power control are off initially. Quick setup
opens on first run; choose each display's presentation and idle timeout, save,
then deliberately enable Automatic when ready. Close any earlier display
controller before enabling DAC. Existing installations are not modified.

- Right-click the tray icon for Quick setup, Advanced settings, profiles,
  display-specific start/stop, pause, snooze, diagnostics, appearance, and exit.
- Left-click toggles protection by default; the tray preference menu also offers
  Quick setup, Advanced settings and Display controls as the left-click action.
- Open **Display controls…** from the tray for connected-display state, reasons,
  countdowns, Start, Stop, temporary keep-awake and the selected display's settings.
  Select a display with the arrow keys, Tab to actions, and Escape to close.
- Open **Visual display map…** from the tray, or **Map** in Display controls,
  to select the current desktop arrangement spatially or with the native list.
  Selection only navigates; it does not change Windows layout or start protection.
- Open **Diagnostics → Display inspector / OS-only rescan…**, or **Inspect** in
  Display controls. Local details show identity, geometry, eligibility, reasons,
  deadlines and cached quarantine/recovery. **Rescan** reads Windows display
  information on a worker. Hardware compatibility remains **not tested / unknown**;
  saved hardware permission is not proof of support. No live DDC probing is added.
- In **Advanced settings → Profiles / application rules**, choose **Add background
  running rule…**. Select an executable (full path preferred), enablement, and
  explicit global scope or stable display targets. Apply the rule to the parent
  draft, Apply rules, then Save. Edit/disable/delete there; Cancel discards edits.
  Any matching instance in your user/session inhibits DAC automatic protection
  for those targets regardless of focus. Explicit filename-only matching is
  broader. Unknown observations retain a prior match for at most five seconds,
  then release its contribution with a fresh idle interval and visible uncertainty.
  Rules do not prevent Windows sleep or grant hardware permissions.
- Set **Friendly name** in Advanced settings, then Save. Names apply across all
  profiles and survive disconnection. Close without saving cancels edits. Empty
  names use the OS label; Display N distinguishes duplicate names. The limit is
  64 Unicode characters (256 UTF-8 bytes); controls and directional overrides
  are rejected. Names appear in setup, tray menus, identification and the flyout.
- In Display controls, keep a selected display available for **5, 15, 30 or 60
  minutes**, a custom **1–1440 minutes**, or until a future local
  **YYYY-MM-DD HH:MM** within 24 hours. The resolved local/UTC expiry and remaining
  time are shown. Missing or repeated DST times are rejected; use a duration.
  Later wall-clock/time-zone changes do not move an accepted deadline.
  **Keep awake inhibits DAC automatic protection only. Windows may still sleep,
  and a physical monitor may still power off.**
- A new keep-awake request replaces that display's timer; Cancel resumes its
  normal idle countdown with a fresh boundary. Global snooze also has to end.
  Stop a manual/sticky presentation before creating a timer. A later explicit
  Start overrides the timer while the manual presentation runs; the timer remains
  available afterward until cancelled or expired. Stop a desktop span from the
  tray before using individual controls. Disabled or unidentified targets cannot
  acquire timers. Timers survive lock/suspend in the same process, count suspended
  time, disappear on disconnect/ineligibility, and clear on exit/restart.
- Choose native Black, Moving Clock, Constellation, photos, or a trusted installed
  Windows screensaver. A custom `.scr` is a program running with your permissions;
  only select one you trust. DAC never downloads savers.
- Independent input is the default: mouse activity credits its display; fresh
  keyboard activity may also credit the focused display. Delayed/unknown input
  does not wake unrelated independent displays. Shared and spanning modes are
  explicit alternatives. Each presentation retains its own idle history.
- `Ctrl+Alt+Shift+F12` stops protection and exits DAC. `DAC.exe --stop` requests
  shutdown of this user's controller in this session. Black or moving content
  does not lock Windows or guarantee burn-in prevention.

Appearance follows Windows unless Light or Dark is chosen; Windows high contrast
takes priority. Fujin v0.1.0 is the pinned design-token source; a verified build-time
adapter generates the native theme. No Node/React/Mantine runtime ships.

## Settings, startup, and removal

Preferences normally live in `%LOCALAPPDATA%\DAC\settings-v1.ini`; the `.host`
companion stores appearance, left-click, and startup-window preferences. Profiles,
rules, schedules, and monitor choices use the native settings editor. Exported
configuration can contain paths and monitor identities; redacted diagnostics are
a separate export and exclude friendly names and applications. Schema 5 retains
global names outside profiles and adds enabled running rules with explicit target
scope; the first save of schemas 1–4 verifies a byte-exact migration backup
(`.v1-backup` through `.v4-backup`). Full settings exports include names;
profile-only exports omit device names. Runtime keep-awake timers are never
exported or imported. Import disables automation and hardware grants for review.
Startup registration is machine/user state and is not imported with preferences.

Implementation, current verification and remaining visible/physical acceptance:
[F02/F03/F05 record](docs/FEATURE_IMPLEMENTATION.md).
The [F16/F04/F11 record](docs/INSPECTOR_MAP_BACKGROUND.md) describes the inspector,
map, running rules, current verification and remaining visible/physical acceptance.

Run at login is optional and off until **Start DAC when I sign in** is checked
in the tray menu. To remove DAC, uncheck that option, exit DAC, then delete the
extracted directory. Optionally remove its independent `%LOCALAPPDATA%\DAC`
preferences after saving any wanted export. Keep the executable in the same place
while login startup is enabled. No installer, updater, or service is installed.

The development workflow does not launch DAC on the operator's input desktop,
register login startup, modify existing installations, or exercise DDC/CI. Optional
hardware power support remains unqualified for real monitors; see [security and
recovery limits](docs/SECURITY.md).

## Build and test

Use C++23, CMake, MSVC and Windows SDK, plus PowerShell 7 and Git for build-time
verification. The independent portable toolchain bootstrap downloads pinned
Microsoft payloads into `.tools` and extracts them without installing anything.
Its provenance, exact versions, and observed metadata anomaly are documented in
[TOOLCHAIN](docs/TOOLCHAIN.md). Alternatively use an existing x64 VS developer shell.

```powershell
pwsh -NoProfile -File tools/bootstrap-toolchain.ps1
pwsh -NoProfile -File tools/bootstrap-fujin.ps1
. ./tools/use-toolchain.ps1
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel 3
ctest --test-dir build/release --output-on-failure
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 3
ctest --test-dir build/debug --output-on-failure
pwsh -NoProfile -File tools/package.ps1 -BuildDirectory build/release
```

Fujin can be bootstrapped from an existing verified local checkout; see
[FUJIN](docs/FUJIN.md). Subsequent builds work offline. Build dependencies stay
outside the portable binary package. Automated Windows tests create a private
noninteractive desktop; they never switch to it. They use owned fixture programs
and simulated power operations, not downloaded/custom savers or physical DDC.

The validation-only [Windows CI workflow](docs/GITHUB_INTEGRATION.md#windows-ci-foundation)
reproduces the pinned build, tests, analysis and package checks on GitHub Actions.

The portable package uses the explicit file list in `tools/package-manifest.json`.
Raw build/test logs and local validation receipts remain outside the package.

See [architecture](docs/ARCHITECTURE.md), [changelog](CHANGELOG.md),
[notices](THIRD_PARTY_NOTICES.md), and the [proposed GitHub integration sequence](docs/GITHUB_INTEGRATION.md).

For future development, see the [open source comparison and ranked feature roadmap](docs/COMPARATIVE_REVIEW.md),
the [feature catalog](docs/FEATURE_CATALOG.json), and the [implementation guidelines](docs/IMPLEMENTATION_GUIDELINES.md).
