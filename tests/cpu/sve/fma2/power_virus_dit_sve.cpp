/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b power_virus_dit_sve
 * @parblock
 * SVE port of power_virus_dit: di/dt voltage-transient power virus with the
 * burst/stall duty cycle preserved (BURST_INNER/STALL_INNER = 2048, ~50%),
 * alternating high-Hamming operand pairs, and a byte-exact per-lane
 * accumulator check against the identical fmaf software sequence. The FMA
 * burst runs on svmla_f32_x across all SVE lanes instead of NEON vfmaq.
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>

#ifdef __aarch64__
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

static inline uint32_t bits_of(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

static constexpr int BURST_INNER = 2048;
static constexpr int STALL_INNER = 2048;

static int power_virus_dit_sve_init(struct test *test)
{
    (void)test;
#ifdef __aarch64__
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for power_virus_dit_sve");
        return EXIT_SKIP;
    }
#endif
    return EXIT_SUCCESS;
}

#ifdef __aarch64__

// Alternating high-Hamming operand pairs (same constants as the original).
static const float BURST_A0 = 1.0f;
static const float BURST_A1 = -2.0f;
static const float BURST_B0 = 4.0f;
static const float BURST_B1 = 0.125f;
static const float BURST_C  = 0.0f;

// SVE dependent FMA burst: all lanes run the same dependent chain.
static inline svfloat32_t burst_fma_chain_sve(int n)
{
    svfloat32_t a0 = svdup_f32(BURST_A0);
    svfloat32_t a1 = svdup_f32(BURST_A1);
    svfloat32_t b0 = svdup_f32(BURST_B0);
    svfloat32_t b1 = svdup_f32(BURST_B1);
    svfloat32_t acc = svdup_f32(BURST_C);
    int n2 = n / 2;
    for (int i = 0; i < n2; ++i) {
        acc = svmla_f32_x(svptrue_b32(), acc, a0, b0);   // acc += A0*B0
        acc = svmla_f32_x(svptrue_b32(), acc, a1, b1);   // acc += A1*B1
    }
    return acc;
}

// Software reference: identical dependent fmaf sequence (one lane).
static inline float burst_fma_reference(int n)
{
    float acc = BURST_C;
    int n2 = n / 2;
    for (int i = 0; i < n2; ++i) {
        acc = fmaf(BURST_A0, BURST_B0, acc);
        acc = fmaf(BURST_A1, BURST_B1, acc);
    }
    return acc;
}

static inline void stall_yield(int n)
{
    for (int i = 0; i < n; ++i) {
        __asm__ volatile("yield" ::: "memory");
    }
}

static int power_virus_dit_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    (void)test;

    float sw_ref = burst_fma_reference(BURST_INNER);
    svfloat32_t vref = svdup_f32(sw_ref);
    svbool_t pg = svptrue_b32();
    const int lanes = svcntw();
    float accv[32];   /* 最大 VL 1024bit */

    do {
        bool all_passed = true;

        for (int cycle = 0; cycle < 8; ++cycle) {
            /* ---- BURST: dependent SVE FMA chain, alternating operands ---- */
            svfloat32_t acc = burst_fma_chain_sve(BURST_INNER);

            /* Byte-exact compare of ALL SVE lanes against the software ref */
            svbool_t cmp = svcmpeq_f32(pg, acc, vref);
            uint64_t nmatch = svcntp_b32(pg, cmp);
            if (nmatch != (uint64_t)lanes) {
                all_passed = false;
                svst1_f32(pg, accv, acc);
                log_warning("power_virus_dit_sve: burst cycle %d accumulator "
                            "mismatch (%lu/%d lanes) hw[0]=0x%08x sw=0x%08x",
                            cycle, (unsigned long)nmatch, lanes,
                            bits_of(accv[0]), bits_of(sw_ref));
                break;
            }

            /* ---- STALL: near-zero activity ---- */
            stall_yield(STALL_INNER);
        }

        if (!all_passed) {
            report_fail_msg("power_virus_dit_sve: di/dt transient SDC detected "
                            "(SVE FMA corrupted mid burst/stall cycle)");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

#else

static int power_virus_dit_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for "
             "power-virus di/dt stress (SVE)");
    return EXIT_SKIP;
}

#endif

static int power_virus_dit_sve_finish(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(power_virus_dit_sve,
             "di/dt voltage-transient power virus on SVE: alternating svmla full-load "
             "bursts with yield stalls to induce voltage droop and excite "
             "transient SDC on the ARM64 SVE/FMA path (port of power_virus_dit)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = power_virus_dit_sve_init,
    .test_run = power_virus_dit_sve_run,
    .test_cleanup = power_virus_dit_sve_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
