/*  NEON variant of tests/cpu/arm_add/cache_stress_memcpy_neon.cpp — same-day NEON pass:
 *  the copy load points become the explicit vld1q/vst1q stream
 *  (arm_add_neon_copy.h); flush/verify semantics identical.  The
  *  scalar original stays; identifiers carry the _neon suffix (ODR).
 */
/*  cache_stress_memcpy_neon — pure memory copy control (633 #211 "Memory
 *  copy only (REP MOVS, no flush)").
 *
 *  The CONTROL ARM of cache_stress_clflush (633 #210): byte-for-byte the
 *  same copy + verify workload — same 64 KB buffers, same anti-fold dst
 *  pre-corruption, same independent golden — with ZERO cache-flush
 *  interference.  Any failure here that does not reproduce under
 *  cache_stress_clflush localises the fault to the flush/copy interplay
 *  rather than the copy path itself; failures appearing ONLY under
 *  clflush are the eviction-pressure signature.
 *
 *  ARM64 equivalent of REP MOVS: the plain memcpy loop (compiler emits
 *  ldp/stp pairs at -O2) — the spec explicitly names "memcpy 库调用或
 *  LDP/STP 循环" as the accepted equivalent, and the instruction under
 *  study in the #210/#211 pair is the FLUSH, not the copy.
 *
 *  x86-64 untouched: registered only under the aarch64 guard in
 *  tests/cpu/meson.build.
 */
#include "sandstone.h"
#include "arm_add_neon_copy.h"
using arm_add_neon::neon_copy;
#include <cstdint>
#include <cstring>
#include "arm_add_excite.h"

#define CSM_SIZE (64 * 1024)

namespace { /* ODR: per-TU CSMData */
struct CSMData {
    uint64_t *src;
    uint64_t *dst;
    uint64_t *golden;
};
}

static int cache_stress_memcpy_init(struct test *test) {
    auto *d = new CSMData;
    if (!d) return EXIT_FAILURE;
    d->src    = new uint64_t[CSM_SIZE / 8];
    d->dst    = new uint64_t[CSM_SIZE / 8];
    d->golden = new uint64_t[CSM_SIZE / 8];
    if (!d->src || !d->dst || !d->golden) return EXIT_FAILURE;

    memset_random(d->src, CSM_SIZE);
    neon_copy(d->golden, d->src, CSM_SIZE);   /* independent golden snapshot */
    memset(d->dst, 0, CSM_SIZE);
    test->data = d;
    return EXIT_SUCCESS;
}

static int cache_stress_memcpy_cleanup(struct test *test) {
    auto *d = static_cast<CSMData *>(test->data);
    delete[] d->src;
    delete[] d->dst;
    delete[] d->golden;
    delete d;
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int cache_stress_memcpy_run(struct test *test, int cpu) {
    auto *d = static_cast<CSMData *>(test->data);
    (void)cpu;
    uint64_t epoch = 0;

    do {
        ++epoch;
        const uint64_t poison = ~epoch;

        /* pre-corrupt dst (anti-golden-fold, identical to clflush arm) */
        for (size_t i = 0; i < CSM_SIZE / 8; ++i)
            d->dst[i] = poison;

        /* pure copy — no flush interference anywhere */
        neon_copy(d->dst, d->src, CSM_SIZE);

        /* verify against the independent golden */
        if (memcmp(d->dst, d->golden, CSM_SIZE) != 0) {
            for (size_t i = 0; i < CSM_SIZE / 8; ++i) { /* full scan (was sampled i += 8: off-grid flips produced no error and the framework judged pass) */
                if (d->golden[i] != d->dst[i]) {
                    report_fail_msg("cache_stress_memcpy_neon: dst qword %zu "
                                    "differs from golden with NO flush "
                                    "present (g=%016lx a=%016lx) — pure "
                                    "copy-path failure, the control arm",
                                    i, (unsigned long)d->golden[i],
                                    (unsigned long)d->dst[i]);
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

DECLARE_TEST(cache_stress_memcpy_neon, "Pure memory copy control, no flush (633 #211 REP MOVS equivalent): the zero-interference arm of cache_stress_clflush (ARM64)")
    .test_init = cache_stress_memcpy_init,
    .test_run = cache_stress_memcpy_run,
    .test_cleanup = cache_stress_memcpy_cleanup,
    .fracture_loop_count = -1,   /* never fracture: our semantic rounds are microseconds; auto mode (40) ends the run in ~10 ms */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
