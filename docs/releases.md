# Release notes

## 0.1.0 source review candidate

This candidate packages the independently maintained Fab/Flexe integration as
production source, with standalone profile creation and explicit dependencies.
No firmware, SD/application assets or host system libraries are bundled.
Candidate build results do not imply hardware timing or runtime acceptance.

The source contains actual-firmware video/audio bridging; held-key PS/2/ULP
input; quiet normal diagnostics; eZ80 FIFO/CTS UART corrections; ARM64 JIT cache
maintenance and guest executable-RAM coherence; EOF descriptor retention;
UART idle-timeout handling and overflow-safe host pacing. It also contains
candidate HALT/IRQ wakeup, physical SRAM address masking and timer source/input
selection changes. Their presence is not a claim that the full combined
candidate has completed interactive runtime review. See [known issues](known-issues.md).

Frontend baseline:
[Fab1.2.5 c22876d1](https://github.com/tomm/fab-agon-emulator/tree/c22876d1f7ad903537c308cf65afd6553b37cdb8).
The assessed newer [Fab1.2.6 9195e0f6](https://github.com/tomm/fab-agon-emulator/tree/9195e0f63053ec53e952dfa5af6c185e13508fb7)
changes have not yet been fully integrated. Overlap with local repairs is
possible; this is not a statement that every newer fix is absent. Missing
UART1 IRQ dispatch, debugger improvements and printer support remain relevant.
The actual-firmware backend differs from Fab's native host VDP, so native
features are not automatically available by copying its loader interface.

ESP32 engine baseline:
[Flexe fbda8e90](https://github.com/levkropp/flexe/tree/fbda8e905ccff36a090e4e33b0ffc2337dd79c52),
with the four recorded patches applied to a generated build export only.
Production vendor source excludes unused upstream developer tools, tests and
CI; its CMake packaging scope is documented in [modifications](../MODIFICATIONS.md).
The eZ80 dependency remains pinned by `Cargo.lock`.

`SOURCE-MANIFEST.sha256` identifies the exact source files. `release.json`
records the candidate version, dependency baselines and distribution contents.
Only reviewed source snapshots and release tags belong in the release history.
