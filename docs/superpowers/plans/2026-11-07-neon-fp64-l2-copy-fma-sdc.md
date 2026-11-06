# NEON FP64 L2 Copy+FMA SDC Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an aarch64-only `memcpy_l2_cache_size_neon_fp64` test that copies an L2-sized FP64 block with NEON and runs byte-exact FP64 FMA over it, so SDC on the NEON/FP64 datapath is detected instead of being hidden behind byte-wise libc `memcpy`.

**Architecture:** A new self-contained test in `tests/cpu/arm64/`, registered in the aarch64-only meson source list. It detects the L2 size (`sysconf` → sysfs `index2/size` → 256 KiB fallback), generates finite FP64 inputs with the framework RNG, computes a scalar `std::fma` golden in `test_init`, and per worker thread performs (a) a NEON `vld1q_f64`/`vst1q_f64` copy of the whole block and (b) an in-place NEON FP64 `vaddq_f64`/`vmulq_f64`/`vfmaq_f64` pass, comparing both against golden data with `memcmp_or_fail` (byte-exact). A small special-value table checks NaN/Inf category agreement, and a 7-element self-check exercises the non-vector-multiple tail loops.

**Tech Stack:** C++23, ARM NEON intrinsics (`float64x2_t`), libm `std::fma`, SDCShield test framework, meson/ninja.

**Spec:** User request 2026-11-07: create a `neon_fp64` version of `memcpy_l2_cache_size`, prioritizing NEON instructions and FP64 because they are more likely to trigger SDC.

## Global Constraints

- The new test is **aarch64-only**; x86-64 source and behavior must remain untouched. Put the file under `tests/cpu/arm64/` and register it only inside the existing `if host_machine.cpu_family() == 'aarch64'` block.
- Finite FP64 results must be compared **byte-exact**; do not use a floating-point tolerance.
- NaN/Inf results must be compared by **category** (NaN / signed-Inf / finite), not by payload bits, because IEEE-754 leaves NaN payload propagation implementation-defined.
- Use the framework RNG (`random64()`) in `test_init`, never `std::random_device`, so failures replay with `-s`.
- The scalar golden must not be auto-vectorized into the same NEON path it is supposed to check. Use `volatile` intermediates in the golden block loop to force scalar FP instructions.
- No success-path logging. `log_debug` is allowed; `log_warning`/`report_fail_msg` only on failure paths.
- No global statics with constructors.
- Build after changing `tests/cpu/arm64/meson.build` with `meson setup --reconfigure` (this also regenerates `sandstone_short_ids.h`), then `ninja -C builddir`.
- The repository enforces one patch per unit: commit the test as one commit and the README/test-catalog update as a second commit.
- Update README counts from **actual command output**, not predicted numbers.

## Review Focus

These are the input/failure classes most likely to produce a misleading pass. Each is pinned to a check in Task 1.

1. **NaN/Inf payloads**: a reasonable user expects special values not to crash or produce false SDC reports; NaN payload propagation differs between NEON and libm, so compare category only. Pinned by `special_values_check()`.
2. **Golden computed on the same NEON path**: a NEON/FP64 fault that corrupts both the result and the golden would pass. Pinned by `volatile` scalar intermediates in `scalar_reference()` / the init golden loop.
3. **L2 detection failure**: a system where `sysconf(_SC_LEVEL2_CACHE_SIZE)` returns 0 (as this host does) must fall back to sysfs `index2/size` (512 KiB here), then 256 KiB, and the block must remain 64-byte aligned. Pinned by `detect_l2_bytes()` plus the init alignment guard.
4. **Non-vector-multiple tails**: a block length not divisible by 4 doubles must still be copied and FMA'd correctly. Pinned by the 7-double `tail_self_check()`.
5. **Multi-thread shared state**: all worker threads share `src`/`golden`; a test that writes those would race. Pinned by keeping `src`/`golden` read-only and allocating a per-thread `dst` in `test_run`, then running the multi-threaded verification step.

---

### Task 1: Add the NEON FP64 L2 copy+FMA test and register it

**Files:**
- Create: `tests/cpu/arm64/memcpy_l2_cache_size_neon_fp64.cpp`
- Modify: `tests/cpu/arm64/meson.build` (append one entry to `arm64_tests`)

**Interfaces:**
- Consumes: `sandstone.h` framework API (`DECLARE_TEST`, `random64`, `aligned_alloc_safe`, `memcmp_or_fail`, `log_skip`, `log_warning`, `log_debug`, `test_time_condition`, `TestResourceIssueSkipCategory`, `CpuNotSupportedSkipCategory`, `group_math`).
- Produces: test name `memcpy_l2_cache_size_neon_fp64`; no other task consumes C++ symbols from this file.

- [x] **Step 1: Create the test source file**

Create `tests/cpu/arm64/memcpy_l2_cache_size_neon_fp64.cpp` with exactly this content:

```cpp
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
```

- [x] **Step 2: Register the test in the aarch64-only meson list**

In `tests/cpu/arm64/meson.build`, inside the `arm64_tests = files(...)` list (currently lines 18–74), add this entry immediately after `'neon_test.cpp',`:

```meson
        # NEON FP64 byte-exact L2-sized copy + FMA SDC stress — the FP64
        # NEON datapath variant of memcpy_l2_cache_size. Built only on aarch64.
        'memcpy_l2_cache_size_neon_fp64.cpp',
```

The resulting beginning of the list must read:

```meson
    arm64_tests = files(
        'neon_test.cpp',
        # NEON FP64 byte-exact L2-sized copy + FMA SDC stress — the FP64
        # NEON datapath variant of memcpy_l2_cache_size. Built only on aarch64.
        'memcpy_l2_cache_size_neon_fp64.cpp',
        'crypto_test.cpp',
        ...
```

- [x] **Step 3: Reconfigure meson and build**

`meson setup --reconfigure` is mandatory: it regenerates `builddir/sandstone_short_ids.h`, which is produced by `scripts/generate-short-ids.pl` scanning `tests/` at configure time. Without it, the new `TEST_ID_memcpy_l2_cache_size_neon_fp64` macro is missing and compilation fails.

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release
ninja -C builddir
```

Expected: configure succeeds; `ninja` completes with zero errors. Pre-existing benign warnings are acceptable; no new warnings/errors from the new file.

- [x] **Step 4: Verify the test is registered**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
./builddir/sdcshield --list-tests | grep -n '^memcpy_l2_cache_size_neon_fp64$'
```

Expected: one matching line, exit status 0.

- [x] **Step 5: Run single-threaded functional verification**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
./builddir/sdcshield -e memcpy_l2_cache_size_neon_fp64 -t 5000 -n 1
```

Expected: `exit: pass`, no `mismatch`, no `SIGSEGV`. Single-threaded is the deterministic check that scalar golden and NEON FMA agree.

- [x] **Step 6: Run multi-threaded functional verification**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
./builddir/sdcshield -e memcpy_l2_cache_size_neon_fp64 -t 5000
```

Expected: `exit: pass`, no mismatches, no crashes. This exercises the 126-thread path and the per-thread `dst` allocation.

- [x] **Step 7: Regression check on an unaffected test**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
./builddir/sdcshield -e zstd19 -t 2000
```

Expected: `exit: pass`, zero SIGSEGV. This proves the build/link change did not break an unrelated test.

- [x] **Step 8: Confirm the diff is aarch64-only**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
git status --short
git diff --stat
```

Expected: only `tests/cpu/arm64/memcpy_l2_cache_size_neon_fp64.cpp` (new), `tests/cpu/arm64/meson.build` (modified), plus the plan file. No x86-64 source, framework, or shared meson changes.

- [x] **Step 9: Commit the test**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
git add tests/cpu/arm64/memcpy_l2_cache_size_neon_fp64.cpp \
        tests/cpu/arm64/meson.build \
        docs/superpowers/plans/2026-11-07-neon-fp64-l2-copy-fma-sdc.md
git commit -m "test: add NEON FP64 L2-sized copy+FMA byte-exact SDC stress"
```

Do not append a `Co-Authored-By` trailer (repository rule).

---

### Task 2: Update README test catalog and counts

**Files:**
- Modify: `README.md` (test catalog table around lines 120–154)

**Interfaces:**
- Consumes: the built binary and test list produced by Task 1.
- Produces: accurate README counts and a catalog row mentioning `memcpy_l2_cache_size_neon_fp64`.

- [ ] **Step 1: Measure the actual test counts**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
prod=$(./builddir/sdcshield --list-tests | wc -l)
beta_or_better=$(./builddir/sdcshield --quality=0 --list-tests | wc -l)
all=$(./builddir/sdcshield --quality=-1 --list-tests | wc -l)
printf 'PROD=%s BETA=%s SKIP=%s TOTAL=%s\n' \
  "$prod" "$((beta_or_better - prod))" "$((all - beta_or_better))" "$all"
```

Record the four printed values. If the pre-Task-1 counts were captured, `PROD` and `TOTAL` should each be exactly one greater now (the new test is PROD). If `meson --reconfigure` picked up other stale tests, that delta may be larger; in that case verify instead that (a) `./builddir/sdcshield --list-tests | grep -qx memcpy_l2_cache_size_neon_fp64`, and (b) `PROD + BETA + SKIP == TOTAL` with the measured values. Do not edit README until the new test is present and the arithmetic is consistent.

- [ ] **Step 2: Update the count sentence**

In `README.md`, replace the sentence that currently starts with `当前 ARM64 构建（Kunpeng 920 / openEuler 24.03 SP3）共 **273 个用例**` with the same sentence using the measured values:

```markdown
当前 ARM64 构建（Kunpeng 920 / openEuler 24.03 SP3）共 **<TOTAL> 个用例**：PROD <PROD>、BETA <BETA>、SKIP <SKIP>。
```

Replace `<TOTAL>`, `<PROD>`, `<BETA>`, `<SKIP>` with the numbers from Step 1. Do not leave angle brackets in the file.

- [ ] **Step 3: Update the quality-level table**

In the `### 用例质量分级` table, set the three counts to the measured values:

```markdown
| 级别 | 名称 | 运行条件 | 实有数量 |
|---|---|---|---|
| -1 | SKIP | `quality >= -1` | <SKIP> |
| 0 | BETA | `quality >= 0` | <BETA> |
| 2 | PROD（默认）| `quality >= 2` | <PROD> |
| | **合计** | | **<TOTAL>** |
```

Replace the placeholders with the measured numbers. Keep the BETA and SKIP name lists below the table unchanged.

- [ ] **Step 4: Add the new test to the catalog table**

In the `| 内存 / 拷贝 | ... |` row, add the new test name to the representative list and mention the FP64 NEON path. For example, change the row's representative list to include:

```markdown
`memcpy_l{1d,2,3}_cache_size`、`memcpy_l2_cache_size_neon_fp64`、`memcpy_rewr`、`mem_disambiguation`、`mmu_stress_arm`
```

and change the detection-capability cell to:

```markdown
各级缓存带宽、FP64 NEON 拷贝+FMA 字节级校验、store-to-load 转发、跨行一致性与内存序、TLB 扰动
```

Also add `memcpy_l2_cache_size_neon_fp64` to the `| FMA / 浮点 | ... |` row's representative list, since it is an FP64 FMA test:

```markdown
| FMA / 浮点 | `fma`、`fma_patterns_*`、`fma_tail*`、`fpu_special_values`、`memcpy_l2_cache_size_neon_fp64` | FMA 模式与尾数精度穷举、特殊值逐字节 golden 比对、FP64 NEON L2 块拷贝+FMA |
```

- [ ] **Step 5: Verify the README numbers match the commands**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
grep -nE '共 \*\*[0-9]+ 个用例|实有数量|\| [0-9-]+ \| (SKIP|BETA|PROD)' README.md | head -10
```

Manually compare each printed number with the Step 1 output. Every number must match; no stale 273/264 values may remain.

- [ ] **Step 6: Commit the README update**

Run:

```bash
cd /home/sdc/root/opendcdiag-arm
git add README.md
git commit -m "docs: catalog the NEON FP64 L2 copy+FMA test and refresh counts"
```

Do not append a `Co-Authored-By` trailer (repository rule).

---

## Verification Matrix

| Requirement | Check |
|---|---|
| Test is aarch64-only and x86 untouched | Task 1 Step 8 diff scope |
| Test builds and links | Task 1 Step 3 `ninja -C builddir` |
| Test is discovered | Task 1 Step 4 `--list-tests` grep |
| NEON FP64 copy is byte-exact | Task 1 Steps 5–6 (copy `memcmp_or_fail`) |
| NEON FP64 FMA is byte-exact vs scalar libm | Task 1 Steps 5–6 (FMA `memcmp_or_fail`) |
| Scalar golden cannot be vectorized into NEON | `volatile` intermediates in `scalar_reference`; Review Focus item 2 |
| NaN/Inf handled by category | `special_values_check()`; Review Focus item 1 |
| L2 detection fallbacks and alignment | `detect_l2_bytes()` + init guard; Review Focus item 3 |
| Tail lengths not divisible by 4 | 7-double `tail_self_check()`; Review Focus item 4 |
| No multi-thread race on shared data | per-thread `dst`; Task 1 Step 6; Review Focus item 5 |
| No unrelated regression | Task 1 Step 7 `zstd19` |
| README counts are accurate | Task 2 Steps 1, 2, 3, 5 |
