/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_str_multiload
 * @parblock
 * Diagnostic variant of movbe: str->ldr ; str->ldr (two independent
 * str-swapped[i] -> reload-input[i] pairs), comparing the two reloads (a, b)
 * against each other and against input[i].
 *
 * Path note (critical): the stores are to swapped[i] but the reloads read
 * input[i] -- a DIFFERENT address. This is exactly the path baseline movbe
 * triggers on (see movbe_dump.cpp:70-76: the compiler folds the second bswap
 * into a reload of input[i] because the store to swapped[i] may alias
 * input). That DIFFERENT-address reload is served by the L1D array, not by
 * store-buffer forwarding. An earlier build of this test reloaded swapped[i]
 * (same-address, forwarding path) and triggered 0/15min -- the same-address
 * forwarding path has a far lower 179 trigger rate (~0.067/min) than the
 * L1D-array reload path (~1/min); this version restores the high-rate path.
 *
 * Form note (critical, the 0-fail fix): an even earlier build used the form
 * str/ldr/ldr (one store then two reloads). That triggered 0/15min even while
 * baseline movbe hit 36/15min in the same window -- because the 2nd ldr's
 * predecessor was another ldr (a load-queue forward, not a fresh L1D array
 * read) AND it perturbed the 1st ldr's schedule. This str->ldr;str->ldr form
 * gives EACH reload its own preceding str, so both are genuine str->ldr
 * trigger-form reloads; the 2nd str is semantically redundant (rewrites the
 * same value) but exists solely to rebuild the str->ldr adjacency for reload #2.
 *
 * Purpose: separate the two candidate SDC mechanisms on core 179 that the
 * V5/V6 localization reports could not split with single-load tests:
 *
 *   - If both reloads agree with each other but DISAGREE with input[i]
 *     => the fault is STORE/forwarding-side and CONSISTENT (the value is
 *        stably wrong at the delivery end). Under V6's "pattern-dependent
 *        hardware defect" model this is the "activated pattern" case: every
 *        reload of this address/pattern is wrong the same way.
 *
 *   - If the two reloads DISAGREE among themselves (a != b)
 *     => the fault is LOAD-path and TRANSIENT (the array holds the correct
 *        value, but a single load delivery was corrupted). This is the
 *        "timing-window single-shot" case of V6's model.
 *
 * Hot loop is a single inline-asm block so the compiler cannot fold or
 * reorder the reloads. The str-swapped -> reload-input adjacency is kept
 * tight (no nops) to hold the 179 trigger window (<=2 cycle) open; the 2nd
 * reload samples the same array-read path to classify store-side vs load-side.
 *
 * Caveat (per movbe_dump.cpp): adding the 2nd reload changes the instruction
 * schedule versus the single-reload movbe. This test is a DIAGNOSTIC, not a
 * daily gate -- its trigger rate may be somewhat lower than movbe's, and a 0
 * in 5 min is not evidence the defect is absent. Its value is in the FAIL
 * classification, not the fail count.
 *
 * The input buffer is filled once at init with random32() and reused every
 * iteration (identical to movbe.cpp / movbe_dump.cpp), so the LCG seed fully
 * determines the bit pattern under test.
 * @endparblock
 */

#include "sandstone.h"

#define MOVBE_BUFFER_SIZE (1 << 14)
#define MULTILOAD_N       2

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
    data->input   = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the REV under test).
     * Note the in-loop `expected = __builtin_bswap32(golden)` below is
     * deliberately left on the same REV unit: it is part of the diagnostic
     * classification path whose hot-loop schedule must not change; the
     * persistent-fault coverage comes from this post-loop golden, not from
     * the in-loop compare. */
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
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t a, b, golden;

            // str-ldr ; str-ldr : each reload is preceded by its OWN store so
            // BOTH reloads sit in the genuine str->ldr trigger form (<=2cyc
            // window, diff-address L1D-array read), not a degenerate
            // load->load same-address re-read. The 2nd str rewrites the same
            // golden -- semantically redundant, its sole purpose is to rebuild
            // the str->ldr adjacency that feeds reload #2. This is the fix for
            // the 0-fail failure: the previous build did str/ldr/ldr, where the
            // 2nd ldr's predecessor was a ldr (load-queue forward, not an L1D
            // array read) AND it perturbed the 1st ldr's schedule; here each
            // ldr is a fresh str->ldr trigger so a failure classifies cleanly:
            // a==b!=input => STORE-side (stable wrong delivery), a!=b =>
            // LOAD-side (single transient corrupt delivery).
            __asm__ volatile(
                "ldr  %w[g], [%[inp]]\n\t"   /* load input[i]            */
                "rev  %w[g], %w[g]\n\t"      /* golden = bswap(input[i]) */
                "str  %w[g], [%[swp]]\n\t"   /* store #1 -> swapped     */
                "ldr  %w[a], [%[inp]]\n\t"   /* reload input[i] #1 (str->ldr) */
                "str  %w[g], [%[swp]]\n\t"   /* store #2 (redundant; rebuild str->ldr) */
                "ldr  %w[b], [%[inp]]\n\t"   /* reload input[i] #2 (str->ldr) */
                : [a] "=&r"(a), [b] "=&r"(b),
                  [g] "=&r"(golden)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );

            // golden == input[i] (the value both reloads must reproduce;
            // golden happens to be bswapped but we compare a/b against the
            // RELOADED input, so a correct core sees a == b == input[i]).
            // Note: a/b are raw reloads of input[i], golden is bswapped --
            // so the direct compare is a == b == bswap_inverse(golden), i.e.
            // a == bswap(golden). We just compare a==b (consistency) and
            // a==bswap(golden) (correctness).
            uint32_t expected = __builtin_bswap32(golden); /* == input[i] */
            if (a == expected && b == expected)
                continue;

            // Classify the failure.
            if (a == b && a != expected) {
                // Both reloads agree, but disagree with input[i]: the wrong
                // value is STABLE at the delivery end -> store/forwarding
                // side, consistent fault (array/forwarding delivers a wrong
                // value every time).
                uint32_t xorv = expected ^ a;
                log_error("MovBE-multiload: STORE-side consistent fault at "
                          "index %u: expected=0x%08X actual=0x%08X xor=0x%08X "
                          "(both reloads agree, disagree with input)",
                          (unsigned)i, expected, a, xorv);
            } else {
                // Reloads disagree among themselves: the array holds the
                // right value but a single load delivery was corrupted ->
                // load path, transient fault. Report which reloads diverged.
                log_error("MovBE-multiload: LOAD-side transient fault at "
                          "index %u: expected=0x%08X "
                          "a=0x%08X%s b=0x%08X%s",
                          (unsigned)i, expected,
                          a, a == expected ? "" : "!",
                          b, b == expected ? "" : "!");
            }
            report_fail_msg("MovBE: multi-load miscompare at index %u",
                             (unsigned)i);
        }
    }

    /* issue #374: post-loop verification of the stored intermediate. The asm
     * hot loop (both str->ldr trigger windows) is untouched; every iteration
     * stores the same rev(input[i]) to swapped[i], so the final content must
     * equal the init-time LUT golden. A persistent REV mispermutation makes
     * the asm rev, the in-loop C bswap, AND the stored bytes all wrong in
     * the same involutive way -- the in-loop a==expected==b check cancels it
     * (bswap(bswap) identity) -- but this byte-exact check against the
     * independently computed golden cannot be fooled by it. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-multiload: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      (unsigned)i, data->input[i], data->expected_swapped[i],
                      data->swapped[i], data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("MovBE: swapped buffer != independent golden at index %u",
                            (unsigned)i);
        }
    }

    return EXIT_SUCCESS;
}

static int movbe_cleanup(struct test *test)
{
    movbe_data *data = (movbe_data *)test->data;
    if (data) {
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_str_multiload, "movbe diagnostic: store swapped[i] + 2 reloads of DIFFERENT addr input[i] (L1D-array path, baseline's 179 trigger path), classifies store-side (consistent) vs load-side (transient) SDC")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
