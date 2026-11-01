/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_c
 * @parblock
 * SDC-trigger probe, variant C: **store-to-load-forwarding on the input
 * line**.
 *
 * Identical to movbe_dump.cpp except the store target is changed from
 * data->swapped[i] to data->input[i] itself. This forces the reload of
 * data->input[i] to go through the store-to-load-forwarding (SLF) path
 * (the just-written value forwarded from the store buffer) instead of a
 * plain cache read, because the store and load now hit the exact same
 * address on the same cache line.
 *
 * If the defect lives in the store-buffer / SLF forwarding path, this
 * variant should trigger *much* harder than the baseline (and possibly
 * with a different XOR signature). If the defect is in the plain cache
 * read path, this variant should still trigger similarly to (or less
 * than) the baseline.
 *
 * NOTE: storing the byte-swapped value back into data->input[i] changes the
 * input pattern every TEST_LOOP iteration (input becomes its own bswap).
 * To keep the per-iteration cost comparable we still do one bswap on the
 * way in and one on the way out; the stored value is the single-swapped
 * form. The round-trip property bswap(bswap(x)) == x still holds for the
 * comparison value `val`, so a healthy core still passes.
 *
 * THREAD-SAFETY (fixed): the baseline comment above ("a healthy core still
 * passes") only holds in the single-threaded case. Under `-n N` the framework
 * runs movbe_run on N threads sharing the SAME test->data buffer; if every
 * thread stores back into the shared data->input[i], a thread's reload of
 * input[i] can observe a value another thread wrote between its store and
 * load, so reloaded != val deterministically (the failure signature is
 * actual == original input, golden == bswap(input)). That is a test-harness
 * race, not an SDC. To preserve the SLF probe's intent (store-to-load
 * forwarding on the input line) without the multi-writer race, each thread
 * now operates on its OWN private copy of the input line: the shared
 * data->input is treated as a read-only seed, copied once per thread into a
 * thread_local buffer, and the store-back/reload happens on that private
 * copy. Single-threaded behavior is unchanged.
 * @endparblock
 */

#include "sandstone.h"
#include <cstring>

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

// Per-thread private input line. The shared data->input is now a read-only
// seed; each thread copies it into its own thread_local buffer on first use
// and does the store-back/reload there, eliminating the multi-writer race on
// the shared buffer. See THREAD-SAFETY note in the file header.
static thread_local struct {
    uint32_t *input;        // private per-thread SLF target
    bool initialized;
} tls;

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

    // Lazily set up this thread's private input line from the shared seed.
    // The private buffer is what we store back into and reload from, so the
    // store-to-load-forwarding probe runs in isolation per thread.
    if (!tls.initialized) {
        tls.input = (uint32_t *)aligned_alloc_safe(64,
                MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        if (!tls.input)
            return EXIT_FAILURE;
        memcpy(tls.input, data->input, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        tls.initialized = true;
    }

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            // PROBE C: store-to-load-forwarding. Read input[i], byte-swap it,
            // store the swapped value back to the SAME address, then
            // immediately reload that address. The reload must be served by
            // the store buffer (SLF) rather than the cache, because the
            // store and load hit the exact same address. A defect in the
            // store-buffer / forwarding path shows up as reloaded != stored.
            volatile uint32_t *vinput = &tls.input[i];
            uint32_t val = *vinput;
            val = __builtin_bswap32(val);

            tls.input[i] = val;             // store to the same address (private)

            uint32_t reloaded = *vinput;    // SLF reload (volatile forces it)

            if (reloaded != val) {
                uint32_t inputv = *vinput;
                uint32_t golden = val;       // what we stored
                uint32_t actual = reloaded;  // what SLF gave back
                uint32_t xorv = golden ^ actual;
                unsigned char *ib = (unsigned char *)&inputv;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                log_error("MovBE-probeC: SLF mismatch at index %u: "
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

    /* issue #374: post-loop verification of the stored intermediate, in the
     * legal-set form this probe's in-place design permits. The store-back
     * flips the byte order of tls.input[i] on EVERY outer iteration, so the
     * final content is bswap^N(seed[i]) with N = this thread's completed
     * outer-iteration count -- a parity that is unknowable post-hoc. The
     * final content must therefore be one of the two legal states {seed,
     * bswap(seed)}; anything else is a persistent byte-swap-unit fault.
     * Coverage boundary (documented honestly): a NON-involutive persistent
     * mispermutation P leaves P^N(seed) outside the legal set for N >= 2 and
     * is caught with certainty; an INVOLUTIVE mispermutation is caught only
     * when this thread's final parity is odd (P^N(seed) == seed for even N).
     * Per-iteration verification would require touching the SLF store->load
     * window this probe exists to isolate, so it is not done. The hot loop
     * above is unchanged. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        uint32_t fin = tls.input[i];
        if (fin != data->input[i] && fin != data->expected_swapped[i]) {
            log_error("MovBE-probeC: swapped-intermediate miscompare at index %u: "
                      "seed=0x%08X legal={0x%08X,0x%08X} actual=0x%08X",
                      (unsigned)i, data->input[i], data->input[i],
                      data->expected_swapped[i], fin);
            report_fail_msg("MovBE: in-place buffer outside legal {seed, bswap(seed)} "
                            "set at index %u", (unsigned)i);
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

DECLARE_TEST(movbe_dump_probe_c, "movbe_dump probe C: store-to-load-forwarding on input line (store back to input)")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
