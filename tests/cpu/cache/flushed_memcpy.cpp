/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test flushed_memcpy
 * @parblock
 * Stress memcpy by constantly rotating an array while simultaneously
 * flushing it out of cache (633 #28 lineage, user spec 2026-11-02).
 *
 * What makes this different from cache_stress_clflush_2 (the #26
 * lineage): the data actually ROTATES — each round copies with a
 * fresh random rotate amount k (dst[i] = src[(i+k) mod N], a
 * cyclic-shift copy that keeps every lane moving) — and the flushes
 * interleave WITH the copy, not after it: every 8 destination lines
 * written, one already-written line is dc-civac'd immediately, so
 * the copy keeps running into lines being evicted underneath it
 * ("while simultaneously flushing", the spec's wording).  After a
 * final drain (dsb sy), the full block is verified against the
 * rotate-expectation built from the untouched src.
 *
 * Round: src <- fresh random64 words; k <- random 1..63; rotate-copy
 * with interleaved flushes; dsb sy; build expected[i] = src[(i+k) mod
 * N]; memcmp(dst, expected) — src is never written after its fill,
 * so the expectation is an independent golden; every dst word is
 * written each round, so no stale-bytes hazard exists.
 *
 * Design traps found in the user's draft (fixed, same family set):
 *   - the "rotating" of the spec was not implemented at all (plain
 *     fill->copy->flush->verify, i.e. the #26 shape again);
 *   - the local store->load consistency pass (tmp = v; loaded = tmp;)
 *     is the same compiler-forwarding fake seen three times today —
 *     removed;
 *   - alignas(64) on a std::vector decorates the stack object, not
 *     the heap data — removed (dc civac is line-granular on any
 *     address anyway);
 *   - std::mt19937/random_device floats -> framework random64 words
 *     (bit-level memcmp, NaN/Inf lanes included);
 *   - per-fail fprintf removed; .fracture_loop_count = -1; _cleanup
 *     naming.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <vector>

static constexpr size_t FM_BLOCK_SIZE = 4 * 1024;          /* 4 KB     */
static constexpr size_t FM_NUM_WORDS  = FM_BLOCK_SIZE / 4;  /* 1024     */
static constexpr size_t FM_LINE       = 64;                /* bytes    */
static constexpr size_t FM_FLUSH_EVERY_LINES = 8;          /* interleave */

/* ARM64 cache-line clean+invalidate (CLFLUSH equivalent) */
static inline void dc_civac(void *addr) {
    __asm__ volatile("dc civac, %0" :: "r"(addr) : "memory");
}

static inline void dsb_sy() {
    __asm__ volatile("dsb sy" ::: "memory");
}

static int flushed_memcpy_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int flushed_memcpy_cleanup(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int flushed_memcpy_run(struct test *test, int cpu) {
    (void)cpu;

    std::vector<uint32_t> src(FM_NUM_WORDS);
    std::vector<uint32_t> dst(FM_NUM_WORDS);
    std::vector<uint32_t> expected(FM_NUM_WORDS);

    uint64_t rounds = 0;

    do {
        /* 1. fresh random source every round */
        for (size_t i = 0; i < FM_NUM_WORDS; ++i)
            src[i] = (uint32_t)random64();

        /* 2. rotate amount for this round */
        const size_t k = 1 + (size_t)(random64() % 63);

        /* 3. rotate-copy with flushes interleaved INTO the copy:
         * every FM_FLUSH_EVERY_LINES completed destination lines,
         * evict one already-written line immediately */
        size_t flushed_lines = 0;
        for (size_t i = 0; i < FM_NUM_WORDS; ++i) {
            dst[i] = src[(i + k) % FM_NUM_WORDS];
            if ((i + 1) % (FM_FLUSH_EVERY_LINES * (FM_LINE / 4)) == 0) {
                /* evict a line a few lines back: genuinely completed
                 * work, still adjacent to the copy frontier */
                size_t victim_line = i / (FM_LINE / 4);
                if (victim_line >= FM_FLUSH_EVERY_LINES) {
                    victim_line -= FM_FLUSH_EVERY_LINES;
                    dc_civac(dst.data() + victim_line * (FM_LINE / 4));
                    ++flushed_lines;
                }
            }
        }
        dsb_sy();   /* drain: all flushes complete before verify */

        /* 4. independent golden: the rotate expectation from src */
        for (size_t i = 0; i < FM_NUM_WORDS; ++i)
            expected[i] = src[(i + k) % FM_NUM_WORDS];

        if (memcmp(dst.data(), expected.data(), FM_BLOCK_SIZE) != 0) {
            for (size_t i = 0; i < FM_NUM_WORDS; ++i) {
                if (dst[i] != expected[i]) {
                    report_fail_msg("flushed_memcpy: rotate-copy mismatch "
                                    "at word %zu (round %lu, k=%zu, "
                                    "flushed %zu lines): expected 0x%08x "
                                    "got 0x%08x", i, (unsigned long)rounds,
                                    k, flushed_lines, expected[i], dst[i]);
                    return EXIT_FAILURE;
                }
            }
        }

        ++rounds;
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(flushed_memcpy, "Rotate-copy (cyclic shift, fresh k each round) with dc-civac evictions interleaved into the copy frontier, full verify vs rotate golden (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = flushed_memcpy_init,
    .test_run = flushed_memcpy_run,
    .test_cleanup = flushed_memcpy_cleanup,
    .fracture_loop_count = -1,   /* never fracture: fast semantic rounds */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST
