/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_rmw_diff
 * @parblock
 * Store-to-load FORWARDING-path isolation probe.
 *
 * Goal: confirm the 179 SDC lies in the store-to-load FORWARDING path itself
 * (not just "any str->ldr proximity").  We already have:
 *   - mrn_rmw_fwd (in mrn_rmw_fwd_vs_l1d.cpp): str res->temp[i], then ldr
 *     temp[i] (SAME address) -> reload served by store-buffer FORWARDING.
 *     Triggers SDC (~15 fail/10min).
 *
 * This file adds the matching control that keeps a tight str->ldr pair but
 * breaks FORWARDING by using a DIFFERENT load address:
 *   - mrn_rmw_diff (this file): str res->tempA[i], then ldr tempB[i] (DIFFERENT
 *     address).  tempB[i] is pre-filled in init with the SAME golden value
 *     that tempA[i] will receive, so the reload "should" read res — but it is
 *     served from the L1D ARRAY (tempB's line is resident), NOT from store-
 *     buffer forwarding (the str to tempA did not write tempB).  The str->ldr
 *     time gap and instruction sequence are as close to fwd's as we can keep
 *     while changing only the load address.
 *
 * Interpretation:
 *   - If SDC disappears here (0 fail) while mrn_rmw_fwd still triggers ->
 *     the defect is specifically in the FORWARDING path (same-address str->ldr
 *     served from the store buffer), NOT merely "str and ldr close together".
 *   - If SDC still triggers here -> the defect is NOT specific to forwarding;
 *     it's the str->ldr proximity / timing / load-port contention.
 *
 * Crucially, this probe changes the LOAD ADDRESS, not the time gap and not the
 * instruction count between str and ldr — so it isolates the forwarding
 * variable more cleanly than the nop (time) or evict (path+time) probes did.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_RMW_COUNT 1024

struct MrnRmwDiffData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *expected;   // golden = srcA op srcB (what res "should" be)
    uint64_t *tempA;      // str target (write res here)
    uint64_t *tempB;      // ldr source (pre-filled with expected; different addr)
};

#if defined(__aarch64__)
static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("str %1, [%0]" : : "r"(addr), "r"(val) : "memory");
}
static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=r"(res) : "r"(addr) : "memory");
    return res;
}
#endif

static int mrn_rmw_diff_init(struct test *test) {
    auto *data = static_cast<MrnRmwDiffData *>(malloc(sizeof(MrnRmwDiffData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->tempA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->tempB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_RMW_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_RMW_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_RMW_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] - data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
        }
        // Pre-fill tempB with the golden value, so an L1D-array reload of
        // tempB[i] "should" read expected[i] (== res that tempA[i] receives).
        data->tempB[i] = data->expected[i];
        data->tempA[i] = 0;  // will be overwritten by str each iteration
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_rmw_diff_cleanup(struct test *test) {
    auto *data = static_cast<MrnRmwDiffData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data->tempA);
        free(data->tempB);
        free(data);
    }
    return EXIT_SUCCESS;
}

static int mrn_rmw_diff_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<MrnRmwDiffData *>(test->data);

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_RMW_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res;
            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a - b; break;
                case 2: res = a ^ b; break;
                default: res = a & b; break;
            }

            // str res to tempA[i], then ldr from tempB[i] (DIFFERENT address).
            // tempB[i] holds expected[i] (==res), resident in L1D since init.
            // Reload served from L1D ARRAY, NOT store-buffer forwarding
            // (the str to tempA cannot forward to a ldr of tempB).
            store_qword(&data->tempA[i], res);
            uint64_t got = load_qword(&data->tempB[i]);

            // Verify: the ldr should read expected[i] (the value pre-stored
            // in tempB).  A mismatch means the L1D-array reload path erred.
            if (got != data->expected[i]) {
                uint64_t golden = data->expected[i];
                uint64_t actual = got;
                uint64_t xorv = golden ^ actual;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                const char *op = (i % 4 == 0) ? "add"
                               : (i % 4 == 1) ? "sub"
                               : (i % 4 == 2) ? "xor" : "and";
                log_error("mrn_rmw_diff: miscompare at index %u (op=%s): "
                          "golden=0x%016lX actual=0x%016lX xor=0x%016lX | "
                          "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                          "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                          (unsigned)i, op,
                          (unsigned long)golden, (unsigned long)actual,
                          (unsigned long)xorv,
                          gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                          ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                          xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                report_fail_msg("mrn_rmw_diff: data miscompare at index %u (op=%s)", (unsigned)i, op);
            }
        }
    }

    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_diff, "mrn_rmw forward-isolation probe: str to tempA then ldr from DIFFERENT addr tempB (L1D-array, not forwarding) — isolates forwarding path")
    .test_init = mrn_rmw_diff_init,
    .test_run = mrn_rmw_diff_run,
    .test_cleanup = mrn_rmw_diff_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
