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
// load_qword_delayed: reload after N data-independent ALU no-ops (mov x9,x9).
// Pure timing delay: touches NO memory and NO cache, so the reload still
// goes through store-buffer FORWARDING (the stored line is still in the
// store buffer; nothing evicted it). Only the str->ldr time gap widened.
// Uses .rept N to emit N no-ops compactly.
//
// Bisection of the timing window: 64 no-ops (~20+ cycles) gave 0 fail / 364
// iters; 1 no-op (~1 cycle) gave 1 fail / 10min (mrn_rmw_nop1_10m.log).
// So the race window is between 1 and 64 cycles. 2 no-ops (~2 cycles) bisects
// the low end: does the residual fail at 1 cycle disappear at 2, or does the
// window extend further?  Store buffer cannot drain in 2 cycles, so ldr is
// STILL served by forwarding -> path identical to fwd/nop1, only timing changes.
static inline uint64_t load_qword_delayed(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 2\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
// load_qword_delayed1: reload after 1 data-independent ALU no-op (mov x9,x9).
// Same path semantics as load_qword_delayed (forwarding kept) but with only a
// ~1-cycle str->ldr gap. Re-creates the historical mrn_rmw_nop1 point (V3 §12.3
// curve) that the current binary lacks — the source only hard-codes 2 and 64
// no-ops; this adds the missing 1-no-op point for a same-source re-test.
static inline uint64_t load_qword_delayed1(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 1\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
// load_qword_delayed4: reload after 4 data-independent ALU no-ops (mov x9,x9).
// Same path semantics as load_qword_delayed (forwarding kept) but with a
// ~4-cycle str->ldr gap.  Extends the gap series fwd(0)/nop1(1)/nop(2)/nop4(4)
// so the non-monotonic 1->2 rise can be checked at 4 (does it keep rising, peak,
// or start dropping toward the nop64=0 plateau?).
static inline uint64_t load_qword_delayed4(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 4\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
// load_qword_delayed8: reload after 8 data-independent ALU no-ops (mov x9,x9).
// Same path semantics as load_qword_delayed (forwarding kept) but with a
// ~8-cycle str->ldr gap.  Extends the gap series to 8 to trace the descent
// from the ~2-cycle peak (nop2=6) toward the nop64=0 plateau: at 8, is the
// rate still falling (nop4~1) or already near 0?
static inline uint64_t load_qword_delayed8(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 8\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
// load_qword_delayed16: reload after 16 data-independent ALU no-ops (mov x9,x9).
// Same path semantics as load_qword_delayed (forwarding kept) but with a
// ~16-cycle str->ldr gap.  Extends the gap series to 16: nop8 (30min) was 0
// fail, matching nop64's plateau — this probes whether 16 (between the 4-cycle
// residual ~1 and the 64-cycle 0) stays 0 or shows any tail triggering that
// shorter windows miss. Longer run (1h) raises confidence that a 0 is "window
// truly closed" rather than "didn't sample enough".
static inline uint64_t load_qword_delayed16(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 16\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
// load_qword_delayed64: reload after 64 data-independent ALU no-ops.  Same
// path semantics as load_qword_delayed (forwarding kept — store buffer
// undrained, no memory/cache touched) but widens the str->ldr gap to ~20+
// cycles.  Historically 0 fail / 364 iters at this gap (vs 1 fail at 1 no-op).
// Used as the SAME-ADDRESS (forwarding) cell of the 2x2 timing matrix; the
// matching DIFFERENT-address cell is mrn_rmw_diff_nop64 (L1D-array read path).
static inline uint64_t load_qword_delayed64(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 64\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
#endif

static int mrn_rmw_nop_init(struct test *test) {
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

static int mrn_rmw_nop_cleanup(struct test *test) {
    auto *data = static_cast<MrnRmwData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

static int mrn_rmw_nop_run(struct test *test, int cpu) {
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
            store_qword(&dst[i], load_qword_delayed(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_error("mrn_rmw_nop: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX xor=0x%016lX | "
                              "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                              (unsigned)i, op,
                              (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)xorv,
                              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("mrn_rmw_nop: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_nop, "mrn_rmw timing-only probe: 2 ALU no-ops between str and ldr (forwarding path kept, store buffer undrained) - bisects the 1..64 cycle race window (nop1=1fail, nop64=0fail)")
    .test_init = mrn_rmw_nop_init,
    .test_run = mrn_rmw_nop_run,
    .test_cleanup = mrn_rmw_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

// ---------------------------------------------------------------------------
// mrn_rmw_nop1: same as mrn_rmw_nop but with only 1 no-op (~1 cycle) between
// str and ldr.  Re-creates the missing low-gap point of the §12.3 curve so the
// fwd(0)/nop1(1)/nop(2) gap series can be re-measured from one source build.
// ---------------------------------------------------------------------------
static int mrn_rmw_nop1_run(struct test *test, int cpu) {
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
            store_qword(&dst[i], load_qword_delayed1(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_error("mrn_rmw_nop1: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX xor=0x%016lX | "
                              "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                              (unsigned)i, op,
                              (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)xorv,
                              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("mrn_rmw_nop1: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_nop1, "mrn_rmw timing-only probe: 1 ALU no-op between str and ldr (forwarding path kept, store buffer undrained) — re-creates the missing ~1-cycle point of the fwd(0)/nop1(1)/nop(2) gap series")
    .test_init = mrn_rmw_nop_init,
    .test_run = mrn_rmw_nop1_run,
    .test_cleanup = mrn_rmw_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

// ---------------------------------------------------------------------------
// mrn_rmw_nop4: same as mrn_rmw_nop but with 4 no-ops (~4 cycles) between
// str and ldr.  Extends the gap series to fwd(0)/nop1(1)/nop(2)/nop4(4): after
// the non-monotonic 1->2 rise (nop1=0, nop2=6 today), does 4 keep rising, peak,
// or begin dropping toward the nop64=0 plateau?
// ---------------------------------------------------------------------------
static int mrn_rmw_nop4_run(struct test *test, int cpu) {
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
            store_qword(&dst[i], load_qword_delayed4(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_error("mrn_rmw_nop4: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX xor=0x%016lX | "
                              "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                              (unsigned)i, op,
                              (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)xorv,
                              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("mrn_rmw_nop4: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_nop4, "mrn_rmw timing-only probe: 4 ALU no-ops between str and ldr (forwarding path kept, store buffer undrained) — extends the fwd(0)/nop1(1)/nop(2)/nop4(4) gap series past the non-monotonic 1->2 rise")
    .test_init = mrn_rmw_nop_init,
    .test_run = mrn_rmw_nop4_run,
    .test_cleanup = mrn_rmw_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

// ---------------------------------------------------------------------------
// mrn_rmw_nop8: same as mrn_rmw_nop but with 8 no-ops (~8 cycles) between
// str and ldr.  Extends the gap series to 8 to trace the descent from the
// ~2-cycle peak (nop2=6) toward the nop64=0 plateau.
// ---------------------------------------------------------------------------
static int mrn_rmw_nop8_run(struct test *test, int cpu) {
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
            store_qword(&dst[i], load_qword_delayed8(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_error("mrn_rmw_nop8: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX xor=0x%016lX | "
                              "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                              (unsigned)i, op,
                              (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)xorv,
                              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("mrn_rmw_nop8: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_nop8, "mrn_rmw timing-only probe: 8 ALU no-ops between str and ldr (forwarding path kept, store buffer undrained) — extends the fwd(0)/nop1(1)/nop(2)/nop4(4)/nop8(8) gap series, tracing the descent from the ~2-cycle peak toward the nop64=0 plateau")
    .test_init = mrn_rmw_nop_init,
    .test_run = mrn_rmw_nop8_run,
    .test_cleanup = mrn_rmw_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

// ---------------------------------------------------------------------------
// mrn_rmw_nop16: same as mrn_rmw_nop but with 16 no-ops (~16 cycles) between
// str and ldr.  Extends the gap series to 16. nop8 (30min) and nop64 are both
// 0-fail; 16 sits between the 4-cycle residual (~1) and the 64-cycle plateau,
// run long (1h) to raise confidence that 0 means "window closed", not "undersampled".
// ---------------------------------------------------------------------------
static int mrn_rmw_nop16_run(struct test *test, int cpu) {
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
            store_qword(&dst[i], load_qword_delayed16(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_error("mrn_rmw_nop16: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX xor=0x%016lX | "
                              "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                              (unsigned)i, op,
                              (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)xorv,
                              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("mrn_rmw_nop16: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_nop16, "mrn_rmw timing-only probe: 16 ALU no-ops between str and ldr (forwarding path kept, store buffer undrained) — extends the gap series to 16, between the 4-cycle residual (~1) and the 64-cycle 0-plateau; run long to confirm the window is truly closed")
    .test_init = mrn_rmw_nop_init,
    .test_run = mrn_rmw_nop16_run,
    .test_cleanup = mrn_rmw_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

// ---------------------------------------------------------------------------
// mrn_rmw_nop64: same-address (store-buffer FORWARDING) path with 64 no-ops
// between str and ldr.  Same design as mrn_rmw_nop but with a 64-no-op gap.
// This is the SAME-ADDRESS cell of the 2x2 timing matrix; the matching
// DIFFERENT-address (L1D-array read) cell is mrn_rmw_diff_nop64.  Comparing
// the two isolates whether nop64's 0-fail is a path switch (store drained ->
// reload switched to L1D-array) or a timing-window effect confined to the
// forwarding path: if mrn_rmw_diff_nop64 (L1D-array, 64 no-ops) still triggers
// while mrn_rmw_nop64 is 0-fail, the 0-fail is NOT a path switch.
// ---------------------------------------------------------------------------
static int mrn_rmw_nop64_run(struct test *test, int cpu) {
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
            store_qword(&dst[i], load_qword_delayed64(&temp[i]));
        }

        if (memcmp(dst, data->expected, MRN_RMW_COUNT * sizeof(uint64_t)) != 0) {
            for (int i = 0; i < MRN_RMW_COUNT; i++) {
                if (dst[i] != data->expected[i]) {
                    uint64_t golden = data->expected[i];
                    uint64_t actual = dst[i];
                    uint64_t xorv = golden ^ actual;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    const char *op = (i % 4 == 0) ? "add"
                                   : (i % 4 == 1) ? "sub"
                                   : (i % 4 == 2) ? "xor" : "and";
                    log_error("mrn_rmw_nop64: miscompare at index %u (op=%s): "
                              "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                              "actual=0x%016lX xor=0x%016lX | "
                              "bytes golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                              (unsigned)i, op,
                              (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                              (unsigned long)golden, (unsigned long)actual,
                              (unsigned long)xorv,
                              gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                              ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                              xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("mrn_rmw_nop64: data miscompare at index %u (op=%s)", (unsigned)i, op);
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_nop64, "mrn_rmw timing-only probe: 64 ALU no-ops between str and ldr (forwarding path kept, store buffer undrained) — same-address cell of the 2x2 timing matrix (historically 0 fail); pair with mrn_rmw_diff_nop64 to decide path-switch vs timing-window")
    .test_init = mrn_rmw_nop_init,
    .test_run = mrn_rmw_nop64_run,
    .test_cleanup = mrn_rmw_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
