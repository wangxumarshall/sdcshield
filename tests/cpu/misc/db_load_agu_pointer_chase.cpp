/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b db_load_agu_pointer_chase
 * @parblock
 * AGU / indirect-addressing probe for the database-machine load-fault class
 * (2026-11-27): a sustained-DB workload shows load-side address errors —
 * reading a wrong address, re-reading the previous address, or reading a
 * zero address. This test targets the first and third modes: the pointer
 * (address operand) the load consumes is computed/staged through the NEON
 * vector file, so an AGU miscalculation, a stale address-register capture,
 * or a zero-capture shows up as the chase visiting the wrong node.
 *
 * Construction: a 16-byte node is one q register — lane0 = index of the
 * next node, lane1 = random magic. init builds a single-ring permutation
 * over 16384 nodes (Fisher-Yates over framework random64()), keeps the
 * memset_random magic bytes untouched, and precomputes, for every start
 * node, the expected 64-hop landing node and the expected magic xor-acc.
 * run chases the ring in a pinned asm block (repeated 64x:
 * ldr q -> fmov d[0] -> fmov d[1] -> eor acc -> and mask -> lsl -> add),
 * i.e. the "Load -> extract address -> Load again" chain the fault report
 * describes, with the address operand captured from the NEON lane into the
 * GPR domain every hop.
 *
 * Safety-by-masking (deliberate deviation from the naive design): the
 * extracted "next" value is clamped with `and #0x3fff` inside the asm
 * block. A corrupted pointer therefore NEVER dereferences outside the
 * node array — a raw SIGSEGV would kill the worker without leaving a
 * report_fail_msg, which is worthless as an SDC detection. Every fault is
 * forced to surface as a value mismatch at the post-chase check instead:
 * wrong end node / wrong xor-acc / periodic full-region memcmp vs golden
 * (the region has no writer, so any content change is a write-path SDC).
 *
 * A zero-address capture reads back next=0, which redirects the chase to
 * node 0 and derails the end-node and acc checks immediately — the
 * zero-idiom mode is caught by the same value-mismatch funnel.
 *
 * quality_level BETA: new-design exciter, no field validation yet; do not
 * promote to PROD before target-machine evidence (balsn_2 precedent).
 * @endparblock
 */

#include "sandstone.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace {

constexpr int      DB_LAPC_NODES_LOG2 = 14;
constexpr uint64_t DB_LAPC_NODES      = 1ull << DB_LAPC_NODES_LOG2;   /* 16384 nodes */
constexpr uint64_t DB_LAPC_NODE_BYTES = 16;                            /* one q register */
constexpr uint64_t DB_LAPC_CHASE      = 64;                            /* hops per sweep */
constexpr uint64_t DB_LAPC_REGION_BYTES = DB_LAPC_NODES * DB_LAPC_NODE_BYTES;

/* magic 0x3fff == DB_LAPC_NODES - 1: clamp keeps every (possibly corrupt)
 * "next" index inside the region; legitimate ring indices are untouched. */
constexpr uint64_t DB_LAPC_IDX_MASK   = DB_LAPC_NODES - 1;

struct Node {
    uint64_t next;   /* lane0: index of the ring successor */
    uint64_t magic;  /* lane1: untouched memset_random bytes */
};

struct DbLoadAguPointerChaseData {
    Node     *nodes  = nullptr;   /* the ring, read-only during run */
    Node     *golden = nullptr;
    uint64_t *end64  = nullptr;   /* per-start expected landing index  */
    uint64_t *acc64  = nullptr;   /* per-start expected magic xor-acc  */
};

} // namespace

static int db_load_agu_pointer_chase_init(struct test *test)
{
    auto *data = static_cast<DbLoadAguPointerChaseData *>(calloc(1, sizeof(DbLoadAguPointerChaseData)));
    if (!data)
        return -ENOMEM;

    data->nodes  = static_cast<Node *>(aligned_alloc(64, DB_LAPC_REGION_BYTES));
    data->golden = static_cast<Node *>(aligned_alloc(64, DB_LAPC_REGION_BYTES));
    data->end64 = static_cast<uint64_t *>(calloc(DB_LAPC_NODES, sizeof(uint64_t)));
    data->acc64 = static_cast<uint64_t *>(calloc(DB_LAPC_NODES, sizeof(uint64_t)));
    if (!data->nodes || !data->golden || !data->end64 || !data->acc64) {
        test->data = data;
        return -ENOMEM;
    }

    /* Random magic bytes first (framework RNG), then structure the ring on
     * top of lane0 only — lane1 keeps its random contents verbatim. */
    memset_random(data->nodes, DB_LAPC_REGION_BYTES);

    /* Single-ring permutation via Fisher-Yates over random64(). */
    uint64_t *perm = static_cast<uint64_t *>(malloc(DB_LAPC_NODES * sizeof(uint64_t)));
    if (!perm) {
        test->data = data;
        return -ENOMEM;
    }
    for (uint64_t i = 0; i < DB_LAPC_NODES; ++i)
        perm[i] = i;
    for (uint64_t i = DB_LAPC_NODES - 1; i > 0; --i) {
        uint64_t j = random64() % (i + 1);
        uint64_t t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }

    Node *nodes = data->nodes;
    for (uint64_t i = 0; i < DB_LAPC_NODES; ++i)
        nodes[perm[i]].next = perm[(i + 1) % DB_LAPC_NODES];

    memcpy(data->golden, nodes, DB_LAPC_REGION_BYTES);

    /* Per-start expectations via a sliding xor window over the ring order.
     * end64/acc64[s] describe the 64-hop chase starting at node s. */
    uint64_t acc = 0;
    for (uint64_t k = 0; k < DB_LAPC_CHASE; ++k)
        acc ^= nodes[perm[k]].magic;
    data->end64[perm[0]] = perm[DB_LAPC_CHASE];
    data->acc64[perm[0]] = acc;
    for (uint64_t i = 1; i < DB_LAPC_NODES; ++i) {
        acc ^= nodes[perm[i - 1]].magic ^ nodes[perm[(i + DB_LAPC_CHASE - 1) % DB_LAPC_NODES]].magic;
        data->end64[perm[i]] = perm[(i + DB_LAPC_CHASE) % DB_LAPC_NODES];
        data->acc64[perm[i]] = acc;
    }

    free(perm);

    test->data = data;
    return EXIT_SUCCESS;
}

static int db_load_agu_pointer_chase_run(struct test *test, int cpu)
{
    auto *data = static_cast<DbLoadAguPointerChaseData *>(test->data);
    Node *nodes = data->nodes;
    const uint64_t base = reinterpret_cast<uint64_t>(nodes);

    uint64_t start = static_cast<uint64_t>(cpu);   /* per-worker phase */
    uint64_t iter = 0;

    TEST_LOOP(test, 1 << 6) {
        const uint64_t s = start & DB_LAPC_IDX_MASK;
        uint64_t cur = base + (s * DB_LAPC_NODE_BYTES);
        uint64_t acc = 0;

        /* Pinned chase: 64x (ldr q -> extract both lanes -> eor acc ->
         * clamp -> scale -> base add). One asm block, "memory" clobber:
         * the compiler may not reorder anything across the chase. */
        __asm__ volatile(
            ".rept 64\n\t"
            "ldr q0, [%[cur]]\n\t"
            "fmov x1, d0\n\t"
            "fmov x2, v0.d[1]\n\t"
            "eor %[acc], %[acc], x2\n\t"
            "and x1, x1, #0x3fff\n\t"
            "lsl x1, x1, #4\n\t"
            "add %[cur], %[base], x1\n\t"
            ".endr\n\t"
            : [cur] "+r"(cur), [acc] "+r"(acc)
            : [base] "r"(base)
            : "v0", "x1", "x2", "memory", "cc");

        const uint64_t end_idx = (cur - base) / DB_LAPC_NODE_BYTES;
        if (end_idx != data->end64[s] || acc != data->acc64[s]) {
            report_fail_msg("db_load_agu_pointer_chase: chase derailed from start node %llu "
                            "(wrong-address / stale / zero capture on the load path): "
                            "landed on %llu, expected %llu; magic-acc 0x%016llx, expected 0x%016llx",
                            static_cast<unsigned long long>(s),
                            static_cast<unsigned long long>(end_idx),
                            static_cast<unsigned long long>(data->end64[s]),
                            static_cast<unsigned long long>(acc),
                            static_cast<unsigned long long>(data->acc64[s]));
        }

        ++start;
        /* The ring has no writer: any content drift is a write-path SDC. */
        if ((++iter & 63) == 0 &&
            memcmp(nodes, data->golden, DB_LAPC_REGION_BYTES) != 0) {
            report_fail_msg("db_load_agu_pointer_chase: node region drifted from golden "
                            "(write-path SDC on a writer-free region)");
        }
    }

    return EXIT_SUCCESS;
}

static int db_load_agu_pointer_chase_cleanup(struct test *test)
{
    auto *data = static_cast<DbLoadAguPointerChaseData *>(test->data);
    if (data) {
        free(data->nodes);
        free(data->golden);
        free(data->end64);
        free(data->acc64);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(db_load_agu_pointer_chase,
             "Database load-fault AGU probe: NEON-constructed single-ring pointer chase (ldr q -> fmov lane -> ldr q, address operand re-captured from the vector file every hop), wrong-address / stale / zero captures forced in-region by an and-clamp and surfaced as end-node + magic-xor mismatches vs precomputed goldens")
    .test_init    = db_load_agu_pointer_chase_init,
    .test_run     = db_load_agu_pointer_chase_run,
    .test_cleanup = db_load_agu_pointer_chase_cleanup,
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
