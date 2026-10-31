/* issue #182 root-cause reproducer: pre-ff8ddec1 f32 operand bias.
 *
 * Reconstructs the random_finite_f32() of the sve512 f32 chain tests as of
 * 3d731d24 (the state the GH arm64 runner failed on): 25% of draws from
 * the F32_FINITE table, 75% assembled as sign | (exp << 23) | mantissa
 * with F64-BIASED exponents 0x3FE/0x3FF. Bit-level consequence (probe
 * 2026-10-31, gcc 12.3.1 -O2 -march=armv8.2-a+sve):
 *
 *   0x3FE << 23 == 0xFF000000 -> sign=1, exponent field 0xFE (254):
 *                                FINITE, value ~ -1.m x 2^127
 *   0x3FF << 23 == 0xFF800000 -> sign=1, exponent field 0xFF (255):
 *                                NaN when mantissa != 0, -Inf when == 0
 *
 * so each constructed draw is ~50/50 NaN-class / huge-finite, and the full
 * generator (with the 25% table draws) is ~37.5% NaN-class / 62.5% finite.
 * Either way the 512-step FMA chain saturates to NaN/Inf with probability
 * 1 - (5/8)^1024 ~ 1 (and huge x huge overflows even without a NaN draw),
 * and a byte-exact golden over such results crosses the scalar-fmadd vs
 * SVE-FMLA IMPLEMENTATION-DEFINED NaN-payload split.
 *
 * RNG NOTE (why this file does not use a plain LCG): an LCG
 * x = a*x + c with odd a,c has a period-2 LSB, and with the 3-draw call
 * pattern here the m-stream's exponent draw can alias onto one parity,
 * degenerating a whole stream to a single exponent class (observed: an
 * early draft reported "4096 NaN / 0 finite" for m[] — an artifact).
 * splitmix64 is used instead so every draw is well mixed.
 *
 * Build: g++ -O2 -march=armv8.2-a+sve -o repro_qnan_bias repro_qnan_bias.cpp
 */
#include <arm_sve.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint64_t sm64_state = 0x12345678deadbeefULL;
static uint64_t splitmix64(void)
{
    uint64_t z = (sm64_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static uint32_t rnd32(void) { return (uint32_t)(splitmix64() >> 32); }

/* F32_FINITE table, verbatim from the tests (all |x| <= 2 finite). */
static const uint32_t F32_FINITE[] = {
    0x3FAAAAABU, 0xBFAAAAABU, 0x3EAAAAABU, 0xBF2AAAABU,
    0x3F800000U, 0xBF800000U, 0x00555555U, 0x80AAAAAAU,
    0x3FCCCCCDU, 0xBFCCCCCDU, 0x3F333333U, 0xBF333333U,
    0x3E555555U, 0xBE555555U, 0x3F666666U, 0xBF666666U,
};

/* random_finite_f32() EXACTLY as 3d731d24 (the buggy pre-ff8ddec1 form). */
static uint32_t old_random_finite_f32(void)
{
    if ((rnd32() & 3) == 0)
        return F32_FINITE[rnd32() % 16];
    uint32_t sign = (rnd32() & 1U) << 31;
    uint32_t mantissa = rnd32() & 0x007FFFFFU;
    uint32_t exp = 0x3FEU + (rnd32() & 1U); /* f64 constants in an f32! */
    return sign | (exp << 23) | mantissa;
}

/* Fixed form (current tree, ff8ddec1): f32 bias-127 exponents. */
static uint32_t new_random_finite_f32(void)
{
    if ((rnd32() & 3) == 0)
        return F32_FINITE[rnd32() % 16];
    uint32_t sign = (rnd32() & 1U) << 31;
    uint32_t mantissa = rnd32() & 0x007FFFFFU;
    uint32_t exp = 0x7EU + (rnd32() & 1U);
    return sign | (exp << 23) | mantissa;
}

static void chain_scalar(const uint32_t *m, const uint32_t *a, uint32_t seed,
                         size_t lanes, size_t lane, uint32_t *out,
                         size_t *sat_step)
{
    float acc;
    uint32_t accw = seed;
    memcpy(&acc, &accw, 4);
    *sat_step = 512;
    for (size_t i = 0; i < 512; ++i) {
        float fm, fa;
        memcpy(&fm, &m[i * lanes + lane], 4);
        memcpy(&fa, &a[i * lanes + lane], 4);
        acc = std::fma(fm, fa, acc);
        if (*sat_step == 512 && !std::isfinite(acc))
            *sat_step = i;   /* first non-finite accumulator step */
    }
    memcpy(&accw, &acc, 4);
    *out = accw;
}

static void chain_sve(const uint32_t *m, const uint32_t *a,
                      const uint32_t *seeds, size_t lanes, uint32_t *out)
{
    const svbool_t pg = svptrue_b32();
    svfloat32_t acc = svld1_f32(pg, reinterpret_cast<const float *>(seeds));
    for (size_t i = 0; i < 512; ++i) {
        svfloat32_t vm = svld1_f32(pg, reinterpret_cast<const float *>(&m[i * lanes]));
        svfloat32_t va = svld1_f32(pg, reinterpret_cast<const float *>(&a[i * lanes]));
        acc = svmla_f32_x(pg, acc, vm, va);
    }
    svst1_f32(pg, reinterpret_cast<float *>(out), acc);
}

static void run_case(const char *tag, uint32_t (*gen)(void))
{
    const size_t lanes = svcntw();
    static uint32_t m[512 * 32], a[512 * 32], seeds[32], hw[32], gold[32];
    static size_t sat[32];

    size_t nan_cnt = 0, inf_cnt = 0, fin_cnt = 0, huge_cnt = 0;
    for (size_t i = 0; i < 512 * lanes; ++i) {
        m[i] = gen();
        a[i] = gen();
    }
    for (size_t i = 0; i < 512 * 2 * lanes; ++i) {  /* BOTH streams */
        const uint32_t *src = (i < 512 * lanes) ? &m[i] : &a[i - 512 * lanes];
        float v;
        memcpy(&v, src, 4);
        if (std::isnan(v)) nan_cnt++;
        else if (std::isinf(v)) inf_cnt++;
        else { fin_cnt++; if (v < -1e30f || v > 1e30f) huge_cnt++; }
    }
    for (size_t l = 0; l < lanes; ++l)
        seeds[l] = 0x3F800000u | (rnd32() & 0x007FFFFFu);

    for (size_t l = 0; l < lanes; ++l)
        chain_scalar(m, a, seeds[l], lanes, l, &gold[l], &sat[l]);
    chain_sve(m, a, seeds, lanes, hw);

    size_t mismatch = 0;
    for (size_t l = 0; l < lanes; ++l)
        if (gold[l] != hw[l]) mismatch++;

    size_t sat_min = 512, sat_max = 0, sat_sum = 0;
    for (size_t l = 0; l < lanes; ++l) {
        if (sat[l] < sat_min) sat_min = sat[l];
        if (sat[l] > sat_max) sat_max = sat[l];
        sat_sum += sat[l];
    }
    printf("[%s] VL=%zu bits, lanes=%zu\n", tag, lanes * 32, lanes);
    printf("[%s] operand composition (both streams, %zu draws): "
           "%zu NaN / %zu Inf / %zu finite (%zu of them |x|>1e30)\n",
           tag, (size_t)(1024 * lanes), nan_cnt, inf_cnt, fin_cnt, huge_cnt);
    printf("[%s] chain saturation (first non-finite acc step, 0-based): "
           "min=%zu mean=%zu max=%zu (512 = never)\n",
           tag, sat_min, sat_sum / lanes, sat_max);
    printf("[%s] scalar-fmadd golden vs SVE-FMLA after 512 steps: "
           "%zu/%zu lanes bit-EXACT %s\n", tag, lanes - mismatch, lanes,
           mismatch ? "MISMATCH -> SDC-detected" : "match");
    for (size_t l = 0; l < lanes && mismatch; ++l)
        if (gold[l] != hw[l])
            printf("[%s]   lane %zu golden=0x%08x actual=0x%08x xor=0x%08x\n",
                   tag, l, gold[l], hw[l], gold[l] ^ hw[l]);
}

int main(void)
{
    printf("== pre-ff8ddec1 generator (exp = 0x3FE/0x3FF << 23, 3d731d24) ==\n");
    run_case("old", old_random_finite_f32);
    printf("\n== fixed generator (exp = 0x7E/0x7F << 23, current tree) ==\n");
    run_case("new", new_random_finite_f32);
    return 0;
}
