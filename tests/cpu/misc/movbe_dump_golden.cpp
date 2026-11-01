/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_golden
 * @parblock
 * Cross-validation variant of movbe_dump: byte-identical hot path, but
 * test_init (which runs on logical core 0, NOT on 179) writes the input
 * buffer to a file right after filling it. That file is the independent
 * golden: it records the exact bits core 0 wrote into the input buffer,
 * which never passed through 179. The fail-branch golden/actual in the log
 * are read BY 179 (movbe_dump.cpp:98-99 reads data->input[i] in the failure
 * branch), so they are themselves potentially corrupted by 179's faulty
 * L1D. Comparing the log's golden/actual against this file's independent
 * golden answers: did 179 even read the correct input?
 *
 * ONLY test_init is changed (cold path). test_run is byte-for-byte copied
 * from movbe_dump.cpp -- verified by objdump that the hot-loop bytes are
 * unchanged, so the 179 trigger window is preserved (per
 * movbe-dump-compiler-dce-root-cause: ANY hot-path change silences the
 * defect). The golden file is written with a per-invocation suffix
 * derived from the run, so concurrent/overlapping runs do not clobber.
 *
 * The hot-path itself, trigger conditions, and fail-branch logging are
 * identical to movbe_dump.cpp; see that file for semantics.
 * @endparblock
 */

#include "sandstone.h"
#include <fcntl.h>
#include <unistd.h>

#define MOVBE_BUFFER_SIZE (1 << 14)

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

struct movbe_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
};

static int movbe_init(struct test *test)
{
    movbe_data *data = (movbe_data *)malloc(sizeof(movbe_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    // Fill the input buffer once on core 0 (NOT 179); reused every iteration.
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the REV under test).
     * Complements the input-dump file below: that file validates the input
     * side offline, this buffer validates the swapped side in-run. */
    movbe_golden_lut_init();
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    // Dump the input buffer to a file as the independent golden. This is
    // written by core 0's test_init, before any thread (including 179)
    // touches the buffer in test_run. Use a fixed path; the harness runs one
    // test per invocation here.
    int fd = open("/tmp/movbe_input_golden.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t total = 0;
        const char *buf = (const char *)data->input;
        size_t len = MOVBE_BUFFER_SIZE * sizeof(uint32_t);
        while ((size_t)total < len) {
            ssize_t n = write(fd, buf + total, len - total);
            if (n <= 0) break;
            total += n;
        }
        close(fd);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_run(struct test *test, int cpu)
{
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
                unsigned char *ib = (unsigned char *)&inputv;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                log_error("MovBE-dump-golden: Round-trip failed at index %u: "
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
     * loop above is unchanged (byte-identical to movbe_dump, preserving the
     * 179 trigger window); every iteration stores the same bswap32(input[i])
     * to swapped[i], so the final content must equal the init-time LUT
     * golden. A persistent REV mispermutation (symmetric fault: applied twice
     * it cancels in the round-trip identity) is caught here byte-exactly. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-dump-golden: swapped-intermediate miscompare at index %u: "
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

DECLARE_TEST(movbe_dump_golden, "movbe_dump + init-dumps-input-buffer-to-file for cross-validation (core-0 golden, hot path byte-identical to movbe_dump)")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
