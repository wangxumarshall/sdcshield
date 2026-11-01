/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sched_storm_futex
 * @parblock
 * Scheduler-activity stimulus (2026-09-12), variant A: futex sleep/wake storm.
 *
 * vmcore evidence chain (docs/CORE179_PANIC_ROOT_CAUSE_SCHEDULER_FINDING_20260912.md):
 * all 11 fatal panics on core 179 died on the SAME instruction,
 * find_busiest_group+0x140, reached via newidle_balance -- the load-balancer
 * path taken when the previous task sleeps/exits and the CPU is about to go
 * idle.  The victims were arbitrary processes (8 different ones, including an
 * opendcdiag test thread); the common factor is scheduling activity on 179,
 * not the payload.  This family of tests turns that deduction into a
 * controlled reproduction attempt: hammer the scheduling path itself and let
 * the kernel error channels (journalctl spurious/Oops, kdump vmcore) be the
 * detector.
 *
 * Mechanism note (counter-intuitive): newidle_balance fires when the CPU has
 * NO other runnable task -- so sched_yield is USELESS as a stimulus (the
 * yielding task stays runnable).  The stimulus must make the thread
 * repeatedly DISAPPEAR.  This variant: a single thread does futex WAIT with a
 * 1us timeout in a tight loop.  Every wait puts the CPU on the
 * newidle_balance path; every timeout expiry wakes it back.  30min at this
 * rate is on the order of 10^7 scheduling events per core.
 *
 * No golden/memcmp: there is no data to verify.  result: pass is the normal
 * outcome.  The interesting failure modes are external (machine panic ->
 * vmcore) or mild (futex syscall returning -EFAULT/-EINVAL -- logged and
 * failed -- the error-value-on-futex-args manifestation).
 *
 * Companion variants: sched_storm_thread (do_task_dead path, the #11 case),
 * sched_storm_hybrid (wake-pullback via load_balance), sched_storm_baseline
 * (pure spin, NO sleep/exit -- the negative control: it must NOT crash; if it
 * does, the scheduler-activity root cause is wrong).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <ctime>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/futex.h>

static int sched_storm_futex_run(struct test *test, int cpu) {
    /* per-thread futex word; nobody ever wakes us -- we self-wake by timeout */
    alignas(64) uint32_t fut = 0;

    uint64_t bad_returns = 0;
    /* N = 1<<16: one futex wait+wake costs ~10us (measured 11s per 1<<20
     * batch), so one iteration is ~0.7s -- same granularity as the family */
    TEST_LOOP(test, 1 << 16) {
        struct timespec ts = {0, 1000};   /* 1us: sleep, then timeout-wake */
        long rc = syscall(SYS_futex, &fut, FUTEX_WAIT_PRIVATE, fut, &ts, nullptr, 0);
        /* rc == -1/ETIMEDOUT is the expected path (self-wake).  Any other
         * error (-EFAULT, -EINVAL, -EAGAIN-without-race) means the kernel
         * read garbage from our (userspace) argument block -- the mild
         * manifestation.  -EAGAIN is legal if fut changed; ours never does,
         * so count it too but only report the hard ones. */
        if (rc != 0 && rc != -1)
            bad_returns++;
        else if (rc == -1 && errno != ETIMEDOUT && errno != EAGAIN)
            bad_returns++;
    }

    if (bad_returns != 0) {
        report_fail_msg("sched_storm_futex: %llu futex syscalls returned unexpected errors",
                        (unsigned long long)bad_returns);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(sched_storm_futex, "Scheduler-storm stimulus A: tight futex WAIT(1us timeout) self-wake loop; every wait drives the newidle_balance path (find_busiest_group, the 11-panic crash site); kernel errors observable via journalctl/vmcore")
    .test_run     = sched_storm_futex_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
