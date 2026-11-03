/* eigen_svd_bidiag_dual.c — dual-channel extraction of sdcshield test
 * `eigen_svd_bidiag_sve` for gem5 CHAOS SE-O3 validation (issue #484).
 *
 * Purpose: reproduce the issue #484 blind spot and prove the fix, at SVE
 * tiers the build host cannot run (host has no SVE — CLAUDE.md gem5-fi
 * rule). Extraction source: tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp
 * @ main 418e4e09 (bidiagonalize_ref / bidiagonalize_sve / random_pm_one
 * shape / bidiag_compare_or_fail judgment / run alternation structure —
 * VERBATIM except the detection-neutral adaptations listed below).
 *
 * Detection-neutral adaptations (cf. fpu_special_sve_dual.c precedent):
 *   - framework random64()          -> splitmix64 (fixed seed, argv)
 *   - report_fail_msg (aborts)      -> g_fails++ + fprintf (count only)
 *   - per-thread TLS buffers        -> plain locals (single context)
 *   - arm64_require_sve_vl gate     -> gem5 --sve_vl satisfies it by
 *                                      construction; VL printed from svcntd()
 *   - TEST_LOOP timed loop          -> fixed iters count (argv)
 * Added (not in the test): per-compare max_diff/tol diagnostic line, FNV
 * checksums, and the two experimental knobs below — none touch the kernel
 * arithmetic or the comparison judgments.
 *
 * argv: <mode old|new> <fault none|tiny> [n=64] [iters=8] [seed=1]
 *   mode=old — the PRE-fix channel set: reroll tolerance vs own scalar
 *              golden + double-pass byte self-consistency + frozen-replay
 *              tolerance vs init scalar golden.
 *   mode=new — old channels + the issue #484 fix: frozen-replay rounds
 *              additionally memcmp the worker output against golden_sve,
 *              the byte-exact expectation computed by the SAME SVE kernel
 *              in the "init" role BEFORE the "worker" rounds (models the
 *              init control thread computing on a different physical core).
 *   fault=tiny — deterministic low-amplitude SDC injection: after every
 *              DUT-role kernel call, flip the mantissa LSB (~1 ULP) of a
 *              diagonal element. Deterministic (both double passes get the
 *              identical flip -> self-consistency passes) and far below
 *              kTolRel*||B||_F (~1e-15 vs ~3.7e-8 at n=64) -> tolerance
 *              passes. This is exactly the escaped fault class of #484.
 *              Never applied to init-role outputs (a healthy init core).
 *
 * Output protocol (guest stdout):
 *   MODE=... FAULT=... VL=<bits> n=<n> iters=<k> fault_elem=(r,c)
 *   [<channel> iter <i>] max_diff=... tol=... (PASS|FAIL)   [per compare]
 *   [FAIL ...] lines on stderr for each counted failure
 *   SDCSHIELD_FAILS=<uint>
 *   FNV_GOLDEN=<hex> FNV_GOLDEN_SVE=<hex> FNV_WORK_LAST=<hex>
 * exit = g_fails ? 1 : 0
 *
 * Build: clang -static -O2 -march=armv9-a+sve -Wall -Wextra \
 *          -o eigen_svd_bidiag_dual eigen_svd_bidiag_dual.c -lm
 * Run:   gem5.opt --quiet --outdir=<dir> configs/se/arm_chaos.py --cpu O3 \
 *          --max_faults 0 --cmd <abs path>/eigen_svd_bidiag_dual \
 *          --sve_vl <1|2|4|8> --workload_args "new tiny 64 8 1"
 */
#include <arm_sve.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned g_fails = 0;
static uint64_t fnv = 1469598103934665603ULL;
static void fnv_u64(uint64_t v)
{
    for (int i = 0; i < 8; ++i) {
        fnv ^= (v >> (8 * i)) & 0xFF;
        fnv *= 1099511628211ULL;
    }
}
static void fnv_buf(const double *p, int n)
{
    for (int i = 0; i < n * n; ++i) {
        uint64_t b;
        memcpy(&b, &p[i], sizeof b);
        fnv_u64(b);
    }
}

/* framework random64() replacement (adaptation; same [-1,1) shaping as the
 * test's random_pm_one: high 53 bits -> fraction, bit 0 -> sign). */
static uint64_t sm64_state;
static uint64_t splitmix64(void)
{
    uint64_t z = (sm64_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static double random_pm_one(void)
{
    uint64_t r = splitmix64();
    double frac = (double)(r >> 11) / 9007199254740992.0; /* [0, 1) */
    return (r & 1) ? (frac - 1.0) : frac;
}

/* C11 aligned_alloc contract (verbatim from the test): sizes rounded up to
 * a multiple of the alignment. */
static inline size_t round_up_64(size_t n)
{
    return (n + 63) & ~(size_t)63;
}

/* ==== VERBATIM from eigen_svd_bidiag_sve.cpp @ 418e4e09 ==== */

static void bidiagonalize_ref(double *A, int n)
{
    double *v = (double *)aligned_alloc(64, round_up_64((size_t)n * sizeof(double)));
    double *w = (double *)aligned_alloc(64, round_up_64((size_t)n * sizeof(double)));
    for (int k = 0; k + 1 < n; ++k) {
        /* left Householder: zero out column k below the diagonal */
        double norm2 = 0.0;
        for (int i = k; i < n; ++i)
            norm2 += A[i * n + k] * A[i * n + k];
        double alpha = sqrt(norm2);
        double v0 = A[k * n + k] - alpha;
        double vtv = v0 * v0;
        for (int i = k + 1; i < n; ++i)
            vtv += A[i * n + k] * A[i * n + k];
        if (vtv < 1e-300)
            continue;
        double tau = 2.0 * v0 * v0 / vtv;
        v[k] = 1.0;
        for (int i = k + 1; i < n; ++i)
            v[i] = A[i * n + k] / v0;
        for (int j = k + 1; j < n; ++j) {
            double dot = 0.0;
            for (int i = k; i < n; ++i)
                dot += v[i] * A[i * n + j];
            dot *= tau;
            for (int i = k; i < n; ++i)
                A[i * n + j] = fma(-v[i], dot, A[i * n + j]);
        }
        A[k * n + k] = alpha;
        for (int i = k + 1; i < n; ++i)
            A[i * n + k] = 0.0;

        /* right Householder */
        if (k + 2 >= n)
            break;
        norm2 = 0.0;
        for (int j = k + 1; j < n; ++j)
            norm2 += A[k * n + j] * A[k * n + j];
        if (norm2 < 1e-300)
            continue;
        alpha = sqrt(norm2);
        v0 = A[k * n + k + 1] - alpha;
        vtv = v0 * v0;
        for (int j = k + 2; j < n; ++j)
            vtv += A[k * n + j] * A[k * n + j];
        if (vtv < 1e-300)
            continue;
        tau = 2.0 * v0 * v0 / vtv;
        w[k + 1] = 1.0;
        for (int j = k + 2; j < n; ++j)
            w[j] = A[k * n + j] / v0;
        for (int i = k + 1; i < n; ++i) {
            double dot = 0.0;
            for (int j = k + 1; j < n; ++j)
                dot += w[j] * A[i * n + j];
            dot *= tau;
            for (int j = k + 1; j < n; ++j)
                A[i * n + j] = fma(-w[j], dot, A[i * n + j]);
        }
        A[k * n + k + 1] = alpha;
        for (int j = k + 2; j < n; ++j)
            A[k * n + j] = 0.0;
    }
    free(v);
    free(w);
}

static void bidiagonalize_sve(double *A, int n, double *v, double *w,
                              const uint64_t *gofs)
{
    for (int k = 0; k + 1 < n; ++k) {
        double norm2 = 0.0;
        for (int i = k; i < n; ++i)
            norm2 += A[i * n + k] * A[i * n + k];
        double alpha = sqrt(norm2);
        double v0 = A[k * n + k] - alpha;
        double vtv = v0 * v0;
        for (int i = k + 1; i < n; ++i)
            vtv += A[i * n + k] * A[i * n + k];
        if (vtv < 1e-300)
            continue;
        double tau = 2.0 * v0 * v0 / vtv;
        v[k] = 1.0;
        for (int i = k + 1; i < n; ++i)
            v[i] = A[i * n + k] / v0;

        for (int j = k + 1; j < n; ++j) {
            double dot = 0.0;
            for (int base = k; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)n);
                svfloat64_t vv = svld1_f64(pg, v + base);
                svfloat64_t aa = svld1_gather_u64offset_f64(
                        pg, A + (size_t)base * n + j,
                        svld1_u64(pg, gofs));
                dot += svaddv_f64(pg, svmul_f64_x(pg, vv, aa));
            }
            dot *= tau;
            for (int base = k; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)n);
                svfloat64_t vv = svld1_f64(pg, v + base);
                svfloat64_t aa = svld1_gather_u64offset_f64(
                        pg, A + (size_t)base * n + j,
                        svld1_u64(pg, gofs));
                svst1_scatter_u64offset_f64(
                        pg, A + (size_t)base * n + j,
                        svld1_u64(pg, gofs),
                        svmls_f64_x(pg, aa, vv, svdup_f64(dot)));
            }
        }
        A[k * n + k] = alpha;
        for (int i = k + 1; i < n; ++i)
            A[i * n + k] = 0.0;

        if (k + 2 >= n)
            break;
        norm2 = 0.0;
        for (int j = k + 1; j < n; ++j)
            norm2 += A[k * n + j] * A[k * n + j];
        if (norm2 < 1e-300)
            continue;
        alpha = sqrt(norm2);
        v0 = A[k * n + k + 1] - alpha;
        vtv = v0 * v0;
        for (int j = k + 2; j < n; ++j)
            vtv += A[k * n + j] * A[k * n + j];
        if (vtv < 1e-300)
            continue;
        tau = 2.0 * v0 * v0 / vtv;
        w[k + 1] = 1.0;
        for (int j = k + 2; j < n; ++j)
            w[j] = A[k * n + j] / v0;

        for (int i = k + 1; i < n; ++i) {
            double dot = 0.0;
            for (int base = k + 1; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)n);
                svfloat64_t ww = svld1_f64(pg, w + base);
                svfloat64_t aa = svld1_f64(pg, A + i * n + base);
                dot += svaddv_f64(pg, svmul_f64_x(pg, ww, aa));
            }
            dot *= tau;
            for (int base = k + 1; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)n);
                svfloat64_t ww = svld1_f64(pg, w + base);
                svfloat64_t aa = svld1_f64(pg, A + i * n + base);
                svst1_f64(pg, A + i * n + base,
                          svmls_f64_x(pg, aa, ww, svdup_f64(dot)));
            }
        }
        A[k * n + k + 1] = alpha;
        for (int j = k + 2; j < n; ++j)
            A[k * n + j] = 0.0;
    }
}

/* ==== end verbatim ==== */

static const double kTolRel = 1e-9;

/* bidiag_compare_or_fail judgment, adapted to counting + diagnostics. */
static void bidiag_compare(const double *got, const double *golden,
                           int n, const char *tag, long iter)
{
    double norm_g2 = 0.0, max_diff = 0.0;
    int worst = 0;
    for (int i = 0; i < n * n; ++i) {
        norm_g2 += golden[i] * golden[i];
        if (!isfinite(got[i]) || !isfinite(golden[i])) {
            fprintf(stderr, "FAIL %s iter %ld: non-finite at (%d, %d): "
                            "got %.17g golden %.17g\n",
                    tag, iter, i / n, i % n, got[i], golden[i]);
            ++g_fails;
            return;
        }
        double diff = fabs(got[i] - golden[i]);
        if (diff > max_diff) {
            max_diff = diff;
            worst = i;
        }
    }
    double tol = kTolRel * sqrt(norm_g2);
    printf("  [%s iter %ld] max_diff=%.3e tol=%.3e (%s)\n",
           tag, iter, max_diff, tol, max_diff <= tol ? "PASS" : "FAIL");
    if (!(max_diff <= tol)) {
        fprintf(stderr, "FAIL %s iter %ld: diverged at (%d, %d): "
                        "|%.17g - %.17g| = %.3e > tol %.3e\n",
                tag, iter, worst / n, worst % n, got[worst], golden[worst],
                max_diff, tol);
        ++g_fails;
    }
}

/* fault injection: flip the mantissa LSB of one diagonal element after the
 * DUT kernel — deterministic, ~1 ULP, never touches init-role outputs. */
static int g_fault_on;
static int g_fault_idx;
static void flip_lsb(double *p)
{
    uint64_t b;
    memcpy(&b, p, sizeof b);
    b ^= 1ULL;
    memcpy(p, &b, sizeof b);
}
static void dut_kernel(double *A, int n, double *v, double *w,
                       const uint64_t *gofs)
{
    bidiagonalize_sve(A, n, v, w, gofs);
    if (g_fault_on)
        flip_lsb(&A[g_fault_idx]);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "new";
    const char *fault = argc > 2 ? argv[2] : "none";
    int n = argc > 3 ? atoi(argv[3]) : 64;
    int iters = argc > 4 ? atoi(argv[4]) : 8;
    uint64_t seed = argc > 5 ? strtoull(argv[5], NULL, 0) : 1;
    const int new_ch = !strcmp(mode, "new");
    g_fault_on = !strcmp(fault, "tiny");
    g_fault_idx = (n / 2) * (n + 1); /* diagonal element (n/2, n/2) */
    sm64_state = seed;

    if (n < 4 || n > 512) {
        fprintf(stderr, "n out of range\n");
        return 2;
    }

    size_t bytes = round_up_64((size_t)n * n * sizeof(double));
    double *orig = (double *)aligned_alloc(64, bytes);
    double *golden = (double *)aligned_alloc(64, bytes);
    double *golden_sve = (double *)aligned_alloc(64, bytes);
    double *l_orig = (double *)aligned_alloc(64, bytes);
    double *l_golden = (double *)aligned_alloc(64, bytes);
    double *work = (double *)aligned_alloc(64, bytes);
    double *work2 = (double *)aligned_alloc(64, bytes);
    double *hv = (double *)aligned_alloc(64, round_up_64((size_t)n * sizeof(double)));
    double *hw = (double *)aligned_alloc(64, round_up_64((size_t)n * sizeof(double)));
    uint64_t *gofs = (uint64_t *)aligned_alloc(64,
            round_up_64(svcntd() * sizeof(uint64_t)));
    if (!orig || !golden || !golden_sve || !l_orig || !l_golden || !work ||
        !work2 || !hv || !hw || !gofs) {
        fprintf(stderr, "alloc failure\n");
        return 2;
    }
    for (uint64_t l = 0; l < svcntd(); ++l)
        gofs[l] = l * (uint64_t)n * sizeof(double);

    /* ---- init role (control thread: scalar golden; new: SVE golden) ---- */
    for (int i = 0; i < n * n; ++i)
        orig[i] = random_pm_one();
    memcpy(golden, orig, bytes);
    bidiagonalize_ref(golden, n);
    if (new_ch) {
        memcpy(golden_sve, orig, bytes);
        bidiagonalize_sve(golden_sve, n, hv, hw, gofs); /* no fault: healthy core */
    }

    /* ---- run role (worker): test's alternation, fracture-safe ---- */
    const int kRerollEvery = 16;
    int since_reroll = kRerollEvery; /* first reroll round re-rolls */
    for (long iter = 0; iter < iters; ++iter) {
        if (iter & 1) {
            /* frozen replay */
            memcpy(work, orig, bytes);
            dut_kernel(work, n, hv, hw, gofs);
            bidiag_compare(work, golden, n, "replay", iter);
            if (new_ch && memcmp(work, golden_sve, bytes) != 0) {
                for (int i = 0; i < n * n; ++i) {
                    if (memcmp(&work[i], &golden_sve[i], sizeof(double)) != 0) {
                        fprintf(stderr, "FAIL replay iter %ld: byte-exact "
                                        "deviation from init-thread SVE golden "
                                        "at (%d, %d): worker %.17g vs init "
                                        "%.17g (issue #484 channel)\n",
                                iter, i / n, i % n, work[i], golden_sve[i]);
                        ++g_fails;
                        break;
                    }
                }
            }
        } else {
            /* reroll + double-pass self-consistency */
            if (++since_reroll >= kRerollEvery) {
                since_reroll = 0;
                for (int i = 0; i < n * n; ++i)
                    l_orig[i] = random_pm_one();
                memcpy(l_golden, l_orig, bytes);
                bidiagonalize_ref(l_golden, n);
            }
            memcpy(work, l_orig, bytes);
            dut_kernel(work, n, hv, hw, gofs);
            bidiag_compare(work, l_golden, n, "reroll", iter);

            memcpy(work2, l_orig, bytes);
            dut_kernel(work2, n, hv, hw, gofs);
            if (memcmp(work, work2, bytes) != 0) {
                for (int i = 0; i < n * n; ++i) {
                    if (memcmp(&work[i], &work2[i], sizeof(double)) != 0) {
                        fprintf(stderr, "FAIL reroll iter %ld: not repeatable "
                                        "at (%d, %d): pass1 %.17g vs pass2 "
                                        "%.17g\n",
                                iter, i / n, i % n, work[i], work2[i]);
                        ++g_fails;
                        break;
                    }
                }
            }
        }
    }

    printf("MODE=%s FAULT=%s VL=%d n=%d iters=%d fault_elem=(%d,%d) "
           "fault_val=%.17g\n",
           mode, fault, (int)(svcntd() * 64), n, iters,
           g_fault_idx / n, g_fault_idx % n, golden[g_fault_idx]);

    uint64_t f;
    fnv_buf(golden, n); f = fnv; printf("FNV_GOLDEN=%016llx\n", (unsigned long long)f);
    if (new_ch) { fnv_buf(golden_sve, n); f = fnv; printf("FNV_GOLDEN_SVE=%016llx\n", (unsigned long long)f); }
    fnv_buf(work, n); f = fnv; printf("FNV_WORK_LAST=%016llx\n", (unsigned long long)f);

    printf("SDCSHIELD_FAILS=%u\n", g_fails);
    free(orig); free(golden); free(golden_sve); free(l_orig); free(l_golden);
    free(work); free(work2); free(hv); free(hw); free(gofs);
    return g_fails ? 1 : 0;
}
