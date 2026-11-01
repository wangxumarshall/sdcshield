#include <sandstone.h>
#include <atomic>
#include <cstdint>
#include <new>

static constexpr size_t CACHE_LINE_SIZE = 64;

namespace { /* ODR: per-TU SharedData (29-file same-name hazard) */
struct alignas(CACHE_LINE_SIZE) SharedData {
    std::atomic<uint64_t> counter;      // 实际累加结果
    std::atomic<uint64_t> total_adds;   // 总尝试递增次数
    /* issue #374: 每核寄存器计数落点 (worker 退出时一次性写入, 各核独占
     * 槽位无竞争)。counter-vs-total_adds 恒等式对"两原子同丢一次更新"
     * 的对称性丢失盲 (整迭代丢弃/一致性丢更新) 是恒真的, 独立 golden =
     * 各核本地寄存器累加的和 —— 它从未经过被测的跨核 cacheline 弹跳
     * 通路, 任何一边丢失都必然使两边失配。 */
    uint64_t *local_adds;
};
}

static int cachebounce_init(struct test *test) {
    auto *data = new (std::align_val_t{CACHE_LINE_SIZE}) SharedData();
    if (!data) return EXIT_FAILURE;
    data->counter = 0;
    data->total_adds = 0;
    const int ndev = device_count();
    data->local_adds = ndev > 0
        ? static_cast<uint64_t *>(calloc(static_cast<size_t>(ndev), sizeof(uint64_t)))
        : nullptr;
    if (ndev > 0 && !data->local_adds) {
        ::operator delete(data, std::align_val_t{CACHE_LINE_SIZE});
        return EXIT_FAILURE;
    }
    test->data = data;
    return EXIT_SUCCESS;
}

static int cachebounce_run(struct test *test, int cpu) {
    auto *data = static_cast<SharedData*>(test->data);
    uint64_t local = 0;   /* 寄存器累加: 独立 golden (issue #374) */
    do {
        data->counter.fetch_add(1, std::memory_order_relaxed);
        data->total_adds.fetch_add(1, std::memory_order_relaxed);
        ++local;
    } while (test_time_condition(test));
    /* 一次性落盘本核计数; 各核独占槽位 (框架每核一个 worker)。 */
    if (data->local_adds && cpu >= 0 && cpu < device_count())
        data->local_adds[cpu] = local;
    return EXIT_SUCCESS;
}

static int cachebounce_finish(struct test *test) {
    auto *data = static_cast<SharedData*>(test->data);
    if (!data) return EXIT_SUCCESS;

    uint64_t c = data->counter.load();
    uint64_t t = data->total_adds.load();

    if (c != t) {
        report_fail_msg("cachebounce: counter (%lu) != total_adds (%lu)", c, t);
        return EXIT_FAILURE;
    }

    /* issue #374: 独立 golden 复核 —— 两个弹跳原子的值都必须等于各核
     * 寄存器计数之和。对称性丢失 (两原子同丢一次更新, 上面的恒等式
     * 看不见) 在这里必然失配。 */
    if (data->local_adds) {
        const int ndev = device_count();
        uint64_t sum = 0;
        for (int i = 0; i < ndev; ++i)
            sum += data->local_adds[i];
        if (c != sum || t != sum) {
            report_fail_msg("cachebounce: bounced atomics (%lu, %lu) != sum of "
                            "per-CPU register golden (%lu) — symmetric lost update",
                            c, t, sum);
            free(data->local_adds);
            data->local_adds = nullptr;
            return EXIT_FAILURE;
        }
    }

    free(data->local_adds);
    data->local_adds = nullptr;
    // 测试通过，框架会自动记录 pass
    return EXIT_SUCCESS;
}

// 使用 group_math（框架已有）
DECLARE_TEST(cachebounce, "Cache line bouncing test for cache coherency")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = cachebounce_init,
    .test_run = cachebounce_run,
    .test_cleanup = cachebounce_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
