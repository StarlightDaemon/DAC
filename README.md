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
  Quick setup and Advanced settings as the left-click action.
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
a separate export. Legacy import disables automation and hardware grants for review.
Startup registration is machine/user state and is not imported with preferences.

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

The portable package uses the explicit file list in `tools/package-manifest.json`.
Raw build/test logs and local validation receipts remain outside the package.

See [architecture](docs/ARCHITECTURE.md), [changelog](CHANGELOG.md),
[notices](THIRD_PARTY_NOTICES.md), and the [proposed GitHub integration sequence](docs/GITHUB_INTEGRATION.md).
