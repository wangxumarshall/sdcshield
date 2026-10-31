# Plan: issue #349 — agu_stress_2src_sve / agu_stress_store_only_sve TEST_LOOP 粒度 1<<13 × 自适应大缓冲 → 超时误报

日期: 2026-10-31 · 分支: worktree-agent-aef475cbb43767e72 · issue: #349 · 范本: #326 / PR #347

## 1. 批判性分析（真实缺陷，与 #326 同构，修）

### 1.1 语义（framework/sandstone.h:138-148 + sandstone_run.cpp:633-640）

TEST_LOOP 第二参数 N = 时间检查粒度（每 N 次 body 查一次 -t deadline）。
默认 -t = 1 s（sandstone_p.h:364）；框架 SIGQUIT 宽限 = max(5×-t+30s, 300s)
（sandstone.cpp:179-190）。两测试未设 fracture_loop_count → 自动 fracture
40 次检查/段（sandstone_run.cpp:1928-1931）→ 段 = 40×N body。

### 1.2 流量定标（本机 128 核 / 61 GiB / SVE1 256-bit）

count = sdc_auto_linear_count(8, 5, 1.0, 600, threads, 1024, 32M)
（sdc_stress_config.h:1134-1204）= 物理内存 600‰ ÷ (8B × 5 缓冲 × 线程数)：

- 全核 128 线程: count = 7,696,569（实测 [压力映射] 精确吻合）= 58.5 MiB/缓冲
- -n 8: 需 123M → 触 hi 上限 32M = 256 MiB/缓冲（实测 [欠压力] 告警）

每 body 流量（全缓冲扫描 + golden memcmp）:
- 2src: 2 ldr z 源读 + str z→ldr z→str z 放大（temp 写/读 + dst 写）+
  memcmp（dst+expected 读）= **7 × count×8 B**/线程
  = 全核 410 MiB/body/线程（全机 52 GiB/body）；-n 8 档 1.75 GiB/body/线程
- store_only: 2 读 + 2 写 + memcmp 2 读 = **6 × count×8 B**
  = 全核 351 MiB；-n 8 档 1.5 GiB

实测 body 耗时（修复后 -t 2ms 自行退出的超调反推）:
- 全核: ~1.05 s/body（-t 30 s → test-runtime 34.1-34.3 s，超调 ≈ 4 body）
- -n 8: ~1.1 s/body 无争用；共 agent 抢带宽时观测到 21.7 s/body
  （test-runtime 86.8 s / 4 body——带宽敏感，量级证据）

### 1.3 缺陷定性

N = 1<<13 = 8192 沿用自标量祖先（agu_stress_2src.cpp:20 固定
AGU_STRESS_2SRC_COUNT=1024 元素 = 8 KiB/缓冲，检查点 ~448 MiB 流量/
线程、亚秒级，合理）；_sve 移植换成自适应 sizing 后粒度未动 →
第一个检查点 = 8192 × body ≈ **2.3 小时**（无争用）~ **2 天**（重争用）。
任何 -t（含默认 1 s、战役 165 s）都无法兑现 → Timed Out 误报。

### 1.4 实测确认（修复前）

- 2src: `-t 2 -n 8` 外部 120 s 杀（exit 143），一个检查点都没到
- store_only: 同命令外部 90 s 杀（exit 124 = timeout 自身）

## 2. 修复

两文件 `TEST_LOOP(test, 1<<13)` → `TEST_LOOP(test, 4)` + 机理注释
（沿用 #347 范本）。

- 检查点 = 4 body ≈ 4-5 s（本机实测）——deadline 超调有界且 ≪ 宽限 300 s
- 段 = 40×4 = 160 body，段重入（temp/dst 重分配+首触）开销占比可忽略
- 压力语义不变：总时长仍由 -t 主导（fracture 段重入到 runtime ≥ -t），
  golden 字节 memcmp 不动，count 自适应不动
- x86 无关：改动在 `#if defined(__aarch64__)` run 函数内，x86 编译 #else
  占位分支（无 TEST_LOOP）
- VL 无关：N 是时间粒度常量，不引入 VL 条件行为；本机 SVE1/256-bit 原生
  跑即完整行使改动语句，无需 gem5-fi

## 3. 验证（实测）

1. 全量干净构建 461/461，零警告零错误
2. 修复前 `-t 2 -n 8`：两测试分别 120 s/90 s 被外部杀，无法自行退出
3. 修复后 `-t 2 -n 8`：2src 4.95 s / store_only 4.58 s 自行退出 `exit: pass`
4. 修复后全核 `-t 30000`：2src 34.3 s / store_only 34.1 s `pass`
   （count=7,696,569，超调 ≈ 4 body）
5. 回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`
6. x86 目检：无 arch 分叉

## 4. 全仓同类排查（子 agent 全量分级，另立跟进 issue）

N≥256 共 188 处；除已处置 3 处外，新发现同构风险家族：
sve_rot_ldr_at_top×5、fma2 四兄弟（movq2dq/movdq2q/movmskpspd/neon_add）、
kreg 五兄弟（arm0102_kreg_{logic,mask,not,select}+arm0102_fma_f32）、
gmp_bigadd_sve、SVE ipsec 16 MiB 档×7（N=256，#347 同型）；
低风险：mrn_rmw_diff_1m_sve（固定 1 MiB 诊断几何）。

## 5. 交付

单 commit（`git commit -s`）→ push worktree 分支 → PR（Fixes #349）→
code-reviewer 检视 → merge → issue 关闭 + 跟进 issue。
