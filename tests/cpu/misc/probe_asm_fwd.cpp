/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b probe_asm_fwd_base  / @b probe_asm_fwd_alu
 * @parblock
 * SDC 受控探针组 v2:手写内联 asm 固定寻址,让 base 和 alu 变体的
 * store→load forwarding 骨架字节级同构,唯一差异是 alu 变体在 str 前插一条 eor。
 *
 * 背景(v2,见 docs/CORE179_TRIGGER_SEQUENCE_HARDWARE_PATH_ANALYSIS.md):
 *   v1 探针(probe_alu_fwd)的 alu_inv 变体 0-fail,但 objdump 显示它因 C 层 eor
 *   导致编译器每轮重取 src 指针(多余变量),没对齐 mrn_rmw 触发窗口,0-fail 无信息量。
 *   本 v2 把 eor 放进内联 asm,寻址和 eor 都由 asm 硬编码,编译器不能改变寻址,
 *   确保 base 和 alu 真正只差一条 eor(单变量纯净对比)。
 *
 * 设计(两个独立 test):
 *   - probe_asm_fwd_base: asm 骨架 = load src → add temp_addr → str → ldr(forwarding) → str dst
 *                        无 eor。期望 0-fail(mrn_nuke 基线复刻)。
 *   - probe_asm_fwd_alu: asm 骨架 = load src → add temp_addr → eor(数据ALU) → str → ldr(forwarding) → str dst
 *                        唯一差异: str 前插一条 eor x?,?,x23(0x5A5A...)。看是否触发。
 *
 * 方法论保障:
 *   - 寻址模式复刻 mrn_rmw objdump: add x0,base,idx; str Rx,[x0]; ldr x0,[x0](同址同寄存器背靠背);
 *   - asm 操作数用 =r 写输出,但 store→load 的地址寄存器(base+idx)在 asm 内被 str 写后再被 ldr 读,
 *     须用 early-clobber(=&r)避免地址寄存器被 reuse 导致崩溃(见 [movbe-l1d-variants-schedule-sensitivity]
 *     教训:首版 =r 纯写输出被 asm 内读→early-clobber 违规→全核崩溃,改 =&r 修复);
 *   - eor 的源值来自 load src(寄存器),目标寄存器与 store 输入相同,数据依赖真实不可 DCE;
 *   - buffer/迭代数与 mrn_nuke 一致(1024 元素,8KB temp+8KB dst,per-thread)。
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define PROBE_ASM_COUNT 1024
#define XOR_CONST 0x5A5A5A5A5A5A5A5Aull

struct ProbeAsmData {
    uint64_t *src;
    uint64_t *expected_alu;  /* probe_asm_fwd_alu 的 expected = src ^ XOR_CONST */
};

static int probe_asm_init(struct test *test) {
    auto *data = static_cast<ProbeAsmData *>(malloc(sizeof(ProbeAsmData)));
    data->src = static_cast<uint64_t *>(aligned_alloc(64, PROBE_ASM_COUNT * sizeof(uint64_t)));
    memset_random(data->src, PROBE_ASM_COUNT * sizeof(uint64_t));
    data->expected_alu = static_cast<uint64_t *>(aligned_alloc(64, PROBE_ASM_COUNT * sizeof(uint64_t)));
    for (int i = 0; i < PROBE_ASM_COUNT; i++)
        data->expected_alu[i] = data->src[i] ^ XOR_CONST;
    test->data = data;
    return EXIT_SUCCESS;
}

static int probe_asm_cleanup(struct test *test) {
    auto *data = static_cast<ProbeAsmData *>(test->data);
    if (data) {
        free(data->src);
        free(data->expected_alu);
        free(data);
    }
    return EXIT_SUCCESS;
}

#if defined(__aarch64__)

/*
 * 内联 asm 热循环骨架(复刻 mrn_rmw objdump 的 add;str;ldr 模式):
 *   输入: srcbase(指针), idx, dstbase(指针), tempbase(指针)
 *   1. ldr xval,[srcbase, idx, lsl #3]   ; load src[i]
 *   [alu 变体在此插 eor xval, xval, xconst]
 *   2. add xaddr, tempbase, idx, lsl #3  ; temp 地址
 *   3. str xval, [xaddr]                 ; store temp[i]
 *   4. ldr xret, [xaddr]                 ; load temp[i] (forwarding, 同址同寄存器)
 *   5. add xaddr2, dstbase, idx, lsl #3  ; dst 地址
 *   6. str xret, [xaddr2]                ; store dst[i]
 *
 * 关键:xaddr 是 str 的地址,之后 ldr 又读它(同址同寄存器)→forwarding 背靠背。
 *     用 early-clobber(=&r)标记 xaddr,因它被 str 写(算地址)后被 ldr 读,不能与输入重用。
 */

/* 变体 BASE: 无 eor */
static inline void probe_asm_fwd_base(uint64_t *temp, uint64_t *dst,
                                       const uint64_t *src, uint64_t idx) {
    uint64_t val, ret;
    uint64_t taddr, daddr;
    __asm__ volatile (
        "ldr %[val], [%[srcbase], %[idx], lsl #3]\n\t"   /* 1. load src[i] */
        "add %[taddr], %[tempbase], %[idx], lsl #3\n\t"   /* 2. temp addr (early-clobber: 写于 str 前, 读于 ldr) */
        "str %[val], [%[taddr]]\n\t"                      /* 3. store temp[i] */
        "ldr %[ret], [%[taddr]]\n\t"                      /* 4. load temp[i] forwarding (同址同寄存器) */
        "add %[daddr], %[dstbase], %[idx], lsl #3\n\t"    /* 5. dst addr */
        "str %[ret], [%[daddr]]\n\t"                      /* 6. store dst[i] */
        : [val] "=&r"(val), [ret] "=&r"(ret),
          [taddr] "=&r"(taddr), [daddr] "=&r"(daddr)
        : [srcbase] "r"(src), [tempbase] "r"(temp),
          [dstbase] "r"(dst), [idx] "r"(idx)
        : "memory"
    );
    (void)val;
}

/* 变体 ALU: store 前插 eor (数据 ALU, 唯一差异) */
static inline void probe_asm_fwd_alu(uint64_t *temp, uint64_t *dst,
                                      const uint64_t *src, uint64_t idx,
                                      uint64_t xconst) {
    uint64_t val, ret;
    uint64_t taddr, daddr;
    __asm__ volatile (
        "ldr %[val], [%[srcbase], %[idx], lsl #3]\n\t"   /* 1. load src[i] */
        "eor %[val], %[val], %[xconst]\n\t"              /* 2. 数据 ALU: val ^= 0x5A5A... (唯一差异!) */
        "add %[taddr], %[tempbase], %[idx], lsl #3\n\t"  /* 3. temp addr */
        "str %[val], [%[taddr]]\n\t"                     /* 4. store temp[i] (经 ALU 加工的值) */
        "ldr %[ret], [%[taddr]]\n\t"                     /* 5. load temp[i] forwarding */
        "add %[daddr], %[dstbase], %[idx], lsl #3\n\t"   /* 6. dst addr */
        "str %[ret], [%[daddr]]\n\t"                     /* 7. store dst[i] */
        : [val] "=&r"(val), [ret] "=&r"(ret),
          [taddr] "=&r"(taddr), [daddr] "=&r"(daddr)
        : [srcbase] "r"(src), [tempbase] "r"(temp),
          [dstbase] "r"(dst), [idx] "r"(idx), [xconst] "r"(xconst)
        : "memory"
    );
    (void)val;
}

#elif defined(__x86_64__) || defined(__i386__)
/* x86 fallback: 用 C 模拟 asm 行为(ARM64 为主目标,x86 仅占位) */
static inline void probe_asm_fwd_base(uint64_t *temp, uint64_t *dst,
                                       const uint64_t *src, uint64_t idx) {
    uint64_t val = src[idx];
    temp[idx] = val;
    dst[idx] = temp[idx];
}
static inline void probe_asm_fwd_alu(uint64_t *temp, uint64_t *dst,
                                      const uint64_t *src, uint64_t idx,
                                      uint64_t xconst) {
    uint64_t val = src[idx] ^ xconst;
    temp[idx] = val;
    dst[idx] = temp[idx];
}
#endif

/* ---------- 变体 BASE: 无数据 ALU (期望 0-fail) ---------- */

static int probe_asm_fwd_base_run(struct test *test, int cpu) {
    auto *data = static_cast<ProbeAsmData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, PROBE_ASM_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, PROBE_ASM_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < PROBE_ASM_COUNT; i++) {
            probe_asm_fwd_base(temp, dst, data->src, (uint64_t)i);
        }
        if (memcmp(dst, data->src, PROBE_ASM_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("probe_asm_fwd_base data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

/* ---------- 变体 ALU: store 前数据 ALU eor (看是否触发) ---------- */

static int probe_asm_fwd_alu_run(struct test *test, int cpu) {
    auto *data = static_cast<ProbeAsmData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, PROBE_ASM_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, PROBE_ASM_COUNT * sizeof(uint64_t)));
    const uint64_t xconst = XOR_CONST;

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < PROBE_ASM_COUNT; i++) {
            probe_asm_fwd_alu(temp, dst, data->src, (uint64_t)i, xconst);
        }
        if (memcmp(dst, data->expected_alu, PROBE_ASM_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("probe_asm_fwd_alu data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

/* ---------- 注册 ---------- */

DECLARE_TEST(probe_asm_fwd_base, "SDC probe v2 (asm-pinned): mrn_nuke baseline store->load, no data ALU — expected 0-fail")
    .test_init = probe_asm_init,
    .test_run = probe_asm_fwd_base_run,
    .test_cleanup = probe_asm_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(probe_asm_fwd_alu, "SDC probe v2 (asm-pinned): store value after eor (data ALU) — tests if data-ALU is trigger. ONLY diff vs base = one eor before str")
    .test_init = probe_asm_init,
    .test_run = probe_asm_fwd_alu_run,
    .test_cleanup = probe_asm_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
