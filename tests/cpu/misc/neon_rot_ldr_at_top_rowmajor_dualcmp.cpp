/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_dualcmp
 * @parblock
 * Dual-store comparison cell of the CORE179 readpath campaign (2026-09-16).
 *
 * The baseline (neon_rot_ldr_at_top_rowmajor) writes the SAME res register
 * with two back-to-back stores per element:
 *
 *     store_vec(&temp[i], res);   // 1st str
 *     store_vec(&dst[i],  res);   // 2nd str
 *
 * but ALL historical checks (memcmp + the K-cells five-buffer dump) only
 * ever inspected dst (the 2nd store's target).  temp (the 1st store's
 * target) was never verified, never dumped — a genuine blind spot.
 *
 * This variant keeps the hot loop BYTE-IDENTICAL to the baseline (verified
 * by objdump) and extends ONLY the cold check path:
 *
 *   - check A (baseline semantics): dst vs golden           -> SDC fail
 *   - check B (NEW):             temp vs golden           -> SDC fail
 *   - check C (NEW):             temp vs dst              -> divergence of
 *     the two stores of the SAME res value
 *
 * Pre-registered readouts (per outer sweep, logged as fail counters):
 *   - A-only  : dst wrong, temp right (error on the 2nd store's leg)
 *   - B-only  : temp wrong, dst right (error on the 1st store's leg)
 *   - A==B==C0 (same element set, temp==dst but != golden):
 *              error arose UPSTREAM of the store fork (load / ALU /
 *               bypass) — both stores recorded the same wrong value
 *   - C!=0 (temp != dst on some element):
 *              the two store legs DIVERGED — the defect sits on (or
 *               after) the fork, i.e. on one store's bypass/store path;
 *               the split of C-cases between B-only and A-only tells
 *               WHICH store leg is more error-prone
 *
 * Per-class counters are reported in the fail message; a per-sweep hex
 * summary of (first differing element, temp/dst/golden words) is dumped
 * to movbe_log/gps_rowmajor/ in the family dump directory (cold path
 * only, gated by the memcmp checks, so the hot loop is untouched).
 *
 * No LCG seed fixing; auto seed only.  Full-core 192 (incl. 179) runs.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define DUALCMP_COUNT 1024
#define DUALCMP_DUMP_DIR "movbe_log/gps_rowmajor"

struct DualCmpData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int dualcmp_init(struct test *test) {
    auto *data = static_cast<DualCmpData *>(malloc(sizeof(DualCmpData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, DUALCMP_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, DUALCMP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, DUALCMP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, DUALCMP_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, DUALCMP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, DUALCMP_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, DUALCMP_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < DUALCMP_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    mkdir(DUALCMP_DUMP_DIR, 0755);

    test->data = data;
    return EXIT_SUCCESS;
}

/* Cold-path classifier: returns per-sweep class counts via out-params.
 * Classes:
 *   A_only : dst wrong, temp right  (2nd store leg suspect)
 *   B_only : temp wrong, dst right  (1st store leg suspect)
 *   AB_div : temp != dst (divergent stores; counted per element, and
 *            each such element also lands in A_only or B_only by which
 *            one matches golden)
 *   AB_same: temp == dst != golden  (upstream of the fork)
 */
struct DualCmpStats {
    unsigned long a_only;    /* dst wrong, temp == golden                */
    unsigned long b_only;    /* temp wrong, dst == golden                */
    unsigned long ab_div;    /* temp != dst (either vs golden)           */
    unsigned long ab_same;   /* temp == dst != golden (upstream error)   */
    int first_bad;           /* first offending element index, -1 if none */
};

static void dualcmp_classify(struct DualCmpData *data, uint64x2_t *temp,
                             uint64x2_t *dst, struct DualCmpStats *st) {
    st->a_only = st->b_only = st->ab_div = st->ab_same = 0;
    st->first_bad = -1;
    for (int i = 0; i < DUALCMP_COUNT; i++) {
        const uint64x2_t t_eq = vceqq_u64(temp[i], data->expected[i]);
        const uint64x2_t d_eq = vceqq_u64(dst[i],  data->expected[i]);
        const bool t_ok = (t_eq[0] == UINT64_MAX) && (t_eq[1] == UINT64_MAX);
        const bool d_ok = (d_eq[0] == UINT64_MAX) && (d_eq[1] == UINT64_MAX);
        const bool td_same = (temp[i][0] == dst[i][0]) && (temp[i][1] == dst[i][1]);
        if (t_ok && d_ok) continue;
        if (st->first_bad < 0) st->first_bad = i;
        if (!td_same) {
            st->ab_div++;
            if (t_ok)      st->a_only++;   /* dst diverged & wrong (2nd store leg) */
            else if (d_ok) st->b_only++;   /* temp diverged & wrong (1st store leg) */
            /* both wrong AND different: rare; count in div only */
        } else {
            st->ab_same++;                 /* same wrong value in both */
        }
    }
}

static int dualcmp_run(struct test *test, int cpu) {
    auto *data = static_cast<DualCmpData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, DUALCMP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, DUALCMP_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < DUALCMP_COUNT; n++) {
            const int i = ascending ? n : (DUALCMP_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        /* ---- COLD check path (gated, hot loop untouched) ---- */
        const bool dst_bad  = memcmp(dst,  data->expected, DUALCMP_COUNT * sizeof(uint64x2_t)) != 0;
        const bool temp_bad = memcmp(temp, data->expected, DUALCMP_COUNT * sizeof(uint64x2_t)) != 0;
        if (dst_bad || temp_bad) {
            struct DualCmpStats st;
            dualcmp_classify(data, temp, dst, &st);

            char path[256];
            snprintf(path, sizeof(path), "%s/dualcmp_fail_cpu%d_sweep%lu.bin", DUALCMP_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst, sizeof(uint64x2_t), DUALCMP_COUNT, f);
                fwrite(data->expected, sizeof(uint64x2_t), DUALCMP_COUNT, f);
                fwrite(temp, sizeof(uint64x2_t), DUALCMP_COUNT, f);
                fwrite(data->srcA, sizeof(uint64x2_t), DUALCMP_COUNT, f);
                fwrite(data->srcB, sizeof(uint64x2_t), DUALCMP_COUNT, f);
                fprintf(f, "dualcmp sweep=%lu a_only=%lu b_only=%lu ab_div=%lu ab_same=%lu first_bad=%d\n",
                        static_cast<unsigned long>(sweep), st.a_only, st.b_only,
                        st.ab_div, st.ab_same, st.first_bad);
                fclose(f);
            }

            char msg[256];
            snprintf(msg, sizeof(msg),
                     "dualcmp miscompare: dst_bad=%d temp_bad=%d | 2nd-str-only(dst-wrong,a)=%lu 1st-str-only(temp-wrong,b)=%lu divergent=%lu same-wrong(upstream)=%lu first_bad=%d",
                     dst_bad ? 1 : 0, temp_bad ? 1 : 0, st.a_only, st.b_only, st.ab_div, st.ab_same, st.first_bad);
            report_fail_msg("%s", msg);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int dualcmp_cleanup(struct test *test) {
    auto *data = static_cast<DualCmpData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_dualcmp, "Dual-store comparison: baseline skeleton, checks BOTH str targets (temp & dst) + their mutual consistency — which store leg carries the SDC, or is it upstream of the fork")
    .test_init    = dualcmp_init,
    .test_run     = dualcmp_run,
    .test_cleanup = dualcmp_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
