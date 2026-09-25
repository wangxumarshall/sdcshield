/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k5inter_rand
 * @parblock
 * Interleaved-layout NATURAL-data control for the K5 cell (2026-09-13).
 *
 * k5far changes TWO things vs the GPS chassis at once: the source
 * layout (two parallel buffers -> one interleaved buffer) and keeps the
 * tags.  This cell holds the interleaved layout but fills with NATURAL
 * random data (memset_random), so the k5far-vs-this pair isolates the
 * LAYOUT effect and this-vs-parent isolates layout-without-tags.  It
 * is also the dump-capable A-track of Phase 0.5 under the champion
 * layout.  Rate readout only (no tags -> no fine classification); dump
 * gives coarse classification (recoverable-window / Hamming / byte
 * granularity) per design doc §9.2.
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

#define K5IR_COUNT 1024
#define K5IR_SRC_ELEMS (K5IR_COUNT * 2)
#define K5IR_DUMP_DIR "movbe_log/gps_rowmajor"

struct K5InterRandData {
    uint64x2_t *src;
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

static int k5ir_init(struct test *test) {
    auto *data = static_cast<K5InterRandData *>(malloc(sizeof(K5InterRandData)));
    data->src     = static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_SRC_ELEMS * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_COUNT * sizeof(uint64x2_t)));

    mkdir(K5IR_DUMP_DIR, 0755);

    memset_random(data->src, K5IR_SRC_ELEMS * sizeof(uint64x2_t));
    memset_random(data->scratch, K5IR_COUNT * sizeof(uint64x2_t));

    for (int i = 0; i < K5IR_COUNT; i++) {
        const uint64x2_t a = data->src[2 * i];
        const uint64x2_t b = data->src[2 * i + 1];
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(a, b); break;
            case 1: data->expected[i] = veorq_u64(a, b); break;
            case 2: data->expected[i] = vandq_u64(a, b); break;
            case 3: data->expected[i] = vorrq_u64(a, b); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k5ir_run(struct test *test, int cpu) {
    auto *data = static_cast<K5InterRandData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < K5IR_COUNT; n++) {
            const int i = ascending ? n : (K5IR_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->src[2 * i]);
            uint64x2_t b = load_vec(&data->src[2 * i + 1]);
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

        if (memcmp(dst, data->expected, K5IR_COUNT * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k5ir_fail_cpu%d_sweep%lu.bin", K5IR_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                uint64x2_t *viewA = static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_COUNT * sizeof(uint64x2_t)));
                uint64x2_t *viewB = static_cast<uint64x2_t *>(aligned_alloc(64, K5IR_COUNT * sizeof(uint64x2_t)));
                for (int i = 0; i < K5IR_COUNT; i++) {
                    viewA[i] = data->src[2 * i];
                    viewB[i] = data->src[2 * i + 1];
                }
                fwrite(dst,        sizeof(uint64x2_t), K5IR_COUNT, f);
                fwrite(data->expected, sizeof(uint64x2_t), K5IR_COUNT, f);
                fwrite(viewA,      sizeof(uint64x2_t), K5IR_COUNT, f);
                fwrite(viewB,      sizeof(uint64x2_t), K5IR_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K5IR_COUNT, f);
                fprintf(f, "k5inter_rand sweep=%lu\n", static_cast<unsigned long>(sweep));
                fclose(f);
                free(viewA);
                free(viewB);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k5inter_rand data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k5ir_cleanup(struct test *test) {
    auto *data = static_cast<K5InterRandData *>(test->data);
    if (data) {
        free(data->src);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k5inter_rand, "K5 layout control: interleaved dual-source layout with NATURAL random data (no tags) — isolates layout effect from tag effect; dump-capable A-track")
    .test_init    = k5ir_init,
    .test_run     = k5ir_run,
    .test_cleanup = k5ir_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
