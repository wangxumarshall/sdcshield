/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b db_load_addr_gap_sweep
 * @parblock
 * Address-capture window TIMING sweep (2026-11-27), companion of
 * db_load_addr_capture and db_load_addr_zero_probe. Those two hold the
 * write->read gap at zero (or add a zeroing prefix); this one slides the
 * gap through eight nop widths — N in {0,1,2,4,8,16,32,64} — between the
 * address register write and the load that consumes it:
 *
 *     add x1, base, off
 *     nop x N
 *     ldr q0, [x1]
 *
 * Diagnostic value: if the capture failure has a timing window, the
 * failure signature changes with N — failing only at small N means the
 * bypass-forwarding path is sick (the RF read has not yet seen the
 * written value); failing only at large N means the opposite (the
 * written value fails to commit and the RF serves something stale or
 * zero). A flat failure across all N points at a value-corruption path
 * rather than a timing race. Each width is pinned in its own asm block
 * (the nop count is the experiment variable and must not be reordered
 * away or merged); everything else is plain C++.
 *
 * Same self-describing tag region as the companion probes: readback
 * decodes to the slot actually visited, failure message carries the
 * nop width that produced it.
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

constexpr uint64_t DB_LGS_SLOTS = 1ull << 10;
constexpr uint64_t DB_LGS_SLOT_MASK = DB_LGS_SLOTS - 1;
constexpr uint64_t DB_LGS_REGION_BYTES = DB_LGS_SLOTS * 16;

/* The window variable: nop count between the address write and the load. */
constexpr uint64_t DB_LGS_GAPS[8] = {0, 1, 2, 4, 8, 16, 32, 64};

struct DbLoadAddrGapSweepData {
    uint64x2_t *tags = nullptr;
    uint64_t *exp_lo  = nullptr;
    uint64_t *exp_hi  = nullptr;
};

} // namespace

static int db_load_addr_gap_sweep_init(struct test *test)
{
    auto *data = static_cast<DbLoadAddrGapSweepData *>(calloc(1, sizeof(DbLoadAddrGapSweepData)));
    if (!data)
        return -ENOMEM;

    data->tags = static_cast<uint64x2_t *>(aligned_alloc(64, DB_LGS_REGION_BYTES));
    data->exp_lo = static_cast<uint64_t *>(calloc(DB_LGS_SLOTS, sizeof(uint64_t)));
    data->exp_hi = static_cast<uint64_t *>(calloc(DB_LGS_SLOTS, sizeof(uint64_t)));
    if (!data->tags || !data->exp_lo || !data->exp_hi) {
        test->data = data;
        return -ENOMEM;
    }

    const uint64_t magic  = random64() & 0xFFFFull;
    const uint64_t magic2 = random64() & 0xFFFFull;
    const uint64_t gen    = random64() & 0xFFFFull;
    const uint64_t gen2   = random64() & 0xFFFFull;

    for (uint64_t s = 0; s < DB_LGS_SLOTS; ++s) {
        const uint64_t lo = (magic << 48) | (s << 16) | gen;
        const uint64_t hi = (magic2 << 48) | ((s ^ gen) << 16) | gen2;
        data->tags[s] = uint64x2_t{lo, hi};
        data->exp_lo[s] = lo;
        data->exp_hi[s] = hi;
    }

    test->data = data;
    return EXIT_SUCCESS;
}

#define DB_LGS_GAP_ASM(n)                                                  \
    __asm__ volatile(                                                      \
        "add x1, %[base], %[off]\n\t"                                      \
        ".rept " #n "\n\tnop\n\t.endr\n\t"                                  \
        "ldr %q0, [x1]\n\t"                                                 \
        : "=w"(got)                                                         \
        : [base] "r"(base), [off] "r"(off)                                 \
        : "x1", "memory")

static int db_load_addr_gap_sweep_run(struct test *test, int cpu)
{
    auto *data = static_cast<DbLoadAddrGapSweepData *>(test->data);
    const uint64_t base = reinterpret_cast<uint64_t>(data->tags);

    uint64_t idx = static_cast<uint64_t>(cpu) & DB_LGS_SLOT_MASK;
    uint64_t iter = 0;

    TEST_LOOP(test, 1 << 6) {
        /* Slot stride 3 (coprime with both 1024 and the 8-wide gap
         * rotation) so a slot-dependent fault cannot masquerade as a
         * gap-dependent one: each slot visits all eight widths. */
        const uint64_t s = idx;
        idx = (idx + 3) & DB_LGS_SLOT_MASK;
        const uint64_t off = s * 16;
        uint64x2_t got;

        /* Rotate through the eight nop widths: the width is the
         * experiment variable, reported with any failure. */
        switch (DB_LGS_GAPS[iter & 7]) {
        case 0:  DB_LGS_GAP_ASM(0);  break;
        case 1:  DB_LGS_GAP_ASM(1);  break;
        case 2:  DB_LGS_GAP_ASM(2);  break;
        case 4:  DB_LGS_GAP_ASM(4);  break;
        case 8:  DB_LGS_GAP_ASM(8);  break;
        case 16: DB_LGS_GAP_ASM(16); break;
        case 32: DB_LGS_GAP_ASM(32); break;
        default: DB_LGS_GAP_ASM(64); break;
        }

        const uint64_t lo = vgetq_lane_u64(got, 0);
        const uint64_t hi = vgetq_lane_u64(got, 1);
        if (lo != data->exp_lo[s] || hi != data->exp_hi[s]) {
            const uint64_t got_slot = (lo >> 16) & DB_LGS_SLOT_MASK;
            /* Previous-visit tracking with stride-3 slots: the literal
             * previous slot is (s+1024-3)&1023; stale first (see
             * db_load_addr_zero_probe), zero is the residual verdict. */
            const uint64_t prev_slot = (s + DB_LGS_SLOTS - 3) & DB_LGS_SLOT_MASK;
            const uint64_t gap = DB_LGS_GAPS[iter & 7];
            const bool stale = got_slot == prev_slot;
            const bool zero_visit = got_slot == 0 && s != 0 && !stale;
            report_fail_msg("db_load_addr_gap_sweep: nop-width %llu window, slot %llu "
                            "expected but the load visited slot %llu (%s); readback "
                            "0x%016llx:0x%016llx, expected 0x%016llx:0x%016llx",
                            static_cast<unsigned long long>(gap),
                            static_cast<unsigned long long>(s),
                            static_cast<unsigned long long>(got_slot),
                            stale
                                ? "STALE previous-address capture"
                                : (zero_visit
                                       ? "slot-0 visit (offset-operand zero capture)"
                                       : "WRONG-VALUE capture"),
                            static_cast<unsigned long long>(lo),
                            static_cast<unsigned long long>(hi),
                            static_cast<unsigned long long>(data->exp_lo[s]),
                            static_cast<unsigned long long>(data->exp_hi[s]));
        }

        ++iter;
    }

    return EXIT_SUCCESS;
}
#undef DB_LGS_GAP_ASM

static int db_load_addr_gap_sweep_cleanup(struct test *test)
{
    auto *data = static_cast<DbLoadAddrGapSweepData *>(test->data);
    if (data) {
        free(data->tags);
        free(data->exp_lo);
        free(data->exp_hi);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(db_load_addr_gap_sweep,
             "Database load-fault capture-window timing sweep: eight pinned nop widths (0..64) between the address-register write and the consuming ldr — failure signature vs nop-count localizes the capture window (small-N-only = bypass forwarding sick, large-N-only = write not committing, flat = value corruption); self-describing tags name the slot actually visited; failure message carries the nop width")
    .test_init    = db_load_addr_gap_sweep_init,
    .test_run     = db_load_addr_gap_sweep_run,
    .test_cleanup = db_load_addr_gap_sweep_cleanup,
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
