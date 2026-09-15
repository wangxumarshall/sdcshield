#!/bin/bash
# Build OpenBLAS 0.3.29 for SDCShield SDC stress testing (aarch64).
# TARGET=TSV110: HiSilicon TaiShan v110 microarchitecture (Kunpeng 920,
# implementer 0x48 part 0xd01). USE_THREAD=0: single-threaded on purpose —
# the SDCShield framework provides one worker thread per core; library-internal
# threading would only add nondeterministic reduction order (false positives).
# NOFORTRAN=1: no Fortran compiler on the host; C LAPACK is included, BLAS
# F77-mangled symbols (dgemm_ etc.) are still exported.
# USE_LOCKING=1: with USE_THREAD=0 the internal packing-buffer pool
# (blas_memory_alloc, table-driven via memorytable[]) is a process-wide
# *unlocked* structure — it is only meant to be safe when the library does
# its own (single) threading. SDCShield's framework instead spawns one
# worker thread per core and all of them call cblas_dgemm concurrently on
# thread-private buffers, so without locks the concurrent
# blas_memory_alloc/free calls hand the same scratch slot to two threads
# and corrupt each other's packing buffers (empirically: 8 threads x 10s
# -> 62 byte mismatches; with USE_LOCKING=1: 128 cores (this machine, all cores)
# x 30s -> 0 (see allcore.yaml).
# The locks guard only the buffer-table metadata, not the kernels, so
# results stay byte-identical to the unlocked build — the single-threaded
# semantics that matter for SDC detection (no library-internal threading,
# no nondeterministic reduction order) are preserved.
# Probe-verified 2026-09-15: ~2min at -j64, cblas_{s,d,z}gemm present,
# strings contain "USE_LOCKING".
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libopenblas.a ] && { echo "install/ already present"; exit 0; }
tar xzf OpenBLAS-0.3.29.tar.gz
cd OpenBLAS-0.3.29
make -j"$JOBS" TARGET=TSV110 USE_THREAD=0 USE_LOCKING=1 NO_SHARED=1 NOFORTRAN=1 NO_AFFINITY=1
# install targets: headers (cblas.h + generated openblas_config.h) + static lib
# NO_SHARED=1 and USE_LOCKING=1 must be repeated here: Makefile.install keys
# off the same flags as the build (it skips the nonexistent shared-library
# copy only when it sees NO_SHARED=1 again, and the installed openblas_config.h
# must report the same USE_LOCKING setting as the compiled library).
make PREFIX="$PWD/../install" NO_SHARED=1 USE_LOCKING=1 install
cd ..
# canonical name for meson
[ -f install/lib/libopenblas_tsv110-r0.3.29.a ] && \
  ln -sf libopenblas_tsv110-r0.3.29.a install/lib/libopenblas.a
echo "OK: $(ls -la install/lib/libopenblas.a)"
