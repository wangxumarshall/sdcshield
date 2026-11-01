/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_xn
 * @parblock
 * SDC-trigger probe, variant XN: **parametrized extra-NOP count** to draw the
 * timing-phase sensitivity curve quantitatively. This is the V3 quantitative
 * instrument (see docs/MOVBE_SDC_CORE179_LOCALIZATION_REPORT_V2.md §3.3).
 *
 * V2 established (qualitatively) that one extra semantically-no-op ALU instr
 * collapses baseline's ~100% trigger to ~10–20% (probe H `and`, probe X `eor`).
 * XN parametrizes the extra-instruction count via the PROBE_XN_NOPS env var
 * (default 0 == baseline), inserting N independent `eor w?,w?,wzr` no-ops on the
 * value register between the first bswap and the store. objdump MUST confirm:
 * (a) exactly N `eor` instructions in the loop before the store, (b) store
 * stays `str [.., x19, lsl #2]` == baseline, (c) NO pointer-reload `ldp` in
 * the loop, (d) store<->reload relationship matches probeX (NOT back-to-back
 * when N>0, because eor breaks the 2nd-bswap-fold-to-reload). For N=0 the loop
 * is byte-identical to baseline (back-to-back), serving as the 100% control.
 *
 * The curve of trigger-rate-vs-N answers: is the phase window narrow (rate→0
 * at small N) or wide (rate persists)? A logistic/step shape pins the phase
 * width; a flat-low shape means the window is narrower than 1 issue slot.
 *
 * Implementation: each NOP is `asm("eor %w0, %w0, wzr" : "=r"(val) : "0"(val));`
 * in a loop run N times. Plain C `val^0u` is DCE'd; asm with IO operands is not.
 * No volatile/memory clobber (that made the compiler reload pointers).
 * @endparblock
 */

#include "sandstone.h"

#define MOVBE_BUFFER_SIZE (1 << 14)

static int g_probe_xn_nops = 0;   /* extra no-op ALU instructions, from PROBE_XN_NOPS */

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
    if (const char *env = getenv("PROBE_XN_NOPS")) {
        long v = strtol(env, nullptr, 10);
        if (v >= 0 && v <= 16) g_probe_xn_nops = (int)v;
    }

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
    const int nops = g_probe_xn_nops;

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            // PROBE XN: insert N semantically-no-op ALU instructions (eor with
            // wzr) on val. asm IO operands prevent DCE; no volatile/memory so
            // the compiler keeps pointers loaded outside the loop. N=0 == baseline.
            for (int n = 0; n < nops; ++n)
                asm("eor %w0, %w0, wzr" : "=r"(val) : "0"(val));

            data->swapped[i] = val;

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
                log_error("MovBE-probeXN(nops=%u): Round-trip failed at index %u: "
                          "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
                          "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
                          "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X",
                          (unsigned)nops, (unsigned)i, inputv, golden, actual, xorv,
                          ib[3], ib[2], ib[1], ib[0],
                          gb[3], gb[2], gb[1], gb[0],
                          ab[3], ab[2], ab[1], ab[0],
                          xb[3], xb[2], xb[1], xb[0]);
                report_fail_msg("MovBE: Round-trip failed at index %u", (unsigned)i);
            }
        }
    }

    /* issue #374: post-loop verification of the stored intermediate. The hot
     * loop above (including the probe-XN no-op eor chain) is unchanged; every
     * iteration stores the same bswap32(input[i]) to swapped[i], so the final
     * content must equal the init-time LUT golden. A persistent REV
     * mispermutation (symmetric fault: applied twice it cancels in the
     * round-trip identity) is caught here byte-exactly. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-probeXN(nops=%u): swapped-intermediate miscompare at "
                      "index %u: input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      (unsigned)nops, (unsigned)i, data->input[i],
                      data->expected_swapped[i], data->swapped[i],
                      data->expected_swapped[i] ^ data->swapped[i]);
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

DECLARE_TEST(movbe_dump_probe_xn, "movbe_dump probe XN: parametrized extra-NOP count (PROBE_XN_NOPS env) for timing-phase sensitivity curve")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
