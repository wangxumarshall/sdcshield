#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef __aarch64__
#include <arm_acle.h>   // ARM64 CRC32 内联函数
#endif

static constexpr size_t BLOCK_SIZE = 1024;


/* randomization hardening H15' (P15): independent software CRC-32
 * reference (bit-by-bit, IEEE 802.3 poly reflected 0xEDB88320, init
 * 0xFFFFFFFF, final xorout) — matches the ARMv8 __crc32b instruction
 * (probe-verified: __crc32b('123456789') chain = 0xCBF43926, the zlib
 * CRC-32, NOT CRC32C). Replaces the previous hw-vs-hw duplicate compute
 * that a deterministic CRC-unit defect would pass. */
static uint32_t crc32_ieee_software(const uint8_t *buf, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return ~crc;
}

static int crc32_fixed_shuffled_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__
static int crc32_fixed_shuffled_run(struct test *test, int cpu) {
    (void)cpu;
    std::vector<uint8_t> local_data(BLOCK_SIZE);
    /* randomization hardening P15: framework RNG */
    auto step_dist = []() { return (int)(random32() % 3); };  /* 0:1字节, 1:2字节, 2:4字节 */

    do {
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            local_data[i] = (uint8_t)random32();
        }

        // 生成随机步长序列（保证两次计算使用相同序列）
        /* boundary fix (H15'): when the tail is 3 bytes, the old code
         * clamped step to 3 but still ran the 4-byte __crc32w path —
         * reading 1 byte past the buffer (latent OOB, previously
         * harmless because BOTH computes made the same OOB read; now
         * that crc2 is an in-bounds software reference it exposes the
         * bug). Clamp to whole steps and finish the tail with bytes. */
        std::vector<int> steps;
        size_t pos = 0;
        while (pos < BLOCK_SIZE) {
            int step = step_dist();
            if (step == 0) step = 1;
            else if (step == 1) step = 2;
            else step = 4;
            if (pos + step > BLOCK_SIZE)
                step = 1;   /* tail: byte steps only */
            steps.push_back(step);
            pos += step;
        }

        // 第一次硬件 CRC 计算
        uint32_t crc1 = 0xFFFFFFFF;
        pos = 0;
        for (int step : steps) {
            if (step == 1) {
                crc1 = __crc32b(crc1, local_data[pos]);
            } else if (step == 2) {
                uint16_t val;
                memcpy(&val, &local_data[pos], 2);
                crc1 = __crc32h(crc1, val);
            } else { // step == 4
                uint32_t val;
                memcpy(&val, &local_data[pos], 4);
                crc1 = __crc32w(crc1, val);
            }
            pos += step;
        }
        crc1 = ~crc1;

        // 内存屏障
        __sync_synchronize();

        // 第二次计算：独立软件参考（IEEE 802.3 CRC-32，逐字节，不依赖步长序列）
        uint32_t crc2 = crc32_ieee_software(local_data.data(), BLOCK_SIZE);

        bool data_ok = (crc1 == crc2);

        uint32_t store_buf = crc1;
        uint32_t reload_buf;
        memcpy(&reload_buf, &store_buf, sizeof(store_buf));
        bool consistent = (reload_buf == crc1);

        bool passed = data_ok && consistent;

        if (!passed) {
            report_fail_msg("crc32_fixed_shuffled: CRC mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}
#else
static int crc32_fixed_shuffled_run(struct test *test, int cpu) {
    (void)cpu;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): ARM __crc32b/h/w CRC instruction required");
    return EXIT_SKIP;
}
#endif

static int crc32_fixed_shuffled_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(crc32_fixed_shuffled, "CRC32 instruction with shuffled access and mixed width (dual-check)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = crc32_fixed_shuffled_init,
    .test_run = crc32_fixed_shuffled_run,
    .test_cleanup = crc32_fixed_shuffled_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
