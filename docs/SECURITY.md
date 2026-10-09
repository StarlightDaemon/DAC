# Windows application threat review

Scope: the standalone candidate and selectively ported native implementation.
This is a source review and local validation record, not penetration testing or
proof of exploit resistance. The desktop qualification gate remains separate.

| Boundary | Controls and findings | Residual risk / qualification |
| --- | --- | --- |
| Privilege and process | As-invoker manifest; no service, injection, hooks, network, telemetry or admin requirement. Exact executable supplied to CreateProcess. | Ordinary user processes share a Windows trust boundary; a compromised account can change executable/configuration. |
| Input privacy | Raw input records attribution/timestamps and bounded aggregate counters; no keystroke text, content, or raw packet log persisted. | Input handling still receives keyboard/mouse activity and must be disclosed. Physical routing, secure desktop, and remote-session cases need qualification. |
| Persistent preferences | Per-user independent directory; bounded UTF-8 input, schema validation, same-directory random CREATE_NEW temporary file, flush and atomic replacement. Save failures retain active state. Import requires review with automation/hardware disabled. | Configuration is not a secrets store. Same-user tampering is outside the isolation guarantee. No migration of live legacy configuration was performed. |
| Installed savers | Fixed Windows system paths; explicit custom local .scr path validation; suspended creation followed by assignment to kill-on-close job; bounded responsiveness checks and native black fallback. | A .scr is executable code with the user's permissions. Job objects are lifecycle controls, **not security sandboxes**. Only run savers the user trusts. Signatures/allowlisting are not a malware guarantee. No arbitrary saver executed by tests. |
| Child containment | Handles identify owned process trees; terminate only owned jobs; stop and host-death regressions. No shell command interpretation. | Custom code can operate outside the intended visual purpose. Test fixtures establish lifecycle behavior only. |
| Photos | Native Windows GDI+; local paths, recursive depth/count/file-size limits, reparse exclusion, image pixel/output bounds, cancellation and black fallback. | Image decoding occurs inside DAC. Decoder bugs and blocking OS file/codec calls remain possible; use trusted local photos and patched Windows. Bounds checked after header decode cannot prove bounded decoder allocation. |
| DLL lookup | Windows system dependencies; review dynamic loads for system-directory-only search; no application plugin loader. | PE imports and search policy require package verification; do not deploy alongside untrusted DLLs. |
| Startup/singleton | Explicit user-controlled per-user run-at-login; quoted executable path; session/user scoped ownership. Tests must use an injected startup store. | Live run-at-login registration remains outside the automated verification scope. Sign-in/Explorer restart and duplicate-instance behavior require separate desktop qualification. |
| Session/restart | WTS lock/unlock and power notifications block/reset presentation ownership. Emergency shortcut and explicit exit/stop paths. | No Windows update/restart, suspend/resume, console/remote handoff, or secure desktop physical qualification performed. DAC does not lock Windows. |
| Hardware | Default off; per-target permission, nonce-bound operation ticket, cancellation/recovery acknowledgement, retained quarantine on ambiguous failure. | DDC/CI may hang or fail to wake hardware. Only simulated protocol tests are authorized; supported monitor/adapter combinations remain unqualified. |
| Diagnostics | Fixed-size counters and timing histograms, bounded event accounting, redacted diagnostic export. | User-selected exported configuration includes paths and monitor identities; diagnostic redaction does not anonymize configuration backups. |
| Distribution | Source/dependency identities, Release build, as-invoker/ASLR/DEP/CFG review, SHA-256 inventory and ZIP verification. | Development candidate is unsigned; hashes detect changes only when compared with a trusted manifest. Authenticode signing/public distribution require a later decision. |

The intermittent all-monitors-covered wake report remains an open field issue.
Neither removing Windhawk nor passing synthetic attribution tests proves it fixed.
The intended independent-wake rule remains: unknown input does not wake unrelated
independent displays; explicit shared/sticky/spanning rules and activation timestamps
are preserved. Emergency recovery remains available.

Automated window/input tests run only on a newly created noninteractive desktop,
never switched to the operator's input desktop. The runner kills only its owned
test job. This is UI isolation, not a security boundary for untrusted code.
