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
        # Mesh domain framework A (12 ports of tests/cpu/mesh/*avx*): the
        # multi-core cooperative write/barrier/read-verify/round-flip
        # protocols and block structures (8/16-element blocks, atomic
        # synchronization, sum + store/reload consistency checks) are
        # preserved from the NEON originals; only the block data movement
        # goes through SVE predicated ld/st. Note: the asymm variants fix a
        # stack-overread UB in the originals (reader loop ran to
        # TOTAL_ELEMENTS over an 8-element stack buffer); the SVE ports sum
        # exactly the BLOCK_ELEMENTS that are written, which matches the
        # described intent.
        'mesh/mesh_upi_sve_sym.cpp',
        'mesh/mesh_upi_sve_symm_int.cpp',
        'mesh/mesh_upi_sve_asymm_int.cpp',
        'mesh/mesh_upi_sve_asymm_write_int.cpp',
        'mesh/mesh_upi_sve_asymm_distrib_int.cpp',
        'mesh/mesh_upi_sve_asymm_distrib_write_int.cpp',
        'mesh/mesh_upi_sve_wide_sym.cpp',
        'mesh/mesh_upi_sve_wide_symm_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_write_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_distrib_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_distrib_write_int.cpp',
        # Mesh domain framework B (6 ports): avx2 trio = per-thread 4MB
        # sweep (read_only/read_L3 golden-sum scan, write_only
        # write-and-readback); avx512 trio = 16-element-block variants
        # (read_only/read_L3 init-golden scans, write_only cooperative
        # writer/reader protocol matching its original).
        'mesh/mesh_upi_sve_read_only_int.cpp',
        'mesh/mesh_upi_sve_read_L3_int.cpp',
        'mesh/mesh_upi_sve_write_only_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_read_only_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_read_L3_int.cpp',
        'mesh/mesh_upi_sve_wide_asymm_write_only_int.cpp',
        # Eigen domain (1): SVD-family workload without the Eigen library.
        # The SVE-backend sibling eigen_svd_cdouble_sve currently crashes
        # (vendored Eigen5 SVE double packet SIGSEGV on the BDCSVD
        # workload), so this port is a self-written deterministic
        # Householder bidiagonalization — the same FMA-dense SVD first
        # phase — with SVE-vectorized inner loops and the EigenSVDTest
        # golden pattern (init computes once, run recomputes and
        # byte-compares). n=128 calibrated at ~0.92ms/iter.
        'eigen/eigen_svd_bidiag_sve.cpp',
    )

    # IPSec domain (24 ports). Two tiers:
    # - HMAC/SHA family (17): encryption stays on OpenSSL EVP, the MAC runs
    #   on the multi-stream SVE HMAC layer (tests/cpu/sve/ipsec/sve_hmac.h
    #   over sve_sha_kernels.h — both verified bit-for-bit against OpenSSL,
    #   5200+1360 vectors, see scripts/sve-sha-verify). NSTREAM lanes: lane 0
    #   carries the original ciphertext message (MAC semantics identical to
    #   the avx-named original), lanes 1..N-1 carry deterministic key-stream
    #   variant messages that fill the remaining SVE lanes; goldens for all
    #   lanes precomputed in init.
    # - EVP-only family (7: GCM x2, CMAC/XCBC x5): the workload has no SHA
    #   component to move to SVE (CMAC/XCBC are AES-CBC-MAC chains inside
    #   OpenSSL; upstream has no SVE AES path), so these are faithful
    #   compute-preserving ports with honest descriptions.
    if framework_config.get('SANDSTONE_SSL_BUILD') == 1
        tests_sve_avx53_ipsec = static_library(
            'tests_sve_avx53_ipsec',
            sources: files(
                'ipsec/ipsec_3des_docsis_hmac_sha1_sve_wide.cpp',
                'ipsec/ipsec_3des_docsis_hmac_sha224_sve_wide.cpp',
                'ipsec/ipsec_3des_docsis_hmac_sha256_sve_wide.cpp',
                'ipsec/ipsec_3des_docsis_hmac_sha384_sve_wide.cpp',
                'ipsec/ipsec_3des_docsis_hmac_sha512_sve_wide.cpp',
                'ipsec/ipsec_aes128_cbc_hmac_sha1_sve.cpp',
                'ipsec/ipsec_aes192_ctr_hmac_sha1_sve.cpp',
                'ipsec/ipsec_aes192_ctr_hmac_sha224_sve.cpp',
                'ipsec/ipsec_aes192_ctr_hmac_sha256_sve.cpp',
                'ipsec/ipsec_aes192_ctr_hmac_sha384_sve.cpp',
                'ipsec/ipsec_aes192_ctr_hmac_sha512_sve.cpp',
                'ipsec/ipsec_aes256_cbc_hmac_sha1_sve.cpp',
                'ipsec/ipsec_aes128_cbc_sha2_224_sve.cpp',
                'ipsec/ipsec_aes192_cbc_sha2_384_sve.cpp',
                'ipsec/ipsec_aes192_cbc_sha2_512_sve.cpp',
                'ipsec/ipsec_aes256_cbc_sha2_224_sve.cpp',
                'ipsec/ipsec_aes256_cbc_sha2_256_sve.cpp',
                'ipsec/ipsec_aes192_gcm_sve.cpp',
                'ipsec/ipsec_aes192_gcm_sve_wide.cpp',
                'ipsec/ipsec_3des_docsis_aes_cmac_sve_wide.cpp',
                'ipsec/ipsec_3des_docsis_aes_xcbc_sve_wide.cpp',
                'ipsec/ipsec_aes128_cbc_aes_cmac_sve.cpp',
                'ipsec/ipsec_aes192_cbc_xcbc_96_sve.cpp',
                'ipsec/ipsec_aes192_ctr_xcbc_sve.cpp',
            ),
            build_by_default: false,
            include_directories: [
                framework_incdir,
            ],
            dependencies: [
                boost_dep,
                crypto_dep,
            ],
            cpp_args: [
                debug_c_flags,
                default_cpp_warn,
                default_cpp_flags,
                march_generic_flags,
                '-march=armv8.2-a+sve',
            ],
        )
        sandstone_tests += [tests_sve_avx53_ipsec]
    endif

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
