# Plan: issue #369 — quick 跑 SIGXCPU 零余量（movbe_l2 / cache_stress_civac）

日期：2026-10-31 · 分支：fix/issue369-quick-sigxcpu · 状态：已完成（本地 A/B/C 验证 + 回归全绿，见「验证」节与两 commit message）

## 现象（issue #369）

pr.yaml `run tests quickly` 步骤（`ulimit -St 120` + `--quick -n4`，4 vCPU
Neoverse-N2 云 runner）偶发 SIGXCPU：run 41199739020 上 movbe_l2
（30952ms）与 cache_stress_civac（134829ms）crash code 24；同内容绿跑
41197455350 两测试 30492/30620ms pass。481 pass / 2 crash。

## 根因核实（全部读码/工件实证）

1. `--quick` 语义（framework/sandstone_opts.cpp:1151-1154）＝
   `max_test_loop_count=1` + `delay_between_tests=0`，**不动 -t**（默认
   1000ms）。TEST_LOOP(test, N) 的 N 是时间检查粒度（sandstone.h:138-148）：
   每 N 个 body 才查一次 deadline；quick 下首个检查点即停 ⇒ **quick 的
   test-runtime ≈ N × body**。
2. 两测试 body = 16384 元素全扫，每元素 `dc civac` + `dsb sy`
   （movbe_l1d_vs_l2.cpp:145 / cache_stress_civac.cpp:105，N=1<<13）⇒
   云 runner 实测检查点 ≈ 30s。
3. `ulimit -St`（RLIMIT_CPU）按**进程全部线程 CPU 总和**计；每个测试在
   fork 出的子进程里跑 4 worker 线程（各自重置计数）⇒ 单测试 CPU 需求 =
   4 × 30s ≈ **122s，正好压在 120s 熔丝上，余量为零**。绿跑压线过、争抢
   （cache_stress_civac 30.6s→134.8s 墙钟）即炸——被杀时 wanted CPU 仍
   ~122-135s（CPU 是工作量函数，steal 膨胀墙钟不膨胀 CPU）。
4. 绿跑工件全量分布（quick.yaml，499 测试）：top3 = sched_storm_hybrid
   33.2s（futex 睡眠型，CPU 远低于 4×墙钟，过线）、cache_stress_civac
   30.6s、movbe_l2 30.5s；第 4 名 movbe_l2_sve 8.2s（33s CPU）。即同批
   （PR #356, commit d7608995）至少两测试零余量、一测试贴线。
5. **这是 issue #360 TEST_LOOP 大粒度缺陷类的残部**：#360 审计标准是
   "自适应大缓冲使 body 变慢"，这三个是"固定缓冲但 body 因 civac/dsb 而
   慢"，N=1<<13 系照搬 movbe_dump 祖先（其 body 无 civac，检查点 <270ms）。
   #360 的修复 PR #363 的 CI 正是炸在这两个残部上。

## 选型论证

- **方向 1（逐测试缩时长）**：单独用＝打地鼠（issue 自己也只抓到 2/3）。
  但按 #347/#359/#360 既定模板（N 缩到检查点秒级 + 机理注释）修残部，
  是**缺陷层的正确修法**：quick CPU 122s→~0.06s，-t 契约恢复
  （框架 TargetLoopDuration=10ms，sandstone_p.h:213），热内环零改动。
- **方向 2（--disable 禁用）**：random_access_sweep/#183 先例是"未定位
  根因的临时排除"；本卡根因明确且已定位到缺陷行——禁用损失 PR 门禁对
  179 家族探针的全部快速覆盖，方向错误。
- **方向 3（提熔丝）**：结构性兜底。120 是 quick 测试普遍 <1s CPU 时代的
  定值；PR #356 后合法最大需求已到 ~122-133s。对齐 sanitizer.yaml 的
  **600**（同步骤形态——4 vCPU 云 runner、--quick -n4——仓库内已有
  eigen utime 599s 实测定值先例）。CPU 时间是工作量函数：争抢的 4× 墙钟
  膨胀**不需要** 4× CPU 余量，600 = 当今最大需求的 ~5 倍。副作用有界：
  挂死检测由框架每测试墙钟看门狗（max(5×-t+30s, 300s)，sandstone.cpp:179
  + wait_for_children SIGQUIT/SIGKILL）与 45min job 超时（实测 ~4min）双兜，
  4 线程满转挂死最坏 150s 墙钟触 SIGXCPU。
- **决定：1+3 组合**。残部按 #360 模板修缺陷（治本），熔丝 120→600 兜
  "下一个"（治类）。不纳入本卡的贴线项（movbe_l2_sve 8.2s 检查点、
  sched_storm_hybrid 33.2s 检查点/低 CPU）记入 #360 台账跟进。

## 修复（两 commit）

1. `test(misc/cache)`: movbe_l2 + cache_stress_civac 的 TEST_LOOP
   N: 1<<13 → 4，各加机理注释（body 流量算术、祖先 N 出处、实测检查点、
   热内环不动论证）+ 本 plan 文档。
2. `ci(pr)`: run tests quickly 的 `ulimit -St` 120 → 600 + 定值注释。

## 验证（已完成，128 核 Kunpeng 920 / SVE-256）

- 干净构建 532/532 零警告；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`。
- **A**（改前二进制，`ulimit -St 120`，quick -n4 两测试）：双双 crash
  code 24 'CPU time limit exceeded'——CI flake 本地复现（user 4m0.0s）。
- **B**（改前二进制，`ulimit -St 600`）：`exit: pass`，movbe_l2 28896ms /
  cache_stress_civac 30154ms——即每 child ~116/121s CPU，零余量算术坐实，
  方向 3 单独有效。
- **C**（改后二进制，`ulimit -St 120`）：`exit: pass`，总墙钟 0.096s
  （51.7ms / 21.6ms）——方向 1 单独有效，CPU 需求缩 ~2400 倍。
- 常规 `-t 2000 -n4`：每测试各片合计 2009/2007ms——-t 契约恢复
  （修复前单检查点即 ~29s）；`exit: pass`。
- 熔丝算术：修复后单测试 CPU 需求 ≪ 120（旧值亦过），600 = 结构性余量；
  PR CI quick workflow 真实跑绿（单次绿不证根治 flake，如实记录）。
