/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_x
 * @parblock
 * SDC-trigger probe, variant X: **minimal no-op ALU instruction** to isolate
 * "instruction-schedule timing perturbation" from "store footprint" and from
 * "back-to-back store<->reload".
 *
 * CONTEXT (from objdump of baseline + probes H/F/G1/G2, all 5 verified to keep
 * store<->reload back-to-back with 0 instructions between str and the reload
 * ldr):
 *   - baseline: swapped[i]                 (footprint = 64KB, 512 lines) → ~100%
 *   - H  LINES=512: swapped[i & 16383]    (footprint == baseline, mask is a
 *                                          SEMANTIC no-op since i<16384) → ~10%
 *   - F: swapped[i & 15]                   (footprint = 1 line)          → ~0%
 *   - G1/G2: ubfiz-indexed small footprint                                → ~0%
 * The tension: H proves that ONE extra no-op ALU instruction (`and x2,x19,x20`,
 * which computes i&16383 == i) drops the rate 100%→10% even though the store
 * address is byte-identical to baseline. But F/G1/G2 also have one extra ALU
 * instruction AND a shrunk footprint, so their 0% cannot separate "footprint
 * too small" from "ALU instruction suppresses timing".
 *
 * PROBE X resolves this: it is byte-for-byte identical to baseline EXCEPT it
 * inserts ONE extra ALU instruction on the hot path that is a SEMANTIC no-op
 * but carries a real register dependency (mirroring H's `and x2,x19,x20`,
 * which reads x19). The chosen instruction is `val = val ^ 0u` after the first
 * bswap: the compiler lowers this to one `eor` on the value register (a real
 * instruction, not DCE-able since the result is observed by the store), but it
 * does not change val, does not touch the address/index, and does not force
 * any pointer reload. The store stays `swapped[i]` (footprint == baseline ==
 * 64KB). The store<->reload stays back-to-back (the eor is BEFORE the store,
 * not between store and reload — mirroring where H's `and` sits). The ONLY
 * change vs baseline is one extra issue slot per iteration carrying a value
 * dependency.
 *
 * This is done in PURE C (no inline asm). An earlier version used
 * `asm volatile("nop")` / `asm volatile("mov x2,x2")`, but the compiler
 * treated the asm as a memory fence and reloaded the input/swapped pointers
 * inside the loop (`ldp x1,x2,[x20]` every iteration), turning the probe into
 * "no-op + extra memory read" and invalidating it. A pure-C `val ^ 0u` is a
 * normal expression the compiler schedules normally, so it does NOT perturb
 * pointer-lifetime analysis and the loop keeps baseline's structure.
 *
 * Decision tree:
 *   - FAIL (rate > 0) → one extra ALU instruction with a value dependency is
 *     enough to perturb the defect → the mechanism is a per-iteration
 *     instruction-schedule / pipeline-phase race, NOT footprint, NOT
 *     back-to-back. This would mean H's 10% drop is a timing effect, and
 *     F/G1/G2's 0% could be partly timing-suppression too.
 *   - PASS (rate == 0) → a dependency-bearing no-op on the value (not the
 *     index) does NOT perturb → H's drop comes specifically from the `and`
 *     reading the loop INDEX x19 (address-generation dependency), narrowing
 *     the mechanism to the store-address path.
 *
 * objdump MUST be checked after build to confirm: (a) one `eor` (or similar)
 * is present in the loop before the store, (b) store stays
 * `str [.., x19, lsl #2]` == baseline, (c) store<->reload stay back-to-back,
 * (d) NO extra pointer-reload `ldp` appears in the loop.
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

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_run(struct test *test, int cpu)
{
    movbe_data *data = (movbe_data *)test->data;

    TEST_LOOP(test, 1 << 13) {
        // Byte-for-byte identical to baseline movbe_dump EXCEPT one inline
        // no-op inserted before the store (mirroring where H places its `and`).
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            // PROBE X: one extra ALU instruction that is a SEMANTIC no-op but
            // carries a real register dependency on val and survives DCE.
            // Uses `asm("eor %w0,%w0,wzr" : "=r"(val) : "0"(val))`: the eor
            // with wzr (zero register) leaves val unchanged, but because val
            // flows through an asm node as output and is observed by the store
            // below, the compiler cannot DCE it. Critically this uses NO
            // `volatile` and NO `memory` clobber, so the compiler does not
            // treat it as a memory fence and keeps the input/swapped pointers
            // loaded once outside the loop (a `volatile`/`memory` asm made the
            // compiler reload `ldp x1,x2,[x20]` every iteration, invalidating
            // an earlier probe X version). This is the ONLY deviation from
            // baseline — store stays swapped[i], footprint stays 64KB,
            // store<->reload stays back-to-back. objdump MUST confirm: (a)
            // exactly one `eor` in the loop before the store, (b) store stays
            // `str [.., x19, lsl #2]` == baseline, (c) NO `ldp` pointer reload
            // in the loop, (d) store<->reload back-to-back.
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
                log_error("MovBE-probeX: Round-trip failed at index %u: "
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

    /* issue #374: post-loop verification of the stored intermediate. The hot
     * loop above (including the probe-X no-op eor) is unchanged; every
     * iteration stores the same bswap32(input[i]) to swapped[i], so the final
     * content must equal the init-time LUT golden. A persistent REV
     * mispermutation (symmetric fault: applied twice it cancels in the
     * round-trip identity) is caught here byte-exactly. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-probeX: swapped-intermediate miscompare at index %u: "
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

DECLARE_TEST(movbe_dump_probe_x, "movbe_dump probe X: minimal no-op ALU instruction before store (isolates timing-phase perturbation from footprint and back-to-back)")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
