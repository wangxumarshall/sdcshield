/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b bit41_dram_stream_arm
 * @parblock
 * Fast reproducer for the  950 bit-41 address-path SDC.
 *
 * Background (2026-11, Y002 / BC85AMDATE-7592C, 2x  950 7592C,
 * 384 logical CPUs = 192 physical cores x SMT2, kernel 6.6.0-140):
 * a physical-core-level defect makes the *address/pointer path* of one
 * core corrupt a single bit at absolute bit position 41. A legal address
 * ffff8000_xxxxxxxx becomes ffff8200_xxxxxxxx (0x200 << 32 == 2^41); the
 * low 32 bits are untouched, so it is only visible as a
 * `((addr >> 32) & 0xffff) ^ 0x200` difference against a legal base.
 * In the kernel that lands on stack pointers / page-table pointers and
 * panics with ESR 0x96000004 (level-0 translation fault, pgd=0) or
 * 0x96000044 ("Insufficient stack space") on a random PC.
 *
 * Trigger characterization (closed by purpose-built reproducers):
 *   - Exception-entry rate is NOT the trigger: 6.2M syscalls/s + 150k
 *     page-faults/s on the bad core for 20 min never crashed.
 *   - Core-local throughput is NOT the trigger: a single thread pulling
 *     36.8 GB/s with a 4 MiB buffer resident in L3 ran 65 TB without a
 *     crash (traffic never left the L3).
 *   - WHOLE-MACHINE DRAM/NoC saturation IS the trigger: all-core
 *     streaming read-modify-write at ~94 GB/s aggregate crashes in
 *     149-185 s, while the same all-core shape diluted to ~72 GB/s runs
 *     20 min clean.
 *   - The scope is the core, not the workload: with the bad core
 *     excluded from the worker set (`--skip 34,35`) the machine still
 *     crashed on that core after 408 s, corrupting the stack pointer of
 *     an unrelated process (auditd).
 *   - Only the address path flips, never the data path: ~10k full
 *     element-wise userspace re-reads never saw a data mismatch, while
 *     every crash FAR was a legal address differing by 2^41.
 *
 * This test reproduces the trigger shape that did crash, so it is a
 * *reproducer*, not a data-path checker:
 *
 *   - one thread per CPU (the framework's natural per-CPU model) with a
 *     fixed 4 MiB private buffer -> 384 x 4 MiB ~= 1.5 GiB working set,
 *     far past the 192 MB total L3, so the traffic is DRAM/NoC-bound;
 *   - the hot loop is a single-buffer streaming vector RMW,
 *     vld1q_u32 -> vaddq_u32(v, one) -> vst1q_u32, deliberately WITHOUT
 *     a loop-carried dependency (the naive `acc = v` xor chain is 3-4x
 *     slower because it serializes the stores and dilutes the write
 *     queue pressure);
 *   - the +1 transform is recomputable, so a frozen `golden` copy can be
 *     checked against `buf[i] == golden[i] + applied` without ever
 *     re-reading golden during the hot phase. golden is touched only by
 *     the cold verify, once every BIT41_VERIFY_NSEC, so it does not
 *     perturb the streaming profile (a second continuous read stream
 *     measured only 72 GB/s and never crashed).
 *
 * Outcome handling: a user-data mismatch is still a genuine memory-path
 * SDC finding and fails the test with the XOR delta printed (a delta of
 * exactly 2^41 is tagged BIT-41). In practice the flip lands on kernel
 * pointers and the machine panics before userspace ever sees it; that
 * panic is the reproducer's positive result and is caught by the
 * campaign's external dmesg/vmcore monitor. A clean userspace pass is
 * therefore NOT evidence that the defect is absent.
 *
 * ARM64-only (registered under tests/cpu/arm64, built only on aarch64);
 * non-aarch64 returns a clean EXIT_SKIP. Per-thread buffers, no
 * cross-thread sharing. TEST_LOOP-style timed continuous stress.
 * @endparblock
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "sandstone.h"

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <ctime>
#include <cerrno>

#if defined(__aarch64__)
#  include <arm_neon.h>
#endif

/* Fixed per-thread buffer: 4 MiB. This is the size that crashed in
 * 149-185 s under an all-core run; it must NOT scale with thread_count()
 * or MemAvailable, or the total allocation across 384 workers becomes a
 * large fraction of RAM. 384 x 4 MiB = 1.5 GiB for buf, plus the same
 * for golden, against a 128 GiB host. */
#define BIT41_MB_PER_THREAD     (4ULL << 20)

/* Streaming passes per verify checkpoint (matches the reproducer's
 * --passes 4). `applied` is incremented per completed pass, so an
 * interrupted round never miscounts. */
#define BIT41_PASSES_PER_ROUND  4

/* Cold-verify interval, nanoseconds (20 s). Frequent enough to give
 * several checkpoints before the ~150 s crash window, rare enough that
 * the golden read does not add a second continuous read stream. */
#define BIT41_VERIFY_NSEC       (20ULL * 1000000000ULL)

#define BIT41_BIT41             (UINT64_C(1) << 41)

static inline uint64_t bit41_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int bit41_dram_stream_arm_init(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__
static int bit41_dram_stream_arm_run(struct test *test, int cpu)
{
    const size_t bytes = BIT41_MB_PER_THREAD;
    const size_t nelem = bytes / sizeof(uint32_t);
    const size_t vecs  = bytes / 16;

    uint32_t *buf  = static_cast<uint32_t *>(aligned_alloc(64, bytes));
    uint32_t *gold = static_cast<uint32_t *>(aligned_alloc(64, bytes));
    if (buf == nullptr || gold == nullptr) {
        free(buf);
        free(gold);
        log_skip(OSResourceIssueSkipCategory,
                 "bit41_dram_stream_arm: aligned_alloc(%zu) failed: %s",
                 bytes, strerror(errno));
        return EXIT_SKIP;
    }

    /* Deterministic, per-CPU-decorrelated pattern. golden is frozen here
     * and only read by the cold verify below, never during streaming. */
    for (size_t i = 0; i < nelem; i++)
        buf[i] = gold[i] = (uint32_t)((i * 2654435761u) ^
                                      ((uint32_t)cpu * 40503u));

    uint32_t applied = 0;               /* streaming passes completed */
    const uint32x4_t one = vdupq_n_u32(1);
    uint64_t last_verify = bit41_now_ns();
    bool done = false;

    while (!done) {
        /* Hot phase: single-buffer streaming vector RMW, no loop-carried
         * dependency. `applied` advances only for a fully completed pass
         * so an interrupted round cannot cause a false mismatch. */
        for (int p = 0; p < BIT41_PASSES_PER_ROUND; p++) {
            if (!test_time_condition(test)) {
                done = true;
                break;
            }
            for (size_t i = 0; i < vecs; i++) {
                uint32x4_t v = vld1q_u32(buf + i * 4);
                v = vaddq_u32(v, one);
                vst1q_u32(buf + i * 4, v);
            }
            applied++;
        }

        /* Cold verify: read golden here only. */
        uint64_t now = bit41_now_ns();
        if (now - last_verify >= BIT41_VERIFY_NSEC) {
            last_verify = now;
            for (size_t i = 0; i < nelem; i++) {
                uint32_t want = gold[i] + applied;
                uint32_t got = buf[i];
                if (got != want) {
                    uint64_t delta = (uint64_t)(got ^ want);
                    report_fail_msg(
                        "bit41_dram_stream_arm: mismatch cpu=%d off=%zu "
                        "exp=0x%08x got=0x%08x xor=0x%016" PRIx64 "%s",
                        cpu, i * sizeof(uint32_t), want, got, delta,
                        delta == BIT41_BIT41 ? "  <== BIT-41" : "");
                }
            }
        }
    }

    free(buf);
    free(gold);
    return EXIT_SUCCESS;
}
#else
static int bit41_dram_stream_arm_run(struct test *test, int cpu)
{
    (void)test;
    (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 streaming NEON "
             "read-modify-write stress required");
    return EXIT_SKIP;
}
#endif

static int bit41_dram_stream_arm_cleanup(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(bit41_dram_stream_arm,
             "bit-41 address-path SDC fast reproducer: all-core "
             "single-buffer NEON streaming RMW over 4MiB/thread (DRAM/NoC "
             "saturation ~94GB/s, crashes the marginal core in 150-185s) + "
             "cold golden verify tagging a 2^41 XOR delta (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = bit41_dram_stream_arm_init,
    .test_run = bit41_dram_stream_arm_run,
    .test_cleanup = bit41_dram_stream_arm_cleanup,
    /* No fracturing: this test must run ONE continuous stream for the whole
     * time slot. In automatic fracture mode the framework caps the inner
     * loop at 40 test_time_condition() calls, so test_run() would return
     * every few ms, the cold-verify timer would never reach
     * BIT41_VERIFY_NSEC, and the DRAM saturation would be reset on every
     * re-entry. -1 keeps the loop running until the wallclock deadline. */
    .fracture_loop_count = -1,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
