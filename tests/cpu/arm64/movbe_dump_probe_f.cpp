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
    bool workers_ran;
};

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
        /* workers_ran gate (issue #374 review): if every worker SKIPped (e.g.
         * a refused SVE VL pin under seccomp/gem5-SE), the buffers were never
         * written and comparing them against the golden would false-FAIL. */
        for (size_t s = 0; data->workers_ran && s < 16; ++s) {
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
