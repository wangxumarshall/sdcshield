/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_nuke_2src_sub
 * @parblock
 * Variant of mrn_nuke_2src_alu using ONLY the sub instruction (a - b), to
 * isolate sub's trigger rate. Skeleton unchanged from mrn_nuke_2src_alu.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_NUKE_2SRC_SUB_COUNT 1024

struct MrnNuke2srcSubData {
    uint64_t *srcA;
    uint64_t *srcB;
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

static int mrn_nuke_2src_sub_init(struct test *test) {
    auto *data = static_cast<MrnNuke2srcSubData *>(malloc(sizeof(MrnNuke2srcSubData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_NUKE_2SRC_SUB_COUNT; i++) {
        data->expected[i] = data->srcA[i] - data->srcB[i];
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_nuke_2src_sub_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnNuke2srcSubData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_NUKE_2SRC_SUB_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res = a - b;  // ONLY sub

            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_NUKE_2SRC_SUB_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("mrn_nuke_2src_sub data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_nuke_2src_sub_cleanup(struct test *test) {
    auto *data = static_cast<MrnNuke2srcSubData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_nuke_2src_sub, "mrn_nuke variant: 2-source load + sub-only ALU (isolate sub instruction)")
    .test_init    = mrn_nuke_2src_sub_init,
    .test_run     = mrn_nuke_2src_sub_run,
    .test_cleanup = mrn_nuke_2src_sub_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
