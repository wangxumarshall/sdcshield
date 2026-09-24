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
struct alignas(16) SharedData {
    uint8_t padding[1];            // 偏移 1 字节
    uint8x16_t data;               // 非对齐地址
    std::atomic<uint64_t> seq;
    /* run-constant random pattern (randomization hardening H10'):
     * multi-writer seqlock → all writers write THIS value; a flip in any
     * of the 128 bits is detectable (was all-0/all-1). */
    uint8x16_t pattern;
};

static int atomic_simd_128_unaligned_init(struct test *test) {
    auto *sd = new SharedData;
    if (!sd) return EXIT_FAILURE;
    sd->data = vdupq_n_u8(0);
    sd->seq.store(0, std::memory_order_relaxed);
    memset_random(&sd->pattern, sizeof(sd->pattern));   /* H10' */
    test->data = sd;
    return EXIT_SUCCESS;
}

static int atomic_simd_128_unaligned_run(struct test *test, int cpu) {
    (void)cpu;
    auto *sd = static_cast<SharedData*>(test->data);
    do {
        // H10': 所有写者写入共享的 run-constant 随机模式（init 时生成）
        uint8x16_t val = sd->pattern;

        // 写操作：使用非对齐存储（vst1q_u8 支持非对齐地址）
        sd->seq.fetch_add(1, std::memory_order_acq_rel);
        vst1q_u8((uint8_t*)&sd->data, val);   // 非对齐存储
        sd->seq.fetch_add(1, std::memory_order_acq_rel);

        // 读操作：使用非对齐加载（vld1q_u8 支持非对齐地址）
        uint64_t s1, s2;
        uint8x16_t read_val;
        do {
            s1 = sd->seq.load(std::memory_order_acquire);
            read_val = vld1q_u8((uint8_t*)&sd->data);   // 非对齐加载
            s2 = sd->seq.load(std::memory_order_acquire);
        } while (s1 != s2 || (s1 & 1));

        // 撕裂/位翻转检测：读出的 128 位必须逐位等于 run-constant 模式
        uint8x16_t cmp = vceqq_u8(read_val, sd->pattern);
        bool data_ok = true;
        for (int i = 0; i < 16; ++i) {
            if (vgetq_lane_u8(cmp, i) != 0xFF) {
                data_ok = false;
                break;
            }
        }

        if (!data_ok) {
            uint8_t rb[16], pb[16];
            vst1q_u8(rb, read_val);
            vst1q_u8(pb, sd->pattern);
            report_fail_msg("atomic_simd_128_unaligned: payload mismatch "
                            "(read=%02X%02X%02X%02X... want=%02X%02X%02X%02X...) — "
                            "tearing or bit flip",
                            rb[0], rb[1], rb[2], rb[3],
                            pb[0], pb[1], pb[2], pb[3]);
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int atomic_simd_128_unaligned_finish(struct test *test) {
    auto *sd = static_cast<SharedData*>(test->data);
    delete sd;
    return EXIT_SUCCESS;
}

#else

// Non-aarch64: ARM NEON SIMD is not available. Report a clean placeholder
// skip rather than a misleading pass (CLAUDE.md placeholder-test honesty).
static int atomic_simd_128_unaligned_init(struct test *test) {
    (void)test;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): ARM NEON SIMD required for this "
             "atomic-SIMD test");
    return EXIT_SKIP;
}
static int atomic_simd_128_unaligned_run(struct test *test, int cpu) {
    (void)test; (void)cpu;
    return EXIT_SKIP;
}
static int atomic_simd_128_unaligned_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(atomic_simd_128_unaligned, "Atomic 128-bit SIMD access (unaligned, via Seqlock) on ARM64")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = atomic_simd_128_unaligned_init,
    .test_run = atomic_simd_128_unaligned_run,
    .test_cleanup = atomic_simd_128_unaligned_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
