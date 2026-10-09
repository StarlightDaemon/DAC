# Publication verification

DAC 0.1.0-dev is a native Windows 11 x64 development candidate. This report
summarizes newly executed local validation of the sanitized publication source.
Raw logs, XML, screenshots, environment data and detailed receipts remain in
ignored local storage. They are neither tracked nor distributed.

## Source and dependency identity

The publication branch starts directly at DAC's initial commit
`48f8d00b1eb2d46a6b0629e339d2716b4a0b76d0`. Source and documentation were
transferred through an explicit 44-file allowlist. The original development
commits are not publication ancestors. No predecessor Git metadata or history
was imported; original workspaces remain unchanged.

The four application files, six test files and two resources match the reviewed
Git source blobs exactly. Application behavior, tests, default-off automation,
hardware permissions and build hardening were not weakened for publication.
Changes concern documentation privacy, explicit packaging, ignored raw evidence
and reproducible publication audits.

Production source SHA-256 identities analyzed and built in this validation:

| File | SHA-256 |
| --- | --- |
| `src/dac.cpp` | `66744797d7c6f2c4398b45cb03cd42774f9d3916c877b894d0ca7ba832d19833` |
| `src/platform.hpp` | `ab1e0a2050eae692f298a576bfc386952af60a3cd61e8f6d3727e82ac5e2da3d` |
| `src/policy.hpp` | `4900bcd89abaeb4cbaa7af974d40fa5d752fbac525749ddac91602dcf9a637fc` |
| `src/standalone.cpp` | `51603daaa75d2eba2719dcce66937ecd2785a32e8c521a4108136bd585cc8759` |

Fujin is a verified tagged Git dependency: `v0.1.0`, commit
`c653620262ef68fa8d58504b6c47bb21aadc5aa5`, annotated tag object
`741af7c946161ff15bc06da07ad8cd233554b8b1`. Configure and every build verify its
origin, tag, commit, clean tree, Git blobs, file hashes and required token schema.
Seventeen theme generation/integrity cases passed in each full suite. See
[FUJIN](FUJIN.md), [source provenance](SOURCE_INVENTORY.md) and the MIT notices.

## Fresh commands and results

Build inputs: MSVC 19.44.35229, Windows SDK 10.0.26100.0, CMake 3.31.6-msvc6,
Ninja 1.12.1, PowerShell 7.6.5 and Git 2.55.0.windows.2. Dependencies and compiler
scratch files stay local to the checkout; no global installer or service is used.
The Microsoft manifest metadata discrepancy remains documented in
[TOOLCHAIN](TOOLCHAIN.md); it is not represented as a successful digest check.

From the repository root, after the documented dependency bootstrap:

```powershell
. ./tools/use-toolchain.ps1
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 2
ctest --test-dir build/debug --output-on-failure
cmake -S . -B build/repro -G Ninja -DCMAKE_BUILD_TYPE=Release -DDAC_BUILD_TESTS=OFF
cmake --build build/repro --parallel 2
./tools/test-package.ps1 -Executable build/release/DAC.exe
./tools/verify-pe.ps1 -Executable build/release/DAC.exe
./tools/package.ps1 -BuildDirectory build/release
./tools/verify-package.ps1 -Archive packages/DAC-0.1.0-dev-windows-x64.zip
```

| Check | Newly executed result |
| --- | --- |
| Release full regression suite | 27/27 passed; zero failures/skips; 95.17 seconds |
| Debug full regression suite | 27/27 passed; zero failures/skips; 101.60 seconds |
| Policy/state machines | 117,444 assertions per configuration; 48 input/media/mute/poll combinations; eight virtual hours |
| Native wake | 72 timing samples and 816 coverage cases, including queued input and preserved peer states |
| Shipping host | Private-desktop startup, singleton ownership, second launch, scoped stop and five-second idle measurement passed in both full suites |
| Production static analysis | Fresh MSVC `/analyze /analyze:only`, exit 0; zero warnings and zero XML defects; source unchanged during analysis |
| PE inspection | AMD64 GUI, version 0.1.0-dev, ASLR/DEP/high-entropy VA/CFG flags, twelve Windows-only imports |
| Independent Release build | Byte-identical executable from a separate clean build directory |
| Packaging regression | Seven checks passed: allowlist isolation, independent stage determinism, extra file/prior receipt rejection, missing file/tamper rejection and overwrite refusal |

The full suites cover storage corruption/recovery, concurrent presentations,
owned child-tree containment and host death, emergency recovery, native settings,
profiles/rules/schedules, slides, media, raw input, fake startup and simulated DDC.
Actual test execution used private noninteractive desktops and fresh local
profiles. It never switched to the operator's input desktop, registered live
startup, executed an arbitrary screensaver or issued a physical DDC operation.

Builds use warnings as errors and the retained security-hardening flags. The
exact analyzer command and SDK-only exclusions are in
[STATIC_ANALYSIS](STATIC_ANALYSIS.md). The shipping application remains independent
of Windhawk, a browser/JavaScript runtime, privileged service and VC runtime DLLs.

## Bounded performance observations

These fresh Release measurements describe private-desktop/synthetic fixtures,
not input-to-photon latency, real tray behavior or physical monitor qualification.
Short CPU samples depend on Windows accounting and scheduling. CPU percentages
refer to one core; no private host identity or hardware fingerprint is published.

| Observation | Result |
| --- | --- |
| Shipping executable idle | 5,000 ms; 31.250 ms CPU (0.6250%); working set 36,601,856 bytes; private bytes 6,447,104 |
| Shipping resources | Handles 469 to 469; GDI objects 55 to 55 |
| Shipping startup / stop | 172 ms / 63 ms |
| Same-source idle fixture | 10,016 ms; 31.250 ms CPU (0.3120%); handles 228 to 228; GDI 20 to 20 |
| UI message round trip | p50 12 microseconds; p95 22; maximum 94 |
| Synthetic raw input | 2,000 samples per each of 12 combinations; p95 1.5 to 3.3 microseconds; maximum 51.4 |
| Isolated scene rendering | 400 hidden frames; 59.286 ms wall, 46.875 ms CPU; stable handles/GDI |
| Integrated scene rendering | 400 hidden frames; 61.536 ms wall, 46.875 ms CPU; stable handles/GDI |
| Warm lifecycle | 50 cycles after two warmups; 3,859 ms; handles 263 to 265; GDI 10 to 10 |
| Presentation repetition | 100 cycles; 19,562 ms; handles 231 to 231; private bytes 3,440,640 to 3,899,392 |

The cold wake-performance run again retained handles 135 to 202 and GDI objects
0 to 8 after DAC owner structures emptied. Warm checks passed their bounds;
cold resource attribution and long-duration qualification remain open. These
results are not characterized as proof of zero leakage.

## Distribution and privacy

Freshly built executable: `build/release/DAC.exe`, 682,496 bytes, SHA-256
`f4c7259914c613b8bd0ddf3a8eccfde41ba43bd808eea4bcc2730f1ba6243f4a`.
This hash was recalculated from the new build. Unchanged application source
produced the same bytes as the previously reviewed candidate.

The portable archive is `packages/DAC-0.1.0-dev-windows-x64.zip`. Its fresh hash
and sanitized publication inventory are recorded separately in
the repository file `docs/PUBLICATION.json`, deliberately outside the ZIP to
avoid recursively embedding an archive's own checksum. The adjacent
`.zip.sha256` file is another direct archive checksum record.

`tools/package-manifest.json` declares exactly 19 archive files: DAC.exe, four
root product/license documents, eleven reviewed Markdown documents, the Fujin
license, a fixed-field PE report and SHA256SUMS.txt. Packaging and CMake installation
use individually declared files. Raw evidence, prior delivery receipts, arbitrary
documents, caches, build tools, source/test binaries and previous packages cannot
enter through directory recursion. ZIP names use ordinal ordering, fixed timestamps
and explicit 128 KiB write blocks. The verifier enforces the exact declared entry
set, every payload hash, PE identity/version/hardening/imports and deterministic
archive reconstruction, without launching the application.

The publication audit inspects every outgoing commit's message and author identity,
every unique reachable file blob, the final worktree and every extracted package
file, including executable ASCII and UTF-16 strings. Detailed findings remain in
ignored local storage; the public summary contains only relative file names,
counts, immutable identities and hashes. The exact synthetic redaction-test path
is classified as a test fixture, not a real local user identity. Credential and
path scans complement manual review; they cannot prove the absence of all secrets.

## Limits and authority

The [feature matrix](FEATURE_PARITY.md) and [qualification plan](QUALIFICATION.md)
retain unfinished physical wake, visible native Edit painting/accessibility,
Explorer/startup, real savers, mixed DPI/HDR, session/restart and DDC compatibility.
The intermittent all-covered wake report remains unresolved. Custom screensavers
retain user permissions, jobs are lifetime controls rather than security sandboxes,
and GDI+ image decoding remains in process. The executable is unsigned.

No remote ref, PR, workflow run, merge, tag, release, live installation, startup
registration or physical monitor operation was performed in this validation.
Future CI requirements are documented separately in
[GITHUB_INTEGRATION](GITHUB_INTEGRATION.md); no workflow was introduced here.
