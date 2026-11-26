/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_dualcmp_l2_256k
 * @parblock
 * L2-resident tier 128KB of neon_rot_ldr_at_top_rowmajor_dualcmp
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * integer read-side recipe (scratch dead asm ldr + two asm source
 * loads + rotating add/eor/and/orr + back-to-back str/str pair) is the
 * object under test, and the parent's dual-store cold-path classifier
 * is inherited verbatim (tests/cpu/misc/
 * neon_rot_ldr_at_top_rowmajor_dualcmp.cpp): BOTH store legs (temp and
 * dst) are checked against the golden plus their mutual consistency —
 * A_only (2nd store leg suspect) / B_only (1st store leg suspect) /
 * AB_div (divergent stores) / AB_same (same wrong value, upstream of
 * the fork) — with a five-buffer binary dump per fail.
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on BOTH compared destination blocks
 * — dualcmp checks temp and dst both).  The scalar golden covers the
 * FULL window at init; the sweep-end memcmps narrow to the slice
 * [off, off+LEN).
 *
 * Deltas from the parent (deliberate, geometry-side): the classify scan
 * and the binary dump cover the WINDOW SLICE only (five buffers x LEN
 * elements from off — 5 x 256KB = 1.25MB per fail file at this tier);
 * the failure message and the dump summary line carry off.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64x2_t = 256KB (inside the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 256KB = 1.25MB —
 * EXCEEDS L2 and self-evicts (the five-buffer chassis geometry, disclosed
 * honestly; the tier axis is the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRDC256_LEN 16384                  /* uint64x2_t elements -> 256KB block */
#define NRDC256_WIN (2 * NRDC256_LEN)     /* 2x-block window (memcpy_l2 method) */
#define NRDC256_DUMP_DIR "movbe_log/dualcmp_rowmajor_l2"

struct NRDC256Data {
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

static int dualcmp_l2_256k_init(struct test *test) {
    auto *data = static_cast<NRDC256Data *>(malloc(sizeof(NRDC256Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NRDC256_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NRDC256_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRDC256_WIN * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NRDC256_WIN * sizeof(uint64x2_t)));

    memset_random(data->srcA, NRDC256_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRDC256_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRDC256_WIN * sizeof(uint64x2_t));

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window — pure C integer
     * ops on the scalar pipeline, never the NEON units under test. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRDC256_WIN; e++) {
            switch ((e / 2) % 4) {   /* op phase: vector index % 4 */
                case 0: ex[e] = sa[e] + sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & sb[e]; break;
                case 3: ex[e] = sa[e] | sb[e]; break;
            }
        }
    }

    mkdir("movbe_log", 0755);
    mkdir(NRDC256_DUMP_DIR, 0755);

    test->data = data;
    return EXIT_SUCCESS;
}

/* Cold-path classifier (inherited from the parent, slice-scoped):
 *   A_only : dst wrong, temp right  (2nd store leg suspect)
 *   B_only : temp wrong, dst right  (1st store leg suspect)
 *   AB_div : temp != dst (divergent stores)
 *   AB_same: temp == dst != golden  (upstream of the fork) */
struct DualCmpL2Stats {
    unsigned long a_only;
    unsigned long b_only;
    unsigned long ab_div;
    unsigned long ab_same;
    int first_bad;
};

static void dualcmp_l2_256k_classify(struct NRDC256Data *data, uint64x2_t *temp,
                                     uint64x2_t *dst, int off, struct DualCmpL2Stats *st) {
    st->a_only = st->b_only = st->ab_div = st->ab_same = 0;
    st->first_bad = -1;
    for (int k = 0; k < NRDC256_LEN; k++) {
        const int i = off + k;
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

static int dualcmp_l2_256k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRDC256Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRDC256_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRDC256_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot4 slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRDC256_LEN / 4)));

        /* poison BOTH compared destination blocks (dualcmp checks temp
         * and dst): a lost write stays 0x5a5a.. and fails its compare */
        memset(temp + off, 0x5A, NRDC256_LEN * sizeof(uint64x2_t));
        memset(dst + off, 0x5A, NRDC256_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRDC256_LEN; n++) {
            const int j = ascending ? n : (NRDC256_LEN - 1 - n);
            const int i = off + j;

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

        /* ---- COLD check path (gated, hot loop untouched), slice-scoped ---- */
        const bool dst_bad  = memcmp(dst + off,  data->expected + off, NRDC256_LEN * sizeof(uint64x2_t)) != 0;
        const bool temp_bad = memcmp(temp + off, data->expected + off, NRDC256_LEN * sizeof(uint64x2_t)) != 0;
        if (dst_bad || temp_bad) {
            struct DualCmpL2Stats st;
            dualcmp_l2_256k_classify(data, temp, dst, off, &st);

            char path[256];
            snprintf(path, sizeof(path), "%s/dualcmp_l2_256k_fail_cpu%d_sweep%lu_off%d.bin", NRDC256_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst + off, sizeof(uint64x2_t), NRDC256_LEN, f);
                fwrite(data->expected + off, sizeof(uint64x2_t), NRDC256_LEN, f);
                fwrite(temp + off, sizeof(uint64x2_t), NRDC256_LEN, f);
                fwrite(data->srcA + off, sizeof(uint64x2_t), NRDC256_LEN, f);
                fwrite(data->srcB + off, sizeof(uint64x2_t), NRDC256_LEN, f);
                fprintf(f, "dualcmp_l2_256k sweep=%lu off=%d a_only=%lu b_only=%lu ab_div=%lu ab_same=%lu first_bad=%d\n",
                        static_cast<unsigned long>(sweep), off, st.a_only, st.b_only,
                        st.ab_div, st.ab_same, st.first_bad);
                fclose(f);
            }

            char msg[300];
            snprintf(msg, sizeof(msg),
                     "dualcmp_l2_256k miscompare: off=%d dst_bad=%d temp_bad=%d | 2nd-str-only(dst-wrong,a)=%lu 1st-str-only(temp-wrong,b)=%lu divergent=%lu same-wrong(upstream)=%lu first_bad=%d",
                     off, dst_bad ? 1 : 0, temp_bad ? 1 : 0, st.a_only, st.b_only, st.ab_div, st.ab_same, st.first_bad);
            report_fail_msg("%s", msg);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int dualcmp_l2_256k_cleanup(struct test *test) {
    auto *data = static_cast<NRDC256Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_dualcmp_l2_256k, "L2-resident tier 128KB of dualcmp: integer read-side chassis preserved (scratch dead ldr + 2-src asm loads + rot ALU + bb str/str), sliding-window geometry transplanted; dual-store cold-path classifier (A_only/B_only/divergent/same-wrong) + five-buffer slice dump, both store legs poisoned (ARM64)")
    .test_init    = dualcmp_l2_256k_init,
    .test_run     = dualcmp_l2_256k_run,
    .test_cleanup = dualcmp_l2_256k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
