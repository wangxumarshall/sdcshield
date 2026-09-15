#!/bin/bash
# Build OpenBLAS 0.3.29 for SDCShield SDC stress testing (aarch64).
# TARGET=TSV110: HiSilicon TaiShan v110 microarchitecture (Kunpeng 920,
# implementer 0x48 part 0xd01). USE_THREAD=0: single-threaded on purpose —
# the SDCShield framework provides one worker thread per core; library-internal
# threading would only add nondeterministic reduction order (false positives).
# NOFORTRAN=1: no Fortran compiler on the host; C LAPACK is included, BLAS
# F77-mangled symbols (dgemm_ etc.) are still exported.
# Probe-verified 2026-09-15: ~2min at -j64, cblas_{s,d,z}gemm present.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libopenblas.a ] && { echo "install/ already present"; exit 0; }
tar xzf OpenBLAS-0.3.29.tar.gz
cd OpenBLAS-0.3.29
make -j"$JOBS" TARGET=TSV110 USE_THREAD=0 NO_SHARED=1 NOFORTRAN=1 NO_AFFINITY=1
# install targets: headers (cblas.h + generated openblas_config.h) + static lib
# NO_SHARED=1 must be repeated here: Makefile.install skips the (nonexistent)
# shared-library copy only when it sees the same flag as the build.
make PREFIX="$PWD/../install" NO_SHARED=1 install
cd ..
# canonical name for meson
[ -f install/lib/libopenblas_tsv110-r0.3.29.a ] && \
  ln -sf libopenblas_tsv110-r0.3.29.a install/lib/libopenblas.a
echo "OK: $(ls -la install/lib/libopenblas.a)"
