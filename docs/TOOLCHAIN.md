# Portable Windows build toolchain

DAC builds independently of Windhawk. The checked-in bootstrap extracts a pinned
Microsoft C++ compiler, Windows SDK, CMake, and Ninja beneath this checkout's
`.tools` directory. It does not install Visual Studio, register SDKs, change the
machine PATH, or run MSI installation actions. `.tools` is development material,
not part of the DAC distribution.

## Versions

| Component | Version |
| --- | --- |
| MSVC x64 compiler | 19.44.35229 |
| MSVC package series | 14.44 / Visual Studio 2022 17.14 |
| MSVC directory layout | 14.44.35207 (the package's own layout name) |
| Windows SDK | 10.0.26100.0 |
| CMake | 3.31.6-msvc6 |
| Ninja | 1.12.1 |

PowerShell 7 and Windows are bootstrap prerequisites. SDK cabinet extraction uses
Windows' `expand.exe`. SDK file paths are reconstructed from MSI tables opened
read-only through Windows Installer COM; no installer or custom action executes.
VSIX extraction uses .NET ZIP support. No third-party bootstrap program is used.

## Use

From the repository root, with PowerShell 7:

```powershell
./tools/bootstrap-toolchain.ps1
. ./tools/use-toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

For an already downloaded archive cache, use `-Offline`. The bootstrap verifies
every archive again before extracting it. Run it when build processes are closed,
because Windows locks compiler DLLs while a build is active. Dot-source
`use-toolchain.ps1` in each new build shell; environment changes, including a
checkout-local compiler temporary directory, affect only that process.

## Provenance and integrity

`tools/toolchain-lock.json` records every exact HTTPS URL, SHA-256 digest, and
archive size for 57 pinned payloads. All archive URLs belong to
`download.visualstudio.microsoft.com`. Packages were selected from Microsoft's
Visual Studio 2022 release manifest, discovered through
`https://aka.ms/vs/17/release/channel`. Each archive's bytes matched its manifest
SHA-256 during dependency selection. Current verification results are recorded in
[VERIFICATION](VERIFICATION.md). The bootstrap also requires a valid Microsoft
Authenticode signature on
each SDK MSI and on the extracted `cl.exe`, `link.exe`, and `rc.exe` before
declaring the toolchain ready.

The readiness probe compiles and links `<cstdint>`, `<string>`, `<expected>`,
`<format>`, `<filesystem>`, `<windows.h>`, and `<gdiplus.h>` with C++23-capable
MSVC language mode, `/W4 /sdl /guard:cf /MT`, and DAC's desktop Windows libraries.
The bootstrap runs this probe before reporting readiness, so missing shared
headers or CRT libraries fail before the product build begins. The probe creates
no windows and changes no monitor or device state.

The channel's advertised SHA-256 and size for the *manifest itself* did not match
the JSON bytes returned by its official URL, including a retry using
`Accept-Encoding: identity`. Both the advertised and observed digests are retained
in the lock file. The reason is unresolved; the manifest digest check is **not**
reported as passing. Archive digest matches, official HTTPS source, and valid
Microsoft signatures form the recorded input provenance; the manifest mismatch
remains unresolved. Future
bootstrap runs use the reviewed pinned archive lock rather than silently resolving
a moving release channel.

The SDK common headers require the OnecoreUap header packages. MSVC's desktop link
also needs `OLDNAMES.lib` from the CRT Store archive. Their inclusion does not make
DAC a Store/UWP application: DAC uses the desktop x64 library paths and native
Win32 subsystem.

## License and distribution scope

MSVC and Windows SDK remain Microsoft tools governed by their own terms; DAC's
repository license does not relicense them. See Microsoft's
[Build Tools license](https://visualstudio.microsoft.com/license-terms/vs2022-ga-diagnosticbuildtools/)
and [SDK downloads and terms](https://developer.microsoft.com/windows/downloads/windows-sdk/).
The Microsoft-packaged CMake archive retains its bundled CMake and dependency
license files. Tool archives and extracted tool files are excluded from the DAC
application package. The application uses the static MSVC runtime as configured
by the build, so a separate VC runtime installer is not part of its local package.

This custom extraction is a repository-local build convenience, not Microsoft's
Visual Studio installer workflow. A developer with an installed compatible
MSVC/SDK can instead build from its normal x64 developer shell.
