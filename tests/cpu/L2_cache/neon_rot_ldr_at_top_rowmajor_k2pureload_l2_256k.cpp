/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k2pureload_l2_256k
 * @parblock
 * L2-resident tier 256KB of neon_rot_ldr_at_top_rowmajor_k2pureload
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * pure-load axis is the object under test — NO stores in the hot loop
 * (four NAMED register-resident accumulators; an acc[] array would let
 * GCC spill it to memory and turn the loop into load-modify-STORE per
 * element — the exact op class this cell eliminates; objdump gate:
 * zero str in the loop), inherited verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_k2pureload.cpp).
 *
 * GOLDEN DELTA (geometry-driven, disclosed): the parent precomputes
 * golden_sum at init over its FIXED buffer; under the sliding window
 * the per-op-class xor sums are SLICE-DEPENDENT, so they cannot be
 * precomputed once.  This tier computes the reference sums SCALAR over
 * the window slice AFTER each hot loop (the loop_check_m
 * recompute-and-compare paradigm — pure C integer ops on the scalar
 * pipeline, never the NEON units under test); a vector-path fault
 * surfaces as scalar-vs-vector divergence at the checksum compare.
 * The hot loop (3 asm ldr + rot ALU + register accumulate) is
 * untouched.
 *
 * TAG LAYOUT REDESIGN (this tier family's own format, disclosed): the
 * parent's 16-bit byte_off caps at 65520; this family's tag is
 * (magic<<48)|((off&0xFFFFFFFF)<<16)|gen — 32-bit offset, the
 * one's-complement integrity field dropped.
 *
 * Input-data method transplant: loop_check_m tier sizes (fixed block =
 * tier) + memcpy_l2_cache_size sliding window (one random 64B-aligned
 * SHARED offset per sweep through a 2x window).  Tagged fill covers
 * the FULL window at init.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64x2_t = 256KB (inside the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads only (NO store traffic — the cell's
 * axis) = 3 x 256KB = 768KB — exceeds L2 (the no-store chassis at this tier) (the no-store
 * chassis geometry; the tier axis is the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRK2P256_LEN 16384                  /* uint64x2_t elements -> 256KB block */
#define NRK2P256_WIN (2 * NRK2P256_LEN)   /* 2x-block window (memcpy_l2 method) */
#define NRK2P256_DUMP_DIR "movbe_log/gps_rowmajor_l2"

#define NRK2P256_MAGIC_A   0xA5A5ULL
#define NRK2P256_MAGIC_B   0x5A5AULL
#define NRK2P256_MAGIC_SCR 0x0F0FULL

struct NRK2P256Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
};

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

/* 32-bit-offset tag — this tier family's own format (see header) */
static inline uint64_t nrk2p128_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                        << 48)
         | ((byte_off        & 0xFFFFFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k2pureload_l2_256k_init(struct test *test) {
    auto *data = static_cast<NRK2P256Data *>(malloc(sizeof(NRK2P256Data)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK2P256_WIN * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK2P256_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRK2P256_WIN * sizeof(uint64x2_t)));

    mkdir("movbe_log", 0755);
    mkdir(NRK2P256_DUMP_DIR, 0755);

    /* Tagged fill over the FULL window (complementary magics) */
    for (int i = 0; i < NRK2P256_WIN; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i],    vcombine_u64(vcreate_u64(nrk2p128_tag(NRK2P256_MAGIC_A,   byte + 0, 0)),
                                                  vcreate_u64(nrk2p128_tag(NRK2P256_MAGIC_A,   byte + 8, 0))));
        store_vec(&data->srcB[i],    vcombine_u64(vcreate_u64(nrk2p128_tag(NRK2P256_MAGIC_B,   byte + 0, 0)),
                                                  vcreate_u64(nrk2p128_tag(NRK2P256_MAGIC_B,   byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(nrk2p128_tag(NRK2P256_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(nrk2p128_tag(NRK2P256_MAGIC_SCR, byte + 8, 0))));
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k2pureload_l2_256k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRK2P256Data *>(test->data);

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const int off = (int)(4 * (random32() % (NRK2P256_LEN / 4)));

        const bool ascending = (sweep & 1) == 0;
        /* four NAMED accumulators (register-resident).  An acc[] array
         * would let GCC spill it to memory and turn the hot loop into a
         * load-modify-STORE per element — the exact op class this cell
         * must eliminate.  Verified by objdump: zero str in the loop. */
        uint64x2_t acc0 = vdupq_n_u64(0);
        uint64x2_t acc1 = vdupq_n_u64(0);
        uint64x2_t acc2 = vdupq_n_u64(0);
        uint64x2_t acc3 = vdupq_n_u64(0);

        for (int n = 0; n < NRK2P256_LEN; n++) {
            const int j = ascending ? n : (NRK2P256_LEN - 1 - n);
            const int i = off + j;

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            /* K2 knob: NO stores — accumulate into named registers */
            switch (i % 4) {
                case 0: acc0 = veorq_u64(acc0, res); break;
                case 1: acc1 = veorq_u64(acc1, res); break;
                case 2: acc2 = veorq_u64(acc2, res); break;
                case 3: acc3 = veorq_u64(acc3, res); break;
            }
        }

        /* Slice-dependent golden: scalar recompute over [off, off+LEN)
         * AFTER the hot loop (loop_check_m recompute-and-compare — see
         * header GOLDEN DELTA).  Both lanes kept separate, the parent's
         * per-class two-lane compare (no fold: a shift-fold would let
         * colliding bit pairs cancel). */
        uint64_t r0l0 = 0, r0l1 = 0, r1l0 = 0, r1l1 = 0;
        uint64_t r2l0 = 0, r2l1 = 0, r3l0 = 0, r3l1 = 0;
        {
            const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
            const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
            for (int k = 0; k < NRK2P256_LEN; k++) {
                const int i = off + k;
                const uint64_t a0 = sa[2*i+0], a1 = sa[2*i+1];
                const uint64_t b0 = sb[2*i+0], b1 = sb[2*i+1];
                switch (i % 4) {
                    case 0: r0l0 ^= a0 + b0; r0l1 ^= a1 + b1; break;
                    case 1: r1l0 ^= a0 ^ b0; r1l1 ^= a1 ^ b1; break;
                    case 2: r2l0 ^= a0 & b0; r2l1 ^= a1 & b1; break;
                    case 3: r3l0 ^= a0 | b0; r3l1 ^= a1 | b1; break;
                }
            }
        }
        const uint64_t acc0l0 = vgetq_lane_u64(acc0, 0), acc0l1 = vgetq_lane_u64(acc0, 1);
        const uint64_t acc1l0 = vgetq_lane_u64(acc1, 0), acc1l1 = vgetq_lane_u64(acc1, 1);
        const uint64_t acc2l0 = vgetq_lane_u64(acc2, 0), acc2l1 = vgetq_lane_u64(acc2, 1);
        const uint64_t acc3l0 = vgetq_lane_u64(acc3, 0), acc3l1 = vgetq_lane_u64(acc3, 1);

        /* COLD path: checksum compare per op class (both lanes) */
        if (r0l0 != acc0l0 || r0l1 != acc0l1 ||
            r1l0 != acc1l0 || r1l1 != acc1l1 ||
            r2l0 != acc2l0 || r2l1 != acc2l1 ||
            r3l0 != acc3l0 || r3l1 != acc3l1) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k2pl_l2_256k_fail_cpu%d_sweep%lu_off%d.bin", NRK2P256_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(data->srcA + off,    sizeof(uint64x2_t), NRK2P256_LEN, f);
                fwrite(data->srcB + off,    sizeof(uint64x2_t), NRK2P256_LEN, f);
                fwrite(data->scratch + off, sizeof(uint64x2_t), NRK2P256_LEN, f);
                fprintf(f, "k2pl_l2_256k sweep=%lu off=%d accs=%016lx,%016lx / %016lx,%016lx / %016lx,%016lx / %016lx,%016lx refs=%016lx,%016lx / %016lx,%016lx / %016lx,%016lx / %016lx,%016lx\n",
                        static_cast<unsigned long>(sweep), off,
                        acc0l0, acc0l1, acc1l0, acc1l1, acc2l0, acc2l1, acc3l0, acc3l1,
                        r0l0, r0l1, r1l0, r1l1, r2l0, r2l1, r3l0, r3l1);
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k2pureload_l2_256k checksum miscompare "
                            "(sweep %lu off %u, records in dump bin)",
                            static_cast<unsigned long>(sweep), (unsigned)off);
        }
        sweep++;
    }

    return EXIT_SUCCESS;
}

static int k2pureload_l2_256k_cleanup(struct test *test) {
    auto *data = static_cast<NRK2P256Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k2pureload_l2_256k, "L2-resident tier 256KB of the K2 pure-load cell: NO stores in the hot loop (named register accumulators, objdump gate zero str), tagged fill (32-bit-offset family format), slice-dependent golden via post-loop scalar recompute (loop_check_m recompute-and-compare — geometry-driven delta disclosed), sliding-window geometry transplanted (ARM64)")
    .test_init    = k2pureload_l2_256k_init,
    .test_run     = k2pureload_l2_256k_run,
    .test_cleanup = k2pureload_l2_256k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
