/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_nuke_dense_fp64_sve
 * @parblock
 * fp64 (FPU-datapath) extension of mrn_nuke_dense_sve — the low-temperature
 * channel detector that fired 3.33/hr (10/10 pinned to CPU122) under FAN100
 * 69-73°C while the neon_rot family stayed silent. Same-night companion run
 * with the integer original: does the low-temp channel travel the integer
 * move path only, or does the FP pipeline participate?
 *
 * The integer original is a pure-move skeleton with no ALU: svst1 temp <-
 * svld1 src, svst1 dst <- raw "ldr z" of temp (store-to-load forwarding
 * path), golden = src itself. A bare type swap (svuint64_t -> svfloat64_t)
 * would emit bit-identical machine code (LD1D/ST1D/LDR do not encode the
 * element type) and thus change nothing — so the fp64 variant routes the
 * data through a REAL FP op that is semantically the identity:
 *
 *   svld1_f64 src -> svmul_f64(v, 1.0) -> svst1_f64 temp -> raw ldr temp
 *   -> svst1_f64 dst -> memcmp(dst, expected)
 *
 * fmul-by-1.0 executes on the FP multiplier (no short-circuit) yet preserves
 * every value bit-class-wise: SNaN -> QNaN (same both sides), -0/+0, Inf,
 * subnormals (FTZ off) — so golden = expected, recomputed at init with the
 * SAME instruction on the SAME memset_random inputs (repo "all goldens
 * runtime-computed" rule; bitwise identical both sides).
 *
 * Kept identical to the integer original: 16384 elements (16x family
 * default), temp->dst forwarding leg via raw "ldr", u64 bit-pattern buffers,
 * cold-path per-element mismatch dump (golden/actual/xor), no /var/tmp file.
 * Known cost: the fmul latency makes the loop run slightly slower than the
 * integer original (marginal load-density drop) — noted, not compensated.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

#define MRN_NUKE_DENSE_FP64_SVE_COUNT 16384   /* 16x the 1024-element family default (same as integer) */

struct MrnNukeDenseFp64SveData {
    uint64_t *src;
    uint64_t *expected;   /* fmul(x,1.0) recomputed at init — integer original had golden=src */
};

static int mrn_nuke_dense_fp64_sve_init(struct test *test)
{
#if defined(__aarch64__)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for mrn_nuke_dense_fp64_sve");
        return EXIT_SKIP;
    }
    auto *data = static_cast<MrnNukeDenseFp64SveData *>(malloc(sizeof(MrnNukeDenseFp64SveData)));
    data->src = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_FP64_SVE_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_FP64_SVE_COUNT * sizeof(uint64_t)));
    memset_random(data->src, MRN_NUKE_DENSE_FP64_SVE_COUNT * sizeof(uint64_t));

    /* golden 同流同指令重算: fmul(x,1.0) 恒等乘, SNaN->QNaN 两侧行为位级一致 */
    const int lanes = svcntd();
    svbool_t pg = svptrue_b64();
    const svfloat64_t one = svdup_f64(1.0);
    for (size_t base = 0; base < MRN_NUKE_DENSE_FP64_SVE_COUNT; base += lanes) {
        int n = (MRN_NUKE_DENSE_FP64_SVE_COUNT - base < (size_t)lanes)
                    ? (int)(MRN_NUKE_DENSE_FP64_SVE_COUNT - base) : lanes;
        svbool_t pgn = svwhilelt_b64((uint64_t)0, (uint64_t)n);
        (void)pg;
        svfloat64_t v = svld1_f64(pgn, reinterpret_cast<const double *>(data->src + base));
        svst1_f64(pgn, reinterpret_cast<double *>(data->expected + base),
                  svmul_f64_x(pgn, v, one));
    }
    test->data = data;
    return EXIT_SUCCESS;
#else
    auto *data = static_cast<MrnNukeDenseFp64SveData *>(malloc(sizeof(MrnNukeDenseFp64SveData)));
    data->src = nullptr;
    data->expected = nullptr;
    test->data = data;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for mrn_nuke_dense_fp64_sve");
    return EXIT_SKIP;
#endif
}

#if defined(__aarch64__)

static inline svfloat64_t load_vec_f64_sve(const void *addr)
{
    svfloat64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

static int mrn_nuke_dense_fp64_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<MrnNukeDenseFp64SveData *>(test->data);
    const int lanes = svcntd();
    const svfloat64_t one = svdup_f64(1.0);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_FP64_SVE_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_DENSE_FP64_SVE_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (size_t base = 0; base < MRN_NUKE_DENSE_FP64_SVE_COUNT; base += lanes) {
            int n = (MRN_NUKE_DENSE_FP64_SVE_COUNT - base < (size_t)lanes)
                        ? (int)(MRN_NUKE_DENSE_FP64_SVE_COUNT - base) : lanes;
            svbool_t pg = svwhilelt_b64((uint64_t)0, (uint64_t)n);
            /* mrn_nuke 骨架保留: 向量 store temp, 转发 ldr z, store dst; 数据流过 FP 乘法器 (恒等 x*1.0) */
            svfloat64_t v = svld1_f64(pg, reinterpret_cast<const double *>(data->src + base));
            svst1_f64(pg, reinterpret_cast<double *>(temp + base), svmul_f64_x(pg, v, one));
            svst1_f64(pg, reinterpret_cast<double *>(dst + base), load_vec_f64_sve(temp + base));
        }
        if (memcmp(dst, data->expected, MRN_NUKE_DENSE_FP64_SVE_COUNT * sizeof(uint64_t)) != 0) {
            /* cold path: locate mismatches, record bit-level evidence (golden/actual/xor) */
            {
                size_t _dump_bad = 0;
                int _dump_shown = 0;
                for (size_t _dump_j = 0; _dump_j < (size_t)(MRN_NUKE_DENSE_FP64_SVE_COUNT); ++_dump_j) {
                    if (dst[_dump_j] != data->expected[_dump_j]) {
                        ++_dump_bad;
                        if (_dump_shown < 8) {
                            log_error("mrn_nuke_dense_fp64_sve: mismatch at index %zu: golden=0x%016llx actual=0x%016llx xor=0x%016llx",
                                      _dump_j,
                                      (unsigned long long)data->expected[_dump_j],
                                      (unsigned long long)dst[_dump_j],
                                      (unsigned long long)(data->expected[_dump_j] ^ dst[_dump_j]));
                            ++_dump_shown;
                        }
                    }
                }
                log_error("mrn_nuke_dense_fp64_sve: total mismatched elements: %zu / %d",
                          _dump_bad, MRN_NUKE_DENSE_FP64_SVE_COUNT);
            }
            report_fail_msg("mrn_nuke_dense_fp64_sve data miscompare");
        }
    }
    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

#else

static int mrn_nuke_dense_fp64_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for mrn_nuke_dense_fp64_sve");
    return EXIT_SKIP;
}

#endif

static int mrn_nuke_dense_fp64_sve_cleanup(struct test *test)
{
    auto *data = static_cast<MrnNukeDenseFp64SveData *>(test->data);
    if (data) {
        free(data->src);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_nuke_dense_fp64_sve,
             "fp64 FPU-datapath extension of mrn_nuke_dense: same pure-move skeleton "
             "(store temp, forwarding ldr, store dst) with the data routed through the FP "
             "multiplier via identity fmul(x,1.0) — probes whether the CPU122 low-temperature "
             "channel fires with the FP pipeline in the datapath")
    .test_init = mrn_nuke_dense_fp64_sve_init,
    .test_run = mrn_nuke_dense_fp64_sve_run,
    .test_cleanup = mrn_nuke_dense_fp64_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
