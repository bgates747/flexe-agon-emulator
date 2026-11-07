# Credits and provenance

Executive summary: Flexe Agon Emulator builds on work by Tom Morton, Lev Kropp,
Dean Belfield, Lennart Benschop and many others. Credits follow the source and
firmware lineage, including earlier libraries, not just this fork's recent
commits. Copyright/licence notices remain authoritative and must accompany
applicable distributions. The historical contributor census is dated 2026-10-13; this source candidate is dated 2026-11-06.

## Core lineage

| Author(s) | Work and evidence |
|---|---|
| Tom Morton and Fab contributors | [Fab Agon Emulator](https://github.com/tomm/fab-agon-emulator); original frontend source at the linked 1.2.5 baseline |
| Tom Morton; Iván Izaguirre | [ez80](https://github.com/tomm/ez80/tree/13c151e89f520b3bae3343bea9d67d06e8037cfa) extends Iván's [iz80](https://github.com/ivanizag/iz80); both copyright names occur in its BSD licence |
| Lev Kropp and Flexe contributors | [Flexe ESP32 engine](https://github.com/levkropp/flexe/tree/fbda8e905ccff36a090e4e33b0ffc2337dd79c52), MIT; pinned in vendor/flexe |
| Dean Belfield (`breakintoprogram`) | Original Quark MOS/VDP firmware; 2022 MIT notices remain in both platform firmware repositories |
| Lennart Benschop | MOS/VDP history and [Agon utilities](https://github.com/lennart-benschop/agon-utilities); the popup-mos SD template points to his utilities by name |
| Steve Sims | VDP audio/refactoring/bug fixes and wider platform contributions, credited in the VDP source header and commit history |
| Jeroen Venema | VDP sprites and VGA mode switching, explicitly credited in the original video source header |
| Damien Guard | Fonts, explicitly credited in the VDP source header; individual font rights/notices still require preservation |
| Igor Chaves Cananea | vdp-gl maintenance, explicitly credited in the VDP source header |
| Fabrizio Di Vittorio | FabGL; credited by the actual MOS CREDITS command and inherited VDP code |
| ChaN | FatFs; credited by MOS CREDITS and its distinct filesystem licence |
| Ralph Hempel | umm_malloc; 2015 MIT copyright and MOS CREDITS |
| Brandon Gates (`bgates747`) | This project's original ESP32 bridge, UART correction, integration patches, tooling and documentation; MIT grant scoped in LICENSING.md |

This platform/firmware lineage does not assert that every person above authored
Fab's frontend or Flexe's engine. Credits do not imply endorsement. The exact
firmware/SD image selected by a release may require additional authors/notices.

## Historical contributor census

The following names are taken from complete author logs through the inspected
revisions, retaining recorded names/aliases rather than guessing identities.
These lists credit repository history; they do not replace each source file's
copyright notice or establish that every historical change is in a given image.

### Fab frontend

Revision: `c22876d1f7ad903537c308cf65afd6553b37cdb8`.

Faye Salwin; Ilian Iliev; J.B. Langston; Jörg Pleumann; Mathieu Cuny; Paul Gregory; S0urceror; Sascha Schneider; Sascha.Schneider; Steve Sims; Tom Morton; bgates747; tim.

### Agon MOS

Revision: `8336409351ee5314e02801a7b72a4f1bb5282519`.

Alexander Sharihin; Andy McCall; Dean Belfield; HeathenUK; Jeroen Venema; Leigh; Leigh Brown; Lennart Benschop; Steve Sims; Tom Morton; breakintoprogram; sadbeast; xianpinder.

### Agon VDP

Revision: `c7ac293d2aa81ddfa693390549bcd909069c8fc3`.

Brendan Fletcher; Curtis Whitley; David Snopek; Dean Belfield; Igor Cananea; Jeroen Venema; Julian Regel; Leigh Brown; Lennart Benschop; Maximilian Gerhardt; Mikołaj Mikołajczyk; Movie Vertigo; S0urceror; Sascha Schneider; Shawn Sijnstra; Steve Sims; Tom Morton; beckadamtheinventor; breakintoprogram; calc84maniac.

`breakintoprogram` is Dean Belfield; `bgates747` is Brandon Gates. The Fab log
records both `Sascha Schneider` and `Sascha.Schneider`; both spellings are
retained above. Other incomplete names are retained as recorded, not expanded
from guesses. Git history and original headers remain the primary evidence.

## Dependency authors

[The locked component census](docs/third-party-components.json) records names,
versions, licence declarations and source/repository pointers for all
64 packages in the inspected Linux Cargo workspace metadata. This
includes the upstream Rust dependency authors, not only our code. Preserve
actual copyright, licence and NOTICE files when assembling a release; Cargo
metadata authors are not an exhaustive legal copyright-holder inventory.

The native adapter additionally depends on OpenSSL, zlib, host C/thread/math
libraries and the ESP32 firmware's own dependencies. SDL is the frontend's
media layer. Credit their upstream authors using the notices for the exact
versions shipped. The actual firmware/SD package may include BBC BASIC,
assemblers, compression tools, fonts and other utilities with further authors.
Do not silently omit their notices because they are outside the Cargo graph.

## Breadcrumbs and preserved notices

- [Original MOS header](https://github.com/AgonPlatform/agon-mos/blob/8336409351ee5314e02801a7b72a4f1bb5282519/src/mos.c) names Dean and implements CREDITS for FabGL, FatFs and umm_malloc.
- [Original VDP header](https://github.com/AgonPlatform/agon-vdp/blob/c7ac293d2aa81ddfa693390549bcd909069c8fc3/video/video.ino) names Dean, Jeroen, Damien, Igor and Steve.
- [Popup MOS](https://github.com/tomm/popup-mos/tree/6b056597d560fbf5802db7c97172d0aaee62da4f) identifies utility/BASIC/font sources, including Lennart's projects.
- [Pinned ez80 README](https://github.com/tomm/ez80/blob/13c151e89f520b3bae3343bea9d67d06e8037cfa/README.md) identifies iz80 ancestry and the ZEXALL/EX8080 test sources. Test corpora have their own notices; do not infer permission for them from the emulator's licence alone.
- `LICENSES/` preserves initial BSD/MIT notices; upstream GPL remains in LICENSE.

Refresh the census against the exact release manifest, including submodule
history, embedded notices and firmware/assets. The exact distribution determines the applicable component notice set.
