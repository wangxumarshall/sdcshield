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
static const uint8_t g_bswap_lut[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
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
    bool workers_ran;
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
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    data->workers_ran = false;   /* issue #374 review: cleanup-sweep gate */

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_run(struct test *test, int cpu)
{
    movbe_data *data = (movbe_data *)test->data;

    data->workers_ran = true;

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
        /* workers_ran gate (issue #374 review): if every worker SKIPped (e.g.
         * a refused SVE VL pin under seccomp/gem5-SE), the buffers were never
         * written and comparing them against the golden would false-FAIL. */
        uint32_t expected = data->expected_swapped[MOVBE_BUFFER_SIZE - 1];
        uint32_t actual = probe_d_sink;
        if (data->workers_ran && actual != expected) {
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
