/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_d
 * @parblock
 * SDC-trigger probe, variant D: **store to a compile-time-constant global
 * address** (decouples store-LFU-occupancy from any cache-line relationship
 * with the input buffer).
 *
 * The baseline movbe_dump hot loop is:
 *   ldr input[i] -> rev -> str swapped[i] -> ldr input[i](reload) -> cmp
 * Probes A (no store) and B (nops after store) both PASS, proving the store
 * and its tight timing to the reload are trigger conditions. But in the
 * baseline the store writes to `swapped[i]`, a different cache line from
 * input[i] -- so the store does NOT dirty the input line. The open question
 * is whether the trigger is:
 *   (a) the store's LSU / store-buffer *occupancy/timing* (address-
 *       independent), or
 *   (b) some *cache-line* effect of the store (address-dependent).
 *
 * Probe D isolates (a): the store target is a single compile-time-constant
 * global `probe_d_sink` whose address has no relationship to data->input.
 * This keeps the store instruction and its LSU/store-buffer timing
 * identical to the baseline, while removing any cache-line coupling between
 * the store and the reloaded input address. The reload of data->input[i]
 * still happens (forced via volatile), so if the defect is in the reload's
 * LSU-contention window, probe D should STILL trigger like the baseline.
 * If probe D passes, the store's address/cache-line matters and the
 * "contention window" theory is wrong.
 *
 * NOTE: every core writes the same global sink, so there is cross-core
 * coherence traffic on that one line. That is the *point* here -- it keeps
 * store-buffer/LSU activity present and address-decoupled from input. The
 * expected healthy behavior is still reload == 1st-read on every core.
 * @endparblock
 */

#include "sandstone.h"

#define MOVBE_BUFFER_SIZE (1 << 14)

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

struct movbe_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
};

// Compile-time-constant store target: decoupled from data->input cache line.
// volatile so the store is never dead-code-eliminated.
static volatile uint32_t probe_d_sink = 0;

static int movbe_init(struct test *test)
{
    movbe_data *data = (movbe_data *)malloc(sizeof(movbe_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
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

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            // PROBE D: store to a compile-time-constant global (not input,
            // not swapped[i]). Keeps the store's LSU/store-buffer timing
            // identical to the baseline but removes cache-line coupling to
            // the reloaded input address. The reload of data->input[i] is
            // forced via volatile so the defect-triggering reload is
            // preserved exactly.
            volatile uint32_t *vinput = &data->input[i];
            uint32_t val = *vinput;
            val = __builtin_bswap32(val);

            probe_d_sink = val;          // store to constant global address

            val = __builtin_bswap32(val);

            uint32_t input_now = *vinput;
            if (val != input_now) {
                uint32_t inputv = *vinput;
                uint32_t golden = val;
                uint32_t actual = input_now;
                uint32_t xorv = golden ^ actual;
                unsigned char *ib = (unsigned char *)&inputv;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                log_error("MovBE-probeD: Round-trip failed at index %u: "
                          "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
                          "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
                          "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X",
                          (unsigned)i, inputv, golden, actual, xorv,
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
        /* issue #374: verify the sink's final content against the independent
         * LUT golden. The sink is a single global written in element order by
         * every worker, so its value is only stable after the last worker's
         * inner loop completes (here, post-join); every worker's final write
         * is bswap32(input[MOVBE_BUFFER_SIZE-1]). A persistent REV
         * mispermutation (symmetric fault: applied twice it cancels in the
         * round-trip identity) leaves the wrong byte order here and is caught
         * byte-exactly. The hot loop above is unchanged. */
        uint32_t expected = data->expected_swapped[MOVBE_BUFFER_SIZE - 1];
        uint32_t actual = probe_d_sink;
        if (actual != expected) {
            log_error("MovBE-probeD: sink != independent golden: "
                      "input[last]=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      data->input[MOVBE_BUFFER_SIZE - 1], expected, actual,
                      expected ^ actual);
            report_fail_msg("MovBE-probeD: sink != independent golden");
        }
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_probe_d, "movbe_dump probe D: store to constant global (LSU timing kept, cache-line coupling removed)")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
