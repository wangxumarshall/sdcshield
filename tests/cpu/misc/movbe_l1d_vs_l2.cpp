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
