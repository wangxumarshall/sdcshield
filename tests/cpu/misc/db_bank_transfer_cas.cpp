/**
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b db_bank_transfer_cas
 * @parblock
 * Bank-transfer conservation probe, exclusive-pair arm (2026-11-28).
 *
 * THE LOAD ENVIRONMENT THIS EXTRACTS (user directive 2026-11-28): the
 * defining feature of a bank database service is MANY RECORDS under
 * CONCURRENT conflicting read-modify-write — transfers, deposits and
 * withdrawals from many cores at once, not the per-thread private
 * buffers the earlier db_load_* pieces used. This piece is that
 * environment: 256 accounts (16 B each: {balance, gen} as ONE 16-byte
 * exclusively-updated pair), all cores hammering arbitrary accounts.
 * With 192-class hosts that is sustained cross-core cache-coherence
 * traffic on dirty lines — snoop, invalidation and dirty-migration
 * paths carry every operation, which no private-buffer test can reach.
 *
 * THE THREE ACCOUNTING BOOKS (detection by invariants, not fixed
 * goldens — data legally changes every op):
 *   1. CONSERVATION: every transfer moves -x/+x (net 0), deposits +x,
 *      withdrawals -x, all mod 2^48 on the balance field. Round-end
 *      (cleanup, after all workers join): sum(balance fields) must
 *      equal initial + sum(per-thread amount ledgers).
 *   2. GEN ACCOUNTING: every RMW bumps the account's gen counter (the
 *      pair's second word), so per-account: gen == sum of per-thread
 *      tallies for that account — this LOCALIZES a lost or duplicated
 *      update to the exact account, with direction (behind = lost RMW,
 *      ahead = duplicated). A global cross-check (sum of account gens
 *      vs sum of thread gen ledgers) further splits WHICH book lies:
 *      accounts all matching tallies but the global totals differing
 *      means the per-thread LEDGER book is corrupted; per-account
 *      mismatches with matching global totals means the tally
 *      DISTRIBUTION is corrupted.
 *   3. PER-THREAD LEDGERS + TALLIES: private to each worker (no races
 *      by construction); published once at run end into single-writer
 *      slots; cleanup reconciles after the framework joins the workers
 *      (spinlock_stale_read chassis, proven).
 * Attribution: gen broken + conservation broken -> lost/duplicated
 * atomic; conservation broken + gen intact -> pure balance-word value
 * corruption (stray store/bit-flip NOT through the RMW path); gen
 * broken + conservation intact -> pair atomicity itself failed (the
 * most interesting signature — both words updated "successfully" but
 * inconsistently).
 *
 * PER-OP IN-RUN CHECK: every ldxp-loaded pair is magic-validated
 * before the store is computed (balance and gen both carry a 16-bit
 * MAGIC in bits 63..48) — a corrupted read in the RMW path is named
 * immediately with account/op/thread/both raw words.
 *
 * MODULAR ARITHMETIC, NO LEGALITY CONSTRAINTS: balance fields wrap mod
 * 2^48 (no negative-balance checks — conservation is exact in modular
 * arithmetic regardless of interleaving), gen counters only increment
 * and cannot realistically reach the magic bits (2^48 RMWs per
 * account). The pair store masks both fields, so no carry ever
 * touches a magic.
 *
 * TRANSFERS ARE TWO INDEPENDENT RMWs (deliberate): -x on A then +x on
 * B, no multi-record atomicity — legal interleavings are unbounded but
 * conservation holds because every op is ledgered at intent time and
 * both halves land by cleanup. A lost HALF-update breaks conservation
 * by exactly that half — the per-account gen tally then names the
 * account whose RMW went missing.
 *
 * ASM JUSTIFICATION (minimal-asm directive): the 16-byte exclusive
 * pair ldxp/stxp has NO C++ intrinsic (std::atomic cannot express a
 * 16 B lock-free CAS pair), so the RMW core is ~10 lines of asm — the
 * only place C++ cannot reach here. Everything else is C++.
 *
 * SOFTWARE-RACE HARDENING (the #1 risk of concurrent test code): sync
 * patterns are only proven ones (std::atomic thread slots, the
 * ldxp-compute-stxp standard CAS shape Linux itself uses); no DIY
 * synchronization; publication happens after join via the framework's
 * cleanup guarantee. Verified zero-fail on the healthy host; scratch
 * injection (Step 5 of the plan) proves the reconciliation FIRES.
 *
 * DEPLOYMENT NOTE: cross-core coherence ping-pong is the
 * highest-current-density load class in this suite; 0203 deployment
 * follows the progressive-load protocol (§12) — single sick core
 * first, controlled sets, then all-core.
 *
 * quality_level BETA. The LSE-atomic twin (db_bank_transfer_lse) is
 * the discriminative pair: different atomic path (ldaddal, no pair),
 * different snoop traffic — a hit on one arm and silence on the other
 * names the atomic path.
 * @endparblock
 */

#include "sandstone.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr unsigned    BANK_ACCTS = 256;        /* 256 × 16 B = 4 KiB */
constexpr uint64_t    BANK_FIELD_MASK = (1ull << 48) - 1;
constexpr uint64_t    BANK_BAL_MAGIC = 0x5A5Aull & 0xFFFFull;   /* GPS-style */
constexpr uint64_t    BANK_GEN_MAGIC = 0x3C3Cull & 0xFFFFull;
constexpr unsigned    BANK_MAX_THREADS = 1024;
constexpr uint64_t    BANK_BATCH = 1ull << 13;  /* ops per time check (SPIN_BATCH lesson) */

struct alignas(16) BankAccount {
    uint64_t balance;   /* BAL_MAGIC<<48 | field48 (mod 2^48) */
    uint64_t gen;       /* GEN_MAGIC<<48 | ctr48 (only increments) */
};

struct BankThreadStats {
    int64_t  amount_ledger;   /* net balance delta this thread applied */
    uint64_t gen_ledger;      /* total gen bumps this thread applied */
    uint64_t *tally;          /* per-account RMW counts (calloc'd in run) */
};

struct BankShared {
    alignas(64) BankAccount accounts[BANK_ACCTS];
    alignas(64) std::atomic<uint64_t> thread_idx;
    uint64_t initial_field_sum;
    BankThreadStats stats[BANK_MAX_THREADS];
};

} // namespace

/* 16 B exclusive-pair RMW: ldxp → C++ magic-check + compute → stxp
 * retry loop (the standard CAS shape; no intrinsic exists for this).
 * Returns false only on a magic failure (already reported). */
static bool bank_cas_add(BankAccount *acct, int64_t delta, unsigned idx,
                         uint64_t ops, uint64_t id)
{
    for (;;) {
        uint64_t ob, og;
        __asm__ volatile("ldxp %0, %1, [%2]"
                         : "=r"(ob), "=r"(og)
                         : "r"(acct));
        if ((ob >> 48) != BANK_BAL_MAGIC || (og >> 48) != BANK_GEN_MAGIC) {
            report_fail_msg("db_bank_transfer_cas: thread %llu op %llu: account "
                            "%u pair read FAILED magic validation in the RMW "
                            "path: balance 0x%016llx (magic %04llx), "
                            "gen 0x%016llx (magic %04llx)",
                            static_cast<unsigned long long>(id),
                            static_cast<unsigned long long>(ops), idx,
                            static_cast<unsigned long long>(ob),
                            static_cast<unsigned long long>(ob >> 48),
                            static_cast<unsigned long long>(og),
                            static_cast<unsigned long long>(og >> 48));
            return false;
        }
        const uint64_t nb = (BANK_BAL_MAGIC << 48) |
                            (((ob & BANK_FIELD_MASK) +
                              static_cast<uint64_t>(delta)) & BANK_FIELD_MASK);
        const uint64_t ng = (BANK_GEN_MAGIC << 48) |
                            (((og & BANK_FIELD_MASK) + 1) & BANK_FIELD_MASK);
        uint32_t st;
        __asm__ volatile("stxp %w0, %1, %2, [%3]"
                         : "=r"(st)
                         : "r"(nb), "r"(ng), "r"(acct)
                         : "memory");
        if (st == 0)
            return true;
        /* exclusive lost to contention — plain retry */
    }
}

static int db_bank_transfer_cas_init(struct test *test)
{
    auto *sd = new BankShared;
    if (!sd)
        return EXIT_FAILURE;

    for (unsigned a = 0; a < BANK_ACCTS; ++a) {
        sd->accounts[a].balance = (BANK_BAL_MAGIC << 48) |
                                   ((1ull << 20) + a);   /* bounded start field */
        sd->accounts[a].gen = BANK_GEN_MAGIC << 48;      /* ctr = 0 */
    }
    uint64_t sum = 0;
    for (unsigned a = 0; a < BANK_ACCTS; ++a)
        sum += sd->accounts[a].balance & BANK_FIELD_MASK;
    sd->initial_field_sum = sum;

    sd->thread_idx.store(0, std::memory_order_relaxed);
    memset(sd->stats, 0, sizeof(sd->stats));
    test->data = sd;
    return EXIT_SUCCESS;
}

static int db_bank_transfer_cas_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *sd = static_cast<BankShared *>(test->data);

    const uint64_t id = sd->thread_idx.fetch_add(1, std::memory_order_relaxed);
    if (id >= BANK_MAX_THREADS) {
        report_fail_msg("db_bank_transfer_cas: more than %u threads",
                        BANK_MAX_THREADS);
        return EXIT_FAILURE;
    }

    /* per-thread private LCG — hot loop free of framework calls */
    uint64_t rng = 0x9E3779B97F4A7C15ULL * (id + 1) + 0x1234567ULL;
    int64_t  amount_ledger = 0;
    uint64_t gen_ledger = 0;
    uint64_t *tally = static_cast<uint64_t *>(calloc(BANK_ACCTS, sizeof(uint64_t)));
    if (!tally) {
        report_fail_msg("db_bank_transfer_cas: tally alloc failed");
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
            case 1: {  /* transfer: two independent RMWs, net 0 */
                unsigned a2 = r & 0xFF;
                if (a2 == a1)
                    a2 = (a2 + 1) & 0xFF;
                if (!bank_cas_add(&sd->accounts[a1], -static_cast<int64_t>(amt),
                                  a1, ops, id) ||
                    !bank_cas_add(&sd->accounts[a2], static_cast<int64_t>(amt),
                                  a2, ops, id))
                    goto out_fail;
                gen_ledger += 2;
                tally[a1]++;
                tally[a2]++;
                break;
            }
            case 2:    /* deposit */
                if (!bank_cas_add(&sd->accounts[a1], static_cast<int64_t>(amt),
                                  a1, ops, id))
                    goto out_fail;
                amount_ledger += amt;
                gen_ledger += 1;
                tally[a1]++;
                break;
            default:   /* withdraw */
                if (!bank_cas_add(&sd->accounts[a1], -static_cast<int64_t>(amt),
                                  a1, ops, id))
                    goto out_fail;
                amount_ledger -= amt;
                gen_ledger += 1;
                tally[a1]++;
                break;
            }
            ++ops;
        }
    } while (test_time_condition(test));

    /* single-writer publication; cleanup runs after the framework
     * joins the workers (spinlock_stale_read chassis) */
    sd->stats[id].amount_ledger = amount_ledger;
    sd->stats[id].gen_ledger = gen_ledger;
    sd->stats[id].tally = tally;
    return EXIT_SUCCESS;

out_fail:
    free(tally);
    return EXIT_FAILURE;
}

static int db_bank_transfer_cas_cleanup(struct test *test)
{
    auto *sd = static_cast<BankShared *>(test->data);
    const uint64_t threads = sd->thread_idx.load(std::memory_order_relaxed);

    /* Book 1: conservation — observed fields vs initial + ledgers (mod 2^48) */
    uint64_t observed_sum = 0;
    for (unsigned a = 0; a < BANK_ACCTS; ++a)
        observed_sum += sd->accounts[a].balance & BANK_FIELD_MASK;
    int64_t ledger_sum = 0;
    uint64_t gen_ledger_sum = 0;
    for (uint64_t t = 0; t < threads && t < BANK_MAX_THREADS; ++t) {
        ledger_sum += sd->stats[t].amount_ledger;
        gen_ledger_sum += sd->stats[t].gen_ledger;
    }
    const uint64_t expected_sum =
        (sd->initial_field_sum + static_cast<uint64_t>(ledger_sum)) &
        BANK_FIELD_MASK;
    const uint64_t observed_mod = observed_sum & BANK_FIELD_MASK;
    const bool conservation_ok = (observed_mod == expected_sum);

    /* Book 2: per-account gen vs tallies — localizes and gives direction.
     * Book 2b (global): sum of account gens vs sum of thread gen ledgers —
     * splits WHICH book lies: per-account all matching but global differing
     * = thread LEDGER book corrupted; per-account mismatch with global
     * matching = tally DISTRIBUTION corrupted. */
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
            ? "gen broken + conservation broken: lost or duplicated RMW "
              "update (see account below)"
            : "conservation broken + ALL gen accounting intact: pure "
              "balance-word VALUE corruption off the RMW path (stray "
              "store / bit flip in memory)";
    } else if (bad_count > 0) {
        failed = true;
        attribution = gen_global_ok
            ? "conservation intact + per-account gen broken but global gen "
              "totals match: the per-thread TALLY book is corrupted (the "
              "distribution lies, the totals do not)"
            : "conservation intact + per-account gen broken: the 16 B pair "
              "atomicity itself failed (balance moved, gen did not, or "
              "vice versa) — the most interesting signature";
    } else if (!gen_global_ok) {
        failed = true;
        attribution = "conservation intact + every account matches its "
                      "tally but global gen totals differ: the per-thread "
                      "LEDGER book is corrupted (accounts self-consistent)";
    }
    if (failed) {
        /* 单消息一次打包：report_fail_msg 在 cleanup（控制线程）首次调用即
         * 中止路径——分多条报文会让后续字段永远不出现（注入自测实证） */
        char msg[768];
        int n = snprintf(msg, sizeof(msg),
                         "db_bank_transfer_cas: reconciliation FAILED after "
                         "%llu ops across %llu threads (%s): conservation "
                         "observed 0x%016llx expected 0x%016llx (delta "
                         "0x%016llx, ledger sum %lld)",
                         static_cast<unsigned long long>(total_ops),
                         static_cast<unsigned long long>(threads),
                         attribution,
                         static_cast<unsigned long long>(observed_mod),
                         static_cast<unsigned long long>(expected_sum),
                         static_cast<unsigned long long>(
                             (observed_mod - expected_sum) & BANK_FIELD_MASK),
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
                              ? "BEHIND tallies: lost RMW update"
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

DECLARE_TEST(db_bank_transfer_cas, "Bank-transfer conservation probe, exclusive-pair arm (multi-record concurrent RMW load environment): 256 accounts {balance,gen} as 16 B ldxp/stxp pairs, all cores doing concurrent transfers/deposits/withdrawals — sustained dirty-line coherence traffic; three accounting books (mod-2^48 conservation + per-account gen counts + per-thread ledgers/tallies) reconcile at cleanup with lost/duplicated/value-corruption/pair-atomicity attribution; per-op magic validation in the RMW path; discriminative pair with db_bank_transfer_lse (ldaddal path)")
    .test_init    = db_bank_transfer_cas_init,
    .test_run     = db_bank_transfer_cas_run,
    .test_cleanup = db_bank_transfer_cas_cleanup,
    .fracture_loop_count = -1,   /* never fracture: cleanup 对账是完整性单元 */
    .quality_level = TEST_QUALITY_BETA,
END_DECLARE_TEST
