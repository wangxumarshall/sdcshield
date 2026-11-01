/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k4gap
 * @parblock
 * K4 load-to-consume gap gradient cell (2026-09-13,
 * docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md §5 K4 axis).
 *
 * The first GPS round's dominant signature (same-buffer FIELD
 * SUBSTITUTION from a neighbouring srcA word, 156/156 in srcA) needs a
 * mechanistic split the rate alone cannot give: is the corrupted data
 * captured on the LOAD RESULT BYPASS (0-cycle load->use path) or on the
 * REGISTER FILE read (when the consumer is far from the load)?
 *
 * OoO honesty (per design doc §5): on an out-of-order core the "gap" is
 * a soft control — inserting NOPs between load and consumer only closes
 * the bypass window (forcing the register file path); the two extremes
 * are mechanically clean, the middle is a mix.  Old data already shows
 * this axis is strongly non-monotonic (mrn_rmw nop 0/1/2/64 =
 * 18/1/6/0) — the SHAPE of non-monotonicity is itself information.
 *
 * This cell fixes the gap at the FAR extreme: 8 architectural NOPs
 * between the 2-src loads and the rotating ALU (well beyond any bypass
 * window), everything else byte-identical to the GPS chassis.  Paired
 * with the GPS chassis itself (gap=0) this brackets the axis; if
 * first-round results demand the middle points, 1/2/4-NOP variants are
 * one-line deltas of this file.
 *
 * Pre-registered readouts:
 *   - far-gap keeps full rate + same field-substitution signature ->
 *     bypass is NOT the exclusive path; register-file read (or a later
 *     shared segment) carries the defect too;
 *   - far-gap collapses to ~0 -> the defect rides the 0-cycle bypass
 *     (narrowing to E1/E2 edges);
 *   - substitution DISTANCE histogram shifts with gap -> the corruption
 *     window itself moves (port arbitration timing).
 *
 * The NOPs use the family's established asm volatile ("nop") idiom
 * (neon_rot_nop_gap.cpp) so the compiler cannot fold or reorder them.
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

#define K4GAP_COUNT 1024
#define K4GAP_NOPS  8
#define K4GAP_DUMP_DIR "movbe_log/gps_rowmajor"

#define K4GAP_MAGIC_A   0xA5A5ULL
#define K4GAP_MAGIC_B   0x5A5AULL
#define K4GAP_MAGIC_SCR 0x0F0FULL

struct K4GapData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
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

static int k4gap_init(struct test *test) {
    auto *data = static_cast<K4GapData *>(malloc(sizeof(K4GapData)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, K4GAP_COUNT * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, K4GAP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K4GAP_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, K4GAP_COUNT * sizeof(uint64x2_t)));

    mkdir(K4GAP_DUMP_DIR, 0755);

    for (int i = 0; i < K4GAP_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i], vcombine_u64(vcreate_u64(gps_tag(K4GAP_MAGIC_A, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K4GAP_MAGIC_A, byte + 8, 0))));
        store_vec(&data->srcB[i], vcombine_u64(vcreate_u64(gps_tag(K4GAP_MAGIC_B, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K4GAP_MAGIC_B, byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(gps_tag(K4GAP_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(gps_tag(K4GAP_MAGIC_SCR, byte + 8, 0))));
    }

    for (int i = 0; i < K4GAP_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k4gap_run(struct test *test, int cpu) {
    auto *data = static_cast<K4GapData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, K4GAP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, K4GAP_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < K4GAP_COUNT; n++) {
            const int i = ascending ? n : (K4GAP_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);

            /* K4 knob: 8-NOP gap between loads and consumer — closes
             * the bypass window, forces the register-file path */
            __asm__ volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");

            uint64x2_t res;
            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, K4GAP_COUNT * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k4gap8_fail_cpu%d_sweep%lu.bin", K4GAP_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst,           sizeof(uint64x2_t), K4GAP_COUNT, f);
                fwrite(data->expected,sizeof(uint64x2_t), K4GAP_COUNT, f);
                fwrite(data->srcA,    sizeof(uint64x2_t), K4GAP_COUNT, f);
                fwrite(data->srcB,    sizeof(uint64x2_t), K4GAP_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K4GAP_COUNT, f);
                fprintf(f, "k4gap8 sweep=%lu\n", static_cast<unsigned long>(sweep));
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k4gap data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k4gap_cleanup(struct test *test) {
    auto *data = static_cast<K4GapData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k4gap, "K4 far-gap cell: 8 NOPs between 2-src loads and rotating ALU (bypass window closed, register-file path forced); brackets the load-to-consume axis with the GPS chassis (gap=0)")
    .test_init    = k4gap_init,
    .test_run     = k4gap_run,
    .test_cleanup = k4gap_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
