# Source candidate validation

The production source and build/setup scripts in this candidate passed a
Linux x86_64 source-archive build on November6,2026. This is build and functional
unit-check evidence; it is not an interactive emulator or hardware result.

| Check | Result |
|---|---|
| Source archive extraction without Git metadata | Passed source manifest verification |
| New empty Cargo cache and locked public acquisition | Complete build passed |
| Pinned ESP32 library and actual-firmware adapter | Compiled successfully |
| Frontend release build | Compiled successfully |
| ULP/PS2, I2S EOF and host pacing C regressions | Passed |
| eZ80 UART Rust unit checks |7 passed |
| Machine crate checks, including HALT/IRQ, SRAM and timer input paths |39 passed |
| Debugger crate checks |1 passed |
| Synthetic profile generation/guards/relocation |14 passed, using a shell stub rather than an emulator |
| Selected source filenames/content and local documentation links | Passed the preparation scan |

Toolchain: Rust/Cargo1.97.1, GCC15.2.0, CMake4.2.3, glibc2.43,
SDL3.4.2, OpenSSL3.5.5 and zlib1.3.1. These identify the tested build environment,
not a promise of ABI compatibility with every Linux distribution. The first
candidate provides source, not an executable binary download or host libraries.

The source tree has a positive file manifest and no development history,
private task records or application assets. Automated privacy checks were
combined with explicit source/doc selection; they are not a security guarantee.
Final release metadata and any future executable/source bundles need the same
review. This candidate was not launched with SDL or a guest firmware image.
Interactive review, exact firmware/application compatibility, long-session
scanout, audio quality and physical timing remain separate. See
[known issues](known-issues.md).
