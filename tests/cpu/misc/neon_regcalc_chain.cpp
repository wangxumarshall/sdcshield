/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_regcalc_chain
 * @parblock
 * Pure register-resident NEON compute chain (2026-11-28, Eaglebu
 * mixed-pressure axis — the zero-memory extreme).
 *
 * WHY THIS AXIS EXISTS: every other NEON piece in this tree couples its
 * vector work to memory traffic, so a fault in the vector pipes (or in
 * the RF/bypass fabric feeding them) is never exercised in isolation —
 * LSU behavior dominates the timing and the vector datapath is a
 * passenger. This piece runs an 8-accumulator LCG-in-registers chain
 * with ZERO memory access in the hot loop: acc_i = acc_i * M32 + A32
 * (per-lane wrapping u32, all lanes independent) across 8 × uint32x4
 * accumulators. Honest build note: GCC contracts the mul+add pair into
 * MLA — the step is 8 in-register vector MACs (plus mov plumbing), same
 * vector-pipe class as the 16-op form, disclosed here. The dependent
 * chain defeats move-elimination folding; eight interleaved accumulators
 * defeat value-forwarding shortcuts. It is the companion extreme of
 * mix_pressure_spectrum's tier sweep — a hit here (with the ratio
 * spectrum silent at its pure-mem tiers) points the fault at the vector
 * unit / RF rather than cache/MMU.
 *
 * HOT LOOP HAS NO MEMORY OPS (the gate IS the piece): all state lives
 * in v-registers between round-end checks — 8 accumulators + 2 constant
 * vectors, well inside the 32-register budget; the objdump gate
 * verifies the step block contains zero ldr/str/ldp/stp. The round-end
 * check (every R steps, cold path, disclosed) snapshots the registers to
 * a stack array and reads the golden from test->data — the ONLY memory
 * traffic outside init. The first draft kept the accumulators in a
 * C ARRAY; the objdump gate caught it living on the stack (4 ldp + 4
 * stp per step — the exact Review-Focus spill), and the fix is the
 * named-local shape, not a relaxed gate.
 *
 * CONSTANTS: M32 = 0x41C64E6D, A32 = 0x9E3779B9 — classic LCG
 * parameters, odd multiplier (bijective per lane), deterministic and
 * exactly replayable scalar-side. Honest note: chosen for high nonzero
 * bit positions, NOT for a measured maximal toggle rate.
 *
 * PER-WORKER PHASE + PER-WORKER GOLDEN (design rulings over the plan
 * sketch): seeds are XOR-phased with cpu * 0x9E3779B9 so cores exercise
 * different data values, which breaks a single init-time golden; each
 * worker therefore scalar-replays its OWN phased chain once at run
 * start (~131K scalar ops, one-shot) to build its golden. Deterministic
 * u32 modular arithmetic is bit-exact between vector and scalar paths.
 * The golden is the SINGLE-ROUND terminal state: the chain re-anchors
 * to the phased seed state every round (cold-path reload) — the first
 * real run caught the drift when the chain was left running past
 * round 1 (live state f^8192(seed) vs golden f^4096(seed), failed at
 * round 1 exactly as the math says), and re-anchoring keeps the
 * golden valid forever.
 *
 * DETECTION: round-end lane-exact compare, fail message carries the
 * accumulator index, lane index, raw actual/expected values, round and
 * cpu — register-direct, no re-scan dependency.
 *
 * TEST_LOOP granularity 1<<14: the condition path costs ~0.3-0.5us in
 * kernel time (measured 2026-11-28, mix_pressure_spectrum ruling) — at
 * 16 ops/step the batch keeps the duty cycle >99%.
 *
 * Zero asm; NEON intrinsics only. quality_level BETA.
 * @endparblock
 */

#include "sandstone.h"

#include <cstdint>
#include <cstdio>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace {

constexpr unsigned RC_ACCS = 8;          /* 8 × uint32x4 accumulators */
constexpr uint64_t RC_STEPS_PER_ROUND = 4096;
constexpr uint32_t RC_M32 = 0x41C64E6Du; /* LCG multiplier (odd, per plan) */
constexpr uint32_t RC_A32 = 0x9E3779B9u; /* LCG addend */

struct RegCalcData {
    uint32_t seeds[RC_ACCS][4];  /* random32 material, init-fixed */
};

} // namespace

static int neon_regcalc_chain_init(struct test *test)
{
    auto *d = static_cast<RegCalcData *>(calloc(1, sizeof(RegCalcData)));
    if (!d)
        return -ENOMEM;

    for (unsigned i = 0; i < RC_ACCS; ++i)
        for (unsigned l = 0; l < 4; ++l)
            d->seeds[i][l] = random32();

    test->data = d;
    return EXIT_SUCCESS;
}

static int neon_regcalc_chain_run(struct test *test, int cpu)
{
    auto *d = static_cast<RegCalcData *>(test->data);

    const uint32x4_t mvec = vdupq_n_u32(RC_M32);
    const uint32x4_t avec = vdupq_n_u32(RC_A32);

    /* Per-worker phase (cpu-diversified data) + per-worker golden:
     * each worker scalar-replays its own phased chain once — u32
     * modular arithmetic is bit-exact against the vector path.
     *
     * The accumulators are NAMED LOCALS, not an array: an x[i]-indexed
     * array lives in memory (objdump gate caught 4 ldp + 4 stp per step
     * in the first draft — the Review-Focus spill, fixed by shape, not
     * by relaxing the gate). Eight named accumulators + two constant
     * vectors sit well inside the 32 v-register budget. */
    const uint32_t phase = static_cast<uint32_t>(cpu) * RC_A32;
    uint32_t golden[RC_ACCS][4];
    uint32x4_t start[RC_ACCS];   /* round-start state: every round replays
                                  * the SAME one-round trajectory (the golden
                                  * is the single-round terminal — the chain
                                  * must re-anchor each round or the live
                                  * state drifts past it; caught by the very
                                  * first real run, round 1) */
    auto seed_acc = [&](unsigned i) -> uint32x4_t {
        uint32_t l[4];
        for (unsigned lidx = 0; lidx < 4; ++lidx) {
            l[lidx] = d->seeds[i][lidx] ^ phase;
            uint32_t g = l[lidx];
            for (uint64_t s = 0; s < RC_STEPS_PER_ROUND; ++s)
                g = g * RC_M32 + RC_A32;
            golden[i][lidx] = g;
        }
        return uint32x4_t{l[0], l[1], l[2], l[3]};
    };
    for (unsigned i = 0; i < RC_ACCS; ++i)
        start[i] = seed_acc(i);
    uint32x4_t acc0 = start[0], acc1 = start[1], acc2 = start[2],
                acc3 = start[3], acc4 = start[4], acc5 = start[5],
                acc6 = start[6], acc7 = start[7];

    uint64_t steps = 0;
    unsigned long rounds = 0;

    TEST_LOOP(test, 1 << 14) {
        /* one step: 8 in-register vector MACs, zero memory */
        acc0 = vaddq_u32(vmulq_u32(acc0, mvec), avec);
        acc1 = vaddq_u32(vmulq_u32(acc1, mvec), avec);
        acc2 = vaddq_u32(vmulq_u32(acc2, mvec), avec);
        acc3 = vaddq_u32(vmulq_u32(acc3, mvec), avec);
        acc4 = vaddq_u32(vmulq_u32(acc4, mvec), avec);
        acc5 = vaddq_u32(vmulq_u32(acc5, mvec), avec);
        acc6 = vaddq_u32(vmulq_u32(acc6, mvec), avec);
        acc7 = vaddq_u32(vmulq_u32(acc7, mvec), avec);

        if (++steps == RC_STEPS_PER_ROUND) {
            /* round-end cold-path check: snapshot the registers to a
             * stack array HERE (once per 4096 steps — the only memory
             * traffic this piece has outside init), then all 32 lanes
             * vs golden. */
            const uint32x4_t snap[RC_ACCS] = {
                acc0, acc1, acc2, acc3, acc4, acc5, acc6, acc7
            };
            for (unsigned i = 0; i < RC_ACCS; ++i) {
                const uint32_t act[4] = {
                    vgetq_lane_u32(snap[i], 0), vgetq_lane_u32(snap[i], 1),
                    vgetq_lane_u32(snap[i], 2), vgetq_lane_u32(snap[i], 3)
                };
                for (unsigned l = 0; l < 4; ++l) {
                    if (act[l] != golden[i][l]) {
                        report_fail_msg("neon_regcalc_chain: cpu %d accumulator "
                                        "%u lane %u mismatch after round %lu: "
                                        "actual 0x%08x expected 0x%08x (pure "
                                        "register-resident chain, zero memory "
                                        "in the hot loop)",
                                        cpu, i, l, rounds, act[l], golden[i][l]);
                        return EXIT_FAILURE;
                    }
                }
            }
            steps = 0;
            ++rounds;
            /* re-anchor: the next round replays the same one-round
             * trajectory from the phased seed state (cold-path reload) */
            acc0 = start[0]; acc1 = start[1]; acc2 = start[2];
            acc3 = start[3]; acc4 = start[4]; acc5 = start[5];
            acc6 = start[6]; acc7 = start[7];
        }
    }

    return EXIT_SUCCESS;
}

static int neon_regcalc_chain_cleanup(struct test *test)
{
    free(static_cast<RegCalcData *>(test->data));
    test->data = nullptr;
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_regcalc_chain, "Register-resident pure NEON compute chain (Eaglebu vector-unit axis, zero-memory extreme): 8 x uint32x4 accumulators run x = x*M32 + A32 per-lane wrapping chains, 16 vector ops per step, ZERO ldr/str in the hot loop (objdump-gated) — vector pipes and RF/bypass exercised with no memory coupling; per-worker cpu-phased data with per-worker scalar-replay golden; lane-exact round-end checks with acc/lane/raw/round/cpu register-direct signature")
    .test_init    = neon_regcalc_chain_init,
    .test_run     = neon_regcalc_chain_run,
    .test_cleanup = neon_regcalc_chain_cleanup,
    .fracture_loop_count = -1,   /* never fracture: round 是完整性单元 */
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
