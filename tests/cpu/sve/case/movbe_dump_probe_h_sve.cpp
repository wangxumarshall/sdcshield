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

/* issue #374 review: parsed ONCE in init (single-threaded, before any
 * worker) so run and cleanup read the same value instead of each
 * re-parsing PROBE_H_LINES with duplicated clamp logic. */
static int g_probe_h_sve_lines = 512;

struct movbe_dump_probe_h_sve_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
    bool workers_ran;
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
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    {
        const char *ev = getenv("PROBE_H_LINES");
        g_probe_h_sve_lines = ev ? atoi(ev) : 512;
        if (g_probe_h_sve_lines < 1)
            g_probe_h_sve_lines = 1;
    }

    data->workers_ran = false;   /* issue #374 review: cleanup-sweep gate */

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

    data->workers_ran = true;

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
                const int h_lines = g_probe_h_sve_lines;
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

/* gcc-10 (20.03/22.03) 在 -O2/-O3 下对本 cleanup 形态 RTL expand ICE（每日 15 镜像
   CI 10 job 红根因）；O1 规避与 run 函数先例（626f6554）同款，冷路径校验语义不变。 */
__attribute__((optimize("O1")))
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
        const int h_lines = g_probe_h_sve_lines;   /* parsed once in init */
        const int lanes = svcntw();
        if (data->workers_ran && lanes > 0 && MOVBE_BUFFER_SIZE % (size_t)lanes == 0
            && (size_t)lanes <= (128 / 4)) {
            /* lanes bound: a full-vector svst1 must fit inside one 32-word
             * slot block; at lanes>32 (VL>1024) a vector straddles two
             * blocks and the last-writer mapping breaks -> guarded skip. */
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
