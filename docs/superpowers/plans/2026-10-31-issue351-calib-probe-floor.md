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

1. `sdc_calibrated_dim` 增加参数 `double probe_floor_ms = 1.0`(置于 `fit` 出参之前, 输入在前后出)
   【勘误 2026-11-02, R2/3d424004: 该位次设计被检视推翻——置于 fit 前使字面量
   0/NULL(『显式不要 fit』旧写法)经 int→double 隐式转换静默重绑
   probe_floor_ms=0.0, probe 抬档循环永假且无告警; 终态为 fit 第 8 参、
   probe_floor_ms 殿后, 详见 §4 R2 台账】:
   - 默认 1.0 = `f84966bc` 之前的历史噪声底语义 → 全部 7 参存量调用(x86 链 + aarch64-非-Linux 链)行为恢复, **调用点源码零改动**
   【勘误 2026-11-02, R2/ddd87bba: 原文『byte-identical 恢复』系过度声明——
   校准依赖墙钟计时不可字节复现, 准确口径 = 同 1ms 停点与代码路径、操作点
   在计时抖动内一致(实测 mdim 473 vs 基线 476), 详见 §4 R2 台账】;
   - 100.0 = spec §4.3 M3 固定开销口径, 由需要的调用方显式传入。
2. 联合求解内部调用(:1062)显式传 `100.0`——ARM-only 路径, PR #302 已 ARM 实测的行为**原样保留**。
3. 注释同步:probe 循环注释改述两级语义(默认 1.0 噪声底 / M3 100.0 显式传参);fit 差分注释「与 100ms 阈值配套」明确为联合求解传参口径;联合求解「探针成本由 100ms 阈值机制约束」注明为显式实参。
4. docs/architecture.md:273-281「已批准例外」段改写:例外撤销——阈值参数化, 存量链恢复 1ms(与改动前语义等价, 同上勘误口径), 100ms 由联合求解在 ARM 路径显式携带。

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

## 4. 检视跟进附录(2026-11-02, R1/R2——登记 scope 演进, 修正 §3 交付承诺漂移)

§1.3 修复方案(4 条)与 §3 交付承诺(2 commits)只覆盖原始审计范围;PR #377
开出后经两轮检视(local-claude 第 5 bot)驱动出以下追加单元,各自独立
commit、独立验证,在此登记,防「按 plan/PR 描述审计 PR」时漏项:

### R1(2026-11-02, 意见锚定 77bb6381)

- `661d604d` fix(framework): fit 差分 + <100ms probe 档致噪组合加运行期
  log_warning——检视指出该组合可编译、可静默运行,a_ms 虚高 → n_run 偏小
  → 激发维度静默欠压,对 SDC 检测工具是覆盖率损失;从纯注释约束升为运行
  期告警(本 plan §1.3 之外的新增行为, 即检视所指 scope drift)。
- `f4e72e33` docs(spec/plan): §4.3 M3 100ms 阈值机制勘误——活代码三处
  把 spec §4.3 引为 100.0 的规范性出处,参数化后机制已变而 spec/plan
  未同步。

### R2(2026-11-02, 意见锚定 f4e72e33)

- `3d424004` fix(framework): fit 出参恢复第 8 参历史位次、probe_floor_ms
  殿后——§1.3 条目 1 原定「置于 fit 出参之前」使字面量 0/NULL(『显式
  不要 fit』的旧写法)经 int→double 隐式转换静默重绑 probe_floor_ms=0.0,
  probe 抬档循环永假且无告警;该位次设计被检视推翻,以本条为准。
- `3bdc9001` fix(framework): R1 告警守卫改判 probe 档**实测时长**(原判
  入参,floor=100 但 hi 截断漏报、慢求解器+缺省 1.0 误报,两个方向都
  错)+ `sdc_m3_probe_floor_ms` 具名常量统一守卫/联合求解实参/告警文案
  三处口径。
- `317de9ba` docs(spec/plan): 勘误段与运行期告警对齐——『可编译、无运行
  期报错』『缺省会静默拿到 1.0』与 661d604d 后的代码直接矛盾。
- `ddd87bba` docs(architecture): 『byte-identical』过度声明改为「同 1ms
  停点与代码路径、计时抖动内一致」(§1.3 条目 1 与 §2 条目 2 的
  byte-identical 措辞同此口径理解——校准依赖墙钟计时, 不可字节复现;
  两处已由 R3 加内联勘误标记)。
- 本 commit: 本附录 + PR 描述同步(改动清单/验证证据更新至 R2 终态)。

### R3（2026-11-02, 意见锚定 a840596d; 去重后 4 条, 全部采纳）

- `328d4eac` fix(comments): fit 位次沿革勘误——git 实证 `f84966bc~1`
  签名止于第 7 参(无 fit), fit 出参与第 8 位次同由 `f84966bc` 引入,
  『f84966bc 之前的第 8 参』系伪沿革(header :619 + spec:149 两处同错)。
- `8d0dc47e` fix(comments): probe 循环注释 byte-identical 残留改语义
  等价口径, 与 `ddd87bba` 裁定对齐(同一 PR 内不再双口径并存)。
- `1c40c923` docs(plan): 本 plan §1.3 两处被推翻陈述(置于 fit 之前的
  位次设计 + byte-identical)加内联【勘误 2026-11-02】标记指回本节;
  本节引用「§2.2」修正为「§2 条目 2」(本 plan §2 为平铺条目)。
- `5aed6fca` docs(architecture): M3 运行期告警补『仅 fit 消费方触发』
  限定——存量 7 参链不传 fit 永不告警(对存量链不告警是正确行为)。

### R4（2026-11-02, 意见锚定 5aed6fca; 2 条）

- `ac7b5fe9` fix(framework): M3 告警文案按传参是否达标分流(检视 #23
  变通处置——采纳文案分流修复, 驳回阈值重构, 依据详见该 commit
  message: 守卫改锚调用方实参会令『fit 消费方忘传 M3 口径』误用回退
  为静默, 且 sub-100 差分属偏离 spec §4.3 M3 方法口径, 告警有意)。
- 本 commit: 台账补 R3/R4 小节 + 计数改 git 实测口径(检视 #24)。

### 计数口径（R4/#24 裁定）

本台账曾以固定数字收尾(『9 commits/159+』), 落盘即随检视跟进过时——
精确重演了本节自称要防止的『按 plan/PR 描述审计 PR 时漏项』漂移。
此后计数一律以 git 实测为准: `git rev-list --count e6a435aa..HEAD`
与 `git diff --stat e6a435aa..HEAD`(本句落盘时 15 commits、5 文件;
每轮检视跟进递增属预期, 不再在文中维护快照)。7 参存量调用
(x86-64 `#else` 链)源码自始零改动。

