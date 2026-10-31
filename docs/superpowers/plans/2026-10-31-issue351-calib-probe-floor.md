# Plan: issue #351 — `sdc_calibrated_dim` probe 阈值 1ms→100ms 无 arch 门控,泄漏到 x86-64 存量链

日期: 2026-10-31 · 分支: fix/issue-351 · issue: #351 · 缺陷引入: PR #302 commit `f84966bc`

## 1. 批判性分析(已复现实证)

### 1.1 语义与根因

- `sdc_calibrated_dim`(framework/sdc_stress_config.h:628)是**无 arch 门控的共享模板**。其 probe 档自适应翻倍循环(:649)在 `f84966bc`(PR #302「联合求解」)中把阈值从 `t1 < 1.0` 改为 `t1 < 100.0`。
- 改动动机是 spec §4.3 M3:probe 档抬到固定开销占比 <10%,供联合求解的 fit 出参做两点差分扣 b——而联合求解是 ARM64-only 路径。
- 现役消费方(全量核查):
  - framework/sdc_stress_config.h:1062 — `sdc_joint_dim_slice` 的 `#if __aarch64__ && __linux__` 分支(唯一 8 参调用, 传 `&fit`)→ **期望 100ms**;
  - tests/cpu/eigen_svd/sandstone_eigen_common.h:261 — `#else`(非 aarch64-Linux, 即 x86-64 参考链)7 参 → **期望历史 1.0**;
  - tests/cpu/eigen_sparse/eigen_sparse.cpp:194 — `#else` 同上。
- 即:**7 参调用方恰好全部是 x86-64 存量链;ARM64 唯一活跃调用方恰好要 100ms**。改动把服务新路径的参数写死进共享模板,恰好绕过 PR #302 自己的「`#else` 旧链逐字保留(rule 5)」与 architecture.md:264-281 的 x86 行为不变声明,且 PR 未做任何 x86 实测——审计成立。

### 1.2 复现(本机 aarch64, 128 核 / 30 GiB)

共享模板与 arch 无关 → standalone harness(放 /tmp,不入仓)以 x86 `#else` 分支**逐字同构的 7 参调用形态**直呼模板;solve_once 用真实校准探针逐字拷贝(eigen_sparse.cpp:103-137 稀疏 Cholesky / sandstone_eigen_common.h:166-191 稠密 BDCSVD sizing_probe);框架符号最小桩(thread_num/log_message/thread_count/total_thread_count/quick_run);编译口径取 compile_commands.json 中 eigen_sparse.cpp 的真实 flags(gnu++23 -O3 -DNDEBUG …),保证探针计时口径与真实构建一致。

对照三份头文件:main HEAD(100ms)/ `f84966bc~1`(改动前基线, 1ms)/ `85a42170`(最初引入)。同 hi 实测:

| 用例(同 hi) | 基线 f84966bc~1 (1ms) | main HEAD (100ms) | 后果 |
|---|---|---|---|
| eigen_sparse, hi=1191 | (p1,mid)=(256,512), k=3.30, 5 probes, 0.41s | (1024,1191), k=3.00, 7 probes, 1.21s | 拟合基数移位, init 校准 3×;final n 同为 1191(本 host hi 夹住的巧合) |
| eigen_svd(BDCSVD f32), hi=765 | (64,128), k=3.00, 3 probes, 0.16s, **mdim=476** | (512,765), 6 probes, 1.29s, **mdim=765** | **sizing +61%(内存足迹 2.6×), init 校准 8×** |

x86-64 参考链的 sizing 结果与 init 成本被实质改变——rule 5 违反实证。

### 1.3 修复方案(最小改动: 参数化, 而非 arch 门控)

阈值本是**校准模型参数**,不是 arch 属性:

1. `sdc_calibrated_dim` 增加参数 `double probe_floor_ms = 1.0`(置于 `fit` 出参之前, 输入在前后出):
   - 默认 1.0 = `f84966bc` 之前的历史噪声底语义 → 全部 7 参存量调用(x86 链 + aarch64-非-Linux 链)行为 byte-identical 恢复, **调用点源码零改动**;
   - 100.0 = spec §4.3 M3 固定开销口径, 由需要的调用方显式传入。
2. 联合求解内部调用(:1062)显式传 `100.0`——ARM-only 路径, PR #302 已 ARM 实测的行为**原样保留**。
3. 注释同步:probe 循环注释改述两级语义(默认 1.0 噪声底 / M3 100.0 显式传参);fit 差分注释「与 100ms 阈值配套」明确为联合求解传参口径;联合求解「探针成本由 100ms 阈值机制约束」注明为显式实参。
4. docs/architecture.md:273-281「已批准例外」段改写:例外撤销——阈值参数化, 存量链恢复 1ms(与改动前 byte-identical), 100ms 由联合求解在 ARM 路径显式携带。

非目标:不动联合求解的 100ms 值本身(审计未质疑该值;spec M3 + PR #302 ARM 实测);不动 eigen_sparse/eigen_svd 调用点源码;不动 `#else` 分支结构。

### 1.4 边界

- aarch64-非-Linux 亦走 `#else` 链, 同样被恢复(此前同样被改)。
- 未来新的 7 参调用方(任意 arch)拿到历史一致语义;要 M3 口径走联合求解或显式传参。
- x86 无 CI job(仓库既有事实)→ x86 非回归 = 源码级检视(唯一改动是模板默认参数值, 恢复 `f84966bc~1` 常量语义;非默认值的唯一调用方在 ARM-only 分支内)+ 共享路径本机 harness 实测(复现与恢复在同一份 arch 无关代码上完成)。

## 2. 验证(实测)

1. `./scripts/build.sh` 全量干净构建, 零新警告。
2. harness 以修复后头重编, 同 hi 复测:sparse(1191)→ (p1,mid)=(256,512)、5 probes、~0.4s;svd(765)→ mdim≈476、3 probes、~0.16s——与 `f84966bc~1` 基线一致(1ms 停点恢复)。
3. ARM64 联合求解路径不变:`-e eigen_sparse` 短跑 before/after, `[联合求解]` n_mem/n_run/n 输出一致(100ms 仍由联合层显式携带)。
4. 回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`。
5. x86 目检:7 参调用点未触碰;模板默认值 = 改动前常量语义。

## 3. 交付

commit 1: 本 plan 文档;commit 2: 修复(framework/sdc_stress_config.h + docs/architecture.md, 同一逻辑单元)→ push fix/issue-351 → PR(Closes #351)→ issue 回帖报告。
