/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b db_bank_transfer_lse
 * @parblock
 * Bank-transfer conservation probe, LSE-atomic arm (2026-11-28).
 *
 * STRUCTURAL TWIN of db_bank_transfer_cas — same load environment (256
 * accounts, all cores doing concurrent transfers/deposits/withdrawals,
 * sustained dirty-line coherence traffic), same four accounting books
 * (conservation + per-account gen tallies + global gen-total
 * cross-check + per-thread ledgers), same cleanup reconciliation and
 * attribution. What differs is THE ATOMIC PATH, and that difference is
 * the pair's whole point:
 *
 *   cas arm : 16 B ldxp/stxp exclusive pair, both words tagged, per-op
 *             double-magic validation, exclusive-monitor retry loop.
 *   lse arm : single-word LDADDAL atomic adds — balance and gen as two
 *             INDEPENDENT atomics, no pair, no retry loop, no exclusive
 *             monitor. Different snoop micro-traffic (no exclusive
 *             mark/resolve handshake; LSE atomics modify in cache).
 *
 * A hit on one arm with the other silent names the atomic path — the
 * discriminative-pair discipline of this fleet.
 *
 * TAG ASYMMETRY (design, disclosed): the LSE balance word is UNTAGGED
 * raw uint64 — an atomic add cannot mask carries, and any tag in the
 * high bits would be corrupted by the first carry past bit 48.
 * Conservation is exact in mod-2^64 arithmetic regardless. The gen
 * word IS tagged (GEN_MAGIC<<48 | counter): it only ever takes +1
 * adds, so a carry into the magic bits needs 2^48 updates on one
 * account — unrealizable. In-run per-op validation therefore covers
 * the GEN path only (old-value magic check on every ldaddal return);
 * the per-op BALANCE read check is the cas twin's exclusive axis.
 * Both arms keep the full four-book cleanup reconciliation, where a
 * balance-word corruption still breaks conservation with gen intact
 * ("pure value corruption off the RMW path").
 *
 * PINNED ASM (minimal-asm directive): two ONE-LINE asm wrappers pin
 * ldaddal itself — without them the compiler may legally lower the
 * atomic to an ldxr/stxr loop or a libatomic call depending on build
 * flags, and this piece exists to exercise exactly the LSE hardware
 * path. objdump gate verifies ldaddal is what ships.
 *
 * HWCAP GATE: if the host does not expose HWCAP_ATOMICS the test
 * skips honestly from init ("to be implemented (placeholder)"-class
 * boundary: the instruction is UNAVAILABLE, not the test unfinished).
 * 920 (ARMv8.2) always passes this gate.
 *
 * SOFTWARE-RACE HARDENING: same as the cas twin — proven sync shapes
 * only, publication after join, 60s-class healthy-host zero-fail
 * baseline, scratch injection proves the reconciliation fires.
 *
 * quality_level BETA.
 * @endparblock
 */

#include "sandstone.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/auxv.h>

#ifndef HWCAP_ATOMICS
#define HWCAP_ATOMICS (1ULL << 8)
#endif

namespace {

constexpr unsigned    BANK_ACCTS = 256;        /* 256 × 16 B */
constexpr uint64_t    BANK_FIELD_MASK = (1ull << 48) - 1;
constexpr uint64_t    BANK_GEN_MAGIC = 0x3C3Cull & 0xFFFFull;   /* same as cas twin */
constexpr unsigned    BANK_MAX_THREADS = 1024;
constexpr uint64_t    BANK_BATCH = 1ull << 13;  /* ops per time check (SPIN_BATCH lesson) */

struct alignas(16) BankAccount {
    uint64_t balance;   /* UNTAGGED raw u64 (see header: atomic adds cannot mask) */
    uint64_t gen;       /* GEN_MAGIC<<48 | ctr48 (tagged; only ever +1) */
};

struct BankThreadStats {
    int64_t  amount_ledger;
    uint64_t gen_ledger;
    uint64_t *tally;
};

struct BankShared {
    alignas(64) BankAccount accounts[BANK_ACCTS];
    alignas(64) std::atomic<uint64_t> thread_idx;
    uint64_t initial_balance_sum;
    BankThreadStats stats[BANK_MAX_THREADS];
};

} // namespace

/* 1 行钉死 ldaddal（防编译器退化——本件就是 LSE 硬件路径本身） */
static inline uint64_t bank_lse_add(uint64_t *p, uint64_t val)
{
    uint64_t old;
    __asm__ volatile("ldaddal %1, %0, [%2]"
                     : "=r"(old)
                     : "r"(val), "r"(p)
                     : "memory");
    return old;
}

static int db_bank_transfer_lse_init(struct test *test)
{
    if ((getauxval(AT_HWCAP) & HWCAP_ATOMICS) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "LSE atomics (HWCAP_ATOMICS) not exposed by this kernel/host");
        return EXIT_SKIP;
    }

    auto *sd = new BankShared;
    if (!sd)
        return EXIT_FAILURE;

    for (unsigned a = 0; a < BANK_ACCTS; ++a) {
        sd->accounts[a].balance = (1ull << 20) + a;   /* untagged field */
        sd->accounts[a].gen = BANK_GEN_MAGIC << 48;   /* ctr = 0 */
    }
    uint64_t sum = 0;
    for (unsigned a = 0; a < BANK_ACCTS; ++a)
        sum += sd->accounts[a].balance;
    sd->initial_balance_sum = sum;

    sd->thread_idx.store(0, std::memory_order_relaxed);
    memset(sd->stats, 0, sizeof(sd->stats));
    test->data = sd;
    return EXIT_SUCCESS;
}

static int db_bank_transfer_lse_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *sd = static_cast<BankShared *>(test->data);

    const uint64_t id = sd->thread_idx.fetch_add(1, std::memory_order_relaxed);
    if (id >= BANK_MAX_THREADS) {
        report_fail_msg("db_bank_transfer_lse: more than %u threads",
                        BANK_MAX_THREADS);
        return EXIT_FAILURE;
    }

    uint64_t rng = 0x9E3779B97F4A7C15ULL * (id + 1) + 0x1234567ULL;
    int64_t  amount_ledger = 0;
    uint64_t gen_ledger = 0;
    uint64_t *tally = static_cast<uint64_t *>(calloc(BANK_ACCTS, sizeof(uint64_t)));
    if (!tally) {
        report_fail_msg("db_bank_transfer_lse: tally alloc failed");
        return EXIT_FAILURE;
    }
    uint64_t ops = 0;

    do {
        for (uint64_t b = 0; b < BANK_BATCH; ++b) {
            rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
            const uint32_t r = static_cast<uint32_t>(rng >> 32);
            const unsigned a1 = (r >> 8) & 0xFF;
            const unsigned amt = ((r >> 20) & 0xF) + 1;   /* 1..16 */
            switch ((r >> 24) & 3) {
            case 0:
            case 1: {  /* transfer: two independent atomic adds, net 0 */
                unsigned a2 = r & 0xFF;
                if (a2 == a1)
                    a2 = (a2 + 1) & 0xFF;
                (void)bank_lse_add(&sd->accounts[a1].balance,
                                   -static_cast<uint64_t>(amt));
                (void)bank_lse_add(&sd->accounts[a2].balance,
                                   static_cast<uint64_t>(amt));
                const uint64_t og1 = bank_lse_add(&sd->accounts[a1].gen, 1);
                const uint64_t og2 = bank_lse_add(&sd->accounts[a2].gen, 1);
                if ((og1 >> 48) != BANK_GEN_MAGIC ||
                    (og2 >> 48) != BANK_GEN_MAGIC) {
                    report_fail_msg("db_bank_transfer_lse: thread %llu op %llu: "
                                    "gen ldaddal returned a non-magic old value "
                                    "(acct %u old 0x%016llx, acct %u old "
                                    "0x%016llx) — corrupted read in the LSE "
                                    "RMW path",
                                    static_cast<unsigned long long>(id),
                                    static_cast<unsigned long long>(ops),
                                    a1,
                                    static_cast<unsigned long long>(og1),
                                    a2,
                                    static_cast<unsigned long long>(og2));
                    free(tally);
                    return EXIT_FAILURE;
                }
                gen_ledger += 2;
                tally[a1]++;
                tally[a2]++;
                break;
            }
            case 2:    /* deposit */
            default: { /* withdraw */
                const int64_t d = ((r >> 24) & 3) == 2
                                      ? static_cast<int64_t>(amt)
                                      : -static_cast<int64_t>(amt);
                (void)bank_lse_add(&sd->accounts[a1].balance,
                                  static_cast<uint64_t>(d));
                const uint64_t og = bank_lse_add(&sd->accounts[a1].gen, 1);
                if ((og >> 48) != BANK_GEN_MAGIC) {
                    report_fail_msg("db_bank_transfer_lse: thread %llu op %llu: "
                                    "gen ldaddal returned a non-magic old value "
                                    "(acct %u old 0x%016llx) — corrupted read "
                                    "in the LSE RMW path",
                                    static_cast<unsigned long long>(id),
                                    static_cast<unsigned long long>(ops), a1,
                                    static_cast<unsigned long long>(og));
                    free(tally);
                    return EXIT_FAILURE;
                }
                amount_ledger += d;
                gen_ledger += 1;
                tally[a1]++;
                break;
            }
            }
            ++ops;
        }
    } while (test_time_condition(test));

    sd->stats[id].amount_ledger = amount_ledger;
    sd->stats[id].gen_ledger = gen_ledger;
    sd->stats[id].tally = tally;
    return EXIT_SUCCESS;
}

static int db_bank_transfer_lse_cleanup(struct test *test)
{
    auto *sd = static_cast<BankShared *>(test->data);
    const uint64_t threads = sd->thread_idx.load(std::memory_order_relaxed);

    /* Book 1: conservation mod 2^64 on raw balance words */
    uint64_t observed_sum = 0;
    for (unsigned a = 0; a < BANK_ACCTS; ++a)
        observed_sum += sd->accounts[a].balance;   /* wrapping u64 sum */
    int64_t ledger_sum = 0;
    uint64_t gen_ledger_sum = 0;
    for (uint64_t t = 0; t < threads && t < BANK_MAX_THREADS; ++t) {
        ledger_sum += sd->stats[t].amount_ledger;
        gen_ledger_sum += sd->stats[t].gen_ledger;
    }
    const uint64_t expected_sum =
        sd->initial_balance_sum + static_cast<uint64_t>(ledger_sum);
    const bool conservation_ok = (observed_sum == expected_sum);

    /* Book 2: per-account gen vs tallies + Book 2b: global gen totals */
    long bad_acct = -1;
    uint64_t bad_observed = 0, bad_expected = 0;
    uint64_t bad_count = 0;
    uint64_t observed_gen_sum = 0;
    uint64_t total_ops = 0;
    for (unsigned a = 0; a < BANK_ACCTS; ++a) {
        uint64_t expect = 0;
        for (uint64_t t = 0; t < threads && t < BANK_MAX_THREADS; ++t)
            if (sd->stats[t].tally)
                expect += sd->stats[t].tally[a];
        total_ops += expect;
        const uint64_t observed = sd->accounts[a].gen & BANK_FIELD_MASK;
        observed_gen_sum += observed;
        if (observed != expect) {
            ++bad_count;
            if (bad_acct < 0) {
                bad_acct = a;
                bad_observed = observed;
                bad_expected = expect;
            }
        }
    }
    const bool gen_global_ok = (observed_gen_sum == gen_ledger_sum);

    bool failed = false;
    const char *attribution = "";
    if (!conservation_ok) {
        failed = true;
        attribution = bad_count > 0
            ? "gen broken + conservation broken: lost or duplicated LSE "
              "update (see account below)"
            : "conservation broken + ALL gen accounting intact: pure "
              "balance-word VALUE corruption (stray store / bit flip in "
              "memory, off the atomic path)";
    } else if (bad_count > 0) {
        failed = true;
        attribution = gen_global_ok
            ? "conservation intact + per-account gen broken but global gen "
              "totals match: the per-thread TALLY book is corrupted"
            : "conservation intact + per-account gen broken: the two "
              "independent LSE adds (balance, gen) diverged — one landed, "
              "the other did not";
    } else if (!gen_global_ok) {
        failed = true;
        attribution = "conservation intact + every account matches its "
                      "tally but global gen totals differ: the per-thread "
                      "LEDGER book is corrupted";
    }

    if (failed) {
        /* 单消息一次打包：report_fail_msg 在 cleanup 首调即中止路径（cas
         * 双胞胎注入自测实证的框架语义） */
        char msg[768];
        int n = snprintf(msg, sizeof(msg),
                         "db_bank_transfer_lse: reconciliation FAILED after "
                         "%llu ops across %llu threads (%s): conservation "
                         "observed 0x%016llx expected 0x%016llx (delta "
                         "0x%016llx, ledger sum %lld)",
                         static_cast<unsigned long long>(total_ops),
                         static_cast<unsigned long long>(threads),
                         attribution,
                         static_cast<unsigned long long>(observed_sum),
                         static_cast<unsigned long long>(expected_sum),
                         static_cast<unsigned long long>(observed_sum - expected_sum),
                         static_cast<long long>(ledger_sum));
        if (bad_acct >= 0) {
            n += snprintf(msg + n, sizeof(msg) - n,
                          "; account %ld (first of %llu deviating): gen "
                          "observed %llu, expected from tallies %llu (%s)",
                          bad_acct,
                          static_cast<unsigned long long>(bad_count),
                          static_cast<unsigned long long>(bad_observed),
                          static_cast<unsigned long long>(bad_expected),
                          bad_observed < bad_expected
                              ? "BEHIND tallies: lost update"
                              : "AHEAD of tallies: duplicated/phantom update");
        }
        if (!gen_global_ok) {
            snprintf(msg + n, sizeof(msg) - n,
                     "; global gen totals: accounts sum %llu vs thread "
                     "ledgers sum %llu",
                     static_cast<unsigned long long>(observed_gen_sum),
                     static_cast<unsigned long long>(gen_ledger_sum));
        }
        report_fail_msg("%s", msg);
    }

    for (uint64_t t = 0; t < threads && t < BANK_MAX_THREADS; ++t)
        free(sd->stats[t].tally);
    delete sd;
    test->data = nullptr;

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

DECLARE_TEST(db_bank_transfer_lse, "Bank-transfer conservation probe, LSE-atomic arm (structural twin of db_bank_transfer_cas): same 256-account concurrent transfer/deposit/withdrawal load and four-book cleanup reconciliation, but the atomic path is single-word LDADDAL (balance and gen as independent atomics, no exclusive pair, no retry loop) — different snoop micro-traffic, discriminative pair with the cas arm; balance untagged mod-2^64 (atomic adds cannot mask carries), gen tagged with per-op old-value magic validation; ldaddal pinned in 1-line asm, HWCAP_ATOMICS-gated honest skip")
    .test_init    = db_bank_transfer_lse_init,
    .test_run     = db_bank_transfer_lse_run,
    .test_cleanup = db_bank_transfer_lse_cleanup,
    .fracture_loop_count = -1,   /* never fracture: cleanup 对账是完整性单元 */
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
