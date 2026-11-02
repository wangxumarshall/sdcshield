/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b locv_a2_mrn_split_consumer
 * @parblock
 * Family A / A2 of the localization-verify campaign (2026-11-01,
 * docs/CORE179_LOCALIZATION_VERIFY_CAMPAIGN_P1_P6_PLAN_20261101.md §3).
 *
 * One-knob delta of mrn_reloaded_2src_alu: the two source loads no
 * longer meet at ONE ALU.  Each load feeds its own single-operand ALU
 * (t1 = a | 0x1111, t2 = b & 0x2222 — orr/and chosen for the highest
 * trigger bias per the k7p op-bias result), and the two ALU results are
 * combined later (res = t1 op t2 with the same i%4 rotation).  The
 * stored value is still an ALU-processed 2-source result; str→ldr stays
 * back-to-back; every load still has a back-to-back ALU consumer.
 *
 * Discriminates the "2src" constraint (2026-09-02 gradient, 1src=0 vs
 * 2src=14): is the operative feature (i) TWO load results bypassed into
 * the SAME ALU simultaneously (dual-operand capture), or (ii) merely
 * two loads present in the window (count/port pressure)?
 *
 * Pre-registered readouts:
 *   - rate holds vs A0 → load count / port pressure is the operative
 *     feature; dual-operand capture is NOT required;
 *   - rate collapses (0 or near-0) → dual-operand bypass simultaneity
 *     is required → the operand-capture point where two bypassed load
 *     results meet is the fault's manifestation site.
 *
 * Confound declared: +2 ALU ops per element vs baseline (timing
 * dilution risk, cf. 3src/4src gradient decay).  Readout is
 * collapse-vs-hold, not fine-grained rate comparison.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define LOCV_A2_COUNT 1024

struct LocvA2Data {
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

static int locv_a2_init(struct test *test) {
    auto *data = static_cast<LocvA2Data *>(malloc(sizeof(LocvA2Data)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A2_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A2_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A2_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, LOCV_A2_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, LOCV_A2_COUNT * sizeof(uint64_t));

    for (int i = 0; i < LOCV_A2_COUNT; i++) {
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

static int locv_a2_run(struct test *test, int cpu) {
    auto *data = static_cast<LocvA2Data *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A2_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 8) {
        // Split consumers: each load feeds its own single-operand ALU;
        // no ALU consumes two bypassed load results simultaneously.
        for (int i = 0; i < LOCV_A2_COUNT; i++) {
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
            if (r != data->expected[i]) {
                report_fail_msg("locv_a2_mrn_split_consumer qword miscompare at %d: "
                                "expected 0x%llx got 0x%llx", i,
                                static_cast<unsigned long long>(data->expected[i]),
                                static_cast<unsigned long long>(r));
            }
        }
    }

    free(temp);
    return EXIT_SUCCESS;
}

static int locv_a2_cleanup(struct test *test) {
    auto *data = static_cast<LocvA2Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(locv_a2_mrn_split_consumer, "locv A2: split consumers — no ALU takes two bypassed load results")
    .test_init    = locv_a2_init,
    .test_run     = locv_a2_run,
    .test_cleanup = locv_a2_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
