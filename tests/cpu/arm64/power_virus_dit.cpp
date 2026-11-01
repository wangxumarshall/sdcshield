/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b power_virus_dit
 * @parblock
 * di/dt voltage-transient power-virus SDC test (research dimension "版图三
 * 瞬态故障动态激发"). The research explicitly calls out: "write code that
 * causes violent local power-consumption swings (a power virus) — e.g. run the
 * NEON (SIMD) unit at full load for 10 cycles, then fully stall for 10 cycles,
 * alternating. The violent current change (di/dt) causes local voltage droop
 * (Voltage Droop), at which point otherwise-correct timing collapses and a
 * transient SDC is very likely to be excited."
 *
 * Static GATE coverage only catches stuck-at faults; most SDC arises from
 * transient faults (voltage droop, thermal noise, crosstalk). No existing
 * test in the suite implements the burst/stall alternation pattern — every
 * other test runs a steady-state loop. This is the first di/dt power-virus.
 *
 * Structure of one cycle:
 *   BURST: a tight *dependent* NEON FMA chain — acc_{k} = vfmaq(acc_{k-1},
 *          a_k, b_k) — where the operands a_k/b_k *alternate between two
 *          high-Hamming-distance values every step* (e.g. 1.0f / -2.0f,
 *          bit patterns 0x3f800000 / 0xc0000000). The dependency keeps the
 *          FMA pipe back-to-back (max instantaneous current), and the
 *          alternating operands force the multiplier input nets to toggle
 *          maximally every cycle (max switching activity / di/dt). The chain
 *          runs BURST_INNER iterations with no test_time_condition() inside.
 *   STALL: a tight loop of ARM64 "yield" hints (pipeline spin), draining the
 *          vector/SIMD units to near-zero activity for STALL_INNER iterations.
 *   BURST and STALL alternate until the test's time budget elapses.
 *
 * Running full-system (all cores) maximises the di/dt current swing (many
 * cores bursting/stalling in lockstep amplifies the droop).
 *
 * SDC detection (issue #376): each BURST's accumulator is compared
 * byte-exact against a software reference that repeats the identical
 * dependent FMA sequence with libm fmaf (single rounding per step,
 * IEEE-754) at every 64-step boundary (32 checkpoints; the last IS the
 * burst-final state). Because the chain is dependent and the operands
 * are fixed, every boundary state is deterministic and bit-identical
 * between NEON and libm — so a 1-bit flip anywhere in the burst (a
 * transient fault corrupting a vector register or the FMA path
 * mid-burst) shows up as a mismatch at the next checkpoint, before the
 * burst's monotonic magnitude growth (+3.75/pair) grinds the flip away
 * by rounding — the absorption blind zone of the old final-only
 * compare. (The
 * previous version discarded the burst result and checked only a single
 * detached post-burst FMA, so a mid-burst fault was largely uncaught; and its
 * "toggle" was `veorq_u32(bits, bits)` which is identically 0 — a no-op that
 * delivered no switching activity at all.)
 *
 * ARM64-native (NEON vfma + yield); on non-aarch64 _run returns EXIT_SKIP.
 * Wired into the arm64 subdir (entered only under the aarch64 meson guard), so
 * x86-64 is untouched.
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <cmath>
#include <limits>

#ifdef __aarch64__
#include <arm_neon.h>
#endif

// Number of NEON operations in a burst before the stall. Chosen large enough
// to build real current / switching activity but small enough that a single
// burst is well under the framework's loop-check granularity.
static constexpr int BURST_INNER = 2048;

// Segmented intermediate checkpoints (issue #376): the burst accumulator
// is byte-compared at SEG_LEN-step boundaries (32 segments per 2048-step
// burst), not only at burst end. The burst's accumulation is a pure
// +3.75-per-pair growth (0 -> ~3840), so a mid-burst low-bit flip is
// ground away by rounding once the magnitude grows past it — a
// final-only compare is blind to that class. The boundary spill is 32
// vst1q per 2048 FMAs (1.6%) and does not break the dependent chain.
static constexpr int SEG_LEN = 64;
static constexpr int SEG_COUNT = BURST_INNER / SEG_LEN;
// Number of yield hints in a stall. Roughly matched to the burst length so
// the burst/stall duty cycle is ~50% (maximises di/dt swing).
static constexpr int STALL_INNER = 2048;

static int power_virus_dit_init(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__

// Per-burst operand pairs. Two high-Hamming-distance float pairs alternate
// every step of the FMA chain: the multiplier input nets flip between very
// different bit patterns each cycle (max switching activity / di/dt), while
// the FMA stays a dependent back-to-back chain (acc_{k} = acc_{k-1} + a_k*b_k).
// All values are well-defined (no NaN/Inf) so the FMA path is determinate and
// the software fmaf reference is bit-exact. Bit patterns:
//   A0 = 1.0f  (0x3f800000),  A1 = -2.0f (0xc0000000)   — high Hamming
//   B0 = 4.0f  (0x40800000),  B1 = 0.125f(0x3e000000)   — high Hamming
static const float BURST_A0 = 1.0f;
static const float BURST_A1 = -2.0f;
static const float BURST_B0 = 4.0f;
static const float BURST_B1 = 0.125f;
static const float BURST_C  = 0.0f;   // initial accumulator

// The dependent FMA burst. Spills the accumulator to seg_out at every
// SEG_LEN-step boundary (issue #376); the last entry IS the burst-final
// state, so the old final-only compare is subsumed. A 1-bit fault in the
// burst corrupts the next boundary snapshot while it is still supra-ulp.
static inline void burst_fma_chain_seg(int n, uint32_t (*seg_out)[4])
{
    float32x4_t a0 = vdupq_n_f32(BURST_A0);
    float32x4_t a1 = vdupq_n_f32(BURST_A1);
    float32x4_t b0 = vdupq_n_f32(BURST_B0);
    float32x4_t b1 = vdupq_n_f32(BURST_B1);
    float32x4_t acc = vdupq_n_f32(BURST_C);
    // Unrolled by 2 so each loop body does one (a0,b0) step then one (a1,b1)
    // step — the alternating operands that maximise multiplier-input toggling.
    int n2 = n / 2;
    for (int i = 0; i < n2; ++i) {
        acc = vfmaq_f32(acc, a0, b0);   // acc += A0*B0
        acc = vfmaq_f32(acc, a1, b1);   // acc += A1*B1  (operands toggle)
        if ((i + 1) % (SEG_LEN / 2) == 0)   /* issue #376 checkpoint spill */
            vst1q_u32(seg_out[2 * (i + 1) / SEG_LEN - 1],
                      vreinterpretq_u32_f32(acc));
    }
}

// Software reference for the burst: identical dependent fmaf sequence,
// recording the same SEG_LEN-step boundary states.
static inline void burst_fma_reference_seg(int n, uint32_t *seg_out)
{
    float acc = BURST_C;
    int n2 = n / 2;
    for (int i = 0; i < n2; ++i) {
        acc = fmaf(BURST_A0, BURST_B0, acc);
        acc = fmaf(BURST_A1, BURST_B1, acc);
        if ((i + 1) % (SEG_LEN / 2) == 0)
            memcpy(&seg_out[2 * (i + 1) / SEG_LEN - 1], &acc, 4);
    }
}

static inline void stall_yield(int n)
{
    // Drain the vector/SIMD units to near-zero activity. "yield" is the ARM
    // hint that signals a spin-wait; it lets the core throttle down, which is
    // exactly the low-activity half of the di/dt cycle.
    for (int i = 0; i < n; ++i) {
        __asm__ volatile("yield" ::: "memory");
    }
}

static int power_virus_dit_run(struct test *test, int cpu)
{
    (void)cpu;
    (void)test;

    // Software reference for the burst's SEG_LEN-step boundary states
    // (issue #376; the last entry is the burst-final accumulator).
    uint32_t sw_seg[SEG_COUNT];
    burst_fma_reference_seg(BURST_INNER, sw_seg);
    uint32_t hw_seg[SEG_COUNT][4];

    do {
        bool all_passed = true;

        // A power-virus cycle: BURST then STALL, alternating. We run a few
        // burst/stall pairs per outer iteration before re-checking the time
        // budget, so the di/dt swing is sustained.
        for (int cycle = 0; cycle < 8; ++cycle) {
            // ---- BURST: dependent FMA chain, alternating high-Hamming
            // operands, no time-check inside. Its SEG_LEN-step boundary
            // snapshots ARE the SDC check targets — a transient fault
            // anywhere in the burst corrupts the next snapshot while it
            // is still supra-ulp, before the growth grinds it away.
            burst_fma_chain_seg(BURST_INNER, hw_seg);

            // Byte-exact compare of all 4 lanes at every boundary. A 1-bit
            // flip in any lane of the burst accumulator is caught at the
            // next 64-step checkpoint.
            for (int seg = 0; seg < SEG_COUNT; ++seg) {
                for (int lane = 0; lane < 4; ++lane) {
                    if (hw_seg[seg][lane] != sw_seg[seg]) {
                        all_passed = false;
                        log_warning("power_virus_dit: burst cycle %d seg %d "
                                    "(after step %d) lane %d golden=0x%08x "
                                    "actual=0x%08x", cycle, seg,
                                    (seg + 1) * SEG_LEN, lane, sw_seg[seg],
                                    hw_seg[seg][lane]);
                        break;
                    }
                }
                if (!all_passed)
                    break;
            }
            if (!all_passed)
                break;

            // ---- STALL: near-zero activity, drain SIMD units.
            stall_yield(STALL_INNER);
        }

        if (!all_passed) {
            report_fail_msg("power_virus_dit: di/dt transient SDC detected "
                            "(NEON FMA corrupted mid burst/stall cycle)");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

#else

static int power_virus_dit_run(struct test *test, int cpu)
{
    (void)cpu;
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for "
             "power-virus di/dt stress");
    return EXIT_SKIP;
}

#endif // __aarch64__

static int power_virus_dit_finish(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(power_virus_dit,
             "di/dt voltage-transient power virus: alternating NEON full-load "
             "bursts with yield stalls to induce voltage droop and excite "
             "transient SDC on the ARM64 SIMD/FMA path")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = power_virus_dit_init,
    .test_run = power_virus_dit_run,
    .test_cleanup = power_virus_dit_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
