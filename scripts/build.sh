#!/usr/bin/env bash
# Build the source snapshot on Linux. No firmware is executed by this script.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
jobs=${BUILD_JOBS:-2}
cargo_bin=${CARGO:-cargo}
rustc_bin=${RUSTC:-rustc}
cc_bin=${CC:-cc}
python_bin=${PYTHON:-python3}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo 'BUILD_JOBS must be positive' >&2; exit 2; }
"$python_bin" scripts/verify_source.py
if [[ ${CARGO_OFFLINE:-0} == 1 ]]; then
    cargo_network=(--offline)
else
    cargo_network=()
    "$cargo_bin" fetch --locked
fi
mkdir -p build
rm -rf -- build/flexe-source
cp -R vendor/flexe build/flexe-source
for name in integration ulp logging i2s-eof; do
    patch --fuzz=0 -p1 -d build/flexe-source < "patches/flexe-$name.patch"
done
# Stable virtual source paths keep developer build roots out of compiled data.
map_args=("-ffile-prefix-map=$root=." "-fdebug-prefix-map=$root=." "-fmacro-prefix-map=$root=.")
printf -v map_flags '%q ' "${map_args[@]}"
cmake -S build/flexe-source -B build/flexe-core -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DFLEXE_LTO=OFF -DNATIVE_ARCH=OFF \
    -DFLEXE_CCACHE=OFF -DCMAKE_C_COMPILER="$cc_bin" -DCMAKE_C_FLAGS="$map_flags"
cmake --build build/flexe-core --target xtensa-emu-lib --parallel "$jobs"
"$cc_bin" -std=c17 -O2 -fPIC -shared -Wall -Wextra "${map_args[@]}" \
    -Ibuild/flexe-source/src adapter/flexe_vdp.c adapter/ulp.c adapter/ps2.c \
    build/flexe-core/libxtensa-emu-lib.a -o build/flexe-vdp.so -lssl -lcrypto -lz -lm -lpthread
"$cc_bin" -std=c17 -O2 -Wall -Wextra "${map_args[@]}" -Iadapter -Ibuild/flexe-source/src \
    tests/ulp_ps2.c adapter/ulp.c adapter/ps2.c build/flexe-core/libxtensa-emu-lib.a \
    -o build/ulp-ps2-tests -lssl -lcrypto -lz -lm -lpthread
build/ulp-ps2-tests
"$cc_bin" -std=c17 -O2 -Wall -Wextra "${map_args[@]}" -Ibuild/flexe-source/src \
    tests/i2s_eof.c build/flexe-core/libxtensa-emu-lib.a -o build/i2s-eof-tests -lssl -lcrypto -lz -lm -lpthread
build/i2s-eof-tests
"$cc_bin" -std=c17 -O2 -Wall -Wextra "${map_args[@]}" -Iadapter tests/clock.c -o build/clock-tests
build/clock-tests
"$rustc_bin" --edition 2021 --test --remap-path-prefix "$root=." \
    agon-ez80-emulator/src/uart.rs -o build/uart-tests
build/uart-tests
printf -v remap_flags '%s\x1f%s' "--remap-path-prefix=$root=." "--remap-path-prefix=${CARGO_HOME:-$HOME/.cargo}=/cargo"
if [[ -n ${CARGO_ENCODED_RUSTFLAGS:-} ]]; then
    export CARGO_ENCODED_RUSTFLAGS="${CARGO_ENCODED_RUSTFLAGS}"$'\x1f'"$remap_flags"
else
    export CARGO_ENCODED_RUSTFLAGS="$remap_flags"
fi
FORCE=1 "$cargo_bin" build "${cargo_network[@]}" --locked --release --jobs "$jobs" -p flexe-agon-emulator
cp "${CARGO_TARGET_DIR:-target}/release/flexe-agon-emulator" build/flexe-agon-emulator
(cd build && sha256sum flexe-agon-emulator flexe-vdp.so > runtime.sha256)
