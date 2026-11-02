/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b locv_a1_mrn_bypass_gap
 * @parblock
 * Family A / A1 of the localization-verify campaign (2026-11-01,
 * docs/CORE179_LOCALIZATION_VERIFY_CAMPAIGN_P1_P6_PLAN_20261101.md §3).
 *
 * One-knob delta of mrn_reloaded_2src_alu: 8 architectural NOPs inserted
 * between the back-to-back reload (ldr temp[i]) and its ONLY consumer
 * (the miscompare).  The str→ldr pair stays adjacent (the 2-cycle
 * forwarding window is untouched); only the reload-result → consumer
 * bypass edge is opened past any load-to-use bypass window.
 *
 * This edge has NEVER been gapped on the mrn skeleton: k4gap (GPS
 * chassis, 2026-09-14) gapped load→ALU (source operands into the ALU),
 * k4bgap gapped ALU→store.  Both were 0-fail.  The reload consumer is
 * the remaining ungapped edge on the strongest portable trigger.
 *
 * Pre-registered readouts:
 *   - 0 fail → the reloaded value is CORRECT in the register file; the
 *     corruption manifests only in the bypass delivery to the immediate
 *     consumer → operand-capture/bypass edge implicated (working
 *     hypothesis strengthened; complements k4gap/k4bgap on this skeleton);
 *   - triggers like A0 → the reload data itself is wrong by the time it
 *     is latched → corruption is upstream in the SQ→load-result path
 *     (merge/forwarding zone back on the table).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define LOCV_A1_COUNT 1024

struct LocvA1Data {
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

// 8 architectural NOPs: scheduling barrier between the reload and its
// consumer (same idiom/far extreme as k4gap/k4bgap, 2026-09-14).
// "nop" is valid on both x86-64 and aarch64 GNU asm.
static inline void bypass_gap8(void) {
    __asm__ volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop");
}

static int locv_a1_init(struct test *test) {
    auto *data = static_cast<LocvA1Data *>(malloc(sizeof(LocvA1Data)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A1_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A1_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A1_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, LOCV_A1_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, LOCV_A1_COUNT * sizeof(uint64_t));

    for (int i = 0; i < LOCV_A1_COUNT; i++) {
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

static int locv_a1_run(struct test *test, int cpu) {
    auto *data = static_cast<LocvA1Data *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, LOCV_A1_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 8) {
        // Identical to mrn_reloaded_2src_alu except: 8-nop gap opens the
        // reload-result → consumer bypass.  str→ldr stays back-to-back.
        for (int i = 0; i < LOCV_A1_COUNT; i++) {
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
            bypass_gap8();
            if (r != data->expected[i]) {
                report_fail_msg("locv_a1_mrn_bypass_gap qword miscompare at %d: "
                                "expected 0x%llx got 0x%llx", i,
                                static_cast<unsigned long long>(data->expected[i]),
                                static_cast<unsigned long long>(r));
            }
        }
    }

    free(temp);
    return EXIT_SUCCESS;
}

static int locv_a1_cleanup(struct test *test) {
    auto *data = static_cast<LocvA1Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(locv_a1_mrn_bypass_gap, "locv A1: 8-nop gap on reload-consumer bypass edge (str/ldr adjacency kept)")
    .test_init    = locv_a1_init,
    .test_run     = locv_a1_run,
    .test_cleanup = locv_a1_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
