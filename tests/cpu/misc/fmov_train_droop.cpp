/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test fmov_train_droop
 * @parblock
 * Core-rail transient-droop excitation structure (2026-11-20, user
 * directive: make the fault machine's undervolt-only SDC detectable at
 * nominal voltage, by recreating the electrical condition with load
 * structure).
 *
 * Background (measured on the fault machine): the original
 * fmov_train_ldr_str/ldp_stp pair detects SDC ONLY under core-rail
 * undervolt (-85mV at default temperature, or -75mV at 90C); at nominal
 * voltage/temperature it never fires.  Mechanism: the undervolt eats
 * ~14% of the slack of the recipe's tightest path (the cross-register-
 * file FMOV bypass chain feeding the back-to-back str;str store-buffer
 * capture), pushing a latent critical path's setup margin through zero.
 * Frequency on that machine is already at its ceiling, so the missing
 * 85mV must come from a TRANSIENT local droop instead: an aggressor
 * that spikes core current so the victim's str;str capture window lands
 * inside the voltage dip.
 *
 * Structure (per chunk, alternating until the buffer pass completes):
 *   AGGRESSOR: burst_n FMAs as 8 parallel dependent FMA chains
 *              (rotating accumulators — both FP pipes saturated every
 *              cycle) with alternating high-Hamming-distance operands
 *              (power_virus_dit lineage: 1.0f/-2.0f, 4.0f/0.125f —
 *              max multiplier-input toggling = max instantaneous
 *              current / di/dt on the FP pipes fed from the core rail).
 *   GAP:       gap NOPs (phase adjust — lets the droop's leading edge
 *              reach the victim window; swept, not guessed).
 *   VICTIM:    chunk_m elements of the fmov_train_ldr_str recipe,
 *              instruction-for-instruction (2-source ldr + FMOV
 *              cross-file round-trip + slot-rotated add/eor/sub +
 *              back-to-back str/str), so the str;str store captures run
 *              right behind the burst.
 * A round covers the whole 32 KiB buffer in chunk_m chunks; after each
 * round the golden is recomputed in scalar C (parent semantics: slot
 * and rot rotate per round) and dstA/dstB are byte-compared — the
 * detection face is IDENTICAL to the parent.
 *
 * Knobs (read at init, -O fmov_train_droop.<knob>=<v>):
 *   burst  FMAs per aggressor burst        (default 512, clamp 16..65536)
 *   chunk  victim elements per adjacency   (default 64,  clamp 16..2048)
 *   gap    NOPs between burst and victim   (default 0,   clamp 0..255,
 *          ~98ns at 2.6GHz — covers the first-droop trough, 30..80ns)
 *   toggle 1 = worst-toggle aggressor: each FMA step veorq'd with an
 *          alternating all-0/all-ones mask (full input-net inversion
 *          per cycle on top of the FMA chain — one more notch of
 *          dynamic current; default 0 = plain alternating operands)
 * The phase space (burst x chunk x gap) is meant to be SWEPT by a
 * campaign script on the fault machine — the droop depth/period of a
 * server PDN is unknown, so the structure scans rather than guesses.
 *
 * Honest boundaries:
 *   - The aggressor's accumulator is sunk (XOR-reduced into a volatile
 *     sink so the chain cannot be optimized away) but NOT compared: it
 *     is an exciter by design.  FMA-path fault detection is power_virus_
 *     dit's job (its burst IS checkpoint-verified there); this test
 *     detects faults on the VICTIM path only.
 *   - Pre-registered verdict on a HEALTHY machine (this host): expected
 *     0 fail — negative-space data.  The value is on the fault machine
 *     at nominal voltage: if the undervolt mechanism is setup-margin
 *     exhaustion, some (burst, chunk, gap) point should fire.
 *   - The sink global is racy across threads by intent (never read for
 *     correctness).
 *
 * Provenance: victim = SIMDFPLoadStoreMemLoopCpyP08 extract behind
 * fmov_train_ldr_str (2026-11-18); aggressor = power_virus_dit burst
 * chain (2026-09 lineage, PROD-verified).  ARM64-only; x86-64 stub.
 * @endparblock
 */

#include "sandstone.h"
#include <test_knobs.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

#if defined(__aarch64__)
#include <arm_neon.h>

#define FTD_NUM_ELEMENTS 2048            /* 16B elements -> 32KB/buffer */
#define FTD_BYTES (FTD_NUM_ELEMENTS * 16)

/* knob defaults and clamps */
#define FTD_BURST_DEFAULT 512
#define FTD_BURST_MIN     16
#define FTD_BURST_MAX     65536
#define FTD_CHUNK_DEFAULT 64
#define FTD_CHUNK_MIN     16
#define FTD_CHUNK_MAX     2048
#define FTD_GAP_DEFAULT   0
#define FTD_GAP_MIN       0
#define FTD_GAP_MAX       255   /* ~98ns at 2.6GHz — covers the first-droop
                                  * trough window (30-80ns) end to end */

/* racy-by-intent sink: keeps the aggressor chain live; never read */
static volatile uint32_t ftd_sink;

namespace { /* ODR: per-TU FtdData */
struct FtdData {
    int burst;
    int chunk;
    int gap;
    int toggle;    /* 0 = alternating-FMA operands (power_virus_dit lineage);
                    * 1 = worst-toggle: every step veorq with an alternating
                    * all-0/all-~0 mask — full input-net inversion per cycle
                    * (max dynamic current on top of the FMA chain) */
};
}

/*
 * AGGRESSOR: 8 PARALLEL dependent FMA chains (rotating-accumulator power
 * virus).  power_virus_dit's single chain keeps only one FMA in flight
 * per ~4-cycle latency (25% pipe duty); 8 chains — each internally
 * dependent (deterministic ordering), mutually independent — saturate
 * both FP pipes every cycle while preserving the proven alternating
 * high-Hamming operands per step (multiplier input nets toggle
 * maximally: 1.0f/-2.0f = 0x3f800000/0xc0000000, 4.0f/0.125f =
 * 0x40800000/0x3e000000).  Total FMAs = 8 * floor(n/8); results are
 * XOR-reduced into the caller's sink accumulator (exciter: kept live,
 * never compared).  Magnitude growth +3.75 per pair keeps values finite
 * (no NaN/Inf) for any n <= 65536.
 *
 * toggle=1 (worst-toggle mode, 2026-11-20 v2): each chain step is
 * followed by veorq with an alternating all-0 / all-ones mask — the
 * accumulator's input nets fully invert every cycle on top of the FMA
 * toggling.  The veorq result feeds the next FMA's addend input, so the
 * chain stays dependent and the values stay finite (xor of finite
 * bit patterns reinterpreted as float can produce huge/NaN exponents —
 * but the FMA then only adds a*b=+3.75/-0.25 per step, and the FINAL
 * reduction is the raw xor, never used as a float result).  The point
 * is current, not arithmetic: every input net of the SIMD unit flips
 * every cycle.
 */
static inline uint32_t ftd_burst(int n, int toggle)
{
    const float32x4_t a0 = vdupq_n_f32(1.0f);     /* 0x3f800000 */
    const float32x4_t a1 = vdupq_n_f32(-2.0f);    /* 0xc0000000 */
    const float32x4_t b0 = vdupq_n_f32(4.0f);     /* 0x40800000 */
    const float32x4_t b1 = vdupq_n_f32(0.125f);   /* 0x3e000000 */
    const uint32x4_t m0 = vdupq_n_u32(0x00000000u);
    const uint32x4_t m1 = vdupq_n_u32(0xffffffffu);
    float32x4_t c0 = vdupq_n_f32(0.0f);
    float32x4_t c1 = vdupq_n_f32(0.0f);
    float32x4_t c2 = vdupq_n_f32(0.0f);
    float32x4_t c3 = vdupq_n_f32(0.0f);
    float32x4_t c4 = vdupq_n_f32(0.0f);
    float32x4_t c5 = vdupq_n_f32(0.0f);
    float32x4_t c6 = vdupq_n_f32(0.0f);
    float32x4_t c7 = vdupq_n_f32(0.0f);
    int n8 = n / 8;
    if (toggle) {
        for (int i = 0; i < n8; ++i) {
            const uint32x4_t m = (i & 1) ? m1 : m0;
            c0 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c0, a0, b0)), m));
            c1 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c1, a1, b1)), m));
            c2 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c2, a0, b0)), m));
            c3 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c3, a1, b1)), m));
            c4 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c4, a0, b0)), m));
            c5 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c5, a1, b1)), m));
            c6 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c6, a0, b0)), m));
            c7 = vreinterpretq_f32_u32(veorq_u32(
                    vreinterpretq_u32_f32(vfmaq_f32(c7, a1, b1)), m));
        }
    } else {
        for (int i = 0; i < n8; ++i) {
            c0 = vfmaq_f32(c0, a0, b0);
            c1 = vfmaq_f32(c1, a1, b1);
            c2 = vfmaq_f32(c2, a0, b0);
            c3 = vfmaq_f32(c3, a1, b1);
            c4 = vfmaq_f32(c4, a0, b0);
            c5 = vfmaq_f32(c5, a1, b1);
            c6 = vfmaq_f32(c6, a0, b0);
            c7 = vfmaq_f32(c7, a1, b1);
        }
    }
    uint32x4_t x = vreinterpretq_u32_f32(c0);
    x = veorq_u32(x, vreinterpretq_u32_f32(c1));
    x = veorq_u32(x, vreinterpretq_u32_f32(c2));
    x = veorq_u32(x, vreinterpretq_u32_f32(c3));
    x = veorq_u32(x, vreinterpretq_u32_f32(c4));
    x = veorq_u32(x, vreinterpretq_u32_f32(c5));
    x = veorq_u32(x, vreinterpretq_u32_f32(c6));
    x = veorq_u32(x, vreinterpretq_u32_f32(c7));
    return vaddvq_u32(x);
}

/* GAP: g NOPs, phase adjust between burst and victim (not eliminable:
 * each is a volatile asm). */
static inline void ftd_gap(int g)
{
    for (int i = 0; i < g; ++i)
        __asm__ volatile("nop");
}

/*
 * VICTIM chunk: the fmov_train_ldr_str hot loop instruction-for-
 * instruction (same registers, same slot rotation), starting at element
 * start_elem and running count elements.  Post-index advance; pointers
 * are local to the chunk.
 */
static void ftd_victim_chunk(const uint64_t *a, const uint64_t *b,
                             uint64_t *da, uint64_t *db, uint64_t rot,
                             int slot, int start_elem, int count)
{
    const uint64_t *pa = a + (size_t)start_elem * 2;
    const uint64_t *pb = b + (size_t)start_elem * 2;
    uint64_t *pda = da + (size_t)start_elem * 2;
    uint64_t *pdb = db + (size_t)start_elem * 2;

    for (int i = 0; i < count; ++i) {
        if (slot == 0) {
            __asm__ volatile(
                "ldr  q0, [%[a]], #16\n\t"
                "ldr  q1, [%[b]], #16\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "add  v6.2d, v1.2d, v4.2d\n\t"
                "str  q6, [%[da]], #16\n\t"
                "str  q6, [%[db]], #16\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v4", "v6", "x4", "x5", "x6", "x7", "memory");
        } else if (slot == 1) {
            __asm__ volatile(
                "ldr  q0, [%[a]], #16\n\t"
                "ldr  q1, [%[b]], #16\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "eor  v6.16b, v1.16b, v4.16b\n\t"
                "str  q6, [%[da]], #16\n\t"
                "str  q6, [%[db]], #16\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v4", "v6", "x4", "x5", "x6", "x7", "memory");
        } else {
            __asm__ volatile(
                "ldr  q0, [%[a]], #16\n\t"
                "ldr  q1, [%[b]], #16\n\t"
                "fmov x4, d0\n\t"
                "fmov x5, v0.d[1]\n\t"
                "add  x6, x4, %[rot]\n\t"
                "add  x7, x5, %[rot]\n\t"
                "fmov d4, x6\n\t"
                "fmov v4.d[1], x7\n\t"
                "sub  v6.2d, v1.2d, v4.2d\n\t"
                "str  q6, [%[da]], #16\n\t"
                "str  q6, [%[db]], #16\n\t"
                : [a] "+r" (pa), [b] "+r" (pb), [da] "+r" (pda), [db] "+r" (pdb)
                : [rot] "r" (rot)
                : "v0", "v1", "v4", "v6", "x4", "x5", "x6", "x7", "memory");
        }
    }
}

/* scalar bit-exact golden: g = b <op> (a + rot) — parent formula verbatim */
static void ftd_golden(const uint64_t *a, const uint64_t *b,
                       uint64_t *g, uint64_t rot, int slot)
{
    size_t n = FTD_NUM_ELEMENTS * 2;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i] + rot;
        g[i] = (slot == 0) ? (b[i] + x)
             : (slot == 1) ? (b[i] ^ x)
             :               (b[i] - x);
    }
}

static int fmov_train_droop_init(struct test *test)
{
    auto *d = new FtdData;
    if (!d)
        return EXIT_FAILURE;

    int64_t v;
    v = get_testspecific_knob_value_int(test, "burst", FTD_BURST_DEFAULT);
    d->burst = (int)v;
    if (d->burst < FTD_BURST_MIN) d->burst = FTD_BURST_MIN;
    if (d->burst > FTD_BURST_MAX) d->burst = FTD_BURST_MAX;

    v = get_testspecific_knob_value_int(test, "chunk", FTD_CHUNK_DEFAULT);
    d->chunk = (int)v;
    if (d->chunk < FTD_CHUNK_MIN) d->chunk = FTD_CHUNK_MIN;
    if (d->chunk > FTD_CHUNK_MAX) d->chunk = FTD_CHUNK_MAX;

    v = get_testspecific_knob_value_int(test, "gap", FTD_GAP_DEFAULT);
    d->gap = (int)v;
    if (d->gap < FTD_GAP_MIN) d->gap = FTD_GAP_MIN;
    if (d->gap > FTD_GAP_MAX) d->gap = FTD_GAP_MAX;

    v = get_testspecific_knob_value_int(test, "toggle", 0);
    d->toggle = (v != 0) ? 1 : 0;

    test->data = d;
    return EXIT_SUCCESS;
}

static int fmov_train_droop_cleanup(struct test *test)
{
    delete static_cast<FtdData *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int fmov_train_droop_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<FtdData *>(test->data);
    const int burst = d->burst;
    const int chunk = d->chunk;
    const int gap = d->gap;
    const int toggle = d->toggle;

    /* per-thread buffers (parent geometry: 32KB each, L1D-domain
     * tight-timing supply — the victim must stay in the fast domain);
     * std::vector's heap block is 16B-aligned = ldr/str q access size */
    std::vector<uint8_t> srcAv(FTD_BYTES), srcBv(FTD_BYTES);
    std::vector<uint8_t> dstAv(FTD_BYTES), dstBv(FTD_BYTES), goldv(FTD_BYTES);
    auto *srcA = reinterpret_cast<uint64_t *>(srcAv.data());
    auto *srcB = reinterpret_cast<uint64_t *>(srcBv.data());
    auto *dstA = reinterpret_cast<uint64_t *>(dstAv.data());
    auto *dstB = reinterpret_cast<uint64_t *>(dstBv.data());
    auto *gold = reinterpret_cast<uint64_t *>(goldv.data());

    /* randomization hardening H14': framework RNG fill (bit patterns) */
    memset_random(srcAv.data(), FTD_BYTES);
    memset_random(srcBv.data(), FTD_BYTES);
    memset(dstAv.data(), 0, FTD_BYTES);
    memset(dstBv.data(), 0, FTD_BYTES);

    uint32_t sink_acc = 0;

    uint64_t round = 0;
    do {
        const int slot = (int)(round % 3);
        const uint64_t rot = 0x9E3779B97F4A7C15ULL + round;

        /* one buffer pass in chunk_m aggressor/victim adjacencies; the
         * tail chunk covers the remainder when 2048 % chunk != 0 */
        for (int start = 0; start < FTD_NUM_ELEMENTS; start += chunk) {
            const int m = (chunk < FTD_NUM_ELEMENTS - start)
                        ? chunk : (FTD_NUM_ELEMENTS - start);
            sink_acc ^= ftd_burst(burst, toggle);
            ftd_gap(gap);
            ftd_victim_chunk(srcA, srcB, dstA, dstB, rot, slot, start, m);
        }

        ftd_golden(srcA, srcB, gold, rot, slot);

        if (memcmp(dstA, gold, FTD_BYTES) != 0 || memcmp(dstB, gold, FTD_BYTES) != 0) {
            for (size_t i = 0; i < FTD_NUM_ELEMENTS * 2; ++i) {
                if (dstA[i] != gold[i]) {
                    report_fail_msg("fmov_train_droop: dstA mismatch at "
                                    "element %zu (round %lu, slot %d, "
                                    "burst %d chunk %d gap %d): "
                                    "expected 0x%016lx got 0x%016lx "
                                    "(droop-excited FMOV consumer chain)",
                                    i / 2, (unsigned long)round, slot,
                                    burst, chunk, gap,
                                    (unsigned long)gold[i],
                                    (unsigned long)dstA[i]);
                    return EXIT_FAILURE;
                }
            }
        }
        ++round;
    } while (test_time_condition(test));

    /* keep the aggressor chain live (see header: sink, never compared) */
    ftd_sink = sink_acc;

    return EXIT_SUCCESS;
}

DECLARE_TEST(fmov_train_droop, "Core-rail transient-droop excitation: dependent NEON FMA burst (aggressor, power_virus_dit lineage, output sunk not compared) + adjustable gap + fmov_train_ldr_str recipe chunk (victim, instruction-for-instruction, byte-exact per-round scalar golden); knobs burst/chunk/gap (-O) sweep the droop phase (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = fmov_train_droop_init,
    .test_run = fmov_train_droop_run,
    .test_cleanup = fmov_train_droop_cleanup,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST

#else  /* !__aarch64__ */

static int fmov_train_droop_init(struct test *test)  { (void)test; return EXIT_SUCCESS; }
static int fmov_train_droop_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SUCCESS; }
static int fmov_train_droop_cleanup(struct test *test) { (void)test; return EXIT_SUCCESS; }

DECLARE_TEST(fmov_train_droop, "Core-rail transient-droop excitation (ARM64-only)")
    .test_init    = fmov_train_droop_init,
    .test_run     = fmov_train_droop_run,
    .test_cleanup = fmov_train_droop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif
