/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_sve
 * @parblock
 * SVE port of movbe: byte-swap followed by another byte-swap restoring
 * the original order, on SVE vector lanes. Each 32-bit element's bytes
 * are permuted by svtbl with the [3,2,1,0] index table (bit-exact vs
 * __builtin_bswap32, 50k random vectors verified); the swapped value is
 * stored and reloaded through svst1/svld1 (the vector store->load path
 * that mirrors the scalar trigger recipe's reload). Input is random
 * (framework RNG at init), reused per iteration exactly as the original.
 * @endparblock
 */

#include "sandstone.h"
#include "sdc_stress_config.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_sve.h>

#ifndef SVE_MAX_VECTOR_BYTES
/* GCC 12 的 arm_sve.h 不提供该 ACLE 宏 (movbe_dump_golden 同款自定义):
 * SVE 架构最大 VL 2048bit = 256 字节 */
#define SVE_MAX_VECTOR_BYTES 256
#endif
#include <device/cpu/arm64/arm64_vl.h>
#endif

#define MOVBE_BUFFER_SIZE (1 << 14)

/* 元素数 count (u32 元素) — 运行时可配置, 统一优先级链:
 *   1. -O movbe_sve.count=N / 配置文件同名条目
 *   2. 环境变量 SDC_MOVBE_SVE_COUNT
 *   3. 设计默认 1<<14 (16384 元素 = 64KB/缓冲)
 * （core179 诊断探针: 未配置时默认 = 原硬编码设计几何, 不做内存
 *   自适应 — 缓冲几何本身是激发参数; 显式配置 clamp 到
 *   [1<<14, 1<<22]。非 16 的倍数 log_warning + 向下取整, 保持 u32
 *   缓冲字节数的 64B cache-line 对齐。） */
static size_t movbe_sve_count(struct test *test)
{
    const int64_t dflt = MOVBE_BUFFER_SIZE;
    const int64_t hi = 1ll << 22;
    int64_t v = sdc_knob_int_clamped(test, "count", 0, dflt, hi);
    if (v == 0 && sdc_env_knob_int(test->id, "count", &v)
        && (v < dflt || v > hi)) {
        int64_t c = (v < dflt) ? dflt : hi;
        log_warning("%s: count=%ld 越界 (有效 %ld..%ld), 已 clamp 为 %ld",
                    test->id, (long)v, (long)dflt, (long)hi, (long)c);
        v = c;
    }
    if (v <= 0)
        v = dflt;
    if (v % 16 != 0) {
        log_warning("%s: count=%ld 非 16 的倍数, 向下取整为 %ld (保持 64B 缓冲对齐)",
                    test->id, (long)v, (long)(v - v % 16));
        v -= v % 16;
    }
    return (size_t)v;
}

struct movbe_sve_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
    size_t count;
};

/* issue #374: identity LUT for the scalar-independent byte-swap golden.
 * The table loads block GCC's bswap-idiom recognizer from folding the
 * shift/or reassembly back into a REV instruction, and the scalar loop form
 * keeps the golden off the SVE TBL/REV units under test -- so a persistent
 * svtbl mispermutation can never corrupt golden and actual identically.
 * Pinned by scripts/check-bswap-golden.sh (zero rev/rev16/rev32 and zero
 * vector-ALU inside the init symbol). */
static uint8_t g_bswap_lut[256];
static void movbe_golden_lut_init(void)
{
    for (int b = 0; b < 256; ++b)
        g_bswap_lut[b] = (uint8_t)b;
}
static uint32_t movbe_golden_bswap32(uint32_t x)
{
    return ((uint32_t)g_bswap_lut[x & 0xff] << 24) |
           ((uint32_t)g_bswap_lut[(x >> 8) & 0xff] << 16) |
           ((uint32_t)g_bswap_lut[(x >> 16) & 0xff] << 8) |
           (uint32_t)g_bswap_lut[x >> 24];
}

static int movbe_sve_init(struct test *test)
{
#if defined(__aarch64__)
    /* All-VL Phase 2 P6: SVE capability + 128..1024-bit VL range
     * gate in one framework-layer call (device/cpu/arm64/arm64_vl.h). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
#endif
    auto *data = (movbe_sve_data *)malloc(sizeof(movbe_sve_data));
    data->count = movbe_sve_count(test);
    data->input = (uint32_t *)aligned_alloc_safe(64, data->count * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, data->count * sizeof(uint32_t));
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, data->count * sizeof(uint32_t));

    for (size_t i = 0; i < data->count; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the svtbl under test).
     * Byte-level bswap32 is VL-independent, so a fixed-size scalar golden
     * covers every VL tier. */
    movbe_golden_lut_init();
    for (size_t i = 0; i < data->count; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    test->data = data;
    return EXIT_SUCCESS;
}

#if defined(__aarch64__)
/* 冷路径报告 (noinline): 让热循环里被比较的两个值在 cmp 后即死
 * (本函数自行按地址重载), 基循环指令序列与加转储前保持一致 */
[[gnu::noinline]] static void movbe_sve_report_mismatch(uint64_t idx,
                                                        const uint32_t *golden,
                                                        const uint32_t *actual)
{
    report_fail_msg("MovBE-SVE: Round-trip failed at index %u: "
                    "golden=0x%08x actual=0x%08x xor=0x%08x",
                    (unsigned)idx, (unsigned)*golden, (unsigned)*actual,
                    (unsigned)(*golden ^ *actual));
}

static int movbe_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = (movbe_sve_data *)test->data;

    /* bswap32 的 svtbl 索引: 每元素 [3,2,1,0] */
    static const uint8_t idx4[4] = {3, 2, 1, 0};

    const int lanes = svcntw();
    /* 索引按运行时 VL 定容: svcntw()*4 = svcntb() ≤ 256 = 架构最大 VL
     * (movbe_dump_golden/slf_sweep VL-portable 范式); 原 64 字节 cap 在
     * lanes>16 时使尾部 lane 索引为垃圾 → 确定性误报 + 失败路径栈越界 */
    uint8_t vidx_bytes[SVE_MAX_VECTOR_BYTES];
    for (int i = 0; i < lanes; ++i)
        for (int b = 0; b < 4; ++b)
            vidx_bytes[i * 4 + b] = (uint8_t)(i * 4 + idx4[b]);
    svbool_t pg_batch = svwhilelt_b8((uint64_t)0, (uint64_t)lanes * 4);
    svuint8_t vidx = svld1_u8(pg_batch, vidx_bytes);

    TEST_LOOP(test, 1 << 13) {
        for (size_t base = 0; base < data->count; base += lanes) {
            int n = (data->count - base < (size_t)lanes) ? (int)(data->count - base) : lanes;
            svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)(n * 4));

            /* 直接堆访存 (无栈中转): 修复版 —— 原版被测路径是
             * 堆 store(swapped) 后紧跟堆 reload, 栈中转会改变被测微操作序列 */
            uint8_t in_bytes[SVE_MAX_VECTOR_BYTES];
            memcpy(in_bytes, data->input + base, n * 4);
            svuint8_t vswapped = svtbl_u8(svld1_u8(pg, in_bytes), vidx);

            /* ★ 堆 store: svst1 直接写 swapped 堆缓冲区 */
            svst1_u8(pg, (uint8_t *)(data->swapped + base), vswapped);

            /* ★ 堆 reload: svld1 直接从 swapped 读回 (被测的 store→load 转发路径) */
            svuint8_t reloaded = svld1_u8(pg, (const uint8_t *)(data->swapped + base));

            /* 再交换一次还原 (同一索引表, svtbl 可逆) */
            svuint8_t vrestored = svtbl_u8(reloaded, vidx);
            uint8_t out_bytes[SVE_MAX_VECTOR_BYTES];
            svst1_u8(pg, out_bytes, vrestored);

            /* 校验: 还原值 == 输入值 */
            uint32_t restored[SVE_MAX_VECTOR_BYTES / 4];
            memcpy(restored, out_bytes, n * 4);
            for (int i = 0; i < n; ++i) {
                if (restored[i] != data->input[base + i]) {
                    movbe_sve_report_mismatch(base + i, &data->input[base + i],
                                              &restored[i]);
                }
            }
        }
    }

    /* issue #374: post-loop verification of the stored intermediate. The
     * vector hot loop above is unchanged; every iteration stores the same
     * svtbl(input) bytes to swapped, so the final content must equal the
     * init-time scalar LUT golden. A persistent svtbl index-table
     * mispermutation (symmetric fault: the wrong table is its own inverse,
     * so svtbl∘svtbl == id and the restore compare passes) is caught here
     * byte-exactly. */
    for (size_t i = 0; i < data->count; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-SVE: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      (unsigned)i, data->input[i], data->expected_swapped[i],
                      data->swapped[i], data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("MovBE-SVE: swapped buffer != independent golden at index %u",
                            (unsigned)i);
        }
    }

    return EXIT_SUCCESS;
}
#else
static int movbe_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_sve");
    return EXIT_SKIP;
}
#endif

static int movbe_sve_cleanup(struct test *test)
{
    auto *data = (movbe_sve_data *)test->data;
    if (data) {
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_sve,
             "SVE byte-swap round-trip: svtbl [3,2,1,0] permutation + vector "
             "store/reload on svst1/svld1 lanes (port of movbe)")
    .test_init = movbe_sve_init,
    .test_run = movbe_sve_run,
    .test_cleanup = movbe_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
