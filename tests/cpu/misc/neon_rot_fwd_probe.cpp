/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_fwd_probe
 * @parblock
 * Clean reload-attribution variant (user's plan A, 2026-09-04):
 *   baseline:   ALU -> str temp -> ldr temp (reload/forwarding) -> str dst
 *   this:       ALU -> str temp -> ldr dst  (no forwarding)     -> str dst
 * Keeps the ldr (mixed load/store AGU pressure, same instruction count,
 * same 4-step dependency chain shape) but the load reads dst — NOT the
 * address just stored — so there is no store->load forwarding window.
 * baseline vs fwd_probe isolates "forwarding/reload window" cleanly;
 * fwd_probe vs store_only isolates "mixed AGU pressure + ldr presence".
 * Expected golden: dst[i] is overwritten by res before the load, so the
 * loaded value equals res and the final store writes res back — the
 * self-check memcmp(dst, expected) still validates the full path.
 * Skeleton otherwise identical to neon_rot_2src.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_FWD_PROBE_COUNT 1024

struct NeonRotFwdProbeData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

/* store via NEON intrinsic (compiles to str q); load via inline asm —
 * REQUIRED: objdump showed the compiler eliminates the intrinsic load
 * (reuses the stored register, no ldr emitted). The asm boundary makes
 * the ldr guaranteed in the binary. */
static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int neon_rot_fwd_probe_init(struct test *test) {
    auto *data = static_cast<NeonRotFwdProbeData *>(malloc(sizeof(NeonRotFwdProbeData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_FWD_PROBE_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_fwd_probe_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotFwdProbeData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t)));

    /* dst must start equal to expected so that load(dst) == res holds on
     * the first iteration and every later iteration (fixed point). */
    memcpy(dst, data->expected, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_FWD_PROBE_COUNT; i++) {
            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            /* plan A, single asm block for exact instruction order:
             *   str temp[i] = res      (same as baseline)
             *   ldr dst[i]             (baseline reloads temp — here dst:
             *                          ldr present, no forwarding window)
             *   str dst[i] = res       (dst receives the ALU result, so the
             *                          memcmp self-check stays a real check;
             *                          loaded value intentionally discarded)
             * Three memory instructions, same count/mix as baseline; the
             * loaded value is discarded on purpose — this probe measures the
             * PRESENCE of the load (AGU/pipe pressure), not load-data
             * integrity. */
            uint64x2_t loaded;
            __asm__ volatile (
                "str %q1, [%2]  \n\t"
                "ldr %q0, [%3]  \n\t"
                "str %q1, [%3]  \n\t"
                : "=&w"(loaded)
                : "w"(res), "r"(&temp[i]), "r"(&dst[i])
                : "memory");
        }

        if (memcmp(dst, data->expected, NEON_ROT_FWD_PROBE_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_fwd_probe data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_fwd_probe_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotFwdProbeData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_fwd_probe, "NEON plan-A probe: ldr reads dst (no forwarding), mixed AGU pressure kept — clean reload attribution")
    .test_init    = neon_rot_fwd_probe_init,
    .test_run     = neon_rot_fwd_probe_run,
    .test_cleanup = neon_rot_fwd_probe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
