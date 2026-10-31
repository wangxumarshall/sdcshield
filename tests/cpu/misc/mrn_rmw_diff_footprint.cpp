/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_rmw_diff_64k
 * @test @b mrn_rmw_diff_1m
 * @parblock
 * Footprint scan probe for core-179 SDC — isolates whether the L1D-array
 * read path still triggers when the buffer reaches / exceeds L1D capacity.
 *
 * Context (see memory footprint-independent-of-l1d-size &
 * sdc-rootcause-is-hardware-not-timing): three SDC-triggering tests span
 * footprint 64B (mrn_rmw_fwd, ~1 cache line) .. 16KB (mrn_rmw_diff, 0.25x
 * L1D) .. 128KB (movbe_dump, 2x L1D). All trigger. This already shows
 * footprint SIZE is not the trigger. The open question this probe answers:
 *
 *   Does the L1D-array read path (different-address str->ldr, NOT forwarding)
 *   still trigger when the buffer is EXACTLY L1D-sized (4-way thrash edge)
 *   vs when it far exceeds L1D (every reload misses L1D, served from L2)?
 *
 * Both tests reuse mrn_rmw_diff's different-address design: str res->tempA[i],
 * then ldr from tempB[i] (DIFFERENT address). tempB is pre-filled with the
 * golden value. The reload CANNOT be served by store-buffer forwarding (the
 * str wrote tempA, not tempB), so it must come from the cache array. The
 * only variable across these two tests is the tempB buffer size (= how
 * much of L1D the reload stream touches).
 *
 *   - mrn_rmw_diff_64k: tempB = 64KB == L1D (64KB/4-way/64B). 1024 lines map
 *     to 256 sets x 4 ways -> each set gets exactly 4 lines -> the stream
 *     JUST fills L1D, sitting on the thrash edge. Reload is L1D-array read
 *     under capacity pressure.  If trigger rate ~= mrn_rmw_diff(16KB),
 *     capacity/thrash is NOT an amplifier.  If rate spikes, it IS.
 *   - mrn_rmw_diff_1m:  tempB = 1MB = 16x L1D.  Every tempB[i] reload misses
 *     L1D (no way the line is resident after a 1MB sweep), served from
 *     L2/L3 refill -> load port.  Data leaves L1D entirely.  If SDC
 *     disappears here (0 fail) while _64k triggers, the defect needs the
 *     load to HIT L1D array (path mechanism), not merely str->ldr timing
 *     (which is unchanged).  If SDC persists, it favors pure-timing.
 *
 * Hot loop byte-identical to mrn_rmw_diff (same str/ldr/ALU, same indices),
 * only MRN_RMW_DIFF_COUNT differs.  Miscompare logging in cold branch only
 * (see memory dump-variant-noremove-report-fail-verified: touching even the
 * cold branch once silenced the defect; we keep report_fail_msg as-is).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

// 64KB == L1D (64KB/4-way/64B): 1024 uint64 = 8192 B... no. 64KB = 8192 uint64.
// We want tempB to be EXACTLY 64KB so its line-stream maps 1:1 onto L1D sets.
#define MRN_RMW_DIFF_64K_COUNT  (8192)          // 8192 * 8B = 64KB == L1D
#define MRN_RMW_DIFF_1M_COUNT   (131072)        // 131072 * 8B = 1MB = 16x L1D

struct MrnRmwDiffFpData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *expected;   // golden = srcA op srcB
    uint64_t *tempA;      // str target (write res here)
    uint64_t *tempB;      // ldr source (pre-filled with expected; different addr)
    int count;
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

static int mrn_rmw_diff_fp_init_common(struct test *test, int count) {
    auto *data = static_cast<MrnRmwDiffFpData *>(malloc(sizeof(MrnRmwDiffFpData)));
    data->count = count;
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, count * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, count * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, count * sizeof(uint64_t)));
    data->tempA = static_cast<uint64_t *>(aligned_alloc(64, count * sizeof(uint64_t)));
    data->tempB = static_cast<uint64_t *>(aligned_alloc(64, count * sizeof(uint64_t)));

    memset_random(data->srcA, count * sizeof(uint64_t));
    memset_random(data->srcB, count * sizeof(uint64_t));

    for (int i = 0; i < count; i++) {
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

static int mrn_rmw_diff_64k_init(struct test *test) {
    return mrn_rmw_diff_fp_init_common(test, MRN_RMW_DIFF_64K_COUNT);
}
static int mrn_rmw_diff_1m_init(struct test *test) {
    return mrn_rmw_diff_fp_init_common(test, MRN_RMW_DIFF_1M_COUNT);
}

static int mrn_rmw_diff_fp_cleanup(struct test *test) {
    auto *data = static_cast<MrnRmwDiffFpData *>(test->data);
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

// Shared hot-loop runner. The loop body is byte-identical to mrn_rmw_diff:
// str res->tempA[i]; ldr from tempB[i] (DIFFERENT addr, L1D-array read, NOT
// forwarding); verify the reload reads expected[i].
static int mrn_rmw_diff_fp_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<MrnRmwDiffFpData *>(test->data);
    const char *tag = test->id;
    int count = data->count;

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < count; i++) {
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
            // tempB[i] holds expected[i]. Reload served from L1D ARRAY (or
            // further when the line isn't resident), NOT store-buffer
            // forwarding (the str to tempA cannot forward to a ldr of tempB).
            store_qword(&data->tempA[i], res);
            uint64_t got = load_qword(&data->tempB[i]);

            // Verify: the ldr should read expected[i].  Cold branch only.
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
                log_error("%s: miscompare at index %u (op=%s): "
                          "golden=0x%016lX actual=0x%016lX xor=0x%016lX | "
                          "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                          "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                          tag, (unsigned)i, op,
                          (unsigned long)golden, (unsigned long)actual,
                          (unsigned long)xorv,
                          gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                          ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                          xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                report_fail_msg("%s: data miscompare at index %u (op=%s)", tag, (unsigned)i, op);
            }
        }
    }

    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_diff_64k, "mrn_rmw footprint probe (L1D thrash edge): diff-addr str->ldr, tempB=64KB==L1D — does L1D-array read trigger at thrash edge?")
    .test_init = mrn_rmw_diff_64k_init,
    .test_run = mrn_rmw_diff_fp_run,
    .test_cleanup = mrn_rmw_diff_fp_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(mrn_rmw_diff_1m, "mrn_rmw footprint probe (L2-reload): diff-addr str->ldr, tempB=1MB=16xL1D — every reload misses L1D (served from L2). If SDC needs L1D hit, should vanish.")
    .test_init = mrn_rmw_diff_1m_init,
    .test_run = mrn_rmw_diff_fp_run,
    .test_cleanup = mrn_rmw_diff_fp_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
