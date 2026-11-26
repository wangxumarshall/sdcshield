/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_dump_l2_512k
 * @parblock
 * L2-resident tier 128KB of neon_rot_ldr_at_top_rowmajor_dump
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * integer read-side recipe (scratch dead asm ldr + two asm source
 * loads + rotating add/eor/and/orr + back-to-back str/str pair) is the
 * object under test; the identity-copy trio in this directory covers
 * the identity axis, this family the rot4 axis.  The cold-path dump
 * discipline is inherited from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_dump.cpp): hot loop
 * item-for-item the parent, the per-index scan + per-record binary
 * dump live ONLY inside the post-sweep mismatch branch.
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on the compared destination block).
 * The scalar golden covers the FULL window at init; the sweep-end
 * memcmp narrows to the slice [off, off+LEN).
 *
 * Deltas from the parent (deliberate, geometry-side): the scan covers
 * the WINDOW SLICE only; the binary record gains a window_off field
 * (this tier family's own format, disclosed here — offline slice math
 * needs it); the log line carries off.  The record's index field stays
 * the ABSOLUTE index (off + j).  On-disk location stays /var/tmp (the
 * 09-08 panic lesson: /tmp is tmpfs and loses records on crash).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 32768 uint64x2_t = 512KB — the
 * user-named upper boundary, exactly L2 capacity on the reference
 * geometry the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 512KB = 2.5MB —
 * EXCEEDS L2 and self-evicts (the five-buffer chassis geometry, disclosed
 * honestly; the tier axis is the input block size).
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

#define NRRD512_LEN 32768                  /* uint64x2_t elements -> 512KB block */
#define NRRD512_WIN (2 * NRRD512_LEN)     /* 2x-block window (memcpy_l2 method) */

struct NRRD512Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
    int dump_fd;
};

/* this tier family's own record format: the parent's fields plus
 * window_off (the slide needs it for offline slice math) */
#pragma pack(push, 1)
struct nrrd128_record {
    uint64_t timestamp_us;
    uint32_t cpu;
    uint32_t iteration;
    uint32_t sweep_dir;     /* 0=ascending 1=descending (iteration & 1) */
    uint32_t window_off;    /* elements: this sweep's random slice start */
    uint32_t index;         /* ABSOLUTE index (off + j) */
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

static int neon_rot_ldr_at_top_rowmajor_dump_l2_512k_init(struct test *test) {
    auto *data = static_cast<NRRD512Data *>(malloc(sizeof(NRRD512Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NRRD512_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NRRD512_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRRD512_WIN * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NRRD512_WIN * sizeof(uint64x2_t)));

    memset_random(data->srcA, NRRD512_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRRD512_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRRD512_WIN * sizeof(uint64x2_t));

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window — pure C integer
     * ops on the scalar pipeline, never the NEON units under test. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRRD512_WIN; e++) {
            switch ((e / 2) % 4) {   /* op phase: vector index % 4 */
                case 0: ex[e] = sa[e] + sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & sb[e]; break;
                case 3: ex[e] = sa[e] | sb[e]; break;
            }
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "/var/tmp/neon_rot_ldr_at_top_rowmajor_dump_l2_512k_%d.bin", getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("neon_rot_ldr_at_top_rowmajor_dump_l2_512k: failed to open dump log %s", path);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dump_l2_512k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRRD512Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRRD512_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRRD512_WIN * sizeof(uint64x2_t)));

    uint32_t iter = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot4 slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRRD512_LEN / 4)));

        /* poison the compared destination block: a lost write stays
         * 0x5a5a.. and fails the slice compare */
        memset(dst + off, 0x5A, NRRD512_LEN * sizeof(uint64x2_t));

        const bool ascending = (iter & 1) == 0;
        for (int n = 0; n < NRRD512_LEN; n++) {
            const int j = ascending ? n : (NRRD512_LEN - 1 - n);
            const int i = off + j;

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

        /* cold path: single slice memcmp per sweep; the per-index scan +
         * dump runs ONLY inside the mismatch branch */
        if (memcmp(dst + off, data->expected + off, NRRD512_LEN * sizeof(uint64x2_t)) != 0) {
            for (int k = 0; k < NRRD512_LEN; k++) {
                const int i = off + k;
                if (!vec_eq(dst[i], data->expected[i])) {
                    /* reread sources here (cold) to classify load- vs ALU-error */
                    uint64x2_t a = load_vec(&data->srcA[i]);
                    uint64x2_t b = load_vec(&data->srcB[i]);
                    uint64x2_t exp = data->expected[i];
                    uint64x2_t act = dst[i];
                    uint64x2_t xorv = veorq_u64(exp, act);

                    log_error("neon_rot_ldr_at_top_rowmajor_dump_l2_512k: idx %u op %u dir %s off %u cpu %u: "
                              "srcA=%016lx%016lx srcB=%016lx%016lx exp=%016lx%016lx "
                              "act=%016lx%016lx xor=%016lx%016lx",
                              (unsigned)i, (unsigned)(i % 4),
                              ascending ? "asc" : "desc", (unsigned)off,
                              (unsigned)device_info[cpu].cpu_number, /* logical CPU */
                              vgetq_lane_u64(a,1), vgetq_lane_u64(a,0),
                              vgetq_lane_u64(b,1), vgetq_lane_u64(b,0),
                              vgetq_lane_u64(exp,1), vgetq_lane_u64(exp,0),
                              vgetq_lane_u64(act,1), vgetq_lane_u64(act,0),
                              vgetq_lane_u64(xorv,1), vgetq_lane_u64(xorv,0));

                    if (data->dump_fd >= 0) {
                        struct timeval tv;
                        gettimeofday(&tv, nullptr);
                        struct nrrd128_record rec = {
                            .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                            .cpu = (uint32_t)device_info[cpu].cpu_number, /* cpu param is thread_num; translate to logical CPU */
                            .iteration = iter,
                            .sweep_dir = ascending ? 0u : 1u,
                            .window_off = (uint32_t)off,
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
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_dump_l2_512k data miscompare (records in dump bin, off %u)", (unsigned)off);
        }
        iter++;
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dump_l2_512k_cleanup(struct test *test) {
    auto *data = static_cast<NRRD512Data *>(test->data);
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

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_dump_l2_512k, "L2-resident tier 128KB of rowmajor_dump: integer read-side chassis preserved (scratch dead ldr + 2-src asm loads + rot ALU + bb str/str), sliding-window geometry transplanted; on fail logs srcA/srcB/exp/act/xor + sweep_dir + window_off per index + binary records (load- vs ALU-error and address-local vs sweep-seam classification) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_dump_l2_512k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_dump_l2_512k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_dump_l2_512k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
