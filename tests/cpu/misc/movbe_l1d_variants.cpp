/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * Multi-variant probe family for core-179 SDC, built on the CONFIRMED
 * high-rate trigger movbe_l1d (see memory movbe-d-positive-control-20260824,
 * l1d-read-path-sdc-confirmed, mrn_rmw_no_trigger_vs_movbe_l1d_trigger).
 *
 * WHY THIS FAMILY EXISTS
 *
 * The 2x2 mrn_rmw timing matrix did NOT close: all four cells (fwd/diff/
 * nop64/diff_nop64) came back 0-fail on the current run, while movbe_l1d
 * triggered in the same window. So mrn_rmw is too low/unstable a trigger to
 * serve as a baseline anchor, and its 0-fails carry no path information.
 * movbe_l1d is the only confirmed stable trigger, so all variants here derive
 * from it. The baseline cell (movbe_l1d_v_rev, structurally == movbe_l1d) MUST
 * trigger; if it does not, the run is invalid and the variant results are
 * discarded — the same "baseline must trigger" guard the mrn_rmw matrix lacked.
 *
 * VARIABLES ISOLATED (each variant changes ONE variable vs the baseline)
 *
 * The baseline hot loop (verified by objdump of the original movbe_l1d):
 *     ldr  w4, [input+i]      ; first read (L1D hit)
 *     rev  w3, w4             ; byte-reverse (32-bit, high switching current)
 *     str  w3, [swapped+i]    ; store to a DIFFERENT address (input != swapped)
 *     ldr  w3, [input+i]      ; RELOAD input[i] (same addr as first read,
 *                             ;   L1D-array read; back-to-back with str)
 *     cmp  w3, w4
 *
 * Variants:
 *   movbe_l1d_v_rev    BASELINE (rev before str)            -> MUST trigger
 *   movbe_l1d_v_nop    str preceded by 64 mov x9,x9         (timing window)
 *   movbe_l1d_v_mul    str preceded by mul (high current)  (pre-str current)
 *   movbe_l1d_v_add    str preceded by add (low current)   (pre-str current)
 *   movbe_l1d_v_rev64  str->ldr gap widened by 64 mov x9,x9 (str-ldr timing)
 *
 * NOTE on "rev as pre-str current" hypothesis: a reviewer noted the original
 * movbe_l1d's str is preceded by `rev` (high bit-flipping current) while
 * mrn_rmw_fwd's str is preceded by `add` (low current). The v_mul / v_add /
 * v_nop variants test whether the PRE-STR instruction's transient current
 * modulates the defect. v_rev64 tests the str->ldr POST gap independently.
 *
 * CRITICAL: hot loop is written in INLINE ASM with explicit clobbers so the
 * compiler CANNOT reorder/schedule/DCE the str->ldr adjacency (see memory
 * movbe-dump-compiler-dce-root-cause: a C loop lets the scheduler pull
 * instructions across the boundary and silently break triggering). Each
 * variant's hot loop is a single asm block.
 *
 * Miscompare logging in COLD branch only; report_fail_msg kept as-is (memory
 * dump-variant-noremove-report-fail-verified: even cold-branch edits once
 * silenced the defect).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

#define MOVBE_BUFFER_SIZE (1 << 14)

struct movbe_v_data {
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

/* issue #374: post-loop verification of the stored intermediate, shared by
 * the eight rev-payload variants. The asm hot loops are untouched (the
 * core-179 str->ldr trigger window is pinned inside the asm block); every
 * iteration stores the same rev(input[i]) to swapped[i], so the final
 * content must equal the init-time LUT golden. A persistent REV
 * mispermutation (symmetric fault: applied twice it cancels in the
 * reload-vs-first-load compare) is caught here byte-exactly. */
static void movbe_v_verify_swapped(struct test *test, const char *tag)
{
    movbe_v_data *data = (movbe_v_data *)test->data;
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("%s: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      tag, (unsigned)i, data->input[i], data->expected_swapped[i],
                      data->swapped[i], data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("%s: swapped buffer != independent golden at index %u",
                            tag, (unsigned)i);
        }
    }
}

static int movbe_v_init(struct test *test)
{
    movbe_v_data *data = (movbe_v_data *)malloc(sizeof(movbe_v_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->input[i] = random32();
    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the REV under test). */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);
    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_v_cleanup(struct test *test)
{
    movbe_v_data *data = (movbe_v_data *)test->data;
    if (data) {
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

// Cold-branch miscompare logger (identical logic/bytes to movbe_l1d_vs_l2.cpp).
static void log_miscompare(const char *tag, unsigned i,
                           uint32_t inputv, uint32_t golden, uint32_t actual)
{
    uint32_t xorv = golden ^ actual;
    unsigned char *ib = (unsigned char *)&inputv;
    unsigned char *gb = (unsigned char *)&golden;
    unsigned char *ab = (unsigned char *)&actual;
    unsigned char *xb = (unsigned char *)&xorv;
    log_error("%s: Round-trip failed at index %u: "
              "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
              "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
              "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
              "xor=%02X%02X%02X%02X",
              tag, i, inputv, golden, actual, xorv,
              ib[3], ib[2], ib[1], ib[0],
              gb[3], gb[2], gb[1], gb[0],
              ab[3], ab[2], ab[1], ab[0],
              xb[3], xb[2], xb[1], xb[0]);
    report_fail_msg("%s: Round-trip failed at index %u", tag, i);
}

// ---------------------------------------------------------------------------
// BASELINE: rev before str, str->ldr back-to-back (structurally == movbe_l1d).
// MUST trigger. If it does not, the run is invalid.
// ---------------------------------------------------------------------------
static int movbe_l1d_v_rev_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"   // first read input[i]
                "rev  %w[v], %w[val]\n\t"      // byte-reverse (high current)
                "str  %w[v], [%[swp]]\n\t"     // store to swapped[i] (diff addr)
                "ldr  %w[v], [%[inp]]\n\t"     // RELOAD input[i] (L1D-array, back-to-back)
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// PRE-STR TIMING: str preceded by 64 mov x9,x9 (widens the gap BEFORE str,
// i.e. between the first ldr and the str). Tests whether the defect needs
// the str->first-ldr adjacency. The str->reload ldr stays back-to-back.
// ---------------------------------------------------------------------------
static int movbe_l1d_v_nop_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_nop";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                ".rept 64\n\t"
                "mov  x9, x9\n\t"
                ".endr\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                "ldr  %w[v], [%[inp]]\n\t"
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// PRE-STR CURRENT (high): replace `rev` with `mul` (high switching current,
// no byte-reverse semantics needed — we don't use the value, we just want the
// transient). Tests the "pre-str instruction current" variable. str->ldr
// stays back-to-back.
// ---------------------------------------------------------------------------
static int movbe_l1d_v_mul_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_mul";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "mul  %w[v], %w[val], %w[val]\n\t"   // high-current ALU (mul)
                "str  %w[v], [%[swp]]\n\t"
                "ldr  %w[v], [%[inp]]\n\t"           // reload, back-to-back
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }

    /* issue #374: post-loop verification of the stored mul payload. The asm
     * hot loop is untouched; every iteration stores the same
     * input[i]*input[i] (mod 2^32) to swapped[i], so the final content must
     * equal the scalar recomputation. Coverage boundary (documented): this
     * variant contains NO byte-swap, so the issue's target fault class (REV
     * mispermutation) does not apply; the golden below uses the same scalar
     * mul unit as the payload (no cheap independent square path exists) --
     * the check still catches store-path corruption and asymmetries, and
     * this variant's reload-path detection (first-load register baseline)
     * was already independent. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        uint32_t expected = data->input[i] * data->input[i];
        if (data->swapped[i] != expected) {
            log_error("%s: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      tag, (unsigned)i, data->input[i], expected,
                      data->swapped[i], expected ^ data->swapped[i]);
            report_fail_msg("%s: swapped buffer != scalar golden at index %u",
                            tag, (unsigned)i);
        }
    }
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// PRE-STR CURRENT (low): replace `rev` with `add` (low current). If v_rev /
// v_mul trigger but v_add does not, the pre-str instruction's transient
// current is a modulating variable.
// ---------------------------------------------------------------------------
static int movbe_l1d_v_add_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_add";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "add  %w[v], %w[val], %w[val]\n\t"   // low-current ALU (add)
                "str  %w[v], [%[swp]]\n\t"
                "ldr  %w[v], [%[inp]]\n\t"           // reload, back-to-back
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }

    /* issue #374: post-loop verification of the stored add payload. The asm
     * hot loop is untouched; every iteration stores the same
     * input[i]+input[i] (mod 2^32) to swapped[i], so the final content must
     * equal the scalar recomputation. Coverage boundary (documented): this
     * variant contains NO byte-swap, so the issue's target fault class (REV
     * mispermutation) does not apply; the golden below uses the same scalar
     * adder as the payload -- the check still catches store-path corruption
     * and asymmetries, and this variant's reload-path detection (first-load
     * register baseline) was already independent. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        uint32_t expected = data->input[i] + data->input[i];
        if (data->swapped[i] != expected) {
            log_error("%s: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      tag, (unsigned)i, data->input[i], expected,
                      data->swapped[i], expected ^ data->swapped[i]);
            report_fail_msg("%s: swapped buffer != scalar golden at index %u",
                            tag, (unsigned)i);
        }
    }
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// POST-STR GAP: widen the str->reload gap with 64 mov x9,x9 (timing window
// AFTER str, between str and the reload). Tests the "str->ldr back-to-back
// adjacency" variable independently of the pre-str instruction.
// ---------------------------------------------------------------------------
static int movbe_l1d_v_rev64_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev64";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                ".rept 64\n\t"
                "mov  x9, x9\n\t"                    // widen str->ldr gap
                ".endr\n\t"
                "ldr  %w[v], [%[inp]]\n\t"          // reload after gap
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// POST-STR GAP (4/8 nop): same as v_rev64 but with only 4 / 8 mov x9,x9
// between str and the reload ldr. Fills the §12.3 0->1->2->64 curve gap:
// 4 and 8 nop are the missing mid-range points to see where the str->ldr
// back-to-back trigger rate actually falls off.
// ---------------------------------------------------------------------------
static int movbe_l1d_v_rev4_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev4";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                ".rept 4\n\t"
                "mov  x9, x9\n\t"
                ".endr\n\t"
                "ldr  %w[v], [%[inp]]\n\t"
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

static int movbe_l1d_v_rev1_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev1";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                ".rept 1\n\t"
                "mov  x9, x9\n\t"
                ".endr\n\t"
                "ldr  %w[v], [%[inp]]\n\t"
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

static int movbe_l1d_v_rev2_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev2";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                ".rept 2\n\t"
                "mov  x9, x9\n\t"
                ".endr\n\t"
                "ldr  %w[v], [%[inp]]\n\t"
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

static int movbe_l1d_v_rev3_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev3";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                ".rept 3\n\t"
                "mov  x9, x9\n\t"
                ".endr\n\t"
                "ldr  %w[v], [%[inp]]\n\t"
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

static int movbe_l1d_v_rev8_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_v_data *data = (movbe_v_data *)test->data;
    const char *tag = "movbe_l1d_v_rev8";

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t *inp = data->input + i;
            uint32_t *swp = data->swapped + i;
            uint32_t val, first;
            __asm__ volatile(
                "ldr  %w[val], [%[inp]]\n\t"
                "rev  %w[v], %w[val]\n\t"
                "str  %w[v], [%[swp]]\n\t"
                ".rept 8\n\t"
                "mov  x9, x9\n\t"
                ".endr\n\t"
                "ldr  %w[v], [%[inp]]\n\t"
                : [v] "=&r"(val), [val] "=&r"(first)
                : [inp] "r"(inp), [swp] "r"(swp)
                : "memory"
            );
            if (val != first) {
                log_miscompare(tag, (unsigned)i, first, first, val);
            }
        }
    }
    movbe_v_verify_swapped(test, tag);
    return EXIT_SUCCESS;
}

/* Variant declarations are spelled out as plain DECLARE_TEST blocks (not a
 * DECLARE_VARIANT wrapper macro) so scripts/generate-short-ids.pl's
 * "^DECLARE_TEST\(" scan finds each variant and emits its TEST_ID_* — the
 * framework's DECLARE_TEST macro requires that define for .shortid. */

DECLARE_TEST(movbe_l1d_v_rev,   "movbe_l1d variant BASELINE rev->str->ldr(reload) back-to-back 32-bit structurally == movbe_l1d MUST trigger (else run invalid)")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_nop,   "movbe_l1d variant 64 nop between first ldr and rev (pre-str timing window) str->reload stays back-to-back")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_nop_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_mul,   "movbe_l1d variant replace rev with mul (high-current ALU) before str tests pre-str transient current")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_mul_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_add,   "movbe_l1d variant replace rev with add (low-current ALU) before str tests pre-str transient current")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_add_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_rev64, "movbe_l1d variant 64 nop between str and reload ldr (post-str timing gap) rev kept")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev64_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_rev4,  "movbe_l1d variant 4 nop between str and reload ldr (post-str timing gap) rev kept — mid-point of str->ldr gap curve (0/1/2/4/8/64)")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev4_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_rev1,  "movbe_l1d variant 1 nop between str and reload ldr (post-str timing gap) rev kept — movbe str->ldr gap curve point (0/1/2/3/4/8/64)")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev1_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_rev2,  "movbe_l1d variant 2 nop between str and reload ldr (post-str timing gap) rev kept — movbe str->ldr gap curve point (0/1/2/3/4/8/64)")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev2_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_rev3,  "movbe_l1d variant 3 nop between str and reload ldr (post-str timing gap) rev kept — bisects the 2->4 nop cliff (2=6fail, 4=0fail)")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev3_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l1d_v_rev8,  "movbe_l1d variant 8 nop between str and reload ldr (post-str timing gap) rev kept — mid-point of str->ldr gap curve (0/1/2/4/8/64)")
    .test_init = movbe_v_init,
    .test_run = movbe_l1d_v_rev8_run,
    .test_cleanup = movbe_v_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
