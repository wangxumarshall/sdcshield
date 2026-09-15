#!/bin/bash
# Build SLEEF 3.9.0 (vectorized transcendental functions) for SDCShield.
# SLEEF_ENABLE_TLFLOAT=OFF is mandatory: the TLFloat submodule's pinned git
# tag is not fetchable offline (quad-precision support is not needed here).
# CMAKE_INSTALL_LIBDIR=lib: GNUInstallDirs would pick lib64 on this host
# (openEuler usr-merge convention); the meson block and this script's
# idempotency check both expect install/lib/libsleef.a.
# Probe-verified 2026-09-15: ~12s at -j64; libsleef.a exports 108 _u10advsimd
# (NEON) + 108 _u10sve (SVE) function families.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libsleef.a ] && { echo "install/ already present"; exit 0; }
tar xzf sleef-3.9.0.tar.gz
cmake -S sleef-3.9.0 -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DSLEEF_ENABLE_TLFLOAT=OFF \
    -DCMAKE_INSTALL_PREFIX="$PWD/install" \
    -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build -j"$JOBS"
cmake --install build
echo "OK: $(ls -la install/lib/libsleef.a)"
