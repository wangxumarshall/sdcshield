/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sve512_gather_scatter_arm
 * @parblock
 * SVE full-vector-length gather/scatter SDC stress — standalone extraction
 * of workload 4 from sve512_fma_arm, extended with explicit read-offset
 * verification and gather-variant coverage (SEVI ASPLOS'26: 76% of
 * memory-class SDC is a wrong-offset read — valid data from the wrong
 * address, never a crash, input-level repro ~0 — so the offset itself
 * must be asserted, not just the value).
 *
 * Drives the SVE memory pipeline at the CPU's runtime vector length:
 *   svld1_gather_u64index_f64   — gather f64 elements at element-index
 *                                 granularity from a permuted index table
 *   svmla_f64_x                 — FMA-transform each gathered element
 *                                 (v = 1.5 + v*v)
 *   svst1_scatter_u64index_f64  — scatter results to a scratch region at
 *                                 the same element indices
 *
 * Explicit read-offset assertion: the source is encoded so the value
 * reveals the index it came from (src[i] bit pattern = i, reconstructible
 * via the mantissa). After every gather, each lane's raw loaded value is
 * checked against decode(idx[lane]) BEFORE the FMA — a wrong-offset read
 * produces a decodable-but-wrong index and fails immediately.
 *
 * Gather variants (round-robin): u64index (scalar base + u64 element
 * indices — the original), u64base (per-lane base addresses in a vector,
 * svld1_gather_u64base_f64), and a wide-stride variant (indices stepped
 * by 64 elements, forcing L1 misses on the gather path — ITHICA MemDiv:
 * forcing different memory-hierarchy paths exposes faults homogeneous
 * traffic never sees).
 *
 * The scatter destination is byte-compared against a scalar golden that
 * indexes through the SAME indirection (a permutation does NOT commute
 * with the scatter, so the golden must map idx[i] -> dst, not linearly).
 *
 * test_init probes HWCAP_SVE via getauxval (no SVE instruction executes
 * before the probe), returning a clean EXIT_SKIP on SVE-less CPUs.
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

static inline uint64_t splitmix64(uint64_t x)
{
    uint64_t z = (x + 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

struct SveGatherScatterData {
    size_t vl_d;
    std::vector<uint64_t> seeds_f64;
    std::vector<uint64_t> gather_src;
    std::vector<uint64_t> gather_idx;
    std::vector<uint64_t> scatter_dst;
    std::vector<uint64_t> scatter_golden;
};

#ifdef __aarch64__

static void gather_scatter_golden(SveGatherScatterData *d)
{
    for (size_t i = 0; i < d->gather_idx.size(); ++i) {
        double v;
        memcpy(&v, &d->gather_src[d->gather_idx[i]], 8);
        v = std::fma(v, v, 1.5);
        memcpy(&d->scatter_golden[d->gather_idx[i]], &v, 8);
    }
}

static void gather_scatter_hw(SveGatherScatterData *d)
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

static int sve512_gather_scatter_arm_init(struct test *test)
{
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "ARM64 SVE not available on this CPU; "
                 "sve512_gather_scatter_arm requires SVE");
        return EXIT_SKIP;
    }

    try {
        auto data = std::make_unique<SveGatherScatterData>();
        data->vl_d = svcntd();

        data->seeds_f64.resize(data->vl_d);
        for (size_t lane = 0; lane < data->vl_d; ++lane) {
            data->seeds_f64[lane] = 0x3FF0000000000000ULL |
                (splitmix64(0xC0FFEE00ULL + lane) & 0x000FFFFFFFFFFFFFULL);
        }

        const size_t gn = 64 * data->vl_d;
        data->gather_src.resize(gn);
        data->gather_idx.resize(gn);
        data->scatter_dst.assign(gn, 0);
        data->scatter_golden.resize(gn);
        // Index-revealing encoding: src[i] = quiet [1,2) double whose low
        // 48 mantissa bits literally carry i. gather loads can be decoded
        // back to the index they actually read (SEVI wrong-offset check).
        for (size_t i = 0; i < gn; ++i) {
            data->gather_src[i] = 0x3FF0000000000000ULL | (uint64_t)i;
            data->gather_idx[i] = i;
        }
        for (size_t i = gn - 1; i > 0; --i) {
            size_t j = (size_t)(splitmix64(0xBEEF0000ULL + i) % (i + 1));
            uint64_t t = data->gather_idx[i];
            data->gather_idx[i] = data->gather_idx[j];
            data->gather_idx[j] = t;
        }

        test->data = data.release();
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        log_skip(TestResourceIssueSkipCategory,
                 "sve512_gather_scatter_arm init: %s", e.what());
        return EXIT_SKIP;
    }
}

// ---------------------------------------------------------------------------
// Explicit read-offset assertion (SEVI: 76% of memory-class SDC is a
// wrong-offset read): the value encoding carries its own index, so a lane
// that read the wrong address decodes to the wrong index.
// ---------------------------------------------------------------------------
static bool check_read_offsets(SveGatherScatterData *d, const uint64_t *raw,
                               const uint64_t *idx, size_t n)
{
    bool ok = true;
    for (size_t i = 0; i < n; ++i) {
        uint64_t decoded = raw[i] & 0x0000FFFFFFFFFFFFULL;
        if (decoded != (idx[i] & 0x0000FFFFFFFFFFFFULL)) {
            log_warning("sve512_gather_scatter: wrong-offset read at slot %zu: "
                        "expected index %llu, value decodes to %llu",
                        i, (unsigned long long)idx[i],
                        (unsigned long long)decoded);
            ok = false;
        }
    }
    return ok;
}

// Wide-stride index table (forces L1 misses on the gather path).
static void build_stride_idx(std::vector<uint64_t> &idx, size_t n, size_t span)
{
    idx.resize(n);
    for (size_t i = 0; i < n; ++i)
        idx[i] = (i * 64) % span;   // stride 64 elements
}

static int sve512_gather_scatter_arm_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<SveGatherScatterData *>(test->data);
    const size_t lanes = d->vl_d;

    // u64base variant operand: per-lane base addresses into src.
    std::vector<uint64_t> base_addrs(d->gather_idx.size());
    for (size_t i = 0; i < base_addrs.size(); ++i)
        base_addrs[i] = reinterpret_cast<uintptr_t>(&d->gather_src[d->gather_idx[i]]);

    // wide-stride variant table
    std::vector<uint64_t> stride_idx;
    build_stride_idx(stride_idx, d->gather_idx.size(), d->gather_src.size());

    do {
        bool all_passed = true;

        // ---- variant 1: u64index (original) + explicit offset check ----
        {
            const size_t n = d->gather_idx.size();
            std::vector<uint64_t> raw(n);
            const svbool_t pg = svptrue_b64();
            for (size_t off = 0; off < n; off += lanes) {
                svuint64_t vidx = svld1_u64(pg, &d->gather_idx[off]);
                svfloat64_t v = svld1_gather_u64index_f64(
                    pg, reinterpret_cast<const double *>(d->gather_src.data()), vidx);
                svst1_f64(pg, reinterpret_cast<double *>(&raw[off]), v);
            }
            if (!check_read_offsets(d, raw.data(), d->gather_idx.data(), n))
                all_passed = false;
        }

        // ---- variant 2: u64base (vector base addresses) ----
        {
            const size_t n = d->gather_idx.size();
            std::vector<uint64_t> raw(n);
            const svbool_t pg = svptrue_b64();
            for (size_t off = 0; off < n; off += lanes) {
                svuint64_t vbase = svld1_u64(pg, &base_addrs[off]);
                svfloat64_t v = svld1_gather_u64base_f64(pg, vbase);
                svst1_f64(pg, reinterpret_cast<double *>(&raw[off]), v);
            }
            if (!check_read_offsets(d, raw.data(), d->gather_idx.data(), n))
                all_passed = false;
        }

        // ---- variant 3: wide stride (L1-miss gather path) ----
        {
            const size_t n = stride_idx.size();
            std::vector<uint64_t> raw(n);
            const svbool_t pg = svptrue_b64();
            for (size_t off = 0; off < n; off += lanes) {
                svuint64_t vidx = svld1_u64(pg, &stride_idx[off]);
                svfloat64_t v = svld1_gather_u64index_f64(
                    pg, reinterpret_cast<const double *>(d->gather_src.data()), vidx);
                svst1_f64(pg, reinterpret_cast<double *>(&raw[off]), v);
            }
            if (!check_read_offsets(d, raw.data(), stride_idx.data(), n))
                all_passed = false;
        }

        // ---- original FMA+scatter round-trip (value-level check) ----
        gather_scatter_hw(d);
        gather_scatter_golden(d);
        for (size_t i = 0; i < d->scatter_dst.size(); ++i) {
            if (d->scatter_dst[i] != d->scatter_golden[i]) {
                report_f64_lane_mismatch("sve512_gather_scatter", i,
                                         d->scatter_golden[i],
                                         d->scatter_dst[i]);
                all_passed = false;
            }
        }
        std::fill(d->scatter_dst.begin(), d->scatter_dst.end(), 0);

        if (!all_passed) {
            report_fail_msg(
                "sve512_gather_scatter_arm: SVE-512 gather/scatter SDC detected "
                "(wrong-offset read or scatter corruption)");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int sve512_gather_scatter_arm_cleanup(struct test *test)
{
    delete static_cast<SveGatherScatterData *>(test->data);
    return EXIT_SUCCESS;
}

#else  /* !__aarch64__ */

static int sve512_gather_scatter_arm_init(struct test *test)
{
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "sve512_gather_scatter_arm requires aarch64 SVE");
    return EXIT_SKIP;
}
static int sve512_gather_scatter_arm_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    return EXIT_SKIP;
}
static int sve512_gather_scatter_arm_cleanup(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

#endif /* __aarch64__ */

DECLARE_TEST(sve512_gather_scatter_arm,
             "SVE full-vector-length gather/scatter SDC stress: "
             "svld1_gather_u64index_f64 + svmla_f64_x + "
             "svst1_scatter_u64index_f64 round-trip over a permuted index "
             "table, byte-exact compared against a scalar golden")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = sve512_gather_scatter_arm_init,
    .test_run = sve512_gather_scatter_arm_run,
    .test_cleanup = sve512_gather_scatter_arm_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
