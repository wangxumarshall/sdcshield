/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_golden_sve
 * @parblock
 * SVE port of movbe_dump_golden (source: missing_testcases_20261009/
 * tests/cpu/misc/movbe_dump_golden.cpp).
 *
 * Cross-validation variant of the movbe round-trip: test_init (which runs
 * on the child main thread before the per-CPU workers spawn) writes the
 * input buffer to a file right after filling it. That file is the
 * independent golden: it records the exact bits written by init, which
 * never passed through a suspect core's load path. The fail-branch
 * golden/actual in the log are re-read by whichever core hit the
 * miscompare, so they are themselves potentially corrupted; comparing the
 * log's golden/actual against this file answers "did the failing core
 * even read the correct input?".
 *
 * The hot loop is identical to movbe_l1d_sve's (the scalar original's run
 * is byte-identical to movbe_dump.cpp's, which movbe_l1d's run copies):
 * in1 = svld1(input); svtbl; svst1(swapped); in2 = asm ldr z reload of
 * input; svtbl back; vector compare with a cold per-element rescan.
 *
 * Deviation from the scalar original (documented): the golden file goes
 * to /var/tmp/movbe_input_golden_sve_<pid>.bin instead of the original's
 * fixed /tmp/movbe_input_golden.bin — /tmp is tmpfs (lost on the
 * crash/hang events this family exists to investigate) and the fixed
 * path lets concurrent runs clobber each other; the dump-capable
 * siblings (neon_rot_ldr_at_top_rowmajor_dump) already chose /var/tmp
 * for exactly this reason. The pid suffix makes overlapping runs safe.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#if defined(__aarch64__)
#include <arm_sve.h>

#ifndef SVE_MAX_VECTOR_BYTES
/* GCC 12 的 arm_sve.h 不提供该 ACLE 宏 (movbe_slf_sweep 同款自定义):
 * SVE 架构最大 VL 2048bit = 256 字节 */
#define SVE_MAX_VECTOR_BYTES 256
#endif
#include <device/cpu/arm64/arm64_vl.h>
#endif

#define MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE (1 << 14)

struct MovbeDumpGoldenSveData {
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

static int movbe_dump_golden_sve_init(struct test *test)
{
#if defined(__aarch64__)
    /* All-VL Phase 2 P6: SVE capability + 128..1024-bit VL range
     * gate in one framework-layer call (device/cpu/arm64/arm64_vl.h). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
#endif
    auto *data = static_cast<MovbeDumpGoldenSveData *>(malloc(sizeof(MovbeDumpGoldenSveData)));
    data->input = static_cast<uint32_t *>(aligned_alloc_safe(64, MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE * sizeof(uint32_t)));
    data->swapped = static_cast<uint32_t *>(aligned_alloc_safe(64, MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE * sizeof(uint32_t)));
    data->expected_swapped = static_cast<uint32_t *>(aligned_alloc_safe(64, MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE * sizeof(uint32_t)));

    /* init 线程一次性填充; 每次迭代复用 (原版语义) */
    for (size_t i = 0; i < MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the svtbl under test).
     * 与下方落盘 golden 互补: 文件离线校验 input 侧, 此缓冲在线校验
     * swapped 侧。 */
    for (size_t i = 0; i < MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    /* 落盘独立 golden: init 写入, 未经任何 worker 核的加载路径 */
    char path[128];
    snprintf(path, sizeof(path), "/var/tmp/movbe_input_golden_sve_%d.bin", (int)getpid());
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t total = 0;
        const char *buf = (const char *)data->input;
        size_t len = MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE * sizeof(uint32_t);
        while ((size_t)total < len) {
            ssize_t n = write(fd, buf + total, len - (size_t)total);
            if (n < 0 && errno == EINTR) continue;   /* 重试被信号打断的写 */
            if (n <= 0) break;
            total += n;
        }
        if ((size_t)total != len) {
            log_error("movbe_dump_golden_sve: incomplete golden write to %s "
                      "(%zd/%zu bytes) — cross-validation golden is truncated",
                      path, total, len);
        }
        close(fd);
    } else {
        log_error("movbe_dump_golden_sve: cannot open golden file %s (errno %d) — "
                  "continuing without cross-validation golden", path, errno);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

#if defined(__aarch64__)

static inline svuint8_t load_vec_u8_sve(const void *addr)
{
    svuint8_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

static int movbe_dump_golden_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<MovbeDumpGoldenSveData *>(test->data);

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
        for (size_t base = 0; base < MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE; base += lanes) {
            int n = (MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE - base < (size_t)lanes)
                        ? (int)(MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE - base) : lanes;
            svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)(n * 4));

            svuint8_t in1 = svld1_u8(pg, (const uint8_t *)(data->input + base));
            svuint8_t vswapped = svtbl_u8(in1, vidx);
            svst1_u8(pg, (uint8_t *)(data->swapped + base), vswapped);

            /* 被测加载: input 的堆 reload (asm 保证, 与 movbe_l1d_sve 同构) */
            svuint8_t in2 = load_vec_u8_sve(data->input + base);

            svuint8_t vround = svtbl_u8(vswapped, vidx);
            if (svptest_any(pg, svcmpne_u8(pg, vround, in2))) {
                /* 冷路径逐元素复核 + 日志。复核基准用被测 reload(in2) ——
                 * 向量级 svcmpne 检出的正是 "寄存器结果 != 被测 reload"。
                 * input 单独记录字节交换前的原始加载(in1), 与 golden(in2)
                 * 分开, 从而可判断 "字节交换前的加载" 是否已与被测 reload
                 * 分叉 (原实现 input==golden==reload, 丢失该判别力)。 */
                svst1_u8(pg, round_bytes, vround);
                uint8_t in1_bytes[SVE_MAX_VECTOR_BYTES];
                svst1_u8(pg, in1_bytes, in1);
                uint8_t in2_bytes[SVE_MAX_VECTOR_BYTES];
                svst1_u8(pg, in2_bytes, in2);
                uint32_t in1w[SVE_MAX_VECTOR_BYTES / 4];
                uint32_t round[SVE_MAX_VECTOR_BYTES / 4];
                uint32_t reload[SVE_MAX_VECTOR_BYTES / 4];
                memcpy(in1w, in1_bytes, (size_t)n * 4);
                memcpy(round, round_bytes, (size_t)n * 4);
                memcpy(reload, in2_bytes, (size_t)n * 4);
                for (int k = 0; k < n; ++k) {
                    if (round[k] != reload[k]) {
                        log_error("MovBE-dump-golden-SVE: Round-trip failed at index %u: "
                                  "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                                  (unsigned)(base + k), in1w[k], reload[k], round[k],
                                  reload[k] ^ round[k]);
                        report_fail_msg("MovBE-SVE: Round-trip failed at index %u",
                                        (unsigned)(base + k));
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
    for (size_t i = 0; i < MOVBE_DUMP_GOLDEN_SVE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-dump-golden-SVE: swapped-intermediate miscompare at "
                      "index %u: input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      (unsigned)i, data->input[i], data->expected_swapped[i],
                      data->swapped[i], data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("MovBE-SVE: swapped buffer != independent golden at index %u",
                            (unsigned)i);
        }
    }

    return EXIT_SUCCESS;
}

#else

static int movbe_dump_golden_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for movbe_dump_golden_sve");
    return EXIT_SKIP;
}

#endif

static int movbe_dump_golden_sve_cleanup(struct test *test)
{
    auto *data = static_cast<MovbeDumpGoldenSveData *>(test->data);
    if (data) {
        free(data->input);
        free(data->swapped);
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_golden_sve,
             "SVE port of movbe_dump_golden: movbe round-trip with init-dumps-input-buffer to "
             "/var/tmp as the independent init-thread golden (hot path identical to movbe_l1d_sve)")
    .test_init = movbe_dump_golden_sve_init,
    .test_run = movbe_dump_golden_sve_run,
    .test_cleanup = movbe_dump_golden_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
