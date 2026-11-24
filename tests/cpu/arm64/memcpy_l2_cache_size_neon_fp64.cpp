/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b memcpy_l2_cache_size_neon_fp64
 * @parblock
 * NEON FP64 byte-exact L2-sized copy + FMA SDC stress.
 *
 * The existing memcpy_l2_cache_size test copies random bytes with libc
 * memcpy and verifies with memcmp. It exercises the L2 capacity but leaves
 * the FP64 NEON datapath untouched. This test keeps the L2-sized working
 * set and adds:
 *
 *   1. A pure NEON FP64 copy (vld1q_f64/vst1q_f64) of an L2-sized double
 *      array, verified byte-exact against the source.
 *   2. In-place NEON FP64 FMA over the copied block:
 *          y_i = fma(x_i, x_i + 1.5, x_i * 0.5)
 *      compared byte-exact against a scalar libm fma() golden block. The
 *      add, multiply and fused multiply-add exercise the FP64 ALU while
 *      the L2-sized block exercises the NEON load/store path.
 *   3. A small IEEE-754 special-value check (NaN/Inf category, finite
 *      byte-exact) for the same FP64 operation.
 *
 * Data are double (FP64) and all finite comparisons are bit-exact. Inputs
 * are generated with the framework RNG in test_init, so failures replay
 * with -s. The block size is the detected L2 size (sysconf, then sysfs,
 * then 256 KiB fallback), rounded down to 64 B. The scalar golden loop
 * uses volatile intermediates so it cannot be auto-vectorized onto the
 * same NEON path it is checking.
 *
 * SDC detection: memcmp_or_fail on the copy and on the FMA result; a 1-bit
 * FP64 or NEON lane fault changes the result and is reported at the first
 * mismatching double. ARM64-native (NEON vld1q_f64/vst1q_f64/vaddq_f64/
 * vmulq_f64/vfmaq_f64 + libm fma); non-aarch64 returns EXIT_SKIP.
 * @endparblock
 */

#include <sandstone.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __aarch64__
#include <arm_neon.h>
#include <unistd.h>
#endif

#ifdef __aarch64__

namespace {

constexpr size_t kCacheLine = 64;
constexpr size_t kMinBlockBytes = 64;
constexpr size_t kDefaultL2Bytes = 256 * 1024;
constexpr size_t kMaxBlockBytes = 4 * 1024 * 1024;
constexpr double kC1 = 1.5;
constexpr double kC2 = 0.5;

// 52-bit alternating high-Hamming mantissas; the prefix keeps them inside
// the 52-bit mantissa field when OR-ed with exponent/sign.
constexpr uint64_t kMantissaAlternatingA = 0x000AAAAAAAAAAAAAULL;
constexpr uint64_t kMantissaAlternating5 = 0x0005555555555555ULL;

static size_t round_down(size_t value, size_t align)
{
    return (value / align) * align;
}

static size_t detect_l2_bytes()
{
    const long reported = sysconf(_SC_LEVEL2_CACHE_SIZE);
    if (reported > 0) {
        const size_t bytes =
            round_down(static_cast<size_t>(reported), kCacheLine);
        if (bytes >= kMinBlockBytes)
            return bytes;
    }

    FILE *f = std::fopen("/sys/devices/system/cpu/cpu0/cache/index2/size", "r");
    if (f) {
        char buf[32] = {};
        unsigned long long value = 0;
        char unit = 0;
        if (std::fgets(buf, sizeof(buf), f)
            && std::sscanf(buf, "%llu%c", &value, &unit) == 2) {
            if (unit == 'K' || unit == 'k')
                value *= 1024ULL;
            else if (unit == 'M' || unit == 'm')
                value *= 1024ULL * 1024ULL;
            std::fclose(f);
            const size_t bytes =
                round_down(static_cast<size_t>(value), kCacheLine);
            if (bytes >= kMinBlockBytes)
                return bytes;
        } else {
            std::fclose(f);
        }
    }

    return kDefaultL2Bytes;
}

struct l2_fp64_state {
    double *src = nullptr;
    double *golden = nullptr;
    size_t bytes = 0;
    size_t count = 0;
};

static inline uint64_t bits_of_double(double value)
{
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static inline double double_of_bits(uint64_t bits)
{
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static double finite_fp64_from_random(uint64_t random)
{
    // Exponent in [1023-8, 1023+8] keeps x+1.5 and x*0.5 finite and avoids
    // overflow/underflow; half the mantissas are alternating high-Hamming
    // patterns to maximize FP64 datapath toggling.
    const uint64_t sign = random & 0x8000000000000000ULL;
    const uint64_t exponent = (1023ULL - 8ULL + ((random >> 1) % 17ULL)) << 52;
    uint64_t mantissa;
    if (random & 0x100ULL)
        mantissa = kMantissaAlternatingA;
    else if (random & 0x200ULL)
        mantissa = kMantissaAlternating5;
    else
        mantissa = (random >> 12) & 0x000FFFFFFFFFFFFFULL;
    return double_of_bits(sign | exponent | mantissa);
}

// Deliberately scalar: volatile intermediates prevent GCC from contracting
// or vectorizing this into the NEON FMA path the test is checking.
static double scalar_reference(double x)
{
    volatile double input = x;
    volatile double t = input + kC1;
    volatile double u = input * kC2;
    return std::fma(input, t, u);
}

static inline bool is_nan_bits(uint64_t bits)
{
    return (bits & 0x7FFFFFFFFFFFFFFFULL) > 0x7FF0000000000000ULL;
}

static inline bool is_inf_bits(uint64_t bits)
{
    return (bits & 0x7FFFFFFFFFFFFFFFULL) == 0x7FF0000000000000ULL;
}

static inline void neon_copy_fp64(double *dst, const double *src, size_t count)
{
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        const float64x2_t s0 = vld1q_f64(src + i);
        const float64x2_t s1 = vld1q_f64(src + i + 2);
        vst1q_f64(dst + i, s0);
        vst1q_f64(dst + i + 2, s1);
    }
    for (; i + 1 < count; i += 2)
        vst1q_f64(dst + i, vld1q_f64(src + i));
    for (; i < count; ++i)
        dst[i] = src[i];
}

static inline void neon_fma_fp64(double *dst, size_t count)
{
    const float64x2_t c1 = vdupq_n_f64(kC1);
    const float64x2_t c2 = vdupq_n_f64(kC2);
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        const float64x2_t x0 = vld1q_f64(dst + i);
        const float64x2_t x1 = vld1q_f64(dst + i + 2);
        const float64x2_t y0 = vfmaq_f64(vmulq_f64(x0, c2),
                                         x0, vaddq_f64(x0, c1));
        const float64x2_t y1 = vfmaq_f64(vmulq_f64(x1, c2),
                                         x1, vaddq_f64(x1, c1));
        vst1q_f64(dst + i, y0);
        vst1q_f64(dst + i + 2, y1);
    }
    for (; i + 1 < count; i += 2) {
        const float64x2_t x = vld1q_f64(dst + i);
        const float64x2_t y = vfmaq_f64(vmulq_f64(x, c2),
                                        x, vaddq_f64(x, c1));
        vst1q_f64(dst + i, y);
    }
    for (; i < count; ++i)
        dst[i] = scalar_reference(dst[i]);
}

static bool tail_self_check()
{
    constexpr size_t kTailCount = 7;
    const double src[kTailCount] = {
        0.0, 1.0, -2.5, 1024.25, -0.000244140625, 3.75, -65536.5
    };
    double dst[kTailCount] = {};
    double golden[kTailCount] = {};

    for (size_t i = 0; i < kTailCount; ++i)
        golden[i] = scalar_reference(src[i]);

    neon_copy_fp64(dst, src, kTailCount);
    if (std::memcmp(dst, src, sizeof(dst)) != 0) {
        log_warning("memcpy_l2_cache_size_neon_fp64: tail copy mismatch");
        return false;
    }

    neon_fma_fp64(dst, kTailCount);
    if (std::memcmp(dst, golden, sizeof(dst)) != 0) {
        log_warning("memcpy_l2_cache_size_neon_fp64: tail FMA mismatch");
        return false;
    }
    return true;
}

static bool special_values_check()
{
    static const uint64_t kSpecial[] = {
        0x0000000000000000ULL, 0x8000000000000000ULL, // +0, -0
        0x0000000000000001ULL, 0x8000000000000001ULL, // +/- min subnormal
        0x0010000000000000ULL, 0x8010000000000000ULL, // +/- min normal
        0x7FEFFFFFFFFFFFFFULL, 0xFFEFFFFFFFFFFFFFULL, // +/- max finite
        0x3FF123456789ABCDULL, 0x400AAAAAAAAAAAAAULL, // high-Hamming normals
        0x7FF0000000000000ULL, 0xFFF0000000000000ULL, // +/- Inf
        0x7FF8000000000000ULL, 0x7FF0000000000001ULL, // qNaN, sNaN
    };
    constexpr size_t kSpecialCount = sizeof(kSpecial) / sizeof(kSpecial[0]);
    const double c = 0.5;

    for (size_t i = 0; i < kSpecialCount; ++i) {
        for (size_t j = 0; j < kSpecialCount; ++j) {
            const double a = double_of_bits(kSpecial[i]);
            const double b = double_of_bits(kSpecial[j]);
            const float64x2_t vd = vfmaq_f64(vdupq_n_f64(c),
                                             vdupq_n_f64(a),
                                             vdupq_n_f64(b));
            double lanes[2];
            vst1q_f64(lanes, vd);
            const uint64_t hw = bits_of_double(lanes[0]);
            const uint64_t sw = bits_of_double(std::fma(a, b, c));

            const bool category_ok =
                (is_nan_bits(hw) && is_nan_bits(sw))
                || (is_inf_bits(hw) && is_inf_bits(sw) && hw == sw)
                || (!is_nan_bits(hw) && !is_inf_bits(hw)
                    && !is_nan_bits(sw) && !is_inf_bits(sw)
                    && hw == sw);
            if (!category_ok) {
                log_warning("memcpy_l2_cache_size_neon_fp64: special-value "
                            "mismatch a=0x%016" PRIx64 " b=0x%016" PRIx64
                            " hw=0x%016" PRIx64 " sw=0x%016" PRIx64,
                            kSpecial[i], kSpecial[j], hw, sw);
                return false;
            }
        }
    }
    return true;
}

} // namespace

static int memcpy_l2_cache_size_neon_fp64_init(struct test *test)
{
    auto *state = static_cast<l2_fp64_state *>(std::malloc(sizeof(l2_fp64_state)));
    if (!state) {
        log_skip(TestResourceIssueSkipCategory,
                 "memcpy_l2_cache_size_neon_fp64: state allocation failed");
        return EXIT_SKIP;
    }

    state->bytes = detect_l2_bytes();
    if (state->bytes > kMaxBlockBytes)
        state->bytes = kMaxBlockBytes;
    state->bytes = round_down(state->bytes, kCacheLine);
    state->count = state->bytes / sizeof(double);

    if (state->bytes < kMinBlockBytes || state->count < 4) {
        std::free(state);
        log_skip(TestResourceIssueSkipCategory,
                 "memcpy_l2_cache_size_neon_fp64: detected block too small");
        return EXIT_SKIP;
    }

    state->src = static_cast<double *>(
        aligned_alloc_safe(kCacheLine, state->bytes));
    state->golden = static_cast<double *>(
        aligned_alloc_safe(kCacheLine, state->bytes));
    if (!state->src || !state->golden) {
        std::free(state->src);
        std::free(state->golden);
        std::free(state);
        log_skip(TestResourceIssueSkipCategory,
                 "memcpy_l2_cache_size_neon_fp64: buffer allocation failed");
        return EXIT_SKIP;
    }

    for (size_t i = 0; i < state->count; ++i) {
        const double x = finite_fp64_from_random(random64());
        state->src[i] = x;
        state->golden[i] = scalar_reference(x);
    }

    if (!tail_self_check() || !special_values_check()) {
        std::free(state->src);
        std::free(state->golden);
        std::free(state);
        return EXIT_FAILURE;
    }

    log_debug("memcpy_l2_cache_size_neon_fp64: block=%zu B, %zu doubles",
              state->bytes, state->count);
    test->data = state;
    return EXIT_SUCCESS;
}

static int memcpy_l2_cache_size_neon_fp64_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *state = static_cast<l2_fp64_state *>(test->data);
    double *dst = static_cast<double *>(
        aligned_alloc_safe(kCacheLine, state->bytes));
    if (!dst) {
        log_skip(TestResourceIssueSkipCategory,
                 "memcpy_l2_cache_size_neon_fp64: per-thread buffer "
                 "allocation failed");
        return EXIT_SKIP;
    }

    do {
        neon_copy_fp64(dst, state->src, state->count);
        memcmp_or_fail(dst, state->src, state->count,
                       "NEON FP64 L2 copy");

        neon_fma_fp64(dst, state->count);
        memcmp_or_fail(dst, state->golden, state->count,
                       "NEON FP64 L2 FMA");
    } while (test_time_condition(test));

    std::free(dst);
    return EXIT_SUCCESS;
}

static int memcpy_l2_cache_size_neon_fp64_finish(struct test *test)
{
    auto *state = static_cast<l2_fp64_state *>(test->data);
    if (state) {
        std::free(state->src);
        std::free(state->golden);
        std::free(state);
        test->data = nullptr;
    }
    return EXIT_SUCCESS;
}

#else // !__aarch64__

static int memcpy_l2_cache_size_neon_fp64_init(struct test *test)
{
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 NEON FP64 required");
    return EXIT_SKIP;
}

static int memcpy_l2_cache_size_neon_fp64_run(struct test *test, int cpu)
{
    (void)test;
    (void)cpu;
    return EXIT_SKIP;
}

static int memcpy_l2_cache_size_neon_fp64_finish(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(memcpy_l2_cache_size_neon_fp64,
             "NEON FP64 byte-exact L2-sized copy + FMA SDC stress")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = memcpy_l2_cache_size_neon_fp64_init,
    .test_run = memcpy_l2_cache_size_neon_fp64_run,
    .test_cleanup = memcpy_l2_cache_size_neon_fp64_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
