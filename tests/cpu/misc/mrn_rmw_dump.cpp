/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_rmw_dump
 * @parblock
 * Dump-on-failure variant of mrn_rmw: identical ALU read-modify-write +
 * store-to-load-forwarding hot path, but on miscompare it records the
 * input / golden / actual / xor values (plus a per-byte breakdown) so the
 * flipped bit of an SDC event can be located. Mirrors the movbe_dump
 * design: the hot loop body is byte-for-byte identical to mrn_rmw.cpp's
 * mrn_rmw_run; the dump lives entirely in the cold failure branch, reading
 * values it needs from data->srcA/srcB/expected and the local res/dst.
 * Do NOT add extra volatile reads or local snapshots to the hot path —
 * that perturbs the instruction schedule and can stop the defect from
 * triggering (see memory movbe-dump-compiler-dce-root-cause).
 *
 * Additionally writes a binary record to a per-process log file on failure:
 *   - inputA, inputB : source operands
 *   - golden          : expected[i] (precomputed ALU result)
 *   - actual_dst      : dst[i] after store-load forwarding
 *   - actual_temp     : temp[i] after ALU store (intermediate)
 *   - xor             : golden ^ actual_dst
 *   - index, iteration, cpu, op_type, timestamp
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#define MRN_RMW_COUNT 1024

struct MrnRmwData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *expected;
    int dump_fd;  // binary log file descriptor
};

#pragma pack(push, 1)
struct mrn_rmw_dump_record {
    uint64_t timestamp_us;   // microseconds since epoch
    uint32_t cpu;            // logical cpu where failure occurred
    uint32_t iteration;      // TEST_LOOP iteration number
    uint32_t index;          // array index where failure occurred
    uint32_t op_type;        // 0=add, 1=sub, 2=xor, 3=and
    uint64_t inputA;         // data->srcA[index]
    uint64_t inputB;         // data->srcB[index]
    uint64_t golden;         // data->expected[index] (precomputed ALU result)
    uint64_t actual_dst;     // dst[index] after store-load forwarding
    uint64_t actual_temp;    // temp[index] after ALU store (intermediate)
    uint64_t res_recompute;  // ALU result recomputed in cold branch from inputA/B
    uint64_t xor_val;        // golden ^ actual_dst
    uint32_t verdict;        // bit0: temp==recompute(store faithful) bit1: temp==dst(fwd faithful)
};
#pragma pack(pop)

#if defined(__x86_64__) || defined(__i386__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("movq %1, %0" : "=m"(*reinterpret_cast<uint64_t*>(addr)) : "r"(val) : "memory");
}
static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("movq %1, %0" : "=r"(res) : "m"(*reinterpret_cast<const uint64_t*>(addr)) : "memory");
    return res;
}

#elif defined(__aarch64__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("str %1, [%0]" : : "r"(addr), "r"(val) : "memory");
}
static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=r"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int mrn_rmw_init(struct test *test) {
    auto *data = static_cast<MrnRmwData *>(malloc(sizeof(MrnRmwData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_RMW_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_RMW_COUNT * sizeof(uint64_t));

    // Compute expected golden values
    for (int i = 0; i < MRN_RMW_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] - data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
        }
    }

    // Open binary dump log (per-process, append)
    char path[256];
    snprintf(path, sizeof(path), "/tmp/mrn_rmw_dump_%d.bin", getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("mrn_rmw_dump: failed to open dump log %s", path);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_rmw_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnRmwData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    uint32_t iter = 0;

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_RMW_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res;

            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a - b; break;
                case 2: res = a ^ b; break;
                case 3: res = a & b; break;
            }

            // Store RMW result to temp, then load to dst
            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        // Hot path above is byte-for-byte identical to mrn_rmw.cpp. Only on
        // miscompare do we locate the first mismatch and dump input/golden/
        // actual/xor + per-byte breakdown. All dump reads are from the
        // source arrays and the freshly computed dst — no hot-path snapshot.
        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t inputA = data->srcA[i];
                    uint64_t inputB = data->srcB[i];
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";

                    // Recompute the ALU result HERE, in the cold branch, from
                    // the same inputA/inputB the hot loop used. Runs only on
                    // failure; does NOT touch the hot loop. Compares the
                    // hot-loop stored temp[i] against this cold recompute to
                    // split ALU-compute-wrong (temp != recompute) from
                    // store/forwarding damage (temp == recompute != golden).
                    uint64_t res_recompute;
                    switch (i % 4) {
                        case 0: res_recompute = inputA + inputB; break;
                        case 1: res_recompute = inputA - inputB; break;
                        case 2: res_recompute = inputA ^ inputB; break;
                        default: res_recompute = inputA & inputB; break;
                    }
                    log_error("mrn_rmw_dump: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX recompute=0x%016lX xor=0x%016lX | "
                              "temp=0x%016lX dst=0x%016lX | %s%s%s",
                              (unsigned)i, op,
                              (unsigned long)inputA, (unsigned long)inputB,
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)res_recompute, (unsigned long)xorv,
                              (unsigned long)temp[i], (unsigned long)dst[i],
                              (temp[i] == res_recompute) ? "[store-faithful]" : "[ALU-COMPUTE-WRONG]",
                              (temp[i] == dst[i]) ? "[fwd-faithful]" : "[fwd-damaged]",
                              (temp[i] == res_recompute && dst[i] == temp[i] && dst[i] != golden)
                                  ? " -> store+fwd OK, error predates this op (input/golden path)"
                                  : "");


                    // Write binary record (cold branch only)
                    if (data->dump_fd >= 0) {
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        struct mrn_rmw_dump_record rec = {
                            .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                            .cpu = (uint32_t)cpu,
                            .iteration = iter,
                            .index = (uint32_t)i,
                            .op_type = (uint32_t)(i % 4),
                            .inputA = inputA,
                            .inputB = inputB,
                            .golden = golden,
                            .actual_dst = actual,
                            .actual_temp = temp[i],
                            .res_recompute = res_recompute,
                            .xor_val = xorv,
                            .verdict = (uint32_t)(((temp[i] == res_recompute) ? 1u : 0u)
                                                | ((temp[i] == dst[i]) ? 2u : 0u)),
                        };
                        if (write(data->dump_fd, &rec, sizeof(rec)) < 0) { /* best effort */ }
                    }

                    report_fail_msg("mrn_rmw_dump: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
        iter++;
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int mrn_rmw_cleanup(struct test *test) {
    auto *data = static_cast<MrnRmwData *>(test->data);
    if (data) {
        if (data->dump_fd >= 0) {
            close(data->dump_fd);
        }
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_dump, "mrn_rmw dump variant: records inputA/inputB/golden/actual_dst/actual_temp/xor binary log on miscompare (ALU RMW + store-load forwarding)")
    .test_init = mrn_rmw_init,
    .test_run = mrn_rmw_run,
    .test_cleanup = mrn_rmw_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
