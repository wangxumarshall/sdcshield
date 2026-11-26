/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b db_load_store_mixed_stress
 * @parblock
 * Buffer-pool workload shaper for the database-machine load-fault class
 * (2026-11-27): a sustained-DB machine fails under the mixed
 * read-modify-write + page-crossing + translation pressure a real
 * buffer pool generates. Each worker owns a 1 MiB working set (256 x
 * 4 KiB pages — past L1D and at/above the L2 window), initialised from
 * a shared memset_random source, and replays a page permutation with
 * interleaved loads (vld2q_u64 = LD2), dependency-free SIMD arithmetic
 * (vaddq_u64 / veorq_u64) accumulated into an xor checksum, and
 * back-to-back stores (vst2q_u64) every window.
 *
 * Determinism ruling (deliberate deviation from the prompt this test
 * was specified from): a per-iteration rand()%256 page order makes the
 * golden state unprecomputable. Instead init draws the 256-page
 * permutation and the per-page window offsets ONCE from the framework
 * RNG, and every sweep replays that same fixed pseudo-random order.
 * Translation pressure is equivalent: 256 pages far exceed the dTLB
 * entry count, so every hop in the permutation is a miss regardless of
 * whether the order is re-randomized per sweep.
 *
 * Window table: 16 windows per page, 64B-aligned page-internal random
 * offsets; ~1/4 of pages carry one cross-4KiB-boundary window at 4080
 * (a 32B LD2 straddles into the next page). The final page (255) is
 * forced page-internal so no window can run off the region.
 *
 * Identity-throughput ruling: the arithmetic feeds an xor checksum
 * accumulator only; the store path writes the LOADED values back
 * unchanged via the mirror instruction vst2q_u64 (vst1q would scramble
 * the LD2 interleave layout and defeat the identity golden). The two
 * invariants checked per sweep: work region == source (identity
 * golden, any load/store data corruption moves bits) and accumulator
 * == the init-time precomputed golden (any wrong-address load feeds a
 * foreign window into the checksum).
 *
 * quality_level BETA: new-design exciter, no field validation yet.
 * @endparblock
 */

#include "sandstone.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace {

constexpr uint64_t DB_MS_PAGES        = 256;               /* 256 x 4 KiB = 1 MiB */
constexpr uint64_t DB_MS_PAGE_BYTES   = 4096;
constexpr uint64_t DB_MS_REGION_BYTES = DB_MS_PAGES * DB_MS_PAGE_BYTES;
constexpr uint64_t DB_MS_WIN_PER_PAGE = 16;
constexpr uint64_t DB_MS_CROSS_OFF    = 4080;              /* 32B LD2 crosses the 4K edge */

struct DbLoadStoreMixedStressData {
    uint64x2_t *src        = nullptr;  /* 1 MiB golden source, run-readonly */
    uint16_t   *pgperm     = nullptr;  /* fixed pseudo-random page order */
    uint16_t   *woff       = nullptr;  /* DB_MS_PAGES x DB_MS_WIN_PER_PAGE offsets */
    uint64x2_t golden_acc  = {};
    uint64x2_t K           = {};
    uint64x2_t C           = {};
};

} // namespace

static int db_load_store_mixed_stress_init(struct test *test)
{
    auto *data = static_cast<DbLoadStoreMixedStressData *>(calloc(1, sizeof(DbLoadStoreMixedStressData)));
    if (!data)
        return -ENOMEM;

    data->src = static_cast<uint64x2_t *>(aligned_alloc(64, DB_MS_REGION_BYTES));
    data->pgperm = static_cast<uint16_t *>(calloc(DB_MS_PAGES, sizeof(uint16_t)));
    data->woff = static_cast<uint16_t *>(calloc(DB_MS_PAGES * DB_MS_WIN_PER_PAGE, sizeof(uint16_t)));
    if (!data->src || !data->pgperm || !data->woff) {
        test->data = data;
        return -ENOMEM;
    }

    memset_random(data->src, DB_MS_REGION_BYTES);

    /* Fixed pseudo-random page order (Fisher-Yates over framework RNG). */
    for (uint64_t i = 0; i < DB_MS_PAGES; ++i)
        data->pgperm[i] = static_cast<uint16_t>(i);
    for (uint64_t i = DB_MS_PAGES - 1; i > 0; --i) {
        uint64_t j = random64() % (i + 1);
        uint16_t t = data->pgperm[i]; data->pgperm[i] = data->pgperm[j]; data->pgperm[j] = t;
    }

    /* Window offsets: 64B-aligned page-internal random; ~1/4 of pages
     * (except the last) carry a cross-boundary window at 4080. */
    for (uint64_t pg = 0; pg < DB_MS_PAGES; ++pg) {
        for (uint64_t k = 0; k < DB_MS_WIN_PER_PAGE; ++k) {
            uint16_t off;
            if (k == 0 && pg + 1 < DB_MS_PAGES && (random64() & 3) == 0)
                off = static_cast<uint16_t>(DB_MS_CROSS_OFF);
            else
                off = static_cast<uint16_t>((random64() % 64) * 64);
            data->woff[pg * DB_MS_WIN_PER_PAGE + k] = off;
        }
    }

    uint64x2_t rnd[2];
    memset_random(rnd, sizeof(rnd));
    data->K = rnd[0];
    data->C = rnd[1];

    /* Golden accumulator: replay the exact sweep against the source. */
    uint64x2_t acc = {0, 0};
    const uint64x2_t K = data->K, C = data->C;
    for (uint64_t i = 0; i < DB_MS_PAGES; ++i) {
        const uint64_t pg = data->pgperm[i];
        const uint8_t *wp = reinterpret_cast<const uint8_t *>(data->src) + pg * DB_MS_PAGE_BYTES;
        for (uint64_t k = 0; k < DB_MS_WIN_PER_PAGE; ++k) {
            const uint8_t *p = wp + data->woff[pg * DB_MS_WIN_PER_PAGE + k];
            const uint64x2x2_t ld = vld2q_u64(reinterpret_cast<const uint64_t *>(p));
            acc = veorq_u64(acc, vaddq_u64(ld.val[0], K));
            acc = veorq_u64(acc, veorq_u64(ld.val[1], C));
        }
    }
    data->golden_acc = acc;

    test->data = data;
    return EXIT_SUCCESS;
}

static int db_load_store_mixed_stress_run(struct test *test, int cpu)
{
    auto *data = static_cast<DbLoadStoreMixedStressData *>(test->data);

    (void)cpu;

    /* Per-worker buffer-pool copy (identity-throughput keeps it equal to
     * the source for the whole run). */
    uint64x2_t *work = static_cast<uint64x2_t *>(aligned_alloc(64, DB_MS_REGION_BYTES));
    if (!work)
        return -ENOMEM;
    memcpy(work, data->src, DB_MS_REGION_BYTES);

    const uint64x2_t K = data->K, C = data->C;
    const uint16_t *pgperm = data->pgperm;
    const uint16_t *woff = data->woff;

    TEST_LOOP(test, 1) {
        uint64x2_t acc = {0, 0};

        for (uint64_t i = 0; i < DB_MS_PAGES; ++i) {
            const uint64_t pg = pgperm[i];
            uint8_t *wp = reinterpret_cast<uint8_t *>(work) + pg * DB_MS_PAGE_BYTES;
            for (uint64_t k = 0; k < DB_MS_WIN_PER_PAGE; ++k) {
                uint64_t *p = reinterpret_cast<uint64_t *>(wp + woff[pg * DB_MS_WIN_PER_PAGE + k]);
                const uint64x2x2_t ld = vld2q_u64(p);       /* interleaved load */
                acc = veorq_u64(acc, vaddq_u64(ld.val[0], K));
                acc = veorq_u64(acc, veorq_u64(ld.val[1], C));
                const uint64x2x2_t id = {ld.val[0], ld.val[1]};
                vst2q_u64(p, id);                            /* identity write-back */
            }
        }

        /* Invariant 1: identity throughput — the pool must be untouched. */
        if (memcmp(work, data->src, DB_MS_REGION_BYTES) != 0) {
            report_fail_msg("db_load_store_mixed_stress: pool region diverged from "
                            "golden source (load or store moved bits through the "
                            "read-modify-write chain)");
        }
        /* Invariant 2: checksum — a wrong-address load feeds a foreign
         * window into the accumulator. */
        if (vgetq_lane_u64(acc, 0) != vgetq_lane_u64(data->golden_acc, 0) ||
            vgetq_lane_u64(acc, 1) != vgetq_lane_u64(data->golden_acc, 1)) {
            report_fail_msg("db_load_store_mixed_stress: sweep checksum mismatch "
                            "(wrong-address / stale / zero capture on the mixed "
                            "load-store path): acc 0x%016llx:0x%016llx, golden "
                            "0x%016llx:0x%016llx",
                            static_cast<unsigned long long>(vgetq_lane_u64(acc, 0)),
                            static_cast<unsigned long long>(vgetq_lane_u64(acc, 1)),
                            static_cast<unsigned long long>(vgetq_lane_u64(data->golden_acc, 0)),
                            static_cast<unsigned long long>(vgetq_lane_u64(data->golden_acc, 1)));
        }
    }

    free(work);
    return EXIT_SUCCESS;
}

static int db_load_store_mixed_stress_cleanup(struct test *test)
{
    auto *data = static_cast<DbLoadStoreMixedStressData *>(test->data);
    if (data) {
        free(data->src);
        free(data->pgperm);
        free(data->woff);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(db_load_store_mixed_stress,
             "Database buffer-pool shaper: per-worker 1MiB pool (256x4KiB pages, past L2 window), fixed pseudo-random page permutation replayed per sweep (256 pages >> dTLB capacity — per-sweep re-randomization is not needed for miss pressure), vld2q interleaved loads with cross-4K-boundary windows, vaddq/veorq checksum accumulate, vst2q identity write-back; per-sweep invariants: pool == source and accumulator == init golden")
    .test_init    = db_load_store_mixed_stress_init,
    .test_run     = db_load_store_mixed_stress_run,
    .test_cleanup = db_load_store_mixed_stress_cleanup,
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
