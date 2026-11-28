/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b memcpy_l2_pointer_chase_calc
 * @parblock
 * Vector-consumer-chain arm of the pointer-chase memcpy fleet (2026-11-28).
 *
 * FLEET POSITION / FAULT-CLASS ORIGIN: second piece of the memcpy fleet,
 * same lineage as memcpy_l2_pointer_chase{,_ro} — the retired
 * memcpy_l2_cache_size_neon_fp64's single iter_322 hit (2026-11-07 soak:
 * data-miscompare with all forensics fields null, "memcmp_offset() could
 * not locate difference" — the canonical IN-FLIGHT LOAD-corruption
 * signature) plus the same host's six October TLS crashes. The sibling
 * header carries the full account; this arm keeps every mechanism of the
 * base copy arm (shuffled Hamiltonian cycle, per-hop register-direct
 * validation, dst poisoning + round-end memcmp, cycle closure).
 *
 * THE NEW AXIS — vector consumer chain (Eaglebu input, 2026-11-27/28):
 * pure SIMD load/store traffic leaves the vector pipes idle — LSU stalls
 * dominate and the computational fabric is never fed. On every validated
 * hop this arm consumes the loaded word with NEON: a uint64x2 pair
 * {w, w ^ XOR_C} accumulated by wrapping vector add. The chase's
 * dependent long-latency load chain stays fully intact (the consumer
 * rides on the loaded value; the data dependency is still the pin — zero
 * asm, intrinsics only, per the 2026-11-27 minimal-asm directive), while
 * the vector add/eor pipeline is fed between the misses.
 *
 * A/B WITHIN THE FLEET: the base pair isolates load-return (_ro) and
 * load+store (copy arm); _calc adds the consumer chain. A round that
 * passes all per-hop checks, the dst memcmp, AND fails only the
 * accumulator golden localizes the corruption to the vector-path
 * consumption of otherwise clean loads — a signature the base arms
 * cannot produce.
 *
 * ORDER-INVARIANT GOLDEN (no replay): the walk visits every element of
 * the cycle exactly once per round, and wrapping u64 add is commutative
 * and associative, so the accumulator golden is independent of the
 * shuffled visiting order. The golden vector is therefore accumulated IN
 * THE SAME PASS that writes the tags — no scalar replay of the shuffle
 * (re-randomized every round) is ever performed.
 *
 * Per-thread buffers; framework RNG for the per-round shuffle; plain
 * vectors without OOM guard (sibling convention).
 *
 * quality_level BETA: new-design exciter — the calc-pressure form
 * alters the chase's steady-state load latencies; no field validation
 * yet.
 * @endparblock
 */

#include <sandstone.h>
#include "sandstone_p.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

static constexpr uint64_t CHASE_MAGIC = 0xA5A5ULL;    /* GPS tag family */
static constexpr size_t CHASE_BLOCK_CAP = 512 * 1024; /* N<=65536 for 16-bit fields */

/* L2 探测序（randomization hardening H8'，与 memcpy_l2_cache_size /
 * memcpy_l2_pointer_chase 逐字同源） */
static size_t get_l2_size() {
    int ndev = device_count();
    for (int i = 0; i < ndev; ++i) {
        int sz = device_info[i].cache[1].cache_data;   /* cache[1] = L2 */
        if (sz > 0)
            return static_cast<size_t>(sz);
    }
    long size = sysconf(_SC_LEVEL2_CACHE_SIZE);
    if (size > 0) return static_cast<size_t>(size);
    return 256 * 1024;
}

struct ChaseCalcData {
    size_t block_size;
    uint64_t xor_c;   /* per-instance lane1 mixing constant */
};

static int chase_calc_init(struct test *test) {
    /* test_init 里探测——此刻框架 topology 已填充 device_info（H8' 教训） */
    auto *d = new ChaseCalcData;
    if (!d)
        return EXIT_FAILURE;
    d->block_size = std::min(get_l2_size(), CHASE_BLOCK_CAP);
    d->xor_c = random64();
    test->data = d;
    return EXIT_SUCCESS;
}

static int chase_calc_cleanup(struct test *test) {
    delete static_cast<ChaseCalcData *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int chase_calc_run(struct test *test, int cpu) {
    (void)cpu;
    auto *d = static_cast<ChaseCalcData *>(test->data);
    const size_t BLOCK_SIZE = d->block_size;
    const uint64_t XOR_C = d->xor_c;
    const uint32_t N = static_cast<uint32_t>(BLOCK_SIZE / 8);

    std::vector<uint64_t> block(N);   /* 自描述追逐环 */
    std::vector<uint64_t> idx(N);     /* 洗牌后的 Hamiltonian 环序 */
    std::vector<uint64_t> dst(N);     /* 拷贝目的地 */

    uint16_t gen = 0;
    unsigned long rounds = 0;

    do {
        /* 1. Fisher-Yates 洗牌 -> 一条随机 Hamiltonian 环 */
        for (uint32_t i = 0; i < N; ++i)
            idx[i] = i;
        for (uint32_t i = N - 1; i > 0; --i) {
            uint32_t k = random32() % (i + 1);
            uint64_t t = idx[i]; idx[i] = idx[k]; idx[k] = t;
        }

        /* 2. 沿环写自描述字；同一 pass 内累加序不变金身向量（见头注——
         *    追逐对全集恰一次覆盖 + 包加法交换结合，金身与洗牌序无关） */
        uint64x2_t gacc = uint64x2_t{0, 0};
        for (uint32_t k = 0; k < N; ++k) {
            uint32_t pos = static_cast<uint32_t>(idx[k]);
            uint32_t nxt = static_cast<uint32_t>(idx[(k + 1) % N]);
            uint32_t prv = static_cast<uint32_t>(idx[(k + N - 1) % N]);
            const uint64_t word = (CHASE_MAGIC << 48) | (static_cast<uint64_t>(nxt) << 32) |
                         (static_cast<uint64_t>(prv) << 16) | gen;
            block[pos] = word;
            gacc = vaddq_u64(gacc, uint64x2_t{word, word ^ XOR_C});
        }

        /* 3. 目的地全量 0x5A 污染（丢写证人） */
        memset(dst.data(), 0x5A, BLOCK_SIZE);

        /* 4. 追逐行走：N 跳依赖加载，落地即校验 + 向量消费链 */
        uint64x2_t acc = uint64x2_t{0, 0};
        const uint32_t start = random32() % N;
        uint32_t p = start;           /* came-from（第 1 跳起有效） */
        uint32_t j = start;
        for (uint32_t hop = 0; hop < N; ++hop) {
            const uint64_t w = block[j];
            const uint64_t magic = w >> 48;
            const uint32_t nxt = static_cast<uint32_t>(w >> 32) & 0xFFFFu;
            const uint32_t prv = static_cast<uint32_t>(w >> 16) & 0xFFFFu;
            const uint16_t g = static_cast<uint16_t>(w);
            if (magic != CHASE_MAGIC || g != gen || nxt >= N ||
                (hop > 0 && prv != p)) {
                report_fail_msg("%s: hop %u from idx %u landed %u: "
                                 "word 0x%016llx (magic %04llx next %u prev %u gen %u), "
                                 "expected prev %u gen %u, round %lu",
                                 test->id, hop, p, j,
                                 static_cast<unsigned long long>(w),
                                 static_cast<unsigned long long>(magic), nxt, prv, g,
                                 p, gen, rounds);
                return EXIT_FAILURE;
            }
            /* 计算消费链（本件新轴）：加载值喂 NEON 包加累加 */
            acc = vaddq_u64(acc, uint64x2_t{w, w ^ XOR_C});
            dst[j] = w;
            p = j;
            j = nxt;
        }

        /* 5. 环闭合：N 跳后必须回到起点（防偏离兜底） */
        if (j != start) {
            report_fail_msg("%s: cycle unclosed after %u hops: ended at %u, "
                            "expected start %u (round %lu)",
                            test->id, N, j, start, rounds);
            return EXIT_FAILURE;
        }

        /* 6. 轮末全量重读校验（持久 store 面检测） */
        if (memcmp(dst.data(), block.data(), BLOCK_SIZE) != 0) {
            size_t el = 0;
            while (el < N && dst[el] == block[el])
                ++el;
            report_fail_msg("%s: dst vs block mismatch after round %lu: "
                            "element %zu dst 0x%016llx expected 0x%016llx",
                            test->id, rounds, el,
                            static_cast<unsigned long long>(dst[el]),
                            static_cast<unsigned long long>(block[el]));
            return EXIT_FAILURE;
        }

        /* 7. 轮末消费链金身校验：逐跳校验全过而累加器错——损坏在
         *    向量消费路（基础双臂给不出的签名） */
        const uint64_t a0 = vgetq_lane_u64(acc, 0);
        const uint64_t a1 = vgetq_lane_u64(acc, 1);
        const uint64_t g0 = vgetq_lane_u64(gacc, 0);
        const uint64_t g1 = vgetq_lane_u64(gacc, 1);
        if (a0 != g0 || a1 != g1) {
            report_fail_msg("%s: consumer-chain accumulator mismatch after "
                            "round %lu (all %u per-hop checks and the dst "
                            "memcmp passed): lane0 acc 0x%016llx golden 0x%016llx, "
                            "lane1 acc 0x%016llx golden 0x%016llx",
                            test->id, rounds, N,
                            static_cast<unsigned long long>(a0),
                            static_cast<unsigned long long>(g0),
                            static_cast<unsigned long long>(a1),
                            static_cast<unsigned long long>(g1));
            return EXIT_FAILURE;
        }

        ++gen;       /* 16 位轮计数，自然回绕（环上每轮全量重写，无陈字残留） */
        ++rounds;
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(memcpy_l2_pointer_chase_calc, "Vector-consumer-chain arm of the pointer-chase memcpy fleet: same shuffled Hamiltonian-cycle dependent loads, per-hop validation and dst copy as the base arm, plus a NEON consume-and-accumulate step on every hop (vector-pipe pressure interleaved into the long-latency miss chain, Eaglebu mixed-pressure axis); order-invariant write-pass accumulator golden; in-flight load-return / store / vector-path corruption detector")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = chase_calc_init,
    .test_run = chase_calc_run,
    .test_cleanup = chase_calc_cleanup,
    .fracture_loop_count = -1,   /* never fracture: L2-scale rounds */
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
