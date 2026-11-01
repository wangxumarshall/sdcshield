/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_nuke_4src_alu
 * @parblock
 * Variant of mrn_nuke with 4 source loads + data-ALU, to test whether the
 * trigger rate keeps increasing with more source loads (dependency chain
 * depth = 3).
 *
 * mrn_nuke_2src_alu (2 sources) triggers at 14fail/36min;
 * mrn_nuke_3src_alu (3 sources) tested separately. This variant raises to
 * 4 sources (srcA..srcD) with a 3-hop ALU chain (a ^ b ^ c ^ d).
 *
 * Skeleton (store→forwarding load→store, back-to-back) UNCHANGED from
 * mrn_nuke / mrn_nuke_2src_alu. Only the source-load count and ALU arity change.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_NUKE_4SRC_ALU_COUNT 1024

struct MrnNuke4srcAluData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *srcC;
    uint64_t *srcD;
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

static int mrn_nuke_4src_alu_init(struct test *test) {
    auto *data = static_cast<MrnNuke4srcAluData *>(malloc(sizeof(MrnNuke4srcAluData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));
    data->srcC = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));
    data->srcD = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t));
    memset_random(data->srcC, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t));
    memset_random(data->srcD, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_NUKE_4SRC_ALU_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] ^ data->srcB[i] ^ data->srcC[i] ^ data->srcD[i]; break;
            case 1: data->expected[i] = ((data->srcA[i] + data->srcB[i]) ^ data->srcC[i]) + data->srcD[i]; break;
            case 2: data->expected[i] = (data->srcA[i] ^ data->srcB[i]) + (data->srcC[i] ^ data->srcD[i]); break;
            case 3: data->expected[i] = data->srcA[i] ^ (data->srcB[i] + data->srcC[i]) ^ data->srcD[i]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_nuke_4src_alu_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnNuke4srcAluData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_NUKE_4SRC_ALU_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t c = data->srcC[i];
            uint64_t d = data->srcD[i];
            uint64_t res;

            switch (i % 4) {
                case 0: res = a ^ b ^ c ^ d; break;
                case 1: res = ((a + b) ^ c) + d; break;
                case 2: res = (a ^ b) + (c ^ d); break;
                case 3: res = a ^ (b + c) ^ d; break;
            }

            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_NUKE_4SRC_ALU_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("mrn_nuke_4src_alu data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_nuke_4src_alu_cleanup(struct test *test) {
    auto *data = static_cast<MrnNuke4srcAluData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->srcC);
        free(data->srcD);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_nuke_4src_alu, "mrn_nuke variant: 4-source load + data-ALU (dependency chain depth = 3)")
    .test_init    = mrn_nuke_4src_alu_init,
    .test_run     = mrn_nuke_4src_alu_run,
    .test_cleanup = mrn_nuke_4src_alu_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
