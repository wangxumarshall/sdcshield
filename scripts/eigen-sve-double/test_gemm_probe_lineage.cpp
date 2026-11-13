// GEMM sizing-probe lineage equivalence (issue #615 residual sweep).
//
// Proves byte-identical equivalence between the pre-collapse VERBATIM inline
// splitmix probes (as carried by gemm_{f64,f32,c128}_sve_body.cpp on
// origin/main 3631a7d4) and the shared single-source template
// tests/cpu/eigen_gemm/gemm_solve_probe.h::gemm_probe (PR #392 collapse).
// The same three variants are what the 7 non-SVE T9 TUs carried pre-#392
// (PR #452 triage: 3 md5 clusters), so this re-proves the T9 collapse
// equivalence against the CURRENT template — the precondition the SVE-body
// forwarding fix relies on.
//
// Channels (issue #410 dual-kernel protocol):
//   A — old vs new byte-identical a/b/c per (variant, n): old fill == verbatim
//       loop, new fill == gemm_probe::fill_pair, then the SAME `c = a * b`;
//       every element finite (catches NaN-payload divergence);
//   B — FNV-1a 64-bit checksums printed per matrix for log quoting;
//   C — VL identity echo: prints the runtime SVE vector length so every
//       tier run self-certifies its tier.
//
// Build & run (native non-SVE host → NEON/scalar packets; from repo root —
// the shared-header include is repo-root-relative, hence -I.):
//   g++ -O2 -std=c++17 -I. -I third-party/eigen5 \
//       scripts/eigen-sve-double/test_gemm_probe_lineage.cpp \
//       -o /tmp/gemm_probe_lineage_native && /tmp/gemm_probe_lineage_native
// Build per SVE tier (README recipe, scripts/eigen-sve-double/gem5/):
//   g++ -O2 -std=c++17 -I. -I third-party/eigen5 -march=armv8.2-a+sve \
//       -msve-vector-bits=<128|256|512|1024> -DEIGEN_ARM64_USE_SVE \
//       scripts/eigen-sve-double/test_gemm_probe_lineage.cpp \
//       -o /tmp/gem5_bins/gemm_probe_lineage_<tier>
// Run tier in gem5 SE (run_sve.sh echoes the resolved gem5 md5 — record it):
//   scripts/eigen-sve-double/gem5/run_sve.sh \
//       /tmp/gem5_bins/gemm_probe_lineage_<tier> --vl <1|2|4|8>
#include <Eigen/Core>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <complex>
#include <vector>

#include "tests/cpu/eigen_gemm/gemm_solve_probe.h"

using namespace Eigen;

typedef Matrix<double, Dynamic, Dynamic> MatD;
typedef Matrix<float, Dynamic, Dynamic> MatF;
typedef Matrix<std::complex<double>, Dynamic, Dynamic> MatC;

#if defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>
#endif

static int failures = 0;
static int triplets = 0;

/* ---- channel B: FNV-1a 64-bit ---- */
static std::uint64_t fnv1a(const void *p, std::size_t bytes)
{
    const unsigned char *q = (const unsigned char *)p;
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < bytes; ++i) {
        h ^= q[i];
        h *= 1099511628211ull;
    }
    return h;
}

/* ---- channel A "old" leg: VERBATIM fill loops (loop bodies kept
 * character-identical to origin/main 3631a7d4; only the enclosing signature
 * adapted to expose the matrices for comparison):
 *   f64 : gemm_f64_sve_body.cpp:115-124 (scalar double stream)
 *   f32 : gemm_f32_sve_body.cpp:115-124 (scalar double stream, f32 store
 *         via double intermediate — 原用例同款转型)
 *   c128: gemm_c128_sve_body.cpp:123-139 (complex re/im 四流) */
static void old_solve_f64(int dim, MatD &a, MatD &b, MatD &c)
{
    a.resize(dim, dim); b.resize(dim, dim);
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (double)(h % 2000u) / 1000.0 - 1.0;
            b(r, col) = (double)(h2 % 2000u) / 1000.0 - 1.0;
        }
    c = a * b;
}

static void old_solve_f32(int dim, MatF &a, MatF &b, MatF &c)
{
    a.resize(dim, dim); b.resize(dim, dim);
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (float)((double)(h % 2000u) / 1000.0 - 1.0);
            b(r, col) = (float)((double)(h2 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
}

static void old_solve_c128(int dim, MatC &a, MatC &b, MatC &c)
{
    a.resize(dim, dim); b.resize(dim, dim);
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            std::uint64_t h3 = h2 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h3 ^= h3 >> 32;
            std::uint64_t h4 = h3 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h4 ^= h4 >> 32;
            a(r, col) = std::complex<double>(
                (double)(h % 2000u) / 1000.0 - 1.0,
                (double)(h2 % 2000u) / 1000.0 - 1.0);
            b(r, col) = std::complex<double>(
                (double)(h3 % 2000u) / 1000.0 - 1.0,
                (double)(h4 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
}

template <typename Mat>
static std::size_t matrix_bytes(const Mat &m)
{
    return (std::size_t)m.rows() * m.cols() * sizeof(typename Mat::Scalar);
}

template <typename Mat>
static bool bytes_equal(const Mat &x, const Mat &y)
{
    return x.rows() == y.rows() && x.cols() == y.cols()
        && std::memcmp(x.data(), y.data(), matrix_bytes(x)) == 0;
}

template <typename Mat>
static std::uint64_t matrix_fnv(const Mat &m)
{
    return fnv1a(m.data(), matrix_bytes(m));
}

/* channel A finiteness leg: all elements finite ([-1,1) by construction). */
static bool all_finite(const MatD &m)
{
    for (int i = 0; i < m.size(); ++i)
        if (!std::isfinite(m.data()[i])) return false;
    return true;
}
static bool all_finite(const MatF &m)
{
    for (int i = 0; i < m.size(); ++i)
        if (!std::isfinite(m.data()[i])) return false;
    return true;
}
static bool all_finite(const MatC &m)
{
    for (int i = 0; i < m.size(); ++i) {
        if (!std::isfinite(m.data()[i].real())) return false;
        if (!std::isfinite(m.data()[i].imag())) return false;
    }
    return true;
}

/* One (variant, n) comparison. The "new" leg reproduces the exact 3-line
 * body of gemm_probe::solve_once<Mat> (the template builds its matrices
 * internally and discards them; its body is literally construct →
 * fill_pair → multiply). */
template <typename Mat, typename OldFn>
static void compare(const char *name, OldFn old_solve, int n)
{
    Mat ao, bo, co, an(n, n), bn(n, n), cn;
    old_solve(n, ao, bo, co);
    gemm_probe::fill_pair(an, bn);
    cn = an * bn;

    triplets++;
    const bool ok_ab = bytes_equal(ao, an) && bytes_equal(bo, bn);
    const bool ok_c = bytes_equal(co, cn);
    const bool ok_fin = all_finite(co) && all_finite(cn);
    const bool ok = ok_ab && ok_c && ok_fin;
    if (!ok) failures++;
    std::printf("%s %-5s n=%-3d a=%016llx b=%016llx c=%016llx %s%s%s\n",
                ok ? "ok" : "FAIL", name, n,
                (unsigned long long)matrix_fnv(an),
                (unsigned long long)matrix_fnv(bn),
                (unsigned long long)matrix_fnv(cn),
                ok_ab ? "" : " [a/b MISMATCH]",
                ok_c ? "" : " [c MISMATCH]",
                ok_fin ? "" : " [NON-FINITE]");
}

int main(void)
{
#if defined(__ARM_FEATURE_SVE)
    std::printf("VL identity: svcntb=%llu bytes (%llu-bit tier)\n",
                (unsigned long long)svcntb(),
                (unsigned long long)svcntb() * 8);
#else
    std::printf("VL identity: non-SVE build (NEON/scalar packets) — host leg\n");
#endif
    for (int n : {1, 2, 3, 5, 16, 17, 33}) {
        compare<MatD>("f64", old_solve_f64, n);
        compare<MatF>("f32", old_solve_f32, n);
        compare<MatC>("c128", old_solve_c128, n);
    }
    std::printf("RESULT: %s (%d/%d (variant,n) triplets byte-identical)\n",
                failures == 0 ? "ALL PASS" : "MISMATCH", triplets - failures,
                triplets);
    return failures == 0 ? 0 : 1;
}
