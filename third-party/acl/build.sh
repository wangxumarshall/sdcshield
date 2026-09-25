#!/bin/bash
# Build Arm Compute Library v23.02 as a static arm_compute-core for SDCShield
# (aarch64). Produces: install/lib/libarm_compute-core.a
#                      install/include/{arm_compute,support,half}/...
#
# Why v23.02: the first official pure-CMake release (v22.11 and earlier are
# scons-only; this host is offline for pip so scons is unobtainable). Its
# CMakeLists.txt requires gcc >= 10.2, which all three target OSes satisfy
# (24.03: 12.3, 22.03: 10.3, 20.03: gcc-toolset-10 = 10.3 via
# scripts/offline-build/supplement-20.03-gcc10.sh) — a single vendored version
# covers all three, replacing the old host-binary scheme (v20.02 .so against
# v22.11 headers, which was ABI-broken on 22.03/20.03 and disabled there).
#
# Why only the `arm_compute_core` target: the SDCShield tests use the NEON
# runtime only (NEGEMM / NECast / NEScheduler / Tensor). NEGEMM.cpp, NECast.cpp
# and Tensor.cpp are all compiled into arm_compute_core (src/CMakeLists.txt:879,
# :904, :960). The arm_compute_sve / arm_compute_sve2 / arm_compute_graph
# targets are separate libraries and are NOT built and NOT linked, so no SVE
# machine code enters the binary and non-SVE target machines stay safe.
#
# Why OPENMP=OFF: the SDCShield framework provides one worker thread per core;
# the tests additionally pin the ACL scheduler to 1 thread
# (NEScheduler::get().set_num_threads(1)). Library-internal threading would
# only add nondeterministic scheduling (same philosophy as vendored OpenBLAS
# USE_THREAD=0).
#
# v23.02 has no CMake install rules, so this script hand-assembles the install
# tree: the archive straight from the build dir, plus the header closure
# (arm_compute/ + top-level support/ + include/half for Half.hpp). The tests'
# include closure was probe-verified to compile against exactly this layout
# with a single -I install/include.
#
# Idempotent: skip if install/lib/libarm_compute-core.a already present.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libarm_compute-core.a ] && { ldd --version 2>/dev/null | head -1 | grep -oE "[0-9]+\.[0-9]+$" > install/.glibc-build-tag 2>/dev/null; echo "install/ already present"; exit 0; }
tar xzf ComputeLibrary-23.02.tar.gz
cmake -S ComputeLibrary-23.02 -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DOPENMP=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DCMAKE_CXX_STANDARD=14
cmake --build build --target arm_compute_core -j"$JOBS"
# hand-assembled install tree (v23.02 ships no install rules)
mkdir -p install/lib install/include
cp build/libarm_compute_core.a install/lib/libarm_compute-core.a
cp -r ComputeLibrary-23.02/arm_compute install/include/
cp -r ComputeLibrary-23.02/support   install/include/
mkdir -p install/include/half
cp -r ComputeLibrary-23.02/include/half/* install/include/half/ 2>/dev/null || true
# provenance: glibc tag, same convention as openssl/openblas build tags
ldd --version 2>/dev/null | head -1 | grep -oE "[0-9]+\.[0-9]+$" > install/.glibc-build-tag 2>/dev/null || true
echo "OK: $(ls -la install/lib/libarm_compute-core.a)"
