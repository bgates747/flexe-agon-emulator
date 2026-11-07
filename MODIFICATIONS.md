# Modifications from upstream

This is an independently maintained Fab/Flexe integration. Original contributions
by Brandon Gates are additionally available under MIT to the extent he owns
them; inherited and third-party material retains its own terms. Relevant
modification dates span October12 through November6,2026.

Frontend base: [Fab1.2.5 c22876d1](https://github.com/tomm/fab-agon-emulator/tree/c22876d1f7ad903537c308cf65afd6553b37cdb8).
ESP32 engine base: [Flexe fbda8e90](https://github.com/levkropp/flexe/tree/fbda8e905ccff36a090e4e33b0ffc2337dd79c52).
Compare those public sources and retained notices with this exact snapshot to
distinguish inherited material from original additions. Our contribution
list does not assert sole authorship of every byte in a mixed file.

| Area | Modifications |
|---|---|
| Adapter | Executed-firmware UART/video/audio bridge, diagnostics, instruction coherence coverage and shutdown |
| ULP/PS2 | Firmware ULP instruction execution, modeled RTC pads/keyboard, typematic/release handling and interrupt scheduling |
| Frontend | Firmware/backend identity, video pacing, clean shutdown, keyboard transitions/focus handling and quiet routine diagnostics |
| eZ80 peripheral model | FIFO/CTS/interrupt semantics and regressions; candidate HALT/IRQ wakeup, physical SRAM address masking and timer input/source routing |
| Core integration patch | ARM64 host instruction cache, guest instruction/IRAM coherence, callback preservation and UART timeout corrections |
| ULP/logging/EOF patches | RTC wake/scheduling integration, opt-in routine logging and preservation of I2S EOF descriptor witnesses |
| Build and distribution | Product name, portable source verification/locked dependency bootstrap, stable build-path remapping, generated stopped profiles and optional CPU budget helper |
| Production engine source packaging | Unchanged pinned library sources/headers and MIT notice; CMake retains the library definition/options and omits unrelated CLI/developer/test/fixture targets. Patches still apply only to generated source. `vendor/flexe/SOURCE.json` records exact source identities. |
| Documentation | Build/run/input instructions, edited release-specific known issues, provenance, credits and licensing scope |

Local corrections overlap some newer Fab work. [Release notes](docs/releases.md)
identify the actual baseline and incomplete upstream integration. The source
candidate's included changes are not a blanket runtime/hardware acceptance claim.
Repackaging, changing product names or editing metadata does not change anyone
else's copyright or licence.
