/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump
 * @parblock
 * Dump-on-failure variant of movbe: byte-swap round-trip logic identical to
 * misc/movbe.cpp. Like the original movbe.cpp, the input buffer is filled
 * once at init time with random32() and reused for every TEST_LOOP
 * iteration -- this keeps the per-iteration cost (and therefore the SDC
 * trigger rate under a fixed time budget) identical to the original movbe
 * test, so a defect that the original movbe can reproduce will also
 * reproduce here under the same conditions/seed.
 *
 * The only difference from movbe.cpp is that, on a round-trip failure, it
 * logs the relevant values plus a per-byte breakdown so the flipped bit(s)
 * can be located, AND writes a binary record to a per-process log file:
 *   - input   : the original input value (data->input[i]) for this index
 *   - expected_swapped : bswap(input) -- what swapped[i] should be
 *   - actual_swapped   : value actually in data->swapped[i] at failure time
 *   - actual_val       : value after two byte-swaps (val in register)
 *   - xor              : input ^ actual_val (the bit(s) that flipped)
 *   - index, iteration, cpu, timestamp
 * Logging is fail-only: a healthy core produces a clean (pass-only) log
 * with no input/golden/actual lines.
 * @endparblock
 */

#include "sandstone.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#define MOVBE_BUFFER_SIZE (1 << 14)

struct movbe_data {
    uint32_t *input;
    uint32_t *swapped;
    int dump_fd;  // binary log file descriptor
};

#pragma pack(push, 1)
struct movbe_dump_record {
    uint64_t timestamp_us;   // microseconds since epoch
    uint32_t cpu;            // logical cpu where failure occurred
    uint32_t iteration;      // TEST_LOOP iteration number
    uint32_t index;          // array index where failure occurred
    uint32_t input;          // data->input[index]
    uint32_t expected_swapped; // bswap(input) - what swapped should be
    uint32_t actual_swapped;   // data->swapped[index] at failure time
    uint32_t actual_val;       // val after two bswaps (in register)
    uint32_t xor_val;          // input ^ actual_val
};
#pragma pack(pop)

static int movbe_init(struct test *test)
{
    movbe_data *data = (movbe_data *)malloc(sizeof(movbe_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    // Fill the input buffer once; it is reused by every TEST_LOOP iteration
    // in test_run (identical to movbe.cpp), so the LCG seed fully determines
    // the input pattern under test.
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    // Open binary dump log (per-process, append)
    char path[256];
    snprintf(path, sizeof(path), "/tmp/movbe_dump_%d.bin", getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("movbe_dump: failed to open dump log %s", path);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_run(struct test *test, int cpu)
{
    movbe_data *data = (movbe_data *)test->data;
    uint32_t iter = 0;

    TEST_LOOP(test, 1 << 13) {
        // The input buffer is filled once at init time and reused for every
        // iteration (identical to movbe.cpp), keeping the per-iteration cost
        // -- and thus the SDC trigger rate under a fixed time budget -- the
        // same as the original movbe test.
        //
        // The loop body below is byte-for-byte identical to movbe.cpp's
        // movbe_run hot path: read input[i], bswap, store to swapped[i],
        // bswap back, compare against data->input[i]. The compiler folds the
        // second bswap into a reload of data->input[i] (because the store to
        // data->swapped[i] may alias data->input), and that reload is the
        // memory path that exercises the silicon defect. Adding *any* extra
        // local snapshot or volatile read here changes the instruction
        // schedule enough to stop the defect from triggering, so the dump is
        // done entirely inside the (cold) failure branch below, reading the
        // values it needs from data->input[i] and the local val.
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            // Perform "movbe" (Byte Swap)
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            // Store swapped value
            data->swapped[i] = val;

            // Perform another "movbe" to restore original order
            val = __builtin_bswap32(val);

            // Check if we got the original value back.
            // ONLY on failure: dump input / golden / actual / xor plus
            // per-byte breakdown so the flipped bit can be located.
            //   input   : the original input value (data->input[i])
            //   golden  : same as input (the value a correct round-trip
            //             must reproduce); read fresh here only on failure
            //   actual  : the value obtained after two byte-swaps (val)
            //   xor     : golden ^ actual (the bit(s) that flipped)
            // (Matches original movbe.cpp's fail-only logging policy.)
            if (val != data->input[i]) {
                uint32_t inputv = data->input[i];
                uint32_t golden = data->input[i];
                uint32_t actual = val;
                uint32_t xorv = golden ^ actual;
                unsigned char *ib = (unsigned char *)&inputv;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                log_error("MovBE-dump: Round-trip failed at index %u: "
                          "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
                          "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
                          "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X",
                          (unsigned)i, inputv, golden, actual, xorv,
                          ib[3], ib[2], ib[1], ib[0],
                          gb[3], gb[2], gb[1], gb[0],
                          ab[3], ab[2], ab[1], ab[0],
                          xb[3], xb[2], xb[1], xb[0]);

                // Write binary record (cold branch only)
                if (data->dump_fd >= 0) {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    struct movbe_dump_record rec = {
                        .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                        .cpu = (uint32_t)cpu,
                        .iteration = iter,
                        .index = (uint32_t)i,
                        .input = inputv,
                        .expected_swapped = __builtin_bswap32(inputv),
                        .actual_swapped = data->swapped[i],
                        .actual_val = actual,
                        .xor_val = xorv,
                    };
                    write(data->dump_fd, &rec, sizeof(rec));
                }

                report_fail_msg("MovBE: Round-trip failed at index %u", (unsigned)i);
            }
        }
        iter++;
    }

    return EXIT_SUCCESS;
}

static int movbe_cleanup(struct test *test)
{
    movbe_data *data = (movbe_data *)test->data;
    if (data) {
        if (data->dump_fd >= 0) {
            close(data->dump_fd);
        }
        free(data->input);
        free(data->swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump, "movbe dump-on-failure variant: random input per iteration, round-trip with input/expected_swapped/actual_swapped/actual_val/xor binary log on failure")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
