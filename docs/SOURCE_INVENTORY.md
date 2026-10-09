# Standalone source inventory and parity plan

This inventory records the historical source selected for the standalone port.
DAC retains the independent history of
[StarlightDaemon/DAC](https://github.com/StarlightDaemon/DAC). Historical source
was selectively adapted; predecessor Git metadata and commits are not imported.

## Source provenance

The historical DAC for Windhawk source is MIT licensed, copyright 2026
StarlightDaemon, from
[StarlightDaemon/dac-windhawk](https://github.com/StarlightDaemon/dac-windhawk)
at commit `e5956a9905073bd437cb4d19d0594012e77d8502`.

The selected R01C revision uses that same baseline with four modified source
files. Their retained SHA-256 identities are:

| File below `windhawk/` | SHA-256 |
| --- | --- |
| `mods/dac-windhawk.wh.cpp` | `4b484d65997953ed6130927254eb0307747dc20be4dfca4af64ad1d566707eee` |
| `tests/platform.cpp` | `13f3611e5b43c3d0bece06106e877de5a00608383b7f154006917512a753ad91` |
| `tests/policy.cpp` | `6a0691637cb1ec25f831fbc24f43ea48061f2fccf3b25a299a0e2f750351f2b5` |
| `tests/nativewake-input-fixture.h` | `40bdb37ed23c2a0e272f2a7963d60556738d91361b91ca67d866456b34dfca2d` |

The clean baseline source has SHA-256
`ca283e60bac5a96b041045eb4410e4eeb983ff055bb57850eb8893944bcdfcfc`.
R01C adds bounded saturating counters, timing histograms, attribution/reset/hide
observations, and tests. It preserves wake attribution and rendering decisions.
Historical test evidence informs the port; it does not qualify this executable.

No inherited OLED Aegis implementation, artwork, or product prose is selected:
the predecessor's provenance register retains unresolved rights for that source.
No Windhawk API header, import library, DLL, engine, compiler, hook, callback, or
runtime is selected. Native DAC monitor/shield geometry is original DAC code.
Fujin is separately pinned, generated, and recorded in the dependency register.

## Feature inventory before porting

All rows below exist in the verified historical source and were selected for port.
Implementation status is recorded in [FEATURE_PARITY](FEATURE_PARITY.md); current validation results belong in [VERIFICATION](VERIFICATION.md).

| Feature group | Selected functionality | Primary verification |
| --- | --- | --- |
| Monitor protection | Stable identities, enablement, independent timers/wake, shared input, spanning, manual/sticky controls, topology recovery | Policy, catalog, session and native-wake fixtures |
| Presentations | Native Black, Moving Clock, Constellation, dim/fade, timed black fallback, concurrent windows | Native rendering and lifecycle fixtures; physical viewing remains separate |
| External content | Local slideshow, installed Windows savers, explicit custom `.scr`, contained preview/configuration | Image, process-containment, hung-child and fallback fixtures |
| Activity | Mouse/keyboard scope, timestamp boundaries, conservative focus/media attribution, foreground/fullscreen, optional XInput | Policy and injected raw-input/media fixtures |
| Automation | Manual/app/scheduled profiles, pause, snooze, battery behavior | Policy transition and UI persistence fixtures |
| Settings | First run, Quick Setup, Advanced Settings, per-monitor options, validated/atomic preferences, import/export/reset/backups | Storage and native UI fixtures |
| Product host | Tray/menu, native controls, singleton, startup-at-login, appearance, CLI, emergency stop, process exit | New standalone lifecycle/settings tests |
| Diagnostics | Redacted bounded events, R01C counters/histograms, recovery and conflict reports | Diagnostic redaction and instrumentation fixtures |
| Hardware | Explicit per-display DDC opt-in, nonce-bound isolated helper, cancellation, fault quarantine | Simulated helper tests only; no physical power operation authorized |

## Deliberate standalone changes

Use independent DAC 0.1.0 version/storage/runtime identities. Replace the host
adapter with a normal executable entry point. Native preferences own appearance,
tray action, and first-run presentation. Login startup is an explicit native UI
choice. Default automation and hardware control remain off. Generate Fujin C++
tokens from the verified dependency. Retain test injection seams and contain
tests within build output instead of operator settings.

The unresolved physical wake incident is retained as a qualification limit.
Hidden-window state observations are not input-to-photon measurements. Hardware,
HDR, accessibility, topology/DPI transitions and long-term desktop operation need
their own observed evidence. A standalone host is not proof of their correctness.
