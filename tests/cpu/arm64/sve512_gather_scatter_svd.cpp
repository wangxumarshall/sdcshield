/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sve512_gather_scatter_svd
 * @parblock
 * SVD-format gather/scatter round-trip with SVD-scale working set (1.44 MB)
 * and 2-D block index pattern matching Eigen BDCSVD's block-of-matrix access.
 *
 * Differs from sve512_gather_scatter_arm: working set 1.44 MB vs 4 KB, 2-D
 * block index table (row-major + column offsets) vs 1-D linear permutation.
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

static constexpr size_t SVD_ROWS = 300;
static constexpr size_t SVD_COLS = 300;
static constexpr size_t SVD_BLOCK_ROWS = 16;
static constexpr size_t SVD_BLOCK_COLS = 16;

static inline uint64_t splitmix64(uint64_t x)
{
    uint64_t z = (x + 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

struct SveGatherScatterSvdData {
    size_t vl_d;
    std::vector<uint64_t> seeds_f64;
    std::vector<uint64_t> gather_src;
    std::vector<uint64_t> gather_idx;
    std::vector<uint64_t> scatter_dst;
    std::vector<uint64_t> scatter_golden;
};

#ifdef __aarch64__

static void gather_scatter_golden(SveGatherScatterSvdData *d)
{
    for (size_t i = 0; i < d->gather_idx.size(); ++i) {
        double v;
        memcpy(&v, &d->gather_src[d->gather_idx[i]], 8);
        v = std::fma(v, v, 1.5);
        memcpy(&d->scatter_golden[d->gather_idx[i]], &v, 8);
    }
}

static void gather_scatter_hw(SveGatherScatterSvdData *d)
{
    const size_t n = d->gather_idx.size();
    const size_t lanes = d->vl_d;
    const svbool_t pg = svptrue_b64();
    const svfloat64_t one_half = svdup_f64(1.5);
    for (size_t off = 0; off < n; off += lanes) {
        svuint64_t vidx = svld1_u64(pg, &d->gather_idx[off]);
        svfloat64_t v = svld1_gather_u64index_f64(
            pg, reinterpret_cast<const double *>(d->gather_src.data()), vidx);
        v = svmla_f64_x(pg, one_half, v, v);
        svst1_scatter_u64index_f64(
            pg, reinterpret_cast<double *>(d->scatter_dst.data()), vidx, v);
    }
}

static void report_f64_lane_mismatch(const char *tag, size_t lane,
                                     uint64_t golden, uint64_t actual)
{
    log_warning("%s: lane %zu golden=0x%016" PRIx64 " "
                "actual=0x%016" PRIx64 " xor=0x%016" PRIx64,
                tag, lane, golden, actual, golden ^ actual);
}

static int sve512_gather_scatter_svd_init(struct test *test)
{
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "ARM64 SVE not available; sve512_gather_scatter_svd requires SVE");
        return EXIT_SKIP;
    }
    try {
        auto data = std::make_unique<SveGatherScatterSvdData>();
        data->vl_d = svcntd();

        data->seeds_f64.resize(data->vl_d);
        for (size_t lane = 0; lane < data->vl_d; ++lane) {
            data->seeds_f64[lane] = 0x3FF0000000000000ULL |
                (splitmix64(0xC0FFEE00ULL + lane) & 0x000FFFFFFFFFFFFFULL);
        }

        // SVD-scale gather/scatter workspace: 300x300 doubles = 720 KB.
        const size_t gn = SVD_ROWS * SVD_COLS;
        data->gather_src.resize(gn);
        data->gather_idx.resize(gn);
        data->scatter_dst.assign(gn, 0);
        data->scatter_golden.resize(gn);

        for (size_t i = 0; i < gn; ++i) {
            data->gather_src[i] = data->seeds_f64[i % data->vl_d];
            data->gather_idx[i] = i;
        }

        // SVD-format block permutation: shuffle within each 16x16 block,
        // preserving the 2-D locality of BDCSVD's block traversal.
        for (size_t br = 0; br < SVD_ROWS; br += SVD_BLOCK_ROWS) {
            for (size_t bc = 0; bc < SVD_COLS; bc += SVD_BLOCK_COLS) {
                for (size_t k = 0; k < SVD_BLOCK_ROWS * SVD_BLOCK_COLS; ++k) {
                    size_t i = k;
                    size_t j = (size_t)(splitmix64(0xBEEF0000ULL +
                                                   (br * SVD_COLS + bc) * 256 +
                                                   k) % (k + 1));
                    size_t row_i = br + i / SVD_BLOCK_COLS;
                    size_t col_i = bc + i % SVD_BLOCK_COLS;
                    size_t row_j = br + j / SVD_BLOCK_COLS;
                    size_t col_j = bc + j % SVD_BLOCK_COLS;
                    uint64_t t = data->gather_idx[row_i * SVD_COLS + col_i];
                    data->gather_idx[row_i * SVD_COLS + col_i] =
                        data->gather_idx[row_j * SVD_COLS + col_j];
                    data->gather_idx[row_j * SVD_COLS + col_j] = t;
                }
            }
        }

        test->data = data.release();
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        log_skip(TestResourceIssueSkipCategory,
                 "sve512_gather_scatter_svd init: %s", e.what());
        return EXIT_SKIP;
    }
}

static int sve512_gather_scatter_svd_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<SveGatherScatterSvdData *>(test->data);
    do {
        bool all_passed = true;
        gather_scatter_hw(d);
        gather_scatter_golden(d);
        for (size_t i = 0; i < d->scatter_dst.size(); ++i) {
            if (d->scatter_dst[i] != d->scatter_golden[i]) {
                report_f64_lane_mismatch("sve512_gather_scatter_svd", i,
                                         d->scatter_golden[i],
                                         d->scatter_dst[i]);
                all_passed = false;
            }
        }
        std::fill(d->scatter_dst.begin(), d->scatter_dst.end(), 0);
        if (!all_passed) {
            report_fail_msg("sve512_gather_scatter_svd: SDC detected");
            return EXIT_FAILURE;
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int sve512_gather_scatter_svd_cleanup(struct test *test)
{
    delete static_cast<SveGatherScatterSvdData *>(test->data);
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(sve512_gather_scatter_svd,
             "SVD-format SVE-512 gather/scatter round-trip: 1.44 MB working "
             "set + 2-D block index permutation, byte-exact vs scalar golden")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = sve512_gather_scatter_svd_init,
    .test_run = sve512_gather_scatter_svd_run,
    .test_cleanup = sve512_gather_scatter_svd_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
