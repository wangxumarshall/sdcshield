/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_reloaded_2src_alu
 * @parblock
 * Variant of mrn_reloaded that adds the 2-source-load + data-ALU dependency
 * chain to the qword (64-bit) segment. mrn_reloaded original does byte/dword/
 * qword store→load back-to-back with raw values → 0-fail on core 179.
 *
 * This variant: only the qword segment, 2 sources (srcA+srcB) + ALU result
 * (a op b), store→load back-to-back forwarding. Byte/dword segments removed
 * to focus on the 64-bit granularity (matching mrn_rmw / mrn_nuke_2src_alu).
 *
 * Tests whether "2src+ALU" triggers on the mrn_reloaded skeleton (which, unlike
 * mrn_pairs, already has back-to-back timing — only the ALU+2source is added).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_RELOADED_2SRC_ALU_COUNT 1024

struct MrnReloaded2srcAluData {
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

static int mrn_reloaded_2src_alu_init(struct test *test) {
    auto *data = static_cast<MrnReloaded2srcAluData *>(malloc(sizeof(MrnReloaded2srcAluData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RELOADED_2SRC_ALU_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RELOADED_2SRC_ALU_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_RELOADED_2SRC_ALU_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_RELOADED_2SRC_ALU_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_RELOADED_2SRC_ALU_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_RELOADED_2SRC_ALU_COUNT; i++) {
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

static int mrn_reloaded_2src_alu_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnReloaded2srcAluData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_RELOADED_2SRC_ALU_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 8) {
        // qword segment only: 2-source + ALU + back-to-back store→load.
        // Skeleton matches mrn_reloaded's qword loop (store→load back-to-back),
        // only the stored value changes from raw src to ALU(srcA op srcB).
        for (int i = 0; i < MRN_RELOADED_2SRC_ALU_COUNT; i++) {
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
            uint64_t r = load_qword(&temp[i]);
            if (r != data->expected[i]) {
                report_fail_msg("mrn_reloaded_2src_alu qword miscompare at %d: "
                                "expected 0x%llx got 0x%llx", i,
                                static_cast<unsigned long long>(data->expected[i]),
                                static_cast<unsigned long long>(r));
            }
        }
    }

    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_reloaded_2src_alu_cleanup(struct test *test) {
    auto *data = static_cast<MrnReloaded2srcAluData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_reloaded_2src_alu, "mrn_reloaded variant: 2-source load + data-ALU on qword segment")
    .test_init    = mrn_reloaded_2src_alu_init,
    .test_run     = mrn_reloaded_2src_alu_run,
    .test_cleanup = mrn_reloaded_2src_alu_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
