/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_h
 * @parblock
 * SDC-trigger probe, variant H: **sweep the store-side distinct-line footprint**
 * to find the threshold at which core-179's defect turns on. Distinguishes a
 * fixed-size resource (MSHR / prefetch / write-combine entries -> threshold
 * near a power of two) from an open-ended footprint effect.
 *
 * Probes A-G established the trigger needs a store stream with a LARGE
 * number of DISTINCT, CONTIGUOUSLY-ADVANCING lines, back-to-back with the
 * reload, in the same LLC domain. Crucially G1 (16 lines, all in ONE LLC
 * set, 16-way over-subscription) PASSED -- disproving single-set eviction.
 * So the surviving variable is the store-side distinct-LINE count. Probe H
 * sweeps it: store to `swapped[(i % LINES) * (LINE/4)]`, where LINES is set
 * by the PROBE_H_LINES env var (default 512 = baseline-equivalent). The
 * stride is one LLC line (128B), so the LINES stores hit LINES distinct
 * consecutive sets (1 way each -- no set over-subscription, unlike G1),
 * and the store address advances into a NEW line every iteration.
 *
 * Sweep points: 32, 64, 128, 256, 512 (512 == baseline footprint, must FAIL).
 * 16 (==probe G2) is known PASS. The threshold line count where the defect
 * turns on pins the resource: e.g. a turn-on near 128/256 distinct lines
 * points at a bounded MSHR/prefetcher pool of that size.
 *
 * Reload of `data->input[i]` uses the full `i` (512 LLC lines, identical to
 * baseline) and is the SDC site. Verified by objdump that the reload is
 * byte-identical to baseline and store<->reload stay back-to-back (the
 * modulo+scale schedule before the 1st ldr, not between store and reload).
 *
 * NOTE: LINES must be a power of two in [1..512]; the store index uses
 * `i & (LINES*32 - 1)` (NOT a runtime modulo) so no `udiv`/`msub` lands on
 * the hot path -- the inner-loop timing stays as close to baseline as
 * possible (only a single `and` mask, scheduled before the 1st ldr). This
 * keeps the store<->reload back-to-back window intact AND avoids the
 * multi-cycle division perturbing per-iteration throughput. init() rejects
 * non-power-of-two LINES values so the mask is always valid.
 *
 * CRITICAL: the stride is 4B (baseline-style) NOT 128B. The store writes
 * 32 words per LLC line before advancing, exactly like baseline. So:
 *   LINES=512 -> mask=16383 -> swapped[i & 16383] == swapped[i] (i in 0..16383)
 *                == BASELINE byte-for-byte. This is the positive control and
 *                MUST fail on core 179; if it doesn't, the probe is invalid.
 * Smaller LINES shrink the distinct-line footprint while keeping the 4B
 * stride and per-line fill pattern identical to baseline.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdlib>

#define MOVBE_BUFFER_SIZE (1 << 14)          /* input words, identical to baseline */
#define PROBE_H_LINE 128                    /* LLC line size on core 179 */
/* swapped holds up to 512 lines * 128B = 64KB (== baseline footprint max). */
#define PROBE_H_MAX_LINES 512
#define PROBE_H_SWAP_WORDS (PROBE_H_MAX_LINES * (PROBE_H_LINE / 4))  /* 512*32 = 16384 */

static uint32_t g_probe_h_mask = PROBE_H_MAX_LINES * (PROBE_H_LINE / 4) - 1;  /* i & mask; default 16383 = baseline */
static uint32_t g_probe_h_lines = PROBE_H_MAX_LINES;      /* for logging only */

struct movbe_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
};

/* issue #374: identity LUT for the scalar-independent byte-swap golden.
 * The table loads block GCC's bswap-idiom recognizer from folding the
 * shift/or reassembly back into a REV instruction -- the exact unit under
 * test -- so a persistent REV mispermutation can never corrupt golden and
 * actual identically. Pinned by scripts/check-bswap-golden.sh (zero
 * rev/rev16/rev32 and zero vector-ALU inside the init symbol). */
static uint8_t g_bswap_lut[256];
static void movbe_golden_lut_init(void)
{
    for (int b = 0; b < 256; ++b)
        g_bswap_lut[b] = (uint8_t)b;
}
static uint32_t movbe_golden_bswap32(uint32_t x)
{
    return ((uint32_t)g_bswap_lut[x & 0xff] << 24) |
           ((uint32_t)g_bswap_lut[(x >> 8) & 0xff] << 16) |
           ((uint32_t)g_bswap_lut[(x >> 16) & 0xff] << 8) |
           (uint32_t)g_bswap_lut[x >> 24];
}

static int movbe_init(struct test *test)
{
    /* Allow overriding the distinct-line count at runtime. Must be a power
     * of two in [1..512] so the store index `i & (LINES*32 - 1)` is a simple
     * mask (no division on the hot path, preserving baseline 4B-stride timing). */
    if (const char *env = getenv("PROBE_H_LINES")) {
        long v = strtol(env, nullptr, 10);
        if (v >= 1 && v <= PROBE_H_MAX_LINES && (v & (v - 1)) == 0) {
            g_probe_h_lines = (uint32_t)v;
            g_probe_h_mask = (uint32_t)v * (PROBE_H_LINE / 4) - 1;
        }
    }

    movbe_data *data = (movbe_data *)malloc(sizeof(movbe_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, PROBE_H_SWAP_WORDS * sizeof(uint32_t));
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the REV under test). */
    movbe_golden_lut_init();
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_run(struct test *test, int cpu)
{
    movbe_data *data = (movbe_data *)test->data;
    const size_t lines = g_probe_h_lines;          /* distinct store lines (logging) */
    const size_t mask = g_probe_h_mask;            /* i & mask = i & (lines*32 - 1) */

    TEST_LOOP(test, 1 << 13) {
        // Hot loop: store index = swapped[i & mask], stride 4B (baseline-style).
        // For LINES=512, mask=16383 and i in 0..16383, so this is EXACTLY
        // swapped[i] == baseline. For smaller LINES the store cycles a smaller
        // region of `lines` distinct LLC lines but keeps the 4B stride and
        // 32-words-per-line fill pattern identical to baseline. Reload of
        // input[i] uses the full `i` (512 LLC lines, identical to baseline)
        // and is the SDC site. The `and` mask schedules before the 1st ldr
        // (verified by objdump) so store<->reload stay back-to-back, and
        // there is NO division on the hot path.
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            // PROBE H: store footprint = `lines` distinct LLC lines, 4B stride.
            data->swapped[i & mask] = val;

            val = __builtin_bswap32(val);

            if (val != data->input[i]) {
                uint32_t inputv = data->input[i];
                uint32_t golden = data->input[i];
                uint32_t actual = val;
                uint32_t xorv = golden ^ actual;
                unsigned char *ib = (unsigned char *)&inputv;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                log_error("MovBE-probeH(lines=%u): Round-trip failed at index %u: "
                          "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
                          "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
                          "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X",
                          (unsigned)lines, (unsigned)i, inputv, golden, actual, xorv,
                          ib[3], ib[2], ib[1], ib[0],
                          gb[3], gb[2], gb[1], gb[0],
                          ab[3], ab[2], ab[1], ab[0],
                          xb[3], xb[2], xb[1], xb[0]);
                report_fail_msg("MovBE: Round-trip failed at index %u", (unsigned)i);
            }
        }
    }

    return EXIT_SUCCESS;
}

static int movbe_cleanup(struct test *test)
{
    movbe_data *data = (movbe_data *)test->data;
    if (data) {
        /* issue #374: post-join verification of the stored intermediate, in
         * the probe's own footprint geometry. The store `swapped[i & mask]`
         * cycles mask+1 word slots (mask+1 = lines*32 divides
         * MOVBE_BUFFER_SIZE for every legal LINES value), so slot s's last
         * writer is element MOVBE_BUFFER_SIZE-(mask+1)+s; once every worker
         * has finished (cleanup runs post-join) the slot content must equal
         * that element's LUT golden. The check MUST be post-join: the slot
         * value depends on which element a worker is currently at, so a
         * run-side sweep would race with still-running workers. With the
         * default LINES=512 (mask+1 == MOVBE_BUFFER_SIZE) every slot has a
         * single element writer, so value-identity would make a run-side
         * sweep safe too -- but non-default LINES recycles slots, and this
         * single post-join form covers both. The hot loop is unchanged; a
         * persistent REV mispermutation (symmetric fault: applied twice it
         * cancels in the round-trip identity) leaves every slot wrong and
         * is caught here. */
        const size_t sweep_mask = g_probe_h_mask;  /* same global the run used */
        for (size_t s = 0; s <= sweep_mask; ++s) {
            size_t last_elem = MOVBE_BUFFER_SIZE - (sweep_mask + 1) + s;
            uint32_t actual = data->swapped[s];
            if (actual != data->expected_swapped[last_elem]) {
                log_error("MovBE-probeH(lines=%u): swapped-intermediate miscompare "
                          "at slot %u (last writer element %u): input=0x%08X "
                          "golden=0x%08X actual=0x%08X xor=0x%08X",
                          (unsigned)g_probe_h_lines, (unsigned)s, (unsigned)last_elem,
                          data->input[last_elem], data->expected_swapped[last_elem],
                          actual, data->expected_swapped[last_elem] ^ actual);
                report_fail_msg("MovBE: swapped slot != independent golden at slot %u",
                                (unsigned)s);
            }
        }
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_probe_h, "movbe_dump probe H: sweep store-side distinct LLC-line footprint (PROBE_H_LINES env; stride=128B=1 line/set, 1 way each) to find defect-on threshold")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
