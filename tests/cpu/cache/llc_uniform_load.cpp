/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test llc_uniform_load
 * @parblock
 * Stress LLC with persistent uniform strided loads (633 #32 lineage,
 * user spec 2026-11-02).
 *
 * The uniform-strided sibling of llc_randomized_load: a fixed
 * 128-byte stride walks two 32 MiB per-thread windows (above LLC, so
 * the stride trains the hardware prefetcher — the exact behaviour the
 * 633 #32 description asks to stress) and the walk start rotates by
 * one element per full pass for pattern diversity.
 *
 *   - data[]: strided LOADS only (memset_random-filled once) — the
 *     pure strided-load pressure;
 *   - check[]: the store->load verification path — the loaded value
 *     is stored at the same index and read back THROUGH INLINE ASM
 *     LDR, then bit-compared.
 *
 * WHY THE ASM LOAD (same measured reasoning as
 * llc_randomized_load): the draft's plain-C++ read-back
 * (check[idx] = v; loaded = check[idx];) is forwarded through the
 * register file by GCC -O2 and the verification folds to constant
 * true.  The volatile asm ldr really traverses the LSU's
 * store-buffer forwarding path — the genuine detection surface, and
 * one the prefetcher cannot mask (forwarding happens before any
 * prefetch interaction).
 *
 * Data is bit-pattern random (memset_random) — wider than the
 * draft's uniform-random floats; comparison is bitwise.  Per-thread
 * buffers: no cross-thread races.  Adaptations vs the draft (same
 * set as the family): framework RNG, no per-fail fprintf,
 * .fracture_loop_count = -1, _cleanup naming.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <vector>

static constexpr size_t LLUL_BUFFER_SIZE = 32 * 1024 * 1024;   /* 32 MiB */
static constexpr size_t LLUL_NUM_WORDS   = LLUL_BUFFER_SIZE / sizeof(uint32_t);
static constexpr size_t LLUL_STRIDE      = 128 / sizeof(uint32_t);  /* 128B stride */

/* anti-forwarding load: really goes to the LSU, not the register file */
static inline uint32_t llul_ldr(const uint32_t *addr) {
    uint32_t v;
    __asm__ volatile("ldr %w0, [%1]" : "=r"(v) : "r"(addr) : "memory");
    return v;
}

static int llc_uniform_load_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_uniform_load_cleanup(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_uniform_load_run(struct test *test, int cpu) {
    (void)cpu;

    std::vector<uint32_t> data(LLUL_NUM_WORDS);   /* strided load source   */
    std::vector<uint32_t> check(LLUL_NUM_WORDS);  /* store->load window    */

    memset_random(data.data(), LLUL_BUFFER_SIZE);
    memset(check.data(), 0, LLUL_BUFFER_SIZE);

    /* random walk start; rotates by one element each full pass */
    size_t offset = random64() % LLUL_STRIDE;

    do {
        for (size_t idx = offset; idx < LLUL_NUM_WORDS; idx += LLUL_STRIDE) {
            /* uniform strided load — trains/stresses the prefetcher */
            uint32_t original = data[idx];

            /* store -> REAL load-back through the LSU forwarding path */
            check[idx] = original;
            uint32_t loaded = llul_ldr(&check[idx]);

            if (memcmp(&original, &loaded, sizeof(uint32_t)) != 0) {
                report_fail_msg("llc_uniform_load: store->load "
                                "consistency failure at idx %zu: stored "
                                "0x%08x loaded 0x%08x (LSU forwarding "
                                "path under strided access)",
                                idx, original, loaded);
                return EXIT_FAILURE;
            }
        }
        offset = (offset + 1) % LLUL_STRIDE;
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(llc_uniform_load, "Stress LLC with persistent uniform 128B-strided loads over 32MiB windows (prefetcher stress); store->asm-ldr load-back bit verification on the LSU forwarding path (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = llc_uniform_load_init,
    .test_run = llc_uniform_load_run,
    .test_cleanup = llc_uniform_load_cleanup,
    .fracture_loop_count = -1,   /* never fracture: fast semantic rounds */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST
