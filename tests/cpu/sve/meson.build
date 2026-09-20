# Copyright 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0
#
# SVE ports of the 53 avx-named tests (tests/cpu/fma, mesh, eigen_svd, ipsec
# families). The originals are NEON 128-bit implementations written on SVE-less
# silicon (see docs/docs_xu/2026-09-19-avx-named-tests-audit.md); these ports
# keep the workload environments byte-for-byte (vector width, random ranges,
# special-value injection, block/stride structure, atomic synchronization,
# golden patterns) and only swap the SIMD engine to SVE1 (armv8.2-a+sve).
#
# Naming rule (user decision 2026-09-19): _avx/_avx2 -> _sve, _avx512 ->
# _sve_wide. The _wide suffix reflects the wider-vector variant of the pair
# (original avx2=256-bit vs avx512=512-bit), NOT the SVE2 instruction set —
# no SVE2 instructions are used (target silicon has SVE1 only, VL=256).
#
# Build discipline mirrors tests/cpu/arm64/meson.build: the whole subdir is
# entered only under the aarch64 guard (x86-64 untouched), and the SVE object
# code lives in its own static library because <arm_sve.h> intrinsics require
# the +sve target — enabling +sve for tests_base could let GCC auto-vectorize
# NEON tests into SVE instructions, which would SIGILL on SVE-less CPUs.
# Every test probes HWCAP_SVE first thing in test_init and returns a clean
# EXIT_SKIP on SVE-less CPUs (sleef_sve.cpp precedent).

if host_machine.cpu_family() == 'aarch64'
    sve_avx53_sources = files(
        # FMA domain (10 ports of tests/cpu/fma/*avx*): same workload
        # environments (VECTOR_SIZE, random ranges, special-value injection
        # patterns, INNER_ITERATIONS, byte-exact memcmp + store/reload
        # consistency) with the SIMD engine swapped to SVE1. VL-agnostic:
        # loads/stores go through svwhilelt predicates in svcntd()/svcntw()
        # batches, so the tests run on any SVE vector length.
        'fma/fma_tail_sve.cpp',
        'fma/fma_tail_sve_wide.cpp',
        'fma/fmatail_sve.cpp',
        'fma/fmatail_sve_wide.cpp',
        'fma/fmatail_nested_sve.cpp',
        'fma/fmatail_nested_sve_wide.cpp',
        'fma/fmatail_double_nested_sve.cpp',
        'fma/fmatail_double_nested_sve_wide.cpp',
        'fma/fma_patterns_sve_wide_ps.cpp',
        'fma/fma_patterns_sve_wide_pd.cpp',
    )

    tests_sve_avx53_lib = static_library(
        'tests_sve_avx53',
        sources: sve_avx53_sources,
        build_by_default: false,
        include_directories: [
            framework_incdir,
        ],
        dependencies: [
            boost_dep,
        ],
        cpp_args: [
            debug_c_flags,
            default_cpp_warn,
            default_cpp_flags,
            march_generic_flags,
            '-march=armv8.2-a+sve',
        ],
    )

    sandstone_tests += [
        tests_sve_avx53_lib,
    ]
endif
