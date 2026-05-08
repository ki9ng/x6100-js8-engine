#!/bin/bash
# Cross-compile the Phase 2 JS8 HB test binary for the X6100 (ARMv7-A).
# Output: ./build/js8-hb-test
#
# Run this from .../x6100-js8-engine/target/.

set -e

BUILDROOT_HOST="${HOME}/AetherX6100Buildroot/build/host"
TOOLCHAIN_FILE="${BUILDROOT_HOST}/share/buildroot/toolchainfile.cmake"

if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo "ERROR: buildroot toolchain not found at ${TOOLCHAIN_FILE}"
    echo "Run an X6100 buildroot build first to populate the host toolchain."
    exit 1
fi

# Add the cross toolchain to PATH so cmake's compiler probes find the
# matching ld/ar/ranlib alongside the gcc/g++ called out in the toolchain
# file. (x6100_gui's build.sh does the same.)
export PATH="${BUILDROOT_HOST}/bin:${PATH}"

mkdir -p build
cd build

cmake .. \
    -G"Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_COLOR_MAKEFILE=OFF

make -j"$(nproc)"

echo
echo "=== build complete ==="
ls -la js8-hb-test
file js8-hb-test
