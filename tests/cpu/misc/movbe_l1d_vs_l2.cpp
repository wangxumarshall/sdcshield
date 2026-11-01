/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_l1d
 * @test @b movbe_l2
 * @parblock
 * Reload-path localization probes for core-179 SDC.
 *
 * Hypothesis: the 179 SDC lies in the per-core load data path (L1D read
 * array -> load port -> register), NOT in cross-core LLC/memory.  If so,
 * the defect should manifest only when the reload *hits* L1D; forcing the
 * reload to miss L1D (serve from L2/L3) should eliminate the SDC.
 *
 * Two SEPARATE tests (not one test with a branch), so neither hot loop
 * contains a regime-selection conditional that would perturb the instruction
 * schedule and silence the timing-sensitive defect (see memory
 * dump-variant-noremove-report-fail-verified — any hot-loop conditional
 * can break triggering).
 *
 *   - movbe_l1d: hot loop byte-identical to movbe_dump.cpp (ldr/rev/str/
 *     ldr/cmp).  Reload of input[i] hits L1D.  Expected: SDC triggers.
 *   - movbe_l2:  same ldr/rev/str/ldr/cmp, but the reload of input[i] is
 *     preceded by `dc civac` + dsb on input[i]'s address, evicting it from
 *     L1D so the reload fetches from L2/L3.  Expected (if hypothesis
 *     holds): SDC disappears.
 *
 * On miscompare, logs input/golden/actual/xor (same as movbe_dump) then
 * report_fail_msg (noreturn) — matching movbe_dump's behavior; one TEST_LOOP
 * pass already logs a batch of events before the thread is killed.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
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

/* issue #374: post-loop verification of the stored intermediate, shared by
 * both reload-path probes. The hot loops are unchanged; every iteration
 * stores the same bswap32(input[i]) to swapped[i], so the final content must
 * equal the init-time LUT golden. A persistent REV mispermutation (symmetric
 * fault: applied twice it cancels in the round-trip identity) is caught here
 * byte-exactly. */
static void movbe_verify_swapped(struct test *test, const char *tag)
{
    movbe_data *data = (movbe_data *)test->data;
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

static int movbe_probe_init(struct test *test)
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

static int movbe_probe_cleanup(struct test *test)
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

// Shared miscompare logger (cold branch only).
static void log_miscompare(const char *tag, unsigned i,
                           uint32_t inputv, uint32_t golden,
                           uint32_t actual, uint32_t xorv)
{
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
// movbe_l1d: reload hits L1D.  Hot loop byte-identical to movbe_dump.cpp.
// ---------------------------------------------------------------------------
static int movbe_l1d_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_data *data = (movbe_data *)test->data;

    TEST_LOOP(test, 1 << 13) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            data->swapped[i] = val;

            val = __builtin_bswap32(val);

            if (val != data->input[i]) {
                uint32_t inputv = data->input[i];
                uint32_t golden = data->input[i];
                uint32_t actual = val;
                uint32_t xorv = golden ^ actual;
                log_miscompare("movbe_l1d", (unsigned)i, inputv, golden, actual, xorv);
            }
        }
    }

    movbe_verify_swapped(test, "movbe_l1d");

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// movbe_l2: evict input[i]'s line from L1D before the reload, forcing the
// reload to fetch from L2/L3.  The evict (dc civac + dsb) runs before the
// hot ldr/rev/str/ldr/cmp sequence; the sequence itself is identical to
// movbe_l1d's.  Note: this test's hot path necessarily differs from
// movbe_dump's (it contains the evict), so its own trigger rate may differ;
// that is expected and is the point of the comparison.
// ---------------------------------------------------------------------------
static inline void evict_l1d_line(const void *addr)
{
    __asm__ volatile(
        "dc civac, %0\n\t"
        "dsb sy\n\t"
        :: "r"(addr) : "memory"
    );
}

static int movbe_l2_run(struct test *test, int cpu)
{
    (void)cpu;
    movbe_data *data = (movbe_data *)test->data;

    /* Loop granularity (issues #369/#360 class): the body is one full
     * 16384-element sweep whose every element pays a `dc civac` + `dsb sy`
     * line-evict + full drain before the reload — ~3.5-3.7 ms per body
     * (measured: 28.9 s quick -n4 on a 128-core Kunpeng 920, 30.5 s on the
     * CI 4-vCPU Neoverse-N2 runner). The N = 1<<13 inherited from
     * movbe_dump.cpp (whose plain ldr/rev/str body has no per-element
     * barrier, checkpoint < 270 ms) therefore slid the first deadline check
     * to ~29-30 s; --quick stops at that first check, so 4 worker threads
     * burned ~116-122 s of CPU per forked test child — at/over the quick
     * CI step's `ulimit -St 120`, zero margin, and runner contention turned
     * it into SIGXCPU flakes (reproduced locally: both crash code 24 under
     * `ulimit -St 120`, pass under 600). N = 4 lands the checkpoint at
     * ~15 ms, inside the framework's TargetLoopDuration = 10 ms band
     * (sandstone_p.h). The hot 16384-element loop is untouched: the check
     * runs only between complete passes and adds no regime-selection
     * conditional inside the hot loop (see file header); in uncapped -t
     * runs the per-second stimulus is unchanged — only the -t overshoot
     * shrinks from ~30 s to one checkpoint. */
    TEST_LOOP(test, 4) {
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            data->swapped[i] = val;

            // Evict input[i]'s cache line from L1D so the reload below
            // must fetch from L2/L3 instead of hitting L1D.
            evict_l1d_line(&data->input[i]);

            val = __builtin_bswap32(val);

            if (val != data->input[i]) {
                uint32_t inputv = data->input[i];
                uint32_t golden = data->input[i];
                uint32_t actual = val;
                uint32_t xorv = golden ^ actual;
                log_miscompare("movbe_l2", (unsigned)i, inputv, golden, actual, xorv);
            }
        }
    }

    movbe_verify_swapped(test, "movbe_l2");

    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_l1d, "movbe reload-path probe (L1D-hit): reload of input[i] hits L1D — core-179 SDC expected")
    .test_init = movbe_probe_init,
    .test_run = movbe_l1d_run,
    .test_cleanup = movbe_probe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(movbe_l2, "movbe reload-path probe (L2-reload): evict input[i] line before reload — if SDC is L1D-path, should disappear")
    .test_init = movbe_probe_init,
    .test_run = movbe_l2_run,
    .test_cleanup = movbe_probe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
