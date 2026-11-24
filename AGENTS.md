# DAC: Display Activity Controls

A lightweight native Windows application (C++23, CMake, MSVC, Windows SDK) that protects idle displays independently. It has its own executable, tray icon, settings and lifecycle, and needs no service, admin rights, account or internet connection. This is a public repository; `main` is the integration line where pull requests merge.

## Build and test

Requirements: PowerShell 7 and Git, plus the pinned portable toolchain (bootstrapped into the ignored `.tools/`) or an existing x64 VS developer shell. Fujin, the pinned design-token source, is bootstrapped into the ignored `.deps/`. Full steps and provenance: `README.md` ("Build and test"), `docs/TOOLCHAIN.md` and `docs/FUJIN.md`.

```powershell
pwsh -NoProfile -File tools/bootstrap-toolchain.ps1
pwsh -NoProfile -File tools/bootstrap-fujin.ps1
. ./tools/use-toolchain.ps1
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel 3
ctest --test-dir build/release --output-on-failure
pwsh -NoProfile -File tools/package.ps1 -BuildDirectory build/release
```

CI (`.github/workflows/windows-ci.yml`, on `main` and `feature/standalone-publication`) runs `tools/ci-windows.ps1` in phases: `Bootstrap`, `Release`, `Debug`, `Analyze`, `Package`, `Installer` and `Audit` (publication privacy). Run a phase locally the same way, for example `./tools/ci-windows.ps1 -Phase Release`. If the workspace provides a qualified local build helper, the workspace notes outside this repository describe it.

## Layout

- `src/`: the application; `tests/`: the test runner, policy and platform tests and fixtures
- `tools/`: bootstrap, build, CI, package, installer and audit scripts; `tools/package-manifest.json` is the explicit list of packaged files
- `installer/`: the standalone installer (`DAC.iss`)
- `resources/`, `cmake/`, `third_party/`: assets, build modules, vendored sources with notices
- `docs/`: architecture, verification, qualification, security and feature records

## Key rules

- Main branch is `main`. Agent work goes on `<agent>/<task>` branches in their own worktrees. See Standing delegation v1 in AGENTS.md.
- **Tests and development never touch the real desktop.** Automated tests use a private non-interactive desktop and simulated power operations. Never launch DAC on the user's input desktop, register login startup, modify an existing installation, run custom or downloaded screensavers, or exercise physical DDC/CI.
- Hardware power control is unqualified on real monitors; don't claim otherwise in docs or code comments.
- The package contains exactly the files in `tools/package-manifest.json`. Adding a packaged file means updating the manifest and the expected counts in the package and installer checks.
- This repository is public and CI audits it for privacy. Never commit absolute local paths, usernames, tool or cache folders, tokens, raw logs or local receipts. `build/`, `out/`, `packages/`, `.tools/` and `.deps/` are ignored and must stay that way.
- No `Co-Authored-By` or other agent attribution lines in commits; the `commit-msg` hook enforces this. Never bypass it.

## Records and hand-over

There is no central open-loops file. Current state lives in `docs/`:
- `VERIFICATION.md`: what has been verified, and how.
- `QUALIFICATION.md`: qualification status and limits.
- `FEATURE_PARITY.md`: what exists compared with the predecessor.
- `FEATURE_IMPLEMENTATION.md` and `INSPECTOR_MAP_BACKGROUND.md`: what is implemented and what acceptance remains.
- `COMPARATIVE_REVIEW.md` and `FEATURE_CATALOG.json`: the ranked roadmap.

Read `IMPLEMENTATION_GUIDELINES.md` before implementing a feature.

Claims and hand-over notes go in `../../HANDOFF.md`: the workspace folder two levels above the repo or worktree root. It's outside Git, so every worktree sees the same file.

## Working in parallel

One agent per worktree. Before starting, read the Claims section of `../../HANDOFF.md` and `git worktree list`, then add your claim line there; mark it done when you finish. Don't edit files another claim covers, and leave other agents' worktrees and uncommitted work alone.

## Standing delegation v1 (operator: StarlightDaemon)
Applies to every agent working in this repo: Claude Code, Codex, Gemini CLI.

Do without asking, then report once at the end of the task:
- Read, build, test, lint. Wait for CI yourself; don't hand back "pending".
- Create agent branches (claude/*, codex/*, gemini/*); commit, rebase,
  fast-forward and merge them. Create agent worktrees.
- Push agent branches (never force). Open and update PRs; enable auto-fix.
- Merge a PR into the default branch when all checks pass, no review
  requests changes, and the merge doesn't publish (see Publication surface).
- Delete agent branches, local and remote, once their commits are in the
  default branch or another pushed branch.
- Keep records current: HANDOFF, OPEN-LOOPS, CHANGELOG and any doc that
  describes branch or PR state.

Ask once, in one message listing everything pending:
- Tags, releases, leaving a -dev version, or any merge that publishes.
- Force push, or rewriting history that's already pushed.
- Any other deletion: worktrees, backup refs, files or branches an agent
  didn't create, commits that exist nowhere else.
- Committing, moving or discarding StarlightDaemon's uncommitted work.
- CI workflows, branch protection, repo settings, secrets, agent permission
  settings, and this section.
- Anything against live hosts, services or devices.

Stop and report: failing checks you can't fix within the task, conflicts in
files StarlightDaemon is editing, or anything this section doesn't cover.

### Publication surface
Merging to `main` publishes nothing. `.github/workflows/windows-ci.yml` builds, tests and packages on `main` but creates no release. Releases go out by tag and GitHub release only.
