/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_gps
 * @parblock
 * GPS-tagged chassis of neon_rot_ldr_at_top_rowmajor (2026-09-13).
 *
 * Phase 0' B-track of the readpath-localization campaign
 * (docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md).  The hot
 * loop is the family skeleton (scratch ldr at top + 2-src loads + i%4
 * rotating ALU + back-to-back str/str + rowmajor alternating sweeps +
 * cold-path memcmp gate) and stays byte-identical to the parent; ONLY
 * the buffer fill differs: every 64-bit word carries a GPS tag so each
 * captured fail self-reports WHERE its bytes came from, instead of
 * being a bare count.
 *
 * Tag layout (one 64-bit word):
 *
 *   bits 63..48  magic    = 0xA5A5 (srcA) / 0x5A5A (srcB) / 0x0F0F (scratch)
 *   bits 47..32  byte_off = byte offset of this word within its buffer
 *   bits 31..16  ~byte_off (complement — integrity check on the address
 *                          fields; a window whose fields disagree with
 *                          their complement flags field-level tampering)
 *   bits 15..0   generation = 0 (CONSTANT in this chassis — see below)
 *
 * srcA and srcB carry COMPLEMENTARY magics so a two-source splice shows
 * A-bytes and B-bytes mixed inside one 64b word — the K5 discriminator
 * grows directly on this encoding.  Scratch has its own magic.
 *
 * Why generation stays constant here: rotating it would require
 * rewriting srcA/srcB between sweeps, which (a) invalidates the
 * init-computed golden (detector breaks) and (b) turns srcA/srcB into
 * dirty, just-stored lines — a cache-state confound on exactly the
 * layout the campaign must freeze.  The stale-generation bucket belongs
 * to the K2 store-geometry variants (real store->reload windows), where
 * the stored data itself is generation-tagged; it is NOT populated by
 * this chassis.  In this chassis every sweep produces identical values,
 * so "old data from another slot" lands in the wrong-line/wrong-word
 * buckets, which the byte_off field resolves on its own.
 *
 * Classification is OFFLINE (hot loop never branches): a failing dst
 * word is matched against the dumped workspace snapshot -> wrong line /
 * wrong word / misaligned byte window (mod-8 histogram) / two-source
 * splice / ALU-mix-in (reconstructed from the snapshot: any candidate
 * "a OP b" is computable offline) / partial-word / bitflip-like
 * (unrecoverable, low Hamming) / unknown.  Signature classification is
 * robust to rate confounds: tagging may change the trigger rate (Phase
 * 0.5 measures that, A-track natural data vs B-track tagged) but cannot
 * change WHERE an already-captured event came from.
 *
 * Dump discipline (09-08 lesson: dumps MUST go to disk, /tmp lost 26
 * records): on miscompare the cold path writes the full five-buffer
 * context to movbe_log/gps_rowmajor/ (dir created in init) as
 * fail_cpu<cpu>_sweep<sweep>.bin + one text line, then report_fail_msg.
 *
 * Engineering constraints inherited from the family:
 *   - hot loop byte-identical to the parent chassis (objdump-verified);
 *   - scratch/src loads via asm volatile (DCE guard);
 *   - no LCG seed fixing; auto-seed only.
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

#define GPS_ROWMAJOR_COUNT 1024
#define GPS_DUMP_DIR "movbe_log/gps_rowmajor"

#define GPS_MAGIC_A   0xA5A5ULL
#define GPS_MAGIC_B   0x5A5AULL
#define GPS_MAGIC_SCR 0x0F0FULL

struct GpsRowmajorData {
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

/* One tagged word.  magic identifies the SOURCE buffer family; byte_off
 * is the word's byte offset inside its buffer (0..16383 here, fits 16b);
 * the complement of byte_off doubles as an integrity check; gen is the
 * generation (constant 0 in this chassis, see file header). */
static inline uint64_t gps_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                      << 48)
         | ((byte_off        & 0xFFFFULL) << 32)
         | ((~byte_off       & 0xFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int gps_rowmajor_init(struct test *test) {
    auto *data = static_cast<GpsRowmajorData *>(malloc(sizeof(GpsRowmajorData)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)));

    mkdir(GPS_DUMP_DIR, 0755);  /* EEXIST is fine; 09-08 lesson: dump to disk */

    /* Tagged fill replaces memset_random — the ONLY functional delta vs
     * the parent chassis.  srcA/srcB magics are complementary so a
     * two-source splice is visible inside a single 64b word. */
    for (int i = 0; i < GPS_ROWMAJOR_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i],    vcombine_u64(vcreate_u64(gps_tag(GPS_MAGIC_A,   byte + 0, 0)),
                                                  vcreate_u64(gps_tag(GPS_MAGIC_A,   byte + 8, 0))));
        store_vec(&data->srcB[i],    vcombine_u64(vcreate_u64(gps_tag(GPS_MAGIC_B,   byte + 0, 0)),
                                                  vcreate_u64(gps_tag(GPS_MAGIC_B,   byte + 8, 0))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(gps_tag(GPS_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(gps_tag(GPS_MAGIC_SCR, byte + 8, 0))));
    }

    /* golden: same rotating composition, computed on the same NEON units
     * — identical to the parent (the tags flow through the ALU; e.g. an
     * eor word carries magic A^B = 0xFFFF in its top half, which the
     * offline classifier reconstructs exactly). */
    for (int i = 0; i < GPS_ROWMAJOR_COUNT; i++) {
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

static int gps_rowmajor_run(struct test *test, int cpu) {
    auto *data = static_cast<GpsRowmajorData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < GPS_ROWMAJOR_COUNT; n++) {
            const int i = ascending ? n : (GPS_ROWMAJOR_COUNT - 1 - n);

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

        if (memcmp(dst, data->expected, GPS_ROWMAJOR_COUNT * sizeof(uint64x2_t)) != 0) {
            /* COLD path, gated by memcmp — never enters the hot loop.
             * Full five-buffer context dump for the offline classifier. */
            char path[256];
            snprintf(path, sizeof(path), "%s/fail_cpu%d_sweep%lu.bin", GPS_DUMP_DIR, cpu,
                     static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst,           sizeof(uint64x2_t), GPS_ROWMAJOR_COUNT, f);
                fwrite(data->expected,sizeof(uint64x2_t), GPS_ROWMAJOR_COUNT, f);
                fwrite(data->srcA,    sizeof(uint64x2_t), GPS_ROWMAJOR_COUNT, f);
                fwrite(data->srcB,    sizeof(uint64x2_t), GPS_ROWMAJOR_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), GPS_ROWMAJOR_COUNT, f);
                fprintf(f, "cpu=%d sweep=%lu\n", cpu, static_cast<unsigned long>(sweep));
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_gps data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int gps_rowmajor_cleanup(struct test *test) {
    auto *data = static_cast<GpsRowmajorData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_gps, "GPS-tagged chassis of ldr_at_top_rowmajor: every 64b word self-reports its source (magic|byte_off|~byte_off|gen); Phase 0' B-track of the readpath-localization campaign")
    .test_init    = gps_rowmajor_init,
    .test_run     = gps_rowmajor_run,
    .test_cleanup = gps_rowmajor_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
