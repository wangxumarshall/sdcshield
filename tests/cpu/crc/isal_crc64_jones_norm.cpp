#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <isa-l/crc.h>
#include <isa-l/crc64.h>

extern "C" {
    uint64_t crc64_jones_norm(uint64_t init, const unsigned char *buf, uint64_t len);
}

static constexpr size_t BLOCK_SIZE = 1024;

static int isal_crc64_jones_norm_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int isal_crc64_jones_norm_run(struct test *test, int cpu) {
    (void)cpu;
    std::vector<uint8_t> local_data(BLOCK_SIZE);
    /* randomization hardening P15: framework RNG (per-thread, -s reproducible) */

    do {
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            local_data[i] = (uint8_t)random32();
        }

        uint64_t crc1 = crc64_jones_norm(0, local_data.data(), BLOCK_SIZE);

        // ARM64 兼容的内存屏障
        __sync_synchronize();

        uint64_t crc2 = crc64_jones_norm_base(0, local_data.data(), BLOCK_SIZE);

        bool data_ok = (crc1 == crc2);

        uint64_t store_buf = crc1;
        uint64_t reload_buf;
        memcpy(&reload_buf, &store_buf, sizeof(store_buf));
        bool consistent = (reload_buf == crc1);

        bool passed = data_ok && consistent;

        if (!passed) {
            report_fail_msg("isal_crc64_jones_norm: CRC mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int isal_crc64_jones_norm_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(isal_crc64_jones_norm, "ISA-L CRC64 Jones normal (random data)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = isal_crc64_jones_norm_init,
    .test_run = isal_crc64_jones_norm_run,
    .test_cleanup = isal_crc64_jones_norm_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
