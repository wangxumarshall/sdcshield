/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_str3_checkall_fp64_sve
 * @parblock
 * fp64 (FPU/vector-FP datapath) extension of neon_rot_ldr_at_top_str3_checkall_sve
 * (the 1.8GHz census #1 detector, 164/hr @AUTO warm). Same skeleton, swapped
 * datapath — the user-mandated experiment: does the CPU122 defect fire on the
 * FP pipeline at all?
 *
 * Identical to the integer original: 3 ldr = dead scratch at top + 2 sources,
 * slot%-rotating ALU op, THREE stores of the same res to temp/junk/dst,
 * alternating asc/desc sweeps, THREE unconditional memcmps per sweep OR-ed
 * into a slot_mask, noinline cold scan classifying WHICH store slot(s)
 * mismatched per element (single-bit mask -> that store's own write path,
 * 0b111 -> upstream), cold re-read transient records, /var/tmp binary dump.
 *
 * Only the vector register side changes: svuint64_t -> svfloat64_t and the
 * four rotating ops add/eor/and/orr -> fadd/fsub/fmul/fabd (all 2-source
 * fully-pipelined FP ops; fdiv deliberately avoided — it would skew the
 * loop's IPC/latency profile vs the integer baseline). Memory buffers stay
 * uint64_t bit-pattern containers so memcmp/records stay byte-exact.
 *
 * Golden correctness: init recomputes expected with the SAME sv* intrinsics
 * on the SAME memset_random inputs in the SAME process (FPCR fixed RN, no
 * FTZ divergence) -> SNaN quieting / Inf / subnormal results are bitwise
 * identical on both sides; memcmp remains a valid byte-exact check (repo
 * "all goldens runtime-computed" rule).
 *
 * Cross-test rate comparison caveat (kept from the original): within-run
 * slot comparison is valid; cross-test rate vs the integer str3_checkall is
 * an order-of-magnitude comparison only (different op mix, same structure).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#if defined(__aarch64__)
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

#define NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS 1024

#pragma pack(push, 1)
struct str3_checkall_fp64_sve_record {
    uint64_t timestamp_us;
    uint32_t cpu;             /* logical CPU */
    uint32_t iteration;       /* 0-based sweep */
    uint32_t sweep_dir;       /* 0=asc 1=desc */
    uint32_t sweep_slot_mask; /* bit0=temp bit1=junk bit2=dst */
    uint32_t index;           /* fp64 element index; 0xFFFFFFFF = sweep-level transient */
    uint32_t index_slot_mask; /* per-element slot mask at rescan */
    uint32_t op;              /* slot%4 */
    uint64_t srcA, srcB, exp, temp, junk, dst;  /* double bit patterns */
};
#pragma pack(pop)

struct NeonRotLdrAtTopStr3CheckallFp64SveData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *scratch;
    uint64_t *expected;
    int lanes;
    int dump_fd;
};

static uint64_t str3_ca_fp64_sve_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000;
}

#if defined(__aarch64__)

static inline void store_vec_f64_sve(void *addr, svfloat64_t val) {
    svst1_f64(svptrue_b64(), static_cast<double *>(addr), val);
}

static inline svfloat64_t load_vec_f64_sve(const void *addr) {
    svfloat64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* noinline: 防冷扫描与热循环融合 (tlbstress 教训) */
__attribute__((noinline)) static void str3_checkall_fp64_sve_cold_scan(
        struct test *test, int cpu, const NeonRotLdrAtTopStr3CheckallFp64SveData *data,
        const uint64_t *temp, const uint64_t *junk, const uint64_t *dst,
        uint32_t sweep_slot_mask, uint64_t iteration, uint32_t sweep_dir)
{
    const size_t elems = (size_t)NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS * data->lanes;
    bool any = false;
    for (size_t j = 0; j < elems; j++) {
        const bool t_bad = temp[j] != data->expected[j];
        const bool k_bad = junk[j] != data->expected[j];
        const bool d_bad = dst[j] != data->expected[j];
        if (!t_bad && !k_bad && !d_bad) continue;
        any = true;
        uint32_t index_slot_mask = (t_bad ? 1u : 0u) | (k_bad ? 2u : 0u) | (d_bad ? 4u : 0u);
        const int slot = (int)(j / (size_t)data->lanes);
        log_error("neon_rot_ldr_at_top_str3_checkall_fp64_sve: slot mismatch at index %zu "
                  "(slot %d op=%d dir=%s sweep_mask=0x%x index_mask=0x%x): "
                  "srcA=0x%016lX srcB=0x%016lX exp=0x%016lX temp=0x%016lX "
                  "junk=0x%016lX dst=0x%016lX",
                  j, slot, slot % 4, sweep_dir ? "desc" : "asc",
                  sweep_slot_mask, index_slot_mask,
                  (unsigned long)data->srcA[j], (unsigned long)data->srcB[j],
                  (unsigned long)data->expected[j], (unsigned long)temp[j],
                  (unsigned long)junk[j], (unsigned long)dst[j]);
        if (data->dump_fd >= 0) {
            str3_checkall_fp64_sve_record rec;
            rec.timestamp_us = str3_ca_fp64_sve_now_us();
            rec.cpu = (uint32_t)device_info[cpu].cpu_number;
            rec.iteration = (uint32_t)iteration;
            rec.sweep_dir = sweep_dir;
            rec.sweep_slot_mask = sweep_slot_mask;
            rec.index = (uint32_t)j;
            rec.index_slot_mask = index_slot_mask;
            rec.op = (uint32_t)(slot % 4);
            rec.srcA = data->srcA[j];
            rec.srcB = data->srcB[j];
            rec.exp = data->expected[j];
            rec.temp = temp[j];
            rec.junk = junk[j];
            rec.dst = dst[j];
            if (write(data->dump_fd, &rec, sizeof rec) < 0) { /* best effort */ }
        }
    }
    if (!any) {
        /* 瞬态错误: memcmp 失配但重扫干净 — sweep 级记录 */
        if (data->dump_fd >= 0) {
            str3_checkall_fp64_sve_record rec;
            memset(&rec, 0, sizeof rec);
            rec.timestamp_us = str3_ca_fp64_sve_now_us();
            rec.cpu = (uint32_t)device_info[cpu].cpu_number;
            rec.iteration = (uint32_t)iteration;
            rec.sweep_dir = sweep_dir;
            rec.sweep_slot_mask = sweep_slot_mask;
            rec.index = 0xFFFFFFFFu;   /* transient marker (原版语义) */
            log_error("neon_rot_ldr_at_top_str3_checkall_fp64_sve: transient mismatch "
                      "(sweep_mask=0x%x, rescan clean)", sweep_slot_mask);
            if (write(data->dump_fd, &rec, sizeof rec) < 0) { /* best effort */ }
        }
    }
}

#endif

static int neon_rot_ldr_at_top_str3_checkall_fp64_sve_init(struct test *test) {
#if defined(__aarch64__)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for neon_rot_ldr_at_top_str3_checkall_fp64_sve");
        return EXIT_SKIP;
    }

    const int lanes = (int)svcntd();
    const size_t elems = (size_t)NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS * lanes;

    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallFp64SveData *>(malloc(sizeof(NeonRotLdrAtTopStr3CheckallFp64SveData)));
    data->lanes = lanes;
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, elems * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, elems * sizeof(uint64_t)));
    data->scratch = static_cast<uint64_t *>(aligned_alloc(64, elems * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, elems * sizeof(uint64_t)));

    memset_random(data->srcA, elems * sizeof(uint64_t));
    memset_random(data->srcB, elems * sizeof(uint64_t));
    memset_random(data->scratch, elems * sizeof(uint64_t));

    /* golden = 同流同指令重算: 与 run 侧完全相同的 sv* 调用, 位级一致
     * (FPCR 固定 RN、同进程无 FTZ 差异, SNaN/Inf/次正规两侧行为相同) */
    svbool_t pg = svptrue_b64();
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS; i++) {
        svfloat64_t a = svld1_f64(pg, reinterpret_cast<const double *>(data->srcA + (size_t)i * lanes));
        svfloat64_t b = svld1_f64(pg, reinterpret_cast<const double *>(data->srcB + (size_t)i * lanes));
        svfloat64_t res;
        switch (i % 4) {
            case 0: res = svadd_f64_x(pg, a, b); break;
            case 1: res = svsub_f64_x(pg, a, b); break;
            case 2: res = svmul_f64_x(pg, a, b); break;
            case 3: res = svabd_f64_x(pg, a, b); break;
        }
        svst1_f64(pg, reinterpret_cast<double *>(data->expected + (size_t)i * lanes), res);
    }

    char path[128];
    snprintf(path, sizeof(path), "/var/tmp/neon_rot_ldr_at_top_str3_checkall_fp64_sve_%d.bin", (int)getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("neon_rot_ldr_at_top_str3_checkall_fp64_sve: cannot open %s (errno %d) — "
                  "continuing without binary records", path, errno);
    }

    test->data = data;
    return EXIT_SUCCESS;
#else
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for neon_rot_ldr_at_top_str3_checkall_fp64_sve");
    return EXIT_SKIP;
#endif
}

#if defined(__aarch64__)
static int neon_rot_ldr_at_top_str3_checkall_fp64_sve_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallFp64SveData *>(test->data);

    if ((int)svcntd() != data->lanes) {
        log_error("neon_rot_ldr_at_top_str3_checkall_fp64_sve: vector length changed between init (%d) and run (%d)",
                  data->lanes, (int)svcntd());
        report_fail_msg("neon_rot_ldr_at_top_str3_checkall_fp64_sve: vector length changed between init and run");
    }

    const size_t elems = (size_t)NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS * data->lanes;
    const size_t bytes = elems * sizeof(uint64_t);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    uint64_t *junk = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, bytes));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_SVE_SLOTS - 1 - n);
            const size_t off = (size_t)i * data->lanes;

            svfloat64_t unused_x = load_vec_f64_sve(data->scratch + off);
            (void)unused_x;

            svfloat64_t a = load_vec_f64_sve(data->srcA + off);
            svfloat64_t b = load_vec_f64_sve(data->srcB + off);
            svfloat64_t res;

            switch (i % 4) {
                case 0: res = svadd_f64_x(svptrue_b64(), a, b); break;
                case 1: res = svsub_f64_x(svptrue_b64(), a, b); break;
                case 2: res = svmul_f64_x(svptrue_b64(), a, b); break;
                case 3: res = svabd_f64_x(svptrue_b64(), a, b); break;
            }

            store_vec_f64_sve(temp + off, res);
            store_vec_f64_sve(junk + off, res);
            store_vec_f64_sve(dst + off, res);
        }
        const uint32_t sweep_dir = ascending ? 0u : 1u;
        sweep++;

        /* 三个无条件 memcmp OR 成 slot_mask (bit0=temp bit1=junk bit2=dst) */
        uint32_t slot_mask = 0;
        if (memcmp(temp, data->expected, bytes) != 0) slot_mask |= 1u;
        if (memcmp(junk, data->expected, bytes) != 0) slot_mask |= 2u;
        if (memcmp(dst, data->expected, bytes) != 0) slot_mask |= 4u;

        if (slot_mask != 0) {
            str3_checkall_fp64_sve_cold_scan(test, cpu, data, temp, junk, dst,
                                             slot_mask, sweep - 1, sweep_dir);
            report_fail_msg("neon_rot_ldr_at_top_str3_checkall_fp64_sve data miscompare "
                            "(slot_mask=0x%x)", slot_mask);
        }
    }

    free(dst);
    free(junk);
    free(temp);
    return EXIT_SUCCESS;
}
#else
static int neon_rot_ldr_at_top_str3_checkall_fp64_sve_run(struct test *test, int cpu) {
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for neon_rot_ldr_at_top_str3_checkall_fp64_sve");
    return EXIT_SKIP;
}
#endif

static int neon_rot_ldr_at_top_str3_checkall_fp64_sve_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallFp64SveData *>(test->data);
    if (data) {
        if (data->dump_fd >= 0)
            close(data->dump_fd);
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_str3_checkall_fp64_sve,
             "fp64 FPU-datapath extension of str3_checkall: same 3-ldr/3-str hot loop and "
             "slot-classifying verification, svfloat64_t with fadd/fsub/fmul/fabd rotation — "
             "probes whether the CPU122 load-return defect fires on the FP pipeline")
    .test_init    = neon_rot_ldr_at_top_str3_checkall_fp64_sve_init,
    .test_run     = neon_rot_ldr_at_top_str3_checkall_fp64_sve_run,
    .test_cleanup = neon_rot_ldr_at_top_str3_checkall_fp64_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
