# Proposed remote integration (not executed)

1. Review the publication commits, feature parity, security findings, package
   checksums, and fresh validation in [VERIFICATION](VERIFICATION.md).
2. With explicit authority, push only `feature/standalone-publication` to the
   existing `https://github.com/StarlightDaemon/DAC.git` origin and open a
   development PR.
3. Separately implement and review a validation-only CI workflow as described
   below. Review source provenance and the Fujin consumer-ledger proposal.
   No legacy repository history is merged.
4. With separate review/merge authority, merge the PR into DAC `main`.
5. Qualify the candidate on an operator-approved isolated visible Windows 11 x64
   desktop: per-monitor wake, all-covered recovery, DPI, HDR, media, controller,
   trusted saver visuals, real tray/shell/startup, and session transitions.
6. Authorize any physical DDC testing separately, with a recovery plan for the
   exact monitor/adapter combination. Do not bundle it into routine desktop tests.
7. Decide signing and prerelease naming; publish the first standalone prerelease
   only after explicit tag/release/upload authority.

## Future CI requirements

No CI workflow was added or executed as part of this publication preparation.
A later validation-only workflow should use a clean Windows x64 runner to build
Debug and Release with the pinned inputs, run the isolated test suites, and run
production static analysis. It should check the explicit package manifest,
publication privacy, payload hashes, executable metadata, and package integrity.
Use read-only `contents` permissions and reviewed actions pinned to immutable
commit identities. The workflow must not publish releases, upload distribution
artifacts, create tags, or change remote references.

This sequence does not itself authorize a push, remote ref mutation, PR, merge,
release, live installation, startup registration, or physical hardware operation.
