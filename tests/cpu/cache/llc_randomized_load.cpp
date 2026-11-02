/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test llc_randomized_load
 * @parblock
 * Stress LLC with persistent randomized loads (633 #31 lineage, user
 * spec 2026-11-02).
 *
 * Two roles per thread, both over 32 MiB windows (larger than LLC, so
 * random indices keep LLC hit rates low and the replacement policy
 * under pressure — the spec's "persistent randomized loads"):
 *   - data[]: random-index LOADS only (memset_random-filled once) —
 *     the pure randomized-load pressure the 633 #31 description asks
 *     for;
 *   - check[]: the store->load verification path — the loaded value
 *     is stored at the same random index and read back THROUGH INLINE
 *     ASM LDR, then bit-compared.
 *
 * WHY THE ASM LOAD (critical, measured reasoning): the user's draft
 * read back with a plain C++ expression (check[idx] = v; loaded =
 * check[idx];) — GCC -O2 forwards the store to the load through the
 * register file (dead-store elimination) and the "verification"
 * compiles to a constant true.  The volatile asm ldr cannot be
 * forwarded, so the read really traverses the store-buffer
 * forwarding path of the LSU — a genuine SDC detection surface (the
 * movbe/locv family's surface).
 *
 * Data is bit-pattern random (memset_random), wider than the draft's
 * uniform-random floats (NaN/Inf/denormal lanes included; comparison
 * is bitwise memcmp so nothing is canonicalised away).
 *
 * Per-thread buffers: no cross-thread races by construction.
 * Adaptations vs the draft (same set as the _2 pair): framework RNG,
 * no per-fail fprintf (report_fail_msg carries the detail),
 * .fracture_loop_count = -1, _cleanup naming.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <vector>

// 32 MiB per buffer, per thread — above LLC so random indices miss
static constexpr size_t LLRL_BUFFER_SIZE = 32 * 1024 * 1024;
static constexpr size_t LLRL_NUM_WORDS   = LLRL_BUFFER_SIZE / sizeof(uint32_t);

/* anti-forwarding load: really goes to the LSU, not the register file */
static inline uint32_t llrl_ldr(const uint32_t *addr) {
    uint32_t v;
    __asm__ volatile("ldr %w0, [%1]" : "=r"(v) : "r"(addr) : "memory");
    return v;
}

static int llc_randomized_load_init(struct test *test) {
    (void)test;
    /* per-thread buffers are allocated in run(); nothing shared */
    return EXIT_SUCCESS;
}

static int llc_randomized_load_cleanup(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_randomized_load_run(struct test *test, int cpu) {
    (void)cpu;

    std::vector<uint32_t> data(LLRL_NUM_WORDS);   /* random-index load source  */
    std::vector<uint32_t> check(LLRL_NUM_WORDS);  /* store->load verify window */

    /* bit-pattern random fill (NaN/Inf/denormal lanes included); the
     * comparison is bitwise, so no float semantics can hide a flip */
    memset_random(data.data(), LLRL_BUFFER_SIZE);
    memset(check.data(), 0, LLRL_BUFFER_SIZE);

    do {
        size_t idx = random64() % LLRL_NUM_WORDS;

        /* randomized LLC load — the pressure the spec asks for */
        uint32_t original = data[idx];

        /* store -> REAL load-back through the LSU forwarding path */
        check[idx] = original;
        uint32_t loaded = llrl_ldr(&check[idx]);

        if (memcmp(&original, &loaded, sizeof(uint32_t)) != 0) {
            report_fail_msg("llc_randomized_load: store->load "
                            "consistency failure at idx %zu: stored "
                            "0x%08x loaded 0x%08x (LSU forwarding path)",
                            idx, original, loaded);
            return EXIT_FAILURE;
        }
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(llc_randomized_load, "Stress LLC with persistent randomized loads over 32MiB windows; store->asm-ldr load-back bit verification on the LSU forwarding path (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = llc_randomized_load_init,
    .test_run = llc_randomized_load_run,
    .test_cleanup = llc_randomized_load_cleanup,
    .fracture_loop_count = -1,   /* never fracture: fast semantic rounds */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST
