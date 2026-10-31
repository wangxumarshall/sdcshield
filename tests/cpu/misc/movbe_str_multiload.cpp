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
};

static int movbe_init(struct test *test)
{
    movbe_data *data = (movbe_data *)malloc(sizeof(movbe_data));
    data->input   = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

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

    return EXIT_SUCCESS;
}

static int movbe_cleanup(struct test *test)
{
    movbe_data *data = (movbe_data *)test->data;
    if (data) {
        free(data->input);
        free(data->swapped);
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
