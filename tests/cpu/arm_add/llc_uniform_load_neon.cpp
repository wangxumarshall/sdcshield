/*  llc_uniform_load_neon — NEON variant of tests/cpu/cache/
 *  llc_uniform_load.cpp (same-day NEON pass; original untouched).
 *
 *  The uniform stride becomes a VECTOR stride: one uint32x4 load
 *  every 128 bytes across the 32 MiB window (the stride that trains
 *  the prefetcher — the #32 stress), walk start rotating by one
 *  vector per pass.  Verification: the loaded vector is stored at
 *  the same index and read back through inline asm "ldr q"
 *  (anti-forwarding — intrinsic store->load pairs can still be
 *  register-folded), bit-compared with veorq/vmaxvq.
 */
#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <arm_neon.h>

static constexpr size_t LLULN_BUFFER_SIZE = 32 * 1024 * 1024;
static constexpr size_t LLULN_NUM_VECS    = LLULN_BUFFER_SIZE / 16;
static constexpr size_t LLULN_STRIDE_VECS = 128 / 16;   /* one vec per 128B */

static inline uint32x4_t ldr_vec(const uint32_t *addr) {
    uint32x4_t v;
    __asm__ volatile("ldr %q0, [%1]" : "=w"(v) : "r"(addr) : "memory");
    return v;
}

static int llc_uniform_load_neon_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_uniform_load_neon_cleanup(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_uniform_load_neon_run(struct test *test, int cpu) {
    (void)cpu;

    std::vector<uint32_t> data(LLULN_NUM_VECS * 4);
    std::vector<uint32_t> check(LLULN_NUM_VECS * 4);

    memset_random(data.data(), LLULN_BUFFER_SIZE);
    memset(check.data(), 0, LLULN_BUFFER_SIZE);

    size_t offset = random64() % LLULN_STRIDE_VECS;

    do {
        for (size_t v = offset; v < LLULN_NUM_VECS; v += LLULN_STRIDE_VECS) {
            uint32_t *dp = data.data() + v * 4;
            uint32_t *cp = check.data() + v * 4;

            /* uniform strided vector load — prefetcher stress */
            uint32x4_t original = vld1q_u32(dp);

            /* store -> REAL vector load-back through the LSU */
            vst1q_u32(cp, original);
            uint32x4_t loaded = ldr_vec(cp);

            uint32x4_t diff = veorq_u32(original, loaded);
            if (vmaxvq_u32(diff) != 0) {
                report_fail_msg("llc_uniform_load_neon: store->load "
                                "mismatch at vec %zu: stored {%08x,"
                                "%08x,%08x,%08x} loaded {%08x,%08x,"
                                "%08x,%08x} (LSU forwarding path under "
                                "strided access)",
                                v,
                                vgetq_lane_u32(original, 0), vgetq_lane_u32(original, 1),
                                vgetq_lane_u32(original, 2), vgetq_lane_u32(original, 3),
                                vgetq_lane_u32(loaded, 0), vgetq_lane_u32(loaded, 1),
                                vgetq_lane_u32(loaded, 2), vgetq_lane_u32(loaded, 3));
                return EXIT_FAILURE;
            }
        }
        offset = (offset + 1) % LLULN_STRIDE_VECS;
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(llc_uniform_load_neon, "Uniform 128B-strided vld1q vector loads over 32MiB (prefetcher stress); store->asm-ldr-q vector load-back bit verification (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = llc_uniform_load_neon_init,
    .test_run = llc_uniform_load_neon_run,
    .test_cleanup = llc_uniform_load_neon_cleanup,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST
