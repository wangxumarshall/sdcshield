/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_str3_checkall
 * @parblock
 * Store-slot classification variant of neon_rot_ldr_at_top_str3
 * (2026-09-17).
 *
 * Parent str3 fired 51 + 49 fails (two runs, all thread 179), but its
 * per-sweep check memcmps ONLY dst -- the third of three stores that all
 * write the SAME res register to three different buffers (temp, junk,
 * dst).  Whether the 1st and 2nd stores also corrupt is UNKNOWN, and the
 * per-slot trigger RATE comparison is the pre-registered question of this
 * cell:
 *   slot_mask 0b111  -> res itself wrong: upstream load/ALU error
 *   slot_mask 1<<k   -> store k's own write path corrupted the value
 *   rates s0 vs s1 vs s2 -> position-dependent store-path failure rates
 *
 * Hot loop is IDENTICAL to str3 (3 ldr: top dead scratch + 2-src
 * srcA/srcB; rotating ALU add/eor/and/orr by i%4; 3 str of the same res
 * to temp/junk/dst in that order; same TEST_LOOP count 1<<13).  The
 * per-sweep section replaces the parent's single memcmp(dst,...) with
 * THREE memcmps -- temp, junk and dst each vs the golden -- all run
 * unconditionally (never gated on dst), so junk-only and temp-only
 * errors are caught.  On any mismatch a noinline cold scan records, per
 * mismatching index, WHICH store slot(s) are wrong plus all three slot
 * actuals and a cold re-read of srcA/srcB (load- vs ALU-error
 * classification, as in rowmajor_dump).
 *
 * Within-run slot-rate comparison (s0 vs s1 vs s2) is immune to the
 * check-section layout confound: all three slots share the same loop,
 * the same sweep and the same timing.  Cross-test rate comparison
 * (str3_checkall vs str3) is NOT valid -- the per-sweep section grew
 * from 1 to 3 memcmps (same known confound as str4_checkall); this test
 * is a slot classifier, not a rate point.
 *
 * Records go to /var/tmp/neon_rot_ldr_at_top_str3_checkall_<pid>.bin
 * (on-disk, per the 09-08 panic lesson: /tmp is tmpfs and loses records
 * on crash).
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

#define NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT 1024

struct NeonRotLdrAtTopStr3CheckallData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
    int dump_fd;
};

#pragma pack(push, 1)
struct neon_rot_ldr_at_top_str3_checkall_record {
    uint64_t timestamp_us;
    uint32_t cpu;
    uint32_t iteration;
    uint32_t sweep_dir;       /* 0=ascending 1=descending */
    uint32_t sweep_slot_mask; /* first-read memcmp result: bit0=temp bit1=junk bit2=dst */
    uint32_t index;           /* 0xFFFFFFFF = transient (sweep-level only, re-read clean) */
    uint32_t op;              /* 0=add 1=eor 2=and 3=orr (i%4); 0xFFFFFFFF for transient */
    uint32_t index_slot_mask; /* cold per-index re-read, same bit layout */
    uint32_t _pad;
    uint64_t srcA_lo, srcA_hi; /* cold re-read (load- vs ALU-error classification) */
    uint64_t srcB_lo, srcB_hi;
    uint64_t exp_lo, exp_hi;
    uint64_t temp_lo, temp_hi; /* per-slot actuals (cold re-read) */
    uint64_t junk_lo, junk_hi;
    uint64_t dst_lo, dst_hi;
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

/* cold path only: noinline so the per-index scan can never be fused or
 * scheduled into the hot loop (tlbstress loop-fusion lesson) */
static void __attribute__((noinline)) str3_checkall_cold_scan(
        struct NeonRotLdrAtTopStr3CheckallData *data,
        uint64x2_t *temp, uint64x2_t *junk, uint64x2_t *dst,
        int cpu, uint32_t iter, bool ascending, uint32_t sweep_slot_mask)
{
    bool any_index = false;

    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT; i++) {
        const uint64x2_t exp = data->expected[i];
        const uint64x2_t t   = temp[i];
        const uint64x2_t j   = junk[i];
        const uint64x2_t d   = dst[i];

        uint32_t m = 0;
        if (!vec_eq(t, exp)) m |= 1u << 0;
        if (!vec_eq(j, exp)) m |= 1u << 1;
        if (!vec_eq(d, exp)) m |= 1u << 2;
        if (m == 0)
            continue;

        any_index = true;
        /* reread sources here (cold) to classify load- vs ALU-error */
        const uint64x2_t a = load_vec(&data->srcA[i]);
        const uint64x2_t b = load_vec(&data->srcB[i]);

        log_error("neon_rot_ldr_at_top_str3_checkall: idx %u op %u dir %s cpu %u: "
                  "slot_mask=0x%x [s0=temp s1=junk s2=dst] "
                  "srcA=%016lx%016lx srcB=%016lx%016lx exp=%016lx%016lx "
                  "temp=%016lx%016lx junk=%016lx%016lx dst=%016lx%016lx",
                  (unsigned)i, (unsigned)(i % 4),
                  ascending ? "asc" : "desc",
                  (unsigned)device_info[cpu].cpu_number,
                  m,
                  vgetq_lane_u64(a,1), vgetq_lane_u64(a,0),
                  vgetq_lane_u64(b,1), vgetq_lane_u64(b,0),
                  vgetq_lane_u64(exp,1), vgetq_lane_u64(exp,0),
                  vgetq_lane_u64(t,1),  vgetq_lane_u64(t,0),
                  vgetq_lane_u64(j,1),  vgetq_lane_u64(j,0),
                  vgetq_lane_u64(d,1),  vgetq_lane_u64(d,0));

        if (data->dump_fd >= 0) {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            struct neon_rot_ldr_at_top_str3_checkall_record rec = {
                .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                .cpu = (uint32_t)device_info[cpu].cpu_number, /* cpu param is thread_num; translate to logical CPU */
                .iteration = iter,
                .sweep_dir = ascending ? 0u : 1u,
                .sweep_slot_mask = sweep_slot_mask,
                .index = (uint32_t)i,
                .op = (uint32_t)(i % 4),
                .index_slot_mask = m,
                ._pad = 0,
                .srcA_lo = vgetq_lane_u64(a, 0), .srcA_hi = vgetq_lane_u64(a, 1),
                .srcB_lo = vgetq_lane_u64(b, 0), .srcB_hi = vgetq_lane_u64(b, 1),
                .exp_lo = vgetq_lane_u64(exp, 0), .exp_hi = vgetq_lane_u64(exp, 1),
                .temp_lo = vgetq_lane_u64(t, 0), .temp_hi = vgetq_lane_u64(t, 1),
                .junk_lo = vgetq_lane_u64(j, 0), .junk_hi = vgetq_lane_u64(j, 1),
                .dst_lo = vgetq_lane_u64(d, 0), .dst_hi = vgetq_lane_u64(d, 1),
            };
            write(data->dump_fd, &rec, sizeof(rec));
        }
    }

    if (!any_index) {
        /* sweep-level mismatch whose per-index re-read came back clean:
         * transient / self-scrubbed error -- still record it */
        log_error("neon_rot_ldr_at_top_str3_checkall: sweep slot_mask=0x%x but per-index re-read clean (transient), cpu %u iter %u dir %s",
                  sweep_slot_mask, (unsigned)device_info[cpu].cpu_number, iter,
                  ascending ? "asc" : "desc");
        if (data->dump_fd >= 0) {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            struct neon_rot_ldr_at_top_str3_checkall_record rec = {
                .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                .cpu = (uint32_t)device_info[cpu].cpu_number,
                .iteration = iter,
                .sweep_dir = ascending ? 0u : 1u,
                .sweep_slot_mask = sweep_slot_mask,
                .index = 0xFFFFFFFFu,
                .op = 0xFFFFFFFFu,
                .index_slot_mask = 0,
                ._pad = 0,
                /* remaining fields zero: the mismatch did not survive the re-read */
            };
            write(data->dump_fd, &rec, sizeof(rec));
        }
    }
}

#endif

static int neon_rot_ldr_at_top_str3_checkall_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallData *>(malloc(sizeof(NeonRotLdrAtTopStr3CheckallData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "/var/tmp/neon_rot_ldr_at_top_str3_checkall_%d.bin", getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("neon_rot_ldr_at_top_str3_checkall: failed to open dump log %s", path);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_str3_checkall_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *junk = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT - 1 - n);

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
            store_vec(&junk[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        /* all three slots checked unconditionally -- temp-only and
         * junk-only errors must not be gated on dst (that gating is
         * exactly the parent's blind spot this variant exists to close) */
        uint32_t slot_mask = 0;
        if (memcmp(temp, data->expected, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)) != 0) slot_mask |= 1u << 0;
        if (memcmp(junk, data->expected, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)) != 0) slot_mask |= 1u << 1;
        if (memcmp(dst,  data->expected, NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_COUNT * sizeof(uint64x2_t)) != 0) slot_mask |= 1u << 2;
        if (slot_mask != 0) {
            str3_checkall_cold_scan(data, temp, junk, dst,
                                    cpu, (uint32_t)(sweep - 1), ascending, slot_mask);
            report_fail_msg("neon_rot_ldr_at_top_str3_checkall store-slot miscompare: sweep slot_mask 0x%x (s0=temp s1=junk s2=dst; records in dump bin)", slot_mask);
        }
    }

    free(dst);
    free(junk);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_str3_checkall_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallData *>(test->data);
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

DECLARE_TEST(neon_rot_ldr_at_top_str3_checkall, "Store-slot classification variant of str3: same hot loop (3 ldr + rot ALU + 3 str of same res to temp/junk/dst); per-sweep 3-way memcmp catches temp-only and junk-only errors; on fail records which store slot(s) mismatched (slot_mask 0b111=upstream load/ALU error, single-bit=that store's write path) with per-slot actuals for s0-vs-s1-vs-s2 rate comparison")
    .test_init    = neon_rot_ldr_at_top_str3_checkall_init,
    .test_run     = neon_rot_ldr_at_top_str3_checkall_run,
    .test_cleanup = neon_rot_ldr_at_top_str3_checkall_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
