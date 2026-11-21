/*  ma_l2evict_rev — L1D-evict reload-path probe, NEON lane structure.
 *  NEON port of tests/cpu/misc/movbe_l1d_vs_l2.cpp (movbe_l2): the scalar
 *  `ldr/rev/str/ldr/cmp` round-trip with `dc civac + dsb sy` evicting the
 *  input line before the reload.  Here the 32-bit byte-swap becomes
 *  vrev32q_u32 on uint32x4 lanes and every load is explicit inline asm
 *  (NEON DCE trap: reloads must be asm, see neon-rot-2src campaign lore).
 *
 *  Purpose: the scalar negative controls (movbe_l2 / mrn_rmw_l1d /
 *  cache_stress_civac) were all 0-fail on core 179 — the defect fires in
 *  the L1D/forwarding domain only.  The trigger recipe, however, needs
 *  the NEON 128-bit/lane load structure (scalar64 = 0/30min).  This cell
 *  crosses the two: evict-path (reload served from L2/L3) + NEON lane
 *  structure.  Pre-registered expectation: 0 fail — if the L2/L3 reload
 *  path stays silent even under the NEON lane structure, "L1D-domain
 *  reload" is necessary independent of lane width.
 *
 *  Known confound (kept deliberately, inherited from the originals):
 *  civac+dsb both evicts AND stretches str→ldr timing — a 0-fail cannot
 *  be cleanly attributed to "bypassing L1D" (intervention-decoupling).
 *  Golden is an init-time scalar snapshot of input (issue #373/374: a
 *  self-inverse transform must not verify against itself).
 */
#include <sandstone.h>
#include <arm_neon.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)

#define MA_L2EVICT_REV_WORDS (16 * 1024 / 4)   /* 4096 words = 16 KiB window */

namespace {

struct MaL2EvictRevData {
    uint32_t input[MA_L2EVICT_REV_WORDS];     /* random source          */
    uint32_t swapped[MA_L2EVICT_REV_WORDS];   /* bswap store target     */
    uint32_t golden[MA_L2EVICT_REV_WORDS];    /* scalar init snapshot   */
};

static inline uint32x4_t ma_rev_load(const uint32_t *addr) {
    uint32x4_t res;
    __asm__ volatile("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* per-32-bit-word byte swap (the NEON analog of __builtin_bswap32):
 * vrev32q_u8 on a byte view — there is no vrev32q_u32 intrinsic */
static inline uint32x4_t bswap32x4(uint32x4_t v) {
    return vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(v)));
}

static inline void evict_l1d_line(const void *addr) {
    __asm__ volatile(
        "dc civac, %0\n\t"
        "dsb sy\n\t"
        :: "r"(addr) : "memory"
    );
}

} /* namespace */

static int ma_l2evict_rev_init(struct test *test) {
    auto *d = new MaL2EvictRevData;
    if (!d)
        return EXIT_FAILURE;
    memset_random(d->input, sizeof(d->input));
    /* scalar golden snapshot — zero vector ALU in init (issue #373) */
    memcpy(d->golden, d->input, sizeof(d->input));
    memset(d->swapped, 0, sizeof(d->swapped));
    test->data = d;
    return EXIT_SUCCESS;
}

static int ma_l2evict_rev_cleanup(struct test *test) {
    delete static_cast<MaL2EvictRevData *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int ma_l2evict_rev_run(struct test *test, int cpu) {
    auto *d = static_cast<MaL2EvictRevData *>(test->data);
    uint64_t round = 0;

    do {
        ++round;
        for (uint32_t w = 0; w < MA_L2EVICT_REV_WORDS; w += 4) {
            uint32x4_t v = ma_rev_load(&d->input[w]);    /* 1st read (asm)  */
            uint32x4_t s = bswap32x4(v);                 /* NEON bswap      */
            vst1q_u32(&d->swapped[w], s);                /* store, other line */

            /* evict input's line so the reload below is L2/L3-served */
            evict_l1d_line(&d->input[w]);

            uint32x4_t r = ma_rev_load(&d->input[w]);    /* RELOAD (asm)    */

            /* cold branch: reload vs the independent golden snapshot
             * (the scalar original's bswap² folds to this same compare) */
            if (vmaxvq_u32(veorq_u32(r, vld1q_u32(&d->golden[w]))) != 0) {
                uint32_t lr[4];
                vst1q_u32(lr, r);
                for (int l = 0; l < 4; ++l) {
                    if (lr[l] != d->golden[w + l]) {
                        log_error("ma_l2evict_rev: reload mismatch word %u "
                                  "(cpu=%d round=%lu): golden=0x%08x "
                                  "actual=0x%08x xor=0x%08x "
                                  "(L2/L3-served NEON reload)",
                                  w + (uint32_t)l, cpu, (unsigned long)round,
                                  d->golden[w + l], lr[l],
                                  d->golden[w + l] ^ lr[l]);
                    }
                }
                report_fail_msg("ma_l2evict_rev: NEON reload after civac evict "
                                "returned wrong data");
            }
        }
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(ma_l2evict_rev, "NEON movbe_l2: vrev32 bswap round-trip, dc civac+dsb evicts the input line before the asm reload (L2/L3-served) — L1D-domain bypass with 128-bit lane structure (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = ma_l2evict_rev_init,
    .test_run = ma_l2evict_rev_run,
    .test_cleanup = ma_l2evict_rev_cleanup,
    .fracture_loop_count = -1,   /* never fracture (family convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif /* __aarch64__ */
