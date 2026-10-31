/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_pairs_2src_alu_keep8
 * @parblock
 * Variant of mrn_pairs that adds 2-source-load + data-ALU (like 5a) but KEEPS
 * the 8-store-then-8-load unroll (timing spread by 7 instructions).
 *
 * Purpose: isolate whether the 7-instruction spread is the deactivating factor.
 * If mrn_pairs_2src_alu (5a, back-to-back) triggers but this (5b, 8+8 spread)
 * does not, the spread kills the trigger — confirming "tight timing is
 * necessary" while showing 2src+ALU alone is not sufficient when spread.
 *
 * Diff vs mrn_pairs: 1 source → 2 sources; raw value → ALU result (a op b).
 * Unroll structure (8 stores then 8 loads) UNCHANGED from mrn_pairs.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_PAIRS_2SRC_ALU_KEEP8_COUNT 1024

struct MrnPairs2srcAluKeep8Data {
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

static int mrn_pairs_2src_alu_keep8_init(struct test *test) {
    auto *data = static_cast<MrnPairs2srcAluKeep8Data *>(malloc(sizeof(MrnPairs2srcAluKeep8Data)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_PAIRS_2SRC_ALU_KEEP8_COUNT; i++) {
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

static int mrn_pairs_2src_alu_keep8_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnPairs2srcAluKeep8Data *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        // 8-store-then-8-load unroll UNCHANGED from mrn_pairs (timing spread 7).
        // Only diff: 2 sources + ALU result instead of raw value.
        for (int i = 0; i < MRN_PAIRS_2SRC_ALU_KEEP8_COUNT; i += 8) {
            uint64_t res[8];
            // 8 stores (ALU on 2 sources)
            for (int j = 0; j < 8; j++) {
                uint64_t a = data->srcA[i+j];
                uint64_t b = data->srcB[i+j];
                switch ((i+j) % 4) {
                    case 0: res[j] = a + b; break;
                    case 1: res[j] = a - b; break;
                    case 2: res[j] = a ^ b; break;
                    case 3: res[j] = a & b; break;
                }
                store_qword(&temp[i+j], res[j]);
            }
            // 8 loads (forwarding, but spread by 7 instructions from its store)
            for (int j = 0; j < 8; j++) {
                store_qword(&dst[i+j], load_qword(&temp[i+j]));
            }
        }

        if (memcmp(dst, data->expected, MRN_PAIRS_2SRC_ALU_KEEP8_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("mrn_pairs_2src_alu_keep8 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_pairs_2src_alu_keep8_cleanup(struct test *test) {
    auto *data = static_cast<MrnPairs2srcAluKeep8Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_pairs_2src_alu_keep8, "mrn_pairs variant: 2-source load + data-ALU, KEEPS 8+8 spread (vs back-to-back)")
    .test_init    = mrn_pairs_2src_alu_keep8_init,
    .test_run     = mrn_pairs_2src_alu_keep8_run,
    .test_cleanup = mrn_pairs_2src_alu_keep8_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
