/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_rmw_fwd
 * @test @b mrn_rmw_l1d
 * @parblock
 * Load-pathway subdivision probe for core-179 SDC.
 *
 * Context: we already localized the 179 SDC to the per-core load data path
 * (L1D read array -> load port) via movbe_l1d_vs_l2 (see memory
 * l1d-read-path-sdc-confirmed).  This probe narrows it further by
 * distinguishing two load sub-paths that both reach the load port:
 *
 *   - store buffer FORWARDING: a load from an address that was just stored
 *     (str then ldr, same address) can be satisfied directly from the store
 *     buffer WITHOUT going through the L1D read array.
 *   - L1D ARRAY read: a load from an address already resident in the L1D
 *     array (not just written) is served by the L1D read array -> load port.
 *
 * mrn_rmw (original) does `str res->temp[i]; dst[i]=ldr temp[i]` — the ldr
 * of temp[i] immediately follows the str of temp[i] at the same address,
 * so it exercises store-buffer FORWARDING.  mrn_rmw is known to trigger the
 * 179 SDC.  Question: does the SDC also occur when the same reload is forced
 * to go through the L1D ARRAY read instead of forwarding?
 *
 * Two SEPARATE tests (no shared run with a regime conditional — that
 * conditional perturbs the hot loop and silences the timing-sensitive
 * defect; see memory dump-variant-noremove-report-fail-verified):
 *
 *   - mrn_rmw_fwd: hot loop byte-identical to mrn_rmw.cpp.  The
 *     `store_qword(&temp[i],res); store_qword(&dst[i], load_qword(&temp[i]))`
 *     pair keeps the str->ldr-same-address forwarding path.  Expected:
 *     SDC triggers (baseline, matches mrn_rmw).
 *   - mrn_rmw_l1d: same hot loop, but `dc civac`+dsb is inserted between the
 *     str to temp[i] and the ldr of temp[i], evicting/flushing the store so
 *     the ldr must be served from the L1D array (or further) rather than
 *     from store-buffer forwarding.  If the SDC disappears here while
 *     mrn_rmw_fwd still triggers, the defect is in the store-buffer
 *     FORWARDING path; if it persists, the common point is downstream
 *     (load port -> register), since both sub-paths converge there.
 *
 * On miscompare, logs inputA/inputB/golden/actual/xor (same as mrn_rmw_dump)
 * then report_fail_msg (noreturn) — matching mrn_rmw's behavior; one pass
 * over MRN_RMW_COUNT already logs a batch of events before thread exit.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_RMW_COUNT 1024

struct MrnRmwData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *expected;
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
static inline void evict_l1d_line(const void *addr) {
    __asm__ volatile(
        "dc civac, %0\n\t"
        "dsb sy\n\t"
        :: "r"(addr) : "memory"
    );
}
#endif

static int mrn_rmw_probe_init(struct test *test) {
    auto *data = static_cast<MrnRmwData *>(malloc(sizeof(MrnRmwData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_RMW_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_RMW_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_RMW_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] - data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_rmw_probe_cleanup(struct test *test) {
    auto *data = static_cast<MrnRmwData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

// Shared miscompare logger (cold branch only).
static void log_miscompare(const char *tag, int i, const char *op,
                           uint64_t inputA, uint64_t inputB,
                           uint64_t golden, uint64_t actual)
{
    uint64_t xorv = golden ^ actual;
    unsigned char *gb = (unsigned char *)&golden;
    unsigned char *ab = (unsigned char *)&actual;
    unsigned char *xb = (unsigned char *)&xorv;
    log_error("%s: miscompare at index %u (op=%s): "
              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
              "actual=0x%016lX xor=0x%016lX | "
              "bytes[b7..b0] golden=%02X%02X%02X%02X%02X%02X%02X%02X "
              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
              tag, (unsigned)i, op,
              (unsigned long)inputA, (unsigned long)inputB,
              (unsigned long)golden, (unsigned long)actual,
              (unsigned long)xorv,
              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
    report_fail_msg("%s: data miscompare at index %u (op=%s)", tag, (unsigned)i, op);
}

// ---------------------------------------------------------------------------
// mrn_rmw_fwd: store-buffer FORWARDING path.  Hot loop byte-identical to
// mrn_rmw.cpp — str to temp[i] immediately followed by ldr of temp[i] at the
// same address, served by store-buffer forwarding.
// ---------------------------------------------------------------------------
static int mrn_rmw_fwd_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<MrnRmwData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));

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

            store_qword(&temp[i], res);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_miscompare("mrn_rmw_fwd", i, op,
                                   data->srcA[i], data->srcB[i],
                                   data->expected[i], dst[i]);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// mrn_rmw_l1d: L1D-ARRAY read path.  Same hot loop, but `dc civac`+dsb is
// inserted between the str to temp[i] and the ldr of temp[i].  This flushes
// the just-stored line out of the store buffer / L1D so the subsequent ldr
// is served from the L1D array (or further) instead of store-buffer
// forwarding.  The str/ldr/ALU sequence around it is otherwise identical.
// ---------------------------------------------------------------------------
static int mrn_rmw_l1d_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<MrnRmwData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));

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

            store_qword(&temp[i], res);
            // Flush the just-stored line so the reload below is served from
            // the L1D array / further, NOT from store-buffer forwarding.
            evict_l1d_line(&temp[i]);
            store_qword(&dst[i], load_qword(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_miscompare("mrn_rmw_l1d", i, op,
                                   data->srcA[i], data->srcB[i],
                                   data->expected[i], dst[i]);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_fwd, "mrn_rmw load-path probe (store-buffer FORWARDING): str then ldr same addr — core-179 SDC expected")
    .test_init = mrn_rmw_probe_init,
    .test_run = mrn_rmw_fwd_run,
    .test_cleanup = mrn_rmw_probe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(mrn_rmw_l1d, "mrn_rmw load-path probe (L1D-array read): evict between str and ldr — distinguishes forwarding vs L1D array")
    .test_init = mrn_rmw_probe_init,
    .test_run = mrn_rmw_l1d_run,
    .test_cleanup = mrn_rmw_probe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
