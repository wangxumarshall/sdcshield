/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mem_disambig_alu
 * @parblock
 * Variant of mem_disambiguation that adds a data-ALU step before the store,
 * to test whether "2-source load + ALU + forwarding at random address" triggers
 * SDC. mem_disambiguation already has 2 sources (offsets, values) but stores
 * the raw value; this variant stores an ALU result (value op offset).
 *
 * Diff vs mem_disambiguation: store value from raw values[i] → ALU(values[i] op offsets[i]).
 * Skeleton (pattern1) unchanged: store_qword(buffer+off, alu_result); load_qword(buffer+off).
 * @endparblock
 */

#include "sandstone.h"

#include <cstdint>

#define DISAMBIG_ALU_BUFFER_SIZE   4096
#define DISAMBIG_ALU_PATTERN_COUNT  512

struct MemDisambigAluData {
    uint64_t *offsets;   /* random 8-byte-aligned offsets within buffer */
    uint64_t *values;    /* random values to store                       */
    uint64_t *expected;  /* expected ALU results                         */
};

#if defined(__x86_64__) || defined(__i386__)

static inline void store_qword(void *addr, uint64_t val)
{
    __asm__ volatile ("movq %1, %0"
        : "=m"(*reinterpret_cast<uint64_t*>(addr))
        : "r"(val)
        : "memory");
}

static inline uint64_t load_qword(const void *addr)
{
    uint64_t result;
    __asm__ volatile ("movq %1, %0"
        : "=r"(result)
        : "m"(*reinterpret_cast<const uint64_t*>(addr))
        : "memory");
    return result;
}

#elif defined(__aarch64__)

static inline void store_qword(void *addr, uint64_t val)
{
    __asm__ volatile ("str %1, [%0]"
        :
        : "r"(addr), "r"(val)
        : "memory");
}

static inline uint64_t load_qword(const void *addr)
{
    uint64_t result;
    __asm__ volatile ("ldr %0, [%1]"
        : "=r"(result)
        : "r"(addr)
        : "memory");
    return result;
}

#endif /* platform */

static int mem_disambig_alu_init(struct test *test)
{
    auto *data = static_cast<MemDisambigAluData *>(malloc(sizeof(MemDisambigAluData)));
    data->offsets  = static_cast<uint64_t *>(malloc(DISAMBIG_ALU_PATTERN_COUNT * sizeof(uint64_t)));
    data->values   = static_cast<uint64_t *>(malloc(DISAMBIG_ALU_PATTERN_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(malloc(DISAMBIG_ALU_PATTERN_COUNT * sizeof(uint64_t)));

    for (int i = 0; i < DISAMBIG_ALU_PATTERN_COUNT; i++) {
        data->offsets[i] = (random64() % (DISAMBIG_ALU_BUFFER_SIZE - 16)) & ~7ULL;
        data->values[i]  = random64();
        // ALU result: rotate xor/add/and/sub (data ALU on 2 sources)
        switch (i % 4) {
            case 0: data->expected[i] = data->values[i] ^ data->offsets[i]; break;
            case 1: data->expected[i] = data->values[i] + data->offsets[i]; break;
            case 2: data->expected[i] = data->values[i] & data->offsets[i]; break;
            case 3: data->expected[i] = data->values[i] - data->offsets[i]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mem_disambig_alu_run(struct test *test, int cpu)
{
    auto *data = static_cast<MemDisambigAluData *>(test->data);

    // Per-thread buffer (same as mem_disambiguation)
    uint8_t *buffer = static_cast<uint8_t *>(aligned_alloc(64, DISAMBIG_ALU_BUFFER_SIZE));

    TEST_LOOP(test, 1 << 13) {
        // Pattern 1 — Basic store-to-load forwarding with ALU on 2 sources.
        // Skeleton unchanged from mem_disambiguation pattern1, only the stored
        // value is now an ALU result instead of the raw value.
        for (int i = 0; i < DISAMBIG_ALU_PATTERN_COUNT; i++) {
            uint64_t off   = data->offsets[i];
            uint64_t value = data->values[i];
            uint64_t res;

            // Data ALU on 2 source loads (offsets + values)
            switch (i % 4) {
                case 0: res = value ^ off; break;
                case 1: res = value + off; break;
                case 2: res = value & off; break;
                case 3: res = value - off; break;
            }

            store_qword(buffer + off, res);
            uint64_t loaded = load_qword(buffer + off);

            if (loaded != data->expected[i]) {
                report_fail_msg(
                    "store-to-load forward (ALU) mismatch at pattern %d "
                    "(offset %llu): expected 0x%llx got 0x%llx",
                    i,
                    static_cast<unsigned long long>(off),
                    static_cast<unsigned long long>(data->expected[i]),
                    static_cast<unsigned long long>(loaded));
            }
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

static int mem_disambig_alu_cleanup(struct test *test)
{
    auto *data = static_cast<MemDisambigAluData *>(test->data);
    if (data) {
        free(data->expected);
        free(data->values);
        free(data->offsets);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mem_disambig_alu, "mem_disambiguation variant: 2-source load + data-ALU before forwarding store")
    .test_init    = mem_disambig_alu_init,
    .test_run     = mem_disambig_alu_run,
    .test_cleanup = mem_disambig_alu_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
