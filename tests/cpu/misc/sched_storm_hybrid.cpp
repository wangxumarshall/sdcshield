/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sched_storm_hybrid
 * @parblock
 * Scheduler-activity stimulus (2026-09-12), variant C: two threads pinned to
 * the same core doing an alternating futex wait/wake handshake.
 *
 * vmcore evidence chain (docs/CORE179_PANIC_ROOT_CAUSE_SCHEDULER_FINDING_20260912.md):
 * the 11 fatal panics died inside load balancing reached from
 * newidle_balance.  The futex variant (A) covers the "sleep" side; this
 * variant additionally covers the WAKE side: when thread A sleeps and B
 * wakes it, the wake-up path and the subsequent placement (B sleeps, A is
 * pulled back) exercise the load_balance main path too -- the direction a
 * woken task takes when its CPU has gone idle in the meantime.
 *
 * Handshake discipline (lost-wakeup-proof): every wait uses a 1ms timeout
 * AND the classic futex seq protocol -- waiter re-reads the sequence
 * counter after every wait return, so a wake that raced ahead of the wait
 * is detected by the counter having moved (no sleep), and a truly lost
 * wake is recovered by the timeout.  This makes the loop deadlock-free by
 * construction (verified: the first build with bare wake-then-wait and no
 * timeout deadlocked -- a wake issued before the peer reaches its wait is
 * dropped by the kernel and the bare wait then sleeps forever).
 *
 * At any instant at most one of the two threads is runnable on the core;
 * every handoff is one sleep (newidle opportunity) + one wake (pullback
 * opportunity).  Both threads run on the same logical CPU (the companion
 * matches its affinity to the worker's), so all scheduling happens on one
 * core -- the strongest per-core stimulus of the family.
 *
 * No golden/memcmp: result: pass is normal; detector is external
 * (journalctl / vmcore).  The stop flag ends the companion; it is joined
 * before returning (no thread leak).  futex errors other than the expected
 * ETIMEDOUT/EAGAIN/EWOULDBLOCK returns are counted and reported (the mild
 * manifestation of garbage reads on futex args).
 *
 * Companions: sched_storm_futex (sleep side only), sched_storm_thread
 * (do_task_dead), sched_storm_baseline (negative control).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <ctime>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sched.h>
#include <cerrno>

namespace {

struct PingPong {
    alignas(64) uint32_t seq_a = 0;   /* companion bumps; main waits on it */
    alignas(64) uint32_t seq_b = 0;   /* main bumps; companion waits on it */
    alignas(64) uint32_t stop = 0;    /* companion exit flag */
    uint64_t companion_bad = 0;       /* unexpected futex rc count (companion) */
};

inline long futex_wait(uint32_t *fut, uint32_t expected) {
    struct timespec ts = {0, 1000000};   /* 1ms timeout: lost-wake recovery */
    return syscall(SYS_futex, fut, FUTEX_WAIT_PRIVATE, expected, &ts, nullptr, 0);
}

inline long futex_wake(uint32_t *fut, uint32_t count) {
    return syscall(SYS_futex, fut, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}

} // namespace

static void *hybrid_companion(void *arg) {
    auto *pp = static_cast<PingPong *>(arg);
    while (true) {
        /* wait until main has bumped seq_b (or timeout -- then re-check) */
        uint32_t seen = pp->seq_b;
        if (pp->stop)
            break;
        long rc = futex_wait(&pp->seq_b, seen);
        if (rc != 0 && rc != -1)
            pp->companion_bad++;
        else if (rc == -1 && errno != ETIMEDOUT && errno != EAGAIN && errno != EWOULDBLOCK)
            pp->companion_bad++;
        if (pp->stop)
            break;
        /* our turn: bump main's counter and wake it */
        pp->seq_a++;
        if (futex_wake(&pp->seq_a, 1) < 0)
            pp->companion_bad++;
    }
    return nullptr;
}

static int sched_storm_hybrid_run(struct test *test, int cpu) {
    PingPong pp;

    /* companion pinned to the same CPU the framework pinned this worker to */
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(sched_getcpu(), &set);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setaffinity_np(&attr, sizeof(set), &set);

    pthread_t companion;
    int rc = pthread_create(&companion, &attr, hybrid_companion, &pp);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        report_fail_msg("sched_storm_hybrid: pthread_create failed (%d)", rc);
        return EXIT_FAILURE;
    }

    uint64_t bad = 0;
    /* N = 1<<16: each handoff is two futex syscalls; ~1s per iteration,
     * same granularity as the rest of the family */
    TEST_LOOP(test, 1 << 16) {
        /* hand the turn to the companion: bump its counter, wake it, then
         * sleep on ours until it bumps back (or 1ms timeout recovers us) */
        pp.seq_b++;
        if (futex_wake(&pp.seq_b, 1) < 0)
            bad++;
        uint32_t seen = pp.seq_a;
        long rc2 = futex_wait(&pp.seq_a, seen);
        if (rc2 != 0 && rc2 != -1)
            bad++;
        else if (rc2 == -1 && errno != ETIMEDOUT && errno != EAGAIN && errno != EWOULDBLOCK)
            bad++;
        if (pp.stop)  /* safety: never expected inside the loop */
            break;
    }

    /* release the companion and join it (no thread leak) */
    pp.stop = 1;
    pp.seq_b++;
    futex_wake(&pp.seq_b, 1);
    pthread_join(companion, nullptr);

    if (bad != 0 || pp.companion_bad != 0) {
        report_fail_msg("sched_storm_hybrid: %llu unexpected futex returns (main %llu, companion %llu)",
                        (unsigned long long)(bad + pp.companion_bad),
                        (unsigned long long)bad, (unsigned long long)pp.companion_bad);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(sched_storm_hybrid, "Scheduler-storm stimulus C: two threads pinned to one core doing alternating futex wait/wake ping-pong (1ms-timeout seq protocol, lost-wakeup-proof); every handoff = one sleep (newidle opportunity) + one wake-pullback (load_balance); kernel errors observable via journalctl/vmcore")
    .test_run     = sched_storm_hybrid_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
