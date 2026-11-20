/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test fmov_train_ldr_str_l2l3
 * @parblock
 * L2/L3-resident counterpart of fmov_train_ldr_str (2026-11-20, user
 * directive: extend the pair with an L2/L3-resident 512 KiB working-set
 * version).
 *
 * The hot-loop instruction sequence is byte-for-byte the proven ridge
 * recipe (2 independent source loads + cross-register-file FMOV consumer
 * chain + slot-rotated add/eor/sub + back-to-back str/str); only the
 * GEOMETRY changes: per-thread buffers grow from 32 KiB to 128 KiB
 * (4 x 128 KiB = 512 KiB working set, 64B-aligned).  L1D (64 KiB on the
 * 920) cannot hold even one buffer, so steady-state supply comes from the
 * private L2 with overflow resident in L3 — the same residence band as
 * loop_check_m.
 *
 * Detection semantics grafted from loop_check_m (LoopCheck group M):
 * every round runs compute pass -> DSB SY -> verify pass.  The verify
 * pass recomputes the op in scalar C and compares THREE references:
 * recomputed r vs stored dstA/dstB vs the init-time golden (computed
 * from pristine sources before any round).  This catches
 *   - transient compute-pass faults (wrong value stored in memory,
 *     seen by the delayed verify read),
 *   - verify-pass faults (recompute disagrees with both references),
 *   - src corruption in memory (recompute and stored agree with each
 *     other but both disagree with the pristine-source golden).
 * Honest boundary (same as loop_check_m): a PERMANENT datapath fault is
 * NOT caught — it is baked into the golden and all three references
 * agree.  The init self-check (one compute+DSB+verify round inside
 * test_init, golden freshly recomputed against a second scalar pass)
 * bounds "environment/compiler baked an error into the golden" but not
 * permanent silicon faults.
 *
 * DESIGN RULING (2026-11-20, discovered in RED): rot/slot are PINNED to
 * (0, add) in the run loop, unlike the parent's per-round rotation.
 * Reason: the init-time golden is a ONE-SHOT snapshot from pristine
 * sources — it stays a valid independent reference only while the recipe
 * is constant.  Rotating would need a golden per (slot,rot) pair, which
 * multiplies per-thread buffers past the 512 KiB working-set budget
 * (the defining property of this variant).  Slot/rot coverage is not
 * lost — the parent pair and the 3+3 extension keep it; this variant
 * trades rotation for residence-depth detection.
 *
 * Pre-registered verdict (2026-11-04 discriminator-grid lineage,
 * ma_src_overl2 ruling A): on THIS host (no-SVE 920, core-179 online)
 * the recipe family's excitation collapses once source loads leave the
 * L1D/L2 fast domain — expected 0 fail.  A 0-fail full-core run is
 * NEGATIVE-SPACE data, not a defect.  The value of this arm is on OTHER
 * machines (e.g. the 920s where memcpy_l2/l3_cache_size detected SDC):
 * the FMOV chain + back-to-back stores fed from the L2/L3 supply
 * domain, plus residence-depth bit-error detection the fast-domain pair
 * structurally cannot see (its verify reads are L1D/L2 hits).
 *
 * Memory note: 640 KiB per-thread footprint (4 compute buffers +
 * golden) x 192 threads ~= 120 MiB peak on a full-core run — trivial
 * for any server host; there is no runtime knob (v1).
 * Provenance: hot loop extracted from SIMDFPLoadStoreMemLoopCpyP08
 * disassembly (user-provided, 2026-11-18), same as the parent pair.
 * The LDP/STP twin of THIS test lives in fmov_train_ldp_stp_l2l3.cpp.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)

#define FTS2_NUM_ELEMENTS 8192           /* 16B elements -> 128KB/buffer */
#define FTS2_BYTES (FTS2_NUM_ELEMENTS * 16)
#define FTS2_ALIGN 64                    /* L2/L3 line; covers ldr/str q */

/*
 * One pass over the buffers, single-register form (ldr/str) —
 * instruction-for-instruction the parent's hot loop (same registers,
 * same slot rotation).  slot: 0=add 1=eor 2=sub on the SIMD side; the
 * GPR consumer is a plain ADD of the rotating constant.  Post-index
 * advance, pointers wrap back each pass (reset by the caller).
 */
static void fts2_pass_ldr_str(const uint64_t *a, const uint64_t *b,
                              uint64_t *da, uint64_t *db, uint64_t rot, int slot)
{
    const uint64_t *pa = a, *pb = b;
    uint64_t *pda = da, *pdb = db;

    for (size_t i = 0; i < FTS2_NUM_ELEMENTS; ++i) {
        if (slot == 0) {
            __asm__ volatile(
                "ldr  q0, [%[a]], #16\n\t"
                "ldr  q1, [%[b]], #16\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "add  v6.2d, v1.2d, v4.2d\n\t"
                "str  q6, [%[da]], #16\n\t"
                "str  q6, [%[db]], #16\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v4", "v6", "x4", "x5", "x6", "x7", "memory");
        } else if (slot == 1) {
            __asm__ volatile(
                "ldr  q0, [%[a]], #16\n\t"
                "ldr  q1, [%[b]], #16\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "eor  v6.16b, v1.16b, v4.16b\n\t"
                "str  q6, [%[da]], #16\n\t"
                "str  q6, [%[db]], #16\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v4", "v6", "x4", "x5", "x6", "x7", "memory");
        } else {
            __asm__ volatile(
                "ldr  q0, [%[a]], #16\n\t"
                "ldr  q1, [%[b]], #16\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "sub  v6.2d, v1.2d, v4.2d\n\t"
                "str  q6, [%[da]], #16\n\t"
                "str  q6, [%[db]], #16\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v4", "v6", "x4", "x5", "x6", "x7", "memory");
        }
    }
}

/* scalar bit-exact golden: g = b <op> (a + rot), per u64 lane — the same
 * formula as the parent pair's fts_golden (issue #373 scalar discipline) */
static void fts2_golden(const uint64_t *a, const uint64_t *b,
                        uint64_t *g, uint64_t rot, int slot)
{
    size_t n = FTS2_NUM_ELEMENTS * 2;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i] + rot;
        g[i] = (slot == 0) ? (b[i] + x)
             : (slot == 1) ? (b[i] ^ x)
             :               (b[i] - x);
    }
}

/*
 * Verify pass (loop_check_m semantics): recompute in scalar C, then
 * three-reference compare — r vs stored dstA, r vs stored dstB, r vs
 * the init-time golden.  Returns false after report_fail_msg on the
 * first mismatch (element-level detail: index, round, slot, expected,
 * got, and WHICH reference disagreed).
 */
static bool fts2_verify(const uint64_t *a, const uint64_t *b,
                        const uint64_t *da, const uint64_t *db,
                        const uint64_t *gold, uint64_t rot, int slot,
                        unsigned long round, const char *phase)
{
    size_t n = FTS2_NUM_ELEMENTS * 2;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i] + rot;
        uint64_t r = (slot == 0) ? (b[i] + x)
                   : (slot == 1) ? (b[i] ^ x)
                   :               (b[i] - x);
        if (r != da[i]) {
            report_fail_msg("fmov_train_ldr_str_l2l3: dstA mismatch at "
                            "element %zu (%s, round %lu, slot %d): "
                            "expected 0x%016lx got 0x%016lx "
                            "(recompute vs stored, L2/L3-resident FMOV chain)",
                            i / 2, phase, round, slot,
                            (unsigned long)r, (unsigned long)da[i]);
            return false;
        }
        if (r != db[i]) {
            report_fail_msg("fmov_train_ldr_str_l2l3: dstB mismatch at "
                            "element %zu (%s, round %lu, slot %d): "
                            "expected 0x%016lx got 0x%016lx "
                            "(recompute vs stored, L2/L3-resident FMOV chain)",
                            i / 2, phase, round, slot,
                            (unsigned long)r, (unsigned long)db[i]);
            return false;
        }
        if (r != gold[i]) {
            report_fail_msg("fmov_train_ldr_str_l2l3: golden mismatch at "
                            "element %zu (%s, round %lu, slot %d): "
                            "recompute 0x%016lx stored 0x%016lx golden "
                            "0x%016lx (src corrupted in memory?)",
                            i / 2, phase, round, slot,
                            (unsigned long)r, (unsigned long)da[i],
                            (unsigned long)gold[i]);
            return false;
        }
    }
    return true;
}

static int fmov_train_ldr_str_l2l3_init(struct test *test)
{
    /* nothing to preallocate: buffers are per-thread, in run() */
    (void)test;
    return EXIT_SUCCESS;
}

static int fmov_train_ldr_str_l2l3_run(struct test *test, int cpu)
{
    (void)cpu;

    /* per-thread 512 KiB working set; 64B alignment (L2/L3 line) */
    auto *srcA = static_cast<uint64_t *>(aligned_alloc(FTS2_ALIGN, FTS2_BYTES));
    auto *srcB = static_cast<uint64_t *>(aligned_alloc(FTS2_ALIGN, FTS2_BYTES));
    auto *dstA = static_cast<uint64_t *>(aligned_alloc(FTS2_ALIGN, FTS2_BYTES));
    auto *dstB = static_cast<uint64_t *>(aligned_alloc(FTS2_ALIGN, FTS2_BYTES));
    auto *gold = static_cast<uint64_t *>(aligned_alloc(FTS2_ALIGN, FTS2_BYTES));
    if (!srcA || !srcB || !dstA || !dstB || !gold) {
        free(srcA); free(srcB); free(dstA); free(dstB); free(gold);
        report_fail_msg("fmov_train_ldr_str_l2l3: buffer allocation failed");
        return EXIT_FAILURE;
    }

    /* randomization hardening H14': framework RNG fill (bit patterns) */
    memset_random(srcA, FTS2_BYTES);
    memset_random(srcB, FTS2_BYTES);
    memset(dstA, 0, FTS2_BYTES);
    memset(dstB, 0, FTS2_BYTES);

    /* golden from pristine sources, one scalar pass.  The run loop
     * below pins rot=0/slot=0 (see the RULING note in the header comment)
     * so this single snapshot stays a valid independent reference for
     * every round; srcA/srcB are never written by the hot loop. */
    fts2_golden(srcA, srcB, gold, 0, 0);

    /* init self-check: one full compute->DSB->verify round with the
     * round-0 recipe.  A failure here means the environment/compiler
     * baked an error into the golden or the pipeline is broken before
     * any stress — fail loudly, do not start the campaign. */
    {
        fts2_pass_ldr_str(srcA, srcB, dstA, dstB, 0, 0);
        __asm__ volatile ("dsb sy" ::: "memory");
        if (!fts2_verify(srcA, srcB, dstA, dstB, gold, 0, 0, 0,
                         "init self-check"))
            return EXIT_FAILURE;
    }

    uint64_t round = 0;
    do {
        /* rot/slot pinned (RULING 2026-11-20): the init-time golden is a
         * one-shot snapshot from pristine sources, which is only a valid
         * independent reference if the recipe never changes.  Slot/rot
         * rotation would need a golden per (slot,rot) — memory that does
         * not fit the 512 KiB/thread budget.  The recipe family's value
         * here is the FMOV chain + back-to-back stores in the L2/L3
         * supply domain, which the pinned form exercises identically;
         * the parent pair keeps the slot/rot rotation coverage. */
        fts2_pass_ldr_str(srcA, srcB, dstA, dstB, 0, 0);
        __asm__ volatile ("dsb sy" ::: "memory");
        if (!fts2_verify(srcA, srcB, dstA, dstB, gold, 0, 0,
                         (unsigned long)round, "run"))
            return EXIT_FAILURE;
        ++round;
    } while (test_time_condition(test));

    free(srcA); free(srcB); free(dstA); free(dstB); free(gold);
    return EXIT_SUCCESS;
}

DECLARE_TEST(fmov_train_ldr_str_l2l3, "L2/L3-resident counterpart of fmov_train_ldr_str: 512 KiB working-set FMOV cross-register-file consumer chain (2-source ldr + FMOV round-trip + add + back-to-back str/str; rot/slot pinned, see header DESIGN RULING), per-round compute + DSB SY + three-reference verify (recompute vs stored vs init golden) (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = fmov_train_ldr_str_l2l3_init,
    .test_run = fmov_train_ldr_str_l2l3_run,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST

#else  /* !__aarch64__ */

static int fmov_train_ldr_str_l2l3_init(struct test *test)  { (void)test; return EXIT_SUCCESS; }
static int fmov_train_ldr_str_l2l3_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SUCCESS; }

DECLARE_TEST(fmov_train_ldr_str_l2l3, "L2/L3-resident counterpart of fmov_train_ldr_str (ARM64-only)")
    .test_init    = fmov_train_ldr_str_l2l3_init,
    .test_run     = fmov_train_ldr_str_l2l3_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif
