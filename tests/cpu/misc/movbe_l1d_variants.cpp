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
};

static int movbe_v_init(struct test *test)
{
    movbe_v_data *data = (movbe_v_data *)malloc(sizeof(movbe_v_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->input[i] = random32();
    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_v_cleanup(struct test *test)
{
    movbe_v_data *data = (movbe_v_data *)test->data;
    if (data) {
        free(data->input);
        free(data->swapped);
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
    return EXIT_SUCCESS;
}

#define DECLARE_VARIANT(name, desc, run) \
DECLARE_TEST(name, desc) \
    .test_init = movbe_v_init, \
    .test_run = run, \
    .test_cleanup = movbe_v_cleanup, \
    .quality_level = TEST_QUALITY_PROD, \
END_DECLARE_TEST

DECLARE_VARIANT(movbe_l1d_v_rev,   "movbe_l1d variant BASELINE rev->str->ldr(reload) back-to-back 32-bit structurally == movbe_l1d MUST trigger (else run invalid)", movbe_l1d_v_rev_run)
DECLARE_VARIANT(movbe_l1d_v_nop,   "movbe_l1d variant 64 nop between first ldr and rev (pre-str timing window) str->reload stays back-to-back", movbe_l1d_v_nop_run)
DECLARE_VARIANT(movbe_l1d_v_mul,   "movbe_l1d variant replace rev with mul (high-current ALU) before str tests pre-str transient current", movbe_l1d_v_mul_run)
DECLARE_VARIANT(movbe_l1d_v_add,   "movbe_l1d variant replace rev with add (low-current ALU) before str tests pre-str transient current", movbe_l1d_v_add_run)
DECLARE_VARIANT(movbe_l1d_v_rev64, "movbe_l1d variant 64 nop between str and reload ldr (post-str timing gap) rev kept", movbe_l1d_v_rev64_run)
DECLARE_VARIANT(movbe_l1d_v_rev4,  "movbe_l1d variant 4 nop between str and reload ldr (post-str timing gap) rev kept — mid-point of str->ldr gap curve (0/1/2/4/8/64)", movbe_l1d_v_rev4_run)
DECLARE_VARIANT(movbe_l1d_v_rev1,  "movbe_l1d variant 1 nop between str and reload ldr (post-str timing gap) rev kept — movbe str->ldr gap curve point (0/1/2/3/4/8/64)", movbe_l1d_v_rev1_run)
DECLARE_VARIANT(movbe_l1d_v_rev2,  "movbe_l1d variant 2 nop between str and reload ldr (post-str timing gap) rev kept — movbe str->ldr gap curve point (0/1/2/3/4/8/64)", movbe_l1d_v_rev2_run)
DECLARE_VARIANT(movbe_l1d_v_rev3,  "movbe_l1d variant 3 nop between str and reload ldr (post-str timing gap) rev kept — bisects the 2->4 nop cliff (2=6fail, 4=0fail)", movbe_l1d_v_rev3_run)
DECLARE_VARIANT(movbe_l1d_v_rev8,  "movbe_l1d variant 8 nop between str and reload ldr (post-str timing gap) rev kept — mid-point of str->ldr gap curve (0/1/2/4/8/64)", movbe_l1d_v_rev8_run)
