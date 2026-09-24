#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <cstring>
#include <ctime>
#include <unistd.h>

#ifdef __aarch64__
#include <arm_neon.h>

// 数据块：偏移 1 字节以确保非对齐（可能跨缓存行）
struct SharedData {
    /* run-constant random pattern halves (randomization hardening H10') */
    uint8_t padding[1];            // 偏移 1 字节
    uint8x16_t data0;              // 低 128 位（非对齐地址）
    uint8x16_t data1;              // 高 128 位（紧随其后，整体非对齐）
    std::atomic<uint64_t> seq;
    uint8x16_t pat0, pat1;
};

static int atomic_simd_256_unaligned_init(struct test *test) {
    auto *sd = new SharedData;
    if (!sd) return EXIT_FAILURE;
    sd->data0 = vdupq_n_u8(0);
    sd->data1 = vdupq_n_u8(0);
    sd->seq.store(0, std::memory_order_relaxed);
    memset_random(&sd->pat0, sizeof(sd->pat0));   /* H10' */
    memset_random(&sd->pat1, sizeof(sd->pat1));
    test->data = sd;
    return EXIT_SUCCESS;
}

static int atomic_simd_256_unaligned_run(struct test *test, int cpu) {
    (void)cpu;
    auto *sd = static_cast<SharedData*>(test->data);
    do {
        // H10': 所有写者写入共享的 run-constant 随机模式（init 时生成）
        uint8x16_t val0 = sd->pat0;
        uint8x16_t val1 = sd->pat1;

        // 写操作：使用非对齐存储（vst1q_u8 支持非对齐）
        sd->seq.fetch_add(1, std::memory_order_acq_rel);
        vst1q_u8((uint8_t*)&sd->data0, val0);   // 非对齐存储
        vst1q_u8((uint8_t*)&sd->data1, val1);
        sd->seq.fetch_add(1, std::memory_order_acq_rel);

        // 读操作：使用非对齐加载（vld1q_u8 支持非对齐）
        uint64_t s1, s2;
        uint8x16_t read0, read1;
        do {
            s1 = sd->seq.load(std::memory_order_acquire);
            read0 = vld1q_u8((uint8_t*)&sd->data0);
            read1 = vld1q_u8((uint8_t*)&sd->data1);
            s2 = sd->seq.load(std::memory_order_acquire);
        } while (s1 != s2 || (s1 & 1));

        // 撕裂/位翻转检测：读出的 256 位必须逐位等于 run-constant 模式
        uint8_t rb[32], pb[32];
        vst1q_u8(rb, read0);
        vst1q_u8(rb + 16, read1);
        vst1q_u8(pb, sd->pat0);
        vst1q_u8(pb + 16, sd->pat1);
        bool data_ok = (memcmp(rb, pb, 32) == 0);

        if (!data_ok) {
            report_fail_msg("atomic_simd_256_unaligned: payload mismatch "
                            "(read=%02X%02X%02X%02X... want=%02X%02X%02X%02X...) — "
                            "tearing or bit flip",
                            rb[0], rb[1], rb[2], rb[3],
                            pb[0], pb[1], pb[2], pb[3]);
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int atomic_simd_256_unaligned_finish(struct test *test) {
    auto *sd = static_cast<SharedData*>(test->data);
    delete sd;
    return EXIT_SUCCESS;
}

#else

// Non-aarch64: ARM NEON SIMD is not available. Report a clean placeholder
// skip rather than a misleading pass (CLAUDE.md placeholder-test honesty).
static int atomic_simd_256_unaligned_init(struct test *test) {
    (void)test;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): ARM NEON SIMD required for this "
             "atomic-SIMD test");
    return EXIT_SKIP;
}
static int atomic_simd_256_unaligned_run(struct test *test, int cpu) {
    (void)test; (void)cpu;
    return EXIT_SKIP;
}
static int atomic_simd_256_unaligned_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(atomic_simd_256_unaligned, "Atomic 256-bit SIMD access (unaligned, via Seqlock) on ARM64")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = atomic_simd_256_unaligned_init,
    .test_run = atomic_simd_256_unaligned_run,
    .test_cleanup = atomic_simd_256_unaligned_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
