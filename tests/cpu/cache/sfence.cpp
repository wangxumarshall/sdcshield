/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test sfence
 * @parblock
 * SFENCE test — using non-temporal and regular store instructions
 * (633 #34 lineage, user spec 2026-11-02).
 *
 * The x86 SFENCE orders STORE COMPLETION across a barrier.  Its most
 * precise ARM64 equivalent is DMB ISHST (store-only ordering,
 * inner-shareable) — narrower and closer than the draft's DSB SY;
 * DSB SY still has a role here, but as the COMPLETION fence before
 * verification, because STNP's stores have weak completion and a
 * bare read-back races the write buffer.
 *
 * Structure per round (the spec's wording, made literal):
 *   1. golden <- fresh random64 words (independent, never written
 *      after the fill);
 *   2. NON-TEMPORAL group: half the word-pairs (which half rotates
 *      every round) written with inline-asm STNP — the ARM64
 *      non-temporal store, bypassing the cache hierarchy;
 *   3. DMB ISHST — the SFENCE itself: the NT stores are ordered
 *      before everything the regular group is about to issue;
 *   4. REGULAR group: the other half of the pairs written with
 *      plain inline-asm STR (cacheable path);
 *   5. DSB SY — completion drain (STNP weak-completion fence);
 *   6. verify: every word read back through inline-asm LDR (not
 *      forwardable to the register file) and bit-compared against
 *      golden.  The NT lanes verify the write-buffer/memory path,
 *      the STR lanes the cacheable store path, and the barrier
 *      guarantees neither group's reads race its own stores.
 *
 * Design traps found in the user's draft (fixed, same family set):
 *   - the description names non-temporal + regular stores; the draft
 *     contained NO non-temporal store at all (just memcpy + DSB SY) —
 *     STNP pair-writes + interleaved STR are the actual workload now;
 *   - the local store->load consistency pass (tmp = v; loaded = tmp;)
 *     is the compiler-forwarding fake — sixth sighting today — the
 *     verification is the asm-ldr full-buffer sweep instead;
 *   - alignas(64) on std::vector (stack object, not heap data)
 *     removed; std::mt19937/random_device floats -> framework
 *     random64 words, bitwise compare; per-fail fprintf removed;
 *     .fracture_loop_count = -1; _cleanup naming.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <vector>

static constexpr size_t SF_NUM_WORDS = 1024;   /* 4 KiB, 512 pairs */

/* The SFENCE equivalent: store-only ordering, inner-shareable */
static inline void dmb_ishst() {
    __asm__ volatile("dmb ishst" ::: "memory");
}

/* completion fence — STNP's weak completion needs this before verify */
static inline void dsb_sy() {
    __asm__ volatile("dsb sy" ::: "memory");
}

/* non-temporal pair store: two 32-bit words straight past the cache */
static inline void stnp_pair(uint32_t *addr, uint32_t w0, uint32_t w1) {
    __asm__ volatile("stnp %w0, %w1, [%2]"
                     :: "r"(w0), "r"(w1), "r"(addr) : "memory");
}

/* plain cacheable 32-bit store */
static inline void str_word(uint32_t *addr, uint32_t w) {
    __asm__ volatile("str %w0, [%1]" :: "r"(w), "r"(addr) : "memory");
}

/* anti-forwarding load: really goes to the LSU, not the register file */
static inline uint32_t ldr_word(const uint32_t *addr) {
    uint32_t v;
    __asm__ volatile("ldr %w0, [%1]" : "=r"(v) : "r"(addr) : "memory");
    return v;
}

static int sfence_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int sfence_cleanup(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int sfence_run(struct test *test, int cpu) {
    (void)cpu;

    std::vector<uint32_t> buf(SF_NUM_WORDS);
    std::vector<uint32_t> golden(SF_NUM_WORDS);

    uint64_t rounds = 0;

    do {
        /* 1. fresh independent golden */
        for (size_t i = 0; i < SF_NUM_WORDS; ++i)
            golden[i] = (uint32_t)random64();

        /* which half of the pairs goes non-temporal rotates per round */
        const unsigned phase = (unsigned)(rounds & 1);

        /* 2. non-temporal group: STNP pair-writes */
        for (size_t p = 0; p < SF_NUM_WORDS / 2; ++p) {
            if (((p + phase) & 1) == 0)
                stnp_pair(&buf[p * 2], golden[p * 2], golden[p * 2 + 1]);
        }

        /* 3. the SFENCE: order the NT stores before the regular ones */
        dmb_ishst();

        /* 4. regular group: plain cacheable stores */
        for (size_t p = 0; p < SF_NUM_WORDS / 2; ++p) {
            if (((p + phase) & 1) != 0) {
                str_word(&buf[p * 2], golden[p * 2]);
                str_word(&buf[p * 2 + 1], golden[p * 2 + 1]);
            }
        }

        /* 5. completion drain before any read-back */
        dsb_sy();

        /* 6. full asm-ldr sweep vs golden */
        for (size_t i = 0; i < SF_NUM_WORDS; ++i) {
            uint32_t v = ldr_word(&buf[i]);
            if (v != golden[i]) {
                report_fail_msg("sfence: read-back mismatch at word %zu "
                                "(round %lu, %s lane): expected 0x%08x "
                                "got 0x%08x", i, (unsigned long)rounds,
                                (((i / 2) + phase) & 1) ? "regular" :
                                "non-temporal", golden[i], v);
                return EXIT_FAILURE;
            }
        }

        ++rounds;
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

DECLARE_TEST(sfence, "SFENCE via dmb-ishst between STNP non-temporal and regular str store groups; dsb-sy completion drain; full asm-ldr bit verification (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = sfence_init,
    .test_run = sfence_run,
    .test_cleanup = sfence_cleanup,
    .fracture_loop_count = -1,   /* never fracture: fast semantic rounds */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST
