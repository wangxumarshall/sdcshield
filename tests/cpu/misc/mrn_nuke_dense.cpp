/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_nuke_dense
 * @parblock
 * Variant of mrn_nuke with the per-loop element count raised from 1024 to
 * 16384, to test the "load absolute count" hypothesis. movbe triggers SDC at
 * ~32768 ldr/loop while mrn_nuke 0-fails at 24576 ldr/loop (1024 elements * 3
 * loads * 8 unroll); this variant raises mrn_nuke's density to ~49152 ldr/loop
 * to see if a density threshold exists. Skeleton (forwarding, raw value,
 * 1-source) unchanged from mrn_nuke.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_NUKE_DENSE_COUNT 16384  /* 16x mrn_nuke's 1024 */

struct MrnNukeDenseData {
    uint64_t *src;
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

static int mrn_nuke_dense_init(struct test *test) {
    auto *data = static_cast<MrnNukeDenseData *>(malloc(sizeof(MrnNukeDenseData)));
    data->src = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_COUNT * sizeof(uint64_t)));
    memset_random(data->src, MRN_NUKE_DENSE_COUNT * sizeof(uint64_t));
    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_nuke_dense_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnNukeDenseData *>(test->data);

    // Per-thread buffers (16x larger: 128KB temp + 128KB dst)
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_NUKE_DENSE_COUNT; i++) {
            // Skeleton UNCHANGED from mrn_nuke: store temp, forwarding load, store dst
            // Only the element count (and thus ldr/loop density) is raised.
            store_qword(&temp[i], data->src[i]);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->src, MRN_NUKE_DENSE_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("mrn_nuke_dense data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_nuke_dense_cleanup(struct test *test) {
    auto *data = static_cast<MrnNukeDenseData *>(test->data);
    if (data) {
        free(data->src);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_nuke_dense, "mrn_nuke variant: 16x element count (16384) to test load-density threshold hypothesis")
    .test_init = mrn_nuke_dense_init,
    .test_run = mrn_nuke_dense_run,
    .test_cleanup = mrn_nuke_dense_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
