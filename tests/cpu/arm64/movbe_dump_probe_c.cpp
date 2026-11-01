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
