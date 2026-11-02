/*  spinlock_stale_read — probe for the "threads spinning on a lock that is
 *  actually free" anomaly (user report from another machine, 2026-10-30),
 *  and for read-path value corruption of the lock word (the CORE179
 *  defect class: loads returning wrong/replaced data).
 *
 *  Probe design (read-consistency check, adapted from the CORE179
 *  same-address re-load recipe):
 *    acquire : CAS spinlock — compare_exchange_strong maps to ARM64 LSE
 *              CASAL; on failure `expected` receives the value the
 *              ATOMIC (RMW) path observed.
 *    probe   : immediately after every failed CAS, a relaxed load of the
 *              same address (plain LDR path), kept adjacent by a compiler
 *              barrier.
 *    fail if : (1) any read of the lock word (CAS-observed or plain) is
 *                  neither 0 nor 1  -> value corruption, no benign
 *                  explanation (the lock word only ever holds 0/1)
 *              (2) >= STALE_CONSEC_THRESHOLD consecutive
 *                  [atomic path saw held + plain path saw free] without
 *                  this thread acquiring once -> spinning on a FREE lock
 *              (3) cleanup: shared counter != sum of per-thread local
 *                  sums -> lost update (classic lock failure)
 *  Single [atomic=held, plain=free] events can be a legitimate release
 *  race and are only counted; an unbroken streak without acquisition is
 *  the signature of a stale read.  Hot loop has no I/O, no clock, no libc
 *  random — everything is reported from cold paths only.  Per-thread
 *  progress/staleness statistics are REPORTED (not asserted) at cleanup
 *  for offline rate analysis.
 */
#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>

#define MAX_THREADS 1024
#define STALE_CONSEC_THRESHOLD 4096
#define STALE_REPORT_FLOOR 128    /* telemetry gate: benign worst observed 79 @ 191 threads x 2min */
/* Spin batching (2026-10-30 fix): the framework logs+fsyncs the -o log
 * on EVERY test_time_condition() call.  A trivial-work inner loop (one
 * CAS per call) is disk-throttled to ~0.3% duty on ext4 (jbd2 commit
 * wait per spin); on tmpfs it is free, which is why the /tmp smoke
 * missed it.  Batch 2^25 spins (~0.3-0.7s of spinning) per framework
 * call. */
#define SPIN_BATCH (1ULL << 25)

struct alignas(64) ThreadStats {
    uint64_t acquisitions;
    uint64_t stale_events;          // single [atomic=held, plain=free] observations
    uint64_t max_consec_stale;
    uint64_t local_sum;
    uint64_t last_atomic_observed;  // forensics: last CAS-observed lock value
    uint64_t last_plain_observed;   // forensics: last plain-load lock value
};

struct SharedData {
    alignas(64) std::atomic<uint64_t> lock;   // 0 = free, 1 = held
    alignas(64) uint64_t counter;             // modified only under lock
    std::atomic<uint64_t> thread_idx;
    ThreadStats stats[MAX_THREADS];
};

static int spinlock_stale_read_init(struct test *test) {
    auto *sd = new SharedData;
    if (!sd) return EXIT_FAILURE;
    sd->lock = 0;
    sd->counter = 0;
    sd->thread_idx.store(0, std::memory_order_relaxed);
    memset(sd->stats, 0, sizeof(sd->stats));
    test->data = sd;
    return EXIT_SUCCESS;
}

static int spinlock_stale_read_run(struct test *test, int cpu) {
    (void)cpu;
    auto *sd = static_cast<SharedData *>(test->data);

    uint64_t id = sd->thread_idx.fetch_add(1, std::memory_order_relaxed);
    if (id >= MAX_THREADS) {
        report_fail_msg("spinlock_stale_read: more than %d threads", MAX_THREADS);
        return EXIT_FAILURE;
    }
    ThreadStats *st = &sd->stats[id];

    /* private LCG — keeps the hot loop free of libc / I/O */
    uint64_t rng = 0x9E3779B97F4A7C15ULL * (id + 1) + 0x1234567ULL;
    uint64_t consec = 0;

    do {
      for (uint64_t batch = 0; batch < SPIN_BATCH; ++batch) {
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        uint64_t inc = (rng >> 48) + 1;

        uint64_t expected = 0;
        if (sd->lock.compare_exchange_strong(expected, 1,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
            /* acquired */
            sd->counter += inc;
            sd->lock.store(0, std::memory_order_release);
            st->local_sum += inc;
            st->acquisitions++;
            consec = 0;
        } else {
            /* CAS failed: `expected` now holds the value the atomic (RMW)
             * path observed. Validate it. */
            st->last_atomic_observed = expected;
            if (expected > 1) {
                report_fail_msg("spinlock_stale_read: thread %lu atomic path "
                                "observed illegal lock value 0x%llx "
                                "(lock word only ever holds 0/1)",
                                (unsigned long)id, (unsigned long long)expected);
                return EXIT_FAILURE;
            }
            /* back-to-back plain load of the same address (plain LDR path);
             * compiler barrier keeps the two reads adjacent */
            asm volatile ("" ::: "memory");
            uint64_t plain = sd->lock.load(std::memory_order_relaxed);
            st->last_plain_observed = plain;
            if (plain > 1) {
                report_fail_msg("spinlock_stale_read: thread %lu plain load "
                                "observed illegal lock value 0x%llx "
                                "(lock word only ever holds 0/1)",
                                (unsigned long)id, (unsigned long long)plain);
                return EXIT_FAILURE;
            }
            if (plain == 0) {
                /* atomic path saw held, plain path saw free: stale-read
                 * candidate. Single events may be a legit release race;
                 * a long unbroken streak without acquiring cannot. */
                consec++;
                st->stale_events++;
                if (consec > st->max_consec_stale)
                    st->max_consec_stale = consec;
                if (consec >= STALE_CONSEC_THRESHOLD) {
                    report_fail_msg("spinlock_stale_read: thread %lu spun on "
                                    "a FREE lock: %llu consecutive "
                                    "[atomic=held, plain=free] observations "
                                    "without acquiring (last atomic=0x%llx "
                                    "plain=0x%llx, acquisitions=%llu)",
                                    (unsigned long)id,
                                    (unsigned long long)consec,
                                    (unsigned long long)expected,
                                    (unsigned long long)plain,
                                    (unsigned long long)st->acquisitions);
                    return EXIT_FAILURE;
                }
            } else {
                consec = 0;   /* legitimately held */
            }
        }
      }  /* spin batch */
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int spinlock_stale_read_cleanup(struct test *test) {
    auto *sd = static_cast<SharedData *>(test->data);

    uint64_t thread_count = sd->thread_idx.load(std::memory_order_relaxed);
    if (thread_count > MAX_THREADS) thread_count = MAX_THREADS;
    uint64_t total_local = 0, total_acq = 0, total_stale = 0;
    uint64_t worst_consec = 0, worst_thread = 0;
    for (uint64_t i = 0; i < thread_count; ++i) {
        total_local += sd->stats[i].local_sum;
        total_acq += sd->stats[i].acquisitions;
        total_stale += sd->stats[i].stale_events;
        if (sd->stats[i].max_consec_stale > worst_consec) {
            worst_consec = sd->stats[i].max_consec_stale;
            worst_thread = i;
        }
    }

    bool data_ok = (sd->counter == total_local);

    /* Telemetry only. The framework re-invokes init/run/cleanup once per
     * iteration (test->data is per-instance), so this fires per instance;
     * stderr is captured into the -o log per block. Print only when the
     * stale streak is deep enough to be interesting (benign worst observed
     * in the 191-thread smoke: 4). Hard failures live in the run loop. */
    if (worst_consec >= STALE_REPORT_FLOOR) {
        fprintf(stderr, "spinlock_stale_read: stale streak %lu (thread %lu): "
                "threads=%lu acquisitions=%lu stale_events=%lu counter=%lu "
                "local_sum=%lu\n",
                (unsigned long)worst_consec, (unsigned long)worst_thread,
                (unsigned long)thread_count, (unsigned long)total_acq,
                (unsigned long)total_stale, (unsigned long)sd->counter,
                (unsigned long)total_local);
        fflush(stderr);
    }

    if (!data_ok) {
        report_fail_msg("spinlock_stale_read: lost update - counter %lu != "
                        "sum(local) %lu",
                        (unsigned long)sd->counter, (unsigned long)total_local);
        delete sd;
        return EXIT_FAILURE;
    }

    delete sd;
    return EXIT_SUCCESS;
}

DECLARE_TEST(spinlock_stale_read, "Spinlock stale-read probe: spinning on a free lock (atomic-vs-plain-load back-to-back consistency) + lost-update check (ARM64 atomic)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = spinlock_stale_read_init,
    .test_run = spinlock_stale_read_run,
    .test_cleanup = spinlock_stale_read_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
