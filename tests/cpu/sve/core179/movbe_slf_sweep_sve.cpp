/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_slf_sweep_sve
 * @parblock
 * Multi-width SLF sweep probe (SVE), extending movbe_dump_probe_c_sve
 * (the hunt-v3 core-122 trigger recipe, kept untouched as evidence).
 * Each thread keeps a private copy of the input, the bytes are reversed
 * within each element by svtbl and stored BACK to the private copy's
 * position, then immediately reloaded from there through svst1/svld1
 * (the vector store->load-forwarding path), restored by a second svtbl
 * (the index table is an involution) and compared bit-exact against the
 * read-only golden buffer. Three improvements over the original probe:
 * (A) multi-width — every pass sweeps three element widths (16-bit
 * [1,0], 32-bit [3,2,1,0], 64-bit [7,6,5,4,3,2,1,0]) through the unified
 * index formula (p/W)*W + (W-1-(p%W)); (B) VL-portable — all stack
 * buffers are sized for the SVE architectural maximum VL of 2048 bits
 * (256 bytes) and the walk uses svcntb() batches with svwhilelt tails,
 * so the test is correct at ANY vector length (the original probe's
 * 16-element stack arrays exactly fill at VL=512, svcntw()=16, and
 * overflow from the first legal VL above that, 640 bits, on);
 * (C) no dead allocations —
 * test->data holds only the golden buffer, cleanup frees everything it
 * tracks. The init self-check permutes 50k random values per width
 * through the same svtbl mechanism and compares bit-exact against
 * __builtin_bswap16/32/64 (fail-closed, family precedent). Input is
 * random (framework RNG at init), reused per iteration exactly as the
 * family.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(__aarch64__)
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

/* 64KB 扫描缓冲 (16384 u32, 按 u32 填充; 运行时按字节扫描 — 与元素宽度解耦)。
 * 必须是 16 的倍数: 否则 svwhilelt 尾批会截断在元素中间, 双 svtbl 对合读到
 * 未定义车道 → 假失败 (今日所有合法 VL 下尾批不可达, 此处纵深防御) */
#define SLF_SWEEP_BUFFER_BYTES (64 * 1024)
static_assert(SLF_SWEEP_BUFFER_BYTES % 16 == 0,
              "tail batches must stay element-aligned for W in {2,4,8}");
/* SVE 架构最大 VL = 2048bit = 256 字节: 所有栈缓冲按此定容 (VL 可移植) */
#define SVE_MAX_VECTOR_BYTES 256
/* init 自检: 每种宽度的随机向量数 (家族 50k 验证先例) */
#define SLF_SWEEP_SELFCHECK_VECS 50000

struct movbe_slf_sweep_sve_data {
    /* 改进 C: 唯一必要字段 — init 填充后只读的 golden 缓冲 */
    uint8_t *golden;
};

#if defined(__aarch64__)
/* 统一索引表: 元素宽 W 字节、字节位 p 的表项 = (p/W)*W + (W-1-(p%W))
 * (元素内字节反转, 对合 — W=4 时即 probe-c 的 [3,2,1,0]) */
static void slf_sweep_build_index_table(uint8_t *vidx_bytes, uint64_t vbytes, int width)
{
    for (uint64_t p = 0; p < vbytes; ++p)
        vidx_bytes[p] = (uint8_t)((p / (uint64_t)width) * (uint64_t)width
                                  + ((uint64_t)width - 1 - (p % (uint64_t)width)));
}

/* dump-on-failure + report_fail_msg (noreturn): index/width/golden/actual/xor + 字节分解 */
static void slf_sweep_report_fail(uint64_t elem_index, uint64_t byte_offset, int width,
                                  const uint8_t *golden, const uint8_t *actual)
{
    uint64_t golden_v = 0, actual_v = 0;
    memcpy(&golden_v, golden, (size_t)width);
    memcpy(&actual_v, actual, (size_t)width);
    uint64_t xor_v = golden_v ^ actual_v;
    char golden_hex[2 * 8 + 1], actual_hex[2 * 8 + 1], xor_hex[2 * 8 + 1];
    for (int i = 0; i < width; ++i) {
        /* MSB 先行 (家族 bytes[b3 b2 b1 b0] 约定) */
        snprintf(golden_hex + 2 * i, 3, "%02X", golden[width - 1 - i]);
        snprintf(actual_hex + 2 * i, 3, "%02X", actual[width - 1 - i]);
        snprintf(xor_hex + 2 * i, 3, "%02X",
                 (uint8_t)(golden[width - 1 - i] ^ actual[width - 1 - i]));
    }
    log_error("MovBE-SLF-sweep: round-trip failed at element %llu (byte offset %llu, "
              "width %d): golden=0x%llX actual=0x%llX xor=0x%llX | bytes[b%d..b0] "
              "golden=%s actual=%s xor=%s",
              (unsigned long long)elem_index, (unsigned long long)byte_offset, width,
              (unsigned long long)golden_v, (unsigned long long)actual_v,
              (unsigned long long)xor_v, width - 1, golden_hex, actual_hex, xor_hex);
    report_fail_msg("MovBE-SLF-sweep: round-trip failed at element %llu "
                    "(byte offset %llu, width %d)",
                    (unsigned long long)elem_index, (unsigned long long)byte_offset, width);
}

/* init 自检 (fail-closed): 每种宽度 N 个随机值过 svtbl 机制 (栈缓冲),
 * 逐位对照 __builtin_bswap16/32/64 — 任一不符即 fail init (绝不静默 SUCCESS) */
static void slf_sweep_selfcheck(int width)
{
    const uint64_t vbytes = (uint64_t)svcntb();
    uint8_t vidx_bytes[SVE_MAX_VECTOR_BYTES];
    slf_sweep_build_index_table(vidx_bytes, vbytes, width);
    svbool_t pg_full = svwhilelt_b8((uint64_t)0, vbytes);
    svuint8_t vidx = svld1_u8(pg_full, vidx_bytes);

    uint8_t src[SVE_MAX_VECTOR_BYTES];
    uint8_t dst[SVE_MAX_VECTOR_BYTES];
    uint64_t checked = 0;
    while (checked < SLF_SWEEP_SELFCHECK_VECS) {
        uint64_t count = vbytes / (uint64_t)width;
        if (count > (uint64_t)SLF_SWEEP_SELFCHECK_VECS - checked)
            count = (uint64_t)SLF_SWEEP_SELFCHECK_VECS - checked;
        uint64_t nbytes = count * (uint64_t)width;

        for (uint64_t e = 0; e < count; ++e) {
            if (width == 2) {
                uint16_t v = (uint16_t)random32();
                memcpy(src + e * 2, &v, 2);
            } else if (width == 4) {
                uint32_t v = random32();
                memcpy(src + e * 4, &v, 4);
            } else {
                uint64_t v = random64();
                memcpy(src + e * 8, &v, 8);
            }
        }

        svbool_t pg = svwhilelt_b8((uint64_t)0, nbytes);
        svuint8_t permuted = svtbl_u8(svld1_u8(pg, src), vidx);
        svst1_u8(pg, dst, permuted);

        for (uint64_t e = 0; e < count; ++e) {
            uint8_t expected[8];
            if (width == 2) {
                uint16_t v, x;
                memcpy(&v, src + e * 2, 2);
                x = __builtin_bswap16(v);
                memcpy(expected, &x, 2);
            } else if (width == 4) {
                uint32_t v, x;
                memcpy(&v, src + e * 4, 4);
                x = __builtin_bswap32(v);
                memcpy(expected, &x, 4);
            } else {
                uint64_t v, x;
                memcpy(&v, src + e * 8, 8);
                x = __builtin_bswap64(v);
                memcpy(expected, &x, 8);
            }
            if (memcmp(dst + e * (uint64_t)width, expected, (size_t)width) != 0) {
                log_error("MovBE-SLF-sweep: init self-check failed at width %d, "
                          "vector %llu: svtbl permutation != __builtin_bswap%d",
                          width, (unsigned long long)(checked + e), width * 8);
                report_fail_msg("movbe_slf_sweep_sve: init self-check failed "
                                "(width %d, vector %llu)", width,
                                (unsigned long long)(checked + e));
            }
        }
        checked += count;
    }
}
#endif

static int movbe_slf_sweep_sve_init(struct test *test)
{
#if defined(__aarch64__)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for movbe_slf_sweep_sve");
        return EXIT_SKIP;
    }
#endif
    auto *data = (movbe_slf_sweep_sve_data *)malloc(sizeof(movbe_slf_sweep_sve_data));
    data->golden = (uint8_t *)aligned_alloc_safe(64, SLF_SWEEP_BUFFER_BYTES);

    /* 64KB 按 u32 填充随机值; 运行时按字节扫描, 三种宽度共用同一 golden */
    uint32_t *fill = (uint32_t *)data->golden;
    for (size_t i = 0; i < SLF_SWEEP_BUFFER_BYTES / sizeof(uint32_t); ++i)
        fill[i] = random32();

#if defined(__aarch64__)
    /* init 自检: 16/32/64-bit 三种宽度 (改进 A 的机制正确性) */
    slf_sweep_selfcheck(2);
    slf_sweep_selfcheck(4);
    slf_sweep_selfcheck(8);
#endif

    test->data = data;
    return EXIT_SUCCESS;
}

#if defined(__aarch64__)
static int movbe_slf_sweep_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = (movbe_slf_sweep_sve_data *)test->data;

    const uint64_t total = SLF_SWEEP_BUFFER_BYTES;
    const uint64_t vbytes = (uint64_t)svcntb();
    /* SVE 架构保证 VL 是 128bit 的倍数 → vbytes 是 16 的倍数 → 对 W∈{2,4,8}
     * 每批 (含 svwhilelt 尾批) 恒元素对齐。256 字节定容封死原探针的栈溢出:
     * 其 16 元素栈容量在 VL=512 (svcntw()=16) 恰好填满, svcntw()>16 即溢出,
     * 首个越界合法 VL 为 640bit (svcntw()=20 → 80 字节写入 64 字节栈缓冲) */

    /* 每线程私有副本 (SLF 探针, 消除多写者竞争; 同 probe-c。fork 模型下
     * 随子进程退出回收, cleanup 只释放 test->data 所辖分配 — 同家族约定) */
    static thread_local uint8_t *priv = nullptr;
    if (!priv)
        priv = (uint8_t *)aligned_alloc(64, SLF_SWEEP_BUFFER_BYTES);

    /* 三种宽度的索引表, 每 run-call 建一次 (svuint8_t 是 sizeless 类型,
     * 不能作数组元素, 故向量本体在宽度循环内从表中加载) */
    static const int widths[3] = {2, 4, 8};
    uint8_t vidx_bytes[3][SVE_MAX_VECTOR_BYTES];
    for (int w = 0; w < 3; ++w)
        slf_sweep_build_index_table(vidx_bytes[w], vbytes, widths[w]);
    svbool_t pg_idx = svwhilelt_b8((uint64_t)0, vbytes);

    TEST_LOOP(test, 1 << 13) {
        /* 改进 A: 每遍在 16/32/64-bit 三种元素宽度间轮换扫描 */
        for (int w = 0; w < 3; ++w) {
            const int width = widths[w];
            svuint8_t vidx = svld1_u8(pg_idx, vidx_bytes[w]);

            /* 每遍宽度扫描前重置私有副本 (store-back 污染; 同 probe-c) */
            memcpy(priv, data->golden, total);

            for (uint64_t base = 0; base < total; base += vbytes) {
                uint64_t n = (total - base < vbytes) ? total - base : vbytes;
                svbool_t pg = svwhilelt_b8((uint64_t)0, n);

                /* 载入本批字节 (向量) → 元素内字节重排 */
                svuint8_t vswapped = svtbl_u8(svld1_u8(pg, priv + base), vidx);

                /* ★ SLF 核心: 交换结果存回同一地址 (svst1 直打堆无栈中转) */
                svst1_u8(pg, priv + base, vswapped);
                __asm__ volatile("" ::: "memory");
                /* ★ 立即重读同一地址 (真实 store→load 转发)。asm memory barrier
                 * 阻断编译器消除真实 reload (无屏障时破坏 SLF 语义, 同 probe-c) */
                svuint8_t reloaded = svld1_u8(pg, (const uint8_t *)priv + base);

                /* 再交换一次还原 (同一索引表, svtbl 对合) → 栈 scratch */
                svuint8_t vrestored = svtbl_u8(reloaded, vidx);
                uint8_t out_bytes[SVE_MAX_VECTOR_BYTES];
                svst1_u8(pg, out_bytes, vrestored);

                /* 校验: 还原字节 == golden 字节 (按字节, 与宽度解耦);
                 * ONLY on failure: dump index/width/golden/actual/xor + 字节分解 */
                for (uint64_t i = 0; i < n; ++i) {
                    if (out_bytes[i] != data->golden[base + i]) {
                        uint64_t elem_off = (i / (uint64_t)width) * (uint64_t)width;
                        slf_sweep_report_fail((base + elem_off) / (uint64_t)width,
                                              base + elem_off, width,
                                              data->golden + base + elem_off,
                                              out_bytes + elem_off);
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
#else
static int movbe_slf_sweep_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_slf_sweep_sve");
    return EXIT_SKIP;
}
#endif

static int movbe_slf_sweep_sve_cleanup(struct test *test)
{
    auto *data = (movbe_slf_sweep_sve_data *)test->data;
    if (data) {
        /* 改进 C: cleanup 释放 test->data 所辖全部分配 (无死分配) */
        free(data->golden);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_slf_sweep_sve,
             "SVE multi-width (16/32/64-bit) byte-swap round-trip through the "
             "store->load-forwarding path: svtbl element-reversal stored back and "
             "reloaded per width, VL-portable, no dead allocations "
             "(extends movbe_dump_probe_c_sve)")
    .test_init = movbe_slf_sweep_sve_init,
    .test_run = movbe_slf_sweep_sve_run,
    .test_cleanup = movbe_slf_sweep_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
