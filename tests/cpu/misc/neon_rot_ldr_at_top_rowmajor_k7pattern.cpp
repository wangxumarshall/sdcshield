/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k7p_zero / k7p_one / k7p_rand / k7p_hiham
 * @parblock
 * K7 pattern battery, 2x64b lane shape (2026-09-13,
 * docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md §8).
 *
 * Real question: is the lane curve (2x64b=223/30min on the parent,
 * 4x32b=0, 16x8b=2) a PHYSICAL-lane effect or a pattern-activation
 * effect?  The first GPS round added a sharp clue: the op distribution
 * of corrupted words is heavily biased (orr 71% / and 23% / eor 5% /
 * add 1%) and the corrupted fields avoid all-0/all-1 byte segments —
 * the damage is pattern-SENSITIVE, not uniform.  This battery maps the
 * activation surface of the 2x64b path directly:
 *
 *   k7p_zero : all-zero operands (both sources)         -> activation floor
 *   k7p_one  : all-one operands                          -> complementary floor
 *   k7p_rand : natural random (memset_random)            -> parent chassis baseline
 *   k7p_hiham: high-Hamming tags (alternating 0x55/0xAA) -> maximum transition density
 *
 * All four share the SAME hot loop (family skeleton, add v0.2d shape,
 * i%4 rotation) — only the fill differs, mirroring the GPS chassis
 * design.  The rand cell doubles as the A-track natural-data reference
 * with dump capability (the parent rowmajor has no dump; this gives
 * the Phase 0.5 A/B comparison its A side).
 *
 * Pre-registered readouts:
 *   - zero/one cells ~0 fail while rand/hiham fail -> activation needs
 *     data diversity in the operand (pattern-gating confirmed; the
 *     lane question becomes "which patterns survive the 32b ALU shape");
 *   - all cells fail similarly -> pattern gating is weak in the 2x64b
 *     path, the 4x32b zero needs a different explanation (lane shape);
 *   - hiham >> rand -> transition density matters (physical coupling).
 *
 * NOTE on the all-zero/all-one cells: with constant operands the ALU
 * results are also constant, so a corrupted word is trivially
 * distinguishable; the golden is the constant expectation.  The GPS
 * tag structure is REPLACED by the pattern itself in zero/one/hiham
 * cells (tags would be uniform data, defeating the point); the rand
 * cell uses GPS tags so the field-substitution classifier works on it.
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

#define K7P_COUNT 1024
#define K7P_DUMP_DIR "movbe_log/gps_rowmajor"

enum K7Pattern {
    P_ZERO,
    P_ONE,
    P_RAND_GPS,     /* GPS-tagged random = B-track shape */
    P_HIHAM,        /* 0x55AA alternating high-Hamming */
};

struct K7PData {
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

/* pattern fill: P_RAND_GPS keeps the GPS tag structure (classifier
 * works); the others are uniform patterns per the cell's purpose. */
static uint64_t pattern_word(enum K7Pattern p, int which, uint64_t byte_off) {
    switch (p) {
        case P_ZERO:     return 0x0000000000000000ULL;
        case P_ONE:      return 0xFFFFFFFFFFFFFFFFULL;
        case P_RAND_GPS: {
            const uint64_t magic = (which == 0) ? 0xA5A5ULL : ((which == 1) ? 0x5A5AULL : 0x0F0FULL);
            return gps_tag(magic, byte_off, 0);
        }
        case P_HIHAM: {
            /* high-Hamming with position info: alternate 0x55/0xAA per
             * byte so every adjacent byte pair differs in 8 bits, and
             * byte_off parity is recoverable offline */
            uint64_t w = 0;
            for (int b = 0; b < 8; b++) {
                const uint64_t nib = (((byte_off + static_cast<uint64_t>(b)) & 1) ? 0xAAULL : 0x55ULL);
                w |= (nib << (8 * b));
            }
            return w;
        }
    }
    return 0;
}

static int k7p_init_common(struct test *test, enum K7Pattern p) {
    auto *data = static_cast<K7PData *>(malloc(sizeof(K7PData)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, K7P_COUNT * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, K7P_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K7P_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, K7P_COUNT * sizeof(uint64x2_t)));

    mkdir(K7P_DUMP_DIR, 0755);

    for (int i = 0; i < K7P_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i],    vcombine_u64(vcreate_u64(pattern_word(p, 0, byte + 0)),
                                                  vcreate_u64(pattern_word(p, 0, byte + 8))));
        store_vec(&data->srcB[i],    vcombine_u64(vcreate_u64(pattern_word(p, 1, byte + 0)),
                                                  vcreate_u64(pattern_word(p, 1, byte + 8))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(pattern_word(p, 2, byte + 0)),
                                                  vcreate_u64(pattern_word(p, 2, byte + 8))));
    }

    for (int i = 0; i < K7P_COUNT; i++) {
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

static int k7p_run_common(struct test *test, int cpu, const char *name) {
    auto *data = static_cast<K7PData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, K7P_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, K7P_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < K7P_COUNT; n++) {
            const int i = ascending ? n : (K7P_COUNT - 1 - n);

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

        if (memcmp(dst, data->expected, K7P_COUNT * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/%s_fail_cpu%d_sweep%lu.bin", K7P_DUMP_DIR, name,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst,           sizeof(uint64x2_t), K7P_COUNT, f);
                fwrite(data->expected,sizeof(uint64x2_t), K7P_COUNT, f);
                fwrite(data->srcA,    sizeof(uint64x2_t), K7P_COUNT, f);
                fwrite(data->srcB,    sizeof(uint64x2_t), K7P_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K7P_COUNT, f);
                fprintf(f, "%s sweep=%lu\n", name, static_cast<unsigned long>(sweep));
                fclose(f);
            }
            report_fail_msg("%s data miscompare", name);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k7p_cleanup_common(struct test *test) {
    auto *data = static_cast<K7PData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

/* ---- four battery cells (same skeleton, fill-only delta) ---- */

static int k7p_zero_init(struct test *t)  { return k7p_init_common(t, P_ZERO); }
static int k7p_one_init(struct test *t)   { return k7p_init_common(t, P_ONE); }
static int k7p_rand_init(struct test *t)  { return k7p_init_common(t, P_RAND_GPS); }
static int k7p_hiham_init(struct test *t) { return k7p_init_common(t, P_HIHAM); }

static int k7p_zero_run(struct test *t, int c)  { return k7p_run_common(t, c, "k7p_zero"); }
static int k7p_one_run(struct test *t, int c)   { return k7p_run_common(t, c, "k7p_one"); }
static int k7p_rand_run(struct test *t, int c)  { return k7p_run_common(t, c, "k7p_rand"); }
static int k7p_hiham_run(struct test *t, int c) { return k7p_run_common(t, c, "k7p_hiham"); }

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_zero, "K7 pattern cell: all-zero operands (activation floor of the 2x64b path)")
    .test_init    = k7p_zero_init,
    .test_run     = k7p_zero_run,
    .test_cleanup = k7p_cleanup_common,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_one, "K7 pattern cell: all-one operands (complementary activation floor)")
    .test_init    = k7p_one_init,
    .test_run     = k7p_one_run,
    .test_cleanup = k7p_cleanup_common,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_rand, "K7 pattern cell: GPS-tagged random operands (B-track baseline; doubles as dump-capable A-track for Phase 0.5)")
    .test_init    = k7p_rand_init,
    .test_run     = k7p_rand_run,
    .test_cleanup = k7p_cleanup_common,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_hiham, "K7 pattern cell: 0x55/0xAA alternating high-Hamming operands (maximum transition density)")
    .test_init    = k7p_hiham_init,
    .test_run     = k7p_hiham_run,
    .test_cleanup = k7p_cleanup_common,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
