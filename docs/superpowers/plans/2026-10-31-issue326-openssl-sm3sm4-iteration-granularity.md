# Plan: issue #326 — openssl_sm3sm4 TEST_LOOP 迭代粒度过粗导致 3min 超时误报

日期: 2026-10-31 · 分支: worktree-agent-aca7192fff060a3d2 · issue: #326

## 1. 批判性分析结论（真实缺陷，修复）

### 1.1 TEST_LOOP 语义核实（framework/sandstone.h:138-148）

TEST_LOOP 是**时间循环**：body 持续执行直到框架要求停止；第二参数 N 是
**检查粒度**——每执行 N 次 body 才调用一次 `test_loop_condition(N)`
（framework/sandstone_run.cpp:633-640）→ `test_time_condition()` 检查
`wallclock_deadline_has_expired(current_test_endtime)`（即 -t 时长）。
**issue 标题"× 256 次迭代"表述不精确（不是总迭代数），但"框架只在
256 轮后才检查时间"完全准确。**

### 1.2 数据量核实（tests/cpu/openssl/openssl_sm3sm4.cpp:56-72）

自适应 `sdc_auto_linear_count(1, 4, 1.5, 600, threads, 1024, hi)`
（framework/sdc_stress_config.h:1134-1204）：
g002（128 GiB 物理内存 × 600‰ ÷ (4 arrays × 1.5 rss_ratio × 128 线程)）
= **102.4 MiB/线程，与 issue 精确吻合**（131072 MiB × 0.6 / 768 = 102.4）。

单次 body 内存流量 ≈ 13 × data_size（读 9×：SM4 加密×2 / SM3×2 / 解密
各 1、memcmp×2 各 2；写 4×：memset_random + 密文×2 + 解密）≈ 1.3 GiB/线程。
128 线程满核带宽竞争下，第一个时间检查点（256 body 后）需 3-4+ 分钟。

### 1.3 超时来源核实

- 战役调度器（/home/sdc/root-xupeng/sdcshield/farron/scheduler.py:53-59）:
  `timeout -k 30 <duration_ms/1000+15> ... -t <duration_ms>` —
  "-t 165s + 外部 timeout 180s" = issue 的 3min。
- 框架自身 SIGQUIT 宽限 = max(5×-t+30s, 300s)（framework/sandstone.cpp:
  179-190），≥5min，来不及介入。
- 结论：-t 的 wallclock deadline 因检查点间隔（256 × 单 body）> 时长而
  无法兑现，外部 timeout 先杀 → "Timed Out (3min)" 误报。

### 1.4 缺陷定性：迭代粒度与数据量演进失配（测试设计缺陷）

git 史：656bc064 初始 SM_DATA_SIZE=1024B 固定 + TEST_LOOP(test,256)
（沿用 openssl_sha.cpp:28 的 512B×256 模板，检查点 ~2.8 MiB 流量，
毫秒级，合理）；ae4a40e5/0140a3b5 把数据量自适应放大 5 个数量级
（1024B → 102.4 MiB，×104857）但粒度未动 → 检查点流量 ~280 GiB。
兄弟测试 openssl_sha.cpp:28 / openssl_sha3.cpp:43 均**固定 512B**，
同粒度无此问题——本测试是唯一"自适应大流量 + 256 粒度"组合。

不是环境配置问题：3min 对其它测试够用（issue 仅报此一个）；
数据量是自适应模型有意的压力设计，不应缩。

## 2. 修复选型

**TEST_LOOP(test, 256) → TEST_LOOP(test, 4)**（tests/cpu/openssl/
openssl_sm3sm4.cpp）。

实测修正的两个语义事实（分析过程中核实）：
- `-t` 单位是**毫秒**（CLAUDE.md：`-t 5000` = 5s）。
- fracture_loop_count=4 使 inner_loop_count（每 N body +1，
  sandstone_run.cpp:625）达到 4 时该段退出；fracture 段循环重入直到
  runtime ≥ current_test_duration（sandstone_run.cpp:1977-1987）。
  因此 N 同时决定 (a) 时间检查点间隔 = N × body；(b) 每段 body 数
  = 4 × N（段切换开销占比 = 0.45/(4N)）。

- 压力意图不变：总压测时长仍由 -t 主导（fracture 段循环重入到
  runtime ≥ -t）。golden 字节比对语义不动。
- 检查点间隔 = 4 body：issue 场景（g002，body ~1-3s）4-12s << 165s；
  本机全核（48.8MiB/线程，实测 body ~0.9s）~3.6s。
- 段 body 数 = 16：段切换（per-thread 4×malloc + free + re-roll
  ≈ 0.45 body）占比 ~3%，不稀释压力；N=1 则段=4 body、切换占比
  ~11%。
- 开销：test_time_condition O(1)（~几十 ns），相对毫秒级以上 body
  可忽略；小内存机器（data_size 下限 1024B，body ~60μs/MiB 量级）
  占比可忽略。
- N=4 是 2 的幂（宏注释惯例）；仓库先例有 1/32/64/128/256 多种。
- 不选缩数据量（损害自适应压力设计）；不选 body 内部分片（打断
  golden 往返语义）。
- x86 非回归：改动在 #if SANDSTONE_SSL_BUILD 内无 arch 分叉，
  sdc_auto_linear_count 在 x86 上同一公式（无 arch 分支），大数据量
  下同样受益，属同向超时修复，无行为分叉。

## 3. 验证记录（实测，本机 128 核/61GB/aarch64，vendored OpenSSL 3.5.0）

1. 构建：SDC_BUILD_JOBS=20 ./scripts/build.sh，461/461 零警告。
2. 修复前基线：`-e openssl_sm3sm4 -t 2 -n 8`（-t 单位 ms）——45s
   后被外部 timeout 杀，yaml `exit: interrupted`；自适应 data_size
   = 780MiB/线程（-n 8 分摊），body ~8.5s，第一个检查点（256 body）
   ≈ 36 分钟，45s 只走了 2%。
3. 修复后 `-t 2 -n 8` 同规模：43s 自行退出，`exit: pass`（init ~39s
   + 第一个检查点 4 body 发现超时退出）。
4. 修复后全核 `-t 30000`（30s，128 线程，data_size 48.8MiB/线程）：
   test-runtime 33.7s `pass`（-t + 1 检查点 ~3.6s）；yaml
   `timing: { duration: 30000, timeout: 300000 }` 验证 test_timeout
   = max(5×-t+30, 300)。
5. g002 模拟（64 线程 × env data_size=100MiB，body 尺度等价
   102.4MiB）：`-t 20000` → 30.5s `pass`。
6. 粒度标定（-n 1，固定 data_size）：1MiB body ~6ms、16MiB ~60ms
   （fracture 16 body 下限主导，109ms/967ms 干净退出）——线性
   ~60ms/MiB。
7. RSS：全核 30s 期间单进程采样峰值 6.5GiB（采样法局限=下界；
   模型口径 24.4GiB 工作集 = 4×48.8MiB×128）。
8. 回归：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
9. x86 目检：改动无 arch 分叉，跨架构同向修复。
10. 无 SVE/SME tier 依赖，本机完整可验证，无需 gem5-fi。

## 4. 交付

单 commit（`git commit -s`）→ push worktree 分支 → PR（正文含根因/
修法/证据/Fixes #326）→ code-reviewer 子 agent 检视 → merge → issue 关闭。
