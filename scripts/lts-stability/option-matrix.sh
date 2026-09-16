#!/bin/bash
# option-matrix.sh — LTS 稳定性验证的选项矩阵定义(被 run-lts-stability.sh source)。
#
# 每个矩阵条目定义一次 sdcshield 调用:
#   run_<id>()   : 完整命令行(在容器内执行,$BIN 为二进制路径)
#   desc_<id>()  : 人类可读描述
# 判定:exit code 0 且输出 yaml 无 "result: fail"、无崩溃标志 → 该条目 PASS。
#       selftest 类(预期 fail/crash 的)用 expect_fail_ 前缀豁免。
#
# SPDX-License-Identifier: Apache-2.0

# 矩阵条目清单(执行顺序)。命名规则:
#   m<NN>_<name>          普通条目,要求 exit 0 + 0 fail + 0 crash
#   x<NN>_<name>          预期非零/预期 fail 的条目(selftest 故障注入),只要求"工具不崩溃地完成"
#                         (父进程 exit code 可非 0,但不得出现工具自身的段错误/挂死/回溯异常)
MATRIX_IDS=(
    m01_full_baseline
    m02_eigen_flaky_n1
    m03_quality_minus1
    m04_quality_beta
    m05_allcores
    m06_rng_constant
    m07_rng_lcg
    m08_rng_aes
    m09_cpuset_single
    m10_cpuset_crossnuma
    m11_fatal_strict
    m12_randomize
    m13_knob_zstd_level_low
    m14_knob_zstd_level_high
    m15_knob_zlib_level_low
    m16_knob_zlib_level_high
    m17_knob_openblas_mdim_min
    m18_knob_openblas_mdim_max
    m19_memcpy_rewr_strategy0
    m20_memcpy_rewr_strategy1
    m21_memcpy_rewr_strategy2
    m22_memcpy_rewr_default
    x23_selftests
    x24_oncrash_sigsegv
    m25_total_time_mode
    m26_verbose_vv
    m27_ipsec_static_ssl
    m28_maxtestcount_1sec
)

# eigen 数值敏感测试(CLAUDE.md: 大规模多线程下 ULP 级偶发假 FAIL,-n 1 稳定)
EIGEN_FLAKY="eigen_svd_double eigen_sparse eigen_svd_cdouble eigen_svd_cdouble_sve"

# 每条目的执行函数。$BIN/$OUT(容器内路径)、$T_SHORT/$T_BASE(时长)由调用方注入。
# 所有调用都带 --ignore-timeout(容器内 cgroup 偶发 timeout 非被测对象)。

run_m01_full_baseline() { "$BIN" --ignore-timeout -t "$T_BASE" -n 8 -o "$OUT/m01.yaml"; }

run_m02_eigen_flaky_n1() {
    local rc=0 t
    for t in $EIGEN_FLAKY; do
        "$BIN" --ignore-timeout -e "$t" -t "$T_BASE" -n 1 -o "$OUT/m02_${t}.yaml" || rc=$?
    done
    return $rc
}

run_m03_quality_minus1() { "$BIN" --quality=-1 --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m03.yaml"; }
run_m04_quality_beta()   { "$BIN" --quality=0   --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m04.yaml"; }

run_m05_allcores() {
    local rc=0 t
    local disable=""
    for t in $EIGEN_FLAKY; do disable="$disable --disable $t"; done
    # shellcheck disable=SC2086
    "$BIN" --ignore-timeout -t "$T_BASE" $disable -o "$OUT/m05.yaml" || rc=$?
    # flaky 4 个 -n 1 补跑(全核跑不判它们)
    for t in $EIGEN_FLAKY; do
        "$BIN" --ignore-timeout -e "$t" -t "$T_BASE" -n 1 -o "$OUT/m05_${t}.yaml" || rc=$?
    done
    return $rc
}

run_m06_rng_constant() { "$BIN" -e zstd19 -e crc32 -e fma --ignore-timeout -t "$T_SHORT" -n 4 -s 'Constant:1' -o "$OUT/m06.yaml"; }
run_m07_rng_lcg()      { "$BIN" -e zstd19 -e crc32 -e fma --ignore-timeout -t "$T_SHORT" -n 4 -s 'LCG:1'      -o "$OUT/m07.yaml"; }
run_m08_rng_aes()      { "$BIN" -e zstd19 -e crc32 -e fma --ignore-timeout -t "$T_SHORT" -n 4 -s 'AES:1'      -o "$OUT/m08.yaml"; }

run_m09_cpuset_single()     { "$BIN" -e zstd19 -e fma --ignore-timeout -t "$T_SHORT" --cpuset=0 -o "$OUT/m09.yaml"; }
run_m10_cpuset_crossnuma()  { "$BIN" -e zstd19 -e fma --ignore-timeout -t "$T_SHORT" --cpuset=0,24,48,72,96,120,144,168 -o "$OUT/m10.yaml"; }

run_m11_fatal_strict() { "$BIN" -F --strict-runtime --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m11.yaml"; }
run_m12_randomize()    { "$BIN" --test-list-randomize --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m12.yaml"; }

run_m13_knob_zstd_level_low()  { "$BIN" -e zstd19 --ignore-timeout -t "$T_SHORT" -n 4 -O zstd19.level=1  -O zstd19.maxbuffersize=4096 -o "$OUT/m13.yaml"; }
run_m14_knob_zstd_level_high() { "$BIN" -e zstd19 --ignore-timeout -t "$T_SHORT" -n 4 -O zstd19.level=19 -o "$OUT/m14.yaml"; }
run_m15_knob_zlib_level_low()  { "$BIN" -e zlib9  --ignore-timeout -t "$T_SHORT" -n 4 -O zlib9.level=1  -O zlib9.maxbuffersize=4096 -o "$OUT/m15.yaml"; }
run_m16_knob_zlib_level_high() { "$BIN" -e zlib9  --ignore-timeout -t "$T_SHORT" -n 4 -O zlib9.level=9  -o "$OUT/m16.yaml"; }

run_m17_knob_openblas_mdim_min() { "$BIN" -e openblas_dgemm -e openblas_sgemm -e openblas_zgemm --ignore-timeout -t "$T_SHORT" -n 4 -O openblas_dgemm.mdim=16 -O openblas_sgemm.mdim=16 -O openblas_zgemm.mdim=16 -o "$OUT/m17.yaml"; }
run_m18_knob_openblas_mdim_max() { "$BIN" -e openblas_dgemm -e openblas_sgemm -e openblas_zgemm --ignore-timeout -t "$T_SHORT" -n 4 -O openblas_dgemm.mdim=4096 -O openblas_sgemm.mdim=4096 -O openblas_zgemm.mdim=4096 -o "$OUT/m18.yaml"; }

run_m19_memcpy_rewr_strategy0() { SANDSTONE_STRATEGY_INDEX=0 "$BIN" -e memcpy_rewr --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m19.yaml"; }
run_m20_memcpy_rewr_strategy1() { SANDSTONE_STRATEGY_INDEX=1 "$BIN" -e memcpy_rewr --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m20.yaml"; }
run_m21_memcpy_rewr_strategy2() { SANDSTONE_STRATEGY_INDEX=2 "$BIN" -e memcpy_rewr --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m21.yaml"; }
run_m22_memcpy_rewr_default()   { "$BIN" -e memcpy_rewr --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m22.yaml"; }

# selftests 框架自检套件。预期 exit 非 0(大量故意 fail/crash 注入)→ x 系语义,
# 但保留 m 前缀编号。判定标准(脚本层):完成全部条目 + 无工具级挂死(timeout)。
# freeze 家族 5 个 --disable:它们冻结直至框架 SIGQUIT/SIGKILL,会把单条目拖到
# 300s+ 且 selftest_freeze_ignore_termination 在 --ignore-timeout 下实测永久挂死
# (rootless 容器外亦复现)——与"容器内稳定性"验证目标正交,故排除。
# 实测(2026-09-16, 本机 builddir-full):disable 后 289 结果全部落盘,exit: invalid(注入预期)。
run_x23_selftests() {
    "$BIN" --selftests --ignore-timeout -t 300 -n 4 \
        --disable selftest_freeze --disable selftest_freeze_fork \
        --disable selftest_freeze_exit_on_termination \
        --disable selftest_freeze_ignore_termination \
        --disable selftest_freeze_socket1 \
        -o "$OUT/x23.yaml"
    local rc=$?
    # exit: invalid/failure 是注入预期;只有挂死(timeout 124)或段错误(139)才算失败
    [ "$rc" -eq 124 ] || [ "$rc" -eq 139 ] || return 0
    return $rc
}

# 预期失败:selftest_sigsegv 就是故意段错误,验证 --on-crash=context 能干净地捕获并报告
# (工具自身不能崩)。exit code 非 0 是预期;x 前缀豁免 exit 判定,只查工具回溯异常。
run_x24_oncrash_sigsegv() { "$BIN" --on-crash=context -e selftest_sigsegv -vv -t 2000 -n 1 -o "$OUT/x24.yaml"; }

run_m25_total_time_mode() { "$BIN" --ignore-timeout -T 10s --strict-runtime --max-test-count 20 -n 8 -o "$OUT/m25.yaml"; }

run_m26_verbose_vv() {
    local rc=0 t
    for t in zstd19 crc32 fma openssl_sha sleef_neon; do
        "$BIN" -e "$t" --ignore-timeout -t "$T_SHORT" -n 2 -vv -o "$OUT/m26_${t}.yaml" || rc=$?
    done
    return $rc
}

run_m27_ipsec_static_ssl() { "$BIN" -e 'ipsec*' -e openssl_sha --ignore-timeout -t "$T_SHORT" -n 8 -o "$OUT/m27.yaml"; }

run_m28_maxtestcount_1sec() { "$BIN" --ignore-timeout --1sec --max-test-count 30 -n 8 -o "$OUT/m28.yaml"; }
