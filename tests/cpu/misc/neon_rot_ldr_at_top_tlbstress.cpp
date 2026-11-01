/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_tlbstress
 * @parblock
 * MMU/TLB-dimension variant of neon_rot_ldr_at_top_rowmajor (2026-09-09).
 *
 * Every SDC-triggering test so far touches a working set that fits in a
 * handful of 4K pages (16 KiB buffers): the dTLB hits on every access and
 * the page-table-walk path is never exercised. This variant keeps the
 * hot-loop skeleton byte-comparable (3rd ldr at top + 2-src loads +
 * rotating ALU (add/eor/and/orr) + back-to-back str/str, alternating
 * ascending/descending sweeps) but SLIDES the working window through a
 * large mmap'ed region: every outer iteration the window base moves by
 * one page, so the same virtual->physical translations are never reused
 * and the dTLB / page-table-walk path is under continuous pressure.
 *
 * The window size per iteration is identical to the parent's footprint
 * (4 x 16 KiB = 64 KiB), so L1D/L2 cache-level pressure is unchanged
 * relative to the parent; the only added dimension is translation
 * pressure (TLB misses + hardware page-table walks).
 *
 *   trigger rate ~unchanged  -> translation path is NOT part of the
 *                              trigger mechanism
 *   trigger rate changes     -> TLB/page-walk involvement, chase with a
 *                              dump variant
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

/* window = same footprint as parent: 1024 entries x 16 B = 16 KiB per buffer.
 * Region = 256 pages per buffer, window slides one page per outer iteration. */
#define NEON_ROT_LDR_AT_TOP_TLBSTRESS_COUNT 1024
#define NEON_ROT_LDR_AT_TOP_TLBSTRESS_PAGES 256
#define NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES \
    (NEON_ROT_LDR_AT_TOP_TLBSTRESS_COUNT * sizeof(uint64x2_t))

struct NeonRotLdrAtTopTlbStressData {
    char *region;      /* one big mapping, 4 buffers carved out of it */
    size_t region_bytes;
    /* NOTE: no per-test golden buffer here. The golden must be recomputed
     * per window per THREAD; a shared one would be a deterministic race
     * (threads drift apart in iter, overwriting each other's golden --
     * the 2026-09-09 smoke run failed all 191 healthy cores this way).
     * run() allocates its own golden alongside dst/temp. */
};

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

/* per-iteration window layout inside the region:
 * [srcA window | srcB window | scratch window] x 16 KiB, contiguous.
 * Window slides by one page (4096 B) each outer iteration, wrapping after
 * (pages - 1) slides so it always stays inside the region.  Sliding by
 * 4096 B with a 48 KiB window means ~12 distinct windows are alive at
 * once across iterations -- old translations fall out of the dTLB while
 * new ones are walked. */
static char *window_base(struct NeonRotLdrAtTopTlbStressData *data, uint64_t iter) {
    const size_t page = 4096;
    const size_t window_span = 3 * NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES;
    const size_t max_off = (NEON_ROT_LDR_AT_TOP_TLBSTRESS_PAGES - 1) * page;
    const size_t off = (size_t)((iter * page) % (max_off + page));
    (void)window_span;
    return data->region + off;
}

/* Golden recompute for the active window. Deliberately NOINLINE: when this
 * loop was inline in run(), GCC 12 fused it with the (same-shape) hot sweep
 * and eliminated the sweep's dst stores -- every core failed the smoke run
 * (2026-09-09). A noinline call is an optimization barrier the compiler
 * cannot fuse loops across; the hot loop stays byte-pure. */
__attribute__((noinline))
static void tlbstress_compute_golden(uint64x2_t *golden, const uint64x2_t *srcA,
                                     const uint64x2_t *srcB, int n) {
    for (int i = 0; i < n; i++) {
        switch (i % 4) {
            case 0: golden[i] = vaddq_u64(srcA[i], srcB[i]); break;
            case 1: golden[i] = veorq_u64(srcA[i], srcB[i]); break;
            case 2: golden[i] = vandq_u64(srcA[i], srcB[i]); break;
            case 3: golden[i] = vorrq_u64(srcA[i], srcB[i]); break;
        }
    }
}

static int neon_rot_ldr_at_top_tlbstress_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopTlbStressData *>(malloc(sizeof(NeonRotLdrAtTopTlbStressData)));

    /* region: 256 pages + room for one full 48 KiB window slide = 1 MiB + 48 KiB.
     * (256 pages of slide range x 4096 + 48 KiB window span, rounded up.) */
    data->region_bytes = NEON_ROT_LDR_AT_TOP_TLBSTRESS_PAGES * 4096
                       + 3 * NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES;
    data->region = static_cast<char *>(mmap(nullptr, data->region_bytes,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
                                            -1, 0));
    if (data->region == MAP_FAILED) {
        log_error("neon_rot_ldr_at_top_tlbstress: mmap failed");
        free(data);
        return EXIT_FAILURE;
    }
    memset_random(data->region, data->region_bytes);

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_tlbstress_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopTlbStressData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES));
    /* per-THREAD golden: threads drift in iter, so a shared golden would be
     * overwritten by whichever thread is in a different window (deterministic
     * race -- see struct comment). */
    uint64x2_t *expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES));

    uint64_t iter = 0;
    TEST_LOOP(test, 1 << 13) {
        char *win = window_base(data, iter);
        uint64x2_t *srcA    = reinterpret_cast<uint64x2_t *>(win);
        uint64x2_t *srcB    = reinterpret_cast<uint64x2_t *>(win + NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES);
        uint64x2_t *scratch = reinterpret_cast<uint64x2_t *>(win + 2 * NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES);

        /* cold path: golden for THIS window (noinline -- see comment on
         * tlbstress_compute_golden: inline version let GCC fuse this loop
         * with the hot sweep and delete the dst stores) */
        tlbstress_compute_golden(expected, srcA, srcB,
                                 NEON_ROT_LDR_AT_TOP_TLBSTRESS_COUNT);

        const bool ascending = (iter & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_TLBSTRESS_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_TLBSTRESS_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&srcA[i]);
            uint64x2_t b = load_vec(&srcB[i]);
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

        if (memcmp(dst, expected, NEON_ROT_LDR_AT_TOP_TLBSTRESS_WINDOW_BYTES) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_tlbstress data miscompare");
        }
        iter++;
    }

    free(expected);
    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_tlbstress_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopTlbStressData *>(test->data);
    if (data) {
        if (data->region != MAP_FAILED) munmap(data->region, data->region_bytes);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_tlbstress, "TLB-dimension variant of ldr_at_top_rowmajor: same skeleton but the 48KiB working window slides one page per sweep through a 1MiB region (continuous dTLB miss + page-table-walk pressure, cache-level pressure unchanged)")
    .test_init    = neon_rot_ldr_at_top_tlbstress_init,
    .test_run     = neon_rot_ldr_at_top_tlbstress_run,
    .test_cleanup = neon_rot_ldr_at_top_tlbstress_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
