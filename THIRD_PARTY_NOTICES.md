# Third-party notices and historical origin

DAC is an independent MIT-licensed standalone application. Native controller,
presentation, settings, diagnostic, and regression-test code was selectively
adapted from StarlightDaemon's MIT-licensed DAC for Windhawk. The source and
R01C hashes are recorded in [SOURCE_INVENTORY](docs/SOURCE_INVENTORY.md).
Copyright (c) 2026 StarlightDaemon; the full MIT grant is in LICENSE.

No Windhawk API, engine, import library, hooks, compiler, or runtime is used by
this application. The historical Git repository and releases remain separate.
No inherited OLED Aegis implementation or artwork was incorporated.

Fujin design tokens: copyright (c) 2026 StarlightDaemon, MIT licensed,
tag v0.1.0, commit c653620262ef68fa8d58504b6c47bb21aadc5aa5.
The exact license is included in `licenses/Fujin-LICENSE` in the binary package
and `third_party/fujin/LICENSE` in source. See [FUJIN](docs/FUJIN.md) for the
verified checkout and build-time adapter.

Windows system APIs, installed screensavers, fonts, and image codecs are supplied
by Windows and are not bundled. Build tools are development dependencies only;
their source, integrity, license, and distribution boundaries are documented in
[TOOLCHAIN](docs/TOOLCHAIN.md).
