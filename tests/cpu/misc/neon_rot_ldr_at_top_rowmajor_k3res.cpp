/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k3res
 * @parblock
 * K3 residency cell: 4x-L1D footprint (2026-09-13,
 * docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md §5 K3 axis).
 *
 * Adjudicates whether the substitution/misaligned-window SOURCE lines
 * are L1D-resident at failure time.  The champion footprint (16 KiB
 * per buffer, 4 buffers = 64 KiB total) sits inside the 64 KiB L1D of
 * Kunpeng 920, so in the GPS chassis every source word the classifier
 * ever identifies was necessarily L1D-resident.  This cell scales the
 * working set 4x (4096 elements per buffer = 64 KiB per buffer, 256
 * KiB total) so a random sweep evicts most lines before they are
 * re-referenced: if the field-substitution sources REMAIN at
 * near-neighbour distances (+-1..3 lines) they are still resident
 * (recently touched by the sweep itself) and the cell distinguishes
 * nothing; but if the signature SURVIVES the footprint change at the
 * same rate while the far-neighbour (evicted) sources DISAPPEAR from
 * the histogram, residency is a necessary condition for the source —
 * pointing INSIDE the L1D array / way-select level rather than any
 * later pipeline stage.
 *
 * Pre-registered readouts:
 *   - rate collapses -> the 64 KiB-resident champion footprint is part
 *     of the trigger recipe (footprint sensitivity, feeds the
 *     footprint-independent-of-L1D-size memory re-check);
 *   - rate holds + source-distance histogram still tight (all resident
 *     neighbours) -> no eviction exposure, cell is uninformative for
 *     residency but the rate-invariance under 4x footprint is still a
 *     useful negative;
 *   - rate holds + histogram grows FAR sources -> far (evicted)
 *     sources participate; L1D-internal selection is disfavoured.
 *
 * Dump: same five-buffer discipline (buffers are 4x larger; dump files
 * ~320 KiB each).  GPS tags unchanged — byte_off still fits 16b
 * (max 65535 >= 65536-16).  CAREFUL: byte offsets reach 65520, which
 * still fits the 16b field; the classifier's byte_off decode is
 * footprint-agnostic.
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

#define K3RES_COUNT 4096              /* 4x the champion: 64 KiB per buffer */
#define K3RES_DUMP_DIR "movbe_log/gps_rowmajor"

#define K3RES_MAGIC_A   0xA5A5ULL
#define K3RES_MAGIC_B   0x5A5AULL
#define K3RES_MAGIC_SCR 0x0F0FULL

struct K3ResData {
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

static inline uint64_t gps_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                      << 48)
         | ((byte_off        & 0xFFFFULL) << 32)
         | ((~byte_off       & 0xFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k3res_init(struct test *test) {
    auto *data = static_cast<K3ResData *>(malloc(sizeof(K3ResData)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, K3RES_COUNT * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, K3RES_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K3RES_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, K3RES_COUNT * sizeof(uint64x2_t)));

    mkdir(K3RES_DUMP_DIR, 0755);

    for (int i = 0; i < K3RES_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i], vcombine_u64(vcreate_u64(gps_tag(K3RES_MAGIC_A, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K3RES_MAGIC_A, byte + 8, 0))));
        store_vec(&data->srcB[i], vcombine_u64(vcreate_u64(gps_tag(K3RES_MAGIC_B, byte + 0, 0)),
                                               vcreate_u64(gps_tag(K3RES_MAGIC_B, byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(gps_tag(K3RES_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(gps_tag(K3RES_MAGIC_SCR, byte + 8, 0))));
    }

    for (int i = 0; i < K3RES_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k3res_run(struct test *test, int cpu) {
    auto *data = static_cast<K3ResData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, K3RES_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, K3RES_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < K3RES_COUNT; n++) {
            const int i = ascending ? n : (K3RES_COUNT - 1 - n);

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

        if (memcmp(dst, data->expected, K3RES_COUNT * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/k3res_fail_cpu%d_sweep%lu.bin", K3RES_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst,           sizeof(uint64x2_t), K3RES_COUNT, f);
                fwrite(data->expected,sizeof(uint64x2_t), K3RES_COUNT, f);
                fwrite(data->srcA,    sizeof(uint64x2_t), K3RES_COUNT, f);
                fwrite(data->srcB,    sizeof(uint64x2_t), K3RES_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K3RES_COUNT, f);
                fprintf(f, "k3res sweep=%lu\n", static_cast<unsigned long>(sweep));
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k3res data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k3res_cleanup(struct test *test) {
    auto *data = static_cast<K3ResData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k3res, "K3 residency cell: 4x-L1D footprint (4096 elems/buffer, 256 KiB total) — exposes whether field-substitution sources are L1D-resident at failure time")
    .test_init    = k3res_init,
    .test_run     = k3res_run,
    .test_cleanup = k3res_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
