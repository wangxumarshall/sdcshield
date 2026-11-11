/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldp
 * @parblock
 * ldp/stp pair-form variant of the fp64 row-major rotating skeleton
 * (2026-11-11).
 *
 * Lineage: userspace framework port of the kernel-module payload
 * tools/fp64_rowmajor_ko/fp64_rowmajor_ldp_stp.c (verify.log 2026-11-10,
 * 16/16 gates PASS: the ko's ldp/stp data path is bit-identical to the
 * userspace_ref ldr q/str q path over all six buffers — srcA/srcB/scratch/
 * expected/temp/dst FNV-1a hashes match, 256-sweep self-check PASS).
 *
 * Instruction substitution (the ko's core, kept verbatim): the rowmajor
 * parent's single 128-bit vector memory ops become X-register pairs with the
 * SAME 16-byte footprint per instruction:
 *     load:  "ldr %q0, [%1]"          ->  "ldp %0, %1, [%2]"   (8B + 8B)
 *     store: vst1q_u64() (str q)      ->  "stp %0, %1, [%2]"   (8B + 8B)
 * Little-endian AArch64: ldp xA,xB fills the first/second 8-byte words of
 * the 16-byte window, matching the v.d[0]/v.d[1] lanes of the parent's
 * ldr q one to one; stp writes them back in the same order. Byte traffic
 * per sweep is unchanged; what changes is the instruction FORM seen by the
 * LSU: pair decode (one instruction, two 8B GPR transactions) plus a
 * GPR-to-NEON register crossing on every element (the vcombine/vgetq_lane
 * FMOV/INS moves between the integer and FP register banks), a forwarding
 * surface the ldr q parent never touches.
 *
 * Skeleton otherwise identical to neon_rot_ldr_at_top_rowmajor: alternating
 * ascending/descending sweeps (address-local vs forward-progress-position
 * signature), a discarded scratch pair-load at the top of each iteration,
 * 2-src pair loads, fp64_rot slot rotation (0=fadd 1=fsub 2=fmul 3=fmax,
 * slot = i%4), back-to-back stp/stp pair, post-sweep memcmp, cold-path
 * failure report.
 *
 * GOLDEN — same-path NEON recomputation, deliberately NOT the issue #373
 * scalar recomputation used by the integer siblings. A scalar oracle is
 * provably unavailable for this payload (probe, 2026-11-11, 1e7 random
 * pairs + full NaN corner cross on this host, GCC 12, -O2/-O3):
 *   - C fmax() is maxNum semantics (returns the non-NaN operand) while
 *     vmaxq_f64 is the NaN-propagating FMAX — an architectural semantics
 *     difference, ~0.05% of random input pairs;
 *   - for both-NaN inputs the scalar FP pipe and the vector pipe disagree
 *     on WHICH NaN payload survives (an implementation-defined corner;
 *     pinned-operand-order inline asm fadd still differs from vaddq). No
 *     scalar-side golden can bit-match the vector pipe there, and both-NaN
 *     lanes occur at ~5e-4 per buffer pair (P(NaN) = 2^-11 per 64-bit word,
 *     both lanes NaN squared, 2048 lanes), so any scalar golden would be
 *     guaranteed fleet flakiness (false SDC alarms).
 *
 * OPERAND-ORDER PIN. A follow-up probe (review, 2026-11-11, 1e7 both-NaN
 * pairs) showed the vector FMAX's result for two NaN operands is
 * operand-POSITION-dependent (vmaxq_f64(a,b) != vmaxq_f64(b,a) on ~50% of
 * both-NaN pairs; vector fadd/fmul select by VALUE instead), so "golden ==
 * actual" for both-NaN lanes also relies on the operand order inside the
 * compiled code. GCC may commute FP operations freely (IEEE does not pin
 * NaN payload propagation), so fp64_rot pins all four ops with inline asm
 * (operand 1 = a) — the order invariant then holds by construction in
 * every loop copy across compilers and rebuilds.
 *
 * Homogeneous-core assumption: the golden is computed on the init thread's
 * core and the results on every worker core of the same SoC. Architecturally
 * each core may implement the both-NaN selection differently; on a
 * heterogeneous SoC (big.LITTLE-class) that would surface as deterministic
 * miscompares. The deployment target is homogeneous fleets (Kunpeng 920).
 * Coverage trade-off, stated honestly: a persistent NEON-datapath fault
 * that computes deterministic wrong bits can corrupt golden and actual
 * identically here and go unnoticed by THIS test; that fault class is
 * covered by the integer siblings' scalar goldens (issue #373). What this
 * test targets — like the ko — is the LSU pair-form excitation: any
 * misgrafted load delivers wrong inputs to a correctly-computed golden and
 * fails the memcmp.
 *
 * Failure report: cold path only, out-of-line (noinline) so its frame never
 * enters the hot function's codegen. Message carries the first two bad
 * elements with A/B (feeding sources), T (1st store), G (dst got), E
 * (golden), X (G^E per lane) — the ko's dmesg format; "miscompare" kept for
 * the campaign grep convention.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sched.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDP_COUNT 1024

struct NeonRotLdpData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

/* ===== ldp/stp pair-form vector memory (ko-verbatim, u64 -> uint64_t) =====
 * Little-endian AArch64: ldp xA,xB,[addr] loads the first/second 8-byte
 * words into xA/xB, matching v.d[0]/v.d[1] of the parent's ldr q form one
 * to one; stp stores them back in the same order. volatile pins the pair
 * instruction form; "memory" pins program order against the memcmp. */
static inline void store_pair_vec(void *addr, uint64x2_t val) {
    const uint64_t lo = vgetq_lane_u64(val, 0);
    const uint64_t hi = vgetq_lane_u64(val, 1);
    __asm__ volatile ("stp %0, %1, [%2]" : : "r"(lo), "r"(hi), "r"(addr) : "memory");
}

static inline uint64x2_t load_pair_vec(const void *addr) {
    uint64_t lo, hi;
    __asm__ volatile ("ldp %0, %1, [%2]" : "=r"(lo), "=r"(hi) : "r"(addr) : "memory");
    return vcombine_u64(vcreate_u64(lo), vcreate_u64(hi));
}

/* FP64 rotation, slot order mirrors the integer parent: 0=fadd 1=fsub
 * 2=fmul 3=fmax. All four ops are inline asm with the operand order
 * PINNED (operand 1 = a): the vector FMAX result for two NaN operands is
 * operand-position-dependent on this core (probe, review 2026-11-11), so
 * golden-vs-actual bit-identity for both-NaN lanes must not depend on GCC
 * commuting the intrinsics between the init and run copies. Reinterpret
 * casts around the asm are compile-time no-ops. */
static inline uint64x2_t fp64_rot(int slot, uint64x2_t a, uint64x2_t b) {
    uint64x2_t res;
    switch (slot) {
        case 0: __asm__ ("fadd %0.2d, %1.2d, %2.2d" : "=w"(res) : "w"(a), "w"(b)); break;
        case 1: __asm__ ("fsub %0.2d, %1.2d, %2.2d" : "=w"(res) : "w"(a), "w"(b)); break;
        case 2: __asm__ ("fmul %0.2d, %1.2d, %2.2d" : "=w"(res) : "w"(a), "w"(b)); break;
        default: __asm__ ("fmax %0.2d, %1.2d, %2.2d" : "=w"(res) : "w"(a), "w"(b)); break;
    }
    return res;
}

#endif

static int neon_rot_ldp_init(struct test *test) {
    auto *data = static_cast<NeonRotLdpData *>(malloc(sizeof(NeonRotLdpData)));
    if (!data) {
        log_skip(RuntimeSkipCategory, "neon_rot_ldp: failed to allocate test data");
        return EXIT_SKIP;
    }
    data->srcA     = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)));
    data->srcB     = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)));
    data->scratch  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch || !data->expected) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
        log_skip(RuntimeSkipCategory, "neon_rot_ldp: failed to allocate test buffers");
        return EXIT_SKIP;
    }

    memset_random(data->srcA, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t));

    /* golden: same-path NEON recomputation (see the file header for the
     * probe-backed reason a scalar golden is impossible here) — the same
     * fp64_rot, the same pair helpers, the same slot mapping as the run
     * loop, computed once on the init thread. */
    for (int i = 0; i < NEON_ROT_LDP_COUNT; i++) {
        store_pair_vec(&data->expected[i],
                       fp64_rot(i % 4, load_pair_vec(&data->srcA[i]),
                                           load_pair_vec(&data->srcB[i])));
    }

    test->data = data;
    return EXIT_SUCCESS;
}

/* Cold path ONLY — kept out-of-line so its stack frame and register
 * pressure never enter the hot function's codegen. cpu is fetched cold
 * (sched_getcpu) because keeping the run()'s cpu parameter live across
 * the loop shifts the hot loop's register allocation (fp64 family
 * convention). */
__attribute__((noinline)) static void neon_rot_ldp_miscompare_report(
        NeonRotLdpData *data, const uint64x2_t *temp,
        const uint64x2_t *dst, uint64_t sweep) {
    const int cpu = sched_getcpu();

    int bad[8];
    int nbad = 0;
    int total_bad = 0;
    for (int i = 0; i < NEON_ROT_LDP_COUNT; i++) {
        if (memcmp(&dst[i], &data->expected[i], sizeof(uint64x2_t)) != 0) {
            total_bad++;
            if (nbad < 8)
                bad[nbad++] = i;
        }
    }

    /* compact per-element summary: A/B = feeding sources, T = 1st store,
     * G = dst (got), E = golden, X = G^E per lane. Two elements in the
     * message (ko's dmesg format). */
    char msg[900];
    int off = snprintf(msg, sizeof(msg),
                       "neon_rot_ldp data miscompare: cpu=%d sweep=%lu nbad=%d bad=",
                       cpu, static_cast<unsigned long>(sweep), total_bad);
    for (int k = 0; k < nbad && k < 2 && off > 0 && off < (int)sizeof(msg) - 260; k++) {
        const int i = bad[k];
        const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
        const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
        const uint64_t *T = reinterpret_cast<const uint64_t *>(&temp[i]);
        const uint64_t *G = reinterpret_cast<const uint64_t *>(&dst[i]);
        const uint64_t *E = reinterpret_cast<const uint64_t *>(&data->expected[i]);
        off += snprintf(msg + off, sizeof(msg) - static_cast<size_t>(off),
                        " | #%d s=%d A=%016lx,%016lx B=%016lx,%016lx T=%016lx,%016lx G=%016lx,%016lx E=%016lx,%016lx X=%016lx,%016lx",
                        i, i % 4, A[0], A[1], B[0], B[1], T[0], T[1], G[0], G[1], E[0], E[1],
                        G[0] ^ E[0], G[1] ^ E[1]);
    }
    report_fail_msg("%s", msg);
}

static int neon_rot_ldp_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdpData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)));
    if (!temp || !dst) {
        free(temp);
        free(dst);
        report_fail_msg("neon_rot_ldp: temp/dst buffer allocation failed");
        return EXIT_FAILURE;
    }

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDP_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDP_COUNT - 1 - n);

            uint64x2_t unused_x = load_pair_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_pair_vec(&data->srcA[i]);
            uint64x2_t b = load_pair_vec(&data->srcB[i]);
            uint64x2_t res = fp64_rot(i % 4, a, b);

            store_pair_vec(&temp[i], res);
            store_pair_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDP_COUNT * sizeof(uint64x2_t)) != 0) {
            /* ---- COLD failure branch ONLY: all report work lives in the
             * out-of-line helper above, so this function's frame and
             * register allocation stay hot-loop-lean. ---- */
            neon_rot_ldp_miscompare_report(data, temp, dst, sweep);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldp_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdpData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldp, "ldp/stp pair-form fp64 rowmajor: rotating fadd/fsub/fmul/fmax over pair-loaded sources with back-to-back pair stores (kernel-module-validated form, tools/fp64_rowmajor_ko)")
    .test_init    = neon_rot_ldp_init,
    .test_run     = neon_rot_ldp_run,
    .test_cleanup = neon_rot_ldp_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
