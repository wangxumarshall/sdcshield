/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_f
 * @parblock
 * SDC-trigger probe, variant F: **store address advances but NEVER crosses a
 * cache line** (isolates "cross-line stepping" from "advance per se", holding
 * the same-LLC-domain condition constant).
 *
 * Baseline movbe_dump hot loop:
 *   ldr input[i] -> rev -> str swapped[i] -> ldr input[i](reload) -> cmp
 * Probe E proved the store must stay in the SAME LLC domain as the reload
 * (swapped on a remote NUMA node -> PASS). Probe D proved a FIXED-address
 * store -> PASS, but probe D's store target was a static global whose
 * physical NUMA node was uncontrolled (likely off-domain), so D confounds
 * "fixed address" with "off-domain". The open question: does the store need
 * to advance ACROSS cache lines, or is any advancing address enough?
 *
 * Probe F keeps the baseline structure byte-for-byte (same allocation via
 * aligned_alloc = first-touch, so core 179's input AND swapped land on node 7,
 * the SAME LLC domain as baseline; same reload of input[i]) and changes
 * EXACTLY ONE source line: the store target goes from `swapped[i]` to
 * `swapped[i & 15]`. Now the store address still advances every iteration
 * (0,4,8,...,60,0,4,... -- 16 distinct word addresses) but is confined to the
 * FIRST 64-byte cache line of swapped. It NEVER crosses into a new line. The
 * reload `data->input[i]` is untouched (full i, advancing across all 16384
 * words = 1024 input lines, exactly as baseline), and is the SDC site.
 *
 * Decision tree:
 *   - PASS -> crossing cache lines is REQUIRED (within-line advance is
 *     insufficient). The advancing store must step across distinct lines /
 *     distinct LLC sets. Strongly supports an LLC-set / store-buffer eviction
 *     interaction as the mechanism (one line = one/few LLC sets = no
 *     cross-set eviction pressure).
 *   - FAIL -> within-line advance is sufficient; cross-line is NOT required.
 *     Points to AGU / store-buffer-replay activity rather than LLC-set
 *     eviction.
 *
 * NOTE: `swapped[i & 15]` forces the compiler to keep the reload of
 * `data->input[i]` (it cannot prove swapped[0..15] does not alias input[i],
 * since both are opaque uint32_t* from the same struct). Verified by objdump
 * that the reload remains `ldr w4, [x1, x19, lsl #2]` and that store<->reload
 * are still back-to-back (the `and` mask schedules before the store, not
 * between store and reload).
 * @endparblock
 */

#include "sandstone.h"

#define MOVBE_BUFFER_SIZE (1 << 14)

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
        // Hot loop is byte-for-byte identical to baseline movbe_dump EXCEPT
        // the store index: `swapped[i & 15]` confines the store to ONE cache
        // line (bytes 0..60 of swapped). The store address still advances
        // every iteration (16 distinct word slots) but never crosses a line.
        // The reload `data->input[i]` below uses the full `i` and is the
        // SDC site -- kept identical to baseline.
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            // PROBE F: store within one cache line (i & 15), not swapped[i].
            // Address advances 0->4->...->60->0->... but stays in line 0.
            data->swapped[i & 15] = val;

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
                log_error("MovBE-probeF: Round-trip failed at index %u: "
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
        /* issue #374: post-join verification of the stored intermediate, in
         * the probe's own slot geometry. The store `swapped[i & 15]` maps 16
         * store slots; slot s's last writer is element
         * MOVBE_BUFFER_SIZE-16+s (16384 is a multiple of 16), so once every
         * worker has finished (cleanup runs post-join) the slot content must
         * equal that element's LUT golden. The check MUST be post-join: the
         * slot value depends on which element a worker is currently at, so a
         * run-side sweep would race with still-running workers and read
         * their in-flight (different-element) values. The hot loop is
         * unchanged; a persistent REV mispermutation (symmetric fault:
         * applied twice it cancels in the round-trip identity) leaves every
         * slot wrong and is caught here. */
        for (size_t s = 0; s < 16; ++s) {
            size_t last_elem = MOVBE_BUFFER_SIZE - 16 + s;
            if (data->swapped[s] != data->expected_swapped[last_elem]) {
                log_error("MovBE-probeF: swapped-intermediate miscompare at slot %u "
                          "(last writer element %u): input=0x%08X golden=0x%08X "
                          "actual=0x%08X xor=0x%08X",
                          (unsigned)s, (unsigned)last_elem, data->input[last_elem],
                          data->expected_swapped[last_elem], data->swapped[s],
                          data->expected_swapped[last_elem] ^ data->swapped[s]);
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

DECLARE_TEST(movbe_dump_probe_f, "movbe_dump probe F: store advances within ONE cache line (cross-line stepping isolated; same-LLC-domain held constant)")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
