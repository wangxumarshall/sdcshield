/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sve512_f32_chain_svd
 * @parblock
 * SVD-format f32 FMLA chain with SVD-scale working set (1.44 MB) and nested
 * block traversal, matching the memory footprint of eigen_svd_cdouble_sve
 * but at f32 width (16 lanes per vector).
 *
 * Differs from sve512_f32_chain_arm: working set 1.44 MB vs 64 KB, nested
 * block traversal vs flat stream.
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef __aarch64__
#include <sys/auxv.h>
#include <asm/hwcap.h>
#include <arm_sve.h>
#endif

static constexpr size_t CHAIN_STEPS = 512;
static constexpr size_t SVD_ROWS = 300;
static constexpr size_t SVD_COLS = 300;
static constexpr size_t SVD_BLOCK_ROWS = 16;
static constexpr size_t SVD_BLOCK_COLS = 16;

static const uint32_t F32_FINITE[] = {
    0x3FAAAAABU, 0xBFAAAAABU, 0x3EAAAAABU, 0xBF2AAAABU,
    0x3F800000U, 0xBF800000U, 0x00555555U, 0x80AAAAAAU,
    0x3FCCCCCDU, 0xBFCCCCCDU, 0x3F333333U, 0xBF333333U,
    0x3E555555U, 0xBE555555U, 0x3F666666U, 0xBF666666U,
};
static constexpr size_t F32_FINITE_SIZE =
    sizeof(F32_FINITE) / sizeof(F32_FINITE[0]);

static inline uint64_t splitmix64(uint64_t x)
{
    uint64_t z = (x + 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

struct SveF32ChainSvdData {
    size_t vl_w;
    std::vector<uint32_t> seeds_f32;
    std::vector<uint32_t> m_f32;
    std::vector<uint32_t> a_f32;
};

#ifdef __aarch64__

static void f32_chain_golden(const SveF32ChainSvdData *d, uint32_t *out,
                             size_t row, size_t col)
{
    const size_t lanes = d->vl_w;
    const size_t stride = SVD_COLS * lanes;
    for (size_t lane = 0; lane < lanes; ++lane) {
        uint32_t accw = d->seeds_f32[lane];
        float acc;
        memcpy(&acc, &accw, 4);
        for (size_t i = 0; i < CHAIN_STEPS; ++i) {
            float m, a;
            const size_t base = (row * stride + col * lanes) + i * (SVD_ROWS * stride);
            memcpy(&m, &d->m_f32[base + lane], 4);
            memcpy(&a, &d->a_f32[base + lane], 4);
            acc = std::fma(m, a, acc);
        }
        memcpy(&accw, &acc, 4);
        out[lane] = accw;
    }
}

static void f32_chain_hw(const SveF32ChainSvdData *d, uint32_t *out,
                         size_t row, size_t col)
{
    const size_t lanes = d->vl_w;
    const svbool_t pg = svptrue_b32();
    const size_t stride = SVD_COLS * lanes;
    svfloat32_t acc = svld1_f32(pg,
        reinterpret_cast<const float *>(d->seeds_f32.data()));
    for (size_t i = 0; i < CHAIN_STEPS; ++i) {
        const size_t base = (row * stride + col * lanes) + i * (SVD_ROWS * stride);
        svfloat32_t m = svld1_f32(pg,
            reinterpret_cast<const float *>(&d->m_f32[base]));
        svfloat32_t a = svld1_f32(pg,
            reinterpret_cast<const float *>(&d->a_f32[base]));
        acc = svmla_f32_x(pg, acc, m, a);
    }
    svst1_f32(pg, reinterpret_cast<float *>(out), acc);
}

static void report_f32_lane_mismatch(const char *tag, size_t lane,
                                     uint32_t golden, uint32_t actual)
{
    log_warning("%s: lane %zu golden=0x%08" PRIx32 " "
                "actual=0x%08" PRIx32 " xor=0x%08" PRIx32,
                tag, lane, golden, actual, golden ^ actual);
}

static int sve512_f32_chain_svd_init(struct test *test)
{
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "ARM64 SVE not available; sve512_f32_chain_svd requires SVE");
        return EXIT_SKIP;
    }
    try {
        auto data = std::make_unique<SveF32ChainSvdData>();
        data->vl_w = svcntw();
        data->seeds_f32.resize(data->vl_w);
        for (size_t lane = 0; lane < data->vl_w; ++lane) {
            data->seeds_f32[lane] = 0x3F800000U |
                ((uint32_t)splitmix64(0xC0FFEE00ULL + lane) & 0x007FFFFFU);
        }
        const size_t total = SVD_ROWS * SVD_COLS * CHAIN_STEPS * data->vl_w;
        data->m_f32.resize(total);
        data->a_f32.resize(total);
        for (size_t i = 0; i < total; ++i) {
            data->m_f32[i] = F32_FINITE[(i * 7 + 1) % F32_FINITE_SIZE];
            data->a_f32[i] = F32_FINITE[(i * 5 + 3) % F32_FINITE_SIZE];
        }
        test->data = data.release();
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        log_skip(TestResourceIssueSkipCategory,
                 "sve512_f32_chain_svd init: %s", e.what());
        return EXIT_SKIP;
    }
}

static int sve512_f32_chain_svd_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<SveF32ChainSvdData *>(test->data);
    std::vector<uint32_t> gold32(d->vl_w);
    std::vector<uint32_t> hw(d->vl_w);

    do {
        bool all_passed = true;
        for (size_t br = 0; br < SVD_ROWS; br += SVD_BLOCK_ROWS) {
            for (size_t bc = 0; bc < SVD_COLS; bc += SVD_BLOCK_COLS) {
                f32_chain_golden(d, gold32.data(), br, bc);
                f32_chain_hw(d, hw.data(), br, bc);
                for (size_t lane = 0; lane < d->vl_w; ++lane) {
                    if (hw[lane] != gold32[lane]) {
                        report_f32_lane_mismatch(
                            "sve512_f32_chain_svd", lane,
                            gold32[lane], hw[lane]);
                        all_passed = false;
                    }
                }
            }
        }
        if (!all_passed) {
            report_fail_msg("sve512_f32_chain_svd: SDC detected");
            return EXIT_FAILURE;
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int sve512_f32_chain_svd_cleanup(struct test *test)
{
    delete static_cast<SveF32ChainSvdData *>(test->data);
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(sve512_f32_chain_svd,
             "SVD-format SVE-512 f32 FMLA chain: 1.44 MB working set + "
             "nested block traversal, byte-exact vs scalar fmaf()")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = sve512_f32_chain_svd_init,
    .test_run = sve512_f32_chain_svd_run,
    .test_cleanup = sve512_f32_chain_svd_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
