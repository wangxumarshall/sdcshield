#!/bin/bash
# Build OpenSSL 3.5.0 as a static libcrypto for SDCShield (aarch64).
# Produces: install/lib/libcrypto.a + install/include/openssl/
# Rationale: no-apps/no-tests trims the build to just the library we link;
# no-shared gives a self-contained static archive (no runtime .so dependency).
# Legacy provider is kept (no no-legacy flag) for potential ipsec test usage.
# Probe-verified 2026-09-15: configure ~15s + build ~15s at -j64 on Kunpeng 920.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libcrypto.a ] && { ldd --version 2>/dev/null | head -1 | grep -oE "[0-9]+\.[0-9]+$" > install/.glibc-build-tag 2>/dev/null; echo "install/ already present"; exit 0; }
rm -rf openssl-3.5.0
mkdir openssl-3.5.0
tar xzf openssl-3.5.0.tar.gz -C openssl-3.5.0 --strip-components=1
cd openssl-3.5.0
./Configure linux-aarch64 --prefix="$PWD/../install" \
    no-shared no-tests no-docs no-apps --release
make -j"$JOBS" build_sw
if ! make install_sw 2>/dev/null; then
    echo "install_sw failed (likely due to no-apps); falling back to install_dev"
    make install_dev
fi
echo "OK: $(ls -la ../install/lib/libcrypto.a)"
