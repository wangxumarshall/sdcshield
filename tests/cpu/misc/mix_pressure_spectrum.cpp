/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mix_pressure_spectrum
 * @parblock
 * Compute:memory ratio-spectrum probe (2026-11-28, Eaglebu mixed-pressure
 * axis).
 *
 * WHY THIS AXIS EXISTS: pure SIMD load/store traffic leaves the vector
 * pipes idle (LSU stalls dominate), and conversely a pure-compute loop
 * never touches the memory hierarchy — a fault whose excitation needs
 * BOTH pressure kinds at once (issue/occupancy + pipe duty-cycle) hides
 * from either extreme. This probe sweeps the mixing ratio in one body:
 * five tiers rotate per round, each round sweeps the whole 1024-slot tag
 * region once (1024 × 16B = 16 KiB, L1-resident — deliberately in the
 * near-core domain per the 2026-11-27 negative report; this piece does
 * NOT reopen the residency axis).
 *
 *   tier 0  pure-mem   : ldr q + str q per slot visit, K = 0
 *   tier 1  K = 1      : 1 const vector-op pair per visit on the loaded word
 *   tier 2  K = 4
 *   tier 3  K = 16
 *   tier 4  PURE-CALC  : zero memory traffic; K = 16 on rotating constant
 *                        sources (same op shape as tier 3, no ldr/str)
 *
 * The excitation-rate-vs-tier profile IS the discriminator: a fault that
 * fires only at high-K tiers points at the vector pipes, only at tier 0
 * at the LSU path, and only when both are present at an interaction
 * (bypass/forwarding under mixed pressure) — Eaglebu's "cache/MMU does
 * not need SIMD; vector unit does not need memory" decoupling, made
 * measurable in one test body. Fail messages always carry the tier.
 *
 * DETECTION SURFACE (three layers, all register-direct):
 *   1. per-visit tag validation against a SEPARATE expected-value copy
 *      (db_load_addr_capture lineage: exp_lo/exp_hi arrays — the copy is
 *      what defeats load-CSE and encodes the same-address re-load
 *      recipe: a transient in-flight corruption reads back different
 *      from the pristine copy). Dual-lane self-describing tags:
 *      lane0 = MAGIC<<48|slot<<16|GEN, lane1 = MAGIC2<<48|(slot^GEN)<<16|GEN2
 *      — a mismatched readback decodes to the slot the load actually
 *      visited (stale-previous / all-zero / wrong-value triage).
 *   2. round-end full memcmp(scratch, tags): every slot is stored once
 *      per mem round; a lost/corrupted store leaves the witness.
 *   3. round-end accumulator golden per tier.
 *
 * ORDER-INVARIANT GOLDEN (design correction over the first draft): the
 * K const-op pairs transform the consumed value LOCALLY per visit —
 * t = f_K(v), acc = acc + t — so the round sum is a plain wrapping sum
 * over per-slot transforms. Accumulating the const pairs straight into
 * the running accumulator instead would make the sum order-dependent
 * (+ and ^ do not associate across visits), which cannot have a single
 * golden across per-worker phase offsets. Same instruction count and
 * pressure shape; the sum becomes commutative.
 *
 * Zero asm (structure is the pin — no zero-gap window needed here);
 * NEON intrinsics only, per the 2026-11-27 minimal-asm directive.
 *
 * quality_level BETA: new-design exciter, no field validation yet.
 * @endparblock
 */

#include "sandstone.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace {

constexpr uint64_t MPS_SLOTS  = 1ull << 10;        /* 1024 × 16B = 16 KiB, L1-resident */
constexpr uint64_t MPS_MASK   = MPS_SLOTS - 1;
constexpr uint64_t MPS_BYTES  = MPS_SLOTS * 16;
constexpr unsigned  MPS_TIERS = 5;

/* K = const vector-op pairs per visit; PURE tier reuses K=16 with zero
 * memory traffic (TIER_MEM false) so both extremes share one shape. */
constexpr unsigned MPS_TIER_K[MPS_TIERS]   = {0, 1, 4, 16, 16};
constexpr bool     MPS_TIER_MEM[MPS_TIERS] = {true, true, true, true, false};

struct MixSpecData {
    uint64x2_t *tags = nullptr;   /* init-fixed, run read-only */
    uint64_t *exp_lo = nullptr;   /* separate expected copies (defeat load-CSE) */
    uint64_t *exp_hi = nullptr;
    uint64_t magic = 0, magic2 = 0, gen = 0, gen2 = 0;
    uint64_t c1_lo = 0, c1_hi = 0;  /* const vector-op pair C1 (+) */
    uint64_t c2_lo = 0, c2_hi = 0;  /* const vector-op pair C2 (^) */
    uint64_t rc1 = 0, rc2 = 0;      /* PURE-tier rotating sources */
    uint64x2_t golden[MPS_TIERS] = {};
};

} // namespace

/* f_K(v): K 次 {v += C1; v ^= C2} 的逐 lane 标量等价（金身重放用） */
static void mps_transform_scalar(uint64_t &lo, uint64_t &hi,
                                 const MixSpecData *d, unsigned k)
{
    for (unsigned j = 0; j < k; ++j) {
        lo = (lo + d->c1_lo) ^ d->c2_lo;
        hi = (hi + d->c1_hi) ^ d->c2_hi;
    }
}

static int mix_pressure_spectrum_init(struct test *test)
{
    auto *d = static_cast<MixSpecData *>(calloc(1, sizeof(MixSpecData)));
    if (!d)
        return -ENOMEM;

    d->tags = static_cast<uint64x2_t *>(aligned_alloc(64, MPS_BYTES));
    d->exp_lo = static_cast<uint64_t *>(calloc(MPS_SLOTS, sizeof(uint64_t)));
    d->exp_hi = static_cast<uint64_t *>(calloc(MPS_SLOTS, sizeof(uint64_t)));
    if (!d->tags || !d->exp_lo || !d->exp_hi) {
        test->data = d;
        return -ENOMEM;
    }

    /* Random material from the framework RNG, fixed for the whole run. */
    d->magic  = random64() & 0xFFFFull;
    d->magic2 = random64() & 0xFFFFull;
    d->gen    = random64() & 0xFFFFull;
    d->gen2   = random64() & 0xFFFFull;
    d->c1_lo  = random64();  d->c1_hi  = random64();
    d->c2_lo  = random64();  d->c2_hi  = random64();
    d->rc1    = random64();  d->rc2    = random64();

    for (uint64_t s = 0; s < MPS_SLOTS; ++s) {
        const uint64_t lo = (d->magic << 48) | (s << 16) | d->gen;
        const uint64_t hi = (d->magic2 << 48) | ((s ^ d->gen) << 16) | d->gen2;
        d->tags[s] = uint64x2_t{lo, hi};
        d->exp_lo[s] = lo;
        d->exp_hi[s] = hi;
    }

    /* Per-tier order-invariant golden: a wrapping sum over per-slot
     * transforms f_K(v); mem tiers consume the tags, the PURE tier the
     * rotating constants (visit index 0..1023, any order). */
    for (unsigned t = 0; t < MPS_TIERS; ++t) {
        uint64_t glo = 0, ghi = 0;
        const unsigned k = MPS_TIER_K[t];
        for (uint64_t i = 0; i < MPS_SLOTS; ++i) {
            uint64_t lo, hi;
            if (MPS_TIER_MEM[t]) {
                lo = d->exp_lo[i];
                hi = d->exp_hi[i];
            } else {
                lo = d->rc1 + i;
                hi = d->rc2 ^ i;
            }
            mps_transform_scalar(lo, hi, d, k);
            glo += lo;   /* wrapping u64 add — order-invariant */
            ghi += hi;
        }
        d->golden[t] = uint64x2_t{glo, ghi};
    }

    test->data = d;
    return EXIT_SUCCESS;
}

static int mix_pressure_spectrum_run(struct test *test, int cpu)
{
    auto *d = static_cast<MixSpecData *>(test->data);
    const uint64x2_t c1 = uint64x2_t{d->c1_lo, d->c1_hi};
    const uint64x2_t c2 = uint64x2_t{d->c2_lo, d->c2_hi};

    std::vector<uint64x2_t> scratch(MPS_SLOTS);  /* per-thread store face */

    uint64_t idx = static_cast<uint64_t>(cpu) & MPS_MASK;  /* per-worker phase */
    uint64_t visits = 0;
    unsigned long rounds = 0;
    unsigned tier = 0;                     /* tier = round % 5 */
    uint64x2_t acc_vec = uint64x2_t{0, 0};

    /* 粒度 1<<14（批处理教训：condition 调用路径内核开销 ~0.3-0.5μs——实测
     * TEST_LOOP(64) 粒度下占空比仅 4-7%（sys 主导），1<<14 使开销占比 <2%，
     * 压力谱件的使命就是占空比本身） */
    TEST_LOOP(test, 1 << 14) {
        uint64x2_t t;
        if (MPS_TIER_MEM[tier]) {
            const uint64_t s = idx;
            const uint64x2_t got = d->tags[s];
            const uint64_t lo = vgetq_lane_u64(got, 0);
            const uint64_t hi = vgetq_lane_u64(got, 1);
            if (lo != d->exp_lo[s] || hi != d->exp_hi[s]) {
                /* The readback tag decodes to the slot the load ACTUALLY
                 * visited (capture-lineage triage) — plus the tier, since
                 * excitation may be tier-dependent. */
                const uint64_t got_slot = (lo >> 16) & MPS_MASK;
                const uint64_t prev_slot = (s + MPS_SLOTS - 1) & MPS_MASK;
                const char *verdict;
                if (lo == 0 && hi == 0)
                    verdict = "ALL-ZERO readback";
                else if (got_slot == prev_slot)
                    verdict = "STALE capture (previous slot)";
                else
                    verdict = "WRONG-VALUE capture";
                report_fail_msg("mix_pressure_spectrum: tier %u (K=%u), expected "
                                "slot %llu, but the load visited slot %llu (%s); "
                                "readback 0x%016llx:0x%016llx, expected "
                                "0x%016llx:0x%016llx, visit %llu round %lu",
                                tier, MPS_TIER_K[tier],
                                static_cast<unsigned long long>(s),
                                static_cast<unsigned long long>(got_slot),
                                verdict,
                                static_cast<unsigned long long>(lo),
                                static_cast<unsigned long long>(hi),
                                static_cast<unsigned long long>(d->exp_lo[s]),
                                static_cast<unsigned long long>(d->exp_hi[s]),
                                static_cast<unsigned long long>(visits),
                                rounds);
                return EXIT_FAILURE;
            }
            scratch[s] = got;             /* store face (memcmp-checked) */
            t = got;                       /* consume rides on the load */
            idx = (idx + 1) & MPS_MASK;
        } else {
            /* PURE-CALC: rotating constant sources, zero memory */
            t = uint64x2_t{d->rc1 + visits, d->rc2 ^ visits};
        }
        const unsigned k = MPS_TIER_K[tier];
        for (unsigned j = 0; j < k; ++j) {
            t = vaddq_u64(t, c1);
            t = veorq_u64(t, c2);
        }
        acc_vec = vaddq_u64(acc_vec, t);
        ++visits;

        if (visits == MPS_SLOTS) {
            /* round end: store face + per-tier accumulator golden */
            if (MPS_TIER_MEM[tier] &&
                memcmp(scratch.data(), d->tags, MPS_BYTES) != 0) {
                uint64_t el = 0;
                while (el < MPS_SLOTS &&
                       vgetq_lane_u64(scratch[el], 0) == vgetq_lane_u64(d->tags[el], 0) &&
                       vgetq_lane_u64(scratch[el], 1) == vgetq_lane_u64(d->tags[el], 1))
                    ++el;
                report_fail_msg("mix_pressure_spectrum: scratch vs tags mismatch "
                                "after round %lu (tier %u K=%u): slot %llu "
                                "scratch 0x%016llx:0x%016llx expected 0x%016llx:0x%016llx",
                                rounds, tier, MPS_TIER_K[tier],
                                static_cast<unsigned long long>(el),
                                static_cast<unsigned long long>(vgetq_lane_u64(scratch[el], 0)),
                                static_cast<unsigned long long>(vgetq_lane_u64(scratch[el], 1)),
                                static_cast<unsigned long long>(vgetq_lane_u64(d->tags[el], 0)),
                                static_cast<unsigned long long>(vgetq_lane_u64(d->tags[el], 1)));
                return EXIT_FAILURE;
            }
            const uint64_t a0 = vgetq_lane_u64(acc_vec, 0);
            const uint64_t a1 = vgetq_lane_u64(acc_vec, 1);
            const uint64_t g0 = vgetq_lane_u64(d->golden[tier], 0);
            const uint64_t g1 = vgetq_lane_u64(d->golden[tier], 1);
            if (a0 != g0 || a1 != g1) {
                report_fail_msg("mix_pressure_spectrum: tier %u (K=%u) accumulator "
                                "mismatch after round %lu%s: lane0 acc 0x%016llx "
                                "golden 0x%016llx, lane1 acc 0x%016llx golden 0x%016llx",
                                tier, MPS_TIER_K[tier], rounds,
                                MPS_TIER_MEM[tier]
                                    ? " (per-slot tag checks and the scratch memcmp passed)"
                                    : " (pure-calc tier)",
                                static_cast<unsigned long long>(a0),
                                static_cast<unsigned long long>(g0),
                                static_cast<unsigned long long>(a1),
                                static_cast<unsigned long long>(g1));
                return EXIT_FAILURE;
            }
            visits = 0;
            ++rounds;
            tier = static_cast<unsigned>(rounds % MPS_TIERS);
            acc_vec = uint64x2_t{0, 0};
        }
    }

    return EXIT_SUCCESS;
}

static int mix_pressure_spectrum_cleanup(struct test *test)
{
    auto *d = static_cast<MixSpecData *>(test->data);
    if (d) {
        free(d->tags);
        free(d->exp_lo);
        free(d->exp_hi);
        free(d);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mix_pressure_spectrum, "Compute:memory ratio spectrum (Eaglebu mixed-pressure axis): 1024-slot 16B dual-lane self-describing tag region, L1-resident, five rotating tiers per round — pure-mem ldr+str, K={1,4,16} const vector-op pairs per visit on the loaded word, pure-calc K=16 on rotating constants with zero memory; per-slot tag validation against a separate expected copy (stale/zero/wrong triage) + round-end scratch memcmp + order-invariant per-tier accumulator golden; excitation-rate-vs-tier profile is the discriminator")
    .test_init    = mix_pressure_spectrum_init,
    .test_run     = mix_pressure_spectrum_run,
    .test_cleanup = mix_pressure_spectrum_cleanup,
    .fracture_loop_count = -1,   /* never fracture: round 是完整性单元 */
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
