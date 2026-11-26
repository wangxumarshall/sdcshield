/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k3res_l2_256k
 * @parblock
 * L2-resident tier 256KB of the K3 residency cell (2026-11-26, user
 * directive: continue the rowmajor L2-tier family — the K3 cell is the
 * last chassis without L2 tiers).  The parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_k3res.cpp) is an
 * ADJUDICATION cell of the CORE179 read-path campaign (§5 K3 axis):
 * a 4x-L1D footprint (4096 elems/buffer, 256 KiB total) used once to
 * adjudicate whether the field-substitution sources are L1D-resident
 * at failure time.
 *
 * MISSION SHIFT, DISCLOSED HERE: this tier does NOT re-run that
 * adjudication — the parent's pre-registered readouts (rate collapse
 * vs far-source histogram growth) belong to the one-shot campaign,
 * not to a standing detector.  What this tier inherits is the
 * chassis: GPS-tagged field-graft fill (srcA/srcB magics
 * complementary, a two-source splice visible inside one 64b word) +
 * five-buffer read-side compute (scratch dead ldr + 2-src asm loads +
 * rot4 ALU + bb str/str) + scalar golden — at L2-sized input blocks,
 * as a standing L2-domain slow detector for the target machines.
 *
 * TAG LAYOUT: this tier family uses the gps_l2 32-bit-offset format
 * (2026-11-26 ruling, inherited): (magic<<48)|((off&0xFFFFFFFF)<<16)
 * |gen — the parent's 16-bit byte_off caps at 65520, which the L2
 * windows exceed; the one's-complement integrity field is dropped to
 * make room (magic stays 16b top, gen 16b bottom).  The offline
 * classifier must use this decode for the _l2 files.
 *
 * Input-data method transplant (the unified batch template of the 1a/
 * 1b waves): fixed block = tier + one random 64B-aligned SHARED offset
 * per sweep through a 2x window + 0x5A poison on the compared
 * destination slice; the tagged fill and the scalar golden cover the
 * FULL window; the sweep-end memcmp narrows to the slice
 * [off, off+LEN); the fail dump covers the slice.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64x2_t = 256KB (4x L1D,
 * inside the >64KB..<512KB window); per-sweep touched set = scratch +
 * srcA + srcB reads + temp + dst writes = 5 x 256KB = 1.25MB — 2.5x
 * L2, the five-buffer chassis crosses into the L2/L3 mixed domain
 * (disclosed honestly; the tier axis is the input block size).  The
 * 128K/512K siblings sit at 640KB/2.5MB touched per sweep.  Memory:
 * run-thread temp+dst = 2 x WIN x 16B = 1MB here (512K tier
 * 2MB/thread); init-shared 4 buffers x WIN x 16B = 2MB.
 *
 * TARGET-MACHINE PROTOCOL (2026-11-26, user ruling): on host 0102 the
 * verification stage drops core 179 from full-core runs (offline the
 * known-bad core first — two full-core runs of this family's
 * k2pureload/k7p_hiham neighbours reset the machine today); on
 * healthy hosts run full-core — these tests may be very effective
 * elsewhere.
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

#define NRK3256_LEN 16384                 /* uint64x2_t elements -> 256KB block */
#define NRK3256_WIN (2 * NRK3256_LEN)     /* 2x-block window (memcpy_l2 method) */
#define NRK3256_DUMP_DIR "movbe_log/k3res_rowmajor_l2"

#define NRK3256_MAGIC_A   0xA5A5ULL
#define NRK3256_MAGIC_B   0x5A5AULL
#define NRK3256_MAGIC_SCR 0x0F0FULL

struct NRK3256Data {
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

/* One tagged word — THE 32-BIT-OFFSET FORMAT of the _l2 tier family
 * (see header): magic 16b top, byte_off 32b middle (the parent's 16b
 * field caps at 65520 and the L2 windows exceed it), gen 16b bottom;
 * the parent's one's-complement integrity field is dropped to make
 * room. */
static inline uint64_t nrk3256_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                        << 48)
         | ((byte_off        & 0xFFFFFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k3res_l2_256k_init(struct test *test) {
    auto *data = static_cast<NRK3256Data *>(malloc(sizeof(NRK3256Data)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK3256_WIN * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK3256_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRK3256_WIN * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, NRK3256_WIN * sizeof(uint64x2_t)));

    mkdir("movbe_log", 0755);
    mkdir(NRK3256_DUMP_DIR, 0755);  /* EEXIST is fine; 09-08 lesson: dump to disk */

    /* Tagged fill over the FULL window — the ONLY functional delta vs
     * the parent's chassis (same complementary magics). */
    for (int i = 0; i < NRK3256_WIN; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i],    vcombine_u64(vcreate_u64(nrk3256_tag(NRK3256_MAGIC_A,   byte + 0, 0)),
                                                  vcreate_u64(nrk3256_tag(NRK3256_MAGIC_A,   byte + 8, 0))));
        store_vec(&data->srcB[i],    vcombine_u64(vcreate_u64(nrk3256_tag(NRK3256_MAGIC_B,   byte + 0, 0)),
                                                  vcreate_u64(nrk3256_tag(NRK3256_MAGIC_B,   byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(nrk3256_tag(NRK3256_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(nrk3256_tag(NRK3256_MAGIC_SCR, byte + 8, 0))));
    }
    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window — pure C integer
     * ops on the scalar pipeline, never the NEON units under test. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRK3256_WIN; e++) {
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

static int k3res_l2_256k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRK3256Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRK3256_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRK3256_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot4 slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRK3256_LEN / 4)));

        /* poison the compared destination block: a lost write stays
         * 0x5a5a.. and fails the slice compare */
        memset(dst + off, 0x5A, NRK3256_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRK3256_LEN; n++) {
            const int j = ascending ? n : (NRK3256_LEN - 1 - n);
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
            store_vec(&dst[i], res);
        }
        sweep++;

        /* cold path: slice compare — only the window is promised
         * written this sweep; the dump covers the slice */
        if (memcmp(dst + off, data->expected + off, NRK3256_LEN * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k3res_l2_256k_fail_cpu%d_sweep%lu_off%d.bin", NRK3256_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst + off,            sizeof(uint64x2_t), NRK3256_LEN, f);
                fwrite(data->expected + off, sizeof(uint64x2_t), NRK3256_LEN, f);
                fwrite(data->srcA + off,     sizeof(uint64x2_t), NRK3256_LEN, f);
                fwrite(data->srcB + off,     sizeof(uint64x2_t), NRK3256_LEN, f);
                fwrite(data->scratch + off,  sizeof(uint64x2_t), NRK3256_LEN, f);
                fprintf(f, "k3res_l2_256k sweep=%lu off=%d\n",
                        static_cast<unsigned long>(sweep), off);
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k3res_l2_256k data miscompare (off %u, records in dump bin)", (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k3res_l2_256k_cleanup(struct test *test) {
    auto *data = static_cast<NRK3256Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k3res_l2_256k, "L2-resident tier 256KB of the K3 residency chassis (adjudication mission retired at L1D, chassis kept): tagged fill (32-bit-offset tag format — this family's own, complement field dropped) + read-side chassis preserved (scratch dead ldr + 2-src asm loads + rot ALU + bb str/str), sliding-window geometry transplanted; field-graft slice dump on fail (ARM64)")
    .test_init    = k3res_l2_256k_init,
    .test_run     = k3res_l2_256k_run,
    .test_cleanup = k3res_l2_256k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
