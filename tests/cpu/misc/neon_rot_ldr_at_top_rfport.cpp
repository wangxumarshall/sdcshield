/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_rfport
 * @parblock
 * Register-file-port-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-09).
 *
 * The parent's per-element register traffic is 3 loads (3 RF writes) +
 * one ALU op (2 RF reads + 1 RF write) + 2 stores (2 RF reads). This
 * variant inserts a chain of 4 pure register-to-register NEON moves
 * (vmov v4, v5 / v5, v6 / v6, v7 / v7, v8) between the source loads and
 * the rotating ALU op. The moves touch NO memory -- they add only
 * register-file read/write port occupancy and bypass-network traffic,
 * plus a serial 4-deep register dependency that the renaming engine must
 * track. The trigger skeleton (3rd ldr at top + 2-src loads + rotating
 * ALU + back-to-back str/str, alternating sweeps) is otherwise intact.
 *
 * NOTE: like lsqfill this changes hot-loop timing (by design); it answers
 * "does RF port / bypass pressure influence the trigger rate relative to
 * the parent skeleton". The moves are emitted via inline asm so GCC
 * cannot fold or elide them.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_RFPORT_COUNT 1024

struct NeonRotLdrAtTopRfportData {
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

/* 4-deep pure register move chain; input in %0, output in %0. The chain
 * v4->v5->v6->v7->v8 is written as consecutive ORR-with-vzr moves on
 * distinct scratch registers, then the last one is returned. */
static inline uint64x2_t rf_move_chain(uint64x2_t in) {
    uint64x2_t out;
    __asm__ volatile (
        "mov v4.16b, %0.16b   \n\t"
        "mov v5.16b, v4.16b   \n\t"
        "mov v6.16b, v5.16b   \n\t"
        "mov v7.16b, v6.16b   \n\t"
        "mov %0.16b, v7.16b   \n\t"
        : "=w"(out) : "w"(in) : "v4", "v5", "v6", "v7");
    return out;
}

#endif

static int neon_rot_ldr_at_top_rfport_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRfportData *>(malloc(sizeof(NeonRotLdrAtTopRfportData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t));

    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_RFPORT_COUNT; i++) {
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

static int neon_rot_ldr_at_top_rfport_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRfportData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_RFPORT_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_RFPORT_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);

            /* 4-deep register move chain on the a-operand: pure RF port
             * + bypass pressure, no memory traffic. Value preserved. */
            a = rf_move_chain(a);

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

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_RFPORT_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_rfport data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rfport_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRfportData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rfport, "RF-port-dimension variant of ldr_at_top_rowmajor: 4-deep asm NEON register move chain between source loads and ALU op (pure RF port/bypass pressure, no memory traffic); trigger skeleton intact")
    .test_init    = neon_rot_ldr_at_top_rfport_init,
    .test_run     = neon_rot_ldr_at_top_rfport_run,
    .test_cleanup = neon_rot_ldr_at_top_rfport_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
