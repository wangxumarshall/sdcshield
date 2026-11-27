/*  barrier_after_lock_store_neon_2 — v2 of
 *  tests/cpu/neon_memcpy_cache/barrier_after_lock_store_neon.cpp (original untouched).
 *
 *  Three v2 changes over the original (2026-11-05):
 *   1. Bounded acquisition: the plain-LDR outer spin and the UNBOUNDED
 *      inner ldaxr spin of the original are replaced by a single
 *      try-lock loop with a spin budget — there is no unbounded spin
 *      path left in the acquire.
 *   2. Timeout snapshot + flow discrimination: on budget exhaustion the
 *      fail message carries (a) the lock word read coherently at the
 *      timeout instant and (b) the expected epoch counter vs what THIS
 *      thread last saw.  A single lock snapshot alone cannot separate a
 *      stuck lock from a transient window; the expected pair can:
 *        expected advanced past last-exp-seen => the lock HAS been freed
 *          and re-acquired since (transient starvation — environmental,
 *          not a stuck lock);
 *        expected frozen at last-exp-seen     => no holder completed a
 *          section since (lock genuinely stuck — holder-frozen or lost
 *          release; separating those two needs a holder heartbeat, a
 *          possible future revision).
 *   3. `yield` (ARM64 hint) in the spin loop throttles the hammer rate
 *      on the lock line.  Note the trade-off: every try arms the
 *      exclusive monitor (heavier per-access coherence than the
 *      original's plain-LDR outer spin), so this calms the loop by
 *      rate, not by per-access weight.
 *
 *  The lock protocol itself is unchanged and ARMv8-correct: LDAXR carries
 *  acquire semantics, the section stores precede dmb ish + plain str 0,
 *  every path that holds the lock unlocks it (the mismatch path unlocks
 *  before reporting).  report_fail_msg is noreturn in this framework
 *  (report_fail_common -> pthread_cancel, sandstone_run.cpp:189), so the
 *  explicit EXIT_FAILURE returns below it are unreachable belt-and-braces.
 *  BETA quality: experimental variant alongside the PROD original — not
 *  in the default set, run via -e.
 */
#include "sandstone.h"
#include <cstdint>
#include <atomic>
#include <arm_neon.h>

namespace { /* ODR: per-TU BALSNData */
struct BALSNData {
    alignas(64) uint64_t lock;              /* 0 = free, 1 = held (scalar protocol) */
    alignas(64) uint32_t protected_val[4];  /* the NEON handoff vector */
    alignas(64) std::atomic<uint64_t> expected;   /* advisory epoch mirror */
    std::atomic<uint64_t> acquisitions;
};
}

#define BALSN_SPIN_LIMIT 200000000ULL

static inline uint32x4_t ld_vec(const uint32_t *a) {
    uint32x4_t v;
    __asm__ volatile("ldr %q0, [%1]" : "=w"(v) : "r"(a) : "memory");
    return v;
}
static inline void st_vec(uint32_t *a, uint32x4_t v) {
    __asm__ volatile("str %q0, [%1]" :: "w"(v), "r"(a) : "memory");
}

/* One bounded try: returns 0 = acquired, 1 = busy or stxr lost the race */
static inline int balsn_try_lock(uint64_t *lock) {
    uint64_t tmp, res;
    __asm__ volatile(
        "   ldaxr  %0, [%2]\n\t"
        "   cbnz   %0, 1f\n\t"
        "   stxr   %w1, %3, [%2]\n\t"
        "   b      2f\n\t"
        "1: mov    %w1, #1\n\t"
        "2:\n\t"
        : "=&r"(tmp), "=&r"(res)
        : "r"(lock), "r"(1UL)
        : "memory");
    return res;
}

/* Bounded acquisition; on timeout writes the coherent lock-word snapshot */
static inline bool balsn_lock_timeout(uint64_t *lock, uint64_t spin_limit,
                                      uint64_t *snapshot) {
    uint64_t spins = 0;
    while (balsn_try_lock(lock) != 0) {
        if (++spins >= spin_limit) {
            /* coherent read (no cache bypass exists on a coherent system) */
            *snapshot = __atomic_load_n(lock, __ATOMIC_SEQ_CST);
            return false;
        }
        /* hint: throttle the spin rate on the lock line */
        __asm__ volatile("yield" ::: "memory");
    }
    return true;
}

/* release: barrier THEN free (section stores visible before lock=0) */
static inline void balsn_unlock(uint64_t *lock) {
    __asm__ volatile("dmb ish" ::: "memory");
    __asm__ volatile("str %0, [%1]" :: "r"(0UL), "r"(lock) : "memory");
}

static int barrier_after_lock_store_neon_2_init(struct test *test) {
    auto *d = new BALSNData;
    if (!d) return EXIT_FAILURE;
    d->lock = 0;
    for (int l = 0; l < 4; ++l) d->protected_val[l] = 0;
    d->expected.store(0, std::memory_order_relaxed);
    d->acquisitions.store(0, std::memory_order_relaxed);
    test->data = d;
    return EXIT_SUCCESS;
}

static int barrier_after_lock_store_neon_2_cleanup(struct test *test) {
    delete static_cast<BALSNData *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int barrier_after_lock_store_neon_2_run(struct test *test, int cpu) {
    auto *d = static_cast<BALSNData *>(test->data);
    (void)cpu;

    /* flow discriminator: the last epoch value this thread completed */
    uint64_t last_exp = 0;

    do {
        uint64_t snapshot_val = 0;

        if (!balsn_lock_timeout(&d->lock, BALSN_SPIN_LIMIT, &snapshot_val)) {
            uint64_t expected_now = d->expected.load(std::memory_order_seq_cst);
            report_fail_msg("barrier_after_lock_store_neon_2: lock never "
                            "freed after %lu spins. snapshot lock=%lu, "
                            "expected=%lu (this thread last saw %lu): %s",
                            (unsigned long)BALSN_SPIN_LIMIT,
                            (unsigned long)snapshot_val,
                            (unsigned long)expected_now,
                            (unsigned long)last_exp,
                            expected_now != last_exp
                                ? "lock has flowed since (transient "
                                  "starvation, not stuck)"
                                : "no holder completed since this "
                                  "thread (lock stuck)");
            /* unreachable: report_fail_msg cancels this thread */
            return EXIT_FAILURE;
        }

        uint64_t exp = d->expected.load(std::memory_order_relaxed);
        uint32x4_t cur = ld_vec(d->protected_val);
        uint32x4_t want = vdupq_n_u32((uint32_t)exp);
        uint32x4_t diff = veorq_u32(cur, want);
        if (vmaxvq_u32(diff) != 0) {
            balsn_unlock(&d->lock);
            report_fail_msg("barrier_after_lock_store_neon_2: handoff vector "
                            "mismatch: expected epoch %lu in all lanes, got "
                            "{%u,%u,%u,%u} — NEON store lost inside the "
                            "lock+barrier path", (unsigned long)exp,
                            vgetq_lane_u32(cur, 0), vgetq_lane_u32(cur, 1),
                            vgetq_lane_u32(cur, 2), vgetq_lane_u32(cur, 3));
            /* unreachable: report_fail_msg cancels this thread */
            return EXIT_FAILURE;
        }

        /* section: store the next handoff epoch as a NEON vector */
        st_vec(d->protected_val, vdupq_n_u32((uint32_t)(exp + 1)));
        d->expected.store(exp + 1, std::memory_order_relaxed);
        d->acquisitions.fetch_add(1, std::memory_order_relaxed);
        last_exp = exp + 1;

        balsn_unlock(&d->lock);
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(barrier_after_lock_store_neon_2, "Write-loss inside the lock+barrier path on the NEON datapath: ldaxr/stxr lock, in-section asm vst epoch vector, dmb ish release — next holder must observe the epoch in all four lanes (ARM64, v2)")
    .test_init = barrier_after_lock_store_neon_2_init,
    .test_run = barrier_after_lock_store_neon_2_run,
    .test_cleanup = barrier_after_lock_store_neon_2_cleanup,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
