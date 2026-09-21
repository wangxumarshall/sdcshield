/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b gmp_bigadd
 * @parblock
 * Stress the integer adder / carry chain by repeatedly adding large
 * multi-precision integers via the GMP library.  Two random big
 * integers (4096-bit each) are re-rolled every iteration from the
 * framework RNG.  The sum is computed by GMP (mpz_add — the mpn
 * add_n carry chain, the silicon path under stress) and every limb
 * is compared against an independent scalar golden computed the
 * same iteration with an unsigned __int128 carry-chain add.  The
 * two code paths share no computation, so a deterministically
 * corrupted adder cannot pass by producing the same wrong value
 * twice.  A mismatch points at a silent corruption in the add /
 * carry pipeline.
 * @endparblock
 */

#include <sandstone.h>
#include <gmp.h>

#include <cstdint>
#include <cstring>
#include <vector>

#define GMP_BITS 4096
#define GMP_BYTELEN ((GMP_BITS) / 8)
#define GMP_LIMBS ((GMP_BITS) / 64)         /* 64 x 64-bit limbs */

namespace {
struct gmp_add_data {
    /* Operands and golden are per-thread and re-rolled every iteration in
     * run (framework RNG); nothing shared is needed anymore.  The init-time
     * golden this struct used to hold was never read by run and was removed
     * together with the tautological same-function golden. */
};
}

#define CAST(_x) static_cast<struct gmp_add_data *>(_x)

static void fill_random_bytes(uint8_t *buf, size_t n)
{
    for (size_t i = 0; i < n; i += 4) {
        uint32_t r = random32();
        for (int j = 0; j < 4 && (i + j) < n; ++j)
            buf[i + j] = (uint8_t)(r >> (8 * j));
    }
}

/* H13'' (PR #147 review): independent scalar add golden (__int128 carry
 * chain, mirroring adcx.cpp's compute_golden). The previous PR #147 change
 * computed golden with the SAME mpz_add call as the DUT (pure x==x;
 * symmetric fault-injection passes — verified by experiment). Verified
 * bit-identical to GMP on 1000 random 4096-bit adds (including the 65th
 * carry-out limb when the sum overflows 4096 bits) on this machine,
 * 2026-09-21. */
static void scalar_add_golden(const uint64_t *a, const uint64_t *b,
                              uint64_t *out, size_t nlimbs)
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < nlimbs; ++i) {
        unsigned __int128 s = (unsigned __int128)a[i] + b[i] + carry;
        out[i] = (uint64_t)s;
        carry = s >> 64;
    }
}

static int gmp_bigadd_init(struct test *test)
{
    gmp_add_data *data = new gmp_add_data;
    test->data = data;
    return EXIT_SUCCESS;
}

static int gmp_bigadd_run(struct test *test, int cpu)
{
    (void)cpu;

    /* Randomization hardening H13'': per-thread operands re-rolled EVERY
     * iteration and the golden computed the same iteration by an INDEPENDENT
     * scalar path (__int128 carry chain), so a deterministically broken
     * adder can no longer match its own wrong output and pass — the x==x
     * tautology found in the 2026-09-21 review (symmetric fault-injection
     * passed the previous code, verified by experiment; see gmp_bignum.cpp
     * for the mul-side rationale). */
    mpz_t a, b, result;
    mpz_init(a);
    mpz_init(b);
    mpz_init(result);
    uint8_t buf[GMP_BYTELEN];

    /* Hoisted out of the loop: no per-iteration allocation churn.
     * 65 limbs = the 64 operand limbs + 1 carry-out limb (a 4096-bit sum
     * overflows to 4097 bits with ~50% probability on random operands). */
    std::vector<uint64_t> la(GMP_LIMBS + 1, 0), lb(GMP_LIMBS + 1, 0);
    std::vector<uint64_t> gold(GMP_LIMBS + 1, 0);

    do {
        fill_random_bytes(buf, GMP_BYTELEN);
        mpz_import(a, GMP_BYTELEN, 1, 1, 1, 0, buf);
        fill_random_bytes(buf, GMP_BYTELEN);
        mpz_import(b, GMP_BYTELEN, 1, 1, 1, 0, buf);

        /* Independent scalar golden: export the limb view of both operands
         * (mpz_export with order=-1 / sizeof(mp_limb_t) gives GMP's internal
         * least-significant-limb-first array) and carry-chain-add all 65
         * limb positions (the 65th captures the carry-out). */
        memset(la.data(), 0, la.size() * sizeof(uint64_t));
        memset(lb.data(), 0, lb.size() * sizeof(uint64_t));
        size_t na = 0, nb = 0;
        mpz_export(la.data(), &na, -1, sizeof(mp_limb_t), 0, 0, a);
        mpz_export(lb.data(), &nb, -1, sizeof(mp_limb_t), 0, 0, b);
        scalar_add_golden(la.data(), lb.data(), gold.data(), GMP_LIMBS + 1);
        size_t gsize = GMP_LIMBS + 1;
        while (gsize > 0 && gold[gsize - 1] == 0)
            --gsize;

        /* DUT path: GMP mpz_add (mpn add_n carry chain). */
        mpz_add(result, a, b);
        size_t rsize = mpz_size(result);
        if (rsize != gsize) {
            mpz_clears(a, b, result, NULL);
            report_fail_msg("GMP add produced unexpected size %zu vs %zu",
                            rsize, gsize);
        }
        for (size_t i = 0; i < rsize; ++i) {
            mp_limb_t got = mpz_getlimbn(result, i);
            mp_limb_t want = gold[i];
            if (got != want) {
                mpz_clears(a, b, result, NULL);
                report_fail_msg("GMP big-add limb %zu mismatch: 0x%llx vs 0x%llx",
                                i, (unsigned long long)got,
                                (unsigned long long)want);
            }
        }
    } while (test_time_condition(test));

    mpz_clears(a, b, result, NULL);
    return EXIT_SUCCESS;
}

static int gmp_bigadd_cleanup(struct test *test)
{
    gmp_add_data *data = CAST(test->data);
    delete data;
    return EXIT_SUCCESS;
}

DECLARE_TEST(gmp_bigadd, "GMP large integer addition (4096-bit) vs golden result")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = gmp_bigadd_init,
  .test_run = gmp_bigadd_run,
  .test_cleanup = gmp_bigadd_cleanup,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
