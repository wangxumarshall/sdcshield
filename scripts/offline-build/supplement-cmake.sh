#!/bin/bash
# supplement-cmake.sh — 为 22.03/20.03 各 SP 的 RPM 子目录补充 cmake(ACL 容器内重建需要)。
#
# 背景: vendored ACL (third-party/acl, v23.02) 用 CMake 构建, 要求 cmake>=3.13。
# 24.03 容器镜像自带 cmake 3.2x, 但 22.03/20.03 离线镜像和 RPM 树都没有 cmake
# (与 sleef 的先例相同)。为了让 22.03/20.03 也能容器内原生重建 ACL(三 OS 全覆盖),
# 从各系列自己的 repo 下载该系列原版 cmake RPM 全家桶(cmake 主包 + data/filesystem/
# rpm-macros)到对应 rpms 子目录。容器内 container-build.sh 检测到 /rpms 里的
# cmake-*.rpm 后自愈安装(rpm -Uvh --nodeps), 随后 ACL 的重建逻辑即可走通。
#
# 依赖链(实测 rpm -qpR): libarchive/libcurl/libexpat/libjsoncpp/libuv/librhash —
# 均为各镜像基础包, --nodeps 安装即可用。
#
# 用法: ./supplement-cmake.sh [22.03|20.03|all]
#   默认 all: 处理 22.03 与 20.03 全部 SP 子目录(24.03 不需要)。
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 各系列的 repo 与 cmake 版本(2026-09-19 实查):
#   22.03 各 SP: cmake-3.22.0-9.oe2203sp{N}  (repo.openeuler.org/openEuler-22.03-LTS-SP{N})
#   20.03 各 SP: cmake-3.16.5-5.oe2003sp{N}  (repo.openeuler.org/openEuler-20.03-LTS-SP{N})
declare -A CMAKE_VER=(
    ["22.03"]="cmake-3.22.0-9"
    ["20.03"]="cmake-3.16.5-5"
)
declare -A CMAKE_SUBPKGS=(
    ["22.03"]="cmake-data cmake-filesystem cmake-rpm-macros"
    ["20.03"]="cmake-data cmake-filesystem cmake-rpm-macros"
)

download_one() {
    local url="$1" dest_dir="$2" pkg="$3"
    local out="$dest_dir/$pkg"
    if [ -f "$out" ] && [ "$(stat -c%s "$out" 2>/dev/null || echo 0)" -gt 1000 ]; then
        echo "  ✓ $pkg (已存在)"
        return 0
    fi
    echo "  ↓ $pkg"
    if ! curl -sL "$url/$pkg" -o "$out" 2>/dev/null; then
        echo "    FAILED 下载 $pkg" >&2
        rm -f "$out"
        return 1
    fi
    if file "$out" 2>/dev/null | grep -qi "HTML"; then
        echo "    错误: $pkg 下载得到的是 HTML(404?), 跳过" >&2
        rm -f "$out"
        return 1
    fi
    return 0
}

supplement_series() {
    local series="$1"
    local base="third-party/rpms/openEuler-$series"
    local ver="${CMAKE_VER[$series]}"
    local sps
    sps=$(ls "$base" 2>/dev/null | grep -E "^SP[0-9]+$|^LTS" || true)
    if [ -z "$sps" ]; then
        echo "警告: $base 下无 SP 子目录(rpms 子模块未 init?), 跳过 $series" >&2
        return 0
    fi
    for sp in $sps; do
        # repo tag 形如 openEuler-22.03-LTS-SP1 / openEuler-20.03-LTS-SP4 / LTS 本体;
        # RPM 文件名 release 段为 oe2203spN / oe2003spN 小写无点(实测 repo 列表)
        local repotag="openEuler-$series-LTS-$sp"
        [ "$sp" = "LTS" ] && repotag="openEuler-$series-LTS"
        local rel
        rel=$(echo "$repotag" | sed -E 's/openEuler-([0-9]+)\.([0-9]+)-LTS(-SP([0-9]+))?/oe\1\2sp\4/' | tr -d '-')
        # oe2203sp4 形态; LTS 本体无 SP 时 sed 已产出 oe2203sp (尾空) — 剥掉尾 sp
        local rel2="${rel%sp}"
        local url="https://repo.openeuler.org/$repotag/everything/aarch64/Packages"
        local dir="$base/$sp"
        # 动态解析该 repo 的实际 cmake 版本(release 段各 SP 不同, 如 22.03 LTS
        # 是 3.22.0-4 而 SP4 是 3.22.0-9); 解析失败回退到本系列已知版本
        local ver_live
        ver_live=$(curl -sL "$url/" 2>/dev/null | grep -oE '"cmake-[0-9.]+-[0-9]+\.'"$rel2"'\.aarch64\.rpm"' | head -1 | tr -d '"' | sed 's/\.aarch64\.rpm//')
        [ -z "$ver_live" ] && ver_live="$ver.$rel2"
        # 下载目标: rpms 树顶层(与 findutils 等同级)。container-build.sh 的
        # [1/5] 段 KEEP 强装(find "$RPMDIR" -maxdepth 1)会自动把 cmake 全家桶
        # 连同 jsoncpp 依赖一起装进容器 — 不需要单独的自装逻辑。
        # 22.03 的 cmake 主二进制额外依赖 libjsoncpp.so.25(20.03 不需要), 从
        # 同 repo 一并下载 jsoncpp 主包。
        local top="$base"
        echo "==> $series $sp  (release: $rel2, cmake: ${ver_live#cmake-} → 顶层)"
        download_one "$url" "$top" "$ver_live.aarch64.rpm" || true
        # 子包与依赖的 release 段/arch 各 repo 不一(cmake-filesystem 在 22.03-LTS
        # 是 aarch64, jsoncpp release 是 -3 而非 -5) — 一律从 repo 列表页动态解析,
        # 解析不到则跳过(容器内 KEEP 强装容忍缺个别子包)
        for sub in ${CMAKE_SUBPKGS[$series]} jsoncpp; do
            subfile=$(curl -sL "$url/" 2>/dev/null | grep -oE '"'"$sub"'-[0-9][^"]*\.(aarch64|noarch)\.rpm"' | grep -vE 'devel|help|gui' | head -1 | tr -d '"')
            [ -n "$subfile" ] && download_one "$url" "$top" "$subfile" || true
        done
        
    done
}

case "${1:-all}" in
    22.03) supplement_series 22.03 ;;
    20.03) supplement_series 20.03 ;;
    all)
        supplement_series 22.03
        supplement_series 20.03
        ;;
    *) echo "用法: $0 [22.03|20.03|all]"; exit 1 ;;
esac
echo "==> 完成。容器构建时 container-build.sh 会检测 /rpms/cmake-*.rpm 自愈安装。"
