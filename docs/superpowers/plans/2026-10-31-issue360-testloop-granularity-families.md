# Plan: issue #360 — TEST_LOOP 时间检查粒度失配：8 家族全仓同构风险修复 + 4 审计候选定级

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 issue #360 列出的"TEST_LOOP 大 N × 自适应大缓冲"家族（实测清点 22 个风险文件）的检查粒度缩到秒级并加机理注释，4 个审计候选逐一给出定级结论（其中 arm64_sdc_sve 的 N 一并修复，共 23 处 TEST_LOOP）。

**Architecture:** 纯机械修复——每处只改 `TEST_LOOP(test, N)` 的 N（8192/1024/256/128 → 4）+ 一段沿用 PR #347/#359 范本结构的英文机理注释（流量计算 + 历史粒度来源 + issue 引用 + fracture 段论证）。不动 sizing/golden/自适应语义。同根因一个 unit：单 commit。

**Tech Stack:** C++20（测试 TU），sdcshield 框架 `TEST_LOOP`（framework/sandstone.h:138-148）。

**Spec:** issue #360（家族清单表 + 审计候选）；范本 PR #347（openssl_sm3sm4，37e17bb1）/ PR #359（agu_stress SVE，ae6399c7）。

## Global Constraints

- x86-64 行为零改动（CLAUDE.md rule 5）：全部 19 个编辑点都在 `#if defined(__aarch64__)` 的 run 函数内（ipsec 的 run 无 arch 分叉但 N 属两 arch 共用路径——需按 #347 先例论证跨 arch 同向修复）。
- 构建零新警告；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`。
- 提交 `git commit -s`（仅 `Signed-off-by: wangxu <wangxumarshall@qq.com>`），push feature 分支，绝不 push main。
- 不虚报：所有 pass 均有真实运行输出佐证。

## 1. 语义与家族清单核实（已完成）

### 1.1 TEST_LOOP 语义（framework/sandstone.h:138-148, sandstone_run.cpp:633-640）

N = 时间检查粒度：每 N 次 body 调一次 `test_loop_condition(N)` → 查 `-t` deadline。默认 `-t`=1 s（sandstone_p.h:364）。框架 SIGQUIT 宽限 = max(5×-t+30s, 300s)（sandstone.cpp:179-190）。本批测试均未设 `fracture_loop_count` → 自动 fracture 40 检查/段（sandstone_run.cpp:1928-1931）→ 段 = 40×N body。

### 1.2 文件清单核实（对 issue 表的修正）

issue 表中 3 个 k 变体路径笔误（缺 `rowmajor`），实际 5 文件全部存在；`ipsec_3des_docsis_aes_xcbc_sve_wide.cpp` 行号 205→实测 207。ipsec 目录 25 文件中仅 7 个是 16 MiB 自适应档（`hi=16<<20`），其余 18 个 hi=1830/1900 字节（µs 级 body，安全，不动）。

| # | 家族 | 文件（22） | 现N | sizing (per_elem, arrays, rss, ‰, hi) | body 流量/线程 |
|---|---|---|---|---|---|
| F1 | sve_rot_ldr ×5 | sve_rot_ldr_at_top.cpp:163, ..._rowmajor.cpp:151, ..._rowmajor_k3res.cpp:198, ..._k5inter_rand.cpp:159, ..._k7pattern.cpp:242（1 处 TEST_LOOP，8 个 DECLARE_TEST 共享） | 8192 | (vl_bytes, 6, 1.0, 600, hi=1M slots) | ~7×buf（3 ldr + 2 str + memcmp 2 读） |
| F2 | movq2dq_sve | fma2/movq2dq_sve.cpp:80 | 8192 | (8, 1, 1.0, 600, hi=64M) | count 次逐元素 dup+st+cmp（迭代数主导） |
| F3 | movdq2q_sve | fma2/movdq2q_sve.cpp:86 | 1024 | (8, 2, 1.0, 600, hi=64M) | count 次逐元素 dup+lastb+cmp |
| F4 | movmskpspd_sve | fma2/movmskpspd_sve.cpp:163 | 8192 | (8, 4, 1.0, 600, hi=64M) | 2×count 行谓词掩码（迭代数主导） |
| F5 | neon_add_sve | fma2/neon_add_sve.cpp:107 | 8192 | (4, 4, 1.0, 600, hi=64M) | ~6×buf（memset + 2R1W 加法 + memcmp 2R） |
| F6 | arm0102_kreg ×4 | kreg/arm0102_kreg_{logic,mask,not,select}_sve.cpp:172/191/174/195 | 8192 | (4, 5, 1.0, 600, hi=64M) | ~7×buf（2 ldr + str/ldr/str 放大 + memcmp 2R） |
| F7 | arm0102_fma_f32_sve | kreg/arm0102_fma_f32_sve.cpp:192 | 8192 | (4, 6, 1.0, 600, hi=64M) | ~8×buf（3 ldr + str/ldr/str 放大 + memcmp 2R） |
| F8 | gmp_bigadd_sve | arithmetic/gmp_bigadd_sve.cpp:102 | 8192 | (8, 4, 1.0, 600, hi=1M limbs) | ~6×buf（2 ldr + st + 进位重读 a + memcmp 2R） |
| F9 | SVE ipsec 16MiB 档 ×7 | ipsec/{3des_docsis_aes_cmac,3des_docsis_aes_xcbc}_sve_wide.cpp:160/207, ipsec_aes128_cbc_aes_cmac_sve.cpp:162, ipsec_aes192_cbc_xcbc_96_sve.cpp:212, ipsec_aes192_ctr_xcbc_sve.cpp:204, ipsec_aes192_gcm_sve.cpp:152, ipsec_aes192_gcm_sve_wide.cpp:152 | 256 | (1, 4, 1.5, 600, hi=16MiB) | ~20×data_size（frozen 8× + reroll 12×：加解密往返 + memcmp） |

（任务书与 issue 称"8 家族约 17 文件"；按仓库实际为 9 行 22 文件——F1 的 k 变体路径修正后 5 文件、ipsec 精确 7 文件。处置范围以 issue 表语义为准，全修。）

### 1.3 VL 门控核实（gem5-fi 必要性判定）

15 个 SVE-asm 文件全部 `arm64_require_sve_vl(test, 128, 1024)`（kreg 另有 lanes∈[4,32] 单位漂移守卫，覆盖全部档）；ipsec 7 文件无 arm64 门控（OpenSSL EVP 运行时自派发）。**无一锁本机（256-bit）不可达档**；N 是 VL 无关的时间粒度常量，不引入 VL 条件行为 → 本机原生运行即完整行使改动语句，无需 gem5-fi（PR #359 同款论证）。

### 1.4 fracture 语义

本批均未设 fracture_loop_count → 段 = 40 检查 × N body。N=4 → 段 160 body；run 函数段重入（每线程 temp/dst 或 ipsec 4 缓冲重分配）相对 160 body 占比可忽略（PR #359 实测同构）。

## 2. 修复前定标（本机 128 核 / 61 GiB / SVE1 256-bit，-n 8）

协议：`-e <test> -t 30 -n 8` + 外部 `timeout`（SIGTERM 宽限）。被外部杀 = 首检查点 > 外部窗口（风险坐实）；自行退出 = 墙钟即首检查点。sizing 实测值取自运行时 [压力映射] 告警行。

| 家族代表 | sizing 实测（-n 8） | 结果 | 首检查点时延 |
|---|---|---|---|
| F1 sve_rot_ldr_at_top | slots 触 hi=1M → buf 32 MiB/缓冲 ×6 | 60 s 杀（`exit: interrupted`） | > 60 s |
| F2 movq2dq_sve | count 触 hi=64M（需 615M，[欠压力] 告警） | 60 s 杀 | > 60 s |
| F3 movdq2q_sve | count 触 hi=64M（需 292M） | 71 s 自行退出 `exit: pass` | ≈ 69 s（body ≈ 67 ms） |
| F4 movmskpspd_sve | count 触 hi=64M | 60 s 杀 | > 60 s |
| F5 neon_add_sve（--beta） | count 触 hi=64M（需 307M） | 60 s 杀 | > 60 s |
| F6 arm0102_kreg_logic_sve | count 触 hi=64M（需 246M） | 60 s 杀 | > 60 s |
| F8 gmp_bigadd_sve | limbs 触 hi=1M → 8 MiB/数组 ×4 | 47 s 自行退出 `exit: pass` | ≈ 45 s（body ≈ 5.5 ms） |
| F9 ipsec_aes192_gcm_sve | data_size 触 hi=16 MiB | 12 s 自行退出 `exit: pass` | ≈ 10 s（body ≈ 39 ms） |
| F9 ipsec_3des_docsis_aes_cmac_sve_wide | data_size 触 hi=16 MiB | 90 s 杀 | > 90 s（3DES 吞吐 ~1/10 of AES） |

结论：9 个家族全部实测坐实——即便在无争用 -n 8 档，首检查点也落在 10 s ~ 分钟级，全部 ≫ `-t`（默认 1 s / 战役 165 s）；全核带宽/密码竞争下进一步放大（PR #359 实测同构放大 20×）。issue 的量级估算与实测一致。

定标过程的方法学事故与教训（记录）：对被 SIGKILL（`timeout -k 5`）的运行，框架的 control 子进程会孤儿化并继续持有大缓冲——一次 mem_disambig_alu_sve 废弃运行孤儿化后 RSS 涨至 52 GiB、机器 swap 抖动 14 分钟。后续一律用 SIGTERM 宽限协议（`timeout -k 290`）。

## 3. 修复

每文件 `TEST_LOOP(test, N)` → `TEST_LOOP(test, 4)` + 机理注释（范本结构：body 流量 × 自适应来源 → 历史粒度为何失配 → issue 引用 → N=4 界定退出时延 + fracture 段论证）。

选型依据（与 #347/#359 一致）：
- 检查点 = 4 body：定标后各家族 body 0.05-4 s → 检查点 0.2-16 s，全部 ≪ 战役外部 180 s，且 ≥ 默认 -t 1 s 的超调有界；
- 段 = 160 body：段重入开销占比 ~0.1-3%；
- 压力语义不变：总时长由 -t 主导，golden/sizing 不动；
- N=4 保持 2 的幂惯例。

## 4. 验证（实测，全部引用真实运行输出）

1. **干净构建**：`rm -rf builddir` → meson setup → `SDC_BUILD_JOBS=20 ./scripts/build.sh` = **537/537，零警告**（含 vendored OpenSSL 3.5.0，worktree 软链主仓库 third-party/openssl/install）。
2. **逐测试 -n 8**：23 处 TEST_LOOP 覆盖的 26 个测试 ID 全部 `-e <t> -t 30 -n 8` 自行退出 `exit: pass`（neon_add_sve/arm64_sdc_sve 需 `--beta`；k7pattern.cpp 注册名是 4 个 k7p_* 变体）。代表 test-runtime：gmp_bigadd 115 ms、ipsec 3DES cmac 11.1 s、movq2dq 2.0 s、arm64_sdc（128 MiB 限流）4.9 s、movdq2q 12.9 s——对比修复前（同规模）：>60 s 零检查点 / >90 s 零检查点 / >60 s / >90 s / ≈69 s。
3. **全核代表 ×3**（`-t 30000` 30 s，128 线程）：sve_rot_ldr_at_top 32.55 s（超调 2.55 s ≈ 4 body×0.64 s）、ipsec_aes192_gcm_sve 31.7 s（超调 1.7 s）、arm0102_kreg_logic_sve 34.6 s（超调 4.6 s ≈ 4 body×1.15 s）——全部 `result: pass`，`-t` 契约在全核竞争下兑现。
4. **回归**：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
5. **x86 非回归（构造性）**：23 个文件全部位于 `tests/cpu/sve/`，该子目录在 tests/cpu/sve/meson.build 里整体 `if host_machine.cpu_family() == 'aarch64'` 门控——x86-64 不编译这些 TU，零影响。
6. **gem5-fi 边界声明**：15 个 SVE-asm 文件门控均为 `arm64_require_sve_vl(test, 128, 1024)`、ipsec 7 文件无 VL 门控——无一锁本机（256-bit）不可达档；N 是 VL 无关的时间粒度常量，本机原生运行即完整行使改动语句（PR #359 同款论证），故无需 gem5-fi。

## 5. 审计候选定级（B 项）

| 候选 | 定级 | 实测依据 | 处置 |
|---|---|---|---|
| `arm64_sdc_sve.cpp:156`（N=128） | **有风险，修**（同根因） | --beta + knob 128 MiB 限流档：90 s 仍零检查点 → body > 0.7 s；默认 sizing（18.3 GiB/线程）外推首检查点 ≈ 3.6 小时 | 本 PR 改 N=4 + 注释 |
| 同上：aligned_alloc 内存放大 | **真 OOM 风险，立跟进 issue** | sizing 语义实测：`sdc_target_workset_bytes(600,2)` 是每 slice 预算（consumers 语义 = 同时跑的大内存消费者各一份），但 run() **每线程**各 `aligned_alloc` 一份全尺寸缓冲并 memcpy 全触——全核 = 128×18.3 GiB 提交需求，本机必被 OOM 杀 | 跟进 issue #365（sizing 应按 1+nthreads 个消费者计），本 PR 不扩 scope |
| `mem_disambig_alu_sve.cpp:143`（N=8192） | **非 TEST_LOOP 问题，但 issue 的"时限安全"在本机不成立——立跟进 issue** | body 仅 512 稀疏 store/load 本身 µs 级，但默认 sizing 18.3 GiB/线程的稀疏首触在 THP=always 下触发 direct compaction/缺页风暴：首个 body 就 > 60 s（60 s 杀仍零检查点）；废弃运行孤儿化后 RSS 52 GiB + swap 抖动 | 跟进 issue #366（sizing 同款每线程放大 + 首触风暴），N 不动（body 本身 µs 级，非粒度根因） |
| `mrn_rmw_diff_footprint_sve.cpp:182` 1m 变体 | **低风险，不动**（诊断探针语义，与 issue 倾向一致） | `-t 2000 -n 8` 24 s 自行退出 pass → 检查点 ≈ 22 s（body ≈ 2.7 ms，固定 1 MiB×5 几何 L2/L3 可容）≪ 战役外部 180 s；默认 -t 1 s 超调 21× 但框架宽限 300 s 内 pass | 记录结论；若未来战役全核档报 Timed Out 再按同范本收紧 |
| x86 `tests/cpu/ipsec/**` 46 处 datasize knob | **默认路径安全；knob 拉满有同构风险——不改（rule 5），建议写 issue comment** | 46 处 knob 全部 `1024..64*1024*1024`（默认 1 KiB → 检查点 µs 级安全）；拉满 64 MiB → body ≈ 1.25 GiB 密码流量 → 256 body 检查点分钟级 | 建议：knob 上限与 N 联动或收紧上限，供维护者决策；x86 行为零改动 |

## 6. 交付

单 commit → push worktree 分支 → PR（正文含定标表、家族处置状态、审计结论、Fixes #360 + 跟进 issue 引用）→ feature-dev:code-reviewer 检视 → 修复循环 → merge → issue 收尾。
