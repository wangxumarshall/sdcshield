/*  spinlock_stale_read_xchg — differential arm of the stale-read probe.
 *
 *  Identical probe structure to spinlock_stale_read, but the lock is
 *  acquired with an UNCONDITIONAL exchange (std::atomic::exchange ->
 *  ARM64 LSE SWPAL).  The exchange does not compare-then-act: whenever
 *  the lock is actually free at the RMW, the swap succeeds regardless of
 *  any stale plain read.  Comparing the two arms on suspect hardware
 *  localises the fault layer:
 *      CAS arm fires, xchg arm clean  -> compare path / CAS-read specific
 *      both arms fire                  -> shared cause (plain-load path,
 *                                          atomic unit, or coherence)
 *
 *  Probe: after every failed exchange (returned old != 0), a relaxed
 *  plain load of the same address.  Fail criteria are the same three:
 *  (1) any observed lock value (exchange-returned or plain) not in {0,1}
 *      -> value corruption (hard, no benign explanation)
 *  (2) >= STALE_CONSEC_THRESHOLD consecutive [exchange returned held +
 *      plain load saw free] without acquiring -> spinning on a FREE lock
 *  (3) cleanup: counter != sum(local_sums) -> lost update
 *  Hot loop: no I/O, no clock, no libc random.  Stats reported cold.
 */
#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>

#define MAX_THREADS 1024
#define STALE_CONSEC_THRESHOLD 4096
#define STALE_REPORT_FLOOR 128    /* telemetry gate: benign worst observed 79 @ 191 threads x 2min */
/* Spin batching (2026-10-30 fix): see spinlock_stale_read.cpp — the
 * framework fsyncs the -o log on every test_time_condition() call, so a
 * one-CAS-per-call loop is disk-throttled to ~0.3% duty on ext4 (tmpfs
 * smokes miss it).  Batch 2^25 spins per framework call. */
#define SPIN_BATCH (1ULL << 25)

struct alignas(64) ThreadStats {
    uint64_t acquisitions;
    uint64_t stale_events;
    uint64_t max_consec_stale;
    uint64_t local_sum;
    uint64_t last_atomic_observed;  // forensics: last exchange-returned value
    uint64_t last_plain_observed;   // forensics: last plain-load value
};

struct SharedData {
    alignas(64) std::atomic<uint64_t> lock;   // 0 = free, 1 = held
    alignas(64) uint64_t counter;             // modified only under lock
    std::atomic<uint64_t> thread_idx;
    ThreadStats stats[MAX_THREADS];
};

static int spinlock_stale_read_xchg_init(struct test *test) {
    auto *sd = new SharedData;
    if (!sd) return EXIT_FAILURE;
    sd->lock = 0;
    sd->counter = 0;
    sd->thread_idx.store(0, std::memory_order_relaxed);
    memset(sd->stats, 0, sizeof(sd->stats));
    test->data = sd;
    return EXIT_SUCCESS;
}

static int spinlock_stale_read_xchg_run(struct test *test, int cpu) {
    (void)cpu;
    auto *sd = static_cast<SharedData *>(test->data);

    uint64_t id = sd->thread_idx.fetch_add(1, std::memory_order_relaxed);
    if (id >= MAX_THREADS) {
        report_fail_msg("spinlock_stale_read_xchg: more than %d threads", MAX_THREADS);
        return EXIT_FAILURE;
    }
    ThreadStats *st = &sd->stats[id];

    /* private LCG — keeps the hot loop free of libc / I/O */
    uint64_t rng = 0x9E3779B97F4A7C15ULL * (id + 1) + 0x89ABCDEFULL;
    uint64_t consec = 0;

    do {
      for (uint64_t batch = 0; batch < SPIN_BATCH; ++batch) {
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        uint64_t inc = (rng >> 48) + 1;

        uint64_t old = sd->lock.exchange(1, std::memory_order_acquire);
        st->last_atomic_observed = old;
        if (old > 1) {
            report_fail_msg("spinlock_stale_read_xchg: thread %lu exchange "
                            "returned illegal lock value 0x%llx "
                            "(lock word only ever holds 0/1)",
                            (unsigned long)id, (unsigned long long)old);
            return EXIT_FAILURE;
        }
        if (old == 0) {
            /* acquired: exchange found the lock free and wrote 1 */
            sd->counter += inc;
            sd->lock.store(0, std::memory_order_release);
            st->local_sum += inc;
            st->acquisitions++;
            consec = 0;
        } else {
            /* exchange saw the lock held (returned 1): probe with a plain
             * load of the same address, kept adjacent by a compiler barrier */
            asm volatile ("" ::: "memory");
            uint64_t plain = sd->lock.load(std::memory_order_relaxed);
            st->last_plain_observed = plain;
            if (plain > 1) {
                report_fail_msg("spinlock_stale_read_xchg: thread %lu plain "
                                "load observed illegal lock value 0x%llx "
                                "(lock word only ever holds 0/1)",
                                (unsigned long)id, (unsigned long long)plain);
                return EXIT_FAILURE;
            }
            if (plain == 0) {
                /* exchange returned held, plain load sees free */
                consec++;
                st->stale_events++;
                if (consec > st->max_consec_stale)
                    st->max_consec_stale = consec;
                if (consec >= STALE_CONSEC_THRESHOLD) {
                    report_fail_msg("spinlock_stale_read_xchg: thread %lu spun "
                                    "on a FREE lock: %llu consecutive "
                                    "[exchange=held, plain=free] observations "
                                    "without acquiring (last exchange=0x%llx "
                                    "plain=0x%llx, acquisitions=%llu)",
                                    (unsigned long)id,
                                    (unsigned long long)consec,
                                    (unsigned long long)old,
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

static int spinlock_stale_read_xchg_cleanup(struct test *test) {
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

    /* Telemetry only (see spinlock_stale_read.cpp): per-instance, gated. */
    if (worst_consec >= STALE_REPORT_FLOOR) {
        fprintf(stderr, "spinlock_stale_read_xchg: stale streak %lu "
                "(thread %lu): threads=%lu acquisitions=%lu stale_events=%lu "
                "counter=%lu local_sum=%lu\n",
                (unsigned long)worst_consec, (unsigned long)worst_thread,
                (unsigned long)thread_count, (unsigned long)total_acq,
                (unsigned long)total_stale, (unsigned long)sd->counter,
                (unsigned long)total_local);
        fflush(stderr);
    }

    if (!data_ok) {
        report_fail_msg("spinlock_stale_read_xchg: lost update - counter %lu "
                        "!= sum(local) %lu",
                        (unsigned long)sd->counter, (unsigned long)total_local);
        delete sd;
        return EXIT_FAILURE;
    }

    delete sd;
    return EXIT_SUCCESS;
}

DECLARE_TEST(spinlock_stale_read_xchg, "Stale-read probe xchg arm: acquire via unconditional exchange (immune to compare-path reads) - differential vs spinlock_stale_read (ARM64 atomic)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = spinlock_stale_read_xchg_init,
    .test_run = spinlock_stale_read_xchg_run,
    .test_cleanup = spinlock_stale_read_xchg_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
