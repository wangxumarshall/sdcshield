/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_store_only
 * @parblock
 * NEON variant with the reload REMOVED: 2 source vector loads + rotating
 * vector ALU {add,eor,and,orr} (uint64x2) + two plain vector stores of the
 * ALU result (store(temp); store(dst)) — no store->load forwarding window.
 * Pairs with agu_stress_store_only (scalar, 1 fail/30min single sample) to
 * adjudicate whether the back-to-back reload is a necessary condition or a
 * strong amplifier for the core-179 trigger, now on the NEON datapath where
 * the baseline (neon_rot_2src) fires ~33 fail/30min. Skeleton otherwise
 * identical to neon_rot_2src.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_STORE_ONLY_COUNT 1024

struct NeonRotStoreOnlyData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

/* store via NEON intrinsic (compiles to str q); source loads via inline asm
 * (same helper as neon_rot_2src). Only the RELOAD is removed in this variant:
 * both stores write the ALU result directly, no store->load window. */
static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int neon_rot_store_only_init(struct test *test) {
    auto *data = static_cast<NeonRotStoreOnlyData *>(malloc(sizeof(NeonRotStoreOnlyData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_STORE_ONLY_COUNT; i++) {
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

static int neon_rot_store_only_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotStoreOnlyData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_STORE_ONLY_COUNT; i++) {
            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            /* 2 stores of the ALU result, NO reload — removes the
             * store->load forwarding window entirely */
            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }

        if (memcmp(dst, data->expected, NEON_ROT_STORE_ONLY_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_store_only data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_store_only_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotStoreOnlyData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_store_only, "NEON variant: 2-src vector load + rotating ALU + 2 stores, NO reload (reload-necessity probe)")
    .test_init    = neon_rot_store_only_init,
    .test_run     = neon_rot_store_only_run,
    .test_cleanup = neon_rot_store_only_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
