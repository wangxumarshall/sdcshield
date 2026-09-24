#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef __aarch64__
#include <arm_acle.h>
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

static int crc32_fixed_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__
static int crc32_fixed_run(struct test *test, int cpu) {
    (void)cpu;
    std::vector<uint8_t> local_data(BLOCK_SIZE);
    /* randomization hardening P15: framework RNG */

    do {
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            local_data[i] = (uint8_t)random32();
        }

        uint32_t crc1 = 0xFFFFFFFF;
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            crc1 = __crc32b(crc1, local_data[i]);
        }
        crc1 = ~crc1;

        __sync_synchronize();

        uint32_t crc2 = crc32_ieee_software(local_data.data(), BLOCK_SIZE);

        bool data_ok = (crc1 == crc2);

        uint32_t store_buf = crc1;
        uint32_t reload_buf;
        memcpy(&reload_buf, &store_buf, sizeof(store_buf));
        bool consistent = (reload_buf == crc1);

        bool passed = data_ok && consistent;

        if (!passed) {
            report_fail_msg("crc32_fixed: CRC mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}
#else
static int crc32_fixed_run(struct test *test, int cpu) {
    (void)cpu;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): ARM __crc32b CRC instruction required");
    return EXIT_SKIP;
}
#endif

static int crc32_fixed_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(crc32_fixed, "CRC32 instruction test (random data, dual-check)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = crc32_fixed_init,
    .test_run = crc32_fixed_run,
    .test_cleanup = crc32_fixed_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
