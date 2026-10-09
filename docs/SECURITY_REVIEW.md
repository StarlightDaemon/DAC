# Independent source security review

Development source-review scope: `src/standalone.cpp`, `src/platform.hpp`,
`src/policy.hpp`, `tests/test-runner.cpp`, the test fixture substitutions, manifest,
and build hardening. This review read the implementation independently of the
source-porting work. It did not execute the product, physical display operations,
arbitrary screensavers, startup registration, or live configuration changes.

The review found ordinary local hardening issues and the implementation was
corrected. No unresolved architectural security blocker was identified in the
reviewed paths. This is a source-review conclusion, not physical desktop or
hardware qualification. The current [verification report](VERIFICATION.md) records executed tests.

## Findings and disposition

| Finding | Evidence and correction | Status |
|---|---|---|
| Legacy import could immediately authorize automatic presentations. | `src/policy.hpp`, `Import`, previously mapped `startupEnabled=1` into active automation; `src/platform.hpp`, `ImportLegacy`, saved the result immediately. The shared `DisarmImportedConfig` (line 86) now clears `automatic`, `hardware`, and `powerAfter` in the base policy and every retained profile. Both legacy and full protection-configuration imports invoke it. | Corrected in source; `tests/policy.cpp` asserts base/profile authority is revoked. Execution results are recorded separately. |
| Drive-letter syntax did not establish a local `.scr` or photo source. | `src/platform.hpp`, `LocalPath` (line 121), now checks fixed/removable drive type, rejects alternate streams and reparse components, opens the final target for attributes, rejects a reparse target, and compares the handle's normalized local path with the requested path. `LocalFile`, `SlideFiles`, and `SlideFrame` use this validation. | Corrected in source. Mapped/UNC/reparse paths are deliberately unsupported. |
| A root custom configuration directory could break helper command quoting. | `src/platform.hpp`, `SetConfigDirectory` (line 141), removes trailing directory separators and rejects drive roots. A validated directory consequently cannot end with the backslash that would escape its command-line closing quote. | Corrected in source. |
| Login startup omitted a selected custom configuration directory. | `src/platform.hpp`, `LoginCommand` (line 194), now includes the validated `--config-directory` override. `ExecutePower` (line 568) forwards the same directory to its private helper, whose argument parser revalidates it. | Corrected in source; registry writes remain prohibited during this assignment. |
| Helper teardown contained unbounded process-handle waits. | `src/platform.hpp`, `ExecutePowerProcess` (line 548), now limits termination waits to 5 seconds. The top-level supervisor in `src/standalone.cpp`, `Supervisor::Run` (line 31), independently requests termination of its own process after the startup/shutdown deadline. | Corrected in source. Kernel/driver behavior and physical wake remain unqualified. |
| Saved appearance was loaded after initial theme creation. | `src/platform.hpp`, `UiMain` (lines 2022–2023 in the follow-up review), now establishes the configuration path and calls `ReadIntegrationSettings` before `ReadUiTheme` and icon creation. | Corrected in source; this is a startup correctness issue, not a privilege boundary. Runtime verification remains separately recorded. |

Line references describe the reviewed source snapshot; the named functions are
the stable reference when nearby implementation changes shift those lines.

## Follow-up: full protection-configuration exchange

The subsequent source review covered `src/platform.hpp`, `ExportSettings`
(line 1474), `ImportSettings` (line 1479), and the shared
`src/policy.hpp`, `DisarmImportedConfig` (line 86).

Export serializes the saved protection configuration, including monitor settings,
profiles, rules and schedules. It can contain paths, identities and saved authority
flags; it is a configuration backup, not a redacted diagnostic report. Native
appearance/tray preferences, Windows login registration and recovery tickets are
excluded by design.

Import reads through the existing 256 KiB/UTF-8 bounded file reader and parses into
a new configuration. Malformed or unsupported input leaves current settings intact.
Before applying it, the shared disarming helper removes automatic activation,
hardware permission and power deadlines from both base and profile policies. A
default-No confirmation explains replacement, disarming, retained stable display
identities, backup, and the need to reopen stale settings drafts.

`Save(next, true, true)` then uses the existing byte-preserving backup and checked
atomic commit path. Failed backup or save leaves the original active configuration
intact; successful replacement resets presentation ownership. Disconnected imported
display identities do not become an arbitrary current-monitor mapping. Host
preferences/startup and recovery tickets are not imported or cleared.

No additional source-review blocker was found in these helpers. This follow-up did
not drive the file picker or confirmation dialog, and does not claim direct UI
execution coverage. Shared parser, disarming and save-path tests support the
underlying mechanisms; actual test results belong in `VERIFICATION.md`.

## Verified implementation controls

| Area | Source evidence | Review conclusion |
|---|---|---|
| Least privilege | `resources/dac.manifest`: `requestedExecutionLevel="asInvoker"`, `uiAccess="false"`. `CMakeLists.txt`: static MSVC CRT, `/DYNAMICBASE`, `/HIGHENTROPYVA`, `/NXCOMPAT`, `/guard:cf`, `/sdl`. | No elevation request, privileged service, injection, or system-wide startup registration is introduced. Final PE hardening remains a build-artifact check. |
| DLL loading | `src/standalone.cpp`, `Main`: `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)` before command dispatch. `src/platform.hpp`, `XInputAdapter`: `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)`. | Runtime XInput discovery is restricted to Windows system DLLs. Static imports load before `Main`; package inspection and trusted executable-directory contents remain necessary. |
| Singleton and stop ownership | `src/platform.hpp`, `UserSessionScope` and `Initialize`: named objects include the current token's user SID and process session ID; mutex creation is the authoritative duplicate-owner check. `src/standalone.cpp`, `RequestStop`: signals only that scope's stop event and waits for the singleton to disappear. | No global cross-user singleton, broad process-name termination, or elevated access is used. Same-user tampering is outside the claimed boundary. |
| Startup persistence | `src/platform.hpp`, `LoginStartupState`, `SetLoginStartup`, `LoginCommand`: exact quoted executable/optional profile command, HKCU Run value only, explicit UI action, and refusal to overwrite a different installation's value. `DAC_HARNESS` substitutes an in-memory startup store. | Optional startup is explicit and per user. No live registration was used by this review. |
| Helper authority | `src/standalone.cpp`, `HardwareHelper`: strict argument count; bounded UTF-8 decoded identity without newlines; 32 lowercase hexadecimal nonce; same-session, live, retained parent handle; matching parent executable image path; and matching named cancellation event. `src/platform.hpp`, `PowerChild`: rechecks the nonce-bound ticket phase and saved per-monitor hardware grant before off. | Invalid or unrelated helper requests fail before physical monitor acquisition. The retained parent handle prevents PID reuse from silently changing the validated parent. |
| DDC recovery | `src/platform.hpp`, `PowerCycle`, `PowerWorker`, `MarkerPending`: writes phase markers around changes, requires the same operation's wake acknowledgement, preserves ambiguous failures as quarantine, and treats inaccessible markers as pending. | Return code alone is insufficient to clear recovery state. No review action issued a hardware command. |
| Screensaver containment | `src/platform.hpp`, `RunWorker`: explicit executable path, fixed argument shapes, suspended creation, successful job assignment before resume, kill-on-close jobs, owned-process/window checks, bounded close and fallback. Installed choices come from the Windows system directory. | No shell expansion or uncontained fallback launch is present. A custom `.scr` retains the user's full permissions; the job is not a security sandbox. |
| Configuration writes | `src/platform.hpp`, `ReadFileText`, `WriteFileText`, `ConfigPath`: 256 KiB read cap, UTF-8/NUL rejection, random same-directory `CREATE_NEW` temporary file, flush, replacement, fail-closed default configuration directory checks. `src/policy.hpp`, `Parse`, `Commit`: bounded nested profiles and validated transactional updates. | Failed writes preserve active preferences. Configuration backups may contain monitor identities and user-selected paths; they are not anonymized diagnostics. |
| Photo rendering | `src/platform.hpp`, `SlideFiles`, `SlideFrame`: local-path validation, depth 16, at most 2,000 catalog entries, 32 MiB catalog file-size filter, reparse exclusion, 40-million-pixel source check, and at-most-4096-per-axis output frames. | These bounds constrain the normal path. Header decode precedes the pixel check; GDI+ remains in process. They are not a proof of bounded decoder allocation or a sandbox for malicious images. |
| Input and diagnostics | `src/platform.hpp`, `Input`, `Record`, `DiagnosticText`, `SafeDiagnosticAlias`, `SafeDiagnosticEvent`: attribution and aggregate timing counters, bounded event queue, controlled diagnostic strings, no saved key text. | Default diagnostic export redacts paths/identities and does not constitute input-content logging. Physical routing reliability remains separate from privacy review. |
| Isolated test runner | `tests/test-runner.cpp`, `wmain`: creates a fresh desktop without switching to it; uses an explicit executable path and Windows argument escaping; restricts inherited handles; creates the fixture suspended; assigns the owned kill-on-close job before resume; terminates only that job on timeout/cleanup. | Suitable for reviewed local UI/lifecycle fixtures. It is neither a sandbox for untrusted executables nor an authorization to run the product on the input desktop. |

## Qualification limits

The native fixture power protocol substitutes physical operations. It can establish
ticket/cancellation/containment behavior, but cannot establish monitor power or wake
reliability. Malformed production-helper CLI checks and a product-host smoke test
must use the private desktop and an explicit repository-local configuration
directory; they do not expand that hardware claim.

A successful source review or synthetic wake test does not close the intermittent
all-monitors-covered wake report. Real mouse/keyboard routing, fullscreen/media
heuristics, DPI/accessibility, sign-in, Explorer recovery, suspend/resume, session
handoff, Windows restart, and supported DDC hardware need separately authorized
desktop or hardware qualification.

Path validation is a local-input check, not an isolation boundary against another
process already operating with the same user's filesystem permissions. No claim is
made that it protects against every same-user time-of-check/time-of-use race.

The candidate remains unsigned. Archive hashes require a trusted manifest to be
meaningful; signing and public distribution require separate review and authority.
