/*  ma_l2evict_2src — L1D-evict 2src-ALU reload probe, NEON.
 *  NEON port of tests/cpu/misc/mrn_rmw_fwd_vs_l1d.cpp (mrn_rmw_l1d):
 *  the 2-source load + rotating ALU + store→reload recipe with
 *  `dc civac + dsb sy` inserted between the store to temp[i] and the
 *  reload of temp[i], so the reload is served from L2/L3 instead of
 *  store-buffer forwarding / L1D array.
 *
 *  Hot loop (per element, 128-bit NEON):
 *      ldr q(srcA[i]); ldr q(srcB[i])          — 2 source loads (asm)
 *      vaddq/veorq/vandq/vorrq u64 by i%4      — rotating vALU
 *      vst1q(temp[i])                          — str 1
 *      dc civac + dsb sy on temp[i]            — the eviction insertion
 *      ldr q(temp[i])                          — RELOAD (asm): L2/L3
 *      vst1q(dst[i])                           — str 2, back-to-back
 *
 *  This is the 3-ldr+ALU+2-str family shape (srcA, srcB, reload) with
 *  the eviction of mrn_rmw_l1d.  Pre-registered expectation: 0 fail —
 *  crosses the evict path with the NEON 128-bit/lane structure that the
 *  trigger recipe requires (scalar64 = 0/30min).  Confound inherited
 *  from the original: civac+dsb evicts AND stretches timing.
 *  Golden is init-time SCALAR computation of the rotating ALU
 *  (issue #373: zero vector ALU in init).
 */
#include <sandstone.h>
#include <arm_neon.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)

#define MA_L2EVICT_2SRC_VECS (16 * 1024 / 16)   /* 1024 vec x 16B = 16 KiB window */

namespace {

struct MaL2Evict2SrcData {
    uint64x2_t srcA[MA_L2EVICT_2SRC_VECS];
    uint64x2_t srcB[MA_L2EVICT_2SRC_VECS];
    uint64x2_t expected[MA_L2EVICT_2SRC_VECS];  /* scalar golden */
};

static inline uint64x2_t ma_2src_load(const uint64x2_t *addr) {
    uint64x2_t res;
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

static int ma_l2evict_2src_init(struct test *test) {
    auto *d = new MaL2Evict2SrcData;
    if (!d)
        return EXIT_FAILURE;
    memset_random(d->srcA, sizeof(d->srcA));
    memset_random(d->srcB, sizeof(d->srcB));
    /* scalar golden — the same rotating ALU on scalar lanes, zero
     * vector ALU in init (issue #373) */
    const uint64_t *a = reinterpret_cast<const uint64_t *>(d->srcA);
    const uint64_t *b = reinterpret_cast<const uint64_t *>(d->srcB);
    uint64_t *e = reinterpret_cast<uint64_t *>(d->expected);
    for (uint32_t i = 0; i < MA_L2EVICT_2SRC_VECS * 2; ++i) {
        switch ((i / 2) % 4) {
            case 0:  e[i] = a[i] + b[i]; break;
            case 1:  e[i] = a[i] ^ b[i]; break;
            case 2:  e[i] = a[i] & b[i]; break;
            default: e[i] = a[i] | b[i]; break;
        }
    }
    test->data = d;
    return EXIT_SUCCESS;
}

static int ma_l2evict_2src_cleanup(struct test *test) {
    delete static_cast<MaL2Evict2SrcData *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int ma_l2evict_2src_run(struct test *test, int cpu) {
    auto *d = static_cast<MaL2Evict2SrcData *>(test->data);
    (void)cpu;

    uint64x2_t *temp = static_cast<uint64x2_t *>(
            aligned_alloc(64, MA_L2EVICT_2SRC_VECS * sizeof(uint64x2_t)));
    uint64x2_t *dst = static_cast<uint64x2_t *>(
            aligned_alloc(64, MA_L2EVICT_2SRC_VECS * sizeof(uint64x2_t)));
    if (!temp || !dst) {
        free(temp);
        free(dst);
        return EXIT_FAILURE;
    }

    uint64_t round = 0;
    do {
        ++round;
        for (uint32_t i = 0; i < MA_L2EVICT_2SRC_VECS; ++i) {
            uint64x2_t a = ma_2src_load(&d->srcA[i]);
            uint64x2_t b = ma_2src_load(&d->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0:  res = vaddq_u64(a, b); break;
                case 1:  res = veorq_u64(a, b); break;
                case 2:  res = vandq_u64(a, b); break;
                default: res = vorrq_u64(a, b); break;
            }

            vst1q_u64(reinterpret_cast<uint64_t *>(&temp[i]), res); /* str 1 */

            /* mrn_rmw_l1d's insertion point: evict the just-stored line
             * so the reload below is L2/L3-served */
            evict_l1d_line(&temp[i]);

            uint64x2_t r = ma_2src_load(&temp[i]); /* RELOAD (asm) */

            vst1q_u64(reinterpret_cast<uint64_t *>(&dst[i]), r);   /* str 2, back-to-back */
        }

        /* cold branch: full-window verify against the scalar golden */
        if (memcmp(dst, d->expected, MA_L2EVICT_2SRC_VECS * sizeof(uint64x2_t)) != 0) {
            const uint64_t *dd = reinterpret_cast<const uint64_t *>(dst);
            const uint64_t *ee = reinterpret_cast<const uint64_t *>(d->expected);
            for (uint32_t i = 0; i < MA_L2EVICT_2SRC_VECS * 2; ++i) {
                if (dd[i] != ee[i]) {
                    log_error("ma_l2evict_2src: miscompare vec %u lane %u "
                              "(round=%lu): golden=0x%016llx actual=0x%016llx "
                              "xor=0x%016llx (reload after civac evict)",
                              i / 2, i % 2, (unsigned long)round,
                              (unsigned long long)ee[i],
                              (unsigned long long)dd[i],
                              (unsigned long long)(ee[i] ^ dd[i]));
                    break;   /* ≤8-line dump discipline */
                }
            }
            report_fail_msg("ma_l2evict_2src: NEON 2src reload after civac "
                            "evict miscompares");
        }
    } while (test_time_condition(test));

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(ma_l2evict_2src, "NEON mrn_rmw_l1d: 2src loads + rotating vALU + str, dc civac+dsb evicts the stored line before the asm reload (L2/L3-served), reload feeds the back-to-back second store (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = ma_l2evict_2src_init,
    .test_run = ma_l2evict_2src_run,
    .test_cleanup = ma_l2evict_2src_cleanup,
    .fracture_loop_count = -1,   /* never fracture (family convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif /* __aarch64__ */
