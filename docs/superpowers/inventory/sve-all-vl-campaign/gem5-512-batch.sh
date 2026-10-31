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
#   - timeout 3600 兜底；断点续跑：$t.done = 该格 rc=0 已收口（PR #307 #10：
#     语义是「已通过」而非「已尝试」——失败/边界格不落 .done，幂等重跑会
#     重新执行它们；gem5 补 PMULL64/DC-CIVAC 编码后的 5 格重验正依赖此行为。
#     代价：4 个 SIM-SPEED 超时边界格每次重跑都会再烧 3600 s——若有意跳过，
#     手动 touch .done 并自担语义）。
#
# 产物（不入库，.gitignore 覆盖）：gem5-512/<t>.log（原始输出，含
#   run_sve.sh 的 G5+md5 身份行）、gem5-512/<t>.d/（m5out 工作目录）、
#   gem5-512/batch.tsv（每测试一行
#   <test>\trc=<n>\twall=<n>s\t<exit-line>\t<sim-exit-line>\tsrc=batch
#   ——末列为逐行溯源戳，PR #307 #25：无该列 = 手工并入或早于戳记的
#   历史行；gem5-512-extract.sh 的机械提取 + 溯源审计输入）、
#   gem5-512/identity.txt（批量时二进制身份：gem5 md5 + sdcshield 版本，
#   PR #307 #7——提取器头部以此为准，杜绝「提取时重算 md5 盖旧数据」）。
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
# Env-overridable for hermetic/off-host reuse (PR #307 #7): defaults are the
# 2026-10-29 campaign locations on the recording host.
LIST=${LIST:-"$HERE/sve-test-list.txt"}
OVR=${OVR:-"$HERE/gem5-overrides.txt"}   # 测试|额外参数（baked 26 之外的降容，标定产物）
OUT=${OUT:-"$HERE/gem5-512"}
G5=${G5:-/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt}
RUN=${RUN:-/home/sdc/wangxu/sdcshield-main/scripts/eigen-sve-double/gem5/run_sve.sh}
BIN=${BIN:-/home/sdc/wangxu/sdcshield-main/builddir/sdcshield}
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

# Binary identity record (PR #307 #7): written at BATCH TIME so that
# gem5-512-extract.sh stamps the verdicts header with the binary that
# PRODUCED the batch.tsv rows — never with whatever is on disk at extract
# time (a gem5 rebuild would silently mis-attribute old rows to a new md5).
# --one only creates the record when absent (standalone re-verification);
# a full batch also checks it and warns loudly on binary change — archive
# $OUT before mixing rows from different binaries.
write_identity() {
    printf 'gem5_md5\t%s\nsdcshield_version\t%s\nrecorded_at\t%s\n' \
        "$(md5sum "$G5" | cut -d' ' -f1)" \
        "$("$BIN" --version 2>/dev/null || echo unknown)" \
        "$(date '+%F %T')" > "$OUT/identity.txt"
}

if [ "${1:-}" = "--one" ]; then
    t="$2"
    if [ -f "$OUT/$t.done" ]; then
        # .done 现行语义 = rc=0 已收口。标记存在但 batch.tsv 末行非 rc=0
        # = 2026-10-30 前旧语义（已尝试）的遗留——删标记、重新执行。
        # 手动 touch .done 跳格仅在无矛盾行（lastrc 为空）时生效；失败行
        # 一律重跑（要强制跳过请归档/删除该行）。
        lastrc=$(awk -F'\t' -v t="$t" '$1 == t { last = $2 } END { print last }' \
                 "$OUT/batch.tsv" 2>/dev/null || true)
        if [ "$lastrc" = "rc=0" ] || [ -z "$lastrc" ]; then
            exit 0                           # 断点续跑：rc=0 已收口即跳过
        fi
        echo "NOTE: $t carries .done but its last batch.tsv row is $lastrc" >&2
        echo "      (stale marker, pre-2026-10-30 attempted-semantics) — re-running" >&2
        rm -f "$OUT/$t.done"
    fi
    [ -f "$OUT/identity.txt" ] || write_identity
    # overrides lookup: exact prefix match on "$t|" (PR #307 #13 — $t used to
    # be interpolated into a grep REGEX; a test name containing a regex
    # metacharacter could silently match a DECOY line first). awk index() is a
    # plain string comparison, no regex.
    extra=$(awk -v t="$t" 'index($0, t "|") == 1 { print substr($0, length(t) + 2); exit }' \
            "$OVR" 2>/dev/null || true)
    # extra args: intentional word-splitting WITHOUT glob expansion (PR #307
    # #13 — the old unquoted $extra also globbed against the cwd $OUT/$t.d,
    # so a "*" in an override value expanded to the files in that directory).
    read -r -a extra_args <<< "${extra:-}"
    mkdir -p "$OUT/$t.d"
    start=$(date +%s)
    # ${arr[@]+...} idiom (PR #307 #37): a bare "${extra_args[@]}" on an
    # EMPTY array is an "unbound variable" under bash < 4.4 + set -u (4.4
    # relaxed it; this host runs 5.2 so it never fired here). The idiom
    # expands to nothing for unset/empty arrays on ALL bash versions — the
    # off-host/hermetic reuse claim holds on 4.2/4.3 too.
    ( cd "$OUT/$t.d" && timeout 3600 env G5="$G5" "$RUN" "$BIN" \
        -e "$t" -t 500 -n 1 -f no --ignore-timeout \
        "${BAKED[@]}" ${extra_args[@]+"${extra_args[@]}"} --vl 4 ) > "$OUT/$t.log" 2>&1
    rc=$?
    end=$(date +%s)
    exitline=$(grep -a -m1 "^exit: " "$OUT/$t.log" || true)
    simexit=$(grep -a "Simulated exit code" "$OUT/$t.log" | tail -1)
    # 6th field src=batch (PR #307 #25): per-row provenance stamp — a row
    # without it is a manual/triage append or predates the stamp; the
    # extractor's provenance audit keys on this. (Legacy 5-field rows parse
    # unchanged: extra fields are ignored downstream.)
    printf '%s\trc=%s\twall=%ss\t%s\t%s\tsrc=batch\n' "$t" "$rc" "$((end-start))" \
        "${exitline:-<no-exit-line>}" "${simexit:-<no-sim-exit>}" \
        >> "$OUT/batch.tsv"
    printf '%s\trc=%s\twall=%ss\t%s\t%s\tsrc=batch\n' "$t" "$rc" "$((end-start))" \
        "${exitline:-<no-exit-line>}" "${simexit:-<no-sim-exit>}"
    # .done = 已通过（rc=0），非「已尝试」（PR #307 #10）：超时/崩溃格不
    # 标记，幂等重跑重新执行它们。batch.tsv 追加写、提取取末行，新行覆盖旧行。
    [ "$rc" -eq 0 ] && touch "$OUT/$t.done"
    exit 0
fi

# List contract (framework parse_test_list_line, PR #307 #21): blank and
# ^# lines are ignored and an optional "|time|default" suffix may follow the
# test name — feed xargs ONLY the effective names (a raw comment line would
# otherwise be run as a bogus -e argument and a suffixed name would miss
# every batch.tsv row).
awk '{ sub(/\r$/, ""); if ($0 == "" || $0 ~ /^#/) next; sub(/\|.*/, ""); print }' \
    "$LIST" > "$OUT/list.effective"

# Duplicate-name guard (PR #307 #19): two concurrent --one instances for the
# same test would share $OUT/$t.d and `>`-clobber $t.log — the exact "logs
# crossed, evidence void" failure triage-512 §1 recorded when two calibration
# instances collided. Fail BEFORE the batch burns hours.
dups=$(sort "$OUT/list.effective" | uniq -d)
if [ -n "$dups" ]; then
    echo "ERROR: duplicate test names in $LIST — concurrent --one instances would clobber each other's .d/.log (triage-512 §1 recorded failure mode):" >&2
    echo "$dups" >&2
    exit 2
fi

echo "batch start: $(date '+%F %T')  P=$P  list=$(wc -l < "$OUT/list.effective") tests"
new_md5=$(md5sum "$G5" | cut -d' ' -f1)
echo "gem5: $G5  md5=$new_md5"
echo "bin:  $BIN  $($BIN --version 2>/dev/null || echo version-unknown)"
if [ -f "$OUT/identity.txt" ]; then
    old_md5=$(awk -F'\t' '$1 == "gem5_md5" {print $2}' "$OUT/identity.txt")
    if [ "$old_md5" != "$new_md5" ]; then
        echo "WARNING: gem5 binary changed since the recorded batch identity" >&2
        echo "         (recorded md5 $old_md5, current $new_md5): batch.tsv would" >&2
        echo "         mix rows from different binaries — archive $OUT first." >&2
    fi
else
    write_identity
fi
# Stale .done audit (PR #307 #10): markers written before 2026-10-30 meant
# "attempted", so failed/boundary cells may carry one. The per-cell guard in
# --one re-runs such cells (stale marker + non-zero last row); this upfront
# listing makes the cost visible before the batch spends hours on them.
if [ -f "$OUT/batch.tsv" ]; then
    stale=$(awk -F'\t' '{ last[$1] = $2 } END { for (t in last) print t "\t" last[t] }' \
            "$OUT/batch.tsv" \
        | while IFS=$'\t' read -r t rc; do
              [ "$rc" = "rc=0" ] && continue
              [ -f "$OUT/$t.done" ] && echo "  $t (last $rc)"
          done)
    if [ -n "$stale" ]; then
        echo "WARNING: .done markers on non-green cells (pre-2026-10-30 attempted-" >&2
        echo "         semantics) — the guard below will RE-RUN these cells this batch:" >&2
        echo "$stale" >&2
    fi
fi
xargs -a "$OUT/list.effective" -P "$P" -I{} "$0" --one {}
echo "batch end:   $(date '+%F %T')"
echo "cells with 'exit: pass': $(grep -c $'\texit: pass' "$OUT/batch.tsv" 2>/dev/null || echo 0) / $(wc -l < "$OUT/list.effective")"
