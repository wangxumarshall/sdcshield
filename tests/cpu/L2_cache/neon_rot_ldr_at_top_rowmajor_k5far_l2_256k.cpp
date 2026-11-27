/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k5far_l2_256k
 * @parblock
 * L2-resident tier 256KB of neon_rot_ldr_at_top_rowmajor_k5far
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * interleaved far-source layout is the object under test — a SINGLE
 * interleaved source buffer (element 2*i: A-family, 2*i+1: B-family;
 * a = src[2i], b = src[2i+1]), inherited verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_k5far.cpp).
 *
 * TAG LAYOUT REDESIGN (this tier family's own format, disclosed): the
 * parent's 16-bit byte_off caps at 65520; the L2 windows exceed it, so
 * this family's tag is (magic<<48)|((off&0xFFFFFFFF)<<16)|gen — 32-bit
 * offset (byte offsets in the FULL interleaved buffer reach 16 x 2 x
 * WIN, 2MB at the 512K tier — fits), the one's-complement integrity
 * field is dropped.  The byte_off records the offset in the FULL
 * interleaved buffer so the classifier resolves address neighbours
 * directly (parent semantics, widened).
 *
 * Input-data method transplant: loop_check_m tier sizes (fixed block =
 * tier) + memcpy_l2_cache_size sliding window (one random 64B-aligned
 * SHARED offset per sweep through a 2x window — the src window region
 * is 2 x LEN elements at [2*off, 2*(off+LEN)), 0x5A poison on the
 * compared destination block).  Tagged fill + scalar golden cover the
 * FULL window at init; the sweep-end memcmp narrows to the slice.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64x2_t = 256KB (inside the user's >64KB..<512KB window); per-sweep touched set =
 * src slice (2 x 128KB) + scratch + temp + dst = 5 x 256KB = 1.25MB —
 * EXCEEDS L2 and self-evicts (the interleaved five-region chassis geometry,
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

#define NRK5F256_LEN 16384                  /* uint64x2_t elements -> 256KB block */
#define NRK5F256_WIN (2 * NRK5F256_LEN)   /* 2x-block window (memcpy_l2 method) */
#define NRK5F256_SRC_ELEMS (2 * NRK5F256_WIN)   /* interleaved: 2 families */
#define NRK5F256_DUMP_DIR "movbe_log/gps_rowmajor_l2"

#define NRK5F256_MAGIC_A   0xA5A5ULL
#define NRK5F256_MAGIC_B   0x5A5AULL
#define NRK5F256_MAGIC_SCR 0x0F0FULL

struct NRK5F256Data {
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

/* 32-bit-offset tag — this tier family's own format (see header) */
static inline uint64_t nrk5f128_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                        << 48)
         | ((byte_off        & 0xFFFFFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k5far_l2_256k_init(struct test *test) {
    auto *data = static_cast<NRK5F256Data *>(malloc(sizeof(NRK5F256Data)));
    data->src     = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5F256_SRC_ELEMS * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5F256_WIN * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, NRK5F256_WIN * sizeof(uint64x2_t)));

    mkdir("movbe_log", 0755);
    mkdir(NRK5F256_DUMP_DIR, 0755);

    /* Interleaved tagged fill over the FULL window — element 2*i:
     * A-family; 2*i+1: B-family; byte_off records the offset in the
     * FULL interleaved buffer (parent semantics, 32-bit widened). */
    for (int e = 0; e < NRK5F256_SRC_ELEMS; e++) {
        const uint64_t byte = static_cast<uint64_t>(e) * 16;
        const uint64_t magic = (e & 1) ? NRK5F256_MAGIC_B : NRK5F256_MAGIC_A;
        store_vec(&data->src[e], vcombine_u64(vcreate_u64(nrk5f128_tag(magic, byte + 0, 0)),
                                              vcreate_u64(nrk5f128_tag(magic, byte + 8, 0))));
    }
    for (int i = 0; i < NRK5F256_WIN; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(nrk5f128_tag(NRK5F256_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(nrk5f128_tag(NRK5F256_MAGIC_SCR, byte + 8, 0))));
    }

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window, pairs
     * (src[2i], src[2i+1]). */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->src);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int i = 0; i < NRK5F256_WIN; i++) {
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

static int k5far_l2_256k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRK5F256Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5F256_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRK5F256_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const int off = (int)(4 * (random32() % (NRK5F256_LEN / 4)));

        memset(dst + off, 0x5A, NRK5F256_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRK5F256_LEN; n++) {
            const int j = ascending ? n : (NRK5F256_LEN - 1 - n);
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

        if (memcmp(dst + off, data->expected + off, NRK5F256_LEN * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k5far_l2_256k_fail_cpu%d_sweep%lu_off%d.bin", NRK5F256_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst + off,            sizeof(uint64x2_t), NRK5F256_LEN, f);
                fwrite(data->expected + off, sizeof(uint64x2_t), NRK5F256_LEN, f);
                fwrite(data->src + 2*off,   sizeof(uint64x2_t), 2*NRK5F256_LEN, f);
                fwrite(data->scratch + off,  sizeof(uint64x2_t), NRK5F256_LEN, f);
                fprintf(f, "k5far_l2_256k sweep=%lu off=%d\n",
                        static_cast<unsigned long>(sweep), off);
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k5far_l2_256k data miscompare (off %u, records in dump bin)", (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k5far_l2_256k_cleanup(struct test *test) {
    auto *data = static_cast<NRK5F256Data *>(test->data);
    if (data) {
        free(data->src);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k5far_l2_256k, "L2-resident tier 256KB of the K5 far-source cell: interleaved dual-source layout (a=src[2i], b=src[2i+1]) with 32-bit-offset tags over the full interleaved window, read-side chassis preserved, sliding-window geometry transplanted; field-graft slice dump on fail (ARM64)")
    .test_init    = k5far_l2_256k_init,
    .test_run     = k5far_l2_256k_run,
    .test_cleanup = k5far_l2_256k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
