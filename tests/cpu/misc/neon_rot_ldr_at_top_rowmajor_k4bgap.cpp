/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k4bgap
 * @parblock
 * K4b ALU-to-store gap gradient cell (2026-09-13,
 * docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md §5 K4b axis;
 * exclusion matrix §6.3 "ALU-mix-in" row).
 *
 * Splits WHERE the corrupted fields enter: E3 (ALU result -> store
 * data, 0-cycle) vs the writeback/operand network behind it.  The
 * first GPS round's field-substitution signature replaced off|~off
 * fields with a NEIGHBOURING srcA word's fields while the ALU result
 * was being produced and stored — if the corruption happens on the
 * ALU->store-data forwarding edge, pushing the store away from the ALU
 * (closing that bypass window) must change the signature or the rate;
 * if the corruption happens earlier (load delivery / operand network),
 * the gap should not matter.
 *
 * Far extreme: 8 architectural NOPs between the rotating ALU and the
 * back-to-back str temp/str dst pair.  Everything else byte-identical
 * to the GPS chassis (this file is a one-knob delta of k4gap.cpp with
 * the gap moved after the ALU).
 *
 * Pre-registered readouts:
 *   - same rate + same signature -> E3 (ALU->store bypass) is NOT the
 *     injection point; the corruption is upstream of the ALU (load
 *     delivery / operand), consistent with the E1/E2 narrowing;
 *   - rate or signature changes -> the ALU->store edge participates;
 *     middle points (1/2/4 NOPs) become interesting and are one-line
 *     deltas of this file.
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

#define K4BGAP_COUNT 1024
#define K4BGAP_DUMP_DIR "movbe_log/gps_rowmajor"

#define K4BGAP_MAGIC_A   0xA5A5ULL
#define K4BGAP_MAGIC_B   0x5A5AULL
#define K4BGAP_MAGIC_SCR 0x0F0FULL

struct K4BGapData {
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

static int k4bgap_init(struct test *test) {
    auto *data = static_cast<K4BGapData *>(malloc(sizeof(K4BGapData)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, K4BGAP_COUNT * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, K4BGAP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K4BGAP_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, K4BGAP_COUNT * sizeof(uint64x2_t)));

    mkdir(K4BGAP_DUMP_DIR, 0755);

    for (int i = 0; i < K4BGAP_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i], vcombine_u64(vcreate_u64(gps_tag(K4BGAP_MAGIC_A, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K4BGAP_MAGIC_A, byte + 8, 0))));
        store_vec(&data->srcB[i], vcombine_u64(vcreate_u64(gps_tag(K4BGAP_MAGIC_B, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K4BGAP_MAGIC_B, byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(gps_tag(K4BGAP_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(gps_tag(K4BGAP_MAGIC_SCR, byte + 8, 0))));
    }

    for (int i = 0; i < K4BGAP_COUNT; i++) {
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

static int k4bgap_run(struct test *test, int cpu) {
    auto *data = static_cast<K4BGapData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, K4BGAP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, K4BGAP_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < K4BGAP_COUNT; n++) {
            const int i = ascending ? n : (K4BGAP_COUNT - 1 - n);

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

            /* K4b knob: 8-NOP gap between ALU and the stores — closes
             * the ALU->store-data bypass window */
            __asm__ volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, K4BGAP_COUNT * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k4bgap8_fail_cpu%d_sweep%lu.bin", K4BGAP_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst,           sizeof(uint64x2_t), K4BGAP_COUNT, f);
                fwrite(data->expected,sizeof(uint64x2_t), K4BGAP_COUNT, f);
                fwrite(data->srcA,    sizeof(uint64x2_t), K4BGAP_COUNT, f);
                fwrite(data->srcB,    sizeof(uint64x2_t), K4BGAP_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K4BGAP_COUNT, f);
                fprintf(f, "k4bgap8 sweep=%lu\n", static_cast<unsigned long>(sweep));
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k4bgap data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k4bgap_cleanup(struct test *test) {
    auto *data = static_cast<K4BGapData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k4bgap, "K4b far-gap cell: 8 NOPs between rotating ALU and the back-to-back stores (ALU->store-data bypass closed); splits E3 injection vs upstream load delivery")
    .test_init    = k4bgap_init,
    .test_run     = k4bgap_run,
    .test_cleanup = k4bgap_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
