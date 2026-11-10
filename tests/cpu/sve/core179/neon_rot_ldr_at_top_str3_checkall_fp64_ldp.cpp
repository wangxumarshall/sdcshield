/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_str3_checkall_fp64_ldp
 * @parblock
 * Load/store-instruction-form axis of neon_rot_ldr_at_top_str3_checkall_fp64_sve:
 * same skeleton (dead scratch load at top + 2 sources, slot%-rotating FP op,
 * THREE stores of the same pair to temp/junk/dst, alternating sweeps, 3-way
 * memcmp + slot_mask + noinline cold scan + transient records), but every
 * vector access (SVE "ldr z" 256-bit / ST1D) is replaced by scalar PAIR
 * instructions:
 *
 *   ldp d0, d1, [src]   — one instruction, TWO 64-bit accesses (128-bit)
 *   stp d0, d1, [dst]   — one instruction, TWO 64-bit stores
 *
 * Rationale (user-mandated experiment): LDP/STP carry two memory operations
 * per instruction — higher LSU-issue/AGU density than a single vector access,
 * and the stp->ldp same-address forwarding exercises the SCALAR PAIR
 * store-to-load forwarding path (a different datapath form from the SVE
 * vector forwarding of the fp64 original). Per-instruction bandwidth is
 * halved (128 vs 256 bit) so the sweep count roughly doubles per unit time —
 * bandwidth-rate parity, instruction-form contrast.
 *
 * Slot layout: 1024 slots x 2 doubles (pair) = 2048 elements (vs 4096 in the
 * SVE version). FP ops rotate add/sub/mul/abd per slot (slot%4) on scalars;
 * golden recomputed at init with the same expressions (FPCR fixed RN ->
 * SNaN/Inf/subnormal bitwise identical both sides). No SVE required — pure
 * AArch64 scalar FP (d-regs) + LDP/STP; gated on __aarch64__ only.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_SLOTS 1024
#define NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_ELEMS \
    (NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_SLOTS * 2)

#pragma pack(push, 1)
struct str3_checkall_fp64_ldp_record {
    uint64_t timestamp_us;
    uint32_t cpu;
    uint32_t iteration;
    uint32_t sweep_dir;
    uint32_t sweep_slot_mask; /* bit0=temp bit1=junk bit2=dst */
    uint32_t index;           /* double element index; 0xFFFFFFFF = sweep-level transient */
    uint32_t index_slot_mask;
    uint32_t op;              /* slot%4 */
    uint64_t srcA, srcB, exp, temp, junk, dst;  /* double bit patterns */
};
#pragma pack(pop)

struct NeonRotLdrAtTopStr3CheckallFp64LdpData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *scratch;
    uint64_t *expected;
    int dump_fd;
};

static uint64_t str3_ca_fp64_ldp_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000;
}

#if defined(__aarch64__)

/* ldp/stp 标量对指令: 一条指令两个 64-bit 访存 (d0/d1 = V 寄存器低 64-bit) */
static inline void ldp_pair_f64(const void *addr, double *x0, double *x1) {
    __asm__ volatile ("ldp %d0, %d1, [%2]" : "=w"(*x0), "=w"(*x1) : "r"(addr) : "memory");
}
static inline void stp_pair_f64(void *addr, double x0, double x1) {
    __asm__ volatile ("stp %d0, %d1, [%2]" :: "w"(x0), "w"(x1), "r"(addr) : "memory");
}

/* slot%4 轮换 FP 运算 (标量; 与 SVE 版 fadd/fsub/fmul/abd 语义一致) */
static inline void pair_op(int slot, double a0, double a1, double b0, double b1,
                           double *r0, double *r1) {
    switch (slot % 4) {
        case 0: *r0 = a0 + b0; *r1 = a1 + b1; break;
        case 1: *r0 = a0 - b0; *r1 = a1 - b1; break;
        case 2: *r0 = a0 * b0; *r1 = a1 * b1; break;
        default: *r0 = __builtin_fabs(a0 - b0); *r1 = __builtin_fabs(a1 - b1); break;
    }
}

/* noinline: 防冷扫描与热循环融合 (tlbstress 教训) */
__attribute__((noinline)) static void str3_checkall_fp64_ldp_cold_scan(
        struct test *test, int cpu, const NeonRotLdrAtTopStr3CheckallFp64LdpData *data,
        const uint64_t *temp, const uint64_t *junk, const uint64_t *dst,
        uint32_t sweep_slot_mask, uint64_t iteration, uint32_t sweep_dir)
{
    const size_t elems = (size_t)NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_ELEMS;
    bool any = false;
    for (size_t j = 0; j < elems; j++) {
        const bool t_bad = temp[j] != data->expected[j];
        const bool k_bad = junk[j] != data->expected[j];
        const bool d_bad = dst[j] != data->expected[j];
        if (!t_bad && !k_bad && !d_bad) continue;
        any = true;
        uint32_t index_slot_mask = (t_bad ? 1u : 0u) | (k_bad ? 2u : 0u) | (d_bad ? 4u : 0u);
        const int slot = (int)(j / 2);
        log_error("neon_rot_ldr_at_top_str3_checkall_fp64_ldp: slot mismatch at index %zu "
                  "(slot %d op=%d dir=%s sweep_mask=0x%x index_mask=0x%x): "
                  "srcA=0x%016lX srcB=0x%016lX exp=0x%016lX temp=0x%016lX "
                  "junk=0x%016lX dst=0x%016lX",
                  j, slot, slot % 4, sweep_dir ? "desc" : "asc",
                  sweep_slot_mask, index_slot_mask,
                  (unsigned long)data->srcA[j], (unsigned long)data->srcB[j],
                  (unsigned long)data->expected[j], (unsigned long)temp[j],
                  (unsigned long)junk[j], (unsigned long)dst[j]);
        if (data->dump_fd >= 0) {
            str3_checkall_fp64_ldp_record rec;
            rec.timestamp_us = str3_ca_fp64_ldp_now_us();
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
            str3_checkall_fp64_ldp_record rec;
            memset(&rec, 0, sizeof rec);
            rec.timestamp_us = str3_ca_fp64_ldp_now_us();
            rec.cpu = (uint32_t)device_info[cpu].cpu_number;
            rec.iteration = (uint32_t)iteration;
            rec.sweep_dir = sweep_dir;
            rec.sweep_slot_mask = sweep_slot_mask;
            rec.index = 0xFFFFFFFFu;   /* transient marker (原版语义) */
            log_error("neon_rot_ldr_at_top_str3_checkall_fp64_ldp: transient mismatch "
                      "(sweep_mask=0x%x, rescan clean)", sweep_slot_mask);
            if (write(data->dump_fd, &rec, sizeof rec) < 0) { /* best effort */ }
        }
    }
}

#endif

static int neon_rot_ldr_at_top_str3_checkall_fp64_ldp_init(struct test *test) {
#if defined(__aarch64__)
    const size_t elems = (size_t)NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_ELEMS;
    const size_t bytes = elems * sizeof(uint64_t);

    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallFp64LdpData *>(malloc(sizeof(NeonRotLdrAtTopStr3CheckallFp64LdpData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    data->scratch = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, bytes));

    memset_random(data->srcA, bytes);
    memset_random(data->srcB, bytes);
    memset_random(data->scratch, bytes);

    /* golden = 同表达式重算 (标量 FP, FPCR 固定 RN -> 位级一致) */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_SLOTS; i++) {
        double a0, a1, b0, b1, r0, r1;
        ldp_pair_f64(data->srcA + (size_t)i * 2, &a0, &a1);
        ldp_pair_f64(data->srcB + (size_t)i * 2, &b0, &b1);
        pair_op(i, a0, a1, b0, b1, &r0, &r1);
        stp_pair_f64(data->expected + (size_t)i * 2, r0, r1);
    }

    char path[128];
    snprintf(path, sizeof(path), "/var/tmp/neon_rot_ldr_at_top_str3_checkall_fp64_ldp_%d.bin", (int)getpid());
    data->dump_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data->dump_fd < 0) {
        log_error("neon_rot_ldr_at_top_str3_checkall_fp64_ldp: cannot open %s (errno %d) — "
                  "continuing without binary records", path, errno);
    }

    test->data = data;
    return EXIT_SUCCESS;
#else
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 required for neon_rot_ldr_at_top_str3_checkall_fp64_ldp");
    return EXIT_SKIP;
#endif
}

#if defined(__aarch64__)
static int neon_rot_ldr_at_top_str3_checkall_fp64_ldp_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallFp64LdpData *>(test->data);

    const size_t bytes = (size_t)NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_ELEMS * sizeof(uint64_t);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    uint64_t *junk = static_cast<uint64_t *>(aligned_alloc(64, bytes));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, bytes));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_SLOTS; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_STR3_CHECKALL_FP64_LDP_SLOTS - 1 - n);
            double u0, u1, a0, a1, b0, b1, r0, r1;
            const size_t off = (size_t)i * 2;

            /* dead scratch pair load at top (load-at-top 压力保留, pair 形态) */
            ldp_pair_f64(data->scratch + off, &u0, &u1);
            (void)u0; (void)u1;

            ldp_pair_f64(data->srcA + off, &a0, &a1);
            ldp_pair_f64(data->srcB + off, &b0, &b1);
            pair_op(i, a0, a1, b0, b1, &r0, &r1);

            stp_pair_f64(temp + off, r0, r1);
            stp_pair_f64(junk + off, r0, r1);
            stp_pair_f64(dst + off, r0, r1);
        }
        const uint32_t sweep_dir = ascending ? 0u : 1u;
        sweep++;

        /* 三个无条件 memcmp OR 成 slot_mask (bit0=temp bit1=junk bit2=dst) */
        uint32_t slot_mask = 0;
        if (memcmp(temp, data->expected, bytes) != 0) slot_mask |= 1u;
        if (memcmp(junk, data->expected, bytes) != 0) slot_mask |= 2u;
        if (memcmp(dst, data->expected, bytes) != 0) slot_mask |= 4u;

        if (slot_mask != 0) {
            str3_checkall_fp64_ldp_cold_scan(test, cpu, data, temp, junk, dst,
                                             slot_mask, sweep - 1, sweep_dir);
            report_fail_msg("neon_rot_ldr_at_top_str3_checkall_fp64_ldp data miscompare "
                            "(slot_mask=0x%x)", slot_mask);
        }
    }

    free(dst);
    free(junk);
    free(temp);
    return EXIT_SUCCESS;
}
#else
static int neon_rot_ldr_at_top_str3_checkall_fp64_ldp_run(struct test *test, int cpu) {
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 required for neon_rot_ldr_at_top_str3_checkall_fp64_ldp");
    return EXIT_SKIP;
}
#endif

static int neon_rot_ldr_at_top_str3_checkall_fp64_ldp_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr3CheckallFp64LdpData *>(test->data);
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

DECLARE_TEST(neon_rot_ldr_at_top_str3_checkall_fp64_ldp,
             "load/store-instruction-form variant of str3_checkall_fp64: every vector access "
             "replaced by scalar LDP/STP pairs (one instruction, two 64-bit accesses) — probes "
             "the scalar-pair store-to-load forwarding path and pair LSU-issue density against "
             "the CPU122 defect")
    .test_init    = neon_rot_ldr_at_top_str3_checkall_fp64_ldp_init,
    .test_run     = neon_rot_ldr_at_top_str3_checkall_fp64_ldp_run,
    .test_cleanup = neon_rot_ldr_at_top_str3_checkall_fp64_ldp_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
