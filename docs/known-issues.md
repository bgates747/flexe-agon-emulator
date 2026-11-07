# Known issues — 0.1.0-alpha source release

This list describes user-visible limitations of the selected source candidate.
It does not claim that every reported stall has the same cause, or that a
successful run establishes a repair. Runtime/firmware identities matter.

| Area | Symptom or limitation | Practical consequence / workaround |
|---|---|---|
| Scanout | Residual vertical jitter and displaced or stale scanlines can appear after longer sessions, including text modes. | The EOF and pacing corrections address specific defects but do not establish that all scanout problems are fixed. No general verified workaround. |
| Asset loading | Large uploads can intermittently stop at different points, sometimes before an application starts. | Successful repeats or added diagnostics do not prove repair. Preserve original firmware/runtime identities and failed traffic when reporting. No general verified workaround. |
| Runtime freezes | A freeze at a gameplay transition has been reported; its failing execution boundary is unisolated. | The report does not establish an emulator, firmware or application cause. No verified general workaround. |
| Audio | Distorted tones, gaps and slower note progression remain possible. Guest production can fall behind host playback. | No causal audio repair or clean-audio guarantee. CPU quotas can worsen missed production. |
| Guest time | MOS/VBlank delivery still depends on native frame changes. Static video intervals and engine differences can affect timeouts or application progress. | Do not interpret MOS ticks as calibrated host time or benchmark processor-speed ratios. No universal scheduling workaround. |
| UART/firmware | UART is an idealized8N1 model. CTS sampling, FIFO/RTS details, scheduling and inherited immediate-EI semantics limit physical timing conclusions. A firmware receive-recovery race has also been reproduced under controlled conditions. | Match the actual VDP firmware. The firmware race does not explain every loading stall, and this release does not bundle a firmware repair. Active UART-timeout divider/clock changes remain unqualified. |
| UART1 IRQ | The current machine does not dispatch UART1 interrupts even though register/FIFO polling exists. | A polling serial client passing does not qualify an interrupt-driven UART1 client. No virtual ESP8266 modem is included. |
| Timers and HALT | Candidate source includes HALT/wakeup, chip-select SRAM and timer-source corrections; combined runtime acceptance is still pending. RTC input is nominal and pin/scheduler timing is modeled. | Do not claim physical timer/synchronizer accuracy or treat source tests as released-runtime evidence. |
| Keyboard | Required ULP operations and PS/2 paths are modeled, with scheduling accommodations. | Unsupported ULP instructions fail closed; broader hardware parity and other firmware/layout combinations remain unqualified. |
| Host load | Sustained emulation can heavily load CPUs and cooling. | Optional Linux CPU quota reduces available CPU time and may slow the guest. It does not guarantee safe temperatures or change CPU boost. |
| Upstream parity | Based on Fab1.2.5; newer1.2.6 changes are not fully integrated, including debugger and printer features. | Do not assume current upstream options or native-VDP functionality exist here. See [release notes](releases.md). |

The classic executable-RAM coherence extension is present in the candidate,
but firmware-specific uploaded code and complete lifecycle tests still need
matching runtime qualification. Source/host checks alone do not establish
application or physical acceptance. Broader ESP32 targets and arbitrary
firmware are outside this initial profile contract.
