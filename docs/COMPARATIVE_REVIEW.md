# DAC open source comparison and feature roadmap

Review date: October 15, 2026, America/Denver. Source baseline: `3ef15bdd7947f331dd33c1a7e85fc9802f5a2f3e`. Product: DAC 0.1.0 development candidate, Windows 11 x64 target.

DAC already combines independent monitor protection, activity policies, multiple presentation types and guarded power control. Its best near-term expansion is to make that functionality easier to operate: an all-display flyout, persistent display names and scoped temporary keep-awake controls. Hardware brightness is a useful later expansion, but its recovery and coexistence cost makes it a separate workstream. Qualifying real wake behavior remains more valuable than adding visual variety.

The [ranked catalog](FEATURE_CATALOG.json) contains 27 researched feature additions or extensions and five qualification gates. Roadmap decision on October 16, 2026: F01, the full command interface and local IPC, is deferred by user request and excluded from the current ranking; 26 candidates remain. [Implementation guidelines](IMPLEMENTATION_GUIDELINES.md) give the current gap, value, source anchors, dependencies, rough effort and acceptance criteria for every item. This review builds the research and implementation plan; it does not change DAC runtime behavior.

Implementation update (October 16): F02/F03/F05 are delivered in [FEATURE_IMPLEMENTATION.md](FEATURE_IMPLEMENTATION.md); scoped F16/F04/F11 are delivered in [INSPECTOR_MAP_BACKGROUND.md](INSPECTOR_MAP_BACKGROUND.md). The comparison and original research scores below remain historical. Live DDC probing is deferred; visible/physical qualification remains pending. F01 stays on hold.
## Project scope and current capabilities

The review covers the tracked product source, native resources, build/dependency/package tooling, test architecture, feature and security records, and retained verification evidence. The baseline contains 77 tracked files. Generated build output, the private toolchain, dependency checkouts and packaged copies are not separate implementations. This is a capability and architecture review, not a new line-by-line security audit or fresh test certification.

| Subsystem | Current behavior and source anchors | Consequence for the roadmap |
| --- | --- | --- |
| Host and lifecycle | [standalone.cpp](../src/standalone.cpp): launch/help/version, scoped shutdown, config-directory override, bounded run and supervisor. [platform.hpp](../src/platform.hpp): singleton, tray, login preference and emergency watcher. | Extend existing ownership and commands; do not introduce a service or duplicate controller. |
| Policy and persistence | [policy.hpp](../src/policy.hpp): schema 3, bounded parsing, transactional config, per-monitor preferences, profiles and imported-permission revocation. | New settings need migration and import tests; runtime leases should remain ephemeral. |
| Input and monitor catalog | `Catalog`, `DeliverInput`, `AttributeInput`, generations and event-time boundaries. Independent, shared, pointer, focus, sticky and spanning behavior exist. | Preserve attribution and peer idle history. Names and a map improve discoverability without replacing identity. |
| Activity observation | Core Audio/process-window heuristics, mute handling, display requests, foreground/fullscreen rules and optional XInput. | Background-process and coverage rules are extensions. Basic media/fullscreen/controller awareness is already present. |
| Presentations | Native black, moving clock, constellation, dim/fade, scene rotation, timed black, slideshow placement/shuffle/recursion and contained trusted `.scr` preview/configuration. | Additional scenes or real animated media must justify resource and decoder cost. GIF file acceptance does not establish animated playback. |
| Automation | Manual/app/daily schedule precedence, debounce, pause, fixed-duration global snooze and battery-to-black. | Add scoped leases and richer triggers; do not relabel existing profiles/schedules as missing. |
| Settings and appearance | Quick Setup, Advanced Settings, unsaved preview, identify overlays, profile/rule editor, import/export/reset, system/light/dark/high contrast and Fujin tokens. | Flyout/map/names, localization and runtime accent are specific deltas. Existing UI still needs visible acceptance. |
| Hardware | Explicit per-target DDC D6 off/wake grants, nonce tickets, helper ownership, cancellation and persistent uncertain-state quarantine. | Power control exists but is physically unqualified. Brightness, calibration and contrast are new capabilities. |
| Diagnostics | Per-target reasons/deadlines, bounded redacted events, attribution/hide/latency counters and local export. | Build a capability inspector from existing data; do not rebuild basic observability. |
| Testing | [policy fixtures](../tests/policy.cpp), [platform fixtures](../tests/platform.cpp), [queued wake fixture](../tests/nativewake-negative.cpp), [private desktop runner](../tests/test-runner.cpp) and [host lifecycle](../tests/host-lifecycle.cpp). | Extend current seams and keep fixture results separate from physical behavior. |
| Build and distribution | [CMake](../CMakeLists.txt), pinned [toolchain](TOOLCHAIN.md)/[Fujin](FUJIN.md), [native resources](../resources/dac.rc), static analysis, PE checks, reproducible ZIP and package verifier. | Clean CI, signing and public distribution remain delivery work. Native runtime size/offline operation are product strengths worth preserving. |
| Provenance and recovery | [source inventory](SOURCE_INVENTORY.md), [security](SECURITY.md), [review](SECURITY_REVIEW.md) and [qualification](QUALIFICATION.md). | Preserve explicit permissions, local-only trusted content and documented recovery limits while selecting features. |

The retained [Release log](../evidence/tests-release.log) and [Debug log](../evidence/tests-debug.log) each report 27/27 passing tests. Those are historical local runs documented on October 8, 2026; they were not rerun for this documentation change. The [superseded investigation log](../evidence/investigation/README.md) is not a current failure or an additional pass. Physical all-covered wake, real media, HDR, visible accessibility, docking/session transitions and DDC remain qualification work under [QUALIFICATION.md](QUALIFICATION.md).

## Open source comparables

These projects cover different parts of DAC's purpose. None of the reviewed documentation establishes a complete substitute for DAC's exact combination. Counts are approximate GitHub values displayed during this review; they measure repository visibility, not active users, reliability or feature demand. PowerToys stars cover its whole suite, so Awake's score is capped in the feature popularity calculation.

| Comparable and primary source | Relationship to DAC | Observed features worth evaluating | Displayed stars | License metadata |
| --- | --- | --- | --- | --- |
| [PowerToys Awake](https://learn.microsoft.com/en-us/windows/powertoys/awake) / [repository](https://github.com/microsoft/PowerToys) | Adjacent power/lifetime controller | Timed or absolute-expiry keep-awake, process lifetime, separate screen-on behavior and CLI. | 139.4k suite-wide | MIT |
| [Lively Wallpaper](https://github.com/lively-community/lively) | Presentation and screensaver manager | Animated/local video content, Windows screensaver bridge, command control and broader activity/coverage rules. | 19.8k | GPL-3.0 |
| [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode) | Adjacent policy/appearance automation | Solar and battery triggers, accent switching, localization, package distribution and updates. | 9.8k | GPL-3.0 |
| [Twinkle Tray](https://github.com/xanderfrangos/twinkle-tray) | Adjacent per-monitor hardware controller | Brightness flyout, hotkeys/CLI, normalization, time/idle brightness, contrast and localization. | 9.2k | MIT |
| [LittleBigMouse](https://github.com/mgth/LittleBigMouse) | Adjacent multi-monitor input/layout utility | Spatial display setup, diagnostics, relative-size adjustment and pointer resistance/routing. | 6.7k | GPL-3.0 |
| [Monitorian](https://github.com/emoacht/Monitorian) | Adjacent compact hardware controller | Friendly names, individual/unison brightness, adjustable ranges, probe/rescan, translations and SDR content brightness. | 4.8k | MIT core |
| [ScreenBlind](https://github.com/hemigapp/ScreenBlind) | Selective-cover/privacy counterpart | Password overlay, cursor trapping and additional scene styles. Its security behavior differs materially from Windows locking. | 3 | MIT |

Monitorian explicitly states that its subscription add-on code is absent from the repository. Its paid CLI, hotkey and conditional-command implementation is therefore excluded as open source feature evidence. CLI evidence instead comes from Twinkle Tray, Lively and Awake. See [Monitorian's add-on disclosure](https://github.com/emoacht/Monitorian#add-on-features).

[OLED Aegis](https://github.com/spenserlee/oled_aegis) is a close functional reference for per-monitor blanking and media awareness, most of which DAC already implements. It is outside the scored open source set because DAC's [provenance inventory](SOURCE_INVENTORY.md) retains unresolved rights for that reference. This review does not select its code or artwork for reuse. Commercial products and publicly downloadable freeware without verified open source terms are also outside this set.

Useful source detail: Lively's [coverage and process algorithms](https://github.com/lively-community/lively/wiki/Performance), [command controls](https://github.com/lively-community/lively/wiki/Command-Line-Controls) and [Windows screensaver integration](https://github.com/lively-community/lively/wiki/Screen-Saver); Monitorian's [renaming, ranges, probe and history](https://github.com/emoacht/Monitorian#readme); Awake's [lifetime and command options](https://learn.microsoft.com/en-us/windows/powertoys/awake). Proposed implementation choices below are DAC design judgments, not claims that the donor uses the same architecture.

## Scoring and catalog tags

Every feature receives usefulness, popularity proxy, product fit, effort and risk scores from 1 to 10. Usefulness is the expected benefit to DAC users: 9-10 central/frequent, 7-8 broadly helpful, 4-6 optional or situational, 1-3 narrow or outside scope. Fit measures alignment with independent display protection. Effort and risk increase with complexity and potential disruption. These are planning judgments rather than user-survey results.

Popularity uses a repeatable ecosystem proxy. Donor reach bands are: at most 10 stars = 1; 100 = 2; 1,000 = 3; 5,000 = 5; 10,000 = 7; 25,000 = 8; 100,000 = 9; above that = 10. Awake is capped at 6. Let R be the highest supporting donor reach and N the number of documented supporting projects. Popularity P is `round(0.6R + 0.4 min(10, 2N))`. A DAC-only inference receives 1, meaning no matched evidence rather than demonstrated unpopularity. Adapted ideas inherit the donor-pattern visibility, not evidence of demand for DAC's exact variant. Sources and evidence classes are explicit in the JSON.

Priority is `0.55 usefulness + 0.20 fit + 0.15 popularity + 0.10 (11 - effort)`, rounded to one decimal. High usefulness dominates; dependencies and risk can still defer a highly scored item. Exclude items deferred by user decision, then sort by priority descending, usefulness and ID. Scores remain unchanged; the JSON retains original research ranks. Gates are separate because validating an existing feature is different from adding one.

Status tags mean: **missing** = no implementation for the stated delta; **partial** = related behavior exists but the stated extension is absent; **unsupported-by-design** = a deliberate present boundary requiring a scope decision. Research proposals describe the October 15 baseline; F02/F03/F05 and scoped F16/F04/F11 have since been implemented as recorded below. Tags such as `native`, `automation`, `ddc`, `optional`, `research` and `defer` allow filtering. Source classes distinguish observed features, DAC adaptations, and DAC-only inferences. Every entry includes local symbol anchors, so the absence claim is tied to the current implementation.

## Ranked feature catalog

U = usefulness, P = popularity proxy, E = effort, R = risk. All are 1-10; priority is also on a 1-10 scale. Full fit scores, categories, tags, current gaps and source lists are in the [JSON catalog](FEATURE_CATALOG.json). Detailed plans use the same stable feature IDs.

| Rank | ID | Feature | U | P | Priority | E | R | Wave |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | [F02](IMPLEMENTATION_GUIDELINES.md#f02-compact-per-display-control-flyout) | Compact per-display control flyout | 9 | 6 | 8.5 | 5 | 3 | 1 |
| 2 | [F03](IMPLEMENTATION_GUIDELINES.md#f03-persistent-friendly-display-names) | Persistent friendly display names | 9 | 4 | 8.5 | 2 | 2 | 1 |
| 3 | [F05](IMPLEMENTATION_GUIDELINES.md#f05-per-display-temporary-keep-awake-leases) | Per-display temporary keep-awake leases | 9 | 4 | 8.3 | 4 | 4 | 1 |
| 4 | [F16](IMPLEMENTATION_GUIDELINES.md#f16-display-capability-inspector-and-rescan-report) | Display capability inspector and rescan report | 8 | 6 | 7.9 | 5 | 6 | 2 |
| 5 | [F04](IMPLEMENTATION_GUIDELINES.md#f04-visual-display-map-for-target-selection) | Visual display map for target selection | 8 | 5 | 7.8 | 5 | 3 | 1 |
| 6 | [F11](IMPLEMENTATION_GUIDELINES.md#f11-rules-for-background-running-applications) | Rules for background running applications | 8 | 6 | 7.6 | 6 | 5 | 2 |
| 7 | [F06](IMPLEMENTATION_GUIDELINES.md#f06-process-bound-temporary-keep-awake-leases) | Process-bound temporary keep-awake leases | 8 | 4 | 7.3 | 6 | 5 | 2 |
| 8 | [F08](IMPLEMENTATION_GUIDELINES.md#f08-weekday-schedules-and-date-exceptions) | Weekday schedules and date exceptions | 8 | 1 | 7.3 | 4 | 4 | 1 |
| 9 | [F23](IMPLEMENTATION_GUIDELINES.md#f23-localization-infrastructure-and-translated-ui) | Localization infrastructure and translated UI | 7 | 8 | 7.3 | 7 | 3 | 2 |
| 10 | [F13](IMPLEMENTATION_GUIDELINES.md#f13-actual-monitor-brightness-control) | Actual monitor brightness control | 8 | 7 | 7.2 | 8 | 9 | 3 |
| 11 | [F10](IMPLEMENTATION_GUIDELINES.md#f10-battery-and-ac-profile-triggers) | Battery and AC profile triggers | 7 | 5 | 7.1 | 4 | 4 | 2 |
| 12 | [F24](IMPLEMENTATION_GUIDELINES.md#f24-package-manager-distribution) | Package manager distribution | 7 | 6 | 7.0 | 5 | 5 | 4 |
| 13 | [F12](IMPLEMENTATION_GUIDELINES.md#f12-coverage-aware-application-inhibition) | Coverage-aware application inhibition | 7 | 6 | 6.8 | 7 | 6 | 2 |
| 14 | [F07](IMPLEMENTATION_GUIDELINES.md#f07-keep-windows-awake-while-dac-manages-displays) | Keep Windows awake while DAC manages displays | 7 | 4 | 6.4 | 6 | 6 | 4 |
| 15 | [F15](IMPLEMENTATION_GUIDELINES.md#f15-scheduled-and-idle-hardware-brightness-stages) | Scheduled and idle hardware brightness stages | 6 | 5 | 6.0 | 6 | 8 | 3 |
| 16 | [F22](IMPLEMENTATION_GUIDELINES.md#f22-explicit-windows-workstation-lock-action) | Explicit Windows workstation lock action | 7 | 1 | 5.9 | 4 | 6 | 4 |
| 17 | [F14](IMPLEMENTATION_GUIDELINES.md#f14-brightness-calibration-and-synchronized-groups) | Brightness calibration and synchronized groups | 6 | 6 | 5.9 | 6 | 7 | 3 |
| 18 | [F25](IMPLEMENTATION_GUIDELINES.md#f25-opt-in-update-notifications) | Opt-in update notifications | 6 | 5 | 5.6 | 6 | 6 | 4 |
| 19 | [F09](IMPLEMENTATION_GUIDELINES.md#f09-sunrise-and-sunset-profile-triggers) | Sunrise and sunset profile triggers | 5 | 5 | 5.4 | 6 | 4 | 4 |
| 20 | [F20](IMPLEMENTATION_GUIDELINES.md#f20-additional-configurable-native-scenes) | Additional configurable native scenes | 5 | 1 | 5.1 | 5 | 3 | 4 |
| 21 | [F27](IMPLEMENTATION_GUIDELINES.md#f27-runtime-accent-selection) | Runtime accent selection | 4 | 5 | 5.1 | 4 | 2 | 4 |
| 22 | [F17](IMPLEMENTATION_GUIDELINES.md#f17-hdr-awareness-and-sdr-content-brightness-investigation) | HDR awareness and SDR content brightness investigation | 5 | 4 | 4.9 | 8 | 8 | 4 |
| 23 | [F19](IMPLEMENTATION_GUIDELINES.md#f19-local-video-and-animated-media-presentations) | Local video and animated media presentations | 5 | 6 | 4.9 | 9 | 7 | 4 |
| 24 | [F18](IMPLEMENTATION_GUIDELINES.md#f18-monitor-contrast-controls) | Monitor contrast controls | 4 | 6 | 4.4 | 6 | 8 | 4 |
| 25 | [F21](IMPLEMENTATION_GUIDELINES.md#f21-optional-windows-screensaver-bridge) | Optional Windows screensaver bridge | 4 | 6 | 4.2 | 8 | 6 | 4 |
| 26 | [F26](IMPLEMENTATION_GUIDELINES.md#f26-pointer-border-resistance-and-routing) | Pointer border resistance and routing | 3 | 5 | 2.9 | 10 | 9 | defer |

**On hold by user decision:** F01 Full command interface and local IPC retains its research score of 8.6/10 but has no current rank or implementation wave. Its plan is retained for possible reconsideration. F21 remains dependent on F01 and cannot proceed while that dependency is deferred.

The first three items improve access to capabilities DAC already owns. F16 makes support and hardware eligibility more understandable. F08 scores lower on popularity only because weekday/date exceptions are a DAC-specific recommendation without a matched donor in this set. F13 has high utility but carries substantially more physical and coexistence risk than the first wave. F26 remains deferred even though its donor is well known.

For a popularity-first view, sort the JSON by `popularity`, then `usefulness`: localization leads at 8, followed by brightness at 7; CLI is excluded. This is evidence of ecosystem visibility. For a usefulness-first view, the three current U9 features lead; six U8 features follow. A low proxy score on scoped keep-awake or friendly naming is not a reason to discard a focused, inexpensive improvement.

## Qualification priorities

| Gate | Existing behavior to qualify or delivery gap to close | Usefulness | Effort | Risk | Result needed |
| --- | --- | --- | --- | --- | --- |
| G01 | Physical wake and topology qualification | 10 | 7 | 9 | Only the attributed independent target dismisses; peer generation/idle history stays intact. |
| G02 | Visible settings and accessibility acceptance | 9 | 5 | 5 | Setup and recovery can be completed by keyboard and screen reader. |
| G03 | Monitor and adapter hardware qualification | 9 | 8 | 10 | Publish a qualified/unsupported/unknown matrix per operation and connection. |
| G04 | Clean CI and trusted release preparation | 9 | 6 | 6 | Clean CI reproduces the existing suites and retains exact source/tool identities. |
| G05 | Real activity and endurance qualification | 9 | 7 | 7 | Ambiguous media does not acquire unwarranted exact-tab attribution. |

The physical wake gate is first. G03 applies before expanding hardware operations. G04 makes local verification reproducible in clean CI and prepares trusted artifacts; it does not authorize publication. Source implementation status and physical qualification status remain separate throughout the roadmap.

## Recommended implementation order

Start with physical wake/accessibility qualification and clean CI in parallel with low-risk settings design. For the first feature wave, implement F03 friendly names, F05 scoped leases and F08 schedule extensions as narrow settings/policy slices. Build the flyout F02 and display map F04 around existing in-process actions, state snapshots and target checks. A full CLI or external IPC service is not required for these features. This order limits the chance of multiple control paths evolving different semantics.

The second wave adds read-only capability visibility, process lifetimes, battery profiles, background rules and localization. Treat collective coverage detection as a prototype with measured overhead. The third wave introduces hardware brightness only after exact monitor/adapter qualification, followed by calibration/groups and brightness schedules. The [implementation plan](IMPLEMENTATION_GUIDELINES.md#delivery-sequence) records dependencies and exit criteria.

Choose later work based on observed requests: system-awake integration, solar triggers, HDR status, extra native content, secure OS locking and distribution can serve distinct audiences. Keep local video, a screensaver bridge, networking/update checks and contrast optional because each expands the product's current contract. Prefer documented coexistence with specialist pointer tools over implementing routing inside DAC.

## Assumptions and decisions to revisit

This ranking assumes DAC remains a lightweight native Windows utility with no required account, service, browser runtime, administrator rights or network. F19 proposes local media through an isolated Windows pipeline; it does not adopt Lively's browser architecture. F25 is an explicit optional-networking decision. F21/F22 revisit currently unsupported OS integration. GPL comparables supply ideas only; file-level source/asset reuse remains a separate provenance decision.

Before selecting a major expansion, collect concrete user scenarios and locally volunteered feedback, then update usefulness and priority without adding telemetry by default. Recheck sources and displayed stars when revising the catalog. A changed baseline should revalidate each absence claim and status against [FEATURE_PARITY.md](FEATURE_PARITY.md), rather than treating this snapshot as permanent truth.
