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
// 共享数据：stop/计数/fail 汇总为进程级共享；victim 数据不在此——
// 每个 per-CPU worker 的 victim 槽是 thread_local 独占缓存行（见 run），
// 消除多 worker 线程交叉写同一 victim_value 的数据竞争
namespace { /* ODR: per-TU SharedData (29-file same-name hazard) */
struct alignas(CACHE_LINE_SIZE) SharedData {
    std::atomic<bool> stop;
    std::atomic<uint64_t> iteration_count;
    std::atomic<bool> victim_failed;   // victim 检出不一致 → run 收尾时 report_fail
};
}

// x86 clflush 的 ARM64 等价原语：dc civac（按虚拟地址 clean & invalidate 到
// 点一致性 PoC）——脏行先写回再失效，下次访问强制从 L2/L3 重取，产生真实
// 的数据缓存逐出压力。按 VA 的缓存维护指令 EL0 用户态可直接执行（无需特
// 权；仓内先例 l2c_cross_cache_line_arm.cpp:165、movbe_l1d_vs_l2_sve.cpp:192
// 均为用户态执行）。地址须缓存行对齐（Kunpeng 920 上非对齐 dc civac 触发
// BUS_ADRALN，见 lsu_store_forward_sve.cpp 注释）；本测试 slot 已 alignas(64)。
// dsb sy 确保维护指令完成后才进下一轮（覆盖旧实现的 __sync_synchronize）。
//
// 被替换的 __builtin___clear_cache 是 I-cache 维护原语（JIT/自修改代码用，
// 逐行 dc cvau + ic ivau）：cvau 不失效 L1 行，数据面零逐出压力（issue #318）。
static inline void clflush_arm(void *ptr) {
    __asm__ volatile("dc civac, %0\n\tdsb sy\n\t" : : "r"(ptr) : "memory");
}

static void aggressor_thread(SharedData *data, float32x4_t *slot) {
    while (!data->stop.load(std::memory_order_relaxed)) {
        clflush_arm((void*)slot);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

static void victim_thread(SharedData *data, float32x4_t *slot) {
    // 每迭代重掷随机浮点数向量（框架 RNG 按线程独立流——含裸 std::thread：
    // framework/random.cpp 为未走框架初始化的线程惰性播种私有引擎，
    // issue #348；frandomf_scale 返回 [0, scale)，取 [-1, 1) 区间做跨符号值）
    while (!data->stop.load(std::memory_order_relaxed)) {
        // 构建向量：4 个独立随机 float
        float32x4_t val = {frandomf_scale(2.0f) - 1.0f,
                           frandomf_scale(2.0f) - 1.0f,
                           frandomf_scale(2.0f) - 1.0f,
                           frandomf_scale(2.0f) - 1.0f};

        // 写入本线程对私有 victim 槽（独占缓存行）
        vst1q_f32((float*)slot, val);

        // 立即读回。必须用 asm 边界：-O3 下编译器对 intrinsic 做
        // store-to-load forwarding，把回读消除成寄存器自比（objdump 实证
        // fcmeq v1.4s, v1.4s 恒真，循环内无任何从 slot 的 load）——校验退
        // 化为空转。neon_rot_2src.cpp 同一问题同一修法（asm 边界保证
        // str q → ldr q 背靠背落到二进制里）。
        float32x4_t actual;
        __asm__ volatile("ldr %q0, [%1]" : "=w"(actual) : "r"(slot) : "memory");

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
    // per-worker victim 槽：thread_local + 64B 对齐 = 每对 aggressor/victim
    // 线程独占一条缓存行。原实现 96 个 worker 共享 test->data->victim_value，
    // PR#244 将框架 RNG 改为 per-thread 独立流后各线程序列不同，交叉写
    // 同址在 load 回读时看到他线程值 → 确定性误报（2026-10-16 战役
    // 事件#001：-n 1 pass ↔ 全核 fail，act[2,3]==ref[2,3] 部分覆盖）
    alignas(CACHE_LINE_SIZE) static thread_local float32x4_t slot;
    std::thread aggressor(aggressor_thread, data, &slot);
    std::thread victim(victim_thread, data, &slot);

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

DECLARE_TEST(cache_stress_aggressor, "Stress cache with aggressive line invalidation (NEON, DC CIVAC)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = cache_stress_aggressor_init,
    .test_run = cache_stress_aggressor_run,
    .test_cleanup = cache_stress_aggressor_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
