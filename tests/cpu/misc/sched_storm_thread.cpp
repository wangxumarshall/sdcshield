/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sched_storm_thread
 * @parblock
 * Scheduler-activity stimulus (2026-09-12), variant B: short-lived thread
 * create/exit/join storm.
 *
 * vmcore evidence chain (docs/CORE179_PANIC_ROOT_CAUSE_SCHEDULER_FINDING_20260912.md):
 * all 11 fatal panics on core 179 died at find_busiest_group+0x140 via
 * newidle_balance.  Case #11 (2026-09-12 10:00, the crash that truncated the
 * ldr4 campaign) had the EXACT stack of this variant: the fatal process was
 * an opendcdiag test thread dying through
 *   do_task_dead -> __schedule -> pick_next_task -> newidle_balance
 *   -> load_balance -> find_busiest_group+0x140
 * i.e. a thread EXIT putting the CPU on the idle-balancing path.
 *
 * This variant recreates that path deliberately, millions of times: each
 * loop iteration creates a short-lived thread whose function body is empty
 * (it exits immediately), then joins it.  Every child-thread exit is one
 * do_task_dead -> newidle_balance traversal on the core where it ran
 * (the framework pins the creating worker to its core; the child, created
 * with no affinity restriction, is scheduled alongside it).
 *
 * pthread_create inside a fork_each_test child has precedent
 * (tests/cpu/cache/cache_stress_aggressor.cpp).  The child lifetime is
 * <1ms so it does not meaningfully compete with the framework's per-core
 * worker model.
 *
 * No golden/memcmp: result: pass is normal; the detector is external
 * (journalctl / vmcore).  pthread_create/join failures are counted and
 * reported through the normal fail channel (resource exhaustion would show
 * up here first).
 *
 * Companions: sched_storm_futex (futex_wait path), sched_storm_hybrid
 * (wake-pullback), sched_storm_baseline (negative control: pure spin, no
 * sleep/exit, must NOT crash).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <pthread.h>
#include <sched.h>

static void *short_lived_thread(void *) {
    return nullptr;
}

static int sched_storm_thread_run(struct test *test, int cpu) {
    /* child threads are created with affinity pinned to the CREATING
     * worker's own CPU: without this, the 24 workers of a NUMA-group
     * control process scatter their short-lived children across all 24
     * cores of the group, every join becomes a cross-core wait, and the
     * whole machine livelocks (measured: 191-core run stalled at 0
     * iterations with 24 R-state threads per control process; single-core
     * run without contention works fine at 1.3s per iteration) */
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(sched_getcpu(), &set);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setaffinity_np(&attr, sizeof(set), &set);

    uint64_t failures = 0;
    /* N = 1<<13: create+join costs ~20us single-core but ~0.75ms at full
     * machine width (191 cores; kernel clone/exit path contention, measured
     * 2026-09-12), so one iteration is ~6s full-width / ~0.16s single-core.
     * N=1<<16 measured 49s per iteration at 191 cores -- the framework
     * checks -T only BETWEEN iterations, so that N made short runs look
     * hung (the "stall at 0 iterations" was this, not a deadlock) */
    TEST_LOOP(test, 1 << 13) {
        pthread_t t;
        int rc = pthread_create(&t, &attr, short_lived_thread, nullptr);
        if (rc != 0) {
            failures++;
            continue;
        }
        rc = pthread_join(t, nullptr);
        if (rc != 0)
            failures++;
    }

    pthread_attr_destroy(&attr);

    if (failures != 0) {
        report_fail_msg("sched_storm_thread: %llu pthread create/join calls failed",
                        (unsigned long long)failures);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(sched_storm_thread, "Scheduler-storm stimulus B: short-lived-thread create->exit->join storm; every child exit drives do_task_dead->newidle_balance (the exact stack of panic case #11, the 09-12 10:00 crash); kernel errors observable via journalctl/vmcore")
    .test_run     = sched_storm_thread_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
