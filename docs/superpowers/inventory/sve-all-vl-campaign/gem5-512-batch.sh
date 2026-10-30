#!/bin/bash
# SVE 全 VL 验证运动 — gem5 SE 512-bit 腿批量执行脚本（Task 6）。
#
# Usage: ./gem5-512-batch.sh [parallelism]
#   默认 P=20（控制器修正案 2；标定实测单实例 RSS ≤ ~1.2 GB——sleef 1.2G /
#   eigen 0.43G / 轻测试 ~1.2G——20 路典型 ~24 GB，远低于 57 GB 可用）。
#
# 执行形状（控制器修正案 + 标定裁定，全部必须在 triage-512.md 头部如实标注）：
#   - gem5 SE（atomic），二进制 md5 3f9011f40dbe80ec1df478c62ddb0b06
#     （Task 5 终版 9-stub 链；G5 显式传入，不信任 run_sve.sh 的陈旧默认值）
#   - run_sve.sh ... -e <test> -t 500 -n 1 -f no --ignore-timeout
#     <26 项 baked -O> [gem5-overrides.txt 的额外参数] --vl 4
#   - -f no（nofork）：gem5 SE 无异步信号投递/跨 fork mmap 共享，fork 模式
#     结构性不可行（Task 5 双侧分析）；判定 = 进程退出码 + 输出中的
#     "exit:" 行（nofork 掩蔽失败归因细节——单测单进程已是本设计）。
#   - 不用 --fatal-skips（Task 6 实证决定，详见 triage-512.md §3）：叠印使
#     run_one_test_once 的 main_thread_data()->has_skipped() 读到 worker
#     Test 字段的垃圾位 → 框架会把"整轮跑完且比对通过"的测试误标
#     Skipped；--fatal-skips 会把这些假标签致命化成批量假红格（fma 全族
#     + sleef 实证复演）。skip 可见性改由分诊期的 tick 地板审计承担。
#   - 26 项 baked -O：native 256 腿裁定 D 的同集（acl dim=512 + 25 项
#     clamp 下限），取自 native-256-LCG1002.stdout.txt 命令行；VL=512 下
#     这些 knob 值（计数/字节语义）仍然有效。
#   - 每测试独立 .d 工作目录（gem5 把 m5out 写进 cwd，并发共用会互相覆写）。
#   - timeout 3600 兜底；断点续跑：$t.done 存在即跳过（幂等重启用）。
#
# 产物（不入库，.gitignore 覆盖）：gem5-512/<t>.log（原始输出）、
#   gem5-512/<t>.d/（m5out 工作目录）、gem5-512/batch.tsv（每测试一行
#   rc/墙钟/exit 行——gem5-512-extract.sh 的机械提取输入）。
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
LIST="$HERE/sve-test-list.txt"
OVR="$HERE/gem5-overrides.txt"   # 测试|额外参数（baked 26 之外的降容，标定产物）
OUT="$HERE/gem5-512"
G5=/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt
RUN=/home/sdc/wangxu/sdcshield-main/scripts/eigen-sve-double/gem5/run_sve.sh
BIN=/home/sdc/wangxu/sdcshield-main/builddir/sdcshield
P=${1:-20}

# 26 项 baked -O（= native-256-LCG1002.stdout.txt 命令行逐项拷贝，勿手改；
# 重复键后值覆盖前值：framework/test_knobs.cpp set_knob —— overrides 文件
# 里的同键 -O 可安全叠加降容）。
BAKED=(-O acl_gemm_sve.dim=512
     -O agu_stress_2src_sve.count=1024
     -O agu_stress_store_only_sve.count=1024
     -O arm0102_fma_f32_sve.count=1024
     -O arm0102_kreg_logic_sve.count=1024
     -O arm0102_kreg_mask_sve.count=1024
     -O arm0102_kreg_not_sve.count=1024
     -O arm0102_kreg_select_sve.count=1024
     -O eigen_svd_bidiag_sve.mdim=64
     -O eigen_svd_cdouble_sve.mdim=300
     -O gmp_bigadd_sve.limbs=256
     -O ipsec_3des_docsis_aes_cmac_sve_wide.data_size=1024
     -O ipsec_3des_docsis_aes_xcbc_sve_wide.data_size=1024
     -O ipsec_aes192_cbc_xcbc_96_sve.data_size=1024
     -O ipsec_aes192_ctr_xcbc_sve.data_size=1024
     -O mem_disambig_alu_sve.buffer_bytes=4096
     -O mesh_upi_sve_sym.buffer_bytes=1048576
     -O movmskpspd_sve.count=1024
     -O sve_rot_ldr_at_top.slots=1024
     -O sve_rot_ldr_at_top_rowmajor.slots=1024
     -O sve_rot_ldr_at_top_rowmajor_k3res.slots=4096
     -O sve_rot_ldr_at_top_rowmajor_k5inter_rand.slots=1024
     -O sve_rot_ldr_at_top_rowmajor_k7p_hiham.slots=1024
     -O sve_rot_ldr_at_top_rowmajor_k7p_one.slots=1024
     -O sve_rot_ldr_at_top_rowmajor_k7p_rand.slots=1024
     -O sve_rot_ldr_at_top_rowmajor_k7p_zero.slots=1024)

mkdir -p "$OUT"

if [ "${1:-}" = "--one" ]; then
    t="$2"
    [ -f "$OUT/$t.done" ] && exit 0          # 断点续跑：已完成即跳过
    extra=""
    extra=$(grep -m1 "^$t|" "$OVR" 2>/dev/null | cut -d'|' -f2- || true)
    mkdir -p "$OUT/$t.d"
    start=$(date +%s)
    ( cd "$OUT/$t.d" && timeout 3600 env G5="$G5" "$RUN" "$BIN" \
        -e "$t" -t 500 -n 1 -f no --ignore-timeout \
        "${BAKED[@]}" $extra --vl 4 ) > "$OUT/$t.log" 2>&1
    rc=$?
    end=$(date +%s)
    exitline=$(grep -a -m1 "^exit: " "$OUT/$t.log" || true)
    simexit=$(grep -a "Simulated exit code" "$OUT/$t.log" | tail -1)
    printf '%s\trc=%s\twall=%ss\t%s\t%s\n' "$t" "$rc" "$((end-start))" \
        "${exitline:-<no-exit-line>}" "${simexit:-<no-sim-exit>}" \
        >> "$OUT/batch.tsv"
    printf '%s\trc=%s\twall=%ss\t%s\t%s\n' "$t" "$rc" "$((end-start))" \
        "${exitline:-<no-exit-line>}" "${simexit:-<no-sim-exit>}"
    touch "$OUT/$t.done"
    exit 0
fi

echo "batch start: $(date '+%F %T')  P=$P  list=$(wc -l < "$LIST") tests"
echo "gem5: $G5  md5=$(md5sum "$G5" | cut -d' ' -f1)"
echo "bin:  $BIN  $($BIN --version 2>/dev/null || echo version-unknown)"
xargs -a "$LIST" -P "$P" -I{} "$0" --one {}
echo "batch end:   $(date '+%F %T')"
echo "cells with 'exit: pass': $(grep -c $'\texit: pass' "$OUT/batch.tsv" 2>/dev/null || echo 0) / $(wc -l < "$LIST")"
