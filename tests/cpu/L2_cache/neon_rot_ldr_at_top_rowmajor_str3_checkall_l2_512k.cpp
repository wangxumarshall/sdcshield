/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_str3_checkall_l2_512k
 * @parblock
 * L2-resident tier 512KB of neon_rot_ldr_at_top_rowmajor_str3_checkall
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * store-slot classification axis is the object under test — three
 * back-to-back str of the same res to temp/junk/dst, per-sweep 3-way
 * memcmp (slot_mask bit per buffer: single-bit = that store's write
 * path, 0b111 = upstream load/ALU error), cold per-index scan with
 * slot masks + binary records, all inherited verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_str3_checkall.cpp).
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on ALL THREE store blocks — all
 * three are checked).  The scalar golden covers the FULL window at
 * init; the sweep-end memcmps narrow to the slice [off, off+LEN).
 *
 * Deltas from the parent (deliberate, geometry-side): the cold scan
 * covers the WINDOW SLICE only; the binary record gains a window_off
 * field (this tier family's own format, disclosed here).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 32768 uint64x2_t = 512KB — the
 * user-named upper boundary, exactly L2 capacity on the reference
 * geometry the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + junk + dst writes = 6 x 512KB =
 * 3MB — exceeds L2 (the six-buffer geometry of the three-slot axis,
 * disclosed honestly; the tier axis is the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRS3_512_LEN 32768                  /* uint64x2_t elements -> 512KB block */
#define NRS3_512_WIN (2 * NRS3_512_LEN)   /* 2x-block window (memcpy_l2 method) */

struct NRS3_512Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;
    int dump_fd;
};

/* this tier family's own record format: the parent's fields plus
 * window_off (the slide needs it for offline slice math) */
#pragma pack(push, 1)
struct nrs3_128_record {
    uint64_t timestamp_us;
    uint32_t cpu;
    uint32_t iteration;
    uint32_t sweep_dir;       /* 0=ascending 1=descending */
    uint32_t window_off;      /* elements: this sweep's random slice start */
    uint32_t sweep_slot_mask; /* per-sweep 3-way memcmp result */
    uint32_t index;           /* ABSOLUTE index (off + j) */
    uint32_t op;              /* 0=add 1=eor 2=and 3=orr (i%4) */
    uint32_t index_slot_mask; /* bit0=temp bit1=junk bit2=dst */
    uint32_t _pad;
    uint64_t srcA_lo, srcA_hi;
    uint64_t srcB_lo, srcB_hi;
    uint64_t exp_lo, exp_hi;
    uint64_t temp_lo, temp_hi;
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

#endif

/* cold path only: noinline so the per-index scan can never be fused or
 * inlined into the hot loop (parent discipline); slice-scoped */
static void __attribute__((noinline)) str3_checkall_l2_cold_scan(
        struct NRS3_512Data *data,
        uint64x2_t *temp, uint64x2_t *junk, uint64x2_t *dst,
        int cpu, uint32_t iter, bool ascending, uint32_t sweep_slot_mask,
        int off)
{
    bool any_index = false;

    for (int k = 0; k < NRS3_512_LEN; k++) {
        const int i = off + k;
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

        log_error("neon_rot_ldr_at_top_str3_checkall_l2_512k: idx %u op %u dir %s off %u cpu %u: "
                  "slot_mask=0x%x [s0=temp s1=junk s2=dst] "
                  "srcA=%016lx%016lx srcB=%016lx%016lx exp=%016lx%016lx "
                  "temp=%016lx%016lx junk=%016lx%016lx dst=%016lx%016lx",
                  (unsigned)i, (unsigned)(i % 4),
                  ascending ? "asc" : "desc", (unsigned)off,
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
            struct nrs3_128_record rec = {
                .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                .cpu = (uint32_t)device_info[cpu].cpu_number,
                .iteration = iter,
                .sweep_dir = ascending ? 0u : 1u,
                .window_off = (uint32_t)off,
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
            if (write(data->dump_fd, &rec, sizeof(rec)) < 0) { /* best effort */ }
        }
    }

    if (!any_index) {
        /* sweep-level mismatch whose per-index re-read came back clean:
         * transient; record it (parent discipline), no per-element data */
        log_error("neon_rot_ldr_at_top_str3_checkall_l2_512k: sweep slot_mask=0x%x but per-index re-read clean (transient), cpu %u iter %u dir %s off %u",
                  sweep_slot_mask, (unsigned)device_info[cpu].cpu_number, iter,
                  ascending ? "asc" : "desc", (unsigned)off);
        if (data->dump_fd >= 0) {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            struct nrs3_128_record rec = {
                .timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec,
                .cpu = (uint32_t)device_info[cpu].cpu_number,
                .iteration = iter,
                .sweep_dir = ascending ? 0u : 1u,
                .window_off = (uint32_t)off,
                .sweep_slot_mask = sweep_slot_mask,
                .index = 0xFFFFFFFFu,
                .op = 0xFFFFFFFFu,
                .index_slot_mask = 0,
                ._pad = 0,
                /* remaining fields zero: the mismatch did not survive the re-read */
            };
            if (write(data->dump_fd, &rec, sizeof(rec)) < 0) { /* best effort */ }
        }
    }
}

static int str3_checkall_l2_512k_init(struct test *test) {
    auto *data = static_cast<NRS3_512Data *>(malloc(sizeof(NRS3_512Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));

    memset_random(data->srcA, NRS3_512_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRS3_512_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRS3_512_WIN * sizeof(uint64x2_t));

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRS3_512_WIN; e++) {
            switch ((e / 2) % 4) {   /* op phase: vector index % 4 */
                case 0: ex[e] = sa[e] + sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & sb[e]; break;
                case 3: ex[e] = sa[e] | sb[e]; break;
            }
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "/var/tmp/neon_rot_ldr_at_top_str3_checkall_l2_512k_%d.bin", getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("neon_rot_ldr_at_top_str3_checkall_l2_512k: failed to open dump log %s", path);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int str3_checkall_l2_512k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRS3_512Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));
    uint64x2_t *junk = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRS3_512_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot4 slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRS3_512_LEN / 4)));

        /* poison ALL THREE store blocks (all are checked): a lost
         * write stays 0x5a5a.. and fails its slot compare */
        memset(temp + off, 0x5A, NRS3_512_LEN * sizeof(uint64x2_t));
        memset(junk + off, 0x5A, NRS3_512_LEN * sizeof(uint64x2_t));
        memset(dst + off, 0x5A, NRS3_512_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRS3_512_LEN; n++) {
            const int j = ascending ? n : (NRS3_512_LEN - 1 - n);
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
            store_vec(&junk[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        /* all three slots checked unconditionally, slice [off, off+LEN)
         * -- temp-only and junk-only errors included (parent discipline) */
        uint32_t slot_mask = 0;
        if (memcmp(temp + off, data->expected + off, NRS3_512_LEN * sizeof(uint64x2_t)) != 0) slot_mask |= 1u << 0;
        if (memcmp(junk + off, data->expected + off, NRS3_512_LEN * sizeof(uint64x2_t)) != 0) slot_mask |= 1u << 1;
        if (memcmp(dst + off,  data->expected + off, NRS3_512_LEN * sizeof(uint64x2_t)) != 0) slot_mask |= 1u << 2;
        if (slot_mask != 0) {
            str3_checkall_l2_cold_scan(data, temp, junk, dst,
                                       cpu, (uint32_t)(sweep - 1), ascending, slot_mask,
                                       off);
            report_fail_msg("neon_rot_ldr_at_top_str3_checkall_l2_512k store-slot miscompare: sweep slot_mask 0x%x off %u (s0=temp s1=junk s2=dst; records in dump bin)", slot_mask, (unsigned)off);
        }
    }

    free(dst);
    free(junk);
    free(temp);
    return EXIT_SUCCESS;
}

static int str3_checkall_l2_512k_cleanup(struct test *test) {
    auto *data = static_cast<NRS3_512Data *>(test->data);
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

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_str3_checkall_l2_512k, "L2-resident tier 512KB of str3_checkall: store-slot classification axis preserved (3 ldr + rot ALU + 3 str of same res to temp/junk/dst, per-sweep 3-way slice memcmp + per-index slot_mask scan + binary records with window_off), sliding-window geometry transplanted (ARM64)")
    .test_init    = str3_checkall_l2_512k_init,
    .test_run     = str3_checkall_l2_512k_run,
    .test_cleanup = str3_checkall_l2_512k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
