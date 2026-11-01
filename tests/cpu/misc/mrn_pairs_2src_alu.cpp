/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_pairs_2src_alu
 * @parblock
 * Variant of mrn_pairs that adds the 2-source-load + data-ALU dependency chain
 * (the strong trigger feature from mrn_nuke_2src_alu / mrn_rmw) AND collapses
 * the 8-store-then-8-load unroll into per-element back-to-back store→load.
 *
 * mrn_pairs original: 1 source, 8 stores (raw value) then 8 loads (forwarding,
 * but timing spread by 7 instructions) → 0-fail on core 179.
 *
 * This variant (5a): 2 sources (srcA+srcB) + ALU (a op b) + per-element
 * back-to-back store→load forwarding. Tests whether "2src+ALU" triggers when
 * timing is tightened to back-to-back.
 *
 * Compare with mrn_pairs_2src_alu_keep8 (5b) which keeps the 8+8 spread: if 5a
 * triggers and 5b does not, the 7-instruction spread is the deactivating factor.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_PAIRS_2SRC_ALU_COUNT 1024

struct MrnPairs2srcAluData {
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

static int mrn_pairs_2src_alu_init(struct test *test) {
    auto *data = static_cast<MrnPairs2srcAluData *>(malloc(sizeof(MrnPairs2srcAluData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_PAIRS_2SRC_ALU_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] - data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_pairs_2src_alu_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnPairs2srcAluData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        // Per-element back-to-back store→load forwarding with 2-source + ALU.
        // (Collapsed from mrn_pairs' 8+8 unroll to tighten timing to back-to-back.)
        for (int i = 0; i < MRN_PAIRS_2SRC_ALU_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res;

            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a - b; break;
                case 2: res = a ^ b; break;
                case 3: res = a & b; break;
            }

            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_PAIRS_2SRC_ALU_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("mrn_pairs_2src_alu data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_pairs_2src_alu_cleanup(struct test *test) {
    auto *data = static_cast<MrnPairs2srcAluData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_pairs_2src_alu, "mrn_pairs variant: 2-source load + data-ALU, per-element back-to-back (vs 8+8 spread)")
    .test_init    = mrn_pairs_2src_alu_init,
    .test_run     = mrn_pairs_2src_alu_run,
    .test_cleanup = mrn_pairs_2src_alu_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
