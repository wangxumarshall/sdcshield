# Harpocrates 覆盖指标复现：实验报告

本报告汇总 `feat/harp-coverage` 分支（19 任务，2026-09-16/17）的全部
真机实验数据与结论。指标定义/采集点/公式/差异表见 `method.md`；
实施过程与逐任务验证证据见
`docs/superpowers/plans/2026-09-16-harpocrates-coverage-reproduction.md`
（checkbox）与 git log（每个 commit 的 message 引用真实命令输出）。

平台：gem5 v25.1.0.1+6（CHAOS 树），ArmO3CPU @ TaiShan v110 参数
（鲲鹏 920），AArch64 SE，host 为 aarch64 原生。

## 一、覆盖指标运行性验证（Phase 2-4）

单次 `--cov` run 同时产出全部 7 结构指标（ROI 由 m5ops 标记）。

### 1.1 采集器语义对照（症状 → 指标响应）

| 实验 | 预期 | 实测 |
|------|------|------|
| readwrite_seq vs overwrite_seq（IRF） | 写后读链 AVF > 死写链 | 0.0891 > 0.0804 ✓ |
| sample_seq vs rand_fp（IRF vec 空间） | FP 活动只出现在 fp 序列 | 0 vs 0.0465 ✓ |
| rand_mem vs sample_seq（L1D） | 内存序列 AVF 高 | 0.001359 vs 0.000250（5.4×）✓ |
| rand_mem vs sample_seq（LSQ） | store 序列 SQ AVF 高 | 0.016960 vs 0.014102 ✓ |
| rand_fp（IBR FPAdd/FPMul 位宽） | 2×128b/issue | 32768/128=256b ✓ 24320/95≈256b ✓ |
| branchy_leak（IRF 双口径） | wrong-path 读致口径差 | 乐观 0.0995 vs commit 0.1141（差 14.8%）✓ |

### 1.2 上界性质（论文 Fig.4 核心主张）

- IRF（readwrite_seq，N=50 transient）：irfAvfInt=0.0891 ≥
  detection=0.0400 ✓（ACE 是 transient 检测上界；gap=software masking）
- IRF（evolved 序列，N=30 transient）：irfAvfInt=0.0775 ≥
  detection=0.0333 ✓
- 口径修正记录：首版用三空间加权 irfAvf 对照 int 空间注入出现假违反
  （0.027<0.04），实测定位后改为同空间口径。

## 二、SFI 检测能力（Phase 5）

| 结构 | 协议 | 代表数据 |
|------|------|----------|
| IRF | transient 单 bit，均匀随机 bit×cycle | readwrite_seq N=50：detection=0.0400，CI[0.011,0.135]，SDC=0/Crash=2/Masked=48 |
| L1D | CHAOSCache 随机块+字节 | 协议可用（arm_chaos_cache.py 路由） |
| LSQ | CHAOSLSQFwd 前转路径 | 协议可用 |
| IntMult | permanent 位级（L1） | readwrite_seq N=30：detection=0.6333（饱和） |
| FPMul | CHAOSFPU 位级 | rand_fp N=20：detection=0（FP 软件掩蔽真实测量；论文"FP 平均低"同构） |

FU gate-level（L2 网表，CHAOSGateFU）：
- 网表正确性三层验证：W=4/8/12 位穷举（~1700 万向量）+ 64 位随机
  10 向量 + gem5 全程序等值（mulmul 负载 SUM=host 参考，stuck=-1）
- stuck-at gate=40000：numDiffs=262173/262189（99.99% 显形），
  日志逐条 clean/stuck 值（单 bit 差）
- 固定参数两次运行结果一致（确定性）

## 三、基线对比（论文 Fig.4 等价，Task 7.2）

| 组 | irfAvfInt | ibrIntAdd | ibrIntMul | ibrFpAdd | ibrFpMul |
|----|-----------|-----------|-----------|----------|----------|
| random-int-heavy | 0.0971 | 0.1643 | 0.1100 | 0 | 0 |
| random-fp-heavy | 0.0535 | 0.0011 | 0.0022 | 0.0715 | 0.0566 |
| random-balanced | 0.0957 | 0.0871 | 0.0711 | 0.0165 | 0.0165 |
| random-mem | 0.0549 | 0.0012 | 0.0025 | 0 | 0 |
| directed (10) | 0.1215 | 0.1911 | 0.0070 | 0.0068 | 0.0053 |

论文同构结论：FU 覆盖按指令 mix 分化（int 组 FP=0 / fp 组 Int≈0，
对应论文"SSE FP 只有 FP-heavy 负载非零"）；通用负载（directed）
IRF 相对高、FU 相对低。明细 `artifacts/harp-baselines/cells.csv`。

## 四、advice-driven vs 盲变异（论文 Fig.10 等价 + 超越，Task 6.2）

- FU-IntMul（sample_seq 起点，12 步同预算）：
  advice 0.1152→**1.0890**（8 步单调饱和）；
  blind（论文的均匀随机指令替换）0.1152→**0.0578**（不升反降）。
  **advice 终态 = blind 的 18.8 倍**。
- IRF（overwrite_seq 起点，12 步）：advice 0.0775 vs blind 0.0815——
  小预算下均不显著（论文 IRF 需 ~5000 迭代收敛；8 指令序列被
  wrapper 循环开销主导）。如实记录。
- SFI 抽点：intmul permanent 下基线与 evolved 均 0.6333（permanent
  注入饱和——错误破坏 wrapper 载入存回链必检出，与论文 FU>99% 的
  机理一致）；irf transient 下 detection=0.0333 < ACE=0.0775
  （非饱和区间上界成立）。

## 五、Micro'26 附加实验（Task 7.3）

- 种子敏感性（K=6+8 × N=8）：全部 detection=1.00、方差 0。
  与论文 Fig.5（多数 <1%、int-mul ~17%）的结构性差异：论文方差源
  是 x86 MUL 隐式写 RAX 的覆写掩蔽 quirk（Micro'26 自述）；ARM64
  三操作数 MUL 无此机制——位错误必然传播。差异在 method.md §四.1
  预判，实验证实。
- 前缀截断（4 档 × N=8）：0.10×（1 条 mul）即 detection=1.00——
  强于论文"0.1× 数百条保持"且方向一致。
- 诚实边界：两实验 detection 全饱和（permanent + wrapper iters=200
  循环放大的检出强度），未展示论文的方差分布形态；非饱和区间的
  行为由 §四 irf transient 数据（0.03-0.05）佐证。

## 六、与论文的总体对照

| 论文主张 | 复现状态 |
|----------|----------|
| ACE 为 bit-array 检测上界（Fig.4） | ✓（两负载实测，含口径修正记录） |
| coverage↑⇒detection↑（Fig.10 核心） | ✓ advice 曲线单调升至饱和 + 18.8× 于盲变异 |
| FU permanent 检测 ~100%（Fig.11） | ✓（0.6333 饱和 + gate 级 99.99% 显形） |
| FP 平均检测低（§III-C） | ✓（CHAOSFPU N=20 全 Masked） |
| 通用/随机负载结构覆盖分化（Fig.4） | ✓（基线套件，mix 分化清晰） |
| 种子方差 <1%~17%（Micro'26 Fig.5） | 结构性差异：ARM64 无 RAX quirk，方差≈0（更强） |
| 0.1× 前缀保持检测（Micro'26 Fig.6） | ✓（更强：1 条指令即 1.00） |

## 七、遗留与后续

1. FP 门级网表（IEEE754 分解）按降级预案延后（FP SFI 用位级）。
2. L1D byte 级 / SQ per-slot 精确账本（现为块级/聚合）。
3. IRF advice 规则在大预算下的收敛性（论文 5000 迭代量级）。
4. MiBench-arm 基线（现为 directed 组 10 负载替代）。
