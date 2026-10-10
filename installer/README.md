# DAC Windows installer

Development candidate: Inno Setup 7.1.0, Windows 11 workstation AMD64 only,
current-user installation at `%LOCALAPPDATA%\Programs\DAC`, `asInvoker`, offline
product installation. Stable AppId: `{BB5697BB-B07F-4DD9-BD9B-8991BDF19A64}`.
There is a Start Menu shortcut, an unchecked optional desktop shortcut and an
Installed Apps uninstall entry. Setup never runs DAC, registers login startup,
enables protection or uses DDC. The executable is unsigned; its adjacent SHA-256
receipt is an integrity record, not a signing or publication claim.

## Lifecycle policy

Setup creates `.dac-lifecycle.lock` in the installation directory before writing
product files. Every installed invocation, including CLI and power helpers, opens
a shared read handle before processing arguments. The OS retains it until complete
process teardown, beyond application shutdown and singleton release. Multiple user
sessions share the same file. Setup and uninstall require an exclusive handle and
hold it for their entire operation. Installed launches during maintenance exit 5.

Portable ZIPs contain no sidecar. Portable instances retain the existing
user/session singleton. Setup never sends the shared stop command or terminates
a product process. An existing executable at the target path requires both the
sidecar and matching current-user uninstall entry; otherwise replacement is
refused. Older portable installations cannot be adopted. Exit installed hosts and
helpers yourself and retry. The Windows executable-image write probe is an
additional check, not proof based on singleton disappearance.

Uninstall leaves preferences, profiles, backups, recovery tickets, quarantine and
unknown files intact. The empty inert sidecar remains, preserving exclusion during
uninstall and later reinstall. The installation directory may remain nonempty.
Uninstall never removes a Run entry: if DAC references this installed executable,
disable login startup in DAC and exit it first. Foreign portable ownership remains.

This is a lifecycle boundary, not a sandbox against same-user tampering.
Reparse-point installation paths and lease files are rejected. Directory
redirection, elevation overrides and closing/restarting overrides are refused.
No reboot-delayed replacement is requested.

## Reproduce locally

Use PowerShell 7 and a reviewed source revision in an isolated worktree. Existing
DAC native toolchain, Fujin, regression, static analysis and privacy gates remain.

```powershell
./tools/ci-windows.ps1 -Phase Bootstrap
./tools/ci-windows.ps1 -Phase Release
./tools/ci-windows.ps1 -Phase Debug
./tools/ci-windows.ps1 -Phase Analyze
./tools/ci-windows.ps1 -Phase Package
./tools/ci-windows.ps1 -Phase Audit
./tools/bootstrap-installer.ps1
./tools/test-installer.ps1 -AuditOnly
./tools/build-installer.ps1
./tools/test-installer.ps1
./tools/verify-installer.ps1 `
    -Installer build/installer/DAC-Setup-0.1.0-dev-windows-x64.exe `
    -Archive build/ci-package/DAC-0.1.0-dev-windows-x64.zip
```

Bootstrap checks archive SHA-256, size and valid pinned Authenticode certificate
before provisioning. Upstream portable mode suppresses toolchain registration,
associations and icons. A pinned complete extracted inventory and ISCC hash are
checked before each build; engine version must be exactly 7.1.0. Use `-Offline`
after caching/provisioning. Signature verification depends on Windows trust and
cached chain/revocation information; an unavailable trust check fails closed.

The digest is independently recorded by the
[immutable upstream release](https://github.com/jrsoftware/issrc/releases/tag/is-7_1_0)
and [Microsoft winget manifest](https://github.com/microsoft/winget-pkgs/blob/master/manifests/j/JRSoftware/InnoSetup/7/7.1.0/JRSoftware.InnoSetup.7.installer.yaml).
The [official download page](https://jrsoftware.org/isdl.php) identifies
Pyrsys B.V. as publisher. Portable provisioning follows
[upstream isportable.iss](https://github.com/jrsoftware/issrc/blob/is-7_1_0/isportable.iss).

Build consumes a verified normalized ZIP and matching external receipt. Every one
of its 19 allowed files gets an explicit compiler entry; no wildcards, private
logs, source, compiler or dependencies enter Setup. Staging, logs and metadata
remain ignored. Existing output directories are refused; select a fresh directory
for a retry. Verification checks the output receipt, AMD64 PE, unsigned status,
compiler/source/include identity and input package. Installed verification derives
every payload hash from archive bytes and requires exactly the 19 product files
plus the sidecar and Inno's two uninstall files.

The publication audit admits only the exact installer script as source text.
Its optional `-Installer` scan inspects shipping EXE ASCII/UTF-16 strings.
One complete public ASCII token can coincide with a runtime username/hostname:
the audit classifies that exact token hash only after confirming the unchanged
pinned upstream Setup.e64 hash and whole token. It records recognized collisions;
all path, credential and other identity checks remain intact.

## Fixtures and remaining qualification

`test-installer.ps1` compiles the same source with fixed fixture-only filesystem
and shortcut paths. Its compile-time fixture code redirects HKCU to a unique
`Software\DAC-Installer-Fixtures\<GUID>` subtree before install/uninstall. These
paths and this code are absent from shipping Setup. The existing test runner puts
the worker on a newly created desktop never switched to the input desktop, inside
its owned kill-on-close job. The worker requires a non-elevated token. Native
holders use production lease code, release synthetic singletons while remaining
alive, and retain process handles to confirm complete exit. Fixture files and
registry records remain for inspection; there is no recursive cleanup.

Checks cover install, payload/tamper rejection, portable refusal, upgrade,
uninstall/reinstall, shortcut opt-in, late teardown, helpers, concurrent launches
and maintenance, failure/retry, preserved settings/recovery and startup ownership.
Live DAC preferences and startup state are compared without modification.
Shipping DAC CLI paths and bounded installed/portable hosts run only on the
private desktop, with fresh profiles and automation/hardware off. Hosts exit on
their own timers; fixtures never issue the shared stop command. No valid hardware
operation or visible-desktop UI is started. Inno's uninstall launcher can finish
before its cleanup descendant: the fixture waits for maintenance exclusion to
release before retrying, rather than assuming parent exit means complete teardown.
The existing 27-test DAC suites remain separate and unchanged.

Fixture success is not a physical desktop installation pass. Actual Installed
Apps/tray/Explorer/sign-in behavior, another interactive session, architecture/OS
rejection on incompatible machines, visible setup/accessibility, physical monitor
wake and DDC require separate qualification authority. File exclusion is
session-independent; fixtures do not launch a second actual Windows session.

Signing, publication and updater behavior remain separate decisions.
