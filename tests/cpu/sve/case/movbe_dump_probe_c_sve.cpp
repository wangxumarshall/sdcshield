/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_c_sve
 * @parblock
 * PROBE C (SVE): store-to-load-forwarding — each thread keeps a private copy of
 * the input, the swapped bytes are stored BACK to the private input position and
 * reloaded from there (vector SLF probe).
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

struct movbe_dump_probe_c_sve_data {
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

static int movbe_dump_probe_c_sve_init(struct test *test)
{
#if defined(__aarch64__)
    /* All-VL Phase 2 P6: SVE capability + 128..1024-bit VL range
     * gate in one framework-layer call (device/cpu/arm64/arm64_vl.h). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
#endif
    auto *data = (movbe_dump_probe_c_sve_data *)malloc(sizeof(movbe_dump_probe_c_sve_data));
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
static int movbe_dump_probe_c_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = (movbe_dump_probe_c_sve_data *)test->data;

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

    /* 每线程私有副本 (SLF 探针, 消除多写者竞争) */
    static thread_local uint32_t *priv_input = nullptr;
    if (!priv_input) priv_input = (uint32_t *)aligned_alloc(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    memcpy(priv_input, data->input, MOVBE_BUFFER_SIZE * sizeof(uint32_t));

    TEST_LOOP(test, 1 << 13) {
        /* 每外层迭代重置私有副本 (store-back 污染下一遍; 原版标量逐 i 独立访问无此问题) */
        memcpy(priv_input, data->input, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        for (size_t base = 0; base < MOVBE_BUFFER_SIZE; base += lanes) {
            int n = (MOVBE_BUFFER_SIZE - base < (size_t)lanes) ? (int)(MOVBE_BUFFER_SIZE - base) : lanes;
            svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)(n * 4));

            /* 载入 n 个 32-bit 元素 (向量) → 字节级重排 */
            uint8_t in_bytes[SVE_MAX_VECTOR_BYTES];
            memcpy(in_bytes, priv_input + base, n * 4);
            svuint8_t vswapped = svtbl_u8(svld1_u8(pg, in_bytes), vidx);

            /* PROBE C: store-to-load-forwarding — 存回私有输入位置再读。
             * asm memory barrier 阻断编译器对 store/reload 序列的优化
             * (无屏障时编译器消除真实 reload, 破坏 SLF 语义)。 */
            memcpy(priv_input + base, in_bytes, n * 4);
            /* ★ 直接 store 到输入位置 (SLF 核心, svst1 直打堆无栈中转) */
            svst1_u8(pg, (uint8_t *)(priv_input + base), vswapped);
            __asm__ volatile("" ::: "memory");
            /* ★ reload: svld1 直接读刚写过的输入位置 (真实 store→load 转发) */
            svuint8_t reloaded = svld1_u8(pg, (const uint8_t *)(priv_input + base));

            /* 再交换一次还原 (同一索引表, svtbl 可逆) */
            svuint8_t vrestored = svtbl_u8(reloaded, vidx);
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

    /* issue #374: post-loop verification of the in-place swapped private
     * copy, in the legal-set form this probe's in-place design permits
     * (modeled on the scalar movbe_dump_probe_c). The store-back flips the
     * byte order of priv_input[i] in place, but each TEST_LOOP body first
     * RESETS the private copy to the shared seed (the SVE port's
     * per-iteration memcpy, absent in the scalar probe), so at every
     * completed body the content is exactly bswap(seed) -- the reset pins
     * the flip parity to odd. The legal-set form {seed, bswap(seed)} is
     * kept (not exact equality) because it is also correct for a
     * partially-executed final body, where each position holds either the
     * reset seed or the flipped value. Coverage boundary (documented
     * honestly): a persistent mispermutation P leaves P(seed[i]) outside
     * that set for random data -- non-involutive AND involutive alike,
     * because the per-iteration reset guarantees exactly one application
     * of P; the scalar probe's even-parity blind spot does not exist here.
     * The check runs in this thread, which owns the thread-local buffer;
     * the hot loop above is unchanged. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        uint32_t fin = priv_input[i];
        if (fin != data->input[i] && fin != data->expected_swapped[i]) {
            log_error("MovBE-SVE-dump: in-place priv buffer outside legal "
                      "{seed, bswap(seed)} set at index %u: seed=0x%08X "
                      "legal={0x%08X,0x%08X} actual=0x%08X",
                      (unsigned)i, data->input[i], data->input[i],
                      data->expected_swapped[i], fin);
            report_fail_msg("MovBE-SVE: in-place priv buffer outside legal "
                            "{seed, bswap(seed)} set at index %u", (unsigned)i);
        }
    }

    return EXIT_SUCCESS;
}
#else
static int movbe_dump_probe_c_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_dump_probe_c_sve");
    return EXIT_SKIP;
}
#endif

static int movbe_dump_probe_c_sve_cleanup(struct test *test)
{
    auto *data = (movbe_dump_probe_c_sve_data *)test->data;
    if (data) {
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_probe_c_sve,
             "SVE byte-swap round-trip with dump-on-failure: svtbl [3,2,1,0] + vector "
             "store/reload, input/golden/actual/xor byte breakdown on failure (port of movbe_dump)")
    .test_init = movbe_dump_probe_c_sve_init,
    .test_run = movbe_dump_probe_c_sve_run,
    .test_cleanup = movbe_dump_probe_c_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
