#!/bin/bash
# ============================================================
# SDC 全谱战役脚本（§7.4 战役协议的可执行化）
# 阶段 1：谱系广域扫（多样性 = 检出率）
#   1a GEMM 尺寸谱（cache 域逐层）
#   1b GEMM 形态谱（转置 × β 相位）
#   1c SLEEF 足迹谱（L1/L2/LLC 工作集）
#   1d FFT 因子谱（pow2 / Bluestein 质数 / 混合 radix）
#   1e 压缩 level 谱（四套 match-finder 数据结构）
#   1f 加密/哈希全家族 + LU + 混合多样性轮
# 阶段 2：全绿档固定 seed 深驻留（--max-test-loop-count=0
#          关 fracturing，CORE179 单模式持续暴露）
#
# 依据 docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md §7.4；
# 日志按阶段/档位分文件，保存在 ./sdc_spectrum_<时间戳>/。
#
# 用法：
#   bash scripts/run/run_sdc_spectrum.sh                   # 默认 15m/档扫 + 2h 驻留
#   SWEEP_TIME=30s DWELL_TIME=1m bash scripts/run/run_sdc_spectrum.sh   # 冒烟
# ============================================================

# 1. 配置参数（可 env 覆盖）
SDC="./builddir/sdcshield"                 # 可执行文件路径
LOG_DIR="./sdc_spectrum_$(date +%Y%m%d_%H%M%S)"   # 日志目录（带时间戳）
SWEEP_TIME="${SWEEP_TIME:-15m}"             # 阶段 1 每档时长
DWELL_TIME="${DWELL_TIME:-2h}"              # 阶段 2 驻留时长

# 2. 创建日志目录
mkdir -p "$LOG_DIR"

echo "========== SDC 全谱战役 =========="
echo "可执行文件: $SDC"
echo "日志目录:   $LOG_DIR"
echo "扫谱时长:   $SWEEP_TIME/档（env SWEEP_TIME 可覆盖）"
echo "驻留时长:   $DWELL_TIME（env DWELL_TIME 可覆盖）"
echo "启动时间:   $(date)"
echo ""

# ---------- 阶段 1a：GEMM 尺寸谱（cache 域逐层） ----------
echo "========== 阶段 1a：GEMM 尺寸谱（mdim 64/256/512/1024） =========="
for m in 64 256 512 1024; do
  echo "[$(date +%H:%M:%S)] mdim=$m → $LOG_DIR/gemm_m${m}.yaml"
  $SDC -O openblas_dgemm.mdim=$m -O openblas_sgemm.mdim=$m \
       -O openblas_zgemm.mdim=$m -O openblas_cgemm.mdim=$m \
       -e openblas_dgemm,openblas_sgemm,openblas_zgemm,openblas_cgemm \
       -t "$SWEEP_TIME" -o "$LOG_DIR/gemm_m${m}.yaml"
done

# ---------- 阶段 1b：GEMM 形态谱（转置 × β 相位） ----------
echo "========== 阶段 1b：GEMM 形态谱（transab 0..3，beta_permille=500） =========="
for tb in 0 1 2 3; do
  echo "[$(date +%H:%M:%S)] transab=$tb → $LOG_DIR/gemm_tb${tb}.yaml"
  $SDC -O openblas_dgemm.transab=$tb -O openblas_dgemm.beta_permille=500 \
       -e openblas_dgemm -t "$SWEEP_TIME" -o "$LOG_DIR/gemm_tb${tb}.yaml"
done

# ---------- 阶段 1c：SLEEF 足迹谱 ----------
echo "========== 阶段 1c：SLEEF 足迹谱（nelems 1024/16384/262144） =========="
for e in 1024 16384 262144; do
  echo "[$(date +%H:%M:%S)] nelems=$e → $LOG_DIR/sleef_e${e}.yaml"
  $SDC -O sleef_neon.nelems=$e -e sleef_neon -t "$SWEEP_TIME" -o "$LOG_DIR/sleef_e${e}.yaml"
done

# ---------- 阶段 1d：FFT 因子谱（pow2 / Bluestein 质数 / 混合 radix） ----------
echo "========== 阶段 1d：FFT 因子谱（n 4096/4099/6144/10000） =========="
for n in 4096 4099 6144 10000; do
  echo "[$(date +%H:%M:%S)] n=$n → $LOG_DIR/fft_n${n}.yaml"
  $SDC -O pocketfft_fft.n=$n -e pocketfft_fft -t "$SWEEP_TIME" -o "$LOG_DIR/fft_n${n}.yaml"
done

# ---------- 阶段 1e：压缩 level 谱 ----------
echo "========== 阶段 1e：压缩 level 谱（level 0..3） =========="
for L in 0 1 2 3; do
  echo "[$(date +%H:%M:%S)] level=$L → $LOG_DIR/igzip_L${L}.yaml"
  $SDC -O isal_igzip.level=$L -e isal_igzip -t "$SWEEP_TIME" -o "$LOG_DIR/igzip_L${L}.yaml"
done

# ---------- 阶段 1f：加密/哈希全家族 + LU + 混合多样性轮 ----------
echo "========== 阶段 1f：加密/哈希全家族 + LU + 混合多样性轮 =========="
echo "[$(date +%H:%M:%S)] crypto → $LOG_DIR/crypto.yaml"
$SDC -e openssl_sha,openssl_sha3,openssl_sm3sm4,isal_crc32_gzip \
     -t "$SWEEP_TIME" -o "$LOG_DIR/crypto.yaml"
echo "[$(date +%H:%M:%S)] lu → $LOG_DIR/lu.yaml"
$SDC -e openblas_lu -t "$SWEEP_TIME" -o "$LOG_DIR/lu.yaml"
echo "[$(date +%H:%M:%S)] mixed → $LOG_DIR/mixed.yaml"
$SDC -e openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip,openssl_sha3,zstd19 \
     -t "$SWEEP_TIME" -o "$LOG_DIR/mixed.yaml"

# ---------- 阶段 2：全绿档固定 seed 驻留 ----------
# --max-test-loop-count=0 关 fracturing：单模式持续暴露（CORE179 判别条件）
echo "========== 阶段 2：深驻留（$DWELL_TIME，fracturing 关闭） =========="
echo "[$(date +%H:%M:%S)] dwell → $LOG_DIR/dwell.yaml"
$SDC --max-test-loop-count=0 -e openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip \
     -t "$DWELL_TIME" -o "$LOG_DIR/dwell.yaml"

# ---------- 汇总 ----------
echo ""
echo "========== 战役完成 =========="
echo "结束时间: $(date)"

PASS_FILES=0
FAIL_FILES=0
for f in "$LOG_DIR"/*.yaml; do
  if grep -q "result: fail" "$f" 2>/dev/null; then
    FAIL_FILES=$((FAIL_FILES + 1))
    echo "FAIL: $f"
  elif grep -q "result: pass" "$f" 2>/dev/null; then
    PASS_FILES=$((PASS_FILES + 1))
  else
    echo "警告: $f 未找到 result 行"
  fi
done
echo "结果: $PASS_FILES 个日志全部 pass，$FAIL_FILES 个含 fail"
echo "详细日志请查看: $LOG_DIR/"
[ "$FAIL_FILES" -eq 0 ]
