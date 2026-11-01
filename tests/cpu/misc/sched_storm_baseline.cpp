/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sched_storm_baseline
 * @parblock
 * Scheduler-activity stimulus (2026-09-12), variant D: NEGATIVE CONTROL.
 *
 * vmcore evidence chain (docs/CORE179_PANIC_ROOT_CAUSE_SCHEDULER_FINDING_20260912.md)
 * attributes the 11 core-179 fatal panics to the scheduling path itself
 * (newidle_balance -> load_balance -> find_busiest_group reading per-CPU
 * runqueue stats), NOT to any compute payload: the victims were 8 different
 * processes with nothing in common except sleeping/exiting on 179.
 *
 * This variant is the falsification leg of that claim: the thread does
 * NOTHING but spin on ALU work -- no sleep, no exit, no futex, no thread
 * creation.  A spinning task never leaves the CPU, so newidle_balance never
 * runs for it.  If the scheduler-activity root cause is right, running this
 * on all 192 cores (179 included) for 30 minutes must NOT produce a
 * find_busiest_group Oops / vmcore, even though it burns the same core and
 * the same wall-clock as the stimulus variants.
 *
 * If this baseline DOES crash on 179 the same way, the root-cause claim is
 * refuted (the trigger would have to be something a pure spin also does --
 * e.g. timer interrupts alone) and the finding must be rolled back.
 *
 * The spin body is deliberately memory-free (register-only dependency
 * chain): the goal is to be busy WITHOUT touching the load path or the
 * scheduler, isolating "runnable task present" as the only condition.
 *
 * A trivial checksum keeps the loop from being folded away and gives the
 * test something to verify (result: pass) -- this is NOT an SDC probe; a
 * mismatch here would be the known user-space SDC, a different phenomenon.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>

static int sched_storm_baseline_run(struct test *test, int cpu) {
    uint64_t x = 0x9e3779b97f4a7c15ULL + static_cast<uint64_t>(cpu);
    uint64_t checksum = 0;

    /* N = 1<<16 with 64 ALU steps inside: ~0.7ms per iteration on a
     * 2.6GHz core -- fine-grained like the rest of the family */
    TEST_LOOP(test, 1 << 16) {
        /* pure register ALU chain: no loads, no stores, no syscalls,
         * no sleeping -- the task stays runnable forever.  The asm
         * volatile "+r" barriers force the chain to be materialized
         * (a plain checksum compare gets DCE'd -- verified in objdump,
         * the first build folded the entire body away). */
        for (int i = 0; i < 64; i++) {
            x = x * 6364136223846793005ULL + 1442695040888963407ULL;
            checksum ^= x >> 31;
        }
        __asm__ volatile ("" : "+r"(x), "+r"(checksum));
    }

    /* checksum is live-out via the asm barrier above; the "verification"
     * is intentionally vacuous -- this is NOT an SDC probe */
    (void)checksum;
    return EXIT_SUCCESS;
}

DECLARE_TEST(sched_storm_baseline, "Scheduler-storm NEGATIVE CONTROL: pure register-ALU spin, no sleep/exit/syscall -- the task never leaves the CPU so newidle_balance never runs; if this crashes on 179 like the stimulus variants do, the scheduler-activity root cause is refuted")
    .test_run     = sched_storm_baseline_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
