/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k
 * @parblock
 * L2-resident tier 256KB of neon_rot_ldr_at_top_rowmajor_fp64_dump
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * FP64 read-side recipe (scratch dead asm ldr + two asm source loads +
 * fadd/fsub/fmul/fmax rotation + back-to-back str/str pair) is the
 * object under test; the identity-copy trio in this directory covers
 * the identity axis, this family the rot4 axis (chassis x block-size
 * discrimination grid).  The cold-path dump discipline is inherited
 * verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_fp64_dump.cpp): dump
 * lives ONLY in the post-sweep failure branch, kept out-of-line so its
 * stack frame never enters the hot function's codegen.
 *
 * KNOWN BOUNDARY inherited unchanged: the golden is the same FP64
 * rotating composition computed on the same NEON units at init (a
 * persistent NEON datapath fault is baked into both — the parent's
 * disclosed boundary; the integer rowmajor parent carries the scalar
 * golden instead).
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on the compared destination block).
 * The golden covers the FULL window at init so every slid slice has
 * its golden; the sweep-end memcmp narrows to the slice [off, off+LEN).
 *
 * Dump deltas from the parent (deliberate, geometry-side): the binary
 * dump and the bad-scan cover the WINDOW SLICE only (six buffers x LEN
 * elements from off — 6 x 256KB = 1.5MB per fail file at this tier);
 * the failure message carries off alongside sweep/cpu/seq.  The
 * parent's whole-buffer hot-loop byte-identity discipline cannot carry
 * to a sliding-window geometry — the hot RECIPE (loads/rot/stores/
 * alternating sweeps) is preserved item-for-item instead, disclosed.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64x2_t = 256KB (inside
 * the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 256KB = 1.25MB —
 * EXCEEDS L2 and self-evicts (the five-buffer chassis geometry,
 * disclosed honestly; the tier axis is the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRFD256_LEN 16384                 /* uint64x2_t elements -> 256KB block */
#define NRFD256_WIN (2 * NRFD256_LEN)     /* 2x-block window (memcpy_l2 method) */
#define NRFD256_DUMP_DIR "movbe_log/fp64_rowmajor_l2"

struct NRFD256Data {
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

/* FP64 rotation, slot order mirrors the integer parent: 0=fadd 1=fsub
 * 2=fmul 3=fmax. Reinterpretations are compile-time no-ops. */
static inline uint64x2_t fp64_rot(int slot, uint64x2_t a, uint64x2_t b) {
    const float64x2_t fa = vreinterpretq_f64_u64(a);
    const float64x2_t fb = vreinterpretq_f64_u64(b);
    float64x2_t fres;
    switch (slot) {
        case 0: fres = vaddq_f64(fa, fb); break;
        case 1: fres = vsubq_f64(fa, fb); break;
        case 2: fres = vmulq_f64(fa, fb); break;
        default: fres = vmaxq_f64(fa, fb); break;
    }
    return vreinterpretq_u64_f64(fres);
}

#endif

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k_init(struct test *test) {
    auto *data = static_cast<NRFD256Data *>(malloc(sizeof(NRFD256Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NRFD256_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NRFD256_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRFD256_WIN * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NRFD256_WIN * sizeof(uint64x2_t)));

    memset_random(data->srcA, NRFD256_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRFD256_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRFD256_WIN * sizeof(uint64x2_t));

    /* golden over the FULL window: same FP64 rotating composition,
     * computed on the same NEON units (parent's disclosed boundary) */
    for (int i = 0; i < NRFD256_WIN; i++) {
        data->expected[i] = fp64_rot(i % 4, data->srcA[i], data->srcB[i]);
    }

    mkdir("movbe_log", 0755);        /* mkdir creates only the last component */
    mkdir(NRFD256_DUMP_DIR, 0755);   /* dualcmp convention; idempotent */

    test->data = data;
    return EXIT_SUCCESS;
}

/* Cold path ONLY - kept out-of-line so its stack frame and register
 * pressure never enter the hot function's codegen (parent discipline).
 * Covers the WINDOW SLICE [off, off+LEN) only. */
__attribute__((noinline)) static void fp64_dump_report_l2(
        struct NRFD256Data *data, uint64x2_t *temp,
        uint64x2_t *dst, uint64_t sweep, int off) {
    /* cpu is NOT a parameter: fetch it cold - the actual core residency
     * at report time (parent convention). */
    const int cpu = sched_getcpu();
    static std::atomic<int> g_seq; /* cold-only: unique filenames across thread restarts */
    const int seq = g_seq.fetch_add(1);

    int bad[8];
    int nbad = 0;
    int total_bad = 0;
    for (int k = 0; k < NRFD256_LEN; k++) {
        const int i = off + k;
        if (memcmp(&dst[i], &data->expected[i], sizeof(uint64x2_t)) != 0) {
            total_bad++;
            if (nbad < 8) bad[nbad++] = i;
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/fp64dump_l2_256k_fail_cpu%d_sweep%lu_off%d_seq%d.bin",
             NRFD256_DUMP_DIR, cpu, static_cast<unsigned long>(sweep), off, seq);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data->srcA + off, sizeof(uint64x2_t), NRFD256_LEN, f);
        fwrite(data->srcB + off, sizeof(uint64x2_t), NRFD256_LEN, f);
        fwrite(data->scratch + off, sizeof(uint64x2_t), NRFD256_LEN, f);
        fwrite(data->expected + off, sizeof(uint64x2_t), NRFD256_LEN, f);
        fwrite(temp + off, sizeof(uint64x2_t), NRFD256_LEN, f);
        fwrite(dst + off, sizeof(uint64x2_t), NRFD256_LEN, f);
        fprintf(f, "fp64dump_l2_256k sweep=%lu cpu=%d off=%d seq=%d nbad=%d bad=",
                static_cast<unsigned long>(sweep), cpu, off, seq, total_bad);
        for (int k = 0; k < nbad; k++) fprintf(f, "%s%d", k ? "," : "", bad[k]);
        fprintf(f, "\n");
        fclose(f);
    }

    /* compact per-element summary in the failure message itself:
     * A/B = feeding sources, T = 1st store, G = dst (got), E = golden,
     * X = G^E per lane. Two elements in the message; everything else
     * in the binary dump. */
    char msg[900];
    int len = snprintf(msg, sizeof(msg),
                       "fp64_dump_l2_256k data miscompare: cpu=%d sweep=%lu off=%d nbad=%d bad=",
                       cpu, static_cast<unsigned long>(sweep), off, total_bad);
    for (int k = 0; k < nbad && k < 2 && len > 0 && len < (int)sizeof(msg) - 260; k++) {
        const int i = bad[k];
        const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
        const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
        const uint64_t *T = reinterpret_cast<const uint64_t *>(&temp[i]);
        const uint64_t *G = reinterpret_cast<const uint64_t *>(&dst[i]);
        const uint64_t *E = reinterpret_cast<const uint64_t *>(&data->expected[i]);
        len += snprintf(msg + len, sizeof(msg) - (size_t)len,
                        " | #%d s=%d A=%016lx,%016lx B=%016lx,%016lx T=%016lx,%016lx G=%016lx,%016lx E=%016lx,%016lx X=%016lx,%016lx",
                        i, i % 4, A[0], A[1], B[0], B[1], T[0], T[1], G[0], G[1], E[0], E[1],
                        G[0] ^ E[0], G[1] ^ E[1]);
    }
    report_fail_msg("%s", msg);
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRFD256Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRFD256_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRFD256_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRFD256_LEN / 4)));

        /* poison the compared destination block: a lost write stays
         * 0x5a5a.. and fails the slice compare */
        memset(dst + off, 0x5A, NRFD256_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRFD256_LEN; n++) {
            const int j = ascending ? n : (NRFD256_LEN - 1 - n);
            const int i = off + j;

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res = fp64_rot(i % 4, a, b);

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        /* cold path: slice compare — only the window is promised
         * written this sweep; all dump/report work out-of-line (parent
         * discipline) */
        if (memcmp(dst + off, data->expected + off, NRFD256_LEN * sizeof(uint64x2_t)) != 0) {
            fp64_dump_report_l2(data, temp, dst, sweep, off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k_cleanup(struct test *test) {
    auto *data = static_cast<NRFD256Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k, "L2-resident tier 256KB of fp64_dump: FP64 read-side chassis preserved (scratch dead ldr + 2-src asm loads + fadd/fsub/fmul/fmax rotation + str/str pair), sliding-window geometry transplanted; cold-path slice dump for bit-position and field-graft analysis (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
