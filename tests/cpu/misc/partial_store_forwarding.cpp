/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b partial_store_forwarding
 * @parblock
 * Load accessing a store buffer entry for partial data. This test
 * performs random stores and loads of varying widths (1, 2, 4, 8 bytes)
 * to a shared memory region. It maintains a shadow buffer to track
 * expected memory contents, and verifies that every load (especially
 * those accessing only a portion of a recently written store buffer
 * entry) returns the correctly merged data.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

// 强制生成不同位宽的 load 指令
static inline uint64_t read_mem(volatile uint8_t *addr, int size) {
    switch (size) {
        case 1: return *reinterpret_cast<volatile uint8_t*>(addr);
        case 2: return *reinterpret_cast<volatile uint16_t*>(addr);
        case 4: return *reinterpret_cast<volatile uint32_t*>(addr);
        case 8: return *reinterpret_cast<volatile uint64_t*>(addr);
    }
    return 0;
}

// 强制生成不同位宽的 store 指令
static inline void write_mem(volatile uint8_t *addr, int size, uint64_t val) {
    switch (size) {
        case 1: *reinterpret_cast<volatile uint8_t*>(addr) = static_cast<uint8_t>(val); break;
        case 2: *reinterpret_cast<volatile uint16_t*>(addr) = static_cast<uint16_t>(val); break;
        case 4: *reinterpret_cast<volatile uint32_t*>(addr) = static_cast<uint32_t>(val); break;
        case 8: *reinterpret_cast<volatile uint64_t*>(addr) = val; break;
    }
}

static int partial_store_forwarding_run(struct test *test, int cpu) {
    // 线程局部的内存缓冲区，避免多线程竞争
    alignas(64) uint8_t real_buf[256];
    alignas(64) uint8_t shadow_buf[256];

    // 初始化数据
    memset_random(real_buf, sizeof(real_buf));
    memcpy(shadow_buf, real_buf, sizeof(real_buf));

    TEST_LOOP(test, 1 << 16) {
        // 随机选取一个偏移量和大小进行写操作
        uintptr_t s_off = random32() % (sizeof(real_buf) - 8);
        int s_sz = 1 << (random32() % 4); // 1, 2, 4, 8
        uint64_t s_val = random64();

        // 随机选取一个偏移量和大小进行读操作 (很可能会访问到刚写入的部分数据)
        uintptr_t l_off = random32() % (sizeof(real_buf) - 8);
        int l_sz = 1 << (random32() % 4); // 1, 2, 4, 8

        /* issue #374: 独立 golden —— 在 store 之前捕获 load 范围的既有
         * 内容 (这些读发生在 store 之前, 不进入 store->load 转发窗口),
         * 使期望的 load 结果可以在寄存器里拼接出来: 重叠于 store 范围
         * 的字节取刚写入的 s_val 字节, 其余取捕获的 pre 字节。原先
         * real vs shadow 是同源双写互比 (副本比副本), store 转发路径的
         * 对称性损坏 (两边同错) 互相抵消; 拼接 golden 不经过第二次
         * 内存读, 转发交付错误必被检出。 */
        uint64_t pre = read_mem(real_buf + l_off, l_sz);

        // 写入真实缓冲区
        write_mem(real_buf + s_off, s_sz, s_val);
        // 更新影子缓冲区
        write_mem(reinterpret_cast<volatile uint8_t*>(shadow_buf + s_off), s_sz, s_val);

        // 内存屏障，防止编译器将 store 和随后的 load 优化掉或重排
        asm volatile("" ::: "memory");

        uint64_t real_val = read_mem(real_buf + l_off, l_sz);
        uint64_t shadow_val = read_mem(reinterpret_cast<volatile uint8_t*>(shadow_buf + l_off), l_sz);

        // 拼接独立 golden: 每字节位置 p ∈ [l_off, l_off+l_sz) 重叠于
        // [s_off, s_off+s_sz) 时取 s_val 的对应字节, 否则取 pre 的对应字节。
        uint64_t expected = 0;
        for (int b = 0; b < l_sz; ++b) {
            uintptr_t p = l_off + static_cast<uintptr_t>(b);
            uint64_t byte = (p >= s_off && p < s_off + static_cast<uintptr_t>(s_sz))
                                ? ((s_val >> (8 * (p - s_off))) & 0xff)
                                : ((pre >> (8 * b)) & 0xff);
            expected |= byte << (8 * b);
        }

        if (real_val != expected) {
            report_fail_msg("Partial store forwarding failed at offset %lu, size %d: "
                            "expected 0x%016lx (independent splice golden), got 0x%016lx",
                            l_off, l_sz, expected, real_val);
        }
        if (real_val != shadow_val) {
            report_fail_msg("Partial store forwarding failed at offset %lu, size %d: "
                            "expected 0x%016lx, got 0x%016lx (shadow divergence)",
                            l_off, l_sz, shadow_val, real_val);
        }
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(partial_store_forwarding, "Load accessing a store buffer entry for partial data")
    .test_run     = partial_store_forwarding_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
