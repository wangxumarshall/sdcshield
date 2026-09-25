/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_add_sve
 * @parblock
 * SVE port of neon_add: repeated array addition on SVE vector lanes
 * (svld1/svadd/svst1) instead of NEON vaddq. Golden precomputed in init
 * on the same SVE units (same pattern as the original); every run
 * recomputes and memcmp-compares.
 * @endparblock
 */

#include "sandstone.h"

#ifdef __aarch64__
#include <arm_sve.h>
#include <sys/auxv.h>
#include <cstdlib>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

#define SVE_ADD_ELEMENTS (1u << 10)
#define SVE_ADD_BUF_SIZE (SVE_ADD_ELEMENTS * sizeof(uint32_t))

struct neon_add_sve_t_ {
        uint32_t *a;
        uint32_t *b;
        uint32_t *golden;
};
typedef struct neon_add_sve_t_ neon_add_sve_t;

#ifdef __aarch64__
static void prv_do_add(const uint32_t *a, const uint32_t *b, uint32_t *res)
{
        /* VL 无关: svcntw() 分批 + whilelt 尾部 */
        for (size_t base = 0; base < SVE_ADD_ELEMENTS; base += svcntw()) {
                svbool_t pg = svwhilelt_b32((uint64_t)base, (uint64_t)SVE_ADD_ELEMENTS);
                svuint32_t r1 = svld1_u32(pg, a + base);
                svuint32_t r2 = svld1_u32(pg, b + base);
                svst1_u32(pg, res + base, svadd_u32_x(pg, r1, r2));
        }
}
#endif

static int neon_add_sve_init(struct test *test)
{
#ifdef __aarch64__
        unsigned long hwcap = getauxval(AT_HWCAP);
        if ((hwcap & HWCAP_SVE) == 0) {
                log_skip(CpuNotSupportedSkipCategory,
                         "to be implemented (placeholder): ARM SVE required for neon_add_sve");
                return EXIT_SKIP;
        }

        neon_add_sve_t *na = static_cast<neon_add_sve_t *>(malloc(sizeof(*na)));

        na->a = static_cast<uint32_t *>(aligned_alloc(64, SVE_ADD_BUF_SIZE));
        na->b = static_cast<uint32_t *>(aligned_alloc(64, SVE_ADD_BUF_SIZE));
        na->golden = static_cast<uint32_t *>(aligned_alloc_safe(64, SVE_ADD_BUF_SIZE));

        memset_random(na->a, SVE_ADD_BUF_SIZE);
        memset_random(na->b, SVE_ADD_BUF_SIZE);
        prv_do_add(na->a, na->b, na->golden);

        test->data = na;

        return EXIT_SUCCESS;
#else
        return EXIT_SUCCESS;
#endif
}

static int neon_add_sve_run(struct test *test, int cpu)
{
#ifdef __aarch64__
        neon_add_sve_t *na = static_cast<neon_add_sve_t *>(test->data);
        uint32_t *res = static_cast<uint32_t *>(aligned_alloc(64, SVE_ADD_BUF_SIZE));

        TEST_LOOP(test, 1 << 13) {
                memset(res, 0, SVE_ADD_BUF_SIZE);
                prv_do_add(na->a, na->b, res);
                memcmp_or_fail(res, na->golden, SVE_ADD_ELEMENTS);
        }

        free(res);

        return EXIT_SUCCESS;
#else
        return EXIT_SUCCESS;
#endif
}

static int neon_add_sve_cleanup(struct test *test)
{
#ifdef __aarch64__
        neon_add_sve_t *na = static_cast<neon_add_sve_t *>(test->data);

        if (na) {
                free(na->golden);
                free(na->b);
                free(na->a);
                free(na);
        }
#endif

        return EXIT_SUCCESS;
}

DECLARE_TEST(neon_add_sve, "Repeatedly add arrays of unsigned integers using ARM SVE instructions (port of neon_add)")
        .test_init = neon_add_sve_init,
        .test_run = neon_add_sve_run,
        .test_cleanup = neon_add_sve_cleanup,
        .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
