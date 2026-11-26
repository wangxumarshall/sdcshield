/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k4bgap_l2_512k
 * @parblock
 * L2-resident tier 512KB of neon_rot_ldr_at_top_rowmajor_gps
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * GPS-tagged fill and dump discipline are the object under test — the
 * sources carry field-graft tags (magic identifies the SOURCE buffer
 * family; srcA/srcB magics are complementary so a two-source splice is
 * visible inside a single 64b word), enabling offline
 * which-source-word-substituted analysis, inherited verbatim from the
 * parent (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_gps.cpp).
 *
 * TAG LAYOUT REDESIGN (this tier family's own format, disclosed here):
 * the parent's gps_tag is (magic<<48)|((off&0xFFFF)<<32)|((~off&0xFFFF)
 * <<16)|gen — its 16-bit byte_off field caps at 65520, which the L2
 * tier windows exceed (256K tier window = 512KB, 512K tier = 1MB).
 * This family's tag: (magic<<48)|((off&0xFFFFFFFF)<<16)|gen — the
 * offset is widened to 32 bits (the 512K window's max offset 1048560
 * fits with room to spare) and the one's-complement integrity field is
 * DROPPED to make room (magic stays 16b top, gen 16b bottom).  The
 * offline classifier must use this decode for the _l2 files.
 *
 * K4b far-gap axis: 8 NOPs between the rotating ALU and the back-to-back
 * stores (ALU->store-data bypass closed; splits E3 injection vs upstream
 * load delivery) — inserted into the GPS L2 chassis verbatim from the parent.
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on the compared destination block).
 * The tagged fill and the scalar golden cover the FULL window at init;
 * the sweep-end memcmp narrows to the slice [off, off+LEN); the fail
 * dump covers the slice.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 32768 uint64x2_t = 512KB — the
 * user-named upper boundary, exactly L2 capacity on the reference
 * geometry the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 512KB = 2.5MB —
 * EXCEEDS L2 and self-evicts (the five-buffer chassis geometry, disclosed
 * honestly; the tier axis is the input block size).
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

#define NRK4B512_LEN 32768                  /* uint64x2_t elements -> 512KB block */
#define NRK4B512_WIN (2 * NRK4B512_LEN)     /* 2x-block window (memcpy_l2 method) */
#define NRK4B512_DUMP_DIR "movbe_log/gps_rowmajor_l2"

#define NRK4B512_MAGIC_A   0xA5A5ULL
#define NRK4B512_MAGIC_B   0x5A5AULL
#define NRK4B512_MAGIC_SCR 0x0F0FULL

struct NRK4B512Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
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

/* One tagged word — THIS TIER FAMILY'S 32-BIT-OFFSET FORMAT (see
 * header): magic 16b top, byte_off 32b middle (the parent's 16b field
 * caps at 65520 and the L2 windows exceed it), gen 16b bottom; the
 * parent's one's-complement integrity field is dropped to make room. */
static inline uint64_t nrpg128_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                        << 48)
         | ((byte_off        & 0xFFFFFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k4bgap_l2_512k_init(struct test *test) {
    auto *data = static_cast<NRK4B512Data *>(malloc(sizeof(NRK4B512Data)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK4B512_WIN * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK4B512_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRK4B512_WIN * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, NRK4B512_WIN * sizeof(uint64x2_t)));

    mkdir("movbe_log", 0755);
    mkdir(NRK4B512_DUMP_DIR, 0755);  /* EEXIST is fine; 09-08 lesson: dump to disk */

    /* Tagged fill over the FULL window — the ONLY functional delta vs
     * the parent's chassis (same complementary magics). */
    for (int i = 0; i < NRK4B512_WIN; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i],    vcombine_u64(vcreate_u64(nrpg128_tag(NRK4B512_MAGIC_A,   byte + 0, 0)),
                                                  vcreate_u64(nrpg128_tag(NRK4B512_MAGIC_A,   byte + 8, 0))));
        store_vec(&data->srcB[i],    vcombine_u64(vcreate_u64(nrpg128_tag(NRK4B512_MAGIC_B,   byte + 0, 0)),
                                                  vcreate_u64(nrpg128_tag(NRK4B512_MAGIC_B,   byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(nrpg128_tag(NRK4B512_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(nrpg128_tag(NRK4B512_MAGIC_SCR, byte + 8, 0))));
    }
    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRK4B512_WIN; e++) {
            switch ((e / 2) % 4) {   /* op phase: vector index % 4 */
                case 0: ex[e] = sa[e] + sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & sb[e]; break;
                case 3: ex[e] = sa[e] | sb[e]; break;
            }
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k4bgap_l2_512k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRK4B512Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRK4B512_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRK4B512_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot4 slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRK4B512_LEN / 4)));

        /* poison the compared destination block: a lost write stays
         * 0x5a5a.. and fails the slice compare */
        memset(dst + off, 0x5A, NRK4B512_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRK4B512_LEN; n++) {
            const int j = ascending ? n : (NRK4B512_LEN - 1 - n);
            const int i = off + j;

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);

            /* K4b knob: 8-NOP gap between the rotating ALU and the
             * back-to-back stores (ALU->store-data bypass closed) */
            __asm__ volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");

            store_vec(&dst[i], res);
        }
        sweep++;

        /* cold path: slice compare — only the window is promised
         * written this sweep; the dump covers the slice */
        if (memcmp(dst + off, data->expected + off, NRK4B512_LEN * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k4bgap_l2_512k_fail_cpu%d_sweep%lu_off%d.bin", NRK4B512_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst + off,            sizeof(uint64x2_t), NRK4B512_LEN, f);
                fwrite(data->expected + off, sizeof(uint64x2_t), NRK4B512_LEN, f);
                fwrite(data->srcA + off,     sizeof(uint64x2_t), NRK4B512_LEN, f);
                fwrite(data->srcB + off,     sizeof(uint64x2_t), NRK4B512_LEN, f);
                fwrite(data->scratch + off,  sizeof(uint64x2_t), NRK4B512_LEN, f);
                fprintf(f, "k4bgap_l2_512k sweep=%lu off=%d\n",
                        static_cast<unsigned long>(sweep), off);
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k4bgap_l2_512k data miscompare (off %u, records in dump bin)", (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k4bgap_l2_512k_cleanup(struct test *test) {
    auto *data = static_cast<NRK4B512Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k4bgap_l2_512k, "L2-resident tier 512KB of the GPS chassis: tagged fill (32-bit-offset tag format — this family's own, complement field dropped) + read-side chassis preserved (scratch dead ldr + 2-src asm loads + rot ALU + bb str/str), sliding-window geometry transplanted; field-graft slice dump on fail (ARM64)")
    .test_init    = k4bgap_l2_512k_init,
    .test_run     = k4bgap_l2_512k_run,
    .test_cleanup = k4bgap_l2_512k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
