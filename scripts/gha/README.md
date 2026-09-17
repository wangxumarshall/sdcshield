# scripts/gha — GitHub Actions 多 OS 验证脚本

[`.github/workflows/multi-os-verify.yml`](../../.github/workflows/multi-os-verify.yml) 的配套脚本。
全部**纯 stdlib**（无 PyYAML / 无第三方依赖），因为三个系列的 openEuler 构建镜像 RPM 树都没有 PyYAML。

## 文件

| 文件 | 作用 |
|---|---|
| `verify-params.py` | 全量参数功能测试（`--quality=-1` 全覆盖、`-n 1/4/8` 三档、openblas `mdim` 扫谱、selftests 正负集），末行 `RESULT: PASS|FAIL` |
| `benchmark.sh` | 固定 `--max-test-loop-count` 采集跨 OS 基准 → `benchmark.tsv` |
| `benchmark.md` | 基准口径说明（为何固定 loop 数、诚实边界） |
| `benchmark-summary.py` | `report` job 汇总 15 个 `benchmark.tsv` 成跨 OS 对比表 |

## verify-params.py 验证矩阵

| 阶段 | 内容 | 判定 |
|---|---|---|
| 枚举 | `--list-tests` / `--quality=-1` / `--selftests` | 计数 >100 |
| all-quality(-n1) | `--quality=-1 -n 1 -t 5000` 全质量级跑一轮 | 0 fail 且 0 crash |
| all-PROD -n {1,4,8} | 多线程并发档，`--disable` eigen 数值类 | 0 fail 且 0 crash |
| selftest @positive | `--selftests --quick -e @positive` | 退出 0 |
| selftest 负面集 | 逐条 `selftest_*fail/abort/sig*` | 非零退出且非 insn 崩溃 |
| openblas mdim | `-O <id>.mdim={64,256,512}` × 3 gemm | 0 fail 且 0 crash |

**用法**：
```bash
# 容器内，镜像已装依赖 + 已 git clone + 已构建 builddir/sdcshield
python3 scripts/gha/verify-params.py --bin "$(pwd)/builddir/sdcshield" --out "$(pwd)/results"
# 快档
python3 scripts/gha/verify-params.py --bin ... --out ... --smoke
```

**已知省略（诚实，非缺陷）**：`sleef_neon`/`sleef_sve` 在无 cmake 的镜像里优雅缺席；`eigen` 数值类在
`-n>1` 档被 `--disable`（规避 CLAUDE.md 记录的 192 核 ULP flakiness），在 all-quality(-n1) 单线程档覆盖。

## benchmark.sh 用法

```bash
bash scripts/gha/benchmark.sh "$(pwd)/builddir/sdcshield" "$(pwd)/benchmark"
# → benchmark/benchmark.tsv: test<TAB>wall_seconds<TAB>loop_count
```

固定 `--max-test-loop-count`（同工作量墙钟对比），只取三系列交集测试。口径与边界见 `benchmark.md`。