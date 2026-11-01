/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_h_sve
 * @parblock
 * PROBE H (SVE): sweep the store-side distinct-line footprint via the
 * PROBE_H_LINES env var (default 512 = baseline-equivalent).
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
#include <cstdlib>

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

struct movbe_dump_probe_h_sve_data {
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

static int movbe_dump_probe_h_sve_init(struct test *test)
{
#if defined(__aarch64__)
    /* All-VL Phase 2 P6: SVE capability + 128..1024-bit VL range
     * gate in one framework-layer call (device/cpu/arm64/arm64_vl.h). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
#endif
    auto *data = (movbe_dump_probe_h_sve_data *)malloc(sizeof(movbe_dump_probe_h_sve_data));
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
static int movbe_dump_probe_h_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = (movbe_dump_probe_h_sve_data *)test->data;

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
            /* PROBE H: 行足迹 sweep (PROBE_H_LINES env, 默认 512 行) — svst1 直打堆 */
            {
                static int h_lines = -1;
                if (h_lines < 0) {
                    const char *ev = getenv("PROBE_H_LINES");
                    h_lines = ev ? atoi(ev) : 512;
                    if (h_lines < 1) h_lines = 1;
                }
                size_t line_idx = (base / 128) % (size_t)h_lines;
                svst1_u8(pg, (uint8_t *)(data->swapped + line_idx * (128 / 4)), vswapped);
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
static int movbe_dump_probe_h_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_dump_probe_h_sve");
    return EXIT_SKIP;
}
#endif

static int movbe_dump_probe_h_sve_cleanup(struct test *test)
{
    auto *data = (movbe_dump_probe_h_sve_data *)test->data;
    if (data) {
#if defined(__aarch64__)
        /* issue #374: post-join verification of the stored intermediate, in
         * the probe's own footprint geometry (derived from the run code as
         * written; NOTE this SVE port's mapping differs from the scalar
         * probe_h's `i & mask` 4B-stride form). base is a WORD offset
         * stepping by lanes; the slot is
         *   line_idx = (base / 128) % h_lines, word offset line_idx*32
         * (a full-vector svst1 per base). floor(base/128) takes every
         * value 0..127 exactly (base spans 0..16384-lanes in lanes-word
         * steps and lanes <= 32 < 128), so the written slots are exactly
         * 0..min(h_lines,128)-1 -- slots at or above that are never
         * touched and are not swept. Slot s's last writer is the largest
         * base with floor(base/128) == f_s, where f_s = the largest
         * f <= 127 with f == s (mod h_lines) = 127 - ((127-s) % h_lines);
         * within bucket f_s the last multiple of lanes is
         *   base_last = 128*(f_s+1) - lanes
         * (128 is a multiple of every legal svcntw() value 4/8/16/32).
         * Every worker runs the identical base sequence and TEST_LOOP only
         * exits between complete inner sweeps, so post-join (cleanup runs
         * after run_threads joins every worker) slot s must hold the bytes
         * of svtbl(input[base_last .. base_last+lanes)) -- compared
         * word-by-word against expected_swapped. The check MUST be
         * post-join: the slot content depends on which vector a worker is
         * currently processing, so a run-side sweep would race with
         * still-running workers. h_lines is recomputed from the same env
         * var with the same clamping the run applies (getenv is stable for
         * the process lifetime, so the values agree); lanes via svcntw():
         * the framework pins the control thread (init/cleanup) and every
         * SVE worker to the machine-max VL, so run and cleanup observe the
         * same svcntw; the divisibility guard skips the sweep (without
         * failing) if that assumption ever broke. The hot loop is
         * unchanged; a persistent svtbl mispermutation (symmetric fault:
         * applied twice it cancels in the round-trip identity) leaves
         * every slot wrong and is caught here. */
        const char *ev = getenv("PROBE_H_LINES");
        int h_lines = ev ? atoi(ev) : 512;
        if (h_lines < 1)
            h_lines = 1;
        const int lanes = svcntw();
        if (lanes > 0 && MOVBE_BUFFER_SIZE % (size_t)lanes == 0) {
            size_t slots = ((size_t)h_lines < 128) ? (size_t)h_lines : 128;
            for (size_t s = 0; s < slots; ++s) {
                size_t f = 127 - ((127 - s) % (size_t)h_lines);
                size_t base_last = 128 * (f + 1) - (size_t)lanes;
                for (size_t j = 0; j < (size_t)lanes; ++j) {
                    size_t elem = base_last + j;
                    uint32_t actual = data->swapped[s * (128 / 4) + j];
                    if (actual != data->expected_swapped[elem]) {
                        log_error("MovBE-SVE-dump: swapped-intermediate miscompare "
                                  "at slot %u word %u (last writer vector %u, "
                                  "element %u): input=0x%08X golden=0x%08X "
                                  "actual=0x%08X xor=0x%08X",
                                  (unsigned)s, (unsigned)j,
                                  (unsigned)(base_last / (size_t)lanes),
                                  (unsigned)elem, data->input[elem],
                                  data->expected_swapped[elem], actual,
                                  data->expected_swapped[elem] ^ actual);
                        report_fail_msg("MovBE-SVE: swapped slot != independent golden "
                                        "at slot %u word %u", (unsigned)s, (unsigned)j);
                    }
                }
            }
        } else {
            /* Unreachable for every legal SVE VL (init gates 128..1024 bits
             * -> lanes 4/8/16/32, all dividing 16384). log_error alone
             * marks the thread failed in this framework, so the skip is
             * logged as a warning to avoid a false failure. */
            log_warning("MovBE-SVE-dump: slot sweep skipped: svcntw()=%d does not "
                        "divide MOVBE_BUFFER_SIZE", lanes);
        }
#endif
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_probe_h_sve,
             "SVE byte-swap round-trip with dump-on-failure: svtbl [3,2,1,0] + vector "
             "store/reload, input/golden/actual/xor byte breakdown on failure (port of movbe_dump)")
    .test_init = movbe_dump_probe_h_sve_init,
    .test_run = movbe_dump_probe_h_sve_run,
    .test_cleanup = movbe_dump_probe_h_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
