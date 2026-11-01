/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b agu_stress_1src
 * @parblock
 * AGU stress control variant: 1 source load only (vs agu_stress_2src's 2).
 * Tests whether the second source load (and its address-generation pressure)
 * is necessary for triggering. Skeleton otherwise identical to agu_stress_2src.
 * Expected per src-load-gradient result (1src = 0-fail); serves as the
 * in-campaign negative control.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define AGU_STRESS_1SRC_COUNT 1024

struct AguStress1srcData {
    uint64_t *srcA;
    uint64_t *expected;
};

#if defined(__x86_64__) || defined(__i386__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("movq %1, %0" : "=m"(*reinterpret_cast<uint64_t*>(addr)) : "r"(val) : "memory");
}

static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("movq %1, %0" : "=r"(res) : "m"(*reinterpret_cast<const uint64_t*>(addr)) : "memory");
    return res;
}

#elif defined(__aarch64__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("str %1, [%0]" : : "r"(addr), "r"(val) : "memory");
}

static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=r"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int agu_stress_1src_init(struct test *test) {
    auto *data = static_cast<AguStress1srcData *>(malloc(sizeof(AguStress1srcData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, AGU_STRESS_1SRC_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, AGU_STRESS_1SRC_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, AGU_STRESS_1SRC_COUNT * sizeof(uint64_t));

    for (int i = 0; i < AGU_STRESS_1SRC_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + 0x5A5A5A5A5A5A5A5AULL; break;
            case 1: data->expected[i] = data->srcA[i] ^ 0x5A5A5A5A5A5A5A5AULL; break;
            case 2: data->expected[i] = data->srcA[i] & 0x5A5A5A5A5A5A5A5AULL; break;
            case 3: data->expected[i] = data->srcA[i] | 0x5A5A5A5A5A5A5A5AULL; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int agu_stress_1src_run(struct test *test, int cpu) {
    auto *data = static_cast<AguStress1srcData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, AGU_STRESS_1SRC_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, AGU_STRESS_1SRC_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < AGU_STRESS_1SRC_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t res;

            switch (i % 4) {
                case 0: res = a + 0x5A5A5A5A5A5A5A5AULL; break;
                case 1: res = a ^ 0x5A5A5A5A5A5A5A5AULL; break;
                case 2: res = a & 0x5A5A5A5A5A5A5A5AULL; break;
                case 3: res = a | 0x5A5A5A5A5A5A5A5AULL; break;
            }

            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, AGU_STRESS_1SRC_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("agu_stress_1src data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int agu_stress_1src_cleanup(struct test *test) {
    auto *data = static_cast<AguStress1srcData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(agu_stress_1src, "AGU stress control: 1-source load + rotating ALU on {srcA, constant}")
    .test_init    = agu_stress_1src_init,
    .test_run     = agu_stress_1src_run,
    .test_cleanup = agu_stress_1src_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
