# DAC real-hardware safety harness (DAC-SAFETY-01)

Status: development tooling, not installation or physical-display qualification.
Real hardware is the intended qualification environment; no VM prerequisite.
The intermittent all-covered wake incident remains unresolved. Removing Windhawk
and passing hidden fixtures do not establish that it is fixed.

**Operator-run only.** The tools here are read-only and are safe for agents to
read, parse and test against fixtures. The staged field procedure below (installing
on the real machine, launching DAC, covering real displays) is for the repository
owner, in person. Agents must not run Stage 1 or later, or run the preflight with
`-Capture` against the owner's machine, without the owner's explicit approval for
that exact action. The repository rule against launching DAC on the input desktop
or exercising physical DDC/CI still applies to automated work.

## Architecture and acceptance boundary

| Boundary | Existing control (source-derived) | Gap / acceptance rule |
| --- | --- | --- |
| Setup -> files/Windows | `installer/DAC.iss`: fixed current-user target, lowest privilege, explicit 28-file payload, no Run section/startup action, optional desktop shortcut off, HKCU uninstall and Start Menu entry | Attended Stage 1 must verify actual integration, token and absence of product launch. Unsigned artifact; never bypass Windows protections. |
| Setup <-> DAC | `installation-lease.hpp`: every installed invocation holds shared sidecar read lease through OS teardown; Setup/uninstall hold exclusive lease and reject unmanaged/reparse targets | Singleton disappearance is insufficient. No maintenance while any host/helper remains. Lease failure is a stop condition, never a reason to remove the lock file. |
| DAC -> presentations | `policy.hpp`: automatic and per-monitor hardware opt-in default false; `platform.hpp`: independent target attribution, native hide and bounded redacted counters | Existing settings/profiles may already grant authority. Use a new approved test profile; never assume defaults apply to existing settings. Physical input routing needs observed tests. |
| DAC -> emergency | `SafetyMain`: separate thread registers Ctrl+Alt+Shift+F12; failure aborts initialization; hotkey sets stop, hides owned windows and ends owned jobs | Registration is not a demonstrated physical key path. Harness simulates safety registration; Stage 2 must demonstrate the real chord before covering displays. |
| DAC -> supervisor | `standalone.cpp`: startup deadline 30 seconds, shutdown deadline 8 seconds, self-termination only, optional `--run-for-seconds 1..86400` | In-process supervisor and OS driver calls are not independent recovery. Runtime bound starts after initialization; allow up to 30 seconds startup + chosen runtime + 8 seconds shutdown, then abort. |
| Stop client -> session | `RequestStop`: scoped user/session event, waits up to 12 seconds for singleton disappearance | A verified CLI path does not identify the event's owner. Do not automatically invoke `--stop`. Require exact ownership and confirm retained process exit separately. |
| Safety tools -> OS | Read-only queries, hashes, explicit allowlisted public summary; optional ignored local evidence | Snapshot, same-user TOCTOU and permissions limitations remain. Name-based inventory cannot prove no renamed/injected controller. No watchdog, service, scheduled task, product launch or recovery actuator is added. |
| Operator -> external recovery | Printed/off-device runbook plus locally rehearsed secure-attention/Task Manager; optionally an independently demonstrated secondary-machine channel | Neither a NIC, installed remote client, same-desktop terminal nor assertion flag proves availability. Manual recovery is mandatory. No guaranteed recovery claim. |

Native lifecycle, emergency, host-death, input-attribution and lease fixtures are
reused. Product sources, package manifest, installer and CI gates remain unchanged.
The safety scripts cannot send a valid DDC command: they contain no hardware API
or product/helper invocation. A static regression checks this deliberately small
surface; this is not a sandbox against someone editing the scripts.

## Tool interfaces and evidence

Use PowerShell 7. Paths below are variables filled by the operator from the local
handoff. Never copy private evidence into a public issue without review.

```powershell
./tools/safety-preflight.ps1 -Installer $setup -Archive $zip
./tools/safety-preflight.ps1 -Installer $setup -Archive $zip -Capture
# Optional assertion, never treated as proof or authorization:
./tools/safety-preflight.ps1 -Installer $setup -Archive $zip -Recovery ManualRehearsed -Capture
# Only after separately approved installation:
./tools/safety-verify-installed.ps1 -Installer $setup -Archive $zip -Baseline $baselinePrivateJson -Capture
# Only observe an already approved, running test process; never starts/stops it:
./tools/safety-observe.ps1 -ProcessId $dacPid -Executable $exe -StartUtc $exactStartUtc -Seconds 10 -Stage 3 -Targets M1 -VisibleOutcome Unknown -Capture
# Repository-local regression, data fixtures and read-only host observations:
./tools/test-safety.ps1 -Installer $setup -Archive $zip
# Bounded observer identity/exit tests on the existing private desktop:
./tools/test-safety-observer.ps1 -Archive $zip -BuildDirectory build/safety-release
```

Preflight defaults to no evidence writes and never launches Setup/DAC. `-Capture`
creates a fresh `out/private/safety/<GUID>/private.json` and `summary.json`, without
overwriting previous runs. The public result reports the GUID only. Private data
includes machine/account/SID/session, PnP topology/adapter associations, driver
versions, process paths/owners, settings file hashes and relevant registry values.
No monitor serial-number query, process command-line, window-title, key-content,
password or network-credential collection is performed. Settings file names may
themselves be sensitive; their contents are not copied. Evidence is ignored, not
encrypted; the operator must choose local access controls and retention. No upload.

Public output uses fixed messages and explicit fields, never raw exception text,
identities, PnP IDs or paths. Read failures produce BLOCKED, invalid artifact or
payload comparisons FAIL, observed conditions PASS, and residual uncertainties
WARNING. Exit codes: preflight 0 = only PASS/WARNING, 2 = blocked, 3 = fail;
post-install 2 = manual gates outstanding, 3 = failed comparison; observer 0 =
identity-matched passive observation, 2 = blocked. Today preflight always has
explicit manual recovery/authorization gates. An overall BLOCKED result must not
be reinterpreted as install-ready merely because artifact verification passes.

The observer is bounded to 1–30 seconds, retains a process handle, requires the
pinned DAC hash plus exact path/PID/UTC start ticks/current SID/session, and samples
working set, handle count and CPU time. Exit is reported only if the retained
process handle observes it. Lack of exit during sampling means unknown, not a hang
or a pass. Observation failure takes no recovery action. Source identity is a
source-derived pinned claim; process samples are automated observations;
`VisibleOutcome`/`RecoveryUsed` are explicitly operator reports. No input is
intercepted. Use DAC's existing **Export redacted diagnostics** for wake/input,
reconciliation and hide counters. Configuration exports are private and are not
redacted diagnostic exports.

### Pinned candidate and receipt trust

Setup: `DAC-Setup-0.1.0-dev-windows-x64.exe`, 3,224,312 bytes, SHA-256
`731ee1dd94cab5fce292e903d88cfcc0cba97812f69bded0cb218245ab402fe1`.
Source (the commit the candidate was built from, clean tree):
`afcabcfc34a851dbbf400a98cc390e1fb3845f22`.
Package SHA-256: `12570df1b5776301a940a938e102d7ee7604ce14a176b597a7939a82ed09bc89`
(28 entries).
Product EXE: `54426f8c9e29e8670bcae02760df48a38d86fae43e5ab2b5dd8dfc222ade3f82`.
Adjacent final-commit `.verification.json` SHA-256:
`d9902746e4ba1f125ef31858d061a36a081ee93b7240cdee0a36eb797c182c6c`.

These pins come from a local run of the CI `Release`, `Analyze`, `Package` and
`Installer` phases on the source commit above. The CI installer phase builds Setup
twice and requires identical bytes, so this Setup is reproducible from that commit.
The existing offline verifier already supplies the embedded 28-file/source/compiler
proof. The workstation tool deliberately checks this fixed candidate and exact
receipt, then derives every payload hash from the independently pinned ZIP. It does
not rebuild Setup, provision a compiler or pretend a mutable adjacent receipt alone
is a trust root. This is integrity checking, not signing, publisher authentication
or a live revocation-service guarantee. Local receipt evidence does not claim
comparison with a downloaded hosted binary.

**Re-pinning.** Any change to the product source, package manifest, installer
script or toolchain produces a different candidate. To qualify a newer build: build
it from a clean tree with the CI phases above, copy the Setup, its `.sha256` and
`.verification.json`, and the ZIP and its `.sha256` to a durable local folder, then
update the `$script:SafetyIdentity` table in `tools/Safety.Common.ps1` (`source`,
`setup`, `bytes`, `receipt`, `archive`, `executable`, `payloadFiles`), this section,
and rerun both safety test scripts. Never edit a pin to make a check pass; a
mismatch means the artifact is not the reviewed candidate.

## Installation-only procedure

1. Confirm the machine, console user/SID/session, unelevated token, physical
   monitor mapping M1/M2/M3 and exact fixed `%LOCALAPPDATA%\Programs\DAC` target.
   Do not assume the agent host is that workstation. Resolve all unavailable
   observations, startup-folder targets, scheduled tasks and portable/Windhawk
   ownership. Name-only absence never clears this operator gate.
2. Re-run preflight with the pinned Setup, ZIP and adjacent `.sha256` and
   `.verification.json`; save local evidence. Review existing settings, profiles,
   recovery tickets/quarantine, target files, startup entries and HKCU uninstall
   registration. Existing installation absence is normal. Preserve any meaningful
   state with a separately approved private backup; do not reset/import/disarm
   production preferences as a shortcut. Reparse, inaccessible or ambiguous paths
   block maintenance. Pin checks are snapshots: rerun immediately before use.
3. Read the dry-run effect list: 28 product files, sidecar, Inno EXE/DAT, Start
   Menu link, HKCU uninstall entry, optional desktop link unchecked. No product
   launch, login startup, automatic protection or hardware authority is expected.
4. Keep this runbook off the covered desktop. Rehearse secure attention and Task
   Manager without DAC. Record the independent recovery channel's actual test,
   time and limitations; no remote software/firewall/account changes are included.
5. Obtain the exact Stage 1 approval described below. Run Setup interactively as
   the confirmed standard user, from the verified local path. Preserve the local
   Setup log. An unexpected elevation request, redirection, protection warning
   needing bypass, running controller, reboot request or changed target aborts
   the test. Do not launch DAC from Explorer, a shortcut, CLI or installer.
6. Run the read-only post-install tool using the fresh preflight private baseline.
   It verifies the complete 28-file payload against ZIP-derived bytes, detects
   extra/missing files, inventories sidecar and uninstaller separately, checks
   current-user Start Menu/uninstall targets and compares settings/Run ownership.
   It refuses to bless unknown extra files; preserve them and investigate.
7. Review generated uninstaller EXE/DAT and empty inert sidecar separately with
   the attended Setup log and existing fixture evidence. Recorded hashes are an
   inventory, not independent authentication of these generated files. Verify
   optional desktop-link state, installed entry/version/quiet-uninstall fields,
   and no unexpected HKLM/startup/service/task changes against observations.
   Snapshot absence of DAC cannot prove it never ran. Record the operator's
   continuous observation, Setup token/log, and any available creation audit.
8. Confirm first-run authority was not granted: unchanged preferences/startup
   baseline, no application invocation, and source-derived defaults for a fresh
   profile. If existing preferences have authority, Stage 2 remains blocked until
   a separate clean qualification profile is approved. Do not change those prefs.

### Uninstall/reinstall recovery (operator-directed only)

Retain logs, pinned original package and private state backup. First exit the
exact owned DAC host/helpers and confirm actual process exit. If the installed
path owns login startup, separately authorize disabling that entry through DAC
and exit it; do not delete foreign portable startup ownership. Use the verified
current-user uninstall entry only after reviewing its exact target. The uninstaller
must acquire the maintenance lease; never remove/bypass it or kill by process name.
Wait for descendant cleanup and lease release, not just the launcher process.
Verify product removal and preserved settings, tickets, quarantine and unknown
files. The empty sidecar/directory can remain intentionally. Reinstall only the
same pinned, reverified Setup after a fresh preflight and new approval. This is
not transactional rollback: crashes, OS failures, changed versions, shared files,
and user changes may leave partial state. Never restore old recovery tickets or
preferences blindly and never erase evidence to force success.

## Emergency procedure: keep an off-device copy

Use the earliest accessible layer; if screens are covered or input unresponsive,
skip an inaccessible tray/terminal immediately. Do not keep retrying a failed
layer while coverage persists. Stop the test at the first unexpected wake/coverage
result; no further stage is allowed until evidence is reviewed.

1. **Tray Exit DAC.** If reachable, select Exit DAC (not just Stop all), then
   confirm the exact process exits and all presentations disappear.
2. **Ctrl+Alt+Shift+F12.** Press the previously demonstrated emergency chord.
   Allow up to 10 seconds for the source-defined 8-second shutdown deadline; if
   visibility/control does not return, use the next accessible recovery layer.
   A hotkey press alone is not proof it was received.
3. **Explicit owned stop.** Only a separately authorized operator may request
   same-user/session stop. Immediately verify the intended DAC executable hash,
   canonical path, retained process identity/start time, owner SID/session and
   absence of competing portable/controller ownership. Confirm the owned primary
   still exists and control-event ownership is not ambiguous. If any check is
   uncertain, do not issue `DAC.exe --stop`; escalate to secure attention.
   Even successful stop-client exit is not proof of full controller exit. Observe
   the retained process handle or Task Manager exact PID/start/path until exited.
   This repository intentionally supplies no automatic stop wrapper: a name/path
   snapshot cannot atomically bind the shared event to a specific process.
4. **Windows secure attention / Task Manager.** Use Ctrl+Alt+Delete. If available,
   open Task Manager and inspect the exact user/session/path/PID/start time of the
   approved test process before an operator-directed End task. Do not terminate
   every process named DAC or Windhawk. This is an operator recovery decision,
   not an automatic action by these tools.
5. **Previously demonstrated secondary-machine recovery.** Use the established
   authenticated channel and the same ownership rules. It must be usable without
   relying on a DAC-coverable local window. RDP can alter topology/session and is
   not itself evidence of physical wake; record this confounder. No channel is
   configured by this task.
6. **Operator-controlled Windows recovery.** If normal routes fail, follow the
   operator's existing physical/restart/recovery procedure after considering
   unsaved work. This requires operator action and is not scripted or guaranteed.

Record which layer was used, visible results and process exit before collecting
diagnostics. Restore control first; never prolong obstruction to collect logs.
Hardware wake cannot be promised by terminating a helper or application.

## Progressive, attended field matrix

Every stage needs its own recorded PASS and explicit authority before progression.
Use a fresh, existing local test-profile directory via `--config-directory` only
after profile creation/launch is separately authorized. Never use production
preferences for convenience. All automatic protection, automatic profile changes,
sticky/shared/span presentations, login startup and DDC remain disabled for Stages
2–5. Use native **Black this monitor** for independent manual tests; no custom
`.scr`, Windows saver configuration or long bright static image. Hardware menu
items must stay unavailable. Verify mapping immediately before each action.

| Stage | Preconditions and allowed actions | Maximum duration | PASS / FAIL, abort and observations |
| --- | --- | --- | --- |
| 1 Installation only | Approved machine/session/artifact/recovery; install and inspect integration, no DAC invocation | 10 minutes attended; unexpected stall stops progression, use recovery plan rather than interrupting a file write blindly | PASS only if 28 files, sidecar/uninstaller, links/registry and preservation checks pass with no elevation/launch/startup. Any mismatch or unexpected prompt fails. Capture before/after hashes, Setup log and operator timeline. |
| 2 First launch/tray | Stage 1 PASS; approved new test profile, automation/hardware off; launch with runtime 60 seconds. Open settings/tray and Exit. Repeat a separate bounded launch to demonstrate emergency hotkey before coverage | Each run 60 seconds + startup/shutdown allowance; total 5 minutes | PASS: tray/settings usable, saved authority off, Exit and chord each fully exit, no coverage or unexpected peers affected. Abort on absent tray, wrong identity, hotkey conflict, auto activation, new authority or stalled exit. Record process identity/start/exit and visual observations. |
| 3 One OLED | Stage 2 PASS and recovery rehearsal; native non-sticky black on one mapped target; test M1, M2, M3 separately. Move/click/wheel on target; use a harmless keyboard action with cursor/foreground mapped in advance | Each cover <=10 seconds; each host <=60 seconds; total 10 minutes | PASS: only intended target dismisses promptly (operator threshold 2 seconds), other screens remain usable, complete exit. Abort immediately on wrong coverage/wake, lost input or identity/topology change. Record target, input category (never key content), foreground scope category, counters and visible timing. |
| 4 Two OLEDs | Stage 3 PASS; manually black two independent targets; test pairs M1+M2, M1+M3, M2+M3, both wake orders; retain third display visible | Each cover <=10 seconds; host <=60 seconds; total 10 minutes | PASS: intended target wakes; covered peer remains until its own attributed input; neither idle history nor generation resets unexpectedly in counters. Keyboard may credit cursor and foreground displays: define both before expecting one wake. Any unexplained wake, non-dismissal after 2 seconds, lost control or peer-history change aborts. Capture before/after redacted counters and operator observations. |
| 5 Three OLEDs | Stages 1–4 PASS; hotkey/secure-attention demonstrated and independent recovery usable off-device; separate all-covered approval | All-covered <=5 seconds, host <=30 seconds; at most three initial attempts, one target-first order each; total 5 minutes | PASS: all three targets cover as requested and independently dismiss in planned order, emergency remains usable, final full exit. Any delayed/ambiguous wake, inability to restore control or unexpected coverage fails immediately; invoke recovery, do not retry for a clean result. Preserve counters/hash/order/input-category and failed visual outcome. This is the historical incident's primary gate. |
| 6 Lifecycle/endurance | Stages 1–5 PASS and separate authority for the exact lifecycle event; real media/input, topology, suspend/resume tested one event per case. Obtain separate permission for any display-setting/topology or machine-power action | Initial cases <=5 minutes, then attended 15 and 30 minute intervals only after review; no unattended run | PASS: visibility, topology identities, media/input behavior, exit and recovery work; no sustained unexplained resource growth across comparable samples. Record driver/build, baseline/deltas for handles/working-set/CPU, real event and visual result. Abort on new mapping, resource drift, missed input or failed wake. Longer intervals need a new plan; short/virtual tests are not endurance proof. |
| 7 Physical power | Not authorized. Separate monitor/adapter-specific DDC capability/off/wake plan, explicit opt-in authority and physical recovery method required | Zero in this task | No physical DDC command, firmware/OLED/display setting change or power acceptance claim. |

Keep panel OLED protections enabled throughout. Software black is not physical
display power-off and is not a guaranteed burn-in prevention mechanism. Do not
extend bright static dwell to photograph a test.

## Machine-specific checklist and first live authorization

Fill this record locally from a fresh preflight; do not publish machine/SID/PnP
identities. Blank or unknown fields block execution.

- Machine label and physical possession confirmed: ____; Windows edition/build:
  ____; native AMD64/workstation: ____; local console SID/session: ____;
  token unelevated: ____; confirmation timestamp/operator: ____.
- Physical OLED M1/M2/M3 -> private adapter/PnP/bounds mapping: ____;
  driver versions: ____; OLED protections unchanged: ____.
- Fixed install path: ____; pinned Setup/ZIP and receipt paths: ____;
  fresh hashes/sizes and preflight evidence GUID: ____.
- Existing installed/portable/Windhawk controller owners and disposition: ____;
  startup Run/folder/task review: ____; settings/profiles/tickets/quarantine
  reviewed and preservation/backup decision: ____.
- Off-device runbook accessible: ____; secure-attention/Task Manager rehearsal:
  ____; separately demonstrated external recovery method/time/limitations: ____;
  operator-controlled last-resort Windows recovery decision: ____.
- Proposed Stage 1 start/end time: ____; attended operator: ____;
  Setup log destination: ____; no product-launch authority: confirmed ____.
- Stage 1 observed elevation/launch behavior: ____; post-install evidence: ____;
  unresolved observations and decision: ____; approval for next stage: ____.

First live approval must explicitly say, in substance: “On [confirmed machine],
as [unelevated console user/session], during [attended window], install only the
Setup with SHA-256 731ee1dd94cab5fce292e903d88cfcc0cba97812f69bded0cb218245ab402fe1
into [fixed current-user target], with [demonstrated recovery/preservation plan].
Collect the local Setup log and read-only post-install evidence. Do not launch
DAC, enable startup/automation/DDC, change production preferences, or restart.”

Resolve every observed blocker before that approval. First launch, physical
presentation, all-covered tests, lifecycle changes, uninstall/reinstall and
physical power each need their own bounded authority. Signing, commit, push, PR
mutation, merge and publication remain outside DAC-SAFETY-01.
