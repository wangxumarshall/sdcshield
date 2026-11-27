/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b db_load_addr_zero_probe
 * @parblock
 * Zero-idiom address-pollution probe (2026-11-27), companion of
 * db_load_addr_capture. The fault class: the load's address register
 * suddenly reads as ZERO. Prime suspect is the zero-idiom / register
 * zeroing path — an `eor x1, x1, x1` (or equivalent move-elimination
 * zero form) executed shortly before the address register is written
 * can, on a defective RF/bypass implementation, leave the zero stuck
 * over the subsequent address write: the write is lost and the load
 * dereferences base+0 (after clamping) — i.e. the field's "reads from
 * address 0".
 *
 * Differential construction: iterations alternate between
 *   A (plain window)      add x1, base, off; ldr q0, [x1]
 *   B (zero-prefixed)     eor x1, x1, x1; add x1, base, off; ldr q0, [x1]
 * so a failure appearing ONLY on B iterations pins the zeroing path;
 * a failure on A too means the plain capture window is also sick
 * (see db_load_addr_capture). Each window is pinned in a 2-4 line asm
 * block — the zero-gap shape is the point of the probe and cannot be
 * expressed in C++; everything else is plain C++.
 *
 * Same self-describing tag region as db_load_addr_capture: the readback
 * decodes to the slot actually visited. Failure triage for B-class:
 *   tag decodes to slot 0  -> zero stuck over the address write
 *   tag decodes to idx-1   -> stale previous-address capture
 *   anything else          -> wrong-value capture
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

constexpr uint64_t DB_LZP_SLOTS = 1ull << 10;
constexpr uint64_t DB_LZP_SLOT_MASK = DB_LZP_SLOTS - 1;
constexpr uint64_t DB_LZP_REGION_BYTES = DB_LZP_SLOTS * 16;

struct DbLoadAddrZeroProbeData {
    uint64x2_t *tags = nullptr;
    uint64_t *exp_lo  = nullptr;
    uint64_t *exp_hi  = nullptr;
};

} // namespace

static int db_load_addr_zero_probe_init(struct test *test)
{
    auto *data = static_cast<DbLoadAddrZeroProbeData *>(calloc(1, sizeof(DbLoadAddrZeroProbeData)));
    if (!data)
        return -ENOMEM;

    data->tags = static_cast<uint64x2_t *>(aligned_alloc(64, DB_LZP_REGION_BYTES));
    data->exp_lo = static_cast<uint64_t *>(calloc(DB_LZP_SLOTS, sizeof(uint64_t)));
    data->exp_hi = static_cast<uint64_t *>(calloc(DB_LZP_SLOTS, sizeof(uint64_t)));
    if (!data->tags || !data->exp_lo || !data->exp_hi) {
        test->data = data;
        return -ENOMEM;
    }

    const uint64_t magic  = random64() & 0xFFFFull;
    const uint64_t magic2 = random64() & 0xFFFFull;
    const uint64_t gen    = random64() & 0xFFFFull;
    const uint64_t gen2   = random64() & 0xFFFFull;

    for (uint64_t s = 0; s < DB_LZP_SLOTS; ++s) {
        const uint64_t lo = (magic << 48) | (s << 16) | gen;
        const uint64_t hi = (magic2 << 48) | ((s ^ gen) << 16) | gen2;
        data->tags[s] = uint64x2_t{lo, hi};
        data->exp_lo[s] = lo;
        data->exp_hi[s] = hi;
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int db_load_addr_zero_probe_run(struct test *test, int cpu)
{
    auto *data = static_cast<DbLoadAddrZeroProbeData *>(test->data);
    const uint64_t base = reinterpret_cast<uint64_t>(data->tags);

    uint64_t idx = static_cast<uint64_t>(cpu) & DB_LZP_SLOT_MASK;
    uint64_t iter = 0;

    TEST_LOOP(test, 1 << 6) {
        const uint64_t s = idx;
        const uint64_t off = s * 16;
        uint64x2_t got;

        if (iter & 1) {
            /* B: zero-idiom prefix, then the address write, then the
             * immediate load. If the zero sticks over the write, the
             * load visits slot 0 and the tag says so. */
            __asm__ volatile(
                "eor x1, x1, x1\n\t"
                "add x1, %[base], %[off]\n\t"
                "ldr %q0, [x1]\n\t"
                : "=w"(got)
                : [base] "r"(base), [off] "r"(off)
                : "x1", "memory");
        } else {
            /* A: plain window (differential control). */
            __asm__ volatile(
                "add x1, %[base], %[off]\n\t"
                "ldr %q0, [x1]\n\t"
                : "=w"(got)
                : [base] "r"(base), [off] "r"(off)
                : "x1", "memory");
        }

        const uint64_t lo = vgetq_lane_u64(got, 0);
        const uint64_t hi = vgetq_lane_u64(got, 1);
        if (lo != data->exp_lo[s] || hi != data->exp_hi[s]) {
            const uint64_t got_slot = (lo >> 16) & DB_LZP_SLOT_MASK;
            const uint64_t prev_slot = (s + DB_LZP_SLOTS - 1) & DB_LZP_SLOT_MASK;
            const bool zero_stuck = got_slot == 0 && s != 0;
            report_fail_msg("db_load_addr_zero_probe: %s-class iteration, slot %llu "
                            "expected but the load visited slot %llu (%s); readback "
                            "0x%016llx:0x%016llx, expected 0x%016llx:0x%016llx",
                            (iter & 1) ? "B(zero-prefixed)" : "A(plain)",
                            static_cast<unsigned long long>(s),
                            static_cast<unsigned long long>(got_slot),
                            zero_stuck
                                ? "ZERO stuck over the address write — zero-idiom pollution"
                                : (got_slot == prev_slot
                                       ? "STALE previous-address capture"
                                       : "WRONG-VALUE capture"),
                            static_cast<unsigned long long>(lo),
                            static_cast<unsigned long long>(hi),
                            static_cast<unsigned long long>(data->exp_lo[s]),
                            static_cast<unsigned long long>(data->exp_hi[s]));
        }

        idx = (idx + 1) & DB_LZP_SLOT_MASK;
        ++iter;
    }

    return EXIT_SUCCESS;
}

static int db_load_addr_zero_probe_cleanup(struct test *test)
{
    auto *data = static_cast<DbLoadAddrZeroProbeData *>(test->data);
    if (data) {
        free(data->tags);
        free(data->exp_lo);
        free(data->exp_hi);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(db_load_addr_zero_probe,
             "Database load-fault zero-idiom probe: alternating plain vs zero-prefixed (eor x1,x1,x1) address-write windows — if the register zeroing sticks over the address write, the load dereferences slot 0 and the self-describing tag names it; differential A/B pinpoints the zeroing path; 2-4 line pinned windows, everything else C++")
    .test_init    = db_load_addr_zero_probe_init,
    .test_run     = db_load_addr_zero_probe_run,
    .test_cleanup = db_load_addr_zero_probe_cleanup,
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
