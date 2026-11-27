/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b memcpy_l2_pointer_chase
 * @test @b memcpy_l2_pointer_chase_ro
 * @parblock
 * Pointer-chase (random-jump, latency-type) twins for the memcpy fleet.
 *
 * FAULT-CLASS ORIGIN (why this axis exists): the retired
 * memcpy_l2_cache_size_neon_fp64 fired exactly once on the 128-core
 * copy-detector host (2026-11-07 soak, run-logs/neonfp64_full_soak_20261107-131426/
 * iter_322): data-miscompare with offset/actual/expected/mask all null and
 * "memcmp_offset() could not locate difference" — the canonical signature of
 * an IN-FLIGHT LOAD corruption (first compare saw the difference, the
 * re-scan read the same clean memory and found none; the corruption lived
 * only in the load's return path).  The same host's six October TLS crashes
 * are the sibling manifestation (in-flight garbage consumed as a pointer ->
 * SIGSEGV).  This pair is the class detector for that fault family; it does
 * NOT aim to re-trigger that retired host's instance (a follow-up week of
 * soaks added zero hits — the defect was episodic, disclosed honestly).
 *
 * REGISTER-DIRECT FAIL SIGNATURE (the null-signature killer): every hop
 * validates the word it just loaded IMMEDIATELY, and the fail message
 * prints the raw loaded word plus decoded magic/next/prev/gen, the
 * came-from index, the landed index, and the round — all taken from the
 * in-register value at fail time.  A transient in-flight corruption needs
 * no memory re-scan to be attributed: the evidence is the loaded word
 * itself.  (A persistent corruption — e.g. a lost round-init store — is
 * caught by the same per-hop checks via the gen field.)
 *
 * MECHANISM — dependent long-latency loads by geometry, no asm: each round
 * shuffles the L2-sized block into ONE random Hamiltonian cycle; the walk's
 * next address is a field of the word just loaded, so the compiler and the
 * prefetcher cannot break the chain (data-dependent addresses), and the
 * random order over a per-core-L2-capacity block self-thrashes L2 — most
 * hops are genuine L3/DRAM misses with the load in flight for its full
 * latency.  Design correction recorded honestly: an earlier draft proposed
 * dc-civac eviction AHEAD of the chase frontier — impossible, future
 * targets are data-dependent and unknowable in advance; capacity
 * self-thrash achieves the same latency profile without any eviction
 * plumbing.  Zero inline asm: the data dependency is the pin.
 *
 * SELF-DESCRIBING TAGS (GPS tag-family lineage, magic 0xA5A5): each 64-bit
 * word = magic<<48 | NEXT<<32 | PREV<<16 | GEN.  PREV is the unique
 * predecessor of this element in the cycle; a corrupted NEXT that lands on
 * a valid in-range element fails the PREV check at the landing hop,
 * because in a Hamiltonian cycle exactly one element has PREV == the
 * index we came from (the true successor) — in-range derailment cannot
 * pass.  GEN (16-bit round counter, natural wrap) catches any stale word
 * from an earlier round.  Hop 0 validates magic/gen/bounds only (the
 * came-from is unknown before the first load); hops 1..N-1 validate
 * PREV as well; after exactly N hops the walk must be back at the start
 * (cycle-closure check).
 *
 * BOUNDS: block = min(probed L2, 512KB) so N = block/8 <= 65536 fits the
 * 16-bit index fields.  On hosts with N < 65536 the NEXT field can decode
 * out of range and an explicit nxt >= N check fires before any deref; at
 * N == 65536 the check is vacuously true-free (16-bit domain is exactly
 * the index range) and PREV carries the detection.
 *
 * THE TWO ARMS (one axis — the store — varied, fleet A/B discipline):
 *   - memcpy_l2_pointer_chase:    the copy arm.  Each validated hop also
 *     stores the word to dst[j] (poisoned 0x5A each round — a lost store
 *     leaves the poison and fails the end memcmp); round-end full memcmp
 *     (dst vs block) adds the persistent store-path detection surface, so
 *     the arm covers load-return AND store data paths.
 *   - memcpy_l2_pointer_chase_ro: the read-only control.  Identical chain,
 *     geometry and per-hop validation, no dst and no end memcmp — the
 *     load-return path in isolation.  A hit on _ro but not on the copy
 *     arm localizes the fault to the load path.
 *
 * Per-thread buffers (no cross-thread races by construction); framework
 * RNG (per-thread streams, -s reproducible); allocation follows the
 * sibling memcpy_l2_cache_size convention (plain vectors, no OOM guard).
 * L2 probe order and the test_init-timing rule (H8') are the sibling's.
 * @endparblock
 */

#include <sandstone.h>
#include "sandstone_p.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static constexpr uint64_t CHASE_MAGIC = 0xA5A5ULL;    /* GPS tag family */
static constexpr size_t CHASE_BLOCK_CAP = 512 * 1024; /* N<=65536 for 16-bit fields */

// 获取 L2 缓存大小（字节）。探测顺序（randomization hardening H8'，
// 与 memcpy_l2_cache_size 逐字同源）：
//   1. 框架 device_info[].cache[]（sysfs 填充）
//   2. sysconf(_SC_LEVEL2_CACHE_SIZE)
//   3. 256KB 兜底
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

static int chase_init(struct test *test) {
    /* test_init 里探测——此刻框架 topology 已填充 device_info（H8' 教训：
     * 静态全局初始化发生在 main 之前，探测会静默落回兜底值）。 */
    auto *block = new size_t(std::min(get_l2_size(), CHASE_BLOCK_CAP));
    if (!block)
        return EXIT_FAILURE;
    test->data = block;
    return EXIT_SUCCESS;
}

static int chase_cleanup(struct test *test) {
    delete static_cast<size_t *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

// 双臂共用行走引擎；do_copy = 拷贝臂（存 dst + 轮末 memcmp），
// false = 纯读对照臂（load 返回路隔离）。
static int chase_run_common(struct test *test, int cpu, bool do_copy) {
    (void)cpu;
    const size_t BLOCK_SIZE = *static_cast<size_t *>(test->data);
    const uint32_t N = static_cast<uint32_t>(BLOCK_SIZE / 8);

    std::vector<uint64_t> block(N);   /* 自描述追逐环 */
    std::vector<uint64_t> idx(N);     /* 洗牌后的 Hamiltonian 环序 */
    std::vector<uint64_t> dst(do_copy ? N : 0);

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

        /* 2. 沿环写自描述字：pos 的 NEXT=环上后继、PREV=环上前驱 */
        for (uint32_t k = 0; k < N; ++k) {
            uint32_t pos = static_cast<uint32_t>(idx[k]);
            uint32_t nxt = static_cast<uint32_t>(idx[(k + 1) % N]);
            uint32_t prv = static_cast<uint32_t>(idx[(k + N - 1) % N]);
            block[pos] = (CHASE_MAGIC << 48) | (static_cast<uint64_t>(nxt) << 32) |
                         (static_cast<uint64_t>(prv) << 16) | gen;
        }

        /* 3. 拷贝臂：目的地全量 0x5A 污染（丢写证人） */
        if (do_copy)
            memset(dst.data(), 0x5A, BLOCK_SIZE);

        /* 4. 追逐行走：N 跳依赖加载，落地即校验（寄存器直出签名） */
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
            if (do_copy)
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

        /* 6. 拷贝臂轮末全量重读校验（持久 store 面检测） */
        if (do_copy && memcmp(dst.data(), block.data(), BLOCK_SIZE) != 0) {
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

        ++gen;       /* 16 位轮计数，自然回绕（环上每轮全量重写，无陈字残留） */
        ++rounds;
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int chase_copy_run(struct test *test, int cpu) {
    return chase_run_common(test, cpu, true);
}

static int chase_ro_run(struct test *test, int cpu) {
    return chase_run_common(test, cpu, false);
}

DECLARE_TEST(memcpy_l2_pointer_chase, "Full-block copy in shuffled pointer-chase order: L2-sized block (capped 512KB) as one random Hamiltonian cycle of self-describing magic/next/prev/gen tags, per-hop validation with register-direct fail signature + end memcmp (dependent long-latency loads; in-flight load-return corruption detector)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = chase_init,
    .test_run = chase_copy_run,
    .test_cleanup = chase_cleanup,
    .fracture_loop_count = -1,   /* never fracture: L2-scale rounds */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(memcpy_l2_pointer_chase_ro, "Read-only twin of memcpy_l2_pointer_chase: same chain, geometry and per-hop validation, no dst store and no end memcmp (load-return path isolation control arm)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = chase_init,
    .test_run = chase_ro_run,
    .test_cleanup = chase_cleanup,
    .fracture_loop_count = -1,   /* never fracture: L2-scale rounds */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
