/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b isal_igzip
 * @parblock
 * Compression round-trip via isa-l igzip (stateless deflate -> inflate ->
 * byte-compare). Match search (data-dependent branches) + high-density
 * stores + Huffman entropy coding: a third compression scheduling sample
 * beside zstd/zlib (ITHICA: Zlib was the highest-yield fleet SDC detector;
 * CORE179 probe H/X: scheduling diversity is detection rate). Verifies both
 * directions: decompressed data must match the original input exactly, and
 * a fixed-input re-compression must reproduce the identical deflate stream
 * (determinism check on the compressor's data path).
 *
 * Compression level knob: -O isal_igzip.level=N, valid 0..3, default 1.
 * The four levels are four different match-finder data structures inside
 * isa-l — 0: static Huffman, no match search; 1: basic hash chain; 2:
 * deeper hash history (IGZIP_LVL2_HASH_SIZE); 3: hash map + long match —
 * i.e. four data-dependent-branch + integer-multiplier phases over the
 * same input. Levels 2/3 additionally exercise the level_buf code path:
 * the stateless API REQUIRES a match-finder scratch buffer there (level 1
 * may omit it per the header docs, level 0 never searches for matches).
 * The buffer is sized ISAL_DEF_LVL{2,3}_MIN — NOT the smaller *_REQ
 * constants: this libisal (2.30.0) rejects the bare REQ sizes with
 * ISAL_INVALID_LEVEL in stateless mode (stateless needs REQ plus one
 * token-history block, which is exactly the header's *_MIN suggested
 * size); see level_buf_size() below. The default (no knob) is level 1
 * without a level_buf — byte-identical to the historical behavior.
 * @endparblock
 */

#include <sandstone.h>

#include <isa-l/igzip_lib.h>

#include <string.h>
#include <stdlib.h>

#define IN_SIZE   (256 * 1024)
#define OUT_SIZE  (IN_SIZE + IN_SIZE / 2)

namespace {
struct igzip_test_data {
    uint8_t *input;                 /* read-only after init */
    uint8_t *comp_golden;           /* golden deflate stream, once in init */
    uint32_t golden_comp_size;
    int level;                      /* compression level 0..3 (knob, default 1) */
    uint8_t *level_buf;             /* golden-side match-finder scratch (levels 2/3) */
};
}

#define CAST(_x) static_cast<struct igzip_test_data *>(_x)

/* Match-finder scratch (level_buf) size the stateless API needs at each
 * level: 0 for levels 0/1, ISAL_DEF_LVL{2,3}_MIN for levels 2/3. Empirically
 * NOT the smaller ISAL_DEF_LVL{2,3}_REQ: this libisal (2.30.0) rejects the
 * bare REQ sizes in stateless mode with ISAL_INVALID_LEVEL — stateless
 * needs REQ plus one token-history block, and REQ + TOKEN_SIZE * 1 * IGZIP_K
 * is exactly the header's *_MIN suggested size (verified on the host
 * library; sizes MIN..DEFAULT all work, MIN is the smallest). */
static uint32_t level_buf_size(int level)
{
    switch (level) {
    case 3:  return ISAL_DEF_LVL3_MIN;
    case 2:  return ISAL_DEF_LVL2_MIN;
    default: return 0;
    }
}

/* Scratch buffers are per-thread: the framework runs one worker thread per
 * core and they all share test->data, so anything written during test_run
 * must be thread-local (comp is the re-compression destination, decomp the
 * inflate destination, and level_buf — levels 2/3 only — the match-finder
 * scratch the stateless API requires; all are written every iteration).
 * They are allocated lazily on first use and intentionally not freed in
 * cleanup, exactly like the openblas_* scratch buffers: the worker threads
 * are gone (or never existed) when the main thread runs test_cleanup, and
 * the whole forked child exits right after, so the OS reclaims the storage.
 * level_buf reuse across iterations is safe: its contents are per-call
 * scratch, isa-l reinitializes what it needs on every stateless call
 * (has_level_buf_init; verified empirically — pre-dirtied buffers yield
 * byte-identical output). */
static thread_local uint8_t *comp = nullptr;
static thread_local uint8_t *decomp = nullptr;
static thread_local uint8_t *level_buf = nullptr;

static int isal_igzip_init(struct test *test)
{
    auto d = new(igzip_test_data);
    test->data = d;
    /* compression-level knob (mdim/nelems precedent): read once, fail
     * loudly on bad values — a silent clamp would run a different
     * match-finder phase than the operator asked for */
    int64_t lvl = get_testspecific_knob_value_int(test, "level", 1);
    if (lvl < 0 || lvl > ISAL_DEF_MAX_LEVEL) {
        report_fail_msg("level knob out of range: %ld (valid 0..%d, default 1; 0=static-Huffman no match search, 1=hash chain, 2=deep hash history, 3=hash map + long match)", (long)lvl, ISAL_DEF_MAX_LEVEL);
    }
    d->level = (int)lvl;
    d->input       = (uint8_t *)malloc(IN_SIZE);
    d->comp_golden = (uint8_t *)malloc(OUT_SIZE);
    uint32_t lb_size = level_buf_size(d->level);
    d->level_buf  = lb_size ? (uint8_t *)malloc(lb_size) : nullptr;
    if (!d->input || !d->comp_golden || (lb_size && !d->level_buf)) {
        report_fail_msg("OOM in isal_igzip init");
    }
    /* High-entropy random input PLUS compressible structure: alternate
     * random blocks with short-period pattern blocks so the match finder
     * does real work (pure random is incompressible and skips the match
     * logic). Framework RNG only — libc rand is trapped by the framework.
     * The mix works for every level: 2/3's deeper match finders do MORE
     * work on the pattern blocks, which is the point. */
    for (size_t i = 0; i < IN_SIZE; ++i) {
        if ((i & 0x3FFF) < 0x2000) {
            d->input[i] = (uint8_t)(i & 0x0F);      /* short-period pattern */
        } else {
            d->input[i] = (uint8_t)random32();      /* random */
        }
    }
    /* golden compression, once, at the knob's level. Levels 2/3 REQUIRE a
     * level_buf on the stateless API; level 1 may omit it (the library
     * then uses the stream's built-in hash array) and level 0 never
     * searches for matches. Default (no knob) = level 1 without a
     * level_buf — byte-identical to the historical behavior. */
    struct isal_zstream stream;
    isal_deflate_stateless_init(&stream);
    stream.end_of_stream = 1;
    stream.flush = NO_FLUSH;
    stream.next_in = d->input;
    stream.avail_in = IN_SIZE;
    stream.next_out = d->comp_golden;
    stream.avail_out = OUT_SIZE;
    stream.level = d->level;
    if (lb_size) {
        stream.level_buf = d->level_buf;
        stream.level_buf_size = lb_size;
    }
    if (isal_deflate_stateless(&stream) != COMP_OK) {
        report_fail_msg("golden isal_deflate_stateless failed (level %d)", d->level);
    }
    d->golden_comp_size = stream.total_out;
    return EXIT_SUCCESS;
}

static int isal_igzip_run(struct test *test, int cpu)
{
    (void)cpu;
    auto d = CAST(test->data);
    uint32_t lb_size = level_buf_size(d->level);
    long iter = 0;
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch buffers */
        if (__builtin_expect(!comp || !decomp || (lb_size && !level_buf), 0)) {
            comp    = (uint8_t *)malloc(OUT_SIZE);
            decomp  = (uint8_t *)malloc(IN_SIZE);
            if (lb_size && !level_buf)
                level_buf = (uint8_t *)malloc(lb_size);
            if (!comp || !decomp || (lb_size && !level_buf))
                report_fail_msg("OOM allocating thread scratch (%zu bytes)",
                                (size_t)OUT_SIZE + IN_SIZE + lb_size);
        }
        ++iter;

        /* compress the same input again: must reproduce the identical
         * stream (compressor data-path determinism — the first SDC check) */
        struct isal_zstream stream;
        isal_deflate_stateless_init(&stream);
        stream.end_of_stream = 1;
        stream.flush = NO_FLUSH;
        stream.next_in = d->input;
        stream.avail_in = IN_SIZE;
        stream.next_out = comp;
        stream.avail_out = OUT_SIZE;
        stream.level = d->level;
        if (lb_size) {
            stream.level_buf = level_buf;
            stream.level_buf_size = lb_size;
        }
        if (isal_deflate_stateless(&stream) != COMP_OK) {
            report_fail_msg("isal_deflate_stateless failed (iteration %ld)", iter);
        }
        if (stream.total_out != d->golden_comp_size) {
            report_fail_msg("compressed size %u != golden %u (iteration %ld)",
                            stream.total_out, d->golden_comp_size, iter);
        }
        memcmp_or_fail(comp, d->comp_golden, d->golden_comp_size);

        /* decompress round-trip: must reproduce the input exactly (the
         * second SDC check — inflate + match reconstruction). The inflate
         * side is level-independent: deflate is deflate. */
        struct inflate_state inf;
        isal_inflate_init(&inf);
        inf.next_in = comp;
        inf.avail_in = d->golden_comp_size;
        inf.next_out = decomp;
        inf.avail_out = IN_SIZE;
        if (isal_inflate(&inf) != ISAL_DECOMP_OK) {
            report_fail_msg("isal_inflate failed (iteration %ld)", iter);
        }
        if (inf.total_out != IN_SIZE) {
            report_fail_msg("decompressed size %u != %d (iteration %ld)",
                            inf.total_out, IN_SIZE, iter);
        }
        memcmp_or_fail(decomp, d->input, IN_SIZE);
    }
    return EXIT_SUCCESS;
}

static int isal_igzip_cleanup(struct test *test)
{
    auto d = CAST(test->data);
    free(d->input); free(d->comp_golden); free(d->level_buf);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(isal_igzip, "isa-l igzip deflate/inflate round-trip SDC stress (level knob -O isal_igzip.level=0..3, default 1: four match-finder data-structure phases — static-Huffman / hash chain / deep hash history / hash map + long match)")
  .groups = DECLARE_TEST_GROUPS(&group_compression),
  .test_init = isal_igzip_init,
  .test_run = isal_igzip_run,
  .test_cleanup = isal_igzip_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
