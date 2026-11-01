/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b agu_stress_unaligned
 * @parblock
 * AGU stress: unaligned-address probe. Same 2-src + rotating ALU +
 * store/reload/store skeleton as agu_stress_2src, but each element occupies a
 * 16-byte slot and is accessed at a rotating byte offset (0..7) within the
 * slot, so 7/8 of all accesses are non-8B-aligned (slots never overlap).
 * AArch64 ldr/str are architecturally unaligned-safe for normal memory, so the
 * house inline-asm helpers are used unchanged — stressing the AGU's unaligned
 * address-computation and access path. Golden values are recomputed from the
 * same shifted source reads (cold verify loop, hot loop untouched).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define AGU_UNALIGNED_COUNT 1024
#define SLOT 16   // per-element slot size in bytes; offset 0..7 inside

struct AguUnalignedData {
    char *srcA;   // COUNT*SLOT bytes
    char *srcB;
};

#if defined(__aarch64__)

static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("str %1, [%0]" : : "r"(addr), "r"(val) : "memory");
}

static inline uint64_t load_qword(const void *addr) {
    uint64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=r"(res) : "r"(addr) : "memory");
    return res;
}

#endif

// address of element i's data at its rotating offset
static inline char *slot_addr(char *base, int i) {
    return base + (size_t)i * SLOT + (i & 7);
}

static int agu_stress_unaligned_init(struct test *test) {
    auto *data = static_cast<AguUnalignedData *>(malloc(sizeof(AguUnalignedData)));
    data->srcA = static_cast<char *>(aligned_alloc(64, (size_t)AGU_UNALIGNED_COUNT * SLOT));
    data->srcB = static_cast<char *>(aligned_alloc(64, (size_t)AGU_UNALIGNED_COUNT * SLOT));

    memset_random(data->srcA, (size_t)AGU_UNALIGNED_COUNT * SLOT);
    memset_random(data->srcB, (size_t)AGU_UNALIGNED_COUNT * SLOT);

    test->data = data;
    return EXIT_SUCCESS;
}

static int agu_stress_unaligned_run(struct test *test, int cpu) {
    auto *data = static_cast<AguUnalignedData *>(test->data);

    char *temp = static_cast<char *>(aligned_alloc(64, (size_t)AGU_UNALIGNED_COUNT * SLOT));
    char *dst  = static_cast<char *>(aligned_alloc(64, (size_t)AGU_UNALIGNED_COUNT * SLOT));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < AGU_UNALIGNED_COUNT; i++) {
            uint64_t a = load_qword(slot_addr(data->srcA, i));
            uint64_t b = load_qword(slot_addr(data->srcB, i));
            uint64_t res;

            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a ^ b; break;
                case 2: res = a & b; break;
                case 3: res = a | b; break;
            }

            char *t = slot_addr(temp, i);
            store_qword(t, res);
            store_qword(slot_addr(dst, i), load_qword(t));
        }

        // cold verify: recompute golden from the same shifted source reads
        bool ok = true;
        for (int i = 0; i < AGU_UNALIGNED_COUNT; i++) {
            uint64_t a = load_qword(slot_addr(data->srcA, i));
            uint64_t b = load_qword(slot_addr(data->srcB, i));
            uint64_t d = load_qword(slot_addr(dst, i));
            uint64_t exp;
            switch (i % 4) {
                case 0: exp = a + b; break;
                case 1: exp = a ^ b; break;
                case 2: exp = a & b; break;
                case 3: exp = a | b; break;
            }
            if (d != exp) { ok = false; break; }
        }
        if (!ok) {
            report_fail_msg("agu_stress_unaligned data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int agu_stress_unaligned_cleanup(struct test *test) {
    auto *data = static_cast<AguUnalignedData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(agu_stress_unaligned, "AGU stress: unaligned rotating-offset access (2-src + ALU rotation + store/reload/store)")
    .test_init    = agu_stress_unaligned_init,
    .test_run     = agu_stress_unaligned_run,
    .test_cleanup = agu_stress_unaligned_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
