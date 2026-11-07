# Licensing

The combined Fab-derived emulator is distributed under GNU GPL version3.
Brandon Gates additionally offers his original contributions under MIT to the
extent he owns their copyright. This does not relicense inherited or third-party
material. Preserve full copyright, licence and warranty notices.

## Components

| Component | Applicable notices |
|---|---|
| Fab-derived frontend/combined program | Root [GPLv3](LICENSE); [Fab source](https://github.com/tomm/fab-agon-emulator) |
| Brandon Gates's original additions/portions | [LICENSE-MIT](LICENSE-MIT), [adapter/LICENSE](adapter/LICENSE) and [MODIFICATIONS.md](MODIFICATIONS.md); excludes copied upstream code and patch context |
| eZ80 machine source | [agon-ez80-emulator/LICENSE](agon-ez80-emulator/LICENSE), Tom Morton's BSD3 notice; current package metadata inherits workspace GPL terms |
| Pinned ez80/iz80 dependency | [BSD notices](LICENSES/ez80-BSD-3-Clause.txt), Tom Morton and Iván Izaguirre; [source](https://github.com/tomm/ez80) |
| Flexe ESP32 engine | [vendor/flexe/LICENSE](vendor/flexe/LICENSE), Lev Kropp's MIT notice; [source](https://github.com/levkropp/flexe) |
| Rust dependencies | Exact versions/sources/checksums in Cargo.lock and [component record](docs/third-party-components.json); retain each dependency's actual notices if redistributed |
| Host SDL3/OpenSSL/zlib and system libraries | Supplied by the build/runtime environment; this source-only candidate bundles no host shared libraries |

Our additional MIT grant applies only to separable original material and
original portions of mixed modifications. It does not cover third-party source,
firmware, patch context, logos, fonts or other assets merely because they appear
near our work. Existing GPL permissions are not withdrawn. The combined program
is not MIT-only. Credits do not imply upstream endorsement.

This initial source candidate contains no firmware, ROMs, SD image or application
assets. Applicable platform notices retained under `LICENSES/` acknowledge their
lineage; they do not imply an image or asset is shipped or clear an arbitrary
future firmware package. Users acquiring firmware separately must retain its
own terms and matching source identity.

## Source and binary distribution

The exact production source, patches, interface definitions and build/setup
scripts are included. Locked Rust dependencies are obtained from their public
sources through Cargo; the production Flexe source subset is included directly.
`SOURCE-MANIFEST.sha256` and `release.json` identify this source snapshot.
Upstream repository links preserve provenance without importing development
history into a release snapshot.

No executable download is part of this source-only candidate. A future binary
release must be accompanied by its matching complete corresponding-source
arrangement, dependency/notice inventory and installation information where
applicable under GPLv3. Source-only build results do not certify an unexamined
binary, firmware or third-party asset distribution. See [CREDITS.md](CREDITS.md),
[NOTICE](NOTICE) and the full component notices. Nothing here adds restrictions
to recipients' rights under the applicable licences.
