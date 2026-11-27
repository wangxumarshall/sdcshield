/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b db_load_addr_capture
 * @parblock
 * Back-to-back address-operand capture probe (2026-11-27).
 *
 * The target fault class, as re-specified by the field report: a load
 * consumes an ADDRESS register that was just written, and the register
 * read suddenly returns the WRONG value, the PREVIOUS value, or ZERO —
 * i.e. the address operand capture (RF read port / bypass network) is
 * what fails, not the loaded data. This is the load-address mirror of
 * the 179 store-data-register fault (rowmajor_fp64_dump family).
 *
 * Design: a 16 KiB L1-resident tag region, 1024 slots of 16 bytes.
 * Every slot carries a self-describing tag — lane0 =
 * (MAGIC<<48)|(slot<<16)|GEN, lane1 = (MAGIC2<<48)|((slot^GEN)<<16)|GEN2
 * — so on a miscompare the READ-BACK VALUE ITSELF decodes to the slot
 * the load actually visited. Per iteration the address register is
 * freshly written from the running index and consumed immediately:
 *
 *     add x1, x_base, x_off     // address register just written
 *     ldr q0, [x1]              // consumed with zero delay-slot gap
 *
 * The two-instruction window is pinned in one asm block because C++
 * cannot guarantee the zero-gap shape (the compiler may schedule the
 * load away from the address write — the whole point of this probe).
 * Everything else — tag generation, checking, triage — is plain C++.
 * No writer exists during run, so any stale capture means x1 read the
 * PREVIOUS iteration's offset: the load visits slot (idx-1), whose tag
 * says so directly. Triage on failure:
 *   tag decodes to slot idx-1      -> stale previous-address capture
 *   readback all-zero             -> the load landed on a zero-filled page
 *                                     (an address-register ZERO capture that
 *                                     did NOT fault; a full absolute-zero
 *                                     capture would crash the worker instead,
 *                                     which the framework also reports)
 *   any other slot                -> wrong-value address
 *
 * The region deliberately stays inside L1 (no TLB/cache noise): this
 * probe isolates the capture window, unlike the earlier db_load_*
 * three (pointer_chase/stale_data_probe/mixed_stress) whose address
 * paths derive from memory contents or plain computation and thus only
 * weakly exercise this window — recorded in the test-matrix §10.1.
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

constexpr uint64_t DB_LAC_SLOTS = 1ull << 10;      /* 1024 x 16B = 16 KiB, L1-resident */
constexpr uint64_t DB_LAC_SLOT_MASK = DB_LAC_SLOTS - 1;
constexpr uint64_t DB_LAC_REGION_BYTES = DB_LAC_SLOTS * 16;

struct DbLoadAddrCaptureData {
    uint64x2_t *tags  = nullptr;  /* self-describing tag region, run-readonly */
    uint64_t *exp_lo   = nullptr; /* expected lane0 per slot */
    uint64_t *exp_hi   = nullptr; /* expected lane1 per slot */
};

} // namespace

static int db_load_addr_capture_init(struct test *test)
{
    auto *data = static_cast<DbLoadAddrCaptureData *>(calloc(1, sizeof(DbLoadAddrCaptureData)));
    if (!data)
        return -ENOMEM;

    data->tags = static_cast<uint64x2_t *>(aligned_alloc(64, DB_LAC_REGION_BYTES));
    data->exp_lo = static_cast<uint64_t *>(calloc(DB_LAC_SLOTS, sizeof(uint64_t)));
    data->exp_hi = static_cast<uint64_t *>(calloc(DB_LAC_SLOTS, sizeof(uint64_t)));
    if (!data->tags || !data->exp_lo || !data->exp_hi) {
        test->data = data;
        return -ENOMEM;
    }

    /* Random tag material from the framework RNG, fixed for the whole run. */
    const uint64_t magic  = random64() & 0xFFFFull;
    const uint64_t magic2 = random64() & 0xFFFFull;
    const uint64_t gen    = random64() & 0xFFFFull;
    const uint64_t gen2   = random64() & 0xFFFFull;

    for (uint64_t s = 0; s < DB_LAC_SLOTS; ++s) {
        const uint64_t lo = (magic << 48) | (s << 16) | gen;
        const uint64_t hi = (magic2 << 48) | ((s ^ gen) << 16) | gen2;
        data->tags[s] = uint64x2_t{lo, hi};
        data->exp_lo[s] = lo;
        data->exp_hi[s] = hi;
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int db_load_addr_capture_run(struct test *test, int cpu)
{
    auto *data = static_cast<DbLoadAddrCaptureData *>(test->data);
    const uint64_t base = reinterpret_cast<uint64_t>(data->tags);

    uint64_t idx = static_cast<uint64_t>(cpu) & DB_LAC_SLOT_MASK;  /* per-worker phase */

    TEST_LOOP(test, 1 << 6) {
        const uint64_t s = idx;
        const uint64_t off = s * 16;
        uint64x2_t got;

        /* The zero-gap window: address register freshly written from the
         * running index, consumed by the very next instruction. Pinned —
         * the compiler must not schedule the ldr away from the add. */
        __asm__ volatile(
            "add x1, %[base], %[off]\n\t"
            "ldr %q0, [x1]\n\t"
            : "=w"(got)
            : [base] "r"(base), [off] "r"(off)
            : "x1", "memory");

        const uint64_t lo = vgetq_lane_u64(got, 0);
        const uint64_t hi = vgetq_lane_u64(got, 1);
        if (lo != data->exp_lo[s] || hi != data->exp_hi[s]) {
            /* The readback tag decodes to the slot the load ACTUALLY
             * visited — the address register capture failed, and this
             * names the address it carried instead. */
            const uint64_t got_slot = (lo >> 16) & DB_LAC_SLOT_MASK;
            const uint64_t prev_slot = (s + DB_LAC_SLOTS - 1) & DB_LAC_SLOT_MASK;
            const char *verdict;
            if (lo == 0 && hi == 0)
                verdict = "ALL-ZERO readback (load landed on a zero-filled page; a full "
                          "absolute-zero capture would crash instead)";
            else if (got_slot == prev_slot)
                verdict = "STALE capture (address register returned the PREVIOUS address)";
            else
                verdict = "WRONG-VALUE capture";
            report_fail_msg("db_load_addr_capture: slot %llu expected, but the load "
                            "visited slot %llu (%s); readback 0x%016llx:0x%016llx, "
                            "expected 0x%016llx:0x%016llx",
                            static_cast<unsigned long long>(s),
                            static_cast<unsigned long long>(got_slot),
                            verdict,
                            static_cast<unsigned long long>(lo),
                            static_cast<unsigned long long>(hi),
                            static_cast<unsigned long long>(data->exp_lo[s]),
                            static_cast<unsigned long long>(data->exp_hi[s]));
        }

        idx = (idx + 1) & DB_LAC_SLOT_MASK;
    }

    return EXIT_SUCCESS;
}

static int db_load_addr_capture_cleanup(struct test *test)
{
    auto *data = static_cast<DbLoadAddrCaptureData *>(test->data);
    if (data) {
        free(data->tags);
        free(data->exp_lo);
        free(data->exp_hi);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(db_load_addr_capture,
             "Database load-fault capture-window probe: address register freshly written (add) and consumed by the very next ldr — zero-gap window pinned in 2-line asm, everything else C++; self-describing slot tags let the readback itself name the address actually visited (stale-previous / zero / wrong-value triage); 16KiB L1-resident region isolates the capture window from cache/TLB noise")
    .test_init    = db_load_addr_capture_init,
    .test_run     = db_load_addr_capture_run,
    .test_cleanup = db_load_addr_capture_cleanup,
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
