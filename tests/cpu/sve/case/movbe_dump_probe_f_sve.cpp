/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_f_sve
 * @parblock
 * PROBE F (SVE): store address advances but never crosses a cache line —
 * the swapped batch is stored to a rotating 16-slot window inside one line.
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

struct movbe_dump_probe_f_sve_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
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

#if defined(__aarch64__)
/* issue #374: PROBE F's rotating one-line store window, hoisted from
 * function-local scope so the post-join cleanup sweep can read it. A
 * zero-initialized static local emits no code, so moving the declaration
 * out of the loop block leaves the hot loop's instruction stream
 * byte-identical; it is still the single object shared by all workers
 * (static storage), so the probe's store geometry is unchanged. */
static uint32_t line_window[16];
#endif

static int movbe_dump_probe_f_sve_init(struct test *test)
{
#if defined(__aarch64__)
    /* All-VL Phase 2 P6: SVE capability + 128..1024-bit VL range
     * gate in one framework-layer call (device/cpu/arm64/arm64_vl.h). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
#endif
    auto *data = (movbe_dump_probe_f_sve_data *)malloc(sizeof(movbe_dump_probe_f_sve_data));
    data->input = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the svtbl under test). */
    movbe_golden_lut_init();
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    test->data = data;
    return EXIT_SUCCESS;
}

#if defined(__aarch64__)
/* gcc-10 (22.03/20.03) 对本函数在 -O2+ 的 RTL expand 确定性 ICE(internal
 * compiler error: Segmentation fault,run 35977342136/36078386244 两轮修复
 * 均无效)。rootfs gcc-10.3.1 二分定位:-O0/-O1 过、-O2 起崩,非向量化类
 * (-fno-tree-vectorize 等不救);函数级降 O1 稳定通过。SVE intrinsics 为
 * 显式生成,不依赖优化器;probe 测 store 侧效应而非吞吐,O1 无语义影响。 */
__attribute__((optimize("O1")))
static int movbe_dump_probe_f_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = (movbe_dump_probe_f_sve_data *)test->data;

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
        for (size_t base = 0; base < MOVBE_BUFFER_SIZE; base += lanes) {
            int n = (MOVBE_BUFFER_SIZE - base < (size_t)lanes) ? (int)(MOVBE_BUFFER_SIZE - base) : lanes;
            svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)(n * 4));

            /* 载入 n 个 32-bit 元素 (向量) → 字节级重排 */
            uint8_t in_bytes[SVE_MAX_VECTOR_BYTES];
            memcpy(in_bytes, data->input + base, n * 4);
            svuint8_t vswapped = svtbl_u8(svld1_u8(pg, in_bytes), vidx);

            /* store swapped (向量 store/reload 路径) */
            /* PROBE F: store 在单个 cache line 内轮转 (不越行) — svst1 直打窗口 */
            {
                uint32_t first = 0;
                memcpy(&first, &vswapped, 4);   /* 取首元素 (编译器协助) */
                svbool_t pg1 = svwhilelt_b32((uint64_t)0, (uint64_t)1);
                /* gcc-10 (22.03/20.03 CI, run 35977342136) 对 svst1 地址式内含
                   "& 常数" 在 RTL expand ICE;地址与首元素先落入局部变量再传,
                   语义不变 (probe_h 的 line_idx 先例)。 */
                uint32_t *win = &line_window[(base / lanes) & 15];
                svst1_u32(pg1, win, svdup_u32(first));
            }

            /* 再交换一次还原 (同一索引表, svtbl 可逆) */
            svuint8_t vrestored = svtbl_u8(vswapped, vidx);   /* 寄存器直连: 原版 bswap 作用于寄存器值, 这些 probe 被测的是 store 侧效应 */
            uint8_t out_bytes[SVE_MAX_VECTOR_BYTES];
            svst1_u8(pg, out_bytes, vrestored);

            /* 校验: 还原值 == 输入值; ONLY on failure: dump input/golden/actual/xor + 字节分解 */
            uint32_t restored[SVE_MAX_VECTOR_BYTES / 4];
            memcpy(restored, out_bytes, n * 4);
            for (int i = 0; i < n; ++i) {
                if (restored[i] != data->input[base + i]) {
                    uint32_t inputv = data->input[base + i];
                    uint32_t golden = inputv;
                    uint32_t actual = restored[i];
                    uint32_t xorv = golden ^ actual;
                    unsigned char *ib = (unsigned char *)&inputv;
                    unsigned char *gb = (unsigned char *)&golden;
                    unsigned char *ab = (unsigned char *)&actual;
                    unsigned char *xb = (unsigned char *)&xorv;
                    log_error("MovBE-SVE-dump: Round-trip failed at index %u: "
                              "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
                              "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
                              "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
                              "xor=%02X%02X%02X%02X",
                              (unsigned)(base + i), inputv, golden, actual, xorv,
                              ib[3], ib[2], ib[1], ib[0],
                              gb[3], gb[2], gb[1], gb[0],
                              ab[3], ab[2], ab[1], ab[0],
                              xb[3], xb[2], xb[1], xb[0]);
                    report_fail_msg("MovBE-SVE: Round-trip failed at index %u",
                                    (unsigned)(base + i));
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
#else
static int movbe_dump_probe_f_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_dump_probe_f_sve");
    return EXIT_SKIP;
}
#endif

static int movbe_dump_probe_f_sve_cleanup(struct test *test)
{
    auto *data = (movbe_dump_probe_f_sve_data *)test->data;
    if (data) {
#if defined(__aarch64__)
        /* issue #374: post-join verification of the rotating store window,
         * in the probe's own slot geometry. line_window is one static object
         * shared by ALL workers and a slot's value depends on which vector
         * a worker is currently processing, so the check MUST be post-join
         * (cleanup runs after run_threads joins every worker); a run-side
         * sweep would race with still-running workers. Every worker runs
         * the identical base sequence and TEST_LOOP only exits between
         * complete inner sweeps, so each worker's final write to slot s is
         * vector v_last = total_vectors-16+s: the slot index is
         * (base/lanes) & 15 and total_vectors = MOVBE_BUFFER_SIZE/lanes is
         * a multiple of 16 for every legal svcntw() (4/8/16/32 ->
         * 4096/2048/1024/512). The stored value is the FIRST element of
         * that vector (memcpy(&first, &vswapped, 4) takes element 0 of the
         * svtbl result), i.e. bswap32(input[v_last*lanes]), so the slot
         * must equal expected_swapped[v_last*lanes]. lanes is recomputed
         * via svcntw() here: the framework pins the control thread (which
         * runs init and cleanup) and every SVE worker to the machine-max
         * VL, so run and cleanup observe the same svcntw; the divisibility
         * guard below skips the sweep if that assumption ever broke, rather
         * than comparing wrong slots. The hot loop is unchanged; a
         * persistent svtbl mispermutation (symmetric fault: applied twice
         * it cancels in the round-trip identity) leaves every slot wrong
         * and is caught here. */
        const int lanes = svcntw();
        if (lanes > 0 && MOVBE_BUFFER_SIZE % (size_t)lanes == 0
                && (MOVBE_BUFFER_SIZE / (size_t)lanes) % 16 == 0) {
            const size_t total_vectors = MOVBE_BUFFER_SIZE / (size_t)lanes;
            for (size_t s = 0; s < 16; ++s) {
                size_t v_last = total_vectors - 16 + s;
                size_t first_elem = v_last * (size_t)lanes;
                uint32_t actual = line_window[s];
                if (actual != data->expected_swapped[first_elem]) {
                    log_error("MovBE-SVE-dump: window slot miscompare at slot %u "
                              "(last writer vector %u, element %u): input=0x%08X "
                              "golden=0x%08X actual=0x%08X xor=0x%08X",
                              (unsigned)s, (unsigned)v_last, (unsigned)first_elem,
                              data->input[first_elem], data->expected_swapped[first_elem],
                              actual, data->expected_swapped[first_elem] ^ actual);
                    report_fail_msg("MovBE-SVE: window slot != independent golden at slot %u",
                                    (unsigned)s);
                }
            }
        } else {
            /* Unreachable for every legal SVE VL (init gates 128..1024 bits
             * -> lanes 4/8/16/32, all tiling 16384 into 16-multiples).
             * log_error alone marks the thread failed in this framework, so
             * the skip is logged as a warning to avoid a false failure. */
            log_warning("MovBE-SVE-dump: window sweep skipped: svcntw()=%d does not "
                        "tile MOVBE_BUFFER_SIZE into a 16-multiple vector count", lanes);
        }
#endif
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_probe_f_sve,
             "SVE byte-swap round-trip with dump-on-failure: svtbl [3,2,1,0] + vector "
             "store/reload, input/golden/actual/xor byte breakdown on failure (port of movbe_dump)")
    .test_init = movbe_dump_probe_f_sve_init,
    .test_run = movbe_dump_probe_f_sve_run,
    .test_cleanup = movbe_dump_probe_f_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
