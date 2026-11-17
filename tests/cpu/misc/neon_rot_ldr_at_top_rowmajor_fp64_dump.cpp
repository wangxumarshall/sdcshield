/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_fp64_dump
 * @parblock
 * Cold-path dump variant of neon_rot_ldr_at_top_rowmajor_fp64 (2026-11-06).
 * (The fp64 parent was a 2026-11-06 campaign working-tree file and was
 * never carried in-tree; this variant is self-contained — test_init
 * regenerates its own golden with scalar (d-register) FP64 ops instead of
 * the vector (v-register) units under test, so the byte-identity rationale
 * below is the design record, not a build-time dependency.)
 *
 * Purpose: bit-position statistics for the FP64 rotation trigger. The
 * parent only reports a generic "data miscompare" (manual memcmp +
 * report_fail_msg, no values), so no got/expected words were ever
 * captured for the FP64 consumer. Open question this answers: do the
 * output mismatches concentrate in the mantissa (like eigen float32
 * 85% / double64 93%) or do they follow the rowmajor family's 16B
 * field-misgrafting mode - which, mapped through fadd/fsub/fmul/fmax
 * on random bit patterns, is NOT expected to stay confined to
 * low-order bits?
 *
 * Design (movbe_dump / mrn_rmw_dump / eigen_dump / dualcmp family
 * pattern, verified repeatedly not to perturb triggering):
 *   - hot loop BYTE-IDENTICAL to the fp64 parent: same TEST_LOOP count,
 *     same scratch-ldr-at-top + 2-src asm loads, same fp64_rot slot
 *     order, same back-to-back str/str pair, same sweep order;
 *   - dump lives ONLY in the cold post-sweep failure branch;
 *   - report_fail_msg is kept, with "miscompare" in the text so the
 *     campaign scripts' grep counting keeps working;
 *   - on failure: binary full-context dump + a compact per-element
 *     summary in the failure message itself.
 *
 * Binary layout per fail file (six buffers of COUNT x uint64x2_t,
 * then one text line):
 *   srcA | srcB | scratch | expected(golden) | temp(1st store) |
 *   dst(2nd store, the compared buffer) |
 *   "fp64dump sweep=.. cpu=.. seq=.. nbad=.. bad=.."
 * Offline analysis from a dump: bit-position histogram of
 * dst^expected per u64 lane (sign=63 / exponent=62..52 /
 * mantissa=51..0); field-graft checks (is dst[i] equal to
 * fp64_rot(slot, X, srcB[i]) for some other word X; does dst[i]
 * appear verbatim elsewhere in any buffer).
 *
 * Dumps land in movbe_log/fp64_rowmajor/ (relative to CWD, created in
 * init; dualcmp convention). If the dir cannot be created the binary
 * dump is skipped silently - the failure message still carries the
 * first differing elements.
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
#include <unistd.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT 1024
#define FP64_DUMP_DIR "movbe_log/fp64_rowmajor"

#if defined(__aarch64__)

struct NeonRotLdrAtTopRowMajorFp64DumpData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
};

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

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorFp64DumpData *>(malloc(sizeof(NeonRotLdrAtTopRowMajorFp64DumpData)));
    if (!data) {
        log_skip(TestResourceIssueSkipCategory,
                 "neon_rot_ldr_at_top_rowmajor_fp64_dump: state allocation failed");
        return EXIT_SKIP;
    }
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch || !data->expected) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
        log_skip(TestResourceIssueSkipCategory,
                 "neon_rot_ldr_at_top_rowmajor_fp64_dump: buffer allocation failed");
        return EXIT_SKIP;
    }

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t));

    /* golden (issue #373): scalar recomputation — scalar FP64 (d-register)
     * instructions, never the vector (v-register) units under test. The
     * scalar ops carry the same IEEE semantics as the vector ones
     * (FADD/FSUB/FMUL/FMAX, NaN-propagating included), and every one of the
     * 2*COUNT lanes is recomputed on the d-register scalar path (which lands
     * on the low 64-bit lane only). Because the golden derives BOTH lanes
     * from that single scalar path, a stuck bit in either lane (low or high)
     * makes one lane of the golden diverge from the healthy lane of the
     * vector result, so a per-lane stuck-at fault now surfaces as a byte
     * difference at the memcmp — the #373 lane-level blind spot is closed.
     * What remains open is a fault in the FP unit's shared
     * control/rounding/exponent logic that corrupts every lane identically:
     * scalar and vector FP64 dispatch to the same F0/F1 pipes, so no FP64
     * arithmetic (scalar or vector) can serve as a fully independent golden
     * (the only such path would be integer emulation of IEEE-754, out of
     * scope); that whole-unit fault class is where the sibling
     * scalar-integer test family fills in. */
    {
        const float64x1_t *sa = reinterpret_cast<const float64x1_t *>(data->srcA);
        const float64x1_t *sb = reinterpret_cast<const float64x1_t *>(data->srcB);
        float64x1_t *ex = reinterpret_cast<float64x1_t *>(data->expected);
        for (int i = 0; i < 2 * NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT; i++) {
            float64x1_t r;
            switch ((i / 2) % 4) {
                case 0: r = vadd_f64(sa[i], sb[i]); break;
                case 1: r = vsub_f64(sa[i], sb[i]); break;
                case 2: r = vmul_f64(sa[i], sb[i]); break;
                default: r = vmax_f64(sa[i], sb[i]); break;
            }
            ex[i] = r;
        }
    }

    mkdir("movbe_log", 0755); /* mkdir creates only the last component */
    mkdir(FP64_DUMP_DIR, 0755); /* dualcmp convention; idempotent */

    test->data = data;
    return EXIT_SUCCESS;
}

/* Cold path ONLY - kept out-of-line so its stack frame and register
 * pressure never enter the hot function's codegen. The hot function
 * must stay byte-identical to the fp64 parent (verify with objdump
 * after every rebuild); this helper is reached only from the post-sweep
 * memcmp failure branch. */
__attribute__((noinline)) static void fp64_dump_report(
        struct NeonRotLdrAtTopRowMajorFp64DumpData *data, uint64x2_t *temp,
        uint64x2_t *dst, uint64_t sweep) {
    /* cpu is NOT a parameter: keeping the run()'s cpu argument live across
     * the loop shifts the whole register allocation (dst x20->x21, temp
     * x21->x22 ...) and rewrites the store-base encodings in the hot loop.
     * Fetch it cold instead - the actual core residency at report time. */
    const int cpu = sched_getcpu();
    static std::atomic<int> g_seq; /* cold-only: unique filenames across thread restarts */
    const int seq = g_seq.fetch_add(1);

    int bad[8];
    int nbad = 0;
    int total_bad = 0;
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT; i++) {
        if (memcmp(&dst[i], &data->expected[i], sizeof(uint64x2_t)) != 0) {
            total_bad++;
            if (nbad < 8) bad[nbad++] = i;
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/fp64dump_fail_cpu%d_sweep%lu_seq%d_pid%d.bin",
             FP64_DUMP_DIR, cpu, static_cast<unsigned long>(sweep), seq, getpid());
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data->srcA, sizeof(uint64x2_t), NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT, f);
        fwrite(data->srcB, sizeof(uint64x2_t), NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT, f);
        fwrite(data->scratch, sizeof(uint64x2_t), NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT, f);
        fwrite(data->expected, sizeof(uint64x2_t), NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT, f);
        fwrite(temp, sizeof(uint64x2_t), NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT, f);
        fwrite(dst, sizeof(uint64x2_t), NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT, f);
        fprintf(f, "fp64dump sweep=%lu cpu=%d seq=%d nbad=%d bad=",
                static_cast<unsigned long>(sweep), cpu, seq, total_bad);
        for (int k = 0; k < nbad; k++) fprintf(f, "%s%d", k ? "," : "", bad[k]);
        fprintf(f, "\n");
        fclose(f);
    } else {
        log_error("neon_rot_ldr_at_top_rowmajor_fp64_dump: failed to open dump log %s", path);
    }

    /* compact per-element summary in the failure message itself:
     * A/B = feeding sources, T = 1st store, G = dst (got), E = golden,
     * X = G^E per lane. Two elements in the message; everything else
     * in the binary dump. */
    char msg[900];
    int off = snprintf(msg, sizeof(msg),
                       "fp64_dump data miscompare: cpu=%d sweep=%lu nbad=%d bad=",
                       cpu, static_cast<unsigned long>(sweep), total_bad);
    for (int k = 0; k < nbad && k < 2 && off > 0 && off < (int)sizeof(msg) - 260; k++) {
        const int i = bad[k];
        const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
        const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
        const uint64_t *T = reinterpret_cast<const uint64_t *>(&temp[i]);
        const uint64_t *G = reinterpret_cast<const uint64_t *>(&dst[i]);
        const uint64_t *E = reinterpret_cast<const uint64_t *>(&data->expected[i]);
        off += snprintf(msg + off, sizeof(msg) - (size_t)off,
                        " | #%d s=%d A=%016lx,%016lx B=%016lx,%016lx T=%016lx,%016lx G=%016lx,%016lx E=%016lx,%016lx X=%016lx,%016lx",
                        i, i % 4, A[0], A[1], B[0], B[1], T[0], T[1], G[0], G[1], E[0], E[1],
                        G[0] ^ E[0], G[1] ^ E[1]);
    }
    report_fail_msg("%s", msg);
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorFp64DumpData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)));
    if (!temp || !dst) {
        free(temp);
        free(dst);
        log_skip(TestResourceIssueSkipCategory,
                 "neon_rot_ldr_at_top_rowmajor_fp64_dump: per-thread buffer allocation failed");
        return EXIT_SKIP;
    }

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res = fp64_rot(i % 4, a, b);

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_ROWMAJOR_FP64_DUMP_COUNT * sizeof(uint64x2_t)) != 0) {
            /* ---- COLD failure branch ONLY: all dump/report work lives in
             * the out-of-line helper above, so this function's frame and
             * register allocation stay identical to the fp64 parent
             * (hot loop byte-identity: verify with objdump after build). ---- */
            fp64_dump_report(data, temp, dst, sweep);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorFp64DumpData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

#else // !__aarch64__

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_init(struct test *test)
{
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 NEON FP64 required");
    return EXIT_SKIP;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_run(struct test *test, int cpu)
{
    (void)test;
    (void)cpu;
    return EXIT_SKIP;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dump_cleanup(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_fp64_dump, "Cold-path dump variant of neon_rot_ldr_at_top_rowmajor_fp64: hot loop byte-identical, failure branch dumps got/expected/xor + full buffer context for bit-position and field-graft analysis")
    .test_init    = neon_rot_ldr_at_top_rowmajor_fp64_dump_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_fp64_dump_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_fp64_dump_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
