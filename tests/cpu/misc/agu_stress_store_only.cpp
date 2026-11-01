/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b agu_stress_store_only
 * @parblock
 * AGU stress: store-heavy probe. Per element: 2 source loads + rotating ALU +
 * 2 stores — but NO reload of temp (no store->load forwarding window).
 * Tests the prediction that removing the back-to-back reload (while keeping
 * 2-src loads, ALU rotation, and store pressure) kills the trigger.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define AGU_STORE_ONLY_COUNT 1024

struct AguStoreOnlyData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *expected;
};

#if defined(__x86_64__) || defined(__i386__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("movq %1, %0" : "=m"(*reinterpret_cast<uint64_t*>(addr)) : "r"(val) : "memory");
}

#elif defined(__aarch64__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("str %1, [%0]" : : "r"(addr), "r"(val) : "memory");
}

#endif

static int agu_stress_store_only_init(struct test *test) {
    auto *data = static_cast<AguStoreOnlyData *>(malloc(sizeof(AguStoreOnlyData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, AGU_STORE_ONLY_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, AGU_STORE_ONLY_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, AGU_STORE_ONLY_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, AGU_STORE_ONLY_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, AGU_STORE_ONLY_COUNT * sizeof(uint64_t));

    for (int i = 0; i < AGU_STORE_ONLY_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] | data->srcB[i]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int agu_stress_store_only_run(struct test *test, int cpu) {
    auto *data = static_cast<AguStoreOnlyData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, AGU_STORE_ONLY_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, AGU_STORE_ONLY_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < AGU_STORE_ONLY_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res;

            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a ^ b; break;
                case 2: res = a & b; break;
                case 3: res = a | b; break;
            }

            // 2 stores, NO reload — removes the store->load forwarding window
            store_qword(&temp[i], res);
            store_qword(&dst[i], res);
        }

        if (memcmp(dst, data->expected, AGU_STORE_ONLY_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("agu_stress_store_only data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int agu_stress_store_only_cleanup(struct test *test) {
    auto *data = static_cast<AguStoreOnlyData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(agu_stress_store_only, "AGU stress: store-only probe (2-src + ALU rotation, no reload/forwarding)")
    .test_init    = agu_stress_store_only_init,
    .test_run     = agu_stress_store_only_run,
    .test_cleanup = agu_stress_store_only_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
