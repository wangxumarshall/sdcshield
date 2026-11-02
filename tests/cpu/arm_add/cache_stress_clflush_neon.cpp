/*  NEON variant of tests/cpu/arm_add/cache_stress_clflush_neon.cpp — same-day NEON pass:
 *  the copy load points become the explicit vld1q/vst1q stream
 *  (arm_add_neon_copy.h); flush/verify semantics identical.  The
  *  scalar original stays; identifiers carry the _neon suffix (ODR).
 */
/*  cache_stress_clflush_neon — memory copy UNDER cache-flush interference
 *  (633 #210 "Memory copy with cache flush interference (CLFLUSH)",
 *  ARM64 equivalent: DC CIVAC).
 *
 *  NOT a duplicate of cache_stress_civac: that test is a single-value
 *  ldr/str round-trip with a self-evict before ITS OWN reload; THIS test
 *  is the 633 #210 batch-copy semantics — a full-buffer copy during
 *  which the destination's already-written lines keep getting flushed
 *  out (dc civac + dsb sy), so completed stores must survive ongoing
 *  eviction pressure.  cache_stress_memcpy (633 #211) is the exact same
 *  copy+verify with ZERO flushing — the controlled pair.
 *
 *  Anti-golden-fold: dst is pre-corrupted with ~epoch (fresh every
 *  round) before the copy, so a store lost to the flush pressure cannot
 *  hide behind last round's correct bytes (issue #374 lineage).  The
 *  golden is an independent init-time copy of src — never recomputed
 *  from dst or src at verify time.
 *
 *  x86-64 untouched: DC CIVAC is aarch64-only; the whole file builds
 *  only under the aarch64 guard in tests/cpu/meson.build.
 */
#include "sandstone.h"
#include "arm_add_neon_copy.h"
using arm_add_neon::neon_copy;
#include <cstdint>
#include <cstring>
#include "arm_add_excite.h"

#define CSF_SIZE        (64 * 1024)              /* 64 KB, 1024 lines */
#define CSF_LINE        64
#define CSF_LINES       (CSF_SIZE / CSF_LINE)
#define CSF_FLUSH_EVERY 8                        /* flush 1 line per 8 copied */

namespace { /* ODR: per-TU CSFData */
struct CSFData {
    uint64_t *src;
    uint64_t *dst;
    uint64_t *golden;
};
}

static inline void csf_flush_line(const void *addr) {
    __asm__ volatile(
        "dc civac, %0\n\t"
        "dsb sy\n\t"
        :: "r"(addr) : "memory");
}

static int cache_stress_clflush_init(struct test *test) {
    auto *d = new CSFData;
    if (!d) return EXIT_FAILURE;
    d->src    = new uint64_t[CSF_SIZE / 8];
    d->dst    = new uint64_t[CSF_SIZE / 8];
    d->golden = new uint64_t[CSF_SIZE / 8];
    if (!d->src || !d->dst || !d->golden) return EXIT_FAILURE;

    memset_random(d->src, CSF_SIZE);
    neon_copy(d->golden, d->src, CSF_SIZE);   /* independent golden snapshot */
    memset(d->dst, 0, CSF_SIZE);
    test->data = d;
    return EXIT_SUCCESS;
}

static int cache_stress_clflush_cleanup(struct test *test) {
    auto *d = static_cast<CSFData *>(test->data);
    delete[] d->src;
    delete[] d->dst;
    delete[] d->golden;
    delete d;
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int cache_stress_clflush_run(struct test *test, int cpu) {
    auto *d = static_cast<CSFData *>(test->data);
    (void)cpu;
    uint64_t epoch = 0;

    do {
        ++epoch;
        const uint64_t poison = ~epoch;

        /* pre-corrupt dst: a store lost this round cannot masquerade as
         * last round's correct bytes */
        for (size_t i = 0; i < CSF_SIZE / 8; ++i)
            d->dst[i] = poison;

        /* batch copy src -> dst with ongoing flush interference on the
         * destination's already-written lines */
        for (int line = 0; line < CSF_LINES; ++line) {
            neon_copy(d->dst + (size_t)line * CSF_LINE / 8,
                   d->src + (size_t)line * CSF_LINE / 8,
                   CSF_LINE);
            if (line % CSF_FLUSH_EVERY == 0 && line >= CSF_FLUSH_EVERY) {
                /* evict an already-written dst line (a few lines back, so
                 * it is genuinely completed work under pressure) */
                int victim = line - CSF_FLUSH_EVERY;
                csf_flush_line(&d->dst[(size_t)victim * CSF_LINE / 8]);
            }
        }

        /* verify against the independent golden */
        if (memcmp(d->dst, d->golden, CSF_SIZE) != 0) {
            for (int line = 0; line < CSF_LINES; ++line) {
                const uint64_t *g = d->golden  + (size_t)line * CSF_LINE / 8;
                const uint64_t *a = d->dst     + (size_t)line * CSF_LINE / 8;
                if (memcmp(g, a, CSF_LINE) != 0) {
                    report_fail_msg("cache_stress_clflush_neon: dst line %d "
                                    "differs from golden under flush "
                                    "interference (g0=%016lx a0=%016lx)",
                                    line, (unsigned long)g[0],
                                    (unsigned long)a[0]);
                }
            }
        }
        /* excitation filler: NEON switching power (see arm_add_excite.h) */
        if (!arm_add_excite::fill(cpu, epoch)) {
            report_fail_msg("excitation filler store->reload mismatch "
                            "(cpu %d round %lu)", cpu,
                            (unsigned long)epoch);
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(cache_stress_clflush_neon, "Memory copy under dc-civac flush interference (633 #210 CLFLUSH equivalent): completed stores must survive ongoing destination eviction (ARM64)")
    .test_init = cache_stress_clflush_init,
    .test_run = cache_stress_clflush_run,
    .test_cleanup = cache_stress_clflush_cleanup,
    .fracture_loop_count = -1,   /* never fracture: our semantic rounds are microseconds; auto mode (40) ends the run in ~10 ms */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
