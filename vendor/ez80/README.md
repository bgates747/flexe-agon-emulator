# eZ80 emulator library

The library comes from [tomm/ez80 at 13c151e8](https://github.com/tomm/ez80/tree/13c151e89f520b3bae3343bea9d67d06e8037cfa). See [LICENSE](LICENSE).

The library source includes an Alexe change in `src/cpu.rs` to accept eZ80
NMI at the active machine's instruction boundary. The change uses the eZ80
ADL/MADL return frame. This export contains the library and its licence.
