/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movbe_dump_probe_e
 * @parblock
 * SDC-trigger probe, variant E: **swapped buffer on a different NUMA/LLC
 * domain than input** (LLC cross-line interaction isolated from the
 * address-advance-stream).
 *
 * Baseline movbe_dump hot loop:
 *   ldr input[i] -> rev -> str swapped[i] -> ldr input[i](reload) -> cmp
 * The store writes to swapped[i] with a linearly-varying address (advances
 * across cache lines). Probe D proved that a *fixed*-address store does NOT
 * trigger (PASS), so the store's advancing address stream is a trigger
 * condition. The open question: is the trigger the advancing *address stream
 * per se, or a *same-LLC-domain interaction* between the swapped writes and
 * the input reloads (both in core 179's local LLC, node 7)?
 *
 * Probe E keeps the baseline structure byte-for-byte (same store to
 * swapped[i], same advancing address stream, same reload of input[i]) but
 * forces swapped and input onto DIFFERENT NUMA nodes / LLC domains:
 *   - input   -> mbind-bound to PROBE_E_INPUT_NODE  (default 7, core 179's home)
 *   - swapped -> mbind-bound to PROBE_E_SWAP_NODE   (default 0, different LLC)
 * So core 179's swapped writes go cross-NUMA (never enter the local LLC for
 * swapped), while input stays local. If probe E STILL triggers, the
 * advancing address stream alone is sufficient (LLC-domain interaction is
 * NOT required). If probe E PASSES, the swapped writes must stay in the
 * same LLC domain as input -- i.e. an LLC cross-line interaction is part of
 * the trigger.
 *
 * Allocation uses mmap + mbind(MPOL_BIND) directly via syscall, so it does
 * not depend on libnuma.
 *
 * NUMA-FALLBACK (fixed): the original probe hardcoded input on node 7 and
 * swapped on node 0, assuming core 179's home node was 7. That is wrong on
 * machines whose topology differs (e.g. this ARM64 host has only nodes 0-3,
 * and memory only on node 1), so mbind() returned EINVAL and the test failed
 * in test_init before the hot loop ever ran -- a test-environment mismatch,
 * not an SDC. The probe now resolves the two nodes at runtime: it scans
 * /sys/devices/system/node/has_memory for nodes that actually have memory,
 * picks two distinct such nodes for the input/swap domains (falling back to
 * a single node, then to plain unbound mmap if fewer than one is available),
 * and only calls mbind() for nodes that exist. Compile-time overrides via
 * PROBE_E_INPUT_NODE / PROBE_E_SWAP_NODE are still honored when they name
 * valid nodes.
 * @endparblock
 */

#include "sandstone.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

#define MOVBE_BUFFER_SIZE (1 << 14)

// Compile-time overrides still honored if they name real memory nodes.
// Defaults (-1) mean "resolve at runtime" (see node_with_memory()).
#ifndef PROBE_E_INPUT_NODE
#  define PROBE_E_INPUT_NODE (-1)
#endif
#ifndef PROBE_E_SWAP_NODE
#  define PROBE_E_SWAP_NODE (-1)
#endif

/* issue #374: identity LUT for the scalar-independent byte-swap golden.
 * The table loads block GCC's bswap-idiom recognizer from folding the
 * shift/or reassembly back into a REV instruction -- the exact unit under
 * test -- so a persistent REV mispermutation can never corrupt golden and
 * actual identically. Pinned by scripts/check-bswap-golden.sh (zero
 * rev/rev16/rev32 and zero vector-ALU inside the init symbol). */
static const uint8_t g_bswap_lut[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
static uint32_t movbe_golden_bswap32(uint32_t x)
{
    return ((uint32_t)g_bswap_lut[x & 0xff] << 24) |
           ((uint32_t)g_bswap_lut[(x >> 8) & 0xff] << 16) |
           ((uint32_t)g_bswap_lut[(x >> 16) & 0xff] << 8) |
           (uint32_t)g_bswap_lut[x >> 24];
}

struct movbe_data {
    uint32_t *input;
    uint32_t *swapped;
    uint32_t *expected_swapped;
};

// mmap a page-aligned region of `bytes` and bind it to NUMA `node` via
// mbind(MPOL_BIND). Returns MAP_FAILED only on mmap failure; mbind failure
// is logged but tolerated (the region stays unbound) so the test can still
// run its hot loop instead of failing in init. We round up to pages.
static void *mmap_on_node(size_t bytes, int node)
{
    long pagesize = sysconf(_SC_PAGESIZE);
    size_t total = (bytes + pagesize - 1) & ~(size_t)(pagesize - 1);
    // Ensure total is at least one page and a multiple of pagesize.
    if (total == 0) total = pagesize;

    void *p = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return p;

    // Build a nodemask with just `node`. Only attempt the bind when the node
    // is a real non-negative index that fits in a single unsigned long mask.
    if (node >= 0 && node < (int)(sizeof(unsigned long) * 8)) {
        unsigned long nodemask = (1UL << node);
        // mbind(addr, len, MPOL_BIND, nodemask, maxnode, flags=0)
        // maxnode = number of bits in nodemask (use 64).
        long r = syscall(__NR_mbind, p, total, 2 /*MPOL_BIND*/,
                         &nodemask, sizeof(unsigned long) * 8, 0);
        if (r != 0) {
            // mbind failed (e.g. node has no memory / does not exist on this
            // host): do NOT abort the whole test. Log and leave the region
            // unbound; the hot loop is still a valid byte-swap round-trip
            // probe, just without the cross-NUMA placement guarantee.
            log_info("MovBE-probeE: mbind to node %d failed (errno=%d); "
                     "continuing unbound", node, errno);
        }
    }
    // Touch every page so the binding takes effect (first-touch).
    memset(p, 0, total);
    return p;
}

// Return the number of NUMA nodes that carry memory, by parsing
// /sys/devices/system/node/has_memory (a cpuset-style list like "1" or
// "0,2-3"). Up to `out_nodes_len` node ids are written to out_nodes in
// ascending order; returns the count filled. On any parse error returns 0.
static int nodes_with_memory(int *out_nodes, int out_nodes_len)
{
    FILE *f = fopen("/sys/devices/system/node/has_memory", "r");
    if (!f) return 0;
    char buf[256];
    int count = 0;
    if (fgets(buf, sizeof(buf), f)) {
        // Parse a cpuset list: comma-separated ranges/terms, e.g. "1" or
        // "0,2-3". Trailing newline is ignored by strtoul.
        char *p = buf;
        while (*p && count < out_nodes_len) {
            // skip non-digit separators
            while (*p && *p != '-' && (*p < '0' || *p > '9')) p++;
            if (!*p) break;
            char *end;
            unsigned long a = strtoul(p, &end, 10);
            p = end;
            unsigned long b = a;
            if (*p == '-') {
                p++;
                b = strtoul(p, &end, 10);
                p = end;
            }
            for (unsigned long n = a; n <= b && count < out_nodes_len; ++n)
                out_nodes[count++] = (int)n;
        }
    }
    fclose(f);
    return count;
}

static int movbe_init(struct test *test)
{
    movbe_data *data = (movbe_data *)malloc(sizeof(movbe_data));

    // Resolve the two placement nodes at runtime. Compile-time overrides
    // (PROBE_E_INPUT_NODE / PROBE_E_SWAP_NODE) win if they name real memory
    // nodes; otherwise we pick two distinct memory-bearing nodes from the
    // host's /sys/devices/system/node/has_memory, falling back to the same
    // node (or none, i.e. unbound mmap) if the host has fewer than two.
    int memnodes[16];
    int nmem = nodes_with_memory(memnodes, 16);
    int input_node = PROBE_E_INPUT_NODE;
    int swap_node = PROBE_E_SWAP_NODE;

    auto node_is_memory = [&](int n) {
        for (int i = 0; i < nmem; ++i) if (memnodes[i] == n) return true;
        return false;
    };
    if (input_node < 0 || !node_is_memory(input_node))
        input_node = (nmem >= 1) ? memnodes[0] : -1;
    if (swap_node < 0 || !node_is_memory(swap_node) || swap_node == input_node)
        swap_node = (nmem >= 2) ? memnodes[1] : input_node;

    data->input = (uint32_t *)mmap_on_node(MOVBE_BUFFER_SIZE * sizeof(uint32_t),
                                           input_node);
    data->swapped = (uint32_t *)mmap_on_node(MOVBE_BUFFER_SIZE * sizeof(uint32_t),
                                             swap_node);
    data->expected_swapped = (uint32_t *)aligned_alloc_safe(64, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
    if (data->input == MAP_FAILED || data->swapped == MAP_FAILED) {
        // mmap itself failed (not mbind -- that is tolerated inside
        // mmap_on_node). This is a genuine allocation failure; abort.
        log_error("MovBE-probeE: mmap failed (input=%p swapped=%p errno=%d)",
                  (void *)data->input, (void *)data->swapped, errno);
        /* issue #374 review: free everything allocated before the failure
         * check (expected_swapped + any mapping that did succeed). */
        if (data->input && data->input != MAP_FAILED)
            munmap(data->input, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        if (data->swapped && data->swapped != MAP_FAILED)
            munmap(data->swapped, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        free(data->expected_swapped);
        free(data);
        return EXIT_FAILURE;
    }
    log_info("MovBE-probeE: placement input_node=%d swap_node=%d "
             "(memnodes=%d, fallback used)",
             input_node, swap_node, nmem);

    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        data->input[i] = random32();
    }

    /* issue #374: independent golden for the swapped intermediate, computed
     * once here with pure scalar LUT lookups (never the REV under test).
     * The golden is plain local memory: its NUMA placement is irrelevant to
     * the input/swapped placement variable this probe isolates. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);

    test->data = data;
    return EXIT_SUCCESS;
}

static int movbe_run(struct test *test, int cpu)
{
    movbe_data *data = (movbe_data *)test->data;

    TEST_LOOP(test, 1 << 13) {
        // Hot loop kept byte-for-byte identical to baseline movbe_dump:
        // read input[i], rev, store swapped[i], reload input[i], compare.
        // Only the buffer placements differ (input node 7, swapped node 0).
        for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
            uint32_t val = data->input[i];
            val = __builtin_bswap32(val);

            data->swapped[i] = val;

            val = __builtin_bswap32(val);

            if (val != data->input[i]) {
                uint32_t inputv = data->input[i];
                uint32_t golden = data->input[i];
                uint32_t actual = val;
                uint32_t xorv = golden ^ actual;
                unsigned char *ib = (unsigned char *)&inputv;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                log_error("MovBE-probeE: Round-trip failed at index %u: "
                          "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X | "
                          "bytes[b3 b2 b1 b0] input=%02X%02X%02X%02X "
                          "golden=%02X%02X%02X%02X actual=%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X",
                          (unsigned)i, inputv, golden, actual, xorv,
                          ib[3], ib[2], ib[1], ib[0],
                          gb[3], gb[2], gb[1], gb[0],
                          ab[3], ab[2], ab[1], ab[0],
                          xb[3], xb[2], xb[1], xb[0]);
                report_fail_msg("MovBE: Round-trip failed at index %u", (unsigned)i);
            }
        }
    }

    /* issue #374: post-loop verification of the stored intermediate. The hot
     * loop above is unchanged; every iteration stores the same
     * bswap32(input[i]) to swapped[i], so the final content must equal the
     * init-time LUT golden. A persistent REV mispermutation (symmetric
     * fault: applied twice it cancels in the round-trip identity) is caught
     * here byte-exactly. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE-probeE: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      (unsigned)i, data->input[i], data->expected_swapped[i],
                      data->swapped[i], data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("MovBE: swapped buffer != independent golden at index %u",
                            (unsigned)i);
        }
    }

    return EXIT_SUCCESS;
}

static int movbe_cleanup(struct test *test)
{
    movbe_data *data = (movbe_data *)test->data;
    if (data) {
        if (data->input && data->input != MAP_FAILED)
            munmap(data->input, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        if (data->swapped && data->swapped != MAP_FAILED)
            munmap(data->swapped, MOVBE_BUFFER_SIZE * sizeof(uint32_t));
        free(data->expected_swapped);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movbe_dump_probe_e, "movbe_dump probe E: swapped on different NUMA/LLC than input (LLC cross-line interaction isolated)")
    .test_init = movbe_init,
    .test_run = movbe_run,
    .test_cleanup = movbe_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
