# 2026-11-01 issue #410——eigen SVE complex 1024-bit 档启用（vendored Eigen 复数 ptranspose scatter 化）

> **For agentic workers:** 本计划是缺陷修复 + 功能启用 + 四档证据链。
> 执行按 CLAUDE.md rule 2 全量自验；每个 Task 一 commit。

## Goal

启用 `eigen_svd_cdouble_sve` 的 1024-bit 复数档：

1. **修复**：vendored Eigen SVE 复数 ptranspose（`Complex.h`）的
   `buf[N][NC]` 越界读（NC>N 时）——改写为 scatter 形状，任意 N/NC 界内；
2. **启用**：meson per-tier 构建加 `svd 1024` 档；分派器加 1024 入口；
3. **验证**：本机 256 档非回归 + gem5-fi 四档（128/256/512/1024）
   `--sve_vl {1,2,4,8}` 同源双 kernel 全绿，1024 为首次真实执行；
4. **收尾**：CLAUDE.md / meson 注释 / 分派器 SKIP 文案同步（rule 7）。

## 缺陷机制（读码实证，与 issue #410 预分析核对一致）

- `third-party/eigen5/Eigen/src/Core/arch/SVE/Complex.h:293-301`：ptranspose
  以 `buf[N][NC]` 行主序按 `buf[i][q]`（i<NC, q<N）读——**界内当且仅当
  NC<=N**（flat 上界 (NC-1)·NC+N-1 ≤ N·NC-1 ⟺ (NC-1)(NC-N) ≤ 0）。
- **唯一非方实例化点**：GEBP rhs 打包 cols4 尾
  `GeneralBlockPanelKernel.h:2977` 的
  `PacketBlock<Packet, (PacketSize % 4) == 0 ? 4 : PacketSize>`——
  complex 1024 时 PacketSize=NC=8 > N=4，越界读（GCC -Warray-bounds，
  PR #299 实证）。本计划独立复核了全 Eigen 源内 ptranspose 调用点清单：
  - rhs ColMajor 打包 cols8（PS=2/4/8）：全方阵（N=NC=PacketSize）；
  - rhs ColMajor 打包 cols4 尾（PS%4==0 才进）：PS=4 方阵、**PS=8 非方**；
  - lhs RowMajor 打包（`PacketBlock<Packet>`，N 默认= size）：全方阵；
  - rhs RowMajor 打包器：**无 ptranspose 调用**（issue 注意事项 1 的审计
    结论——无需改动）。
- 复数 128/256 档不进 cols4 向量分支（PS%4≠0 走标量尾），512 档恰方阵
  （N=NC=4），均安全。
- complex-complex gebp_traits 硬编码 `nr = 4`
  （GeneralBlockPanelKernel.h:722）⇒ `packet_cols8=0`，**所有复数
  ColMajor rhs 打包列组都走 cols4 循环**；1024 档每一组 4 列（深度对齐
  PS=8 时）都调用非方 ptranspose——48 维 blocked BDCSVD（双 kernel 的
  kDim=48，≥48 即 blocked 路径）大量执行该位点。

## 方案：复数 ptranspose scatter 化（实数 GEMM 四档全编的机械原因）

实数版（`PacketMath.h:983-996`）是 scatter：`buffer[i + j*N] = in[i].lane[j]`
散布 + 顺序读回 `out[p].lane[l] = buffer[p*NR + l]`——位置 x 有
`out[p].lane[l] = in[x mod N].lane[x div N]`（x = p*NR+l），任意 N/NR 界内
（x ↦ (x mod N, x div N) 是 [0, N·NR) 上的双射）。

复数版同构（complex 粒度）：`buffer[i + m*N] = in[i].complex[m]`，读回
`out[q].complex[m'] = buffer[q*NC + m']`，即

```
out[q][m'] = in[(q*NC + m') mod N][(q*NC + m') div N]
```

- **N=4, NC=8（1024 cols4 尾）**：`out[q][m'] = in[m' mod 4][2q + m' div 4]`
  ——与 issue 预推导逐字吻合。GEBP 消费端布局由标量尾锚定
  （`blockB[4u+c] = rhs(k0+u, col c)`，`GeneralBlockPanelKernel.h:2990-2996`）；
  scatter 版位置 `q*8+m' = 4(2q+m' div 4)+(m' mod 4)` 逐位吻合标量布局
  （`8p+m = 4·(2p + m div 4) + (m mod 4)`）。
- **方阵（N=NC，即 512/cols8/lhs 全部现有形状）**：化为
  `out[q][m'] = in[m'][q]`——真复数粒度转置，与现实现**逐位相同**
  （非回归的代数证明）。
- **512 档与标量尾的一致性**（现状正确的反证锚）：现实现位置 `4t+m ←
  rhs(k0+t, col m)` = 标量尾布局 `4u+c ← rhs(k0+u, col c)`——512 档现
  行为本就与消费端一致，scatter 版在 512 保持同一置换。

### 实现要点（Complex.h）

f64-lane 视图：packet 的 lane l（complex m = l>>1，re/im r = l&1）散布到
buffer 的 complex 槽 `i + m*N`，即 f64 绝对索引 `2*(i + m*N) + r`；基址
指针携带 `2*i`，故每 lane 索引 `sidx[l] = (l>>1)*2*N + (l&1)`（与 i 无关，
循环外构造一次）；读回 `out[q] = svld1_f64(buffer + 2*q*NC)`。复用
`svst1_scatter_u64index_f64`/`svld1_f64`（本文件 pgather/pscatter/ploaddup
同款原语），16 B complex 对以两次 f64 lane 传输。

## Tasks（每 Task 一 commit）

1. **vendored Eigen 补丁**：Complex.h ptranspose scatter 化
   （补丁层先例 be3b61c6/f0468f02/4f78a694/652117ed——直接改 vendored 源，
   commit message 详述根因/推导/验证）。验证：本机构建零新警告；
   **1024 档编译 `-Warray-bounds` 不再报**（越界的直接证据）。
2. **启用 1024 档**：`tests/cpu/meson.build` foreach 加 `['svd','1024']`
   + 注释更新；`svd_cdouble_sve.cpp` 分派器 6 处（头注释/命名空间/
   solve_once/init tier 验证/make_data/run 分派/cleanup/DECLARE_TEST 描述）。
   验证：本机 256 档 `-e eigen_svd_cdouble_sve -t 30 -O ...mdim=300` pass；
   `--list-tests` 不变（1024 档本机仍按 tier 门控 SKIP——本机 max VL=256）。
3. **gem5-fi 四档验证**（同源双 kernel，`/home/sdc/wangxu/gem5-fi/
   workloads/sdcshield/eigen_svd_cdouble_sve_dual.cpp`）：
   - 双 kernel 1024 stub 改为真 payload（镜像分派器变更，同源）；
   - 以**本 worktree 的 vendored Eigen**（含修复）四档重建
     （`-msve-vector-bits={128,256,512,1024}`）；
   - `arm_chaos.py --cpu O3 --max_faults 0 --sve_vl {1,2,4,8}` 四腿，
     gate = stdout `SDCSHIELD_FAILS=0` + checksum；
   - **512 档 checksum 须复现先例 `50ff99225080c21c`**（fc123cd2）——
     scatter 与现实现方阵逐位等价的实证（bit-identical 非回归证据）；
   - 1024 档为首次真实执行（核心交付）。
4. **回归 + 文档收尾**：`-e zstd19 -t 2000 -n 1` → pass；x86 目检
   （改动全在 `arch/SVE/` 层，x86 Eigen 路径零触碰）；CLAUDE.md
   "complex SVD payload has no 1024-bit variant" 表述更新。

## 验证矩阵

| 项 | 命令/方式 | 判据 |
|---|---|---|
| 构建 | `meson setup` + `SDC_BUILD_JOBS=20 ./scripts/build.sh` | 零新警告；svd vl1024 库编出且无 -Warray-bounds |
| 本机 256 | `-e eigen_svd_cdouble_sve -t 30 -O eigen_svd_cdouble_sve.mdim=300` | exit: pass |
| gem5 128/256/512/1024 | 双 kernel `--sve_vl 1/2/4/8` | 各档 `SDCSHIELD_FAILS=0` + checksum；512 复现先例值 |
| 回归 | `-e zstd19 -t 2000 -n 1` | exit: pass |
| x86 | 目检（aarch64 guard 内） | 零触碰 |

## 风险与边界

- Complex.h ptranspose 为所有复数 GEMM 消费者共享：方阵形状（512/
  cols8/lhs）行为由代数等价保证 + 512 checksum 复现实证；256 档本机
  真机跑（PS=2 不进 cols4 向量路径，走标量尾——本就不过 ptranspose）；
- gem5 1024 腿时长未知（512 双 kernel 先例可完成；真实二进制 512 是
  SIM-SPEED 边界——双 kernel kDim=48 远轻于 mdim=300 真测试）；腿设
  有界 timeout，超时如实报边界，不虚报；
- golden 语义：svd_cdouble 的 golden 是 per-thread same-source——四档
  `SDCSHIELD_FAILS=0` 即 golden 一致性证明（同矩阵两次求解 memcmp）。
