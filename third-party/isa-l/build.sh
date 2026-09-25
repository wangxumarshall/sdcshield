#!/bin/bash
# Build isa-l 2.32.1 (Intel Intelligent Storage Acceleration Library) for
# SDCShield SDC stress testing (aarch64). The isal_* tests exercise the
# igzip deflate/inflate round-trip (isal_igzip) and the CRC16/32/64 family
# (isal_crc*): NEON pmull CRC kernels + hand-written aarch64 igzip assembly
# — data-dependent branch chains (match search), high-density stores and
# Huffman entropy coding, a third compression scheduling sample beside
# zstd/zlib (ITHICA: Zlib was the highest-yield fleet SDC detector;
# CORE179 probe H/X: scheduling diversity is detection rate).
# Why Makefile.unx and not ./autogen.sh + configure: the autoconf path adds
# nothing on aarch64 (its nasm feature probes are x86-only) and drags in an
# autoconf dependency; Makefile.unx + make.inc detect aarch64 via uname and
# compile the aarch64 kernels (crc/*_pmull.S, igzip/aarch64/*.S) with plain
# gcc, no nasm (verified by probe build: zero .asm/nasm invocations).
# install/ keeps ONLY the static archive: the make install target also
# produces libisal.so* (order-only prereq of install) and the igzip CLI —
# both deleted afterwards so nothing but headers + libisal.a remain; the
# tests link -l:libisal.a and a stray .so could otherwise win a future
# -lisal lookup.
# Idempotent: skip if install/lib/libisal.a already present.
# Probe-verified 2026-09-16: builds in ~10s at -j64 on Kunpeng 920;
# nm shows crc32_gzip_refl, crc64_*_pmull, isal_deflate_stateless,
# isal_inflate, and the aarch64 multibinary dispatchers.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libisal.a ] && { echo "install/ already present"; exit 0; }
tar xzf isa-l-2.32.1.tar.gz
cd isa-l-2.32.1
# lib: static archive (bin/isa-l.a); slib + isa-l.h: order-only prereqs of
# the install target (install copies the .so and needs isa-l.h generated).
# Build all three, then install into ../install.
make -f Makefile.unx -j"$JOBS" lib slib isa-l.h
make -f Makefile.unx install prefix="$PWD/../install"
cd ..
# keep only what meson links against: static archive + headers
rm -f install/lib/libisal.so install/lib/libisal.so.[0-9]*
rm -rf install/bin install/share
# canonical name: Makefile.unx installs bin/isa-l.a as lib/libisal.a already;
# no symlink dance needed (unlike openblas).
echo "OK: $(ls -la install/lib/libisal.a)"
