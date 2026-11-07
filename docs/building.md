# Building from the source snapshot

The release source contains the frontend, eZ80 machine, actual-firmware adapter,
production ESP32 engine source, maintained patches, required unit checks and
notices. A source archive does not need nested Git repositories or submodules.

## Linux prerequisites

Install GNU Make, Bash, Python3.9 or newer, CMake3.14 or newer, a C17 compiler,
binutils, patch, a recent Rust/Cargo toolchain (tested1.97.1), pkg-config, SDL3 development headers
and library, OpenSSL development files and zlib development files. Rust version
requirements can also be imposed by the locked dependencies. A release build
must use a compatible SDL3 shared library at runtime. SDL2 is not a substitute.
The build uses system host libraries; it does not download or install them.

On distributions providing SDL3, the relevant development packages commonly
include `libsdl3-dev`, `libssl-dev`, `zlib1g-dev`, `libudev-dev`, `pkg-config`, `build-essential`,
`cmake` and `python3`. Check your distribution's package names and SDL3 version.

## Build

```bash
make
```

`BUILD_JOBS=2` is the default compiler concurrency. Set `CARGO`, `RUSTC`, `CC`
or `PYTHON` to choose installed tools. The build verifies source checksums,
applies the four integration patches only to `build/flexe-source`, builds the
core/adapter, runs the required C and UART unit checks and builds the frontend.
No firmware or SDL window is executed by those C/UART checks. CMake's host-CPU
native tuning is disabled so it does not silently specialize distributed code
to the build workstation. Local source-root paths are remapped in compiler
output; packaged binary contents still require their own inspection.

Outputs:

| File | Purpose |
|---|---|
| `build/flexe-agon-emulator` | Frontend executable |
| `build/flexe-vdp.so` | Actual ESP32 firmware backend |
| `build/runtime.sha256` | Checksums for these outputs |

Rust dependencies are acquired with `cargo fetch --locked` from the sources
and checksums in `Cargo.lock`. This includes
[ez80](https://github.com/tomm/ez80/tree/13c151e89f520b3bae3343bea9d67d06e8037cfa).
Network access is needed for the first fetch; no prepopulated developer cache
is a prerequisite. To test a fresh cache, set an empty writable `CARGO_HOME`
and use an installed standalone Cargo/Rust compiler. Once dependencies are
available, `CARGO_OFFLINE=1 make` builds without fetching. Keep the lockfile.

Additional machine/debugger checks, without executing a guest image:

```bash
FORCE=1 cargo test --locked -p agon-ez80-emulator -p agon-light-emulator-debugger
python3 -m unittest discover -s tests -p 'test_profile.py'
```

`python3 scripts/verify_source.py` verifies this exact source snapshot.
Intentional source edits require regenerating `SOURCE-MANIFEST.sha256` to match
those edits; its purpose is release integrity, not restricting modifications.
Generated `build/`, `target/`, profiles and dependency caches are not source
release contents. No inherited native-VDP packaging scripts are required.

## Dependency and source provenance

[The component record](third-party-components.json), `Cargo.lock`,
`vendor/flexe/SOURCE.json` and the full notices identify source/dependency terms.
The vendored ESP32 source is an unpatched production subset of the pinned
public source. Its CMake file retains the library definition and omits unused
standalone executables, developer tests and integration runners. See
[MODIFICATIONS.md](../MODIFICATIONS.md). The actual engine patches are applied
when building; vendor source is not modified in place.

Firmware and SD/application content are separate inputs. Obtain them through
[the runtime instructions](running.md), with their own exact identities and
rights. Source build success alone is not firmware, hardware, audio, graphics
or real-time acceptance.
