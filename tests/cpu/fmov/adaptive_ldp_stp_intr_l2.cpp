/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b adaptive_ldp_stp_intr_l2
 * @parblock
 * L2-residency control twin of adaptive_ldp_stp_intr (2026-11-25, user
 * directive: capacity-skip-L1 / L2-resident control group).
 *
 * Same engine chassis as adaptive_ldp_stp_intr — 27-combo priority
 * sweep (loads 2-4 chained > stores 2-4 temp-to-dst-last > consumer
 * ALU rotation / EXT #8 / pure copy), zero asm (vld1q/vst1q with
 * compiler-chosen encodings, -fno-tree-loop-distribute-patterns),
 * COPY combos keep extra sources live via the checked xor
 * accumulator, shm wall-clock anchor under this engine's own name
 * prefix — with the ONE difference being the buffer SIZE:
 * ALDL2_COUNT 8192 = 128KB per buffer (vs 16KB).
 *
 * Residency math on the reference Kunpeng 920 (L1D 64KB/core, L2
 * 512KB/core): every combo capacity-bypasses L1 (per-buffer 128KB =
 * 2x L1D), and residency then rides the load/store-count axis as a
 * GRADIENT, recorded honestly:
 *
 *   l2s2 hot set = 4 x 128KB = 512KB -> exactly AT the private L2
 *                                      capacity (clean L2 with
 *                                      boundary pressure)
 *   l3s3         = 6 x 128KB = 768KB -> spills toward L3
 *   l4s4         = 8 x 128KB = 1MB   -> firmly L3-serviced
 *
 * The residency axis and the count axis are deliberately entangled in
 * this variant — holding every combo in clean L2 would require
 * <=64KB/buffer, giving up the per-buffer L1 capacity-skip.  Campaign
 * per-combo verdicts read the two axes together.
 *
 * Golden: PURE SCALAR (issue #373 paradigm, check-scalar-golden.sh
 * gated on aldl2_scalar_golden): the same word-level fold as the
 * intr engine's pair-form branch, plus (COPY only) the accumulator
 * golden (acc lane 0 = xor of every EXTRA source's even-index words,
 * lane 1 = odd-index words).
 *
 * Env contract (own prefix, mirrors the intr engine):
 *   ADAPTIVE_LDP_STP_INTR_L2_{KEY, VERIFY, COVFILE, COMBO, MODE,
 *   TEST_SLICE}; shm names "adaptive_ldp_stp_intr_l2_*" — all engine
 * prefixes (asm / _intr / _intr_l2) are mutually non-matching under
 * strncmp, so the engines coexist in one suite.
 * @endparblock
 */

#include "sandstone.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#if defined(__aarch64__)
#include <arm_neon.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif

#define ALDL2_COMBO_COUNT 27
#define ALDL2_SLICE_SECONDS 1800            /* 30min combat slice */
#define ALDL2_COUNT 8192                    /* elements per buffer -> 128KB (L2 residency control) */
#define ALDL2_BYTES (ALDL2_COUNT * 16)      /* 16KB per buffer */

enum { ALDL2_C_ALUROT = 0, ALDL2_C_EXT = 1, ALDL2_C_COPY = 2 };

static inline void aldl2_combo_unpack(int id, int *loads, int *stores, int *consumer)
{
    *consumer = id % 3; id /= 3;
    *stores = id % 3 + 2; id /= 3;
    *loads = id + 2;
}

#if defined(__aarch64__)

/* ------------------------------------------------------------------ */
/* shm machinery (mirrors the asm engine; own name prefix)              */
/* ------------------------------------------------------------------ */

struct AdaptiveLdpStpIntrL2Shm {
    struct timespec anchor;               /* wall-clock anchor */
    uint32_t ready;                       /* creator publish flag */
    uint32_t verify_ctr;                  /* verify-mode round-robin */
    uint32_t sdc_flag;                    /* set by any failing child */
};

/* Stale-object reaper: the default ppid-derived name outlives every
 * child by design (cross-child anchor); each child reaps objects whose
 * parent process is demonstrably dead.  KEY'd objects belong to the
 * campaign script.  See the asm twin for the full rationale. */
static void aldl2_shm_reap_stale(void)
{
    if (getenv("ADAPTIVE_LDP_STP_INTR_L2_KEY") != nullptr)
        return;
    DIR *dir = opendir("/dev/shm");
    if (dir == nullptr)
        return;
    static const char prefix[] = "adaptive_ldp_stp_intr_l2_ppid";
    const size_t plen = sizeof(prefix) - 1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, prefix, plen) != 0)
            continue;
        char *end = nullptr;
        const long pid = strtol(ent->d_name + plen, &end, 10);
        if (end == ent->d_name + plen || *end != '\0' || pid <= 1)
            continue;
        if (kill((pid_t)pid, 0) == 0 || errno != ESRCH)
            continue;               /* alive, or not provably dead: skip */
        char path[9 + 256 + 1];     /* "/dev/shm/" + NAME_MAX + NUL */
        snprintf(path, sizeof(path), "/dev/shm/%s", ent->d_name);
        if (unlink(path) == 0)
            fprintf(stderr, "adaptive_ldp_stp_intr_l2: reaped stale shm object %s "
                    "(parent process %ld is gone)\n", ent->d_name, pid);
    }
    closedir(dir);
}

static struct AdaptiveLdpStpIntrL2Shm *aldl2_shm_setup(void)
{
    aldl2_shm_reap_stale();

    const char *key = getenv("ADAPTIVE_LDP_STP_INTR_L2_KEY");
    char name[144];
    if (key) {
        snprintf(name, sizeof(name), "/adaptive_ldp_stp_intr_l2_%s", key);
    } else {
        snprintf(name, sizeof(name), "/adaptive_ldp_stp_intr_l2_ppid%ld", (long)getppid());
    }

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    bool creator = (fd >= 0);
    if (!creator) {
        if (errno != EEXIST)
            return nullptr;
        fd = shm_open(name, O_RDWR, 0600);
        if (fd < 0)
            return nullptr;
    }

    if (creator && ftruncate(fd, sizeof(struct AdaptiveLdpStpIntrL2Shm)) != 0) {
        close(fd);
        return nullptr;
    }

    auto *shm = static_cast<struct AdaptiveLdpStpIntrL2Shm *>(
            mmap(nullptr, sizeof(struct AdaptiveLdpStpIntrL2Shm), PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0));
    close(fd);
    if (shm == MAP_FAILED)
        return nullptr;

    if (creator) {
        memset(shm, 0, sizeof(*shm));
        clock_gettime(CLOCK_MONOTONIC, &shm->anchor);
        __atomic_store_n(&shm->ready, 1u, __ATOMIC_RELEASE);
    } else {
        for (int i = 0; i < 200000; i++) {
            if (__atomic_load_n(&shm->ready, __ATOMIC_ACQUIRE) == 1u)
                break;
            usleep(10);
        }
    }
    return shm;
}

static int64_t aldl2_seconds_since(const struct timespec *anchor, struct timespec *now)
{
    clock_gettime(CLOCK_MONOTONIC, now);
    int64_t s = (int64_t)(now->tv_sec - anchor->tv_sec);
    if (now->tv_nsec < anchor->tv_nsec)
        s -= 1;
    return s < 0 ? 0 : s;
}

static int aldl2_pick_combo(struct AdaptiveLdpStpIntrL2Shm *shm, int test_mode, int test_slice)
{
    if (!shm)
        return -1;
    if (!test_mode &&
        __atomic_load_n(&shm->sdc_flag, __ATOMIC_ACQUIRE) != 0)
        return -1;
    if (__atomic_load_n(&shm->ready, __ATOMIC_ACQUIRE) != 1u)
        return -1;

    const char *force = getenv("ADAPTIVE_LDP_STP_INTR_L2_COMBO");
    if (force != nullptr) {
        int c = atoi(force);
        if (c >= 0 && c < ALDL2_COMBO_COUNT)
            return c;
    }

    if (getenv("ADAPTIVE_LDP_STP_INTR_L2_VERIFY") != nullptr) {
        uint32_t prev = __atomic_fetch_add(&shm->verify_ctr, 1u, __ATOMIC_RELAXED);
        return (int)(prev % ALDL2_COMBO_COUNT);
    }

    struct timespec now;
    if (test_mode) {
        int64_t elapsed = aldl2_seconds_since(&shm->anchor, &now);
        int64_t idx = elapsed / test_slice;
        if (idx >= ALDL2_COMBO_COUNT)
            return -1;
        return (int)idx;
    }

    int64_t elapsed = aldl2_seconds_since(&shm->anchor, &now);
    int64_t idx = elapsed / ALDL2_SLICE_SECONDS;
    if (idx > ALDL2_COMBO_COUNT - 1)
        idx = ALDL2_COMBO_COUNT - 1;
    return (int)idx;
}

static void aldl2_covfile_trace(int test_mode, int combo)
{
    const char *path = getenv("ADAPTIVE_LDP_STP_INTR_L2_COVFILE");
    if (!path)
        return;
    char line[160];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    char iso[32];
    strftime(iso, sizeof(iso), "%FT%T", &tm);
    snprintf(line, sizeof(line), "%s %d %s %d\n", iso, (int)getpid(),
             getenv("ADAPTIVE_LDP_STP_INTR_L2_VERIFY") ? "verify"
                 : test_mode ? "test" : "normal",
             combo);
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0)
        return;
    (void)!write(fd, line, strlen(line));
    close(fd);
}

static void aldl2_combo_tag(int id, char *buf, size_t n)
{
    int loads, stores, consumer;
    aldl2_combo_unpack(id, &loads, &stores, &consumer);
    snprintf(buf, n, "combo#%d l%d s%d %s", id, loads, stores,
             consumer == ALDL2_C_ALUROT ? "alurot"
                 : consumer == ALDL2_C_EXT ? "ext" : "copy");
}

/* ------------------------------------------------------------------ */
/* scalar golden (issue #373 paradigm — no vector instruction here;    */
/* noinline: the gate disassembles THIS symbol; ald_rot64-style call   */
/* barriers against constprop-clone auto-vectorization, the asm arm  */
/* hit that on 2026-11-24)                                             */
/* ------------------------------------------------------------------ */

__attribute__((noinline)) static uint64_t aldl2_rot64(int slot, uint64_t a, uint64_t b)
{
    switch (slot) {
        case 0:  return a + b;
        case 1:  return a ^ b;
        case 2:  return a & b;
        default: return a | b;
    }
}

/* expected: the pair-form fold (same derivation as the asm engine).
 * acc_golden (COPY only): lane 0 = xor of every EXTRA source's
 * even-index words, lane 1 = odd-index words; zeros otherwise. */
__attribute__((noinline)) static void aldl2_scalar_golden(int L, int consumer,
                                                         uint64_t *const *src,
                                                         uint64_t *expected,
                                                         uint64_t *acc_golden)
{
    acc_golden[0] = 0;
    acc_golden[1] = 0;

    for (size_t j = 0; j < ALDL2_COUNT / 2; ++j) {
        const size_t w = j * 4;
        uint64_t e0 = src[0][w + 0], e1 = src[0][w + 1];
        uint64_t o0 = src[0][w + 2], o1 = src[0][w + 3];
        if (consumer == ALDL2_C_ALUROT) {
            const int s1 = (int)(j % 4);
            const int s2 = (int)((j + 2) % 4);
            for (int k = 1; k < L; ++k) {
                e0 = aldl2_rot64(s1, e0, src[k][w + 0]);
                e1 = aldl2_rot64(s1, e1, src[k][w + 1]);
                o0 = aldl2_rot64(s2, o0, src[k][w + 2]);
                o1 = aldl2_rot64(s2, o1, src[k][w + 3]);
            }
        } else if (consumer == ALDL2_C_EXT) {
            for (int k = 1; k < L; ++k) {
                const uint64_t ne0 = e1, no0 = o1;
                e1 = src[k][w + 0]; o1 = src[k][w + 2];
                e0 = ne0;           o0 = no0;
            }
        } else {                        /* ALDL2_C_COPY */
            for (int k = 1; k < L; ++k) {
                acc_golden[0] ^= src[k][w + 0] ^ src[k][w + 2];
                acc_golden[1] ^= src[k][w + 1] ^ src[k][w + 3];
            }
        }
        expected[w + 0] = e0;
        expected[w + 1] = e1;
        expected[w + 2] = o0;
        expected[w + 3] = o1;
    }
}

/* ------------------------------------------------------------------ */
/* combo bodies: compile-time instantiation over <L, S, C>              */
/* ------------------------------------------------------------------ */

/* hot-path consumer: the rotating integer ALU (family chassis) */
static inline uint64x2_t aldl2_rot4(int slot, uint64x2_t a, uint64x2_t b)
{
    switch (slot) {
        case 0:  return vaddq_u64(a, b);
        case 1:  return veorq_u64(a, b);
        case 2:  return vandq_u64(a, b);
        default: return vorrq_u64(a, b);
    }
}

struct Aldl2Ctx {
    struct test *test;
    int cpu;
    struct AdaptiveLdpStpIntrL2Shm *shm;
    int combo;
};

template <int L, int S, int C>
static int aldl2_combo_body(struct Aldl2Ctx *ctx)
{
    /* TEST_LOOP's first macro parameter is a positional placeholder the
     * expansion does not reference — keep the binding warning-free */
    [[maybe_unused]] auto *test = ctx->test;

    uint64_t *srcbuf[L];
    uint64_t *tgtbuf[S];
    for (int k = 0; k < L; ++k) {
        srcbuf[k] = static_cast<uint64_t *>(aligned_alloc(32, ALDL2_BYTES));
        if (!srcbuf[k]) {
            for (int m = 0; m < k; ++m) free(srcbuf[m]);
            report_fail_msg("adaptive_ldp_stp_intr_l2: source buffer allocation failed");
            return EXIT_FAILURE;
        }
        memset_random(srcbuf[k], ALDL2_BYTES);
    }
    for (int m = 0; m < S; ++m) {
        tgtbuf[m] = static_cast<uint64_t *>(aligned_alloc(32, ALDL2_BYTES));
        if (!tgtbuf[m]) {
            for (int q = 0; q < m; ++q) free(tgtbuf[q]);
            for (int k = 0; k < L; ++k) free(srcbuf[k]);
            report_fail_msg("adaptive_ldp_stp_intr_l2: target buffer allocation failed");
            return EXIT_FAILURE;
        }
    }
    auto *expected = static_cast<uint64_t *>(aligned_alloc(32, ALDL2_BYTES));
    uint64_t acc_golden[2];
    if (!expected) {
        for (int k = 0; k < L; ++k) free(srcbuf[k]);
        for (int m = 0; m < S; ++m) free(tgtbuf[m]);
        report_fail_msg("adaptive_ldp_stp_intr_l2: expected buffer allocation failed");
        return EXIT_FAILURE;
    }

    aldl2_scalar_golden(L, C, srcbuf, expected, acc_golden);

    char tag[48];
    aldl2_combo_tag(ctx->combo, tag, sizeof(tag));

    TEST_LOOP(test, 1 << 12) {
        const uint64_t *p[L];
        uint64_t *t[S];
        for (int k = 0; k < L; ++k) p[k] = srcbuf[k];
        for (int m = 0; m < S; ++m) t[m] = tgtbuf[m];
        uint64x2_t acc = vdupq_n_u64(0);

        for (size_t j = 0; j < ALDL2_COUNT / 2; ++j) {
            const size_t w = j * 4;
            const uint64x2_t v0e = vld1q_u64(&p[0][w + 0]);
            const uint64x2_t v0o = vld1q_u64(&p[0][w + 2]);
            [[maybe_unused]] uint64x2_t v1e, v1o, v2e, v2o, v3e, v3o;
            if constexpr (L >= 2) {
                v1e = vld1q_u64(&p[1][w + 0]);
                v1o = vld1q_u64(&p[1][w + 2]);
            }
            if constexpr (L >= 3) {
                v2e = vld1q_u64(&p[2][w + 0]);
                v2o = vld1q_u64(&p[2][w + 2]);
            }
            if constexpr (L >= 4) {
                v3e = vld1q_u64(&p[3][w + 0]);
                v3o = vld1q_u64(&p[3][w + 2]);
            }

            uint64x2_t r0, r1;
            if constexpr (C == ALDL2_C_COPY) {
                /* first source passes through; the extra sources stay
                 * live via the checked xor accumulator (DCE defense —
                 * see header) */
                r0 = v0e; r1 = v0o;
                if constexpr (L >= 2) { acc = veorq_u64(acc, v1e); acc = veorq_u64(acc, v1o); }
                if constexpr (L >= 3) { acc = veorq_u64(acc, v2e); acc = veorq_u64(acc, v2o); }
                if constexpr (L >= 4) { acc = veorq_u64(acc, v3e); acc = veorq_u64(acc, v3o); }
            } else if constexpr (C == ALDL2_C_ALUROT) {
                r0 = v0e; r1 = v0o;
                const int s1 = (int)(j % 4);
                const int s2 = (int)((j + 2) % 4);
                if constexpr (L >= 2) { r0 = aldl2_rot4(s1, r0, v1e); r1 = aldl2_rot4(s2, r1, v1o); }
                if constexpr (L >= 3) { r0 = aldl2_rot4(s1, r0, v2e); r1 = aldl2_rot4(s2, r1, v2o); }
                if constexpr (L >= 4) { r0 = aldl2_rot4(s1, r0, v3e); r1 = aldl2_rot4(s2, r1, v3o); }
            } else {                        /* ALDL2_C_EXT */
                r0 = v0e; r1 = v0o;
                if constexpr (L >= 2) { r0 = vextq_u64(r0, v1e, 1); r1 = vextq_u64(r1, v1o, 1); }
                if constexpr (L >= 3) { r0 = vextq_u64(r0, v2e, 1); r1 = vextq_u64(r1, v2o, 1); }
                if constexpr (L >= 4) { r0 = vextq_u64(r0, v3e, 1); r1 = vextq_u64(r1, v3o, 1); }
            }

            /* stores: every target gets the result pair, dst last */
            if constexpr (S >= 1) { vst1q_u64(&t[0][w + 0], r0); vst1q_u64(&t[0][w + 2], r1); }
            if constexpr (S >= 2) { vst1q_u64(&t[1][w + 0], r0); vst1q_u64(&t[1][w + 2], r1); }
            if constexpr (S >= 3) { vst1q_u64(&t[2][w + 0], r0); vst1q_u64(&t[2][w + 2], r1); }
            if constexpr (S >= 4) { vst1q_u64(&t[3][w + 0], r0); vst1q_u64(&t[3][w + 2], r1); }
        }

        /* cold path: the LAST target is the compared dst; COPY combos
         * additionally cold-check the accumulator against its golden */
        const uint64_t acc0 = vgetq_lane_u64(acc, 0);
        const uint64_t acc1 = vgetq_lane_u64(acc, 1);
        if (memcmp(tgtbuf[S - 1], expected, ALDL2_BYTES) != 0 ||
            (C == ALDL2_C_COPY && (acc0 != acc_golden[0] || acc1 != acc_golden[1]))) {
            if (C == ALDL2_C_COPY && (acc0 != acc_golden[0] || acc1 != acc_golden[1])) {
                report_fail_msg("adaptive_ldp_stp_intr_l2: %s accumulator mismatch: "
                                "expected {%016lx,%016lx} got {%016lx,%016lx} "
                                "(copy-consumer extra-source load train)",
                                tag, (unsigned long)acc_golden[0],
                                (unsigned long)acc_golden[1],
                                (unsigned long)acc0, (unsigned long)acc1);
                if (ctx->shm)
                    __atomic_store_n(&ctx->shm->sdc_flag, 1u, __ATOMIC_RELEASE);
            }
            for (size_t i = 0; i < ALDL2_COUNT * 2; ++i) {
                if (tgtbuf[S - 1][i] != expected[i]) {
                    report_fail_msg("adaptive_ldp_stp_intr_l2: %s data miscompare "
                                    "at u64 %zu: expected 0x%016lx "
                                    "got 0x%016lx (compiler-built, no asm)",
                                    tag, i, (unsigned long)expected[i],
                                    (unsigned long)tgtbuf[S - 1][i]);
                    if (ctx->shm)
                        __atomic_store_n(&ctx->shm->sdc_flag, 1u, __ATOMIC_RELEASE);
                    for (int k = 0; k < L; ++k) free(srcbuf[k]);
                    for (int m = 0; m < S; ++m) free(tgtbuf[m]);
                    free(expected);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    for (int k = 0; k < L; ++k) free(srcbuf[k]);
    for (int m = 0; m < S; ++m) free(tgtbuf[m]);
    free(expected);
    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* framework plumbing                                                  */
/* ------------------------------------------------------------------ */

/* the 27-entry dispatch table: id order == lexicographic priority
 * (loads > stores > consumer); kind axis exists only in the asm arm */
#define ALDL2_BODY(L, S) \
    &aldl2_combo_body<L, S, ALDL2_C_ALUROT>, \
    &aldl2_combo_body<L, S, ALDL2_C_EXT>,    \
    &aldl2_combo_body<L, S, ALDL2_C_COPY>
#define ALDL2_ROW(L) ALDL2_BODY(L, 2), ALDL2_BODY(L, 3), ALDL2_BODY(L, 4)

static int (*const kAldiRun[ALDL2_COMBO_COUNT])(struct Aldl2Ctx *) = {
    ALDL2_ROW(2),
    ALDL2_ROW(3),
    ALDL2_ROW(4),
};

struct Aldl2Data {
    struct AdaptiveLdpStpIntrL2Shm *shm;
    int test_mode;
    int test_slice;
};

static int adaptive_ldp_stp_intr_l2_init(struct test *test)
{
    auto *data = static_cast<struct Aldl2Data *>(malloc(sizeof(struct Aldl2Data)));
    data->shm = aldl2_shm_setup();
    if (data->shm == nullptr)
        fprintf(stderr, "adaptive_ldp_stp_intr_l2: shm setup failed — this child "
                "will idle (combo sweep degraded)\n");
    data->test_mode = 0;
    data->test_slice = 600;
    const char *mode = getenv("ADAPTIVE_LDP_STP_INTR_L2_MODE");
    if (mode != nullptr) {
        if (strcmp(mode, "test") == 0)
            data->test_mode = 1;
        else if (strcmp(mode, "combat") != 0)
            fprintf(stderr, "adaptive_ldp_stp_intr_l2: ADAPTIVE_LDP_STP_INTR_L2_MODE must be combat|test, ignoring '%s'\n", mode);
    }
    const char *ts = getenv("ADAPTIVE_LDP_STP_INTR_L2_TEST_SLICE");
    if (ts != nullptr) {
        int v = atoi(ts);
        if (v >= 1)
            data->test_slice = v;
        else
            fprintf(stderr, "adaptive_ldp_stp_intr_l2: ADAPTIVE_LDP_STP_INTR_L2_TEST_SLICE must be >=1 second, keeping default 600\n");
    }
    test->data = data;
    return EXIT_SUCCESS;
}

static int adaptive_ldp_stp_intr_l2_run(struct test *test, int cpu)
{
    auto *data = static_cast<struct Aldl2Data *>(test->data);
    int combo = aldl2_pick_combo(data->shm, data->test_mode, data->test_slice);
    aldl2_covfile_trace(data->test_mode, combo);

    if (combo < 0)
        return EXIT_SUCCESS;             /* post-SDC or survey done or shm unusable: idle */

    struct Aldl2Ctx ctx = { test, cpu, data->shm, combo };
    return kAldiRun[combo](&ctx);
}

static int adaptive_ldp_stp_intr_l2_cleanup(struct test *test)
{
    auto *data = static_cast<struct Aldl2Data *>(test->data);
    /* the shm MAPPING dies with the child; the named object outlives
     * children by design — ppid-derived objects reaped by later runs,
     * KEY'd objects belong to the campaign script (see asm twin) */
    free(data);
    test->data = nullptr;
    return EXIT_SUCCESS;
}

#else  /* !__aarch64__ */

static int adaptive_ldp_stp_intr_l2_init(struct test *test)  { (void)test; return EXIT_SUCCESS; }
static int adaptive_ldp_stp_intr_l2_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SUCCESS; }
static int adaptive_ldp_stp_intr_l2_cleanup(struct test *test) { (void)test; return EXIT_SUCCESS; }

#endif

DECLARE_TEST(adaptive_ldp_stp_intr_l2, "L2-residency control twin of adaptive_ldp_stp_intr: same 27-combo zero-asm engine (loads 2-4 > stores 2-4 > ALU/EXT/copy, checked xor accumulator for COPY extras) with 128KB per buffer — every combo capacity-bypasses L1 (2x L1D per buffer), hot set rides the count axis as a gradient: 512KB (l2s2, at L2) to 1MB (l4s4, L3-serviced); own shm/env prefix (ARM64)")
    .test_init    = adaptive_ldp_stp_intr_l2_init,
    .test_run     = adaptive_ldp_stp_intr_l2_run,
    .test_cleanup = adaptive_ldp_stp_intr_l2_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
