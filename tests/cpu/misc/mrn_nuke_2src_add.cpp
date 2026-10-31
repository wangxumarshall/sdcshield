/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_nuke_2src_add
 * @parblock
 * Variant of mrn_nuke_2src_alu using ONLY the add instruction (a + b), to test
 * which single ALU instruction triggers SDC most. mrn_nuke_2src_alu (mixed
 * add/sub/eor/and) triggers at 14fail/36min; this isolates add.
 *
 * Skeleton (2-source load, store→forwarding load→store, back-to-back) UNCHANGED.
 * Only the ALU operation is fixed to add (no i%4 rotation).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_NUKE_2SRC_ADD_COUNT 1024

struct MrnNuke2srcAddData {
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

static int mrn_nuke_2src_add_init(struct test *test) {
    auto *data = static_cast<MrnNuke2srcAddData *>(malloc(sizeof(MrnNuke2srcAddData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_NUKE_2SRC_ADD_COUNT; i++) {
        data->expected[i] = data->srcA[i] + data->srcB[i];
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_nuke_2src_add_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnNuke2srcAddData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_NUKE_2SRC_ADD_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res = a + b;  // ONLY add

            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_NUKE_2SRC_ADD_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("mrn_nuke_2src_add data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_nuke_2src_add_cleanup(struct test *test) {
    auto *data = static_cast<MrnNuke2srcAddData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_nuke_2src_add, "mrn_nuke variant: 2-source load + add-only ALU (isolate add instruction)")
    .test_init    = mrn_nuke_2src_add_init,
    .test_run     = mrn_nuke_2src_add_run,
    .test_cleanup = mrn_nuke_2src_add_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
