# Fujin native integration

DAC consumes the generated outputs of the **Fujin `v0.1.0` tagged Git checkout** at commit
`c653620262ef68fa8d58504b6c47bb21aadc5aa5`. The annotated tag object is
`741af7c946161ff15bc06da07ad8cd233554b8b1`. This is the Fujin package release;
it is unrelated to Fujin's RAIDEN Edict version.

The dependency is installed beneath the ignored `.deps/Fujin` directory. It is
not a hand-carried JSON snapshot, workspace alias, or copy of Fujin components.
The executable contains only the generated native values and requires no Git,
PowerShell, Fujin checkout, JavaScript, browser, network, React, or Mantine at runtime.

## Install and build

From the DAC repository, install the pinned dependency:

```powershell
pwsh -NoProfile -File tools/bootstrap-fujin.ps1
```

Bootstrap reads the canonical GitHub repository. Network access is needed only
for that first install. To reuse an existing local Fujin repository without
writing to it or downloading its history again, supply its path with
`-LocalSource`.

The bootstrap verifies the configured source identity and exact annotated tag,
clones with `--no-hardlinks`, and checks out the pinned release with LF bytes.
It sets the new checkout's origin to `https://github.com/StarlightDaemon/Fujin.git`.
An existing dependency checkout is verified and never reset or repaired silently.
No historical DAC repository is required.

`cmake/Fujin.cmake` supplies `dac_add_fujin(target)`. It runs
`tools/generate-theme.ps1` at configure time and before every target build,
including a build whose source timestamps have not changed. The generator checks
the origin, annotated tag, peeled commit, clean working tree, required Git blob
identities and SHA-256 values from `third_party/fujin/lock.json`. It then validates
the input schema and emits `<build>/generated/fujin.hpp`. Identical output retains
its timestamp. Debug and Release build trees have separate generated headers.

The lock hashes refer to original Git blob bytes. If a separately supplied
checkout has CRLF conversion, create the dependency with bootstrap instead of
weakening integrity checks. `FUJIN_SOURCE_DIR` may select another clean checkout
of the same exact release; it is read-only to the generator.

```powershell
cmake -S . -B build/release -DDAC_FUJIN_ACCENT=violet
pwsh -NoProfile -File tools/test-theme.ps1 -BuildDirectory build/release
```

The release provides seven CSS accent presets: violet (default), indigo, blue,
cyan, teal, green, and orange. `DAC_FUJIN_ACCENT` chooses the generated branding
accent at build time. The application follows Windows light/dark preferences and
uses Windows system colors/fonts in high contrast; high contrast overrides the
Fujin palette for accessibility. It does not change global Windows preferences.

## Native mapping and verification

`dist/tokens-resolved.json` supplies dark and light semantic color roles and the
font-family list. The native family selects its first named face (`Verdana`).
`dist/tokens.css` supplies the generated spacing, typography, zero radius,
border widths, and accent overrides omitted from the legacy JSON format.
Colors are strictly checked as six-digit RGB values; native family names and
numeric dimensions are bounded and validated before entering generated C++.

The adapter preserves the established DAC native painter's `fujin::Palette`
interface. `fontSize` derives from `--fujin-font-size-sm`, `spacing` from
`--fujin-spacing-md`, and `radius` from `--fujin-radius-default`. The Win32 painter
scales pixel values to monitor DPI. No palette value is manually transcribed into
the adapter. All generated headers carry Fujin's MIT notice; the portable package
also includes `licenses/Fujin-LICENSE` (from `third_party/fujin/LICENSE` in source).

`tools/test-theme.ps1` validates deterministic generation, all supported accents,
checksum tamper rejection, missing input rejection, malformed JSON, missing
semantic/scalar tokens, invalid colors, nonzero radius, unsafe font-family input,
and incorrect release identity. Malformed cases use isolated copies below the
build directory; the dependency and original Fujin repository remain unchanged.

An intentional dependency upgrade requires reviewing Fujin's new tagged release,
updating the lock identities/hashes and adapter compatibility together, bootstrapping
a clean matching checkout, and rerunning all theme and native UI tests. Do not edit
Fujin's generated outputs or re-pin a tag silently.

## Deferred consumer registration

The Fujin consumer ledger has **not** been modified. The proposed row remains
subject to separate review and authorization:

| Repository | Output consumed | Pinned to | Adopted | Contact / notes |
|---|---|---|---|---|
| DAC | `dist/tokens-resolved.json` and `dist/tokens.css` from tagged Git install | `v0.1.0` (`c653620262ef68fa8d58504b6c47bb21aadc5aa5`) | Pending registration | Build-time native C++ adapter; light/dark, build-selected accent, Windows high-contrast override. |

Registration in `StarlightDaemon/Fujin/CONSUMERS.md` requires separate authority.

## Pinned provenance

Bootstrap installs the exact annotated release tag identified above. The MIT
license and original Git blob identities are preserved in the dependency lock.
A later upstream branch state is not a substitute for the pinned tag. Current
verification results are recorded in [VERIFICATION](VERIFICATION.md).
