/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b arm0102_kreg_mask
 * @parblock
 * SDC trigger-format variant of tests/cpu/vector/kreg7.cpp + kreg8.cpp
 * (SIMD mask expansion: broadcast each mask bit to a full-width element).
 * Originals expand one scalar mask on the stack with printf in the loop.
 * This variant applies the verified core-179 trigger recipe:
 *   - 2 source loads from persistent heap buffers (asm-guaranteed ldr q)
 *   - payload = per-element select-and-broadcast: cmp of the two sources
 *     produces a per-lane mask (like kreg7's masks of various widths),
 *     then that mask bit is EXPANDED to all-ones/all-zeros per lane
 *     (kreg8 semantics), ROTATED across 4 compare variants per i%4:
 *       eq / gt (signed) / hi (unsigned) / tst
 *   - back-to-back vector store -> reload -> store (amplifier)
 * Golden precomputed in init on the same NEON units; memcmp in cold path.
 * Original files untouched.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define ARM0102_KREG_MASK_COUNT 1024   /* uint32x4 elements -> 16KB per buffer */

struct Arm0102KregMaskData {
    uint32x4_t *srcA;
    uint32x4_t *srcB;
    uint32x4_t *expected;
};

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint32x4_t val) {
    vst1q_u32(static_cast<uint32_t *>(addr), val);
}

static inline uint32x4_t load_vec(const void *addr) {
    uint32x4_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int arm0102_kreg_mask_init(struct test *test) {
    auto *data = static_cast<Arm0102KregMaskData *>(malloc(sizeof(Arm0102KregMaskData)));
    data->srcA = static_cast<uint32x4_t *>(aligned_alloc(64, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t)));
    data->srcB = static_cast<uint32x4_t *>(aligned_alloc(64, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t)));
    data->expected = static_cast<uint32x4_t *>(aligned_alloc(64, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t)));

    memset_random(data->srcA, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t));
    memset_random(data->srcB, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < ARM0102_KREG_MASK_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vceqq_s32(vreinterpretq_s32_u32(data->srcA[i]),
                                                  vreinterpretq_s32_u32(data->srcB[i])); break;
            case 1: data->expected[i] = vcgtq_s32(vreinterpretq_s32_u32(data->srcA[i]),
                                                  vreinterpretq_s32_u32(data->srcB[i])); break;
            case 2: data->expected[i] = vceqq_u32(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vtstq_u32(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int arm0102_kreg_mask_run(struct test *test, int cpu) {
    auto *data = static_cast<Arm0102KregMaskData *>(test->data);

    uint32x4_t *temp = static_cast<uint32x4_t *>(aligned_alloc(64, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t)));
    uint32x4_t *dst  = static_cast<uint32x4_t *>(aligned_alloc(64, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < ARM0102_KREG_MASK_COUNT; i++) {
            uint32x4_t a = load_vec(&data->srcA[i]);   /* ldr q — source 1 */
            uint32x4_t b = load_vec(&data->srcB[i]);   /* ldr q — source 2 */
            uint32x4_t res;

            switch (i % 4) {                            /* payload rotation */
                case 0: res = vceqq_s32(vreinterpretq_s32_u32(a), vreinterpretq_s32_u32(b)); break; /* eq */
                case 1: res = vcgtq_s32(vreinterpretq_s32_u32(a), vreinterpretq_s32_u32(b)); break; /* gt */
                case 2: res = vceqq_u32(a, b); break;                                              /* eq-u */
                case 3: res = vtstq_u32(a, b); break;                                              /* tst */
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], load_vec(&temp[i]));     /* back-to-back str->ldr->str */
        }

        if (memcmp(dst, data->expected, ARM0102_KREG_MASK_COUNT * sizeof(uint32x4_t)) != 0) {
            report_fail_msg("arm0102_kreg_mask data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int arm0102_kreg_mask_cleanup(struct test *test) {
    auto *data = static_cast<Arm0102KregMaskData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(arm0102_kreg_mask, "arm-0102 variant of kreg7/8.cpp: 2-src vec load + rotating compare-mask expand (eq/gt/eq-u/tst) + store/reload/store")
    .test_init    = arm0102_kreg_mask_init,
    .test_run     = arm0102_kreg_mask_run,
    .test_cleanup = arm0102_kreg_mask_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
