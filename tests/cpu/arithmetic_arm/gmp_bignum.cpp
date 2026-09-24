/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b gmp_bignum
 * @parblock
 * Stress the integer execution units by repeatedly multiplying
 * large multi-precision integers via the third-party GMP library.
 * Two random big integers (4096-bit each) are re-rolled every
 * iteration from the framework RNG.  The product is computed by
 * GMP (mpz_mul — the mpn / UMULL 64x64->128 kernels, the silicon
 * path under stress) and every limb is compared against an
 * independent scalar golden computed the same iteration with an
 * unsigned __int128 schoolbook multiply.  The two code paths share
 * no computation, so a deterministically corrupted multiply unit
 * cannot pass by producing the same wrong value twice.  Any
 * mismatch indicates a silent data corruption in the ALU /
 * multiply pipeline of the core running the thread.
 * @endparblock
 */

#include <sandstone.h>
#include <gmp.h>

#include <cstdint>
#include <cstring>
#include <vector>

#define GMP_BITS 4096
#define GMP_BYTELEN ((GMP_BITS) / 8)        /* 512 bytes */
#define GMP_LIMBS ((GMP_BITS) / 64)         /* 64 x 64-bit limbs */

namespace {
struct gmp_data {
    /* Operands and golden are per-thread and re-rolled every iteration in
     * run (framework RNG); nothing shared is needed anymore.  The init-time
     * golden this struct used to hold was never read by run and was removed
     * together with the tautological same-function golden. */
};
}

#define CAST(_x) static_cast<struct gmp_data *>(_x)

/* fill a buffer with random bytes derived from random32() */
static void fill_random_bytes(uint8_t *buf, size_t n)
{
    for (size_t i = 0; i < n; i += 4) {
        uint32_t r = random32();
        for (int j = 0; j < 4 && (i + j) < n; ++j)
            buf[i + j] = (uint8_t)(r >> (8 * j));
    }
}

/* H13'' (PR #147 review): independent scalar golden — __int128 schoolbook
 * multiply, a different code path from GMP's mpn/UMULL kernels. The previous
 * PR #147 change computed golden with the SAME mpz_mul call as the DUT (pure
 * x==x; symmetric fault-injection passes — verified by experiment). Verified
 * bit-identical to GMP on 200 random 4096x4096 inputs (full 128-limb
 * product) on this machine, 2026-09-21. */
static void schoolbook_mul_golden(const uint64_t *a, const uint64_t *b,
                                  uint64_t *out, size_t nlimbs)
{
    std::vector<uint64_t> acc(2 * nlimbs, 0);
    for (size_t i = 0; i < nlimbs; ++i) {
        unsigned __int128 carry = 0;
        for (size_t j = 0; j < nlimbs; ++j) {
            unsigned __int128 cur = (unsigned __int128)a[i] * b[j] + acc[i + j] + carry;
            acc[i + j] = (uint64_t)cur;
            carry = cur >> 64;
        }
        acc[i + nlimbs] += (uint64_t)carry;
    }
    memcpy(out, acc.data(), 2 * nlimbs * sizeof(uint64_t));
}

static int gmp_bignum_init(struct test *test)
{
    gmp_data *data = new gmp_data;
    test->data = data;
    return EXIT_SUCCESS;
}

static int gmp_bignum_run(struct test *test, int cpu)
{
    (void)cpu;

    /* Randomization hardening H13'': per-thread operands re-rolled EVERY
     * iteration (framework RNG via fill_random_bytes — per-thread stream,
     * -s reproducible) and the golden computed the same iteration by an
     * INDEPENDENT scalar path (__int128 schoolbook), so a deterministically
     * broken multiply unit can no longer match its own wrong output and
     * pass — the x==x tautology found in the 2026-09-21 review (symmetric
     * fault-injection passed the previous code, verified by experiment). */
    mpz_t a, b, result;
    mpz_init(a);
    mpz_init(b);
    mpz_init(result);
    uint8_t buf[GMP_BYTELEN];

    /* Hoisted out of the loop: no per-iteration allocation churn. */
    std::vector<uint64_t> la(GMP_LIMBS, 0), lb(GMP_LIMBS, 0);
    std::vector<uint64_t> gold(2 * GMP_LIMBS, 0);

    do {
        fill_random_bytes(buf, GMP_BYTELEN);
        /* order=1 (most significant word first), size=1, endian=little, nails=0 */
        mpz_import(a, GMP_BYTELEN, 1, 1, 1, 0, buf);
        fill_random_bytes(buf, GMP_BYTELEN);
        mpz_import(b, GMP_BYTELEN, 1, 1, 1, 0, buf);

        /* Independent scalar golden: export the limb view of both operands
         * (mpz_export with order=-1 / sizeof(mp_limb_t) gives GMP's internal
         * least-significant-limb-first array; both directions go through the
         * mpz_t so the VALUE is identical to what mpz_mul sees) and
         * schoolbook-multiply into the full 128-limb product. */
        memset(la.data(), 0, la.size() * sizeof(uint64_t));
        memset(lb.data(), 0, lb.size() * sizeof(uint64_t));
        size_t na = 0, nb = 0;
        mpz_export(la.data(), &na, -1, sizeof(mp_limb_t), 0, 0, a);
        mpz_export(lb.data(), &nb, -1, sizeof(mp_limb_t), 0, 0, b);
        schoolbook_mul_golden(la.data(), lb.data(), gold.data(), GMP_LIMBS);
        size_t gsize = 2 * GMP_LIMBS;
        while (gsize > 0 && gold[gsize - 1] == 0)
            --gsize;

        /* DUT path: GMP mpz_mul (mpn / UMULL 64x64->128 kernels). */
        mpz_mul(result, a, b);
        size_t rsize = mpz_size(result);
        if (rsize != gsize) {
            mpz_clears(a, b, result, NULL);
            report_fail_msg("GMP mul produced unexpected size %zu vs %zu",
                            rsize, gsize);
        }
        for (size_t i = 0; i < rsize; ++i) {
            mp_limb_t got = mpz_getlimbn(result, i);
            mp_limb_t want = gold[i];
            if (got != want) {
                mpz_clears(a, b, result, NULL);
                report_fail_msg("GMP big-mul limb %zu mismatch: 0x%llx vs 0x%llx",
                                i, (unsigned long long)got,
                                (unsigned long long)want);
            }
        }
    } while (test_time_condition(test));

    mpz_clears(a, b, result, NULL);
    return EXIT_SUCCESS;
}

static int gmp_bignum_cleanup(struct test *test)
{
    gmp_data *data = CAST(test->data);
    delete data;
    return EXIT_SUCCESS;
}

DECLARE_TEST(gmp_bignum, "GMP large integer multiplication (4096-bit) vs golden result")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = gmp_bignum_init,
  .test_run = gmp_bignum_run,
  .test_cleanup = gmp_bignum_cleanup,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
