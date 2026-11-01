/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k2pureload
 * @parblock
 * K2 store-geometry cell: PURE LOAD, no stores in the hot loop
 * (2026-09-13, docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md §5
 * K2 axis; exclusion matrix §6.3 "stale-generation" row).
 *
 * Adjudicates whether the store side is NECESSARY for the trigger: the
 * first GPS round already showed store->load forwarding is not needed
 * (temp is never reloaded, fails still happen) — but the back-to-back
 * str temp/str dst pair remains in the chassis, so "some store traffic
 * needed" is still open.  This cell removes ALL stores from the hot
 * loop: the ALU results are accumulated into a checksum register chain
 * (still consumed, so the loads stay live and the bypass network is
 * still exercised), and the checksum is compared against the golden
 * checksum once per sweep in the cold path.
 *
 * Outcomes (pre-registered):
 *   - full-rate fails with the same field-substitution signature and
 *     substitution sources STILL in srcA  -> stores not needed at all,
 *     "store_only(14) vs fwd_probe(0)" contradiction closed: neither
 *     store nor forwarding is necessary; the trigger is load-side only;
 *   - rate collapses to ~0 -> store traffic is a necessary amplifier;
 *     the old contradiction becomes "store presence matters, forwarding
 *     does not";
 *   - substitution sources change buffer (e.g. start appearing from
 *     scratch) -> removing stores rerouted the defect (signature shift
 *     is itself data).
 *
 * Checksum design (detector integrity): per-element XOR-fold of the ALU
 * result into 4 rotating accumulators (one per op class, so op-specific
 * corruption is not averaged away).  A single corrupted element flips
 * its accumulator; the cold-path compare catches it.  Note the
 * accumulation chain ADDS a serial dependency the chassis did not have
 * (xor into one register) — this is a known, declared deviation: the
 * alternative (per-element store) is exactly what this cell must not
 * do.  The dependency keeps loads live with minimal store-port impact.
 *
 * Hot loop delta vs GPS chassis (declared, single knob): str temp/str
 * dst pair replaced by eor-accumulate into v8..v11 (4 accumulators,
 * rotating by i%4 to match the op rotation).  Everything else —
 * scratch ldr at top, 2-src loads, i%4 ALU rotation, rowmajor
 * alternating sweeps, GPS tags, cold-path compare + dump — identical.
 *
 * Dump discipline: on checksum miscompare the cold path dumps
 * dst-equivalent (the accumulated checksums), expected checksums, and
 * the three source buffers, same directory as the GPS chassis for
 * classifier compatibility.  NOTE: without per-element stores the
 * failing ELEMENT cannot be located from the dump alone — the
 * accumulator flip only says "at least one element of that op class
 * was wrong".  The cell's primary readout is therefore RATE + which
 * accumulator class flipped; signature classification of individual
 * words is NOT available by design (that would require the stores this
 * cell removes).  No LCG seed fixing; auto-seed only.
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

#define K2PL_COUNT 1024
#define K2PL_DUMP_DIR "movbe_log/gps_rowmajor"

#define K2PL_MAGIC_A   0xA5A5ULL
#define K2PL_MAGIC_B   0x5A5AULL
#define K2PL_MAGIC_SCR 0x0F0FULL

struct K2PureLoadData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t  golden_sum[4];   /* per-op-class golden checksums */
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

static inline uint64_t gps_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                      << 48)
         | ((byte_off        & 0xFFFFULL) << 32)
         | ((~byte_off       & 0xFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k2pureload_init(struct test *test) {
    auto *data = static_cast<K2PureLoadData *>(malloc(sizeof(K2PureLoadData)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, K2PL_COUNT * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, K2PL_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K2PL_COUNT * sizeof(uint64x2_t)));

    mkdir(K2PL_DUMP_DIR, 0755);

    for (int i = 0; i < K2PL_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i], vcombine_u64(vcreate_u64(gps_tag(K2PL_MAGIC_A, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K2PL_MAGIC_A, byte + 8, 0))));
        store_vec(&data->srcB[i], vcombine_u64(vcreate_u64(gps_tag(K2PL_MAGIC_B, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K2PL_MAGIC_B, byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(gps_tag(K2PL_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(gps_tag(K2PL_MAGIC_SCR, byte + 8, 0))));
    }

    /* golden checksums: same accumulation the hot loop performs */
    uint64x2_t sum[4] = {
        vdupq_n_u64(0), vdupq_n_u64(0), vdupq_n_u64(0), vdupq_n_u64(0),
    };
    for (int i = 0; i < K2PL_COUNT; i++) {
        const uint64x2_t a = data->srcA[i];
        const uint64x2_t b = data->srcB[i];
        uint64x2_t r;
        switch (i % 4) {
            case 0: r = vaddq_u64(a, b); break;
            case 1: r = veorq_u64(a, b); break;
            case 2: r = vandq_u64(a, b); break;
            default: r = vorrq_u64(a, b); break;
        }
        sum[i % 4] = veorq_u64(sum[i % 4], r);
    }
    for (int k = 0; k < 4; k++)
        data->golden_sum[k] = sum[k];

    test->data = data;
    return EXIT_SUCCESS;
}

static int k2pureload_run(struct test *test, int cpu) {
    auto *data = static_cast<K2PureLoadData *>(test->data);

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        /* four NAMED accumulators (register-resident).  An acc[] array
         * would let GCC spill it to memory and turn the hot loop into a
         * load-modify-STORE per element — the exact op class this cell
         * must eliminate.  Verified by objdump: zero str in the loop. */
        uint64x2_t acc0 = vdupq_n_u64(0);
        uint64x2_t acc1 = vdupq_n_u64(0);
        uint64x2_t acc2 = vdupq_n_u64(0);
        uint64x2_t acc3 = vdupq_n_u64(0);

        for (int n = 0; n < K2PL_COUNT; n++) {
            const int i = ascending ? n : (K2PL_COUNT - 1 - n);

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

        /* COLD path: checksum compare per op class (both lanes) */
        bool bad = false;
        const uint64x2_t accs[4] = { acc0, acc1, acc2, acc3 };
        for (int k = 0; k < 4; k++) {
            const uint64_t g0 = vgetq_lane_u64(data->golden_sum[k], 0);
            const uint64_t g1 = vgetq_lane_u64(data->golden_sum[k], 1);
            const uint64_t a0 = vgetq_lane_u64(accs[k], 0);
            const uint64_t a1 = vgetq_lane_u64(accs[k], 1);
            if (g0 != a0 || g1 != a1)
                bad = true;
        }
        if (bad) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k2pl_fail_cpu%d_sweep%lu.bin", K2PL_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                const uint64x2_t accv[4] = { acc0, acc1, acc2, acc3 };
                uint64x2_t gold[4];
                for (int k = 0; k < 4; k++) { gold[k] = data->golden_sum[k]; }
                fwrite(accv,  sizeof(uint64x2_t), 4, f);
                fwrite(gold,  sizeof(uint64x2_t), 4, f);
                fwrite(data->srcA,    sizeof(uint64x2_t), K2PL_COUNT, f);
                fwrite(data->srcB,    sizeof(uint64x2_t), K2PL_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K2PL_COUNT, f);
                fprintf(f, "k2pureload sweep=%lu\n", static_cast<unsigned long>(sweep));
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k2pureload checksum miscompare");
        }
        sweep++;
    }

    return EXIT_SUCCESS;
}

static int k2pureload_cleanup(struct test *test) {
    auto *data = static_cast<K2PureLoadData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k2pureload, "K2 pure-load cell: NO stores in hot loop (xor-accumulate instead); adjudicates whether store traffic is necessary for the 179 read-path trigger; GPS tags + dump discipline")
    .test_init    = k2pureload_init,
    .test_run     = k2pureload_run,
    .test_cleanup = k2pureload_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
