#!/bin/bash
# package-built.sh — 在 CI 容器内把刚构建的 sdcshield 打成自包含 built/ 包。
#
# 这是 multi-os-verify.yml 原生容器模式的打包步骤:job 直接跑在
# ghcr.io/wangxumarshall/sdcshield-offline:<series>-<sp> 镜像里(与本地
# podman 链路同 image),所以不需要 package-built-artifacts.sh 的 podman
# 包装 —— ldd/strip 直接可用(15 镜像实测全有,20.03-LTS 无 GNU tar 用
# bsdtar 兜底)。
#
# 产物结构与本地 podman 链路的 third-party/rpms/.../built/ 一致:
#   sdcshield            (stripped 二进制)
#   libs/                (非系统自带的运行时 .so, 见 collect libs 规则)
#   run-sdcshield.sh     (设 LD_LIBRARY_PATH 后 exec; full 模式两段式 eigen)
#   MANIFEST.tsv         (文件 sha256 + 体积 + 来源)
#   VERSION              (git sha / series / cpp_std / binary-sha256 / build-hash)
#   BUILD-HASH           (与 build-all.sh compute_build_hash 公式完全一致)
# 外层再打一个 tar.gz 供 upload-artifact。
#
# libs 收集规则(对齐 package-built-artifacts.sh 的判定,GHA 容器形态):
#   20.03     二进制 RPATH 指向 /opt/openEuler/gcc-toolset-10/root/usr/lib64,
#             ldd 列出的 /opt/openEuler/* 库全拷(cp -L 解引用,soname 与
#             real file 两个名字都落盘)。
#   22.03/24.03 恒拷 libatomic.so.1(最小目标机缺,镜像 /usr/lib64/ 有,
#             与本地从 LTS RPM 提取同源)。
#   其余(libc/libm/libz/libzstd/libgmp/系统 libstdc++)视为目标机自带。
#   注意:本地 built/libs 的 libarm_compute*.so 是老方案(-Dacl_incdir 动态
#   链接)遗留死重,ACL 已改 vendored 静态 -l:libarm_compute-core.a,二进制
#   ldd 不依赖 —— 本脚本不拷,产物更小更干净。
#
# 用法: package-built.sh <builddir> <outdir> <series> <sp>
#   builddir : 含 sdcshield 二进制的构建目录(如 builddir/)
#   outdir   : 产物落盘目录(如 dist/openEuler-24.03LTS_SP3)
#   series   : 24.03 | 22.03 | 20.03
#   sp       : LTS | SP1..SP4
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

BUILDDIR="${1:?usage: $0 <builddir> <outdir> <series> <sp>}"
OUTDIR="${2:?usage: $0 <builddir> <outdir> <series> <sp>}"
SERIES="${3:?usage: $0 <builddir> <outdir> <series> <sp>}"
SP="${4:?usage: $0 <builddir> <outdir> <series> <sp>}"

# SP 归一化:workflow 矩阵传镜像 tag 后缀完整形式(如 "LTS-SP3"),CLI 惯例
# 是裸 "SP3";两者都接受,统一成裸形式再走 case。LTS 本体两种形式同形。
if [ "$SP" != "LTS" ]; then
    SP="${SP#LTS-}"
fi

case "$SP" in
    LTS)   SP_DIR="LTS";    SP_LABEL="LTS" ;;
    SP[1-4]) SP_DIR="LTS_$SP"; SP_LABEL="LTS-$SP" ;;
    *) echo "bad sp: $SP" >&2; exit 1 ;;
esac
case "$SERIES" in
    24.03) MACRO="";        CPPSTD="gnu++23" ;;
    22.03) MACRO="OPENEULER_22_03"; CPPSTD="gnu++20" ;;
    20.03) MACRO="OPENEULER_20_03"; CPPSTD="gnu++20" ;;
    *) echo "bad series: $SERIES" >&2; exit 1 ;;
esac

OS_TAG="openEuler-${SERIES}${SP_DIR}"
BIN="$BUILDDIR/sdcshield"
[ -x "$BIN" ] || { echo "ERROR: $BIN 不存在" >&2; exit 1; }

# 仓库根(脚本在 scripts/gha/ 下):BUILD-HASH 要 git ls-tree 源码树,
# run-sdcshield.sh 模板内嵌,不依赖仓内其它文件 —— 但 git ls-tree 需要
# 在仓内跑。GHA checkout 的工作目录即仓根,取脚本自身位置向上两级。
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 20.03-LTS 最小镜像无 GNU tar(只有 bsdtar),与 container-build.sh TAR_BIN 兜底同模式
TAR_BIN="$(command -v tar || command -v bsdtar || echo tar)"

mkdir -p "$OUTDIR/libs"
STAGE="$OUTDIR/$OS_TAG"
rm -rf "$STAGE"
mkdir -p "$STAGE/libs"

# ── 1) stripped 二进制 ──
echo "==> $OS_TAG: 拷贝 + strip 二进制"
cp "$BIN" "$STAGE/sdcshield"
# strip 失败不致命(保留未 strip 二进制也能跑),但 stderr 透传便于诊断
if ! strip --strip-debug --strip-unneeded "$STAGE/sdcshield" 2>&1; then
    echo "    ⚠ strip 失败(保留未 strip 二进制继续)" >&2
fi
chmod +x "$STAGE/sdcshield"

# ── 2) libs 收集(ldd 直跑;容器==构建容器,RPATH 可解析) ──
echo "==> $OS_TAG: 收集运行时依赖库"
rm -f "$STAGE/libs/"*.so*
copy_lib() {
    # 拷一个库文件:传入名 + real file 名都落盘(cp -L 解引用成硬拷贝)。
    # 例:传入 soname libatomic.so.1(symlink)→ real file
    # libatomic.so.1.2.0;两份都拷,目标机无论按哪个名字找都在。
    local path="$1"
    [ -f "$path" ] || return 0
    cp -L "$path" "$STAGE/libs/$(basename "$path")" || return 0
    local real
    real="$(readlink -f "$path" 2>/dev/null || echo "$path")"
    # real file 与传入名不同才补第二份(同文件两个名字,磁盘双份硬拷贝)
    if [ "$real" != "$path" ] && [ -f "$real" ]; then
        cp -L "$real" "$STAGE/libs/$(basename "$real")" || true
    fi
    return 0
}

# 20.03: toolset 运行库在 RPATH 里,ldd 输出 /opt/openEuler/... 全路径,全拷
while IFS= read -r lib; do
    [ -z "$lib" ] && continue
    case "$lib" in
        /opt/openEuler/*) copy_lib "$lib" ;;
        # 兜底:ACL 老方案动态链接残留(新方案静态链接,正常不出现;
        # ldd "=> not found" 时 $3 是 "not",[ -f ] 判 false 自然跳过)
        /usr/lib64/libarm_compute*.so*|/usr/lib64/libarm_compute_graph.so*) copy_lib "$lib" ;;
    esac
done < <(ldd "$BIN" 2>/dev/null | awk '/=>/ {print $3}' | grep '^/' | sort -u)

# 22.03/24.03: 最小 KIWI 目标容器缺 libatomic.so.1(镜像 /usr/lib64/ 有)
# —— 与本地链路从 LTS RPM 提取同源同效。20.03 的 libatomic 在 toolset
#    路径里,上面的 /opt 匹配已覆盖(若无 toolset 路径,系统 libatomic 也拷)。
if [ "$SERIES" != "20.03" ]; then
    for cand in /usr/lib64/libatomic.so.1 /lib64/libatomic.so.1; do
        [ -f "$cand" ] && copy_lib "$cand" && break
    done
fi
echo "    libs: $(ls "$STAGE/libs/" 2>/dev/null | wc -l) 个: $(ls "$STAGE/libs/" 2>/dev/null | tr '\n' ' ')"

# ── 3) run-sdcshield.sh(模板与 package-built-artifacts.sh 逐字一致) ──
cat > "$STAGE/run-sdcshield.sh" <<'RUN_EOF'
#!/bin/bash
# run-sdcshield.sh — 在目标机上运行随包的 sdcshield 二进制。
# 自动设置 LD_LIBRARY_PATH 指向随包 libs/ 目录。
#
# 用法:
#   ./run-sdcshield.sh [sdcshield 参数...]     原样透传给二进制
#   ./run-sdcshield.sh full [参数...]          全核满载 eigen 运算:
#     第一段: 11 个稳定 eigen 测试, 不带 -n (默认全部 CPU 满载)
#     第二段: 4 个数值敏感测试 (eigen_svd_double/eigen_sparse/
#             eigen_svd_cdouble/eigen_svd_cdouble_sve) -n 1 补跑,
#             避免大规模多线程下 ULP 级偶发假 FAIL (平台已知特性)
#   可透传 -t <time> 覆盖默认每测试 60s (如 full -t 120s);
#   透传的 -n 只作用于第一段, 第二段恒为 -n 1。
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# 20.03 的二进制 RPATH 指向 /opt/openEuler/gcc-toolset-10/root/usr/lib64;
# 若目标机没装 toolset, 上面的 libs/ 提供了同名库, LD_LIBRARY_PATH 优先于 RPATH。

if [ "$1" = "full" ]; then
    shift
    FULL_T="60s"
    # 用户自带 -t/--test-time 则不注入默认值
    for a in "$@"; do
        case "$a" in
            -t|--test-time|-t*|--test-time=*) FULL_T="" ;;
        esac
    done
    [ -n "$FULL_T" ] && set -- -t "$FULL_T" "$@"
    RC=0
    # 第一段: 11 个稳定 eigen 测试, 全核满载 (无 -n → 默认所有 CPU)
    "$SCRIPT_DIR/sdcshield" -e 'eigen*' \
        --disable eigen_svd_double --disable eigen_sparse \
        --disable eigen_svd_cdouble --disable eigen_svd_cdouble_sve \
        "$@" || RC=$?
    # 第二段: 数值敏感的 4 个测试, 单线程补跑 (-n 1 追加在后, 优先于透传 -n)
    "$SCRIPT_DIR/sdcshield" -e eigen_svd_double -e eigen_sparse \
        -e eigen_svd_cdouble -e eigen_svd_cdouble_sve \
        "$@" -n 1 || RC=$?
    exit "$RC"
fi

exec "$SCRIPT_DIR/sdcshield" "$@"
RUN_EOF
chmod +x "$STAGE/run-sdcshield.sh"

# ── 4) BUILD-HASH(公式与 build-all.sh compute_build_hash 完全一致) ──
# 源码树哈希 + container-build.sh 哈希 + cpp_std/macro + 镜像 input-hash + tag
# 注:各命令的 stderr 透传(不吞),CI 失败时日志里能看到是哪条 git/grep 炸了。
src_hash=$(git -C "$SRC_ROOT" ls-tree -r HEAD -- framework tests meson.build meson_options.txt 2>&1 | sha256sum | awk '{print $1}')
cb_hash=$(sha256sum "$SRC_ROOT/scripts/offline-build/container-build.sh" | awk '{print $1}')
img_hash=$(grep -P "^${SERIES}-${SP_LABEL}\t" "$SRC_ROOT/scripts/offline-build/images/image-manifest.tsv" 2>/dev/null | awk -F'\t' '{print $2}')
img_hash="${img_hash:-no-image}"
build_hash=$(printf '%s|%s|%s|%s|%s|%s-%s\n' "$src_hash" "$cb_hash" "$CPPSTD" "${MACRO:-none}" "$img_hash" "$SERIES" "$SP_LABEL" | sha256sum | awk '{print $1}')
echo "$build_hash" > "$STAGE/BUILD-HASH"

# ── 5) MANIFEST.tsv + VERSION ──
{
    echo "# MANIFEST.tsv — $OS_TAG 产物校验清单 (GHA CI build)"
    echo -e "file\tsha256\tsize\tsource"
    for f in sdcshield run-sdcshield.sh; do
        [ -f "$STAGE/$f" ] && printf '%s\t%s\t%s\t%s\n' "$f" "$(sha256sum "$STAGE/$f" | awk '{print $1}')" "$(stat -c%s "$STAGE/$f")" "built"
    done
    for f in "$STAGE"/libs/*; do
        [ -f "$f" ] || continue
        printf '%s\t%s\t%s\t%s\n' "libs/$(basename "$f")" "$(sha256sum "$f" | awk '{print $1}')" "$(stat -c%s "$f")" "bundled"
    done
} > "$STAGE/MANIFEST.tsv"

git_sha=$(git -C "$SRC_ROOT" rev-parse --short HEAD 2>/dev/null || echo "unknown")
bin_sha=$(sha256sum "$STAGE/sdcshield" | awk '{print $1}')
built_date=$(git -C "$SRC_ROOT" log -1 --format=%ci HEAD 2>/dev/null | cut -d' ' -f1)
cat > "$STAGE/VERSION" <<VERSION_EOF
sdcshield $OS_TAG
git: $git_sha
built: ${built_date:-unknown}
series: $SERIES  sp: $SP
cpp_std: $CPPSTD  macro: ${MACRO:-none}
image: ghcr.io/wangxumarshall/sdcshield-offline:${SERIES}-${SP_LABEL}
binary-sha256: ${bin_sha:0:64}
build-hash: ${build_hash:0:64}
built-by: github-actions multi-os-verify
VERSION_EOF

# ── 6) tar.gz ──
TARBALL="$OUTDIR/sdcshield-${OS_TAG}-${git_sha}.tar.gz"
if ! "$TAR_BIN" -czf "$TARBALL" -C "$OUTDIR" "$OS_TAG"; then
    echo "ERROR: tar 打包失败 ($TAR_BIN -czf $TARBALL)" >&2
    exit 1
fi
echo "==> 产出自包含 tarball: $TARBALL ($(du -h "$TARBALL" | awk '{print $1}'))"
echo "    含: sdcshield libs/ run-sdcshield.sh MANIFEST.tsv VERSION BUILD-HASH"
# 列内容仅作展示;head 提前关管道在 pipefail 下会 SIGPIPE 误杀,改用 sed 限量
"$TAR_BIN" -tzf "$TARBALL" 2>/dev/null | sed -n '1,12p' || true
