/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b locv_a3_mrn_split_gap
 * @parblock
 * Family A / A3 of the localization-verify campaign (2026-11-01,
 * docs/CORE179_LOCALIZATION_VERIFY_CAMPAIGN_P1_P6_PLAN_20261101.md §3).
 *
 * A1 + A2 combined: split consumers (no ALU takes two bypassed load
 * results) AND an 8-nop gap opening the reload-result → consumer
 * bypass.  Completes the 2×2 orthogonal table against A0 (neither) ,
 * A1 (gap only), A2 (split only).
 *
 * Pre-registered readout: expected to collapse if EITHER single knob
 * collapses (the knobs act on different edges; no interaction term is
 * predicted by the working hypothesis).  Any outcome other than
 * collapse (i.e. A3 triggers while A1 or A2 is 0) would indicate the
 * two knobs interact — itself informative for the merge-zone picture.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define LOCV_A3_COUNT 1024

struct LocvA3Data {
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

static inline void bypass_gap8(void) {
    __asm__ volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop");
}

static int locv_a3_init(struct test *test) {
    auto *data = static_cast<LocvA3Data *>(malloc(sizeof(LocvA3Data)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A3_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A3_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A3_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, LOCV_A3_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, LOCV_A3_COUNT * sizeof(uint64_t));

    for (int i = 0; i < LOCV_A3_COUNT; i++) {
        uint64_t t1 = data->srcA[i] | 0x1111111111111111ULL;
        uint64_t t2 = data->srcB[i] & 0x2222222222222222ULL;
        switch (i % 4) {
            case 0: data->expected[i] = t1 + t2; break;
            case 1: data->expected[i] = t1 - t2; break;
            case 2: data->expected[i] = t1 ^ t2; break;
            case 3: data->expected[i] = t1 & t2; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int locv_a3_run(struct test *test, int cpu) {
    auto *data = static_cast<LocvA3Data *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A3_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 8) {
        // A1+A2: split consumers + 8-nop gap on reload consumer.
        for (int i = 0; i < LOCV_A3_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t t1 = a | 0x1111111111111111ULL;
            uint64_t t2 = b & 0x2222222222222222ULL;
            uint64_t res;

            switch (i % 4) {
                case 0: res = t1 + t2; break;
                case 1: res = t1 - t2; break;
                case 2: res = t1 ^ t2; break;
                case 3: res = t1 & t2; break;
            }

            store_qword(&temp[i], res);
            uint64_t r = load_qword(&temp[i]);
            bypass_gap8();
            if (r != data->expected[i]) {
                report_fail_msg("locv_a3_mrn_split_gap qword miscompare at %d: "
                                "expected 0x%llx got 0x%llx", i,
                                static_cast<unsigned long long>(data->expected[i]),
                                static_cast<unsigned long long>(r));
            }
        }
    }

    free(temp);
    return EXIT_SUCCESS;
}

static int locv_a3_cleanup(struct test *test) {
    auto *data = static_cast<LocvA3Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(locv_a3_mrn_split_gap, "locv A3: split consumers + 8-nop reload-consumer gap (A1+A2)")
    .test_init    = locv_a3_init,
    .test_run     = locv_a3_run,
    .test_cleanup = locv_a3_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
