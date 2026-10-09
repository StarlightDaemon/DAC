# GitHub integration status and pending acceptance gates

1. The sanitized standalone implementation, feature parity, security findings,
   package checksums and local validation are documented in [VERIFICATION](VERIFICATION.md).
2. The authorized `feature/standalone-publication` branch is published at
   `https://github.com/StarlightDaemon/DAC.git`; [draft PR #1](https://github.com/StarlightDaemon/DAC/pull/1)
   targets `main`. No legacy repository history was merged.
3. Validation-only Windows x64 CI was implemented and passed on both the
   [branch push](https://github.com/StarlightDaemon/DAC/actions/runs/37877951396)
   and [PR synthetic merge](https://github.com/StarlightDaemon/DAC/actions/runs/37877951558).
   Source provenance and Fujin consumer-ledger registration remain separate.
4. With separate review/merge authority, decide whether to merge PR #1 into DAC `main`.
5. Qualify the candidate on an operator-approved isolated visible Windows 11 x64
   desktop: per-monitor wake, all-covered recovery, DPI, HDR, media, controller,
   trusted saver visuals, real tray/shell/startup, and session transitions.
6. Authorize any physical DDC testing separately, with a recovery plan for the
   exact monitor/adapter combination. Do not bundle it into routine desktop tests.
7. Decide signing and prerelease naming; publish the first standalone prerelease
   only after explicit tag/release/upload authority.

The published branch and draft PR were separately authorized. This document does
not authorize a merge, release, live installation, startup registration, physical
hardware operation, or any additional remote mutation.

## Windows CI foundation

`.github/workflows/windows-ci.yml` validates standalone DAC 0.1.0-dev on a
GitHub-hosted `windows-2025` x64 runner. It runs for all pull requests (including
drafts) and pushes to `main` and `feature/standalone-publication`, without path
filters. Pull requests use checkout's default event revision: the synthetic merge
commit, so results cover integration with the base branch. Pushes validate the
pushed revision. Obsolete runs on the same PR/ref are cancelled.

### Inputs and authority

The only action is `actions/checkout` v6.0.2, pinned to
`de0fac2e4500dabe0009e67214ff5f5447ce83dd`, verified against its
[upstream release](https://github.com/actions/checkout/releases/tag/v6.0.2).
It fetches complete history for the publication audit and does not persist
credentials. Workflow permissions are limited to `contents: read`; no production
secrets, privileged PR trigger, environment, shared dependency cache, artifact
upload, distribution, release or ref mutation is used. This follows GitHub's
[secure-use guidance](https://docs.github.com/en/actions/reference/security/secure-use).

The repository bootstrap verifies pinned Microsoft archive SHA-256 values,
official HTTPS origins, SDK MSI signatures and extracted compiler/linker/resource
compiler signatures. CI additionally checks every downloaded payload's locked
size. Compiler, SDK, CMake and Ninja come from the lock, never a moving Visual
Studio channel or the runner's installed compiler. Fujin bootstrap and each
configure/build retain the locked origin, annotated tag, commit, clean-tree,
blob, hash and schema checks. No dependency integrity failure has a fallback.

The documented Microsoft manifest metadata discrepancy remains unresolved.
CI explicitly reports it and uses the reviewed archive lock, as described in
[TOOLCHAIN](TOOLCHAIN.md); it neither resolves the channel nor claims the
manifest digest passed. Updating that lock requires a separate provenance review.
Git, PowerShell, Windows and the hosted image remain platform prerequisites and
can change. Pinned build inputs do not establish identical output across every
future hosted OS/runtime image.

### Validation gates

One sequential job avoids downloading the same toolchain twice. Its total limit
is 45 minutes; individual steps and child commands also have execution bounds.
`tools/ci-windows.ps1` performs these phases:

1. `Bootstrap`: run the 22 CI driver regression checks, then extract and verify
   the pinned toolchain and run its C++/Win32/GDI+
   readiness probe, check archive sizes and verify Fujin.
2. `Release`, then `Debug`: separate Ninja build directories, explicit tests ON,
   existing warnings-as-errors and hardening flags, compilation at parallelism 3.
   Check the exact reviewed 27-test inventory, run sequentially with a 130-second
   test timeout and a five-minute command bound, and require fresh JUnit results
   with all 27 tests run and no failures, errors or skips. Private-desktop
   unavailability fails validation; no test is replaced or bypassed.
3. `Analyze`: the production translation unit and generated Fujin header using
   the Release definitions and [documented MSVC analysis](STATIC_ANALYSIS.md),
   with `/WX` added. Only SDK/MSVC include paths are external; project warnings
   remain enabled. Require successful exit and a fresh empty `DEFECTS` XML report.
4. `Package`: independently build Release without tests and require the shipping
   executable hashes to match. Run PE inspection and all seven packaging
   regression checks, create a package in `build/ci-package`, then independently
   verify the 19-entry allowlist, payload hashes, version, Windows imports,
   hardening flags and byte-identical normalized ZIP reconstruction.
5. `Audit`: scan history after the reviewed initial publication commit
   `48f8d00b1eb2d46a6b0629e339d2716b4a0b76d0`, the final tracked/nonignored
   worktree and all 19 staged package files, including executable ASCII/UTF-16
   strings. The existing publication audit's noreply identity requirements and
   heuristic credential/path checks remain intact. Findings or incomplete scans
   fail the job; future contributor identities may require publication review.

### Output and local reproduction

Runner workspace, temporary and profile paths are masked before checkout.
The CI driver runs commands with argument arrays, redirects stdout/stderr into
ignored `out/private/ci`, and prints only fixed phase/command labels, exit codes
and aggregate results. It never replays raw compiler/test output or exception
messages. JUnit, analysis XML, detailed audit reports, fixtures, packages and
toolchain files remain ignored and are not uploaded. GitHub's own job setup and
checkout logging are platform-controlled; inspect the first hosted run's logs
before accepting the privacy behavior. Modified PR workflow code is untrusted
code and still requires review; log filtering is not a security sandbox.

Failures identify the command and phase. Reproduce locally to inspect ignored
logs rather than uploading private evidence. From PowerShell 7 in the checkout:

```powershell
./tools/ci-windows.ps1 -Phase Bootstrap
./tools/ci-windows.ps1 -Phase Release
./tools/ci-windows.ps1 -Phase Debug
./tools/ci-windows.ps1 -Phase Analyze
./tools/ci-windows.ps1 -Phase Package
./tools/ci-windows.ps1 -Phase Audit
```

The package stage refuses overwrites. For a repeat local packaging run, use a
fresh checkout or deliberately remove only the prior ignored `build/ci-package`
outputs after reviewing their exact path. Bootstrap rechecks cached archives;
no cache is used in the hosted workflow.

### Acceptance and remaining prerequisites

Local results and syntax checks alone are not GitHub execution evidence. Hosted
Windows x64 validation succeeded on both the branch push and PR synthetic merge
([push run](https://github.com/StarlightDaemon/DAC/actions/runs/37877951396),
[PR run](https://github.com/StarlightDaemon/DAC/actions/runs/37877951558)).
The runs exercised pinned inputs and the hosted runner's bootstrap and tests.
Future revisions require their own fresh CI results. Fork PR execution may require
maintainer approval under repository settings. The stable check name is
`Windows x64 validation`; required-check/branch-protection settings remain pending.

The hosted Windows Server image provides automation coverage, not physical
Windows 11 monitor qualification. Signing, releases, deployment, installation,
real startup, visible desktop qualification and physical DDC testing remain
outside this workflow's authority. No claim in the historical local verification
report is upgraded to a hosted CI pass by adding this workflow.

### Local implementation validation

Fresh CI-driver Release and Debug builds passed 27/27 tests each (95 and 101
seconds), with no failures/errors/skips. Production analysis passed with zero
defects, independent Release executable bytes matched, all seven packaging
regression checks and PE/normalized ZIP verification passed, and history/worktree/
package privacy scans reported zero findings. The 22 driver regression checks,
PowerShell parsing, actionlint 1.7.11 and Git whitespace checks passed.

Cached pinned archives were reverified and extracted in a fresh ignored local
fixture; Microsoft signatures, readiness probe, payload sizes and Fujin checks
passed. Re-extraction into the already-used local compiler directory hit a DLL
lock held by a Microsoft compiler telemetry process. No process was interrupted.
The hosted runs successfully bootstrapped before compilation, establishing CI
execution in the tested GitHub runner environment. This does not qualify
physical desktop behavior or eliminate the documented toolchain-manifest caveat.
