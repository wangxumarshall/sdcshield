/*  llc_randomized_load_neon — NEON variant of tests/cpu/cache/
 *  llc_randomized_load.cpp (same-day NEON pass; original untouched).
 *
 *  Same two-role structure, widened to the 16-byte NEON datapath:
 *   - data[]: RANDOM-INDEX vld1q_u32 loads over the 32 MiB window
 *     (random LLC roaming, the #31 pressure);
 *   - check[]: the loaded VECTOR is stored at the same random index
 *     and read back THROUGH INLINE ASM "ldr q" — vector loads via
 *     intrinsics after an intrinsic store CAN still be register-
 *     forwarded by GCC, the same folding trap as the scalar pair;
 *     the asm ldr cannot.  Bit-compared with veorq/vmaxvq.
 */
#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <arm_neon.h>

static constexpr size_t LLRLN_BUFFER_SIZE = 32 * 1024 * 1024;
static constexpr size_t LLRLN_NUM_VECS    = LLRLN_BUFFER_SIZE / 16;

/* anti-forwarding VECTOR load: "w" constraint pins a NEON register */
static inline uint32x4_t ldr_vec(const uint32_t *addr) {
    uint32x4_t v;
    __asm__ volatile("ldr %q0, [%1]" : "=w"(v) : "r"(addr) : "memory");
    return v;
}

static int llc_randomized_load_neon_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_randomized_load_neon_cleanup(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int llc_randomized_load_neon_run(struct test *test, int cpu) {
    (void)cpu;

    std::vector<uint32_t> data(LLRLN_NUM_VECS * 4);   /* random-index loads */
    std::vector<uint32_t> check(LLRLN_NUM_VECS * 4);  /* store->load verify */

    memset_random(data.data(), LLRLN_BUFFER_SIZE);
    memset(check.data(), 0, LLRLN_BUFFER_SIZE);

    do {
        size_t vidx = random64() % LLRLN_NUM_VECS;
        uint32_t *dp = data.data() + vidx * 4;
        uint32_t *cp = check.data() + vidx * 4;

        /* randomized LLC vector load — the pressure */
        uint32x4_t original = vld1q_u32(dp);

        /* store -> REAL vector load-back through the LSU */
        vst1q_u32(cp, original);
        uint32x4_t loaded = ldr_vec(cp);

        uint32x4_t diff = veorq_u32(original, loaded);
        if (vmaxvq_u32(diff) != 0) {
            report_fail_msg("llc_randomized_load_neon: store->load "
                            "mismatch at vec %zu: stored {%08x,%08x,"
                            "%08x,%08x} loaded {%08x,%08x,%08x,%08x} "
                            "(LSU forwarding path)",
                            vidx,
                            vgetq_lane_u32(original, 0), vgetq_lane_u32(original, 1),
                            vgetq_lane_u32(original, 2), vgetq_lane_u32(original, 3),
                            vgetq_lane_u32(loaded, 0), vgetq_lane_u32(loaded, 1),
                            vgetq_lane_u32(loaded, 2), vgetq_lane_u32(loaded, 3));
            return EXIT_FAILURE;
        }
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(llc_randomized_load_neon, "Random LLC roaming with vld1q vector loads over 32MiB; store->asm-ldr-q vector load-back bit verification on the LSU forwarding path (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = llc_randomized_load_neon_init,
    .test_run = llc_randomized_load_neon_run,
    .test_cleanup = llc_randomized_load_neon_cleanup,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST
