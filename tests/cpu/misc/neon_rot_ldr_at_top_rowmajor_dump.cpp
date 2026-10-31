/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_dump
 * @parblock
 * Dump-on-failure variant of neon_rot_ldr_at_top_rowmajor (2026-09-09).
 *
 * Parent is the highest-rate SDC trigger measured so far on CPU 179
 * (309 fails / 30 min = 10.3/min in the 2026-09-08 campaign, 2.2x the
 * runner-up), but it only reports a one-line "data miscompare" -- WHAT
 * core 179 actually computed wrong, and WHETHER the error follows the
 * ascending or the descending sweep, was never recorded.
 *
 * Hot loop is byte-identical to neon_rot_ldr_at_top_rowmajor.cpp (same
 * alternating ascending/descending sweeps over 1024 entries, same 3rd
 * ldr-at-top + 2-src load + rotating ALU (add/eor/and/orr) + back-to-back
 * str/str skeleton, same TEST_LOOP count) so the SDC trigger rate under a
 * fixed time budget is unchanged. The ONLY difference is the cold failure
 * path: on memcmp mismatch it logs, per mismatching index, the source pair,
 * the expected (golden) vector, the actual dst vector, and their xor --
 * plus a binary record to /var/tmp/neon_rot_ldr_at_top_rowmajor_dump_<pid>.bin
 * (on-disk location per the 09-08 panic lesson: /tmp is tmpfs and loses
 * records on crash; movbe_dump / mrn_rmw_dump / eigen_gemm dump design).
 *
 * Record fields: timestamp_us, cpu, iteration, sweep_dir (0=ascending,
 * 1=descending), index, op (0=add,1=eor,2=and,3=orr), srcA, srcB (full 16B
 * vectors as 2x u64), expected, actual, xor (each 2x u64). The xor
 * localizes the flipped bit(s): load-path errors show srcA/srcB corruption,
 * ALU-path errors show clean sources with a corrupted result. sweep_dir +
 * index answer the parent's discriminator directly: address-local errors
 * (same L1D set/way) keep the same index distribution across directions;
 * forward-progress-position errors shift with the sweep seam.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT 1024

struct NeonRotLdrAtTopRowMajorDumpData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
    int dump_fd;
};

#pragma pack(push, 1)
struct neon_rot_ldr_at_top_rowmajor_dump_record {
    uint64_t timestamp_us;
    uint32_t cpu;
    uint32_t iteration;
    uint32_t sweep_dir;     /* 0=ascending 1=descending (iteration & 1) */
    uint32_t index;
    uint32_t op;            /* 0=add 1=eor 2=and 3=orr (i%4) */
    uint32_t _pad;
    uint64_t srcA_lo, srcA_hi;
    uint64_t srcB_lo, srcB_hi;
    uint64_t exp_lo, exp_hi;
    uint64_t act_lo, act_hi;
    uint64_t xor_lo, xor_hi;
};
#pragma pack(pop)

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* cold-path helper: full-vector equality test */
static inline bool vec_eq(uint64x2_t x, uint64x2_t y) {
    uint64x2_t diffs = veorq_u64(x, y);
    uint64x2_t orred = vpaddq_u64(diffs, diffs);   /* fold lanes together */
    return vgetq_lane_u64(orred, 0) == 0 && vgetq_lane_u64(orred, 1) == 0;
}

#endif

static int neon_rot_ldr_at_top_rowmajor_dump_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorDumpData *>(malloc(sizeof(NeonRotLdrAtTopRowMajorDumpData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "/var/tmp/neon_rot_ldr_at_top_rowmajor_dump_%d.bin", getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("neon_rot_ldr_at_top_rowmajor_dump: failed to open dump log %s", path);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dump_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorDumpData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)));

    uint32_t iter = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (iter & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }

        /* cold path: single memcmp per sweep (identical to parent's
         * per-sweep cost); the per-index scan + dump runs ONLY inside
         * the mismatch branch */
        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT * sizeof(uint64x2_t)) != 0) {
            for (int i = 0; i < NEON_ROT_LDR_AT_TOP_ROWMAJOR_DUMP_COUNT; i++) {
                if (!vec_eq(dst[i], data->expected[i])) {
                    /* reread sources here (cold) to classify load- vs ALU-error */
                    uint64x2_t a = load_vec(&data->srcA[i]);
                    uint64x2_t b = load_vec(&data->srcB[i]);
                    uint64x2_t exp = data->expected[i];
                    uint64x2_t act = dst[i];
                    uint64x2_t xorv = veorq_u64(exp, act);

                    log_error("neon_rot_ldr_at_top_rowmajor_dump: idx %u op %u dir %s cpu %u: "
                              "srcA=%016lx%016lx srcB=%016lx%016lx exp=%016lx%016lx "
                              "act=%016lx%016lx xor=%016lx%016lx",
                              (unsigned)i, (unsigned)(i % 4),
                              ascending ? "asc" : "desc",
                              (unsigned)device_info[cpu].cpu_number, /* logical CPU */
                              vgetq_lane_u64(a,1), vgetq_lane_u64(a,0),
                              vgetq_lane_u64(b,1), vgetq_lane_u64(b,0),
                              vgetq_lane_u64(exp,1), vgetq_lane_u64(exp,0),
                              vgetq_lane_u64(act,1), vgetq_lane_u64(act,0),
                              vgetq_lane_u64(xorv,1), vgetq_lane_u64(xorv,0));

                    if (data->dump_fd >= 0) {
                        struct timeval tv;
                        gettimeofday(&tv, nullptr);
                        struct neon_rot_ldr_at_top_rowmajor_dump_record rec = {
                            .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                            .cpu = (uint32_t)device_info[cpu].cpu_number, /* cpu param is thread_num; translate to logical CPU */
                            .iteration = iter,
                            .sweep_dir = ascending ? 0u : 1u,
                            .index = (uint32_t)i,
                            .op = (uint32_t)(i % 4),
                            ._pad = 0,
                            .srcA_lo = vgetq_lane_u64(a, 0), .srcA_hi = vgetq_lane_u64(a, 1),
                            .srcB_lo = vgetq_lane_u64(b, 0), .srcB_hi = vgetq_lane_u64(b, 1),
                            .exp_lo = vgetq_lane_u64(exp, 0), .exp_hi = vgetq_lane_u64(exp, 1),
                            .act_lo = vgetq_lane_u64(act, 0), .act_hi = vgetq_lane_u64(act, 1),
                            .xor_lo = vgetq_lane_u64(xorv, 0), .xor_hi = vgetq_lane_u64(xorv, 1),
                        };
                        if (write(data->dump_fd, &rec, sizeof(rec)) < 0) { /* best effort */ }
                    }
                }
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_dump data miscompare (records in dump bin)");
        }
        iter++;
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dump_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorDumpData *>(test->data);
    if (data) {
        if (data->dump_fd >= 0) close(data->dump_fd);
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_dump, "Dump variant of neon_rot_ldr_at_top_rowmajor: same hot loop (alternating asc/desc sweeps + 3rd ldr at top + 2-src + rot ALU + bb str/str); on fail logs srcA/srcB/exp/act/xor + sweep_dir per index + binary records (load- vs ALU-error and address-local vs sweep-seam classification)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_dump_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_dump_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_dump_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
