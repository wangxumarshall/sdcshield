/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b probe_fwd_base  / @b probe_fwd_alu  / @b probe_fwd_alu_inv
 * @parblock
 * SDC 受控探针组:验证"store 前数据 ALU 加工"是否为 179 SDC 触发开关。
 *
 * 背景(见 docs/CORE179_TRIGGER_SEQUENCE_HARDWARE_PATH_ANALYSIS.md):
 *   22 测试 30min 实测 + objdump 取证推出 2×2 矩阵——触发(movbe/mrn_rmw)全落在
 *   "store 前数据 ALU + 紧邻重载"格;不触发(mrn_nuke/mem_disambig/atomic_seq_cst)
 *   的 store→load 微操作与 mrn_rmw 字节同构(add;str;ldr 同址同寄存器背靠背),
 *   唯一差异是 store 的值未经数据 ALU 加工(原样 store)。
 *   但"数据 ALU"是事后归纳,样本仅 2,需受控探针验证因果。
 *
 * 探针设计(基于 mrn_nuke 骨架,0-fail 基线):
 *   - probe_fwd_base:    mrn_nuke 原样。store data->src[i](原值,无数据 ALU)→ ldr 同址 forwarding。
 *                        期望 0-fail(复现 mrn_nuke 基线)。
 *   - probe_fwd_alu:     在 store 前对原值做 `val ^ 0u`(纯 C,语义不变但强制编译器
 *                        生成 eor 数据 ALU)。store 的值经数据 ALU 加工后被 forwarding 紧邻重载。
 *                        期望:若"数据 ALU + 紧邻重载"是触发开关,则触发;否则 0-fail。
 *   - probe_fwd_alu_inv: 在 store 前对原值做 `val ^ 0x5A5A5A5A5A5A5A5Aull`(真改值,数据 ALU
 *                        加工),store+forwarding 重载后,验证时 reverse xor 还原比对。
 *                        这是"数据 ALU 加工且改值"的真攻击版,排除"^0 被 DCE"的疑虑。
 *
 * 方法论保障(按 [movbe-l1d-variants-schedule-sensitivity]/[movbe-dump-compiler-dce-root-cause]):
 *   - 纯 C `val ^ 0u` 不用 asm nop(避免被当 memory fence 导致指针重载,见 probe_x 教训);
 *   - `^0u` 结果被 store 观察故不可 DCE,但若编译器识 `x^0=x` 折叠则需 probe_fwd_alu_inv 兜底;
 *   - 三个 test 共用同一 store_qword/load_qword 内联 asm,str→ldr 同址 forwarding 骨架字节同构,
 *     只差 store 前是否插入数据 ALU;
 *   - buffer 大小/共享性/迭代数与 mrn_nuke 完全一致(1024 元素,8KB temp+8KB dst,per-thread)。
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define PROBE_COUNT 1024

struct ProbeData {
    uint64_t *src;
    uint64_t *expected_inv;  // probe_fwd_alu_inv 的 expected = src ^ 0x5A5A...
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

#elif defined(__x86_64__) || defined(__i386__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("movq %1, %0" : "=m"(*reinterpret_cast<uint64_t*>(addr)) : "r"(val) : "memory");
}

static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("movq %1, %0" : "=r"(res) : "m"(*reinterpret_cast<const uint64_t*>(addr)) : "memory");
    return res;
}

#endif

/* ---------- 公共 init/cleanup ---------- */

static int probe_fwd_init(struct test *test) {
    auto *data = static_cast<ProbeData *>(malloc(sizeof(ProbeData)));
    data->src = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));
    memset_random(data->src, PROBE_COUNT * sizeof(uint64_t));
    data->expected_inv = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));
    for (int i = 0; i < PROBE_COUNT; i++)
        data->expected_inv[i] = data->src[i] ^ 0x5A5A5A5A5A5A5A5Aull;
    test->data = data;
    return EXIT_SUCCESS;
}

static int probe_fwd_cleanup(struct test *test) {
    auto *data = static_cast<ProbeData *>(test->data);
    if (data) {
        free(data->src);
        free(data->expected_inv);
        free(data);
    }
    return EXIT_SUCCESS;
}

/* ---------- 变体 A: probe_fwd_base (mrn_nuke 原样基线, 无数据 ALU) ---------- */

static int probe_fwd_base_run(struct test *test, int cpu) {
    auto *data = static_cast<ProbeData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < PROBE_COUNT; i++) {
            /* 基线: store 原值, 无数据 ALU 加工 */
            store_qword(&temp[i], data->src[i]);
            uint64_t val = load_qword(&temp[i]);
            store_qword(&dst[i], val);
        }
        if (memcmp(dst, data->src, PROBE_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("probe_fwd_base data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

/* ---------- 变体 B: probe_fwd_alu (store 前数据 ALU: val ^ 0, 语义不变) ---------- */

static int probe_fwd_alu_run(struct test *test, int cpu) {
    auto *data = static_cast<ProbeData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < PROBE_COUNT; i++) {
            /* 数据 ALU: val ^ 0 (强制 eor, 结果=原值, 被 store 观察) */
            uint64_t val = data->src[i] ^ 0u;
            store_qword(&temp[i], val);
            uint64_t reloaded = load_qword(&temp[i]);
            store_qword(&dst[i], reloaded);
        }
        if (memcmp(dst, data->src, PROBE_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("probe_fwd_alu data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

/* ---------- 变体 C: probe_fwd_alu_inv (store 前数据 ALU 且真改值: val ^ 0x5A5A...) ---------- */

static int probe_fwd_alu_inv_run(struct test *test, int cpu) {
    auto *data = static_cast<ProbeData *>(test->data);
    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, PROBE_COUNT * sizeof(uint64_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < PROBE_COUNT; i++) {
            /* 数据 ALU 且真改值: val ^ 0x5A5A... (eor, 结果≠原值) */
            uint64_t val = data->src[i] ^ 0x5A5A5A5A5A5A5A5Aull;
            store_qword(&temp[i], val);
            uint64_t reloaded = load_qword(&temp[i]);
            store_qword(&dst[i], reloaded);
        }
        /* expected_inv[i] = src[i] ^ 0x5A5A..., 与 dst 比对 */
        if (memcmp(dst, data->expected_inv, PROBE_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("probe_fwd_alu_inv data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

/* ---------- 注册 ---------- */

DECLARE_TEST(probe_fwd_base, "SDC probe: mrn_nuke baseline (store raw value, no data ALU) — expected 0-fail")
    .test_init = probe_fwd_init,
    .test_run = probe_fwd_base_run,
    .test_cleanup = probe_fwd_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(probe_fwd_alu, "SDC probe: store value after data ALU (val^0, semantic no-op but forces eor) — tests data-ALU trigger")
    .test_init = probe_fwd_init,
    .test_run = probe_fwd_alu_run,
    .test_cleanup = probe_fwd_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(probe_fwd_alu_inv, "SDC probe: store value after data ALU with real value change (val^0x5A5A) — anti-DCE attack version")
    .test_init = probe_fwd_init,
    .test_run = probe_fwd_alu_inv_run,
    .test_cleanup = probe_fwd_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
