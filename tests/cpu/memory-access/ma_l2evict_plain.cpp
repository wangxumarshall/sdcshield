/*  ma_l2evict_plain — plain store→L2-reload round-trip probe, NEON.
 *  NEON port of tests/cpu/cache/cache_stress_civac.cpp's stated design:
 *  a plain copy round-trip whose store→load-forwarding reload is replaced
 *  by `dc civac + dsb sy` on the JUST-STORED line, so the reload of the
 *  stored data is served from L2/L3 instead of the store buffer / L1D.
 *
 *  Distinct from ma_l2evict_rev (which evicts the INPUT line before
 *  re-reading input) and ma_l2evict_2src (2src rotating vALU + evict of
 *  the stored temp feeding a back-to-back second store): this cell is
 *  the minimal plain-copy arm — no ALU, single source, evict between
 *  the store and its own reload.  Together the three cells carry the
 *  three original scalar negative controls (movbe_l2 / mrn_rmw_l1d /
 *  cache_stress_civac) into the NEON 128-bit lane structure.
 *
 *  Pre-registered expectation: 0 fail — the scalar evict-path controls
 *  were all 0-fail on core 179; if the L2/L3 reload path stays silent
 *  under the NEON lane structure too, "L1D-domain reload" is necessary
 *  independent of lane width.  Confound inherited: civac+dsb evicts AND
 *  stretches timing (intervention-decoupling).  Golden is an init-time
 *  scalar snapshot of input (issue #373).
 */
#include <sandstone.h>
#include <arm_neon.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)

#define MA_L2EVICT_PLAIN_WORDS (16 * 1024 / 4)   /* 4096 words = 16 KiB window */

namespace {

struct MaL2EvictPlainData {
    uint32_t input[MA_L2EVICT_PLAIN_WORDS];   /* random source        */
    uint32_t temp[MA_L2EVICT_PLAIN_WORDS];    /* stored-then-evicted  */
    uint32_t golden[MA_L2EVICT_PLAIN_WORDS];  /* scalar init snapshot */
};

static inline uint32x4_t ma_plain_load(const uint32_t *addr) {
    uint32x4_t res;
    __asm__ volatile("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

static inline void evict_l1d_line(const void *addr) {
    __asm__ volatile(
        "dc civac, %0\n\t"
        "dsb sy\n\t"
        :: "r"(addr) : "memory"
    );
}

} /* namespace */

static int ma_l2evict_plain_init(struct test *test) {
    auto *d = new MaL2EvictPlainData;
    if (!d)
        return EXIT_FAILURE;
    memset_random(d->input, sizeof(d->input));
    /* scalar golden snapshot — zero vector ALU in init (issue #373) */
    memcpy(d->golden, d->input, sizeof(d->input));
    memset(d->temp, 0, sizeof(d->temp));
    test->data = d;
    return EXIT_SUCCESS;
}

static int ma_l2evict_plain_cleanup(struct test *test) {
    delete static_cast<MaL2EvictPlainData *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int ma_l2evict_plain_run(struct test *test, int cpu) {
    auto *d = static_cast<MaL2EvictPlainData *>(test->data);
    uint64_t round = 0;

    do {
        ++round;
        for (uint32_t w = 0; w < MA_L2EVICT_PLAIN_WORDS; w += 4) {
            uint32x4_t v = ma_plain_load(&d->input[w]);   /* source read (asm) */

            vst1q_u32(&d->temp[w], v);                    /* plain store     */

            /* evict the JUST-STORED line: the reload below cannot come
             * from store-buffer forwarding / L1D — it must be L2/L3 */
            evict_l1d_line(&d->temp[w]);

            uint32x4_t r = ma_plain_load(&d->temp[w]);    /* reload of stored (asm) */

            /* cold branch: reload vs the independent golden snapshot
             * (issue #373 — not vs v, which rides the same load path) */
            if (vmaxvq_u32(veorq_u32(r, vld1q_u32(&d->golden[w]))) != 0) {
                uint32_t lr[4];
                vst1q_u32(lr, r);
                for (int l = 0; l < 4; ++l) {
                    if (lr[l] != d->golden[w + l]) {
                        log_error("ma_l2evict_plain: stored-reload mismatch "
                                  "word %u (cpu=%d round=%lu): "
                                  "golden=0x%08x actual=0x%08x xor=0x%08x "
                                  "(reload of civac-evicted store)",
                                  w + (uint32_t)l, cpu, (unsigned long)round,
                                  d->golden[w + l], lr[l],
                                  d->golden[w + l] ^ lr[l]);
                    }
                }
                report_fail_msg("ma_l2evict_plain: NEON reload of "
                                "civac-evicted store returned wrong data");
            }
        }
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(ma_l2evict_plain, "NEON cache_stress_civac design: plain copy store, dc civac+dsb evicts the JUST-STORED line before the asm reload (L2/L3-served store-reload round-trip, no ALU) (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = ma_l2evict_plain_init,
    .test_run = ma_l2evict_plain_run,
    .test_cleanup = ma_l2evict_plain_cleanup,
    .fracture_loop_count = -1,   /* never fracture (family convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif /* __aarch64__ */
