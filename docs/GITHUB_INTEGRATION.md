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

Checkout uses `actions/checkout` v6.0.2, pinned to
`de0fac2e4500dabe0009e67214ff5f5447ce83dd`, verified against its
[upstream release](https://github.com/actions/checkout/releases/tag/v6.0.2).
It fetches complete history for the publication audit and does not persist
credentials. Workflow permissions are limited to `contents: read`; no production
secrets, privileged PR trigger, environment, shared dependency cache, release or
ref mutation is used. Development artifact upload uses `actions/upload-artifact`
v7.0.2, pinned to `cf430e030ddbb5b0abf93d22962f4752f3646cd9`. Its exact tag
was independently checked through upstream Git refs and the GitHub API against
the [official release](https://github.com/actions/upload-artifact/releases/tag/v7.0.2).
The action uses Node 24 on the hosted runner and its artifact service credentials;
it needs no new repository write permission or secret. This follows GitHub's
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

1. `Bootstrap`: run the CI driver regression checks (including development
   artifact policy and receipt fixtures), then extract and verify
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

After `Audit`, two final workflow steps run only when
`success() && github.event_name == 'push' && github.ref == 'refs/heads/main'`.
The first requires both exact package files and recalculates the ZIP's SHA-256,
requiring the adjacent receipt to match the digest and exact archive filename.
The second uploads only `build/ci-package/DAC-0.1.0-dev-windows-x64.zip` and
`build/ci-package/DAC-0.1.0-dev-windows-x64.zip.sha256`. A missing file, mismatched
receipt or any prior failed/cancelled validation prevents upload. PR events and
feature pushes cannot upload. The upload action also fails if no files are found;
the preceding guard is needed because that action setting alone does not require
every listed file to exist. Hidden files and artifact overwrites are disabled.
The two files are wrapped in a GitHub Actions ZIP with compression level 0 and
14-day retention. The artifact name is
`DAC-0.1.0-dev-windows-x64-<full tested commit SHA>` using `github.sha`.

### Output and local reproduction

Runner workspace, temporary and profile paths are masked before checkout.
The CI driver runs commands with argument arrays, redirects stdout/stderr into
ignored `out/private/ci`, and prints only fixed phase/command labels, exit codes
and aggregate results. It never replays raw compiler/test output or exception
messages. JUnit, analysis XML, detailed audit reports, fixtures, intermediate
builds, caches and toolchain files remain ignored and are not uploaded. Only the
verified portable ZIP and its receipt are eligible for main-push distribution.
GitHub's own job setup and
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

### Download and verify on a separate Windows 11 desktop

This mechanism is intended for development qualification, including a separate
three-monitor desktop. It creates an expiring Actions artifact, not a GitHub
Release, tag, signed installer or public prerelease. DAC remains an unsigned,
unqualified `0.1.0-dev` candidate. Download and checksum verification do not
qualify physical monitor behavior or authorize product execution.

1. Sign in to GitHub with repository read access and open
   [Windows CI](https://github.com/StarlightDaemon/DAC/actions/workflows/windows-ci.yml).
   Filter to branch `main` and event `push`; choose the latest completed successful
   run. Confirm its full source commit SHA and the successful upload step. A PR
   run or an older validation-only run will have no development artifact.
2. In that run's summary, find **Artifacts** and download
   `DAC-0.1.0-dev-windows-x64-<full commit SHA>`. Check the name against the selected
   run's commit. Artifacts expire after 14 days and can be deleted earlier; if
   missing or expired, use a newer successful main push with a confirmed artifact.
   Do not substitute a PR/feature build. GitHub documents the
   [authenticated download procedure](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/download-workflow-artifacts).
3. Save the outer artifact ZIP in a fresh download directory on the qualification
   desktop. Run the following in PowerShell from that directory, replacing the
   source-commit placeholder with the full SHA from the run. These commands only
   extract and inspect files; they do not launch DAC.

```powershell
$ErrorActionPreference = 'Stop'
$sourceCommit = '<full commit SHA from the selected main push>'
if ($sourceCommit -cnotmatch '^[0-9a-f]{40}$') { throw 'Enter the full run commit SHA.' }
$artifactName = 'DAC-0.1.0-dev-windows-x64-' + $sourceCommit
$outerZip = Join-Path $PWD ($artifactName + '.zip')
$download = Join-Path $PWD ('DAC-download-' + $sourceCommit)
Expand-Archive -LiteralPath $outerZip -DestinationPath $download
$portableZip = Join-Path $download 'DAC-0.1.0-dev-windows-x64.zip'
$receipt = $portableZip + '.sha256'
$digest = (Get-FileHash -LiteralPath $portableZip -Algorithm SHA256).Hash.ToLowerInvariant()
$expectedReceipt = $digest + '  DAC-0.1.0-dev-windows-x64.zip' + "`n"
if ([IO.File]::ReadAllText($receipt) -cne $expectedReceipt) {
    throw 'Portable archive checksum mismatch; stop qualification.'
}
'Portable archive checksum verified: ' + $digest
Expand-Archive -LiteralPath $portableZip -DestinationPath (Join-Path $download 'portable')
$root = Join-Path $download 'portable/DAC-0.1.0-dev-windows-x64'
Get-Content -LiteralPath (Join-Path $root 'SHA256SUMS.txt')
$exeLines = @(Get-Content -LiteralPath (Join-Path $root 'SHA256SUMS.txt') |
    Where-Object { $_ -cmatch '^[0-9a-f]{64}  DAC\.exe$' })
if ($exeLines.Count -ne 1) { throw 'Expected exactly one DAC.exe checksum.' }
$expectedExe = $exeLines[0].Substring(0, 64)
$actualExe = (Get-FileHash -LiteralPath (Join-Path $root 'DAC.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualExe -cne $expectedExe) { throw 'Extracted DAC.exe checksum mismatch.' }
'Extracted DAC.exe checksum verified: ' + $actualExe
```

The outer GitHub artifact digest and inner portable ZIP digest describe different
bytes. Use the adjacent `.zip.sha256` for the inner ZIP and `SHA256SUMS.txt` for
its extracted executable. Documentation changes can change portable ZIP bytes;
never compare them to a hardcoded historical publication digest. These checks
detect byte mismatches; provenance still depends on selecting the trusted main
run and matching its commit. The existing CI package verifier additionally checks
the complete 19-file allowlist, PE properties and normalized archive integrity.

Review [QUALIFICATION](QUALIFICATION.md) before any separately authorized visible
desktop test. Per-monitor wake, all-covered recovery, mixed DPI/HDR and session
transitions on all three physical monitors remain qualification work. Physical
DDC, live startup registration and real saver execution need their existing
separate authorization and recovery arrangements.

The distribution workflow must be reviewed and merged under separate authority
before the first main-push artifact can exist. Its first actual download and
post-download integrity verification remain acceptance gates; this documentation
does not claim that an artifact has already been uploaded.

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
