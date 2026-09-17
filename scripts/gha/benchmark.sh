#!/bin/bash
# benchmark.sh — 在 openEuler 容器内采集跨 OS 基准(固定 loop 数,同工作量墙钟对比)。
#
# 背景/口径见 scripts/gha/benchmark.md。这里只做采集:
#   bash benchmark.sh <bin> <outdir>
#     <bin>    容器内 sdcshield 绝对路径(如 /__w/.../builddir/sdcshield)
#     <outdir> 输出目录(挂载回 host)
# 产出:<outdir>/benchmark.tsv,列: test<TAB>wall_seconds<TAB>loop_count
#
# 设计:共享 CI runner 跑不了确定性 assert(云实例非用户实机);固定墙钟在不同 OS 上会跑到
# 不同迭代数,不可比。改用 --max-test-loop-count N 让每个 OS 每个测试跑恰好 N 次 main loop,
# 工作量一致,只比"完成同样工作量的墙钟"。结果口径是跨 openEuler 版本的相对近似值。
#
# SPDX-License-Identifier: Apache-2.0
set -uo pipefail

BIN="${1:?usage: benchmark.sh <bin> <outdir>}"
OUTDIR="${2:?usage: benchmark.sh <bin> <outdir>}"
mkdir -p "$OUTDIR"

# 跨三系列交集测试(20.03/22.03/24.03 皆有;sleef/isal 是 24.03 独占,不进主表避免 N/A 泛滥)。
# 选默认压缩档 zstd/zlib(单 loop ~80s/~177s),而非最高档 zstd19/zlib1(700s+/168s+),保证 bounded。
BENCH_TESTS=(
  openblas_dgemm openblas_sgemm zstd zlib fma crc32 pocketfft_fft \
  memcpy_l2_cache_size eigen_gemm_double_dynamic_square gmp_bignum openssl_sha
)
LONG_LOOP=1      # 单 loop 较长的测试(zstd/zlib 压缩往返)降为 1 次
DEFAULT_LOOP=2   # 其余 2 次(openblas 单 loop ~54s → 2 loop ~2min)

OUT="$OUTDIR/benchmark.tsv"
: > "$OUT"
printf 'test\twall_seconds\tloop_count\n' >> "$OUT"

# 先取一次 test 列表,避免每个测试都重新枚举
LIST="$("$BIN" --list-tests 2>/dev/null)"

for t in "${BENCH_TESTS[@]}"; do
  if ! printf '%s\n' "$LIST" | grep -qx "$t"; then
    printf '%s\tskip\t0\n' "$t" >> "$OUT"
    continue
  fi
  LOOP=$DEFAULT_LOOP
  case "$t" in
    zstd|zlib) LOOP=$LONG_LOOP ;;
  esac
  YAML="$OUTDIR/$t.yaml"
  rm -f "$YAML"
  "$BIN" --max-test-loop-count "$LOOP" --retest-on-failure=0 \
          -n 1 -e "$t" -o "$YAML" >/dev/null 2>&1 || true
  # test-runtime(秒)= 完成 LOOP 次 loop 的墙钟
  RT=$(grep -E '^  test-runtime:' "$YAML" 2>/dev/null | head -1 | awk '{print $2}')
  RT=${RT:-skip}
  printf '%s\t%s\t%s\n' "$t" "$RT" "$LOOP" >> "$OUT"
done

echo "benchmark written: $OUT"
cat "$OUT"