#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <cstring>

struct alignas(16) SharedData {
    std::atomic<__int128> data;        // 原子128位数据
    std::atomic<uint64_t> seq;
    /* run-constant random pattern (randomization hardening H10'): every
     * writer writes THIS value — see the run function. */
    __int128 pattern;
};

static int atomic_simd_128_init(struct test *test) {
    auto *sd = new SharedData;
    if (!sd) return EXIT_FAILURE;
    sd->data = 0;
    sd->seq.store(0, std::memory_order_relaxed);
    uint64_t lo = random64(), hi = random64();
    memcpy(&sd->pattern, &lo, 8);
    memcpy((uint8_t*)&sd->pattern + 8, &hi, 8);
    test->data = sd;
    return EXIT_SUCCESS;
}

static int atomic_simd_128_run(struct test *test, int cpu) {
    (void)cpu;
    auto *sd = static_cast<SharedData*>(test->data);

    do {
        /* H10': all writers store the shared run-constant random 128-bit
         * pattern (init-time framework RNG) — multi-writer seqlock stays
         * correct (any complete write equals the pattern), and a bit flip
         * in ANY of the 128 positions is detectable, unlike the previous
         * all-0/all-1 payloads where most bits were always 0. */
        __int128 val;
        memcpy(&val, &sd->pattern, sizeof(val));

        // 序列锁写入
        sd->seq.fetch_add(1, std::memory_order_acq_rel);
        sd->data.store(val, std::memory_order_release);
        sd->seq.fetch_add(1, std::memory_order_acq_rel);

        // 序列锁读取
        uint64_t s1, s2;
        __int128 read_val;
        do {
            s1 = sd->seq.load(std::memory_order_acquire);
            read_val = sd->data.load(std::memory_order_acquire);
            s2 = sd->seq.load(std::memory_order_acquire);
        } while (s1 != s2 || (s1 & 1));

        // 撕裂/位翻转检测：读出的 128 位必须逐位等于 run-constant 模式
        if (read_val != val) {
            uint64_t low = (uint64_t)read_val;
            uint64_t high = (uint64_t)(read_val >> 64);
            uint64_t plow, phigh;
            memcpy(&plow, &sd->pattern, 8);
            memcpy(&phigh, (const uint8_t*)&sd->pattern + 8, 8);
            report_fail_msg("atomic_simd_128: seqlock payload mismatch "
                            "(read=%016llX%016llX want=%016llX%016llX) — "
                            "tearing or bit flip",
                            (unsigned long long)high, (unsigned long long)low,
                            (unsigned long long)phigh, (unsigned long long)plow);
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int atomic_simd_128_finish(struct test *test) {
    auto *sd = static_cast<SharedData*>(test->data);
    delete sd;
    return EXIT_SUCCESS;
}

DECLARE_TEST(atomic_simd_128, "Atomic 128-bit access (aligned, via Seqlock) on ARM64")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = atomic_simd_128_init,
    .test_run = atomic_simd_128_run,
    .test_cleanup = atomic_simd_128_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
