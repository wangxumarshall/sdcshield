/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test fmov_train_ldp_stp_l2l3
 * @parblock
 * LDP/STP pair-form twin of fmov_train_ldr_str_l2l3 (2026-11-20, user
 * directive: extend the parent pair with L2/L3-resident 512 KiB
 * working-set versions; this file is the ldp/stp half).
 *
 * Same geometry, detection semantics and rulings as the ldr/str twin:
 * 128 KiB/buffer (4 buffers + golden = 640 KiB/thread), 512 KiB
 * working set resident in the private-L2/L3 band; hot loop
 * instruction-for-instruction the parent fmov_train_ldp_stp
 * (post-indexed Q-pair trains, 32B/instr; cross-register-file FMOV
 * consumer chain x4-x11 for the two elements per body; back-to-back
 * STP Q-pair stores), rot/slot PINNED to (0, add) — see the DESIGN
 * RULING in fmov_train_ldr_str_l2l3.cpp (init-time golden is a one-shot
 * pristine snapshot, valid as an independent reference only while the
 * recipe is constant; rotation memory does not fit the budget; the
 * parent pair and the 3+3 extension keep the rotation coverage).
 *
 * Per round: compute pass -> DSB SY -> verify pass recomputing in
 * scalar C and comparing three references (recompute vs stored
 * dstA/dstB vs init golden).  Honest boundary identical to
 * loop_check_m and the ldr/str twin: permanent datapath faults are
 * baked into the golden and not caught; the init self-check bounds
 * "environment/compiler baked an error into the golden".
 *
 * Pre-registered verdict (2026-11-04 discriminator-grid lineage,
 * ma_src_overl2 ruling A): expected 0 fail on THIS host — negative-
 * space data; the arm's value is on machines where the L2/L3 supply
 * domain is the excitation band (e.g. 920s where memcpy_l2/l3_cache_
 * size detected SDC) and for residence-depth bit errors the
 * fast-domain parent structurally cannot see.
 *
 * Memory note: 640 KiB per-thread footprint (4 compute buffers +
 * golden) x 192 threads ~= 120 MiB peak on a full-core run — trivial
 * for any server host; no runtime knob (v1).
 * Provenance: SIMDFPLoadStoreMemLoopCpyP08 disassembly extract
 * (user-provided, 2026-11-18), same as the whole fmov_train family.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)

#define FTP2_NUM_ELEMENTS 8192           /* 16B elements -> 128KB/buffer */
#define FTP2_BYTES (FTP2_NUM_ELEMENTS * 16)
#define FTP2_ALIGN 64                    /* L2/L3 line; covers Q-pairs */

/*
 * One pass, pair form: two elements per body — instruction-for-
 * instruction the parent's hot loop (v0-v7, x4-x11, post-index #32).
 * slot argument kept for signature parity with the ldr/str twin's
 * chassis; callers pin it to 0 (add) per the DESIGN RULING.
 */
static void ftp2_pass_ldp_stp(const uint64_t *a, const uint64_t *b,
                              uint64_t *da, uint64_t *db, uint64_t rot, int slot)
{
    const uint64_t *pa = a, *pb = b;
    uint64_t *pda = da, *pdb = db;

    for (size_t i = 0; i < FTP2_NUM_ELEMENTS; i += 2) {
        if (slot == 0) {
            __asm__ volatile(
                "ldp  q0, q1, [%[a]], #32\n\t"
                "ldp  q2, q3, [%[b]], #32\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "fmov x8, d1\n\t"
                "fmov x9, v1.d[1]\n\t"
                "add  x10, x8, %[rot]\n\t"
                "add  x11, x9, %[rot]\n\t"
                "fmov d5, x10\n\t"
                "fmov v5.d[1], x11\n\t"
                "add  v6.2d, v2.2d, v4.2d\n\t"
                "add  v7.2d, v3.2d, v5.2d\n\t"
                "stp  q6, q7, [%[da]], #32\n\t"
                "stp  q6, q7, [%[db]], #32\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "memory");
        } else if (slot == 1) {
            __asm__ volatile(
                "ldp  q0, q1, [%[a]], #32\n\t"
                "ldp  q2, q3, [%[b]], #32\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "fmov x8, d1\n\t"
                "fmov x9, v1.d[1]\n\t"
                "add  x10, x8, %[rot]\n\t"
                "add  x11, x9, %[rot]\n\t"
                "fmov d5, x10\n\t"
                "fmov v5.d[1], x11\n\t"
                "eor  v6.16b, v2.16b, v4.16b\n\t"
                "eor  v7.16b, v3.16b, v5.16b\n\t"
                "stp  q6, q7, [%[da]], #32\n\t"
                "stp  q6, q7, [%[db]], #32\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "memory");
        } else {
            __asm__ volatile(
                "ldp  q0, q1, [%[a]], #32\n\t"
                "ldp  q2, q3, [%[b]], #32\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "fmov x8, d1\n\t"
                "fmov x9, v1.d[1]\n\t"
                "add  x10, x8, %[rot]\n\t"
                "add  x11, x9, %[rot]\n\t"
                "fmov d5, x10\n\t"
                "fmov v5.d[1], x11\n\t"
                "sub  v6.2d, v2.2d, v4.2d\n\t"
                "sub  v7.2d, v3.2d, v5.2d\n\t"
                "stp  q6, q7, [%[da]], #32\n\t"
                "stp  q6, q7, [%[db]], #32\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "memory");
        }
    }
}

/* scalar bit-exact golden: g = b <op> (a + rot), per u64 lane — the same
 * formula as the parent pair's ftp_golden (issue #373 scalar discipline).
 * The pinned recipe is (rot=0, slot=0) but the slot parameter stays so
 * the three arms above stay reachable/auditable. */
static void ftp2_golden(const uint64_t *a, const uint64_t *b,
                        uint64_t *g, uint64_t rot, int slot)
{
    size_t n = FTP2_NUM_ELEMENTS * 2;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i] + rot;
        g[i] = (slot == 0) ? (b[i] + x)
             : (slot == 1) ? (b[i] ^ x)
             :               (b[i] - x);
    }
}

/* Verify pass (loop_check_m semantics, same three references as the
 * ldr/str twin).  Returns false after report_fail_msg on mismatch. */
static bool ftp2_verify(const uint64_t *a, const uint64_t *b,
                        const uint64_t *da, const uint64_t *db,
                        const uint64_t *gold, uint64_t rot, int slot,
                        unsigned long round, const char *phase)
{
    size_t n = FTP2_NUM_ELEMENTS * 2;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i] + rot;
        uint64_t r = (slot == 0) ? (b[i] + x)
                   : (slot == 1) ? (b[i] ^ x)
                   :               (b[i] - x);
        if (r != da[i]) {
            report_fail_msg("fmov_train_ldp_stp_l2l3: dstA mismatch at "
                            "element %zu (%s, round %lu, slot %d): "
                            "expected 0x%016lx got 0x%016lx "
                            "(recompute vs stored, L2/L3-resident pair form)",
                            i / 2, phase, round, slot,
                            (unsigned long)r, (unsigned long)da[i]);
            return false;
        }
        if (r != db[i]) {
            report_fail_msg("fmov_train_ldp_stp_l2l3: dstB mismatch at "
                            "element %zu (%s, round %lu, slot %d): "
                            "expected 0x%016lx got 0x%016lx "
                            "(recompute vs stored, L2/L3-resident pair form)",
                            i / 2, phase, round, slot,
                            (unsigned long)r, (unsigned long)db[i]);
            return false;
        }
        if (r != gold[i]) {
            report_fail_msg("fmov_train_ldp_stp_l2l3: golden mismatch at "
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

static int fmov_train_ldp_stp_l2l3_init(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

static int fmov_train_ldp_stp_l2l3_run(struct test *test, int cpu)
{
    (void)cpu;

    /* per-thread 640 KiB footprint; 64B alignment covers Q-pairs */
    auto *srcA = static_cast<uint64_t *>(aligned_alloc(FTP2_ALIGN, FTP2_BYTES));
    auto *srcB = static_cast<uint64_t *>(aligned_alloc(FTP2_ALIGN, FTP2_BYTES));
    auto *dstA = static_cast<uint64_t *>(aligned_alloc(FTP2_ALIGN, FTP2_BYTES));
    auto *dstB = static_cast<uint64_t *>(aligned_alloc(FTP2_ALIGN, FTP2_BYTES));
    auto *gold = static_cast<uint64_t *>(aligned_alloc(FTP2_ALIGN, FTP2_BYTES));
    if (!srcA || !srcB || !dstA || !dstB || !gold) {
        free(srcA); free(srcB); free(dstA); free(dstB); free(gold);
        report_fail_msg("fmov_train_ldp_stp_l2l3: buffer allocation failed");
        return EXIT_FAILURE;
    }

    memset_random(srcA, FTP2_BYTES);
    memset_random(srcB, FTP2_BYTES);
    memset(dstA, 0, FTP2_BYTES);
    memset(dstB, 0, FTP2_BYTES);

    /* golden from pristine sources, one scalar pass (pinned recipe) */
    ftp2_golden(srcA, srcB, gold, 0, 0);

    /* init self-check: one full compute->DSB->verify round */
    {
        ftp2_pass_ldp_stp(srcA, srcB, dstA, dstB, 0, 0);
        __asm__ volatile ("dsb sy" ::: "memory");
        if (!ftp2_verify(srcA, srcB, dstA, dstB, gold, 0, 0, 0,
                         "init self-check"))
            return EXIT_FAILURE;
    }

    uint64_t round = 0;
    do {
        /* rot/slot pinned (DESIGN RULING, see fmov_train_ldr_str_l2l3) */
        ftp2_pass_ldp_stp(srcA, srcB, dstA, dstB, 0, 0);
        __asm__ volatile ("dsb sy" ::: "memory");
        if (!ftp2_verify(srcA, srcB, dstA, dstB, gold, 0, 0,
                         (unsigned long)round, "run"))
            return EXIT_FAILURE;
        ++round;
    } while (test_time_condition(test));

    free(srcA); free(srcB); free(dstA); free(dstB); free(gold);
    return EXIT_SUCCESS;
}

DECLARE_TEST(fmov_train_ldp_stp_l2l3, "L2/L3-resident counterpart of fmov_train_ldp_stp: 512 KiB working-set post-indexed Q-pair load trains (32B/instr) + cross-register-file FMOV consumer chain + pair stores, per-round compute + DSB SY + three-reference verify (recompute vs stored vs init golden) (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = fmov_train_ldp_stp_l2l3_init,
    .test_run = fmov_train_ldp_stp_l2l3_run,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST

#else  /* !__aarch64__ */

static int fmov_train_ldp_stp_l2l3_init(struct test *test)  { (void)test; return EXIT_SUCCESS; }
static int fmov_train_ldp_stp_l2l3_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SUCCESS; }

DECLARE_TEST(fmov_train_ldp_stp_l2l3, "L2/L3-resident counterpart of fmov_train_ldp_stp (ARM64-only)")
    .test_init    = fmov_train_ldp_stp_l2l3_init,
    .test_run     = fmov_train_ldp_stp_l2l3_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif
