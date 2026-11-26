/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b db_load_stale_data_probe
 * @parblock
 * Stale-return probe for the database-machine load-fault class
 * (2026-11-27): the field report shows loads returning the value of a
 * PREVIOUS address/read. This test is a MemDiv-style cache-hierarchy
 * manipulation: it overwrites a line in place, forces the line out of
 * L1D with a proper data-cache invalidate-and-clean, then reloads — the
 * reload must reflect the NEW value; if the load path (LSQ/LVQ entry,
 * L1D fill, or store-to-load forwarding residue) serves the OLD value,
 * the test fires immediately.
 *
 * Sequence per slot (one pinned asm block, "memory" clobber):
 *   ldr q0, [p]        -- establish the line in L1D / queue activity
 *   str q1, [p]        -- overwrite in place with the slot's new random value
 *   dc civac, p        -- clean+invalidate the data line to L2/L3
 *   dsb sy             -- completion barrier BEFORE the reload
 *   ldr q2, [p]        -- reload: must observe the new value
 * The reload is compared (veorq_u64) against the new value in-loop; a
 * mismatch is reported with old/new/reloaded lanes so a capture can be
 * triaged as stale-old-value vs third-value corruption.
 *
 * Check-direction note (deliberate deviation from the prompt this test
 * was specified from): comparing q0 against q2 is NOT the check — those
 * are the pre-overwrite and post-overwrite reads and are expected to
 * differ in a healthy machine. The invariant is q2 == new value; the
 * defect signature is q2 == old value.
 *
 * Eviction note: dc civac + dsb sy, never __builtin___clear_cache — on
 * aarch64 that helper goes through libc __clear_cache which invalidates
 * the INSTRUCTION cache and does not evict a data line (see
 * cache_stress_civac header). Each sweep covers all 16384 16-byte slots
 * and ends with a full-region memcmp against the new-value array (the
 * only legal post-overwrite state).
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

constexpr uint64_t DB_SDPROBE_SLOTS = 1ull << 14;   /* 16384 x 16B = 256 KiB */
constexpr uint64_t DB_SDPROBE_REGION_BYTES = DB_SDPROBE_SLOTS * 16;

struct DbLoadStaleDataProbeData {
    uint64x2_t *target  = nullptr;  /* overwritten in place every sweep */
    uint64x2_t *newvals  = nullptr; /* per-slot new random value; run-readonly */
};

} // namespace

static int db_load_stale_data_probe_init(struct test *test)
{
    auto *data = static_cast<DbLoadStaleDataProbeData *>(calloc(1, sizeof(DbLoadStaleDataProbeData)));
    if (!data)
        return -ENOMEM;

    data->target = static_cast<uint64x2_t *>(aligned_alloc(64, DB_SDPROBE_REGION_BYTES));
    data->newvals = static_cast<uint64x2_t *>(aligned_alloc(64, DB_SDPROBE_REGION_BYTES));
    if (!data->target || !data->newvals) {
        test->data = data;
        return -ENOMEM;
    }

    memset_random(data->target, DB_SDPROBE_REGION_BYTES);
    memset_random(data->newvals, DB_SDPROBE_REGION_BYTES);

    test->data = data;
    return EXIT_SUCCESS;
}

static int db_load_stale_data_probe_run(struct test *test, int cpu)
{
    auto *data = static_cast<DbLoadStaleDataProbeData *>(test->data);
    uint64x2_t *target = data->target;
    uint64x2_t *newvals = data->newvals;

    (void)cpu;

    TEST_LOOP(test, 1) {
        for (uint64_t i = 0; i < DB_SDPROBE_SLOTS; ++i) {
            uint64x2_t *p = &target[i];
            const uint64x2_t newv = newvals[i];
            uint64x2_t old_v;
            uint64x2_t reloaded;

            __asm__ volatile(
                "ldr %q[oldv], [%[p]]\n\t"
                "str %q[newv], [%[p]]\n\t"
                "dc civac, %[p]\n\t"
                "dsb sy\n\t"
                "ldr %q[reloaded], [%[p]]\n\t"
                : [reloaded] "=&w"(reloaded), [oldv] "=&w"(old_v)
                : [p] "r"(p), [newv] "w"(newv)
                : "memory");

            const uint64x2_t x = veorq_u64(reloaded, newv);
            if (vgetq_lane_u64(x, 0) | vgetq_lane_u64(x, 1)) {
                const uint64_t r0 = vgetq_lane_u64(reloaded, 0);
                const uint64_t r1 = vgetq_lane_u64(reloaded, 1);
                const uint64_t n0 = vgetq_lane_u64(newv, 0);
                const uint64_t n1 = vgetq_lane_u64(newv, 1);
                const uint64_t o0 = vgetq_lane_u64(old_v, 0);
                const uint64_t o1 = vgetq_lane_u64(old_v, 1);
                const bool stale = (r0 == o0 && r1 == o1);
                report_fail_msg("db_load_stale_data_probe: slot %llu reload after "
                                "overwrite+dc-civac+dsb mismatch (%s): "
                                "reloaded 0x%016llx:0x%016llx, new 0x%016llx:0x%016llx, "
                                "old 0x%016llx:0x%016llx",
                                static_cast<unsigned long long>(i),
                                stale ? "STALE old value returned by load path"
                                      : "third-value corruption",
                                static_cast<unsigned long long>(r0),
                                static_cast<unsigned long long>(r1),
                                static_cast<unsigned long long>(n0),
                                static_cast<unsigned long long>(n1),
                                static_cast<unsigned long long>(o0),
                                static_cast<unsigned long long>(o1));
            }
        }

        /* Every slot has now been overwritten, evicted and reloaded: the
         * whole region must equal the new-value array. */
        if (memcmp(target, newvals, DB_SDPROBE_REGION_BYTES) != 0) {
            report_fail_msg("db_load_stale_data_probe: full-region mismatch after "
                            "complete overwrite sweep (stale residue surviving dc civac)");
        }
    }

    return EXIT_SUCCESS;
}

static int db_load_stale_data_probe_cleanup(struct test *test)
{
    auto *data = static_cast<DbLoadStaleDataProbeData *>(test->data);
    if (data) {
        free(data->target);
        free(data->newvals);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(db_load_stale_data_probe,
             "Database load-fault stale probe (MemDiv-style): per-slot pinned sequence ldr q / str new q / dc civac + dsb sy / reload — reload MUST equal the new value (q2==q0 is the defect signature, not the check); in-loop veorq triage with old/new/reloaded lanes, full-region memcmp each sweep")
    .test_init    = db_load_stale_data_probe_init,
    .test_run     = db_load_stale_data_probe_run,
    .test_cleanup = db_load_stale_data_probe_cleanup,
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
