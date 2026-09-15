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
};
}

#define CAST(_x) static_cast<struct igzip_test_data *>(_x)

/* Scratch buffers are per-thread: the framework runs one worker thread per
 * core and they all share test->data, so anything written during test_run
 * must be thread-local (comp is the re-compression destination and decomp
 * the inflate destination; both are written every iteration). They are
 * allocated lazily on first use and intentionally not freed in cleanup,
 * exactly like the openblas_* scratch buffers: the worker threads are gone
 * (or never existed) when the main thread runs test_cleanup, and the whole
 * forked child exits right after, so the OS reclaims the storage. */
static thread_local uint8_t *comp = nullptr;
static thread_local uint8_t *decomp = nullptr;

static int isal_igzip_init(struct test *test)
{
    auto d = new(igzip_test_data);
    test->data = d;
    d->input       = (uint8_t *)malloc(IN_SIZE);
    d->comp_golden = (uint8_t *)malloc(OUT_SIZE);
    if (!d->input || !d->comp_golden) {
        report_fail_msg("OOM in isal_igzip init");
    }
    /* High-entropy random input PLUS compressible structure: alternate
     * random blocks with short-period pattern blocks so the match finder
     * does real work (pure random is incompressible and skips the match
     * logic). Framework RNG only — libc rand is trapped by the framework. */
    for (size_t i = 0; i < IN_SIZE; ++i) {
        if ((i & 0x3FFF) < 0x2000) {
            d->input[i] = (uint8_t)(i & 0x0F);      /* short-period pattern */
        } else {
            d->input[i] = (uint8_t)random32();      /* random */
        }
    }
    /* golden compression, once. Level 1 (the fastest level that still
     * runs the LZ77 match finder; level 0 is hash-free static-Huffman
     * only) — and for stateless level 1 no level_buf is required. */
    struct isal_zstream stream;
    isal_deflate_stateless_init(&stream);
    stream.end_of_stream = 1;
    stream.flush = NO_FLUSH;
    stream.next_in = d->input;
    stream.avail_in = IN_SIZE;
    stream.next_out = d->comp_golden;
    stream.avail_out = OUT_SIZE;
    stream.level = 1;
    if (isal_deflate_stateless(&stream) != COMP_OK) {
        report_fail_msg("golden isal_deflate_stateless failed");
    }
    d->golden_comp_size = stream.total_out;
    return EXIT_SUCCESS;
}

static int isal_igzip_run(struct test *test, int cpu)
{
    (void)cpu;
    auto d = CAST(test->data);
    long iter = 0;
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch buffers */
        if (__builtin_expect(!comp || !decomp, 0)) {
            comp    = (uint8_t *)malloc(OUT_SIZE);
            decomp  = (uint8_t *)malloc(IN_SIZE);
            if (!comp || !decomp)
                report_fail_msg("OOM allocating thread scratch (%zu bytes)",
                                (size_t)OUT_SIZE + IN_SIZE);
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
        stream.level = 1;
        if (isal_deflate_stateless(&stream) != COMP_OK) {
            report_fail_msg("isal_deflate_stateless failed (iteration %ld)", iter);
        }
        if (stream.total_out != d->golden_comp_size) {
            report_fail_msg("compressed size %u != golden %u (iteration %ld)",
                            stream.total_out, d->golden_comp_size, iter);
        }
        memcmp_or_fail(comp, d->comp_golden, d->golden_comp_size);

        /* decompress round-trip: must reproduce the input exactly (the
         * second SDC check — inflate + match reconstruction) */
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
    free(d->input); free(d->comp_golden);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(isal_igzip, "isa-l igzip deflate/inflate round-trip SDC stress")
  .groups = DECLARE_TEST_GROUPS(&group_compression),
  .test_init = isal_igzip_init,
  .test_run = isal_igzip_run,
  .test_cleanup = isal_igzip_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
