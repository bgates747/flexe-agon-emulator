# Firmware and stopped profiles

A profile keeps runtime, MOS/map, actual ESP32 firmware and SD contents together.
The generator only prepares files; it never launches the emulator. Use the
Flexe product entry point for the actual-firmware backend.

## Acquire firmware

1. Download official [MOS v3.0.2 Arthur](https://github.com/AgonPlatform/agon-mos/releases/tag/v3.0.2)
   assets `MOS.bin` and `MOS.map`. Source is
   [AgonPlatform/agon-mos](https://github.com/AgonPlatform/agon-mos/tree/v3.0.2).
   The generator pins these published SHA256 values:

   | Asset | SHA256 |
   |---|---|
   | `MOS.bin` | `d564243283972690933a4554296ad6202ca4ef54572279533a942960846bebae` |
   | `MOS.map` | `d69e60bbce61a7b4b3eef318ba395f11c4e1a5b585755755113992dc94edcb86` |

2. Obtain a compatible classic ESP32 Agon VDP `.bin`, and optionally its matched
   ELF, from the public [Agon VDP project](https://github.com/AgonPlatform/agon-vdp).
   Record the exact source revision, build/flash-image configuration and SHA256.
   Supply the actual image and its public source URL to the profile generator.
   No particular arbitrary VDP build is certified by this source-only candidate.
   Firmware extensions or custom protocols require their matching firmware.
   Do not substitute ESP8266 or another ESP32 variant just because its image
   has a `.bin` extension. No firmware is bundled by this release.
3. Supply your own appropriately licensed SD/application content after profile
   creation. An empty card does not include BASIC, editors or other commands.
   The public [Popup MOS](https://github.com/tomm/popup-mos) project is an optional
   source of Agon utilities; check each included component's terms and identity.
   Never map a containing source tree back into its own nested profile/card.

## Create a profile

From the source/build directory, using your actual input paths and firmware
hash, run:

```bash
python3 scripts/create_profile.py \
  --runtime build/flexe-agon-emulator --backend build/flexe-vdp.so \
  --mos inputs/MOS.bin --mos-map inputs/MOS.map \
  --firmware inputs/VDP.bin --firmware-sha256 YOUR_FIRMWARE_SHA256 \
  --firmware-source https://github.com/AgonPlatform/agon-vdp/tree/YOUR_REVISION \
  --output .emulator
```

Replace the uppercase placeholders before invoking the command. The generator
refuses existing output, wrong MOS/map hashes, mismatched host architecture and
a backend lacking the Flexe identity exports. The binary and library must match
this host, and have been built together. A matched diagnostic ELF can be added
with `--firmware-elf inputs/VDP.elf --elf-sha256 YOUR_ELF_SHA256`; the adapter
uses it for diagnostic symbols, not firmware compatibility replacements.

System library lookup is used by default. If this build needs a separately
installed SDL/host library set, supply `--library-dir` and `--library-sha256`,
a standard SHA256 manifest with flat library filenames. The generator copies
and pins that explicit set under profile `lib/` and puts it first in library
lookup. It does not copy a developer's default library directory automatically.
Dependency closure and redistribution rights remain your responsibility.

## Launch locally

```bash
cd .emulator
./flexe-agon-emulator --precise-interrupts
```

The named entry is a real generated wrapper. `flexe-agon-emulator.bin` is its
relative raw-runtime link; do not invoke it directly. The wrapper verifies
runtime/MOS/map/firmware/marker checksums and forbids identity-changing runtime
arguments. It sets the explicit MOS/card/backend paths and exports actual
firmware location. There is no native-VDP fallback. Regenerate a new profile
for an intentional runtime or firmware change. Runtime paths are computed
from the profile directory; moving the whole stopped profile retains them.

JIT is the default Xtensa engine. `AGON_FLEXE_ENGINE=interp` selects the
interpreter when launching the wrapper; results and speed can differ. Routine
console diagnostics are quiet; `--verbose` enables them. Warnings/errors remain.
Generated `profile.json` identifies inputs; `last-run/` receives optional
backend diagnostics. These local runtime records are not release source files.

The frontend's `--help` describes display, debugger, input and CPU options.
Some inherited firmware/menu options are not applicable to this pinned profile;
the wrapper intentionally prevents changing its selected firmware/backend.
Keyboard behaviour and limits are described in [keyboard](keyboard.md).

## Optional Linux CPU budget

With a systemd user manager, run the quota helper from the profile directory:

```bash
python3 /path/to/source/scripts/run_cpu_quota.py \
  --quota 170 --period-ms 10 -- ./flexe-agon-emulator --precise-interrupts
```

170 permits aggregate CPU time equivalent to1.7 CPUs across all threads and
children; `--quota max` is unlimited. The helper creates a transient scope,
makes no persistent host power changes and does not disable CPU boost. A quota
can slow guest video, audio and loading; it is not a temperature guarantee.
Monitor host cooling/temperature and avoid unattended overheating. Do not
change host boost policy merely to make a loading test pass.
