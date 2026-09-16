#!/bin/bash
# run-lts-stability.sh — 在 openEuler LTS 容器镜像上做 SDCShield 全量用例 × 全严格选项稳定性验证。
#
# 用法:
#   ./run-lts-stability.sh <series> <sp> [--smoke]     单镜像验证
#     series: 24.03 | 22.03 | 20.03
#     sp    : LTS | SP1 | SP2 | SP3 | SP4 | all
#   --smoke  只跑 m01 短版(快速冒烟,验证脚本链路)
#
# 每镜像流程:
#   1) 镜像存在性检查(localhost/openeuler-offline:<tag>)
#   2) 容器内原生构建全功能二进制(复用 scripts/offline-build/container-build.sh,
#      追加 -Dssl_link_type=static → vendored openssl 3.5.0 静态进二进制,
#      ipsec46 + openssl_sha 全部编入;openblas/sleef/pocketfft 随 /src 挂载进容器,
#      由 third-party/*/install/ 产物提供,static 链接)
#   3) 选项矩阵(option-matrix.sh,28 个组合)逐条在纯净容器里跑
#      (只挂二进制 + 结果目录 + 随包运行时库 built/libs)
#   4) 判定:全部条目 PASS → "RESULT: PASS <tag>",任一失败 → RESULT: FAIL
#
# 环境变量:
#   T_BASE (默认 1000)  基线/全核条目每测试毫秒
#   T_SHORT(默认 500)   其余条目每测试毫秒
#   STEP_TIMEOUT(默认 1800) 单条目容器超时秒
#   SKIP_BUILD=1        复用 build-out/<tag>/sdcshield(已构建过)
#
# 输出: build-out/lts-stability/<tag>/{build.log,matrix.log,<id>.yaml}
#
# SPDX-License-Identifier: Apache-2.0
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OFFLINE_DIR="$SRC_ROOT/scripts/offline-build"

SERIES="${1:?usage: $0 <series> <sp> [--smoke]}"
SP="${2:?usage: $0 <series> <sp> [--smoke]}"
SMOKE=0
[ "${3:-}" = "--smoke" ] && SMOKE=1
case "$SP" in
    LTS|SP1|SP2|SP3|SP4|all) ;;
    *) echo "bad sp: $SP" >&2; exit 1 ;;
esac

T_BASE="${T_BASE:-1000}"
T_SHORT="${T_SHORT:-500}"
export T_BASE T_SHORT

source "$SCRIPT_DIR/option-matrix.sh"

run_one_sp() {
    local series="$1" sp="$2"
    local sp_dir sp_label
    case "$sp" in
        LTS)     sp_dir="LTS";    sp_label="LTS" ;;
        SP[1-4]) sp_dir="LTS_$sp"; sp_label="LTS-$sp" ;;
    esac
    local tag="openEuler-${series}${sp_dir}"
    local img="localhost/openeuler-offline:${series}-${sp_label}"
    local out_host="$SRC_ROOT/build-out/lts-stability/${tag}"
    local bin_host="$SRC_ROOT/build-out/${tag}/sdcshield"
    mkdir -p "$out_host"

    echo "==================== $tag ===================="

    # 1) 镜像检查
    if ! podman image inspect "$img" >/dev/null 2>&1; then
        echo "RESULT: FAIL $tag (image-missing $img)"; return 1
    fi

    # 2) 原生构建(全功能:SSL 静态链接)。稳定性验证语义下不复用旧产物:
    #    要验证的正是"当前 HEAD 在该 OS 的原生工具链下从头构建+全矩阵"。
    if [ "${SKIP_BUILD:-0}" != "1" ] || [ ! -x "$bin_host" ]; then
        echo "==> [build] $tag (ssl_link_type=static)"
        if ! "$OFFLINE_DIR/container-build.sh" "$series" "$sp" -Dssl_link_type=static \
                > "$out_host/build.log" 2>&1; then
            echo "RESULT: FAIL $tag (build) — 见 $out_host/build.log"
            tail -20 "$out_host/build.log" | sed 's/^/    | /'
            return 1
        fi
    else
        echo "==> [build] $tag 复用已有二进制 (SKIP_BUILD=1)"
    fi
    [ -x "$bin_host" ] || { echo "RESULT: FAIL $tag (no-binary)"; return 1; }

    # 挂载策略(与 offline-build/run-full-tests.sh 同源):
    #   24.03: host /usr/lib64 (host == 同 SP,glibc/gcc12 libstdc++ 匹配)
    #   22.03/20.03: 随包 built/libs(libstdc++/libatomic/libgcc toolset 兼容库)
    local libs_dir="$SRC_ROOT/third-party/rpms/openEuler-${series}/${tag}/built/libs"
    local mounts=(-v "$bin_host:/bin/sdcshield:ro,Z" -v "$out_host:/out:Z")
    local ld_path=""
    if [ "$series" = "24.03" ]; then
        mounts+=(-v /usr/lib64:/usr/lib64:ro)
    fi
    # 22.03/20.03: 缺 libatomic 自愈(与 run-full-tests.sh 相同,从 RPM 树提取)
    if { [ "$series" = "22.03" ] || [ "$series" = "20.03" ]; } \
            && [ ! -e "$libs_dir/libatomic.so.1" ]; then
        local rpmdir="$SRC_ROOT/third-party/rpms/openEuler-${series}/${tag}/rpms"
        local la_rpm
        la_rpm=$(ls "$rpmdir"/libatomic-*.rpm 2>/dev/null | head -1)
        if [ -n "$la_rpm" ]; then
            mkdir -p "$libs_dir"
            rm -rf /tmp/_la-stab && mkdir /tmp/_la-stab
            (cd /tmp/_la-stab && rpm2cpio "$la_rpm" | cpio -idm --quiet 2>/dev/null) \
                && cp /tmp/_la-stab/usr/lib64/libatomic.so* "$libs_dir/" 2>/dev/null
            rm -rf /tmp/_la-stab
        fi
    fi
    if [ -d "$libs_dir" ]; then
        mounts+=(-v "$libs_dir:/opt/built-libs:ro,Z"); ld_path="/opt/built-libs"
    fi

    # 构建产物核验:全功能二进制必须 ≥280 测试(ipsec46+openssl_sha 等编入标志)
    local ntests
    if [ -n "$ld_path" ]; then
        ntests=$(timeout 120 podman run --rm --user=0 "${mounts[@]}" \
            -e LD_LIBRARY_PATH="$ld_path" "$img" /bin/sdcshield --list-tests 2>/dev/null | wc -l)
    else
        ntests=$(timeout 120 podman run --rm --user=0 "${mounts[@]}" \
            "$img" /bin/sdcshield --list-tests 2>/dev/null | wc -l)
    fi
    ntests=${ntests:-0}
    echo "    list-tests: $ntests"
    if [ "$ntests" -lt 280 ]; then
        echo "RESULT: FAIL $tag (too-few-tests $ntests < 280 — SSL/vendored 未生效?)"
        return 1
    fi

    # 3) 选项矩阵
    local ids=("${MATRIX_IDS[@]}")
    [ "$SMOKE" = 1 ] && ids=(m01_full_baseline)

    local pass_n=0 fail_n=0 failed_ids=""
    local id rc verdict
    : > "$out_host/matrix.log"
    for id in "${ids[@]}"; do
        printf '  -- %s ... ' "$id"
        # 清理该条目历史 yaml(判定按文件名前缀统计,残留会误报)
        rm -f "$out_host/$id.yaml" "$out_host"/"$id"_*.yaml
        rc=0
        if [ -n "$ld_path" ]; then
            timeout "${STEP_TIMEOUT:-1800}" podman run --rm --user=0 "${mounts[@]}" \
                -v "$SCRIPT_DIR/option-matrix.sh:/matrix.sh:ro,Z" \
                -e LD_LIBRARY_PATH="$ld_path" \
                -e "T_BASE=$T_BASE" -e "T_SHORT=$T_SHORT" \
                "$img" bash -c '
                    mkdir -p /var/tmp /tmp 2>/dev/null
                    source /matrix.sh
                    BIN=/bin/sdcshield
                    OUT=/out
                    run_'"$id"'
                ' >> "$out_host/matrix.log" 2>&1
            rc=$?
        else
            timeout "${STEP_TIMEOUT:-1800}" podman run --rm --user=0 "${mounts[@]}" \
                -v "$SCRIPT_DIR/option-matrix.sh:/matrix.sh:ro,Z" \
                -e "T_BASE=$T_BASE" -e "T_SHORT=$T_SHORT" \
                "$img" bash -c '
                    mkdir -p /var/tmp /tmp 2>/dev/null
                    source /matrix.sh
                    BIN=/bin/sdcshield
                    OUT=/out
                    run_'"$id"'
                ' >> "$out_host/matrix.log" 2>&1
            rc=$?
        fi

        # 判定
        verdict=PASS
        # m 系条目:必须 exit 0;x 系(预期失败注入)允许非 0
        [ "$rc" -ne 0 ] && [ "${id:0:1}" = "m" ] && verdict=FAIL
        # 任何条目被 timeout 杀死(124)= 挂死,FAIL
        [ "$rc" -eq 124 ] && verdict=FAIL
        # 工具自身崩溃标志(heap corruption 等,不是测试预期注入的)
        if tail -100 "$out_host/matrix.log" | grep -qE 'double free|corruption detected|Aborted \(core dumped\)'; then
            verdict=FAIL
        fi
        # m 系条目还要求本条目 yaml 无 result: fail(条目输出文件已在本轮清空,
        # 出现 fail 即当前轮真实结果)
        if [ "$verdict" = PASS ] && [ "${id:0:1}" = "m" ]; then
            local y
            for y in "$out_host/$id.yaml" "$out_host"/"$id"_*.yaml; do
                [ -f "$y" ] || continue
                if grep -q 'result: fail' "$y" 2>/dev/null; then
                    verdict=FAIL
                    printf '[yaml-fail %s] ' "$(basename "$y")"
                    break
                fi
            done
        fi

        echo "$verdict (exit=$rc)"
        if [ "$verdict" = PASS ]; then
            pass_n=$((pass_n+1))
        else
            fail_n=$((fail_n+1)); failed_ids="$failed_ids $id"
        fi
    done

    # 4) yaml fail 扫描(独立于 exit code 的第二判定层:任何 yaml 出现
    #    result: fail 即软件 bug —— core179 已隔离,数值错误不是硬件噪声)
    # 注意:m 系条目已逐条判过 yaml fail;此处全量兜底扫描,但排除预期注入
    # 的 x 系条目输出(x24 是故意崩溃的 selftest)。
    local yfail_total=0 y
    for y in "$out_host"/*.yaml; do
        [ -f "$y" ] || continue
        case "$(basename "$y")" in x*.yaml) continue ;; esac
        local f
        f=$(grep -c 'result: fail' "$y" 2>/dev/null | tr -d '[:space:]')
        f=${f:-0}
        [ "$f" -gt 0 ] 2>/dev/null && { yfail_total=$((yfail_total+f)); echo "    yaml-fail: $(basename "$y") ($f)"; }
    done

    echo "    matrix: pass=$pass_n fail=$fail_n yaml_result_fail=$yfail_total"
    if [ "$fail_n" -gt 0 ] || [ "$yfail_total" -gt 0 ]; then
        echo "RESULT: FAIL $tag (matrix fail=$fail_n[$failed_ids ] yaml_fail=$yfail_total)"
        return 1
    fi
    echo "RESULT: PASS $tag (matrix $pass_n/${#ids[@]} yaml_fail=0)"
    return 0
}

# ── main ──
overall=0
if [ "$SP" = "all" ]; then
    for sp in LTS SP1 SP2 SP3 SP4; do
        run_one_sp "$SERIES" "$sp" || overall=1
    done
else
    run_one_sp "$SERIES" "$SP" || overall=1
fi
exit $overall
