/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_l1d_sve
 * @test @b movbe_l2_sve
 * @parblock
 * SVE ports of movbe_l1d / movbe_l2 (reload-path localization probes for the
 * core-179 SDC; source: missing_testcases_20261009/tests/cpu/misc/
 * movbe_l1d_vs_l2.cpp).
 *
 * Hypothesis under test: the defect lies in the per-core load data path
 * (L1D read array -> load port -> register). Each batch of svcntw() u32
 * elements runs the movbe round-trip whose TESTED load is the *reload of
 * the input batch from the heap*:
 *
 *   in1      = svld1(input)                  load 1 (heap, direct)
 *   vswapped = svtbl(in1, [3,2,1,0])        REV 1
 *   svst1(swapped, vswapped)                STR
 *   [movbe_l2_sve only: evict input lines]  dc civac + dsb sy
 *   in2      = ldr z (input)                RELOAD — the tested load
 *   vround   = svtbl(vswapped, idx)         REV 2 (register side, like the
 *                                           scalar original's val = bswap(val))
 *   cmp vround != in2                       bswap∘bswap = id, so this is
 *                                           exactly the original's
 *                                           "bswap(bswap(in1)) != reload"
 *
 *   - movbe_l1d_sve: the reload hits L1D. Expected on a defective core:
 *     SDC triggers.
 *   - movbe_l2_sve: every cache line covering the input batch is evicted
 *     before the reload, so it is served from L2/L3. Expected (if the
 *     hypothesis holds): SDC disappears.
 *
 * The reload is asm-guaranteed (volatile ldr z + "memory" clobber): in2
 * reads the same address as in1, so a plain svld1 could be CSE'd away or
 * hoisted above the swapped-store — the asm boundary keeps the reload real
 * and in place (the u8x16 original documents the same compiler-DCE hazard
 * for intrinsic reloads).
 *
 * Two SEPARATE tests (not one with a branch) so neither hot loop contains
 * a regime-selection conditional (any hot-loop conditional can silence the
 * timing-sensitive defect). The two run bodies are separate template
 * instantiations; the evict is compile-time dead in the l1d instantiation.
 *
 * Deviations from the scalar originals (documented):
 *   - bswap runs on SVE lanes via the svtbl [3,2,1,0] byte index table
 *     (bit-exact vs __builtin_bswap32, 50k-vector verified — movbe_sve
 *     precedent);
 *   - the evict runs once per cache line covering the batch instead of
 *     once per 4-byte element (the original re-evicts the same line 16x);
 *     every line is flushed before the reload either way — semantically
 *     equivalent, and the batch reload misses L1D exactly as intended;
 *   - the comparison is vector-wide (svcmpne + svptest_any) with a cold
 *     per-element rescan for logging, instead of per-element scalar cmp.
 * @endparblock
 */

#include "sandstone.h"
#include "sdc_stress_config.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)
#include <arm_sve.h>

#ifndef SVE_MAX_VECTOR_BYTES
/* GCC 12 的 arm_sve.h 不提供该 ACLE 宏 (movbe_slf_sweep 同款自定义):
 * SVE 架构最大 VL 2048bit = 256 字节 */
#define SVE_MAX_VECTOR_BYTES 256
#endif
#include <device/cpu/arm64/arm64_vl.h>
#endif

#define MOVBE_L1D_SVE_BUFFER_SIZE (1 << 14)

/* 元素数 count (u32 元素) — 运行时可配置, 统一优先级链 (init 由
 * movbe_l1d_sve / movbe_l2_sve 两测试共享, 经 test->id 各自独立):
 *   1. -O movbe_l1d_sve.count=N / movbe_l2_sve.count=N (或配置文件同名条目)
 *   2. 环境变量 SDC_MOVBE_L1D_SVE_COUNT / SDC_MOVBE_L2_SVE_COUNT
 *   3. 设计默认 1<<14 (16384 元素 = 64KB/缓冲)
 * （core179 诊断探针: 未配置时默认 = 原硬编码设计几何, 不做内存
 *   自适应 — 缓冲几何本身是激发参数; 显式配置 clamp 到
 *   [1<<14, 1<<22]。非 16 的倍数 log_warning + 向下取整, 保持 u32
 *   缓冲字节数的 64B cache-line 对齐。） */
static size_t movbe_l1d_sve_count(struct test *test)
{
    const int64_t dflt = MOVBE_L1D_SVE_BUFFER_SIZE;
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

struct MovbeL1dL2SveData {
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
static const uint8_t g_bswap_lut[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
static uint32_t movbe_golden_bswap32(uint32_t x)
{
    return ((uint32_t)g_bswap_lut[x & 0xff] << 24) |
           ((uint32_t)g_bswap_lut[(x >> 8) & 0xff] << 16) |
           ((uint32_t)g_bswap_lut[(x >> 16) & 0xff] << 8) |
           (uint32_t)g_bswap_lut[x >> 24];
}

static int movbe_l1d_sve_init(struct test *test)
{
#if defined(__aarch64__)
    /* All-VL Phase 2 P6: SVE capability + 128..1024-bit VL range
     * gate in one framework-layer call (device/cpu/arm64/arm64_vl.h). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
#endif
    auto *data = static_cast<MovbeL1dL2SveData *>(malloc(sizeof(MovbeL1dL2SveData)));
    data->count = movbe_l1d_sve_count(test);
    data->input = static_cast<uint32_t *>(aligned_alloc_safe(64, data->count * sizeof(uint32_t)));
    data->swapped = static_cast<uint32_t *>(aligned_alloc_safe(64, data->count * sizeof(uint32_t)));
    data->expected_swapped = static_cast<uint32_t *>(aligned_alloc_safe(64, data->count * sizeof(uint32_t)));

    for (size_t i = 0; i < data->count; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the svtbl under test). */
    for (size_t i = 0; i < data->count; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_l1d_sve_cleanup(struct test *test)
{
    auto *data = static_cast<MovbeL1dL2SveData *>(test->data);
    if (data) {
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

/* Shared miscompare logger (cold branch only) — same field set as the
 * scalar originals. */
static void log_miscompare_sve(const char *tag, unsigned i,
                               uint32_t inputv, uint32_t golden,
                               uint32_t actual, uint32_t xorv)
{
    unsigned char *ib = (unsigned char *)&inputv;
    unsigned char *gb = (unsigned char *)&golden;
    unsigned char *ab = (unsigned char *)&actual;
    unsigned char *xb = (unsigned char *)&xorv;
    log_error("%s: Round-trip failed at index %u: "
              "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
              "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
              "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
              "xor=%02X%02X%02X%02X",
              tag, i, inputv, golden, actual, xorv,
              ib[3], ib[2], ib[1], ib[0],
              gb[3], gb[2], gb[1], gb[0],
              ab[3], ab[2], ab[1], ab[0],
              xb[3], xb[2], xb[1], xb[0]);
    report_fail_msg("%s: Round-trip failed at index %u", tag, i);
}

#if defined(__aarch64__)

/* asm 保证的被测 reload: in2 与 in1 同地址, 纯 svld1 会被 CSE/上提;
 * volatile + "memory" clobber 同时钉住它相对 swapped-store 的位置 */
static inline svuint8_t load_vec_u8_sve(const void *addr)
{
    svuint8_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* Evict every cache line covering [addr, addr+bytes): dc civac + dsb sy
 * per line. The scalar original evicts per 4-byte element (same line
 * repeatedly); one eviction per line leaves the same end state — all
 * lines flushed before the reload. */
static inline void evict_l1d_range_sve(const void *addr, size_t bytes)
{
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)63;
    uintptr_t end = (uintptr_t)addr + bytes;
    for (uintptr_t a = start; a < end; a += 64) {
        __asm__ volatile("dc civac, %0\n\tdsb sy\n\t" :: "r"(a) : "memory");
    }
}

/* Shared round-trip body. evict != 0 inserts the L1D eviction between the
 * swapped-store and the input reload (movbe_l2_sve); the template
 * parameter is compile-time constant, so each instantiation's hot loop
 * carries no regime conditional — mirroring the originals' two-separate-
 * tests discipline. */
template<int evict> static int movbe_l1d_sve_run_impl(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<MovbeL1dL2SveData *>(test->data);

    /* bswap32 的 svtbl 索引: 每元素 [3,2,1,0] (movbe_sve 构造) */
    static const uint8_t idx4[4] = {3, 2, 1, 0};
    const int lanes = svcntw();
    /* 索引按运行时 VL 定容: svcntw()*4 = svcntb() ≤ 256 = 架构最大 VL
     * (movbe_slf_sweep VL-portable 范式); 原 64 字节 cap 在 lanes>16 时
     * 使尾部 lane 索引为垃圾 → 确定性误报 + 失败路径栈越界 */
    uint8_t vidx_bytes[SVE_MAX_VECTOR_BYTES];
    for (int i = 0; i < lanes; ++i)
        for (int b = 0; b < 4; ++b)
            vidx_bytes[i * 4 + b] = (uint8_t)(i * 4 + idx4[b]);
    svbool_t pg_batch = svwhilelt_b8((uint64_t)0, (uint64_t)lanes * 4);
    svuint8_t vidx = svld1_u8(pg_batch, vidx_bytes);

    uint8_t round_bytes[SVE_MAX_VECTOR_BYTES];

    TEST_LOOP(test, 1 << 13) {
        for (size_t base = 0; base < data->count; base += lanes) {
            int n = (data->count - base < (size_t)lanes)
                        ? (int)(data->count - base) : lanes;
            svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)(n * 4));

            /* 加载 1 (堆直读) + REV 1 + 堆 store 到 swapped */
            svuint8_t in1 = svld1_u8(pg, (const uint8_t *)(data->input + base));
            svuint8_t vswapped = svtbl_u8(in1, vidx);
            svst1_u8(pg, (uint8_t *)(data->swapped + base), vswapped);

            /* movbe_l2_sve: reload 前冲刷 input 批覆盖的全部 L1D 行 */
            if (evict)
                evict_l1d_range_sve(data->input + base, (size_t)n * 4);

            /* 被测加载: input 的堆 reload (asm 保证) */
            svuint8_t in2 = load_vec_u8_sve(data->input + base);

            /* REV 2 (寄存器侧, 同原版 val = bswap(val)) + 向量比对:
             * svtbl∘svtbl = 恒等 ⇒ vround == in1, 谓词等价于原版
             * "bswap(bswap(in1)) != reload" */
            svuint8_t vround = svtbl_u8(vswapped, vidx);
            if (svptest_any(pg, svcmpne_u8(pg, vround, in2))) {
                /* 冷路径: 落栈逐元素复核 + 日志。复核必须用被测 reload(in2)
                 * 而非 input 的又一次重读: 向量级 svcmpne 检出的正是
                 * "寄存器结果 != 被测 reload", 若冷路径重读 input 会引入
                 * 第三个值, 可能漏报被检出的 lane 或错报索引。
                 * golden = 被测 reload(in2), actual = 双 bswap 寄存器值
                 * (与原版 scalars 字段同源)。 */
                svst1_u8(pg, round_bytes, vround);
                uint8_t in2_bytes[SVE_MAX_VECTOR_BYTES];
                svst1_u8(pg, in2_bytes, in2);
                uint32_t round[SVE_MAX_VECTOR_BYTES / 4];
                uint32_t reload[SVE_MAX_VECTOR_BYTES / 4];
                memcpy(round, round_bytes, (size_t)n * 4);
                memcpy(reload, in2_bytes, (size_t)n * 4);
                for (int k = 0; k < n; ++k) {
                    if (round[k] != reload[k]) {
                        log_miscompare_sve(evict ? "movbe_l2_sve" : "movbe_l1d_sve",
                                           (unsigned)(base + k), reload[k], reload[k],
                                           round[k], reload[k] ^ round[k]);
                    }
                }
            }
        }
    }

    /* issue #374: post-loop verification of the stored intermediate. The
     * vector hot loop and the cold-path re-check above (which deliberately
     * uses the tested reload in2 as its reference) are unchanged; every
     * iteration stores the same svtbl(input) bytes to swapped, so the final
     * content must equal the init-time scalar LUT golden. A persistent svtbl
     * index-table mispermutation (symmetric fault: the wrong table is its
     * own inverse, so svtbl∘svtbl == id and the vector compare passes) is
     * caught here byte-exactly. */
    for (size_t i = 0; i < data->count; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_miscompare_sve(evict ? "movbe_l2_sve" : "movbe_l1d_sve",
                               (unsigned)i, data->input[i], data->expected_swapped[i],
                               data->swapped[i],
                               data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("%s: swapped buffer != independent golden at index %u",
                            evict ? "movbe_l2_sve" : "movbe_l1d_sve", (unsigned)i);
        }
    }

    return EXIT_SUCCESS;
}

static int movbe_l1d_sve_run(struct test *test, int cpu)
{
    return movbe_l1d_sve_run_impl<0>(test, cpu);
}

static int movbe_l2_sve_run(struct test *test, int cpu)
{
    return movbe_l1d_sve_run_impl<1>(test, cpu);
}

#else

static int movbe_l1d_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_l1d_sve");
    return EXIT_SKIP;
}

static int movbe_l2_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_l2_sve");
    return EXIT_SKIP;
}

#endif

DECLARE_TEST(movbe_l1d_sve,
             "SVE movbe reload-path probe (L1D-hit): asm-guaranteed svld1 heap reload of the "
             "input batch hits L1D (port of movbe_l1d)")
    .test_init = movbe_l1d_sve_init,
    .test_run = movbe_l1d_sve_run,
    .test_cleanup = movbe_l1d_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST

DECLARE_TEST(movbe_l2_sve,
             "SVE movbe reload-path probe (L2-reload): evict input lines (dc civac + dsb) before "
             "the asm-guaranteed svld1 reload — if SDC is L1D-path, should disappear (port of movbe_l2)")
    .test_init = movbe_l1d_sve_init,
    .test_run = movbe_l2_sve_run,
    .test_cleanup = movbe_l1d_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
