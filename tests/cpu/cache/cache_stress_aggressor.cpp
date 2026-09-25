#include <sandstone.h>
#include <atomic>
#include <cstdint>
#include <new>
#include <thread>
#include <chrono>
#include <cstdio>
#ifdef __aarch64__
#include <arm_neon.h>          // NEON intrinsics (aarch64 only)
#include <sys/auxv.h>         // hardware feature detection (aarch64)
#endif

static constexpr size_t CACHE_LINE_SIZE = 64;

#ifdef __aarch64__
// 共享数据：128位NEON向量（4个float）对齐到缓存行
struct alignas(CACHE_LINE_SIZE) SharedData {
    float32x4_t victim_value;          // 128位NEON向量
    std::atomic<bool> stop;
    std::atomic<uint64_t> iteration_count;
    std::atomic<bool> victim_failed;   // victim 检出不一致 → run 收尾时 report_fail
};

// ARM 缓存行冲刷（将指定地址的缓存行写回并无效）
static inline void clflush_arm(void *ptr) {
    // 使用内联汇编执行 DC CIVAC（数据缓存清空并无效）
    // 注意：需要特权级别，若在用户空间可能无效，需通过 __clear_cache 等
    // 这里使用 __builtin___clear_cache 作为通用替代（会清空指令缓存，但对数据也有效）
    __builtin___clear_cache((char*)ptr, (char*)ptr + CACHE_LINE_SIZE);
    // 内存屏障确保冲刷完成
    __sync_synchronize();
}

static void aggressor_thread(SharedData *data) {
    while (!data->stop.load(std::memory_order_relaxed)) {
        clflush_arm((void*)data);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

static void victim_thread(SharedData *data) {
    // 每迭代重掷随机浮点数向量（框架 RNG 按线程独立流；frandomf_scale
    // 返回 [0, scale)，取 [-1, 1) 区间做跨符号值）
    while (!data->stop.load(std::memory_order_relaxed)) {
        // 构建向量：4 个独立随机 float
        float32x4_t val = {frandomf_scale(2.0f) - 1.0f,
                           frandomf_scale(2.0f) - 1.0f,
                           frandomf_scale(2.0f) - 1.0f,
                           frandomf_scale(2.0f) - 1.0f};

        // 写入共享数据（非对齐存储，但数据已对齐，可用 vst1q_f32）
        vst1q_f32((float*)&data->victim_value, val);

        // 立即读取
        float32x4_t actual = vld1q_f32((float*)&data->victim_value);

        // 比较两个向量是否完全相同（逐元素比较）
        uint32x4_t cmp = vceqq_f32(val, actual);
        bool passed = (vgetq_lane_u32(cmp, 0) && vgetq_lane_u32(cmp, 1) &&
                       vgetq_lane_u32(cmp, 2) && vgetq_lane_u32(cmp, 3));

        data->iteration_count.fetch_add(1, std::memory_order_relaxed);

        if (!passed) {
            // 检出 SDC：置位让 run 收尾 report_fail（fail-closed），
            // 只打印一次首错现场（不再每迭代刷 stderr）
            float ref[4], act[4];
            vst1q_f32(ref, val);
            vst1q_f32(act, actual);
            fprintf(stderr,
                    "cache_stress_aggressor: store/load mismatch "
                    "ref=[%g,%g,%g,%g] act=[[%g,%g,%g,%g]]\n",
                    ref[0], ref[1], ref[2], ref[3],
                    act[0], act[1], act[2], act[3]);
            data->victim_failed.store(true, std::memory_order_relaxed);
        }
    }
}
#endif // __aarch64__

static int cache_stress_aggressor_init(struct test *test) {
#ifdef __aarch64__
    auto *data = new (std::align_val_t{CACHE_LINE_SIZE}) SharedData();
    if (!data) return EXIT_FAILURE;
    data->victim_value = vdupq_n_f32(0.0f);   // 初始化为0
    data->stop = false;
    data->iteration_count = 0;
    data->victim_failed = false;
    test->data = data;
    return EXIT_SUCCESS;
#else
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for cache_stress_aggressor");
    return EXIT_SKIP;
#endif
}

static int cache_stress_aggressor_run(struct test *test, int cpu) {
#ifdef __aarch64__
    auto *data = static_cast<SharedData*>(test->data);
    std::thread aggressor(aggressor_thread, data);
    std::thread victim(victim_thread, data);

    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (test_time_condition(test));

    data->stop.store(true, std::memory_order_relaxed);
    aggressor.join();
    victim.join();

    // fail-closed：victim 的校验结果必须上报（原实现无条件 EXIT_SUCCESS，
    // 把检出的 store/load 不一致静默吞掉）
    if (data->victim_failed.load(std::memory_order_relaxed)) {
        report_fail_msg("cache_stress_aggressor: victim store/load mismatch "
                        "under aggressor CLFLUSH (see stderr first-failure dump, "
                        "iterations=%lu)",
                        data->iteration_count.load(std::memory_order_relaxed));
    }
    return EXIT_SUCCESS;
#else
    (void)cpu;
    (void)test;
    return EXIT_SKIP;
#endif
}

static int cache_stress_aggressor_finish(struct test *test) {
#ifdef __aarch64__
    auto *data = static_cast<SharedData*>(test->data);
    if (data) {
        operator delete(data, std::align_val_t{CACHE_LINE_SIZE});
        test->data = nullptr;
    }
#else
    (void)test;
#endif
    return EXIT_SUCCESS;
}

DECLARE_TEST(cache_stress_aggressor, "Stress cache with aggressive CLFLUSH (NEON)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = cache_stress_aggressor_init,
    .test_run = cache_stress_aggressor_run,
    .test_cleanup = cache_stress_aggressor_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
