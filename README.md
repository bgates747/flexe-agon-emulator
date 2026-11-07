# Flexe Agon Emulator

Flexe Agon Emulator combines the eZ80, MOS, storage and SDL frontend of
[Fab Agon Emulator](https://github.com/tomm/fab-agon-emulator) with
[Flexe](https://github.com/levkropp/flexe), executing actual classic ESP32 VDP
firmware for video, audio and keyboard handling.

This is the **0.1.0 source review candidate**, an independently maintained
derivative. Real firmware execution does not promise hardware timing or
complete peripheral compatibility. Read the [known issues](docs/known-issues.md).

## Build and run

1. On Linux, install the prerequisites in [building](docs/building.md).
2. Run `make` from this source snapshot. It obtains locked public Rust
   dependencies and builds `build/flexe-agon-emulator` and `build/flexe-vdp.so`.
   The pinned production ESP32 engine source is already included; no Git
   submodules or other project checkouts are required.
3. Obtain the MOS and compatible VDP firmware separately and create a stopped
   profile using [running](docs/running.md). Firmware, ROMs, SD contents and
   application assets are not bundled. Run the generated profile-local
   `./flexe-agon-emulator` wrapper after changing to that directory.

The initial binary target is Linux x86_64. ARM64 source builds are possible,
but architecture-specific build and runtime qualification are separate.
Windows/macOS/Flatpak packages are not supplied by this candidate.

## Documentation

- [Build prerequisites and source archives](docs/building.md)
- [Firmware, profiles, controls and CPU budget](docs/running.md)
- [Keyboard path and limits](docs/keyboard.md)
- [Known issues](docs/known-issues.md)
- [Release notes and upstream baseline](docs/releases.md)
- [Build and unit-check evidence](docs/validation.md)
- [Licensing](LICENSING.md), [credits](CREDITS.md) and [modifications](MODIFICATIONS.md)

## Upstream baseline

The frontend baseline is [Fab 1.2.5](https://github.com/tomm/fab-agon-emulator/tree/c22876d1f7ad903537c308cf65afd6553b37cdb8).
Newer [Fab 1.2.6](https://github.com/tomm/fab-agon-emulator/releases/tag/1.2.6)
changes have **not yet been fully integrated**. Some fixes overlap local work;
this is not a claim that every newer fix is absent. The ESP32 engine baseline
is [Flexe fbda8e90](https://github.com/levkropp/flexe/tree/fbda8e905ccff36a090e4e33b0ffc2337dd79c52),
with explicit integration patches. The public source manifest records the exact
snapshot; the release history contains source snapshots rather than inherited
development commits. Upstream names and credits do not imply endorsement.

The combined Fab-derived program is GPLv3-covered. Brandon Gates's original
contributions are additionally available under MIT to the extent he owns them;
this does not relicense upstream work. See [NOTICE](NOTICE), the full licence
texts and [LICENSING.md](LICENSING.md).
