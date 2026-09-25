/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b insert_extract_sve
 * @parblock
 * SVE port of insert_extract: random lane insert + extract round-trip on
 * an SVE vector. Insert = predicate-gated single-lane write (svsel with a
 * single-lane predicate); extract = svlastb on the shifted vector or a
 * predicate-gated read. Store/load consistency via svst1/svld1 + svcmpeq.
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <random>
#include <cstring>
#include <atomic>

#ifdef __aarch64__
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

#define IE_LANES 4

struct test_data_sve {
    float mem_buf[16];
};

static int insert_extract_sve_init(struct test *test) {
    (void)test;
#ifdef __aarch64__
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for insert_extract_sve");
        return EXIT_SKIP;
    }
#endif
    auto *data = new test_data_sve;
    std::mt19937 rng(std::random_device{}());
    (void)rng;
    memset(data->mem_buf, 0, sizeof(data->mem_buf));
    test->data = data;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__
static int insert_extract_sve_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<test_data_sve *>(test->data);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    static std::atomic<uint64_t> iter{0};

    do {
        // 1. 生成原始向量 (4 lane, 与原版一致)
        float orig_vals[IE_LANES];
        for (int i = 0; i < IE_LANES; ++i) orig_vals[i] = dist(rng);
        svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)IE_LANES);
        svfloat32_t orig_vec = svld1_f32(pg, orig_vals);

        // 2. 随机选择 lane 和插入值
        int lane = (int)(rng() % IE_LANES);
        float insert_val = dist(rng);

        // 3. SVE 插入: 单 lane 谓词选择 (svsel) —— vsetq_lane 的谓词等价
        uint32_t lane_sel[IE_LANES];
        for (int i = 0; i < IE_LANES; ++i) lane_sel[i] = (i == lane) ? 1u : 0u;
        svbool_t lane_p = svcmpne_n_u32(pg, svld1_u32(pg, lane_sel), 0);
        svfloat32_t ins_vec = svdup_f32(insert_val);
        svfloat32_t new_vec = svsel_f32(lane_p, ins_vec, orig_vec);

        // SVE 提取: 谓词控读回该 lane (vgetq_lane 的等价读)
        float new_vals[IE_LANES];
        svst1_f32(pg, new_vals, new_vec);
        float extracted = new_vals[lane];

        // 4. 验证插入/提取一致性
        bool insert_extract_ok = (extracted == insert_val);

        // 5. Store/Load 一致性测试
        svst1_f32(pg, data->mem_buf, orig_vec);
        svfloat32_t loaded_vec = svld1_f32(pg, data->mem_buf);
        bool store_load_ok = (svcntp_b32(pg, svcmpeq_f32(pg, orig_vec, loaded_vec)) == IE_LANES);

        bool passed = insert_extract_ok && store_load_ok;

        uint64_t iteration = iter.fetch_add(1, std::memory_order_relaxed);
        const char *color = passed ? "\033[32m" : "\033[31m";
        const char *result_str = passed ? "PASS" : "FAIL";

        fprintf(stderr, "insert_extract_sve: Iter %lu, lane=%d, insert_val=%.2f, extracted=%.2f, result=%s%s%s\n",
                iteration, lane, insert_val, extracted, color, result_str, "\033[0m");
        fflush(stderr);

        if (!passed) {
            report_fail_msg("insert_extract_sve: insert/extract or store/load consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}
#else
static int insert_extract_sve_run(struct test *test, int cpu) {
    (void)test; (void)cpu;
    return EXIT_SKIP;
}
#endif

static int insert_extract_sve_finish(struct test *test) {
    auto *data = static_cast<test_data_sve *>(test->data);
    delete data;
    return EXIT_SUCCESS;
}

DECLARE_TEST(insert_extract_sve, "SVE lane insert/extract round-trip via predicate-gated select (port of insert_extract)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = insert_extract_sve_init,
    .test_run = insert_extract_sve_run,
    .test_cleanup = insert_extract_sve_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
