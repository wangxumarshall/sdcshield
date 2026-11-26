/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k5inter_rand_l2_128k
 * @parblock
 * L2-resident tier 128KB of neon_rot_ldr_at_top_rowmajor_k5inter_rand
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * interleaved dual-source layout is the object under test — a SINGLE
 * interleaved source buffer (element 2*i: A-family, 2*i+1: B-family;
 * a = src[2i], b = src[2i+1]), inherited verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_k5inter_rand.cpp).
 *
 * LAYOUT CONTROL, NO TAGS (inherited from the parent): natural random
 * data via memset_random — this cell isolates the interleaved-layout
 * effect from the tag effect (k5far_l2 carries the tagged arm), so no
 * GPS tags and no tag-layout considerations apply here.
 *
 * Input-data method transplant: loop_check_m tier sizes (fixed block =
 * tier) + memcpy_l2_cache_size sliding window (one random 64B-aligned
 * SHARED offset per sweep through a 2x window — the src window region
 * is 2 x LEN elements at [2*off, 2*(off+LEN)), 0x5A poison on the
 * compared destination block).  The memset_random fill + scalar golden cover the
 * FULL window at init; the sweep-end memcmp narrows to the slice.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 8192 uint64x2_t = 128KB (2x L1D,
 * inside the user's >64KB..<512KB window); per-sweep touched set =
 * src slice (2 x 128KB) + scratch + temp + dst = 5 x 128KB = 640KB —
 * slightly exceeds L2 (the interleaved five-region chassis geometry,
 * disclosed honestly; the tier axis is the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRK5I128_LEN 8192                  /* uint64x2_t elements -> 128KB block */
#define NRK5I128_WIN (2 * NRK5I128_LEN)   /* 2x-block window (memcpy_l2 method) */
#define NRK5I128_SRC_ELEMS (2 * NRK5I128_WIN)   /* interleaved: 2 families */
#define NRK5I128_DUMP_DIR "movbe_log/gps_rowmajor_l2"


struct NRK5I128Data {
    uint64x2_t *src;                      /* interleaved A/B families */
    uint64x2_t *scratch;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int k5inter_rand_l2_128k_init(struct test *test) {
    auto *data = static_cast<NRK5I128Data *>(malloc(sizeof(NRK5I128Data)));
    data->src     = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5I128_SRC_ELEMS * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5I128_WIN * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, NRK5I128_WIN * sizeof(uint64x2_t)));

    mkdir("movbe_log", 0755);
    mkdir(NRK5I128_DUMP_DIR, 0755);

    /* Natural random fill over the FULL window (layout control — no
     * tags; the interleaved geometry alone is this cell's variable) */
    memset_random(data->src, NRK5I128_SRC_ELEMS * sizeof(uint64x2_t));
    memset_random(data->scratch, NRK5I128_WIN * sizeof(uint64x2_t));

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window, pairs
     * (src[2i], src[2i+1]). */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->src);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int i = 0; i < NRK5I128_WIN; i++) {
            const uint64_t a0 = sa[4 * i + 0], a1 = sa[4 * i + 1];
            const uint64_t b0 = sa[4 * i + 2], b1 = sa[4 * i + 3];
            switch (i % 4) {
                case 0: ex[2*i+0] = a0 + b0; ex[2*i+1] = a1 + b1; break;
                case 1: ex[2*i+0] = a0 ^ b0; ex[2*i+1] = a1 ^ b1; break;
                case 2: ex[2*i+0] = a0 & b0; ex[2*i+1] = a1 & b1; break;
                case 3: ex[2*i+0] = a0 | b0; ex[2*i+1] = a1 | b1; break;
            }
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k5inter_rand_l2_128k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRK5I128Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5I128_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5I128_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const int off = (int)(4 * (random32() % (NRK5I128_LEN / 4)));

        memset(dst + off, 0x5A, NRK5I128_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRK5I128_LEN; n++) {
            const int j = ascending ? n : (NRK5I128_LEN - 1 - n);
            const int i = off + j;

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->src[2 * i]);
            uint64x2_t b = load_vec(&data->src[2 * i + 1]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst + off, data->expected + off, NRK5I128_LEN * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k5inter_rand_l2_128k_fail_cpu%d_sweep%lu_off%d.bin", NRK5I128_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst + off,            sizeof(uint64x2_t), NRK5I128_LEN, f);
                fwrite(data->expected + off, sizeof(uint64x2_t), NRK5I128_LEN, f);
                fwrite(data->src + 2*off,   sizeof(uint64x2_t), 2*NRK5I128_LEN, f);
                fwrite(data->scratch + off,  sizeof(uint64x2_t), NRK5I128_LEN, f);
                fprintf(f, "k5inter_rand_l2_128k sweep=%lu off=%d\n",
                        static_cast<unsigned long>(sweep), off);
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k5inter_rand_l2_128k data miscompare (off %u, records in dump bin)", (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k5inter_rand_l2_128k_cleanup(struct test *test) {
    auto *data = static_cast<NRK5I128Data *>(test->data);
    if (data) {
        free(data->src);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k5inter_rand_l2_128k, "L2-resident tier 128KB of the K5 layout-control cell: interleaved dual-source layout (a=src[2i], b=src[2i+1]) with NATURAL random data (no tags — layout control vs k5far_l2), read-side chassis preserved, sliding-window geometry transplanted; field-graft slice dump on fail (ARM64)")
    .test_init    = k5inter_rand_l2_128k_init,
    .test_run     = k5inter_rand_l2_128k_run,
    .test_cleanup = k5inter_rand_l2_128k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
