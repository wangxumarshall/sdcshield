/*  ma_src_overl2 — 3-ldr+ALU+2-str trigger recipe with sources beyond L2.
 *
 *  Discriminating cell for the question "can the recipe's data size
 *  simply exceed L1D/L2?": the neon_rot_ldr_at_top_rowmajor skeleton
 *  (dead scratch ldr at top + 2 source loads + i%4 rotating vALU +
 *  back-to-back double store + cold-branch memcmp) is kept
 *  instruction-identical, and ONLY the source arrays are scaled:
 *
 *      srcA / srcB : 4 MiB each  (262144 x 16B) — 8 MiB live sources,
 *                    8x the per-core L2 (512 KiB), streamed block-wise
 *                    so source loads miss L2 and are L3/DRAM-served
 *                    (first round DRAM, steady state L3: 24 MiB/socket).
 *      scratch / temp / dst : 16 KiB windows — stay L1D-resident, so the
 *                    dead 3rd ldr and both stores remain in the
 *                    L1D/forwarding domain (the control variables).
 *
 *  Sizing note: fixed 4 MiB per source deliberately exceeds every L2
 *  figure reported for this part (512 KiB/core private; sysfs cache
 *  index has also been seen reporting 1.25 MiB) — no topology probing,
 *  reproducible footprint.
 *
 *  PRE-REGISTERED READOUT (write the judgment before running):
 *    A) 0 fail!!!  -> the trigger requires the SOURCE loads to be
 *       L1D-domain-served.  k3res (4x-L1D = 256 KiB, still < L2) kept
 *       triggering at baseline rate because same-iteration temporal
 *       locality kept the corrupting donor lines resident; beyond L2
 *       even that is gone, so exposure collapses.  This would re-confirm
 *       "L1D-domain reload" as a necessary gate independent of footprint.
 *    B) still triggers -> exposure does NOT ride the source loads'
 *       L1D-domain reads: it rides the store path / the resident window
 *       / the dead scratch ldr — follow up with the GPS chassis.
 *
 *  Counting caveat: fail-block granularity here is per-BLOCK (the cold
 *  memcmp runs after each 16 KiB block), whereas rowmajor checks once
 *  per sweep — do not compare fail-block counts across the two without
 *  converting units.  Golden is init-time SCALAR computation of the
 *  rotating ALU over the full 4 MiB (issue #373: zero vector ALU in
 *  init).  fracture disabled: the framework's cache fracturing would
 *  perturb the footprint experiment.
 */
#include <sandstone.h>
#include <arm_neon.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)

#define MA_SRC_OVERL2_SRC_VECS  (262144)   /* x 16B = 4 MiB per source  */
#define MA_SRC_OVERL2_WIN_VECS  (1024)     /* x 16B = 16 KiB window     */
#define MA_SRC_OVERL2_BLOCKS    (MA_SRC_OVERL2_SRC_VECS / MA_SRC_OVERL2_WIN_VECS)

namespace {

struct MaSrcOverL2Data {
    uint64x2_t srcA[MA_SRC_OVERL2_SRC_VECS];     /* 4 MiB, L3/DRAM-served */
    uint64x2_t srcB[MA_SRC_OVERL2_SRC_VECS];     /* 4 MiB, L3/DRAM-served */
    uint64x2_t expected[MA_SRC_OVERL2_SRC_VECS]; /* 4 MiB scalar golden   */
    uint64x2_t scratch[MA_SRC_OVERL2_WIN_VECS];  /* 16 KiB, L1D-resident  */
};

static inline uint64x2_t ma_overl2_load(const uint64x2_t *addr) {
    uint64x2_t res;
    __asm__ volatile("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

} /* namespace */

static int ma_src_overl2_init(struct test *test) {
    auto *d = new MaSrcOverL2Data;
    if (!d)
        return EXIT_FAILURE;
    memset_random(d->srcA, sizeof(d->srcA));
    memset_random(d->srcB, sizeof(d->srcB));
    memset_random(d->scratch, sizeof(d->scratch));
    /* scalar golden over the full 4 MiB — same rotating ALU on scalar
     * lanes, zero vector ALU in init (issue #373).  Rotation uses the
     * WINDOW index parity (g % 4), matching the hot loop's i % 4. */
    const uint64_t *a = reinterpret_cast<const uint64_t *>(d->srcA);
    const uint64_t *b = reinterpret_cast<const uint64_t *>(d->srcB);
    uint64_t *e = reinterpret_cast<uint64_t *>(d->expected);
    for (uint32_t g = 0; g < MA_SRC_OVERL2_SRC_VECS * 2; ++g) {
        switch ((g / 2) % 4) {
            case 0:  e[g] = a[g] + b[g]; break;
            case 1:  e[g] = a[g] ^ b[g]; break;
            case 2:  e[g] = a[g] & b[g]; break;
            default: e[g] = a[g] | b[g]; break;
        }
    }
    test->data = d;
    return EXIT_SUCCESS;
}

static int ma_src_overl2_cleanup(struct test *test) {
    delete static_cast<MaSrcOverL2Data *>(test->data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int ma_src_overl2_run(struct test *test, int cpu) {
    auto *d = static_cast<MaSrcOverL2Data *>(test->data);
    (void)cpu;

    /* per-thread L1D-resident window (rowmajor idiom) */
    uint64x2_t *temp = static_cast<uint64x2_t *>(
            aligned_alloc(64, MA_SRC_OVERL2_WIN_VECS * sizeof(uint64x2_t)));
    uint64x2_t *dst = static_cast<uint64x2_t *>(
            aligned_alloc(64, MA_SRC_OVERL2_WIN_VECS * sizeof(uint64x2_t)));
    if (!temp || !dst) {
        free(temp);
        free(dst);
        return EXIT_FAILURE;
    }

    uint64_t round = 0;
    do {
        /* rowmajor's per-sweep direction alternation */
        const bool ascending = (round & 1) == 0;
        ++round;

        for (uint32_t blk = 0; blk < MA_SRC_OVERL2_BLOCKS; ++blk) {
            const uint64x2_t *sa = &d->srcA[blk * MA_SRC_OVERL2_WIN_VECS];
            const uint64x2_t *sb = &d->srcB[blk * MA_SRC_OVERL2_WIN_VECS];

            /* hot window — instruction-identical to rowmajor's sweep:
             * dead scratch ldr + 2 source ldr + rotating vALU + double
             * str; only the source base walks across the 8 MiB range */
            for (uint32_t n = 0; n < MA_SRC_OVERL2_WIN_VECS; ++n) {
                const uint32_t i = ascending ? n : (MA_SRC_OVERL2_WIN_VECS - 1 - n);

                uint64x2_t unused_x = ma_overl2_load(&d->scratch[i]);
                (void)unused_x;

                uint64x2_t a = ma_overl2_load(&sa[i]);
                uint64x2_t b = ma_overl2_load(&sb[i]);
                uint64x2_t res;

                switch (i % 4) {
                    case 0:  res = vaddq_u64(a, b); break;
                    case 1:  res = veorq_u64(a, b); break;
                    case 2:  res = vandq_u64(a, b); break;
                    default: res = vorrq_u64(a, b); break;
                }

                vst1q_u64(reinterpret_cast<uint64_t *>(&temp[i]), res);
                vst1q_u64(reinterpret_cast<uint64_t *>(&dst[i]), res);
            }

            /* cold branch per block: verify the window against the
             * scalar golden for this block */
            if (memcmp(dst, &d->expected[blk * MA_SRC_OVERL2_WIN_VECS],
                       MA_SRC_OVERL2_WIN_VECS * sizeof(uint64x2_t)) != 0) {
                const uint64_t *dd = reinterpret_cast<const uint64_t *>(dst);
                const uint64_t *ee = reinterpret_cast<const uint64_t *>(
                        &d->expected[blk * MA_SRC_OVERL2_WIN_VECS]);
                for (uint32_t i = 0; i < MA_SRC_OVERL2_WIN_VECS * 2; ++i) {
                    if (dd[i] != ee[i]) {
                        log_error("ma_src_overl2: miscompare block %u vec %u "
                                  "lane %u (round=%lu): golden=0x%016llx "
                                  "actual=0x%016llx xor=0x%016llx "
                                  "(sources beyond L2)",
                                  blk, i / 2, i % 2, (unsigned long)round,
                                  (unsigned long long)ee[i],
                                  (unsigned long long)dd[i],
                                  (unsigned long long)(ee[i] ^ dd[i]));
                        break;   /* ≤8-line dump discipline */
                    }
                }
                report_fail_msg("ma_src_overl2: 2src recipe miscompares "
                                "with L3/DRAM-served sources");
            }
        }
    } while (test_time_condition(test));

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(ma_src_overl2, "3-ldr+ALU+2-str rowmajor recipe with 4 MiB-per-source arrays (8 MiB live sources, 8x per-core L2): source loads L3/DRAM-served while scratch/temp/dst stay L1D-resident — footprint-beyond-L2 discriminating cell (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = ma_src_overl2_init,
    .test_run = ma_src_overl2_run,
    .test_cleanup = ma_src_overl2_cleanup,
    .fracture_loop_count = -1,   /* never fracture — cache placement is the variable */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif /* __aarch64__ */
