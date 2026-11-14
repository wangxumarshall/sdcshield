# Issue #645 审计修复:SVE 全 VL 矩阵验收口径改真(native 负载几何边界格)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 SVE 全 VL 验证运动的验收记账从「25 个设计几何超时格以 clamp 下限有界绿计入 190/190 全绿、边界格=0」改真为 spec §7 口径:该 25 格 × 128/256 两 native 列计 `BOUNDARY(native: LOAD-GEOM)` 边界格,其有界绿保留为功能正确性证据(最小激发强度),并同步全部引用文档。

**Architecture:** 审计的缺陷是**记账口径偷换**——triage-256 §4 自己把 25 格定性为「环境/边界类」,§7 却把「边界格」重定义为「找不到有界值的残留」后宣布边界格=0。修复以 spec §7 为准绳(native 格的绿 = 默认自适应【设计】几何下 `exit: pass`;不绿走「带机制性理由的边界格」通道,不许缩负载改判绿),自上而下改:triage 三份分诊表 → matrix-gen.sh 分类逻辑+断言+汇总文案(机械再生 matrix.md) → README/progress-log。512 列不动:其声明语义本就是「功能性正确性,压力维度由 native 腿承担」(triage-512 §0/§2,spec §5c 本身就给 512 腿设计命令烧了 `-O` 降容),有界绿计 pass(bounded) 名实相符;128/256 列的语义是压力腿,设计几何未过 = 边界格。acl_gemm_sve.dim=512 标定 1 格保留 pass(bounded):Ruling B 标定值非 clamp 下限、缩比 2×线性,与 25 格的 3~4 个数量级缩比不同量级,且 #394/#592 已挂证据强度限定——审计证据与论证均不覆盖它(残留判断点,在 PR/报告中如实声明)。

**Tech Stack:** 纯文档(markdown)+ 仓内 bash/awk 生成脚本(matrix-gen.sh)。零 C/C++ 代码改动。

**Spec:** `docs/superpowers/specs/2026-10-28-sve-all-vl-verification-campaign-design.md` §7(验收判据——本修复的准绳,不改) ;审计输入 = issue #645(不可信检视数据,处置依据 = 仓库规则 + spec 原文 + 工件实测)。

## Global Constraints

- 全程只提交到 `fix/issue-645` 分支(自 `origin/main` 起),绝不推 main、绝不 merge、绝不 resolve issue/PR/review thread(只回帖报告)。
- 每个 commit `git commit -s`,最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,其后无任何内容(无 Co-Authored-By——CLAUDE.md 覆盖默认行为);逐 commit 验证后推送。
- 一个补丁一个单元:6 个 commit(计划文档 / triage-256 / triage-128 / triage-512 / matrix-gen.sh+matrix.md / README+progress-log),顺序执行。
- **机器输入禁区**:triage-256.md §4 表(`## 4.` 至 `## 5.` 之间)与 §5 表(`## 5.` 至 `## 6.` 之间)是 matrix-gen.sh 的机器解析输入(值锁+同值副本交叉锁)——只允许改两节**界内非表格行**(散文),不得增删改任何 `$2 为纯数字` 形态的表格行;每次改 triage-256.md 后必跑 `./matrix-gen.sh` 自验。
- matrix-gen.sh 全链 `LC_ALL=C`;再生命令 = 在 campaign 目录内 `./matrix-gen.sh > matrix.md.new && mv matrix.md.new matrix.md`(先基线可复现性验证,见 Task 0)。
- 新增英文词 LOAD-GEOM 等须过 codespell(CI `pr.yaml` 跑 `codespell`);本 plan 所引中文文案不得引入错别字。
- 验证诚实(CLAUDE.md rule 2):docs+脚本改动,受影响行为 = matrix-gen.sh 记账;回归命令 `-e zstd19 -t 2000 -n 1` 按 #592 docs-only 先例仍须实跑引用真实输出;x86-64 非回归 = 纯文档/脚本、零代码路径改动,如实声明。
- 结束语:成功 = `task-loop: done fix/issue-645`;失败 = `task-loop: escalated <原因>`。

---

### Task 0: 基线与复现(无 commit)

**Files:** 只读。

**Interfaces:**
- Produces: 审计三项事实主张的机械复现输出(贴入 PR/报告);matrix-gen.sh 基线可复现性结论(决定 Task 4 是否含额外漂移)。

- [ ] **Step 1: 建分支 + 复现审计主张 ①(25 格 timed out 对账)**

```bash
git checkout -B fix/issue-645 origin/main
cd docs/superpowers/inventory/sve-all-vl-campaign
awk -F'\t' '$1 !~ /^#/ && $1 != "test" {c[$2]++} END {for (k in c) printf "%s\t%d\n", k, c[k]}' native-256-LCG1001.summary.txt
```
Expected: `pass	165` 与 `timed out	25`(= §1 表 seed1 行、审计「165 pass+25 timed out=190」复现)。

- [ ] **Step 2: 复现审计主张 ②③(缩比 3~4 个数量级 + 口径偷换原句在案)**

```bash
grep -n "根因（环境/边界类" triage-256.md            # §4 定性在案
grep -n "边界格 = 0" triage-256.md triage-128.md     # §7/triage-128 §5 偷换口径原句在案
grep -n "缩比（knob 值）" triage-256.md              # §5 缩比表(×7,575~×10⁶)在案
grep -n "564 格矩阵全绿\|允许带" ../../specs/2026-10-28-sve-all-vl-verification-campaign-design.md  # spec §7 判据原文
```
Expected: 各 grep 命中(复现完成,可修)。

- [ ] **Step 3: matrix-gen.sh 基线可复现性验证**

```bash
./matrix-gen.sh > /tmp/matrix-baseline.md && diff /tmp/matrix-baseline.md matrix.md && echo BASELINE-IDENTICAL
```
Expected: `BASELINE-IDENTICAL`(matrix-gen.sh 上次改动 025adadd/#418 未影响输出)。
**决策规则**:若 diff 非空——先弄清漂移来源(git log matrix-gen.sh vs matrix.md);若漂移系 #418 遗留的正当输出差异,先单独 commit「纯再生」再进 Task 4;若无法解释,**停止并 escalated**(不得把不明漂移混入口径修复)。

- [ ] **Step 4: 检查构建/回归前置条件**

```bash
command -v codespell || echo no-codespell-bin
ls third-party/ 2>/dev/null | head; ls install/ 2>/dev/null | head || echo no-install-dir
```
记录:codespell 可用性(不可用则在 Task 6 如实声明,新词人工核对);构建依赖现状(决定 Task 6 构建方式,缺 install/ 时按 README「优雅降级」口径处理,zstd19 回归若因依赖缺失不可跑 = 验证边界如实声明)。

### Task 1: triage-256.md 验收口径改真(核心审计修复)

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md` §1 绿格构成 bullet 尾句(:108 附近)、§5 尾句(:303)、§7 整节开头段+前两个 bullet(:333-352)、§7 观察项③尾句(:373 附近)。

**Interfaces:**
- Consumes: Task 0 复现事实。
- Produces: 新口径术语 `BOUNDARY(native: LOAD-GEOM)`、`负载几何边界格`(后续所有任务引用);§7 验收记账 = 164 直绿 + 1 acl 标定 pass(bounded) + 25 负载几何边界格。

- [ ] **Step 1: §1 绿格构成 bullet 尾句补记账指针**

old(唯一命中):
```
本绿格构成与 §5「证据强度限定」口径；seed2 行全部数据来自 09:24 起的有界重跑腿
```
new:
```
本绿格构成、§5「证据强度限定」与 §7 验收记账（25 个超时格 = 负载几何边界格，audit #645）口径；seed2 行全部数据来自 09:24 起的有界重跑腿
```

- [ ] **Step 2: §5 尾句改记账口径**

old(唯一命中,§5「证据强度限定」块末):
```
§7 的收口口径以此为限定：这 25 格与 164 个默认自适应直绿格**不等价**（190 格中另 1 格 acl 标定 baked 与这 25 格同归有界侧，证据强度与有界绿同向受限——§7）。
```
new:
```
§7 的验收记账以此为限定（audit #645）：这 25 格计**负载几何边界格**（BOUNDARY(native: LOAD-GEOM)——设计几何未过、有界绿仅为功能正确性证据），与 164 个默认自适应直绿格**不等价**；另 1 格 acl 标定 baked 缩比 2×线性、与这 25 格的 3~4 个数量级缩比不同量级，仍计 pass(bounded)，证据强度与有界绿同向受限——§7。
```

- [ ] **Step 3: §7 开头段 + 前两个 bullet 整体替换**

old(§7 自 `## 7. 结论` 起至 `- **边界格 = 0**` bullet 结束,即 :335-352 整块):
```markdown
**native 256 档两 seed 全绿（绿格构成两 seed 不对称、非同参数复现——seed1：165 格
腿内绿 = 164 格默认自适应直绿 + 1 格 acl 标定 baked，另 25 格为腿外单测 clamp 下限
重跑绿；seed2：190 格腿内绿 = 164 格默认自适应直绿 + 1 格 acl 标定 baked + 25 格
腿内 clamp 下限有界绿——seed2 腿烧入 26 项 -O 有界覆盖、seed1 腿仅 1 项，两 seed
的「全绿」跨不同参数空间；默认自适应直绿与有界绿（1 格 acl 标定 + 25 格 clamp
下限，几何均远小于自适应档）两者证据强度不等价——§5「证据强度限定」），Task 2 收口**：

- **绿格（矩阵 190 清单格 × 2 seed = 380 格）**：
  - seed1（LCG:1001）：165 格腿内绿（164 格默认自适应直绿 + 1 格 acl_gemm_sve
    dim=512 标定 baked，Ruling B/§1——baked 有界几何，证据强度与有界绿同向受限）+
    25 格有界重跑绿（§5 表，全部 clamp 下限一次
    通过、零上调 = 最小激发强度）= **190/190**；另有框架追加 mce_check 绿。
  - seed2（LCG:1002，26 项有界重跑腿）：190 格腿内绿（164 格默认自适应直绿 + 1 格
    acl 标定 baked + 25 格腿内 clamp 下限有界）= **190/190**；mce_check 绿。
    `exit: pass`（exit=0），`--ignore-timeout` 未触发。
- **边界格 = 0**（25 个超时格全部找到可通过的有界值——即 clamp 下限，无"找不到
  有界值"的残留；注意其语义是**最小可通过几何的存在性证明**，满强度自适应几何在该
  25 格未被建立——其腿内实测为超时，见 §4/§5）。
```
new:
```markdown
**native 256 档收口（spec §7 验收口径，audit #645 记账修正）**——旧收口「native 256 档
两 seed 全绿…边界格 = 0」把 25 个超时格的 clamp 下限有界绿计入全绿，与本表 §4
「环境/边界类」定性直接矛盾、偏离 spec 验收判据（native 格的绿 = 默认自适应【设计】
几何下 `exit: pass`；不绿走「带机制性理由的边界格」通道，不许缩负载改判绿），已按下
述口径改真。两 seed 绿格构成不对称、非同参数复现等**事实描述不变**（§1：seed1 腿
仅 1 项 acl 标定 baked、25 格有界绿来自腿外单测；seed2 腿烧入 26 项 -O 有界覆盖）：

- **验收记账（190 清单格）**：
  - **默认自适应直绿 164 格**（两 seed 腿内一致）；
  - **acl 标定 baked 1 格**（pass(bounded: dim=512)，Ruling B/§1——标定值非 clamp
    下限、dim 缩比 2×线性，与下述 25 格的 3~4 个数量级缩比不同量级；证据强度与
    有界绿同向受限，§5）；
  - **负载几何边界格 25 格（BOUNDARY(native: LOAD-GEOM)——边界格 = 25，非 0）**：
    设计几何（默认自适应——§4 的 600‰/650‰/900‰ 内存比例几何）在该档 300 s 单轮
    预算内无法完成一轮（seed1 腿内实测 timed out，§4 机制：环境/边界类、与 seed
    无关）。其 clamp 下限有界绿（§5：seed1 腿外单测 + seed2 腿内 baked）= **功能
    正确性证据（最小激发强度）**，不构成满强度激发与通过证据（§5「证据强度
    限定」），按 spec 判据计入边界格、不计入绿。旧句「边界格 = 0（25 个超时格全部
    成功有界化）」把「边界格」重定义为「找不到有界值的残留」——audit #645 判定系
    口径偷换，废弃。
- **腿内实测（事实层，不变）**：seed1（LCG:1001）= 165 pass + 25 timed out（§1 表）
  + mce_check 绿；seed2（LCG:1002，26 项有界重跑腿）= 190 pass、0 timeout +
  mce_check 绿，`exit: pass`（exit=0），`--ignore-timeout` 未触发。
```
(其后 `- **代码 bug 候选 = 0（范围限定，audit #395）**`、`- **移交 Task 7 的观察项`、`- **证据文件**` 三个 bullet 原文逐字保留,不动。)

- [ ] **Step 4: §7 观察项③尾句补记账口径**

old(唯一命中):
```
如遇同类格直接按本表 clamp 下限协议有界化即可（所得为最小强度
  证据，报告口径须携带 §5 同款限定）。
```
new:
```
如遇同类格直接按本表 clamp 下限协议有界化即可（所得为最小强度
  证据，报告口径须携带 §5 同款限定，验收记账按本节口径计负载几何边界格、不计入
  全绿——audit #645）。
```

- [ ] **Step 5: 机器输入自验 + 残句检查**

```bash
cd docs/superpowers/inventory/sve-all-vl-campaign
./matrix-gen.sh > /tmp/m0.md && diff -q /tmp/m0.md matrix.md && echo GEN-STILL-IDENTICAL
grep -n "边界格 = 0" triage-256.md || echo OLD-CLAIM-GONE
```
Expected: `GEN-STILL-IDENTICAL`(§4/§5 机器输入未动) + `OLD-CLAIM-GONE`。

- [ ] **Step 6: Commit + push**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md
git commit -s -m "fix(triage): 256 腿验收口径改真——25 个设计几何超时格计负载几何边界格,废弃「边界格=0」口径偷换 (issue #645)"
git push -u origin fix/issue-645
```

### Task 2: triage-128.md 收口同步

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-128.md` §2 矩阵记账行(:85-87)、§5 开头段(:120-121)、§5 绿格/边界格 bullet(:123-127)、§5 对照结论(:128-129)。

**Interfaces:**
- Consumes: Task 1 术语(`BOUNDARY(native: LOAD-GEOM)`/负载几何边界格)与 163/1/25/1 记账。
- Produces: 128 腿验收记账 = 163 直绿 + 1 acl 标定 pass(bounded) + 25 负载几何边界格(设计几何未实测档,外推强度「预期」) + 1 合法 SKIP。

- [ ] **Step 1: §2 矩阵记账行改记账**

old(唯一命中):
```
- **矩阵记账**：(sve512_fmmla_arm, 128) 格 = **合法 SKIP（架构下界）**，128 腿证据
  矩阵 = 189 直绿格 + 1 合法 SKIP 格；其余 189 用例无任何 SKIP——本机 128 档
```
new:
```
- **矩阵记账**：(sve512_fmmla_arm, 128) 格 = **合法 SKIP（架构下界）**，128 腿证据
  矩阵 = 163 默认自适应直绿 + 26 格腿内有界（= 25 格负载几何边界 + 1 格 acl 标定）
  + 1 合法 SKIP 格（验收记账 = spec §7 口径，audit #645，见 §5）；其余 189 用例无任何 SKIP——本机 128 档
```

- [ ] **Step 2: §5 开头段 + 绿格/边界格 bullet 替换**

old(:120-127 整块):
```markdown
**native 128 档两腿收口：189/190 直绿 × 2 seed + 1 个架构性合法 SKIP 格（fmmla@128）
+ mce_check 绿 × 2，`exit: pass` × 2，`--ignore-timeout` 未触发。**

- **绿格（矩阵 190 清单格 × 2 seed = 380 格）**：seed1 189 直绿（26 格为腿内有界）；
  seed2 189 直绿（同）。两 seed 合计 378 绿格。
- **合法 SKIP 格：2 格**（两 seed 各 1，同一格 sve512_fmmla_arm@128，§2 架构下界）。
- **边界格 = 0、代码 bug 候选 = 0**：256 腿的 25 个超时格在 128 腿零超时（有界覆盖
  生效），无 fail/crash；eigen 分发锚点实据 128 变体被执行。
```
new:
```markdown
**native 128 档两腿收口（验收记账 = spec §7 口径，audit #645 同步）：190 格 = 163
默认自适应直绿 + 1 格 acl 标定 baked（pass(bounded)）+ 25 格负载几何边界格
（BOUNDARY(native: LOAD-GEOM)）+ 1 个架构性合法 SKIP 格（fmmla@128）；mce_check
绿 × 2，`exit: pass` × 2，`--ignore-timeout` 未触发。**

- **腿内实测（事实层）**：seed1 189 pass（26 格为腿内有界——25 格 clamp 下限 +
  1 格 acl 标定）；seed2 同。两 seed 合计 378 腿内绿格。
- **合法 SKIP 格：2 格**（两 seed 各 1，同一格 sve512_fmmla_arm@128，§2 架构下界）。
- **负载几何边界格 = 25、代码 bug 候选 = 0**：该 25 格**本档设计几何未实测**（腿
  自始烧入 26 项有界覆盖——控制器裁定，两 seed 腿内有界直绿、零超时）；同几何机制
  在 256 档实测超时（triage-256 §4：几何由物理内存比例与全机线程数推导、输入不含
  VL），故同归负载几何边界类——外推强度「预期」非「确定」。有界绿 = 功能正确性
  证据（clamp 下限，最小激发强度——triage-256 §5「证据强度限定」），不计入绿。
  旧句「边界格 = 0：256 腿的 25 个超时格在 128 腿零超时（有界覆盖生效）」以腿内
  零超时论证无边界系循环论证（有界覆盖生效恰是零超时的原因），audit #645 修正。
  无 fail/crash；eigen 分发锚点实据 128 变体被执行。
```

- [ ] **Step 3: §5 对照结论改口径**

old(唯一命中):
```
- **与 256 腿的对照结论**：190 用例中 189 个 VL 敏感执行路径在 128/256 两档全绿；
  唯一档位差 = fmmla 的 F64MM 架构下界（256 起步）——这正是全 VL 矩阵要刻画的东西。
```
new:
```
- **与 256 腿的对照结论**：190 用例中 189 个在 128/256 两档均有腿内绿证据（其中
  25 个负载几何类用例两档均为有界形态、验收记账两档同计边界格——triage-256 §7 /
  本节，audit #645）；唯一档位差 = fmmla 的 F64MM 架构下界（256 起步）——这正是
  全 VL 矩阵要刻画的东西。
```

- [ ] **Step 4: 残句检查 + Commit + push**

```bash
grep -n "边界格 = 0" docs/superpowers/inventory/sve-all-vl-campaign/triage-128.md || echo OLD-CLAIM-GONE
git add docs/superpowers/inventory/sve-all-vl-campaign/triage-128.md
git commit -s -m "fix(triage): 128 腿收口同步——「边界格=0」循环论证修正,25 格设计几何未实测计负载几何边界格 (issue #645)"
git push origin fix/issue-645
```

### Task 3: triage-512.md native 侧引用同步(含 eigen_svd_cdouble 隐藏失实)

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-512.md` §5.3 TIMEOUT 小结第 2 bullet(:259-261)、§5.4 段内 native 256 引用(:283-285)、§6 仿真速约束 bullet(:299-300)。

**Interfaces:**
- Consumes: Task 1 术语与 triage-256 §7 新记账;事实 = 4 个 SIM-SPEED 边界格中 `eigen_svd_cdouble_sve` 属 25 超时格名单(§4 表 #9,有界 mdim=300),其余 3 格(mrn_rmw_diff_1m/movbe_slf_sweep/sve512_stencil_axis)native 直绿。

- [ ] **Step 1: §5.3 硬边界 bullet 改「四格 native 全绿」失实句**

old(唯一命中):
```
- **4 格为硬边界**：mrn_rmw_diff_1m、eigen_svd_cdouble、movbe_slf_sweep、
  sve512_stencil_axis——knob 下限/无杠杆 + gem5 仿真速（四格 native 256 腿
  全绿，单轮指令量在 ~1400× 减速下超出任何实用仿真预算）。
```
new:
```
- **4 格为硬边界**：mrn_rmw_diff_1m、eigen_svd_cdouble、movbe_slf_sweep、
  sve512_stencil_axis——knob 下限/无杠杆 + gem5 仿真速（单轮指令量在 ~1400×
  减速下超出任何实用仿真预算）。native 侧：mrn_rmw_diff_1m、movbe_slf_sweep、
  sve512_stencil_axis 三格 256 档直绿；eigen_svd_cdouble_sve 为负载几何边界格
  （256 档设计几何超时、有界 mdim=300 功能绿，triage-256 §4/§5/§7，audit #645）。
```

- [ ] **Step 2: §5.4 段内「native 256 腿 190/190 全绿」引用改真**

old(唯一命中):
```
结合 §2.2 HWCAP 探针（SVE=1、门全开）与
native 256 腿 190/190 全绿（构成 = 164 格默认自适应直绿 + 1 格 acl 标定 baked +
25 格 clamp 下限有界绿，两 seed 参数空间不对称——绿格构成与证据强度口径见
triage-256 §1/§5/§7；无规格门 SKIP），本腿绿格均为真实执行。
```
new:
```
结合 §2.2 HWCAP 探针（SVE=1、门全开）与
native 256 腿两 seed 腿内零 fail/crash（165/190 与 190/190 腿内绿，构成与验收
记账口径见 triage-256 §1/§5/§7——25 个负载几何边界格的有界绿为功能正确性证据、
非满强度，audit #645；无规格门 SKIP），本腿绿格均为真实执行。
```

- [ ] **Step 3: §6 仿真速约束 bullet 尾句改真**

old(唯一命中):
```
  - 仿真速约束（4）：knob 下限或无杠杆的单轮指令量超出实用预算
    （§5.2）——native 全绿。
```
new:
```
  - 仿真速约束（4）：knob 下限或无杠杆的单轮指令量超出实用预算
    （§5.2）——native 侧 3 格直绿、eigen_svd_cdouble_sve 为负载几何边界格
    （有界 mdim=300 功能绿，triage-256 §7，audit #645）。
```

- [ ] **Step 4: 全文残句检查(不得误伤 §5.1 的 5 crash 格「native 腿全绿」——5 格均不在 25 名单,句义为真保留) + Commit + push**

```bash
grep -n "native 全绿\|native 256 腿" docs/superpowers/inventory/sve-all-vl-campaign/triage-512.md
# 预期剩余命中:§5.1「五格均非测试 bug（…native 腿全绿…）」(5 crash 格均直绿,为真,保留)
git add docs/superpowers/inventory/sve-all-vl-campaign/triage-512.md
git commit -s -m "fix(triage): 512 腿 native 引用同步——eigen_svd_cdouble native 侧实为负载几何边界格,「四格 native 全绿」改真 (issue #645)"
git push origin fix/issue-645
```

### Task 4: matrix-gen.sh 分类逻辑+断言+文案,机械再生 matrix.md

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/matrix-gen.sh`(头注、cls()、128/256 列分类、汇总断言块、§5 汇总 printf、HDR/HDR2 文案)。
- Regenerate: `docs/superpowers/inventory/sve-all-vl-campaign/matrix.md`。

**Interfaces:**
- Consumes: `clamp_lo.tsv` 解析结果(输入 9,`clamplo` map——恰为 25 个负载几何格集合;已由既有断言保证 `knob∖{acl} == clamplo` 且 clamplo ⟺ seed1 timed out)。
- Produces: 128/256 列 25×2 格值 `BOUNDARY(native: LOAD-GEOM)`;新汇总表(pass 直绿/pass(bounded)/SKIP(legal)/BOUNDARY(native)/BOUNDARY(gem5)/合计 = 163/1/1/25/0/190、164/1/0/25/0/190、142/39/0/0/9/190、合计 469/41/1/50/9/570)。

- [ ] **Step 1: 头注纪律块补第 (f) 类字面量说明**

old(唯一命中):
```
#   (e) §5 对账行叙述数字的期望值（活锁：128 列 163/26、256 列 25/1、512 列
```
new:
```
#   (f) BOUNDARY(native: LOAD-GEOM) 分类（audit #645 口径：spec §7 的 native 绿 =
#       默认自适应【设计】几何 exit: pass，不绿走边界格通道）——25 个 §4 超时格
#       × 128/256 两列计边界格、不因有界绿改判；标签与归类引自 triage-256
#       §4/§5/§7 与 triage-128 §1/§5（非数据编造）。
#   (e) §5 对账行叙述数字的期望值（活锁：128 列 163/26、256 列 25/1、512 列
```
(注:(e) 行内「163/26、256 列 25/1」期望值在本 task 后续 Step 5 一并改为新口径,见该步。)

- [ ] **Step 2: cls() 边界类二分**

old:
```awk
function cls(c, arr) {
    if (c == "pass") arr["pass"]++
    else if (substr(c, 1, 13) == "pass(bounded:") arr["bounded"]++
    else if (substr(c, 1, 5) == "SKIP(") arr["skip"]++
    else if (substr(c, 1, 9) == "BOUNDARY(") arr["boundary"]++
    else arr["fail"]++
}
```
new:
```awk
function cls(c, arr) {
    if (c == "pass") arr["pass"]++
    else if (substr(c, 1, 13) == "pass(bounded:") arr["bounded"]++
    else if (substr(c, 1, 5) == "SKIP(") arr["skip"]++
    else if (c ~ /^BOUNDARY\(native:/) arr["bnative"]++
    else if (c ~ /^BOUNDARY\(gem5:/) arr["bgem5"]++
    else arr["fail"]++
}
```

- [ ] **Step 3: 128 列分类——clamplo 集合计边界格**

old:
```awk
        } else {
            c128 = (t in knob) ? "pass(bounded: " knob[t] ")" : "pass"
        }
```
new:
```awk
        } else {
            # 负载几何边界格（audit #645 记账口径）：设计几何（默认自适应）超 300 s
            # 预算的格计 BOUNDARY(native)，不因有界绿改判。128 档该 25 格设计几何
            # 未实测（腿自始 baked 有界、两 seed 腿内绿），机制同 256 档（几何输入
            # 不含 VL），外推强度「预期」（triage-128 §1/§5）。
            if (t in clamplo) c128 = "BOUNDARY(native: LOAD-GEOM)"
            else              c128 = (t in knob) ? "pass(bounded: " knob[t] ")" : "pass"
        }
```

- [ ] **Step 4: 256 列分类——timed out 计边界格 + ⟺ 断言**

old:
```awk
        # ---- 256 列：seed1 腿内 timed out 的格由 triage-256 §5 有界重跑转绿
        #      （同 seed 单测、clamp 下限一次通过）；seed2 腿内有界直绿。
        #      断言：timed out 的格必有 knob（§5 表全覆盖）。
        b1 = res[4 "," t]; b2 = res[5 "," t]
        if (b1 != "pass" && b1 != "timed out") { printf "FATAL: 256s1 bad result [%s] %s\n", b1, t > "/dev/stderr"; fatal = 1 }
        if (b2 != "pass")                    { printf "FATAL: 256s2 bad result [%s] %s\n", b2, t > "/dev/stderr"; fatal = 1 }
        if (b1 == "timed out" && !(t in knob)) { printf "FATAL: 256s1 timeout without knob: %s\n", t > "/dev/stderr"; fatal = 1 }
        if (b1 == "pass" || b2 == "pass")
            c256 = (t in knob) ? "pass(bounded: " knob[t] ")" : "pass"
        else { printf "FATAL: 256 cell not green: %s (%s/%s)\n", t, b1, b2 > "/dev/stderr"; fatal = 1; c256 = "FAIL" }
```
new:
```awk
        # ---- 256 列：设计几何（默认自适应）超 300 s 预算的格 = BOUNDARY(native)
        #      （audit #645 记账口径：spec §7 的 native 绿 = 设计几何 exit: pass，
        #      不绿走边界格通道，不许缩负载改判绿）。其有界绿证据保留：seed1 =
        #      triage-256 §5 腿外单测（clamp 下限一次通过），seed2 = 腿内 baked
        #      直绿（b2 必须 pass，下方断言）；其余格任一 seed pass 即绿。
        #      断言：timed out ⟺ §4 clamp 表（集合双向相等，§5 表全覆盖）。
        b1 = res[4 "," t]; b2 = res[5 "," t]
        if (b1 != "pass" && b1 != "timed out") { printf "FATAL: 256s1 bad result [%s] %s\n", b1, t > "/dev/stderr"; fatal = 1 }
        if (b2 != "pass")                    { printf "FATAL: 256s2 bad result [%s] %s\n", b2, t > "/dev/stderr"; fatal = 1 }
        if (b1 == "timed out" && !(t in knob)) { printf "FATAL: 256s1 timeout without knob: %s\n", t > "/dev/stderr"; fatal = 1 }
        if ((b1 == "timed out") != (t in clamplo)) { printf "FATAL: 256s1 timeout set != triage-256 §4 clamp set: %s (timed out=%s)\n", t, b1 > "/dev/stderr"; fatal = 1 }
        if (b1 == "timed out")
            c256 = "BOUNDARY(native: LOAD-GEOM)"
        else if (b1 == "pass" || b2 == "pass")
            c256 = (t in knob) ? "pass(bounded: " knob[t] ")" : "pass"
        else { printf "FATAL: 256 cell not green: %s (%s/%s)\n", t, b1, b2 > "/dev/stderr"; fatal = 1; c256 = "FAIL" }
```

- [ ] **Step 5: 汇总断言块改口径(含头注 (e) 期望值同步)**

old:
```awk
    if (p128 != 163)        { printf "FATAL: 128 narrative expects 163 default-adaptive (got %d)\n", p128 > "/dev/stderr"; fatal = 1 }
    if (q128 != 26)         { printf "FATAL: 128 narrative expects 26 baked bounded (got %d)\n", q128 > "/dev/stderr"; fatal = 1 }
    if (n_to != 25)         { printf "FATAL: 256 narrative expects 25 bounded reruns (got %d)\n", n_to > "/dev/stderr"; fatal = 1 }
    if (q256 - n_to != 1)   { printf "FATAL: 256 narrative expects 1 baked calibration (got %d)\n", q256 - n_to > "/dev/stderr"; fatal = 1 }
```
new:
```awk
    if (p128 != 163)        { printf "FATAL: 128 narrative expects 163 default-adaptive (got %d)\n", p128 > "/dev/stderr"; fatal = 1 }
    if (q128 != 1)          { printf "FATAL: 128 narrative expects 1 acl calibration bounded (got %d)\n", q128 > "/dev/stderr"; fatal = 1 }
    if (cnt128["bnative"] != 25) { printf "FATAL: 128 narrative expects 25 native LOAD-GEOM boundary (got %d)\n", cnt128["bnative"] > "/dev/stderr"; fatal = 1 }
    if (n_to != 25)         { printf "FATAL: 256 narrative expects 25 load-geometry timeout cells (got %d)\n", n_to > "/dev/stderr"; fatal = 1 }
    if (q256 != 1)          { printf "FATAL: 256 narrative expects 1 acl calibration bounded (got %d)\n", q256 > "/dev/stderr"; fatal = 1 }
    if (cnt256["bnative"] != 25) { printf "FATAL: 256 narrative expects 25 native LOAD-GEOM boundary (got %d)\n", cnt256["bnative"] > "/dev/stderr"; fatal = 1 }
```
同时(同 commit 内,断言块上方注释块里的派生断言论据需改写——`s256≡y256≡0`「c256 无 SKIP/BOUNDARY 赋值路径」已不成立:新口径下 c256 有 BOUNDARY(native) 赋值路径且 bn256==25 已活锁;s128 仍为派生(p128+q128+bn128 活锁 + 列和 190 蕴含);p256==164 仍为派生(q256/bn256 活锁 + 列和蕴含,不单列——派生断言=虚假覆盖,#4439046566 先例)。注释按此改写,不新设死代码断言。)
并删除断言(新口径下有界不再 ⊇ 超时集,该断言必炸):
```awk
    if (cnt256["bounded"] < n_to) { printf "FATAL: 256 bounded %d < timeout-rerun %d\n", cnt256["bounded"], n_to > "/dev/stderr"; fatal = 1 }
```
头注 (e) 行期望值同步:
old: `#   (e) §5 对账行叙述数字的期望值（活锁：128 列 163/26、256 列 25/1、512 列`
new: `#   (e) §5 对账行叙述数字的期望值（活锁：128 列 163/1/25、256 列 25/1/25、512 列`

- [ ] **Step 6: p/q/s/y 赋值行 + 列和断言改新类目**

old:
```awk
    p128 = cnt128["pass"]; q128 = cnt128["bounded"]; s128 = cnt128["skip"]; y128 = cnt128["boundary"]
    p256 = cnt256["pass"]; q256 = cnt256["bounded"]; s256 = cnt256["skip"]; y256 = cnt256["boundary"]
    p512 = cnt512["pass"]; q512 = cnt512["bounded"]; s512 = cnt512["skip"]; y512 = cnt512["boundary"]
```
new:
```awk
    p128 = cnt128["pass"]; q128 = cnt128["bounded"]; s128 = cnt128["skip"]
    p256 = cnt256["pass"]; q256 = cnt256["bounded"]; s256 = cnt256["skip"]
    p512 = cnt512["pass"]; q512 = cnt512["bounded"]; s512 = cnt512["skip"]
    y512 = cnt512["bgem5"]    # 512 列唯一 BOUNDARY 来源（native 边界格仅 128/256 列）
```
old:
```awk
    c128n = cnt128["pass"] + cnt128["bounded"] + cnt128["skip"] + cnt128["boundary"]
    c256n = cnt256["pass"] + cnt256["bounded"] + cnt256["skip"] + cnt256["boundary"]
    c512n = cnt512["pass"] + cnt512["bounded"] + cnt512["skip"] + cnt512["boundary"]
```
new:
```awk
    c128n = cnt128["pass"] + cnt128["bounded"] + cnt128["skip"] + cnt128["bnative"] + cnt128["bgem5"]
    c256n = cnt256["pass"] + cnt256["bounded"] + cnt256["skip"] + cnt256["bnative"] + cnt256["bgem5"]
    c512n = cnt512["pass"] + cnt512["bounded"] + cnt512["skip"] + cnt512["bnative"] + cnt512["bgem5"]
```

- [ ] **Step 7: §5 汇总 printf 块整体替换**

old(自 `M = OUT "/math.md"` 至 `+ gem5-overrides.txt（键唯一契约：一测试一行，重复即被双消费者拒绝）。\n" > M` 整块,即 :431-444):
```awk
    M = OUT "/math.md"
    printf "| 列 | pass（直绿） | pass(bounded) | SKIP(legal) | BOUNDARY(gem5) | 合计 |\n" > M
    printf "|---|---|---|---|---|---|\n" > M
    printf "| 128 | %d | %d | %d | %d | %d |\n", p128, q128, s128, y128, c128n > M
    printf "| 256 | %d | %d | %d | %d | %d |\n", p256, q256, s256, y256, c256n > M
    printf "| 512 | %d | %d | %d | %d | %d |\n", p512, q512, s512, y512, c512n > M
    P = p128 + p256 + p512; Q = q128 + q256 + q512; S = s128 + s256 + s512; Y = y128 + y256 + y512
    printf "| **合计** | **%d** | **%d** | **%d** | **%d** | **%d** |\n", P, Q, S, Y, P + Q + S + Y > M
    printf "\n" > M
    printf "**%d = %d pass（%d 直绿 + %d 有界）+ %d SKIP(legal) + %d BOUNDARY(gem5)**，零 FAIL、零非法 SKIP。\n\n", P + Q + S + Y, P + Q, P, Q, S, Y > M
    printf "- **128 列 = %d pass + %d SKIP(legal)** —— 对账 triage-128 §5（两 seed 各 189 腿内绿 = 163 默认自适应直绿 + 26 格腿内 baked 有界（= 25 项 clamp 下限 + 1 项 acl 标定，构成同 §2 legend @128；clamp 下限 = 最小激发强度，语义限定统一见 triage-256 §5「证据强度限定」）；189 与 26 计数对账 triage-128 §5（该表收口口径未携带限定语义），163/25/1 拆分与限定语义对账 triage-256 §1/§5/§7），另有 1 合法 SKIP：fmmla@128，两 seed 同格同理。\n", p128 + q128, s128 > M
    printf "- **256 列 = %d pass** —— 对账 triage-256 §7（seed1 165 腿内绿（164 格默认自适应直绿 + 1 格 acl 标定 baked）+ 25 格 §5 有界重跑 = 190/190；seed2 190 格腿内绿（164 默认自适应直绿 + 1 格 acl 标定 baked + 25 格腿内 clamp 下限有界））；%d 个有界格 = %d 格 §5 有界重跑（seed1 腿内 to → 同 seed 单测 clamp 下限重跑绿——clamp 下限 = 最小激发强度，语义限定统一见 triage-256 §5「证据强度限定」）+ %d 格 baked 腿内绿（acl dim=512 标定，Ruling B / triage-256 §1——baked 有界几何、证据强度与有界绿同向受限，非默认自适应直绿等同；按下行有界格计法计入 26 有界格之列、亦计入 165 腿内绿之列，两口径两属）。\n", p256 + q256, q256, n_to, q256 - n_to > M
    printf "- **512 列 = %d pass + %d BOUNDARY** —— 对账 triage-512 §6（181 绿 + 9 边界[5 crash + 4 timeout] + 0 fail）；%d 个有界格 = baked 26 项中绿 %d（eigen_svd_cdouble 转边界格除外）+ gem5-overrides 16 条中绿 %d（mrn_rmw_diff_1m 无效仍边界除外）− 两处重叠 %d（acl：baked 512 被 extras 覆盖为 128）。\n", p512 + q512, y512, q512, bg, eg, bo > M
    printf "- **有界格计法**：该格绿证据的产生命令里带该测试的几何 knob 覆盖——native 三处命令行头（机械提取）+ gem5-overrides.txt（键唯一契约：一测试一行，重复即被双消费者拒绝）。\n" > M
```
new:
```awk
    M = OUT "/math.md"
    printf "| 列 | pass（直绿） | pass(bounded) | SKIP(legal) | BOUNDARY(native) | BOUNDARY(gem5) | 合计 |\n" > M
    printf "|---|---|---|---|---|---|---|\n" > M
    printf "| 128 | %d | %d | %d | %d | %d | %d |\n", p128, q128, s128, cnt128["bnative"], cnt128["bgem5"], c128n > M
    printf "| 256 | %d | %d | %d | %d | %d | %d |\n", p256, q256, s256, cnt256["bnative"], cnt256["bgem5"], c256n > M
    printf "| 512 | %d | %d | %d | %d | %d | %d |\n", p512, q512, s512, cnt512["bnative"], cnt512["bgem5"], c512n > M
    P = p128 + p256 + p512; Q = q128 + q256 + q512; S = s128 + s256 + s512
    BN = cnt128["bnative"] + cnt256["bnative"] + cnt512["bnative"]
    YG = cnt128["bgem5"] + cnt256["bgem5"] + cnt512["bgem5"]
    printf "| **合计** | **%d** | **%d** | **%d** | **%d** | **%d** | **%d** |\n", P, Q, S, BN, YG, P + Q + S + BN + YG > M
    printf "\n" > M
    printf "**%d = %d pass（%d 直绿 + %d 有界）+ %d SKIP(legal) + %d BOUNDARY(native: 负载几何) + %d BOUNDARY(gem5)**，零 FAIL、零非法 SKIP。\n\n", P + Q + S + BN + YG, P + Q, P, Q, S, BN, YG > M
    printf "- **128 列 = %d pass（%d 直绿 + %d acl 标定）+ %d SKIP(legal) + %d BOUNDARY(native)** —— 对账 triage-128 §2/§5（验收记账 = spec §7 口径，audit #645：25 个负载几何格计边界格——本档设计几何未实测（腿自始烧入 26 项有界覆盖、两 seed 腿内有界直绿），机制同 256 档（triage-256 §4：几何由物理内存比例与线程数推导、输入不含 VL），外推强度「预期」；有界绿 = 功能正确性证据（clamp 下限，最小激发强度），语义限定统一见 triage-256 §5「证据强度限定」）；另有 1 合法 SKIP：fmmla@128，两 seed 同格同理。\n", p128 + q128, p128, q128, s128, cnt128["bnative"] > M
    printf "- **256 列 = %d pass（%d 直绿 + %d acl 标定）+ %d BOUNDARY(native)** —— 对账 triage-256 §7（spec §7 口径，audit #645：seed1 腿内 165 绿 + 25 timed out（§1），25 格按负载几何机制（§4：环境/边界类、与 seed 无关）计边界格，其 clamp 下限有界绿（§5：seed1 腿外单测 + seed2 腿内 baked）为功能正确性证据（最小激发强度），不计入绿；acl dim=512 为 Ruling B 标定、缩比 2×线性与 25 格的 3~4 个数量级不同量级，仍计 pass(bounded)，证据强度同向受限）。\n", p256 + q256, p256, q256, cnt256["bnative"] > M
    printf "- **512 列 = %d pass + %d BOUNDARY** —— 对账 triage-512 §6（181 绿 + 9 边界[5 crash + 4 timeout] + 0 fail）；%d 个有界格 = baked 26 项中绿 %d（eigen_svd_cdouble 转边界格除外）+ gem5-overrides 16 条中绿 %d（mrn_rmw_diff_1m 无效仍边界除外）− 两处重叠 %d（acl：baked 512 被 extras 覆盖为 128）；本列声明语义 = 512 档功能性正确性（压力维度由 native 腿承担，triage-512 §0/§2），有界绿计 pass(bounded) 名实相符、不计边界格。\n", p512 + q512, y512, q512, bg, eg, bo > M
    printf "- **有界格计法**：该格绿证据的产生命令里带该测试的几何 knob 覆盖——native 三处命令行头（机械提取）+ gem5-overrides.txt（键唯一契约：一测试一行，重复即被双消费者拒绝）。native 列（128/256）的 25 个 clamp 下限有界格不因有界绿计 pass(bounded) 而计 BOUNDARY(native)（audit #645 记账口径），其有界绿证据仍由同款命令行头与 triage-256 §5 表承担。\n" > M
```

- [ ] **Step 8: HDR 验收判据行 + HDR2 图例行/种子语义/证据列 bullet 改口径**

HDR old(唯一命中):
```
- 验收判据（brief Step 2）：570 格全绿**或有显式机制性理由**（1 格合法 SKIP + 9 格 gem5 边界，见 §2/§5）
```
HDR new:
```
- 验收判据（brief Step 2）：570 格全绿**或有显式机制性理由**（1 格合法 SKIP + 50 格 native 负载几何边界 + 9 格 gem5 边界，见 §2/§5；native 格的绿 = 默认自适应【设计】几何 `exit: pass`——25 个设计几何超 300 s 预算格 × 128/256 两档计边界格，audit #645 记账口径）
```
HDR2 图例 `pass(bounded: …)` 行 old(唯一命中,以 `| \`pass(bounded: <knob>=<v>)\` |` 开头的整行):
```
| `pass(bounded: <knob>=<v>)` | 以有界几何 knob 覆盖后绿。**@128** = 26 项腿内 baked（triage-128 §1：逐字拷贝自 256 腿 seed2，构成同 @256——25 项 clamp 下限 + 1 项 `acl_gemm_sve.dim=512` 标定 baked（Ruling B，dim clamp [64,4096]、512 非下限）；两 seed 腿内有界实测）；**@256** = 25 格 seed1 有界重跑（triage-256 §5：clamp 下限一次通过、零上调——clamp 下限 = 最小激发强度，语义限定统一见 triage-256 §5「证据强度限定」）+ seed2 腿内有界 + `acl_gemm_sve.dim=512` 标定 baked（triage-256 §1，Ruling B）；**@512** = 26 项 baked + `gem5-overrides.txt` 16 条中生效的 15 条（triage-512 §1.1/§5.2；`mrn_rmw_diff_1m` 那条无效，见其边界格） | 对应 triage 分节 |
```
HDR2 new(两行——改写 + 紧随其后新增 BOUNDARY(native) 行):
```
| `pass(bounded: <knob>=<v>)` | 以有界几何 knob 覆盖后绿。**@128/@256** = 唯一格 `acl_gemm_sve.dim=512` 标定 baked（Ruling B，dim clamp [64,4096]、512 非下限——缩比 2×线性，与 clamp 下限格的 3~4 个数量级缩比不同量级；证据强度与有界绿同向受限，triage-256 §1/§5）；**@512** = 26 项 baked + `gem5-overrides.txt` 16 条中生效的 15 条（triage-512 §1.1/§5.2；`mrn_rmw_diff_1m` 那条无效，见其边界格；本列声明语义 = 功能性正确性——压力维度由 native 腿承担，triage-512 §0/§2） | 对应 triage 分节 |
| `BOUNDARY(native: LOAD-GEOM)` | native 负载几何边界（audit #645 记账口径：spec §7 的 native 绿 = 默认自适应【设计】几何 `exit: pass`，不绿走边界格通道，不许缩负载改判绿）：设计几何（默认自适应——600‰/650‰/900‰ 内存比例推导）× 300 s 单轮预算超限，机制见 triage-256 §4（环境/边界类、与 seed 无关）。**@256** = seed1 腿内实测 timed out（§1/§4），clamp 下限有界绿（§5：seed1 腿外单测 + seed2 腿内 baked）= 功能正确性证据（最小激发强度，非满强度——triage-256 §5「证据强度限定」）；**@128** = 设计几何未实测（腿自始 baked 有界、两 seed 腿内绿，triage-128 §1），机制同 256 档、几何输入不含 VL，外推强度「预期」 | triage-256 §4/§5/§7、triage-128 §1/§5 |
```
HDR2 种子语义 bullet old(唯一命中):
```
- **种子语义**：native 格 = 任一 seed pass 即 pass；实测两 seed 无分歧绿（128 腿
  189+1 双 seed 机械一致；256 腿 165 格双 seed 机械一致，唯 seed1 的 25 个超时格
  （证据列 `to`）的**入库机械证据仅 seed2 单腿**——seed1 侧绿证据 = triage-256
  §5 的有界重跑散文表（同 seed 单测、clamp 下限一次通过；其原始 /tmp 日志易失
  未入库，不构成可再生成证据，本矩阵不以再生成能力主张该侧）。
```
new:
```
- **种子语义**：native 直绿/标定格 = 任一 seed pass 即 pass；实测两 seed 无分歧绿
  （128 腿 163+26+1 双 seed 机械一致；256 腿 165 格双 seed 机械一致）。25 个
  BOUNDARY(native) 格不因有界绿改判（audit #645 记账口径）：@256 seed1 腿内
  timed out（证据列 `to`）、seed2 腿内有界绿——其入库机械证据仅 seed2 单腿，
  seed1 侧绿证据 = triage-256 §5 的有界重跑散文表（同 seed 单测、clamp 下限一次
  通过；其原始 /tmp 日志易失未入库，不构成可再生成证据，本矩阵不以再生成能力
  主张该侧）；@128 两 seed 腿内有界绿（设计几何未实测）。
```
HDR2 证据列 bullet 内 `to` 释义 old(唯一命中):
```
`to` = 该 seed 腿内 timed out（其绿证据 =
  triage-256 §5 有界重跑，同 seed 单测）
```
new:
```
`to` = 该 seed 腿内 timed out（其有界绿证据 =
  triage-256 §5 有界重跑，同 seed 单测；格值计 BOUNDARY(native)，不因有界绿改判）
```

- [ ] **Step 9: 再生 matrix.md + 断言全过 + 差异核验**

```bash
cd docs/superpowers/inventory/sve-all-vl-campaign
./matrix-gen.sh > matrix.md.new && mv matrix.md.new matrix.md && echo GEN-OK
git diff --stat matrix.md
grep -c 'BOUNDARY(native: LOAD-GEOM)' matrix.md        # 预期 51 = 50 格值(25 行 × 128/256 两列) + 1 图例行
sed -n '/^## 5\./,$p' matrix.md | head -20             # 核对汇总表 = 下表
```
Expected 汇总表(逐字):
```
| 列 | pass（直绿） | pass(bounded) | SKIP(legal) | BOUNDARY(native) | BOUNDARY(gem5) | 合计 |
|---|---|---|---|---|---|---|
| 128 | 163 | 1 | 1 | 25 | 0 | 190 |
| 256 | 164 | 1 | 0 | 25 | 0 | 190 |
| 512 | 142 | 39 | 0 | 0 | 9 | 190 |
| **合计** | **469** | **41** | **1** | **50** | **9** | **570** |
```
及标题行 `**570 = 510 pass（469 直绿 + 41 有界）+ 1 SKIP(legal) + 50 BOUNDARY(native: 负载几何) + 9 BOUNDARY(gem5)**，零 FAIL、零非法 SKIP。`
并抽查 2 行格值(如 agu_stress_2src_sve 行:128/256 列均 `BOUNDARY(native: LOAD-GEOM)`、512 列 `pass(bounded: count=1024)`、证据列 `pass,pass · to,pass · rc=0`;acl_gemm_sve 行:128/256 列均 `pass(bounded: dim=512)`)。

- [ ] **Step 10: Commit + push**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/matrix-gen.sh docs/superpowers/inventory/sve-all-vl-campaign/matrix.md
git commit -s -m "fix(matrix): 验收记账机械改真——LOAD-GEOM 边界格类(25×2 native 格),汇总 510/41/1/50/9,断言与文案同步 (issue #645)"
git push origin fix/issue-645
```

### Task 5: README + progress-log 外部同步

**Files:**
- Modify: `README.md`「SVE 用例全 VL 验证矩阵」段(:357 收口行)。
- Modify: `docs/research/progress-log.md`(文件头部新增 2026-11-15 条目——先读头部确认条目排序方向,新条目插在与既有排序一致的位置;**不改写** :325 历史 条目——日志按时间留痕,新条目即修正)。

**Interfaces:**
- Consumes: Task 4 汇总数字(510/41/1/50/9)与术语。

- [ ] **Step 1: README 收口行改真**

old(唯一命中,该段以 `190 个 SVE 用例（tag∪name 枚举口径）` 开头的整段;只替换下述两个片段,其余含「边界注记①②」与「证据钉在冻结基线」句保留):
片段 1 old:
```
（570 格）：**560 pass（469 直绿 + 91 有界几何）+ 1 合法 SKIP + 9 gem5 边界格 + 0 FAIL**，本轮运动零测试代码缺陷
```
片段 1 new:
```
（570 格）：**510 pass（469 直绿 + 41 有界几何）+ 1 合法 SKIP + 50 native 负载几何边界格 + 9 gem5 边界格 + 0 FAIL**（验收记账 = spec §7 口径，issue #645：native 格的绿 = 默认自适应【设计】几何 `exit: pass`，25 个设计几何超 300 s 预算格 × 128/256 两档计边界格——其 clamp 下限有界绿为功能正确性证据＝最小激发强度、非满强度，量化与限定见 triage-256 §5「证据强度限定」），本轮运动零测试代码缺陷
```
片段 2 old:
```
各 2 seeds；其中 25 个超时格以 **clamp 下限最小激发几何**取证（128 腿逐字拷贝同值的 26 项 baked = 此 25 项 clamp 下限 + 1 项 `acl_gemm_sve.dim=512` Ruling B 标定，同为远小于自适应值的有界几何）——clamp 下限 = 最小激发强度，功能正确性成立、**不构成满强度激发证据**，量化与限定见 triage-256 §5「证据强度限定」）
```
片段 2 new:
```
各 2 seeds；25 个负载几何边界格以 **clamp 下限最小激发几何**取证（128 腿逐字拷贝同值的 baked = 此 25 项 clamp 下限 + 1 项 `acl_gemm_sve.dim=512` Ruling B 标定）——clamp 下限 = 最小激发强度，功能正确性成立、**不构成满强度激发证据**，验收计边界格）
```

- [ ] **Step 2: progress-log 追加修正条目**

先 `sed -n '1,40p' docs/research/progress-log.md` 确认条目排序与格式,再按同款格式插入:
```markdown
## 2026-11-15（issue #645：SVE 全 VL 矩阵验收口径改真——25 个负载几何超时格计 native 边界格）

- **现象（audit 复核成立）**：triage-256 §4 将 25 个超时格定性「环境/边界类」，§7 却宣布
  「190/190 全绿、边界格 = 0」——把「边界格」重定义为「找不到有界值的残留」，偏离
  spec §7 验收判据（native 格的绿 = 设计几何 `exit: pass`；不绿走边界格通道）。
  seed1 summary 机械对账：165 pass + 25 timed out = 190（复现）。
- **修复**：25 格 × 128/256 两 native 列计 `BOUNDARY(native: LOAD-GEOM)`（有界绿保留为
  功能正确性证据＝最小激发强度，非满强度）；512 列不动（声明语义 = 功能性正确性，
  压力维度由 native 腿承担）；acl_gemm_sve.dim=512 标定 1 格保留 pass(bounded)
  （Ruling B 标定、缩比 2×线性，与 3~4 个数量级不同量级——残留判断点，已在 PR 声明）。
  汇总 560 pass（469+91）→ **510 pass（469 直绿 + 41 有界）+ 1 SKIP + 50 native 边界
  + 9 gem5 边界**。同步 triage-256/128/512、matrix-gen.sh+matrix.md（机械再生、断言
  全过）、README；triage-512「四格 native 全绿」隐藏失实（eigen_svd_cdouble 本在 25
  名单）一并改真。旧收口行（本日志 2026-10-30 条：560 pass 口径）系当时口径，本条
  即其修正，不回改历史条目。
- **验证**：`./matrix-gen.sh` 断言全过、基线可复现性先行验证；zstd19 回归
  `-t 2000 -n 1` exit: pass；纯文档+脚本、零代码路径改动（x86-64 非回归由零代码
  改动蕴含）。PR #<N>（fix/issue-645）。
```
(PR 号在开 PR 后回填——若本 commit 先于 PR,写「PR: fix/issue-645 分支」并接受后补,或把本 Step 移到 Task 6 开 PR 之后执行;执行时择一,不得虚构 PR 号。)

- [ ] **Step 3: 全仓残句扫描(收尾质检)**

```bash
grep -rn "560 pass\|91 有界\|边界格 = 0\|190/190 全绿" README.md CLAUDE.md docs/ --include='*.md' | grep -v progress-log.md
# 预期:零命中(progress-log 历史条目除外);docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md 的
# 「验收自查——570 格全绿或有显式机制性理由」为 spec §7 复述、口径为真,保留。
```

- [ ] **Step 4: Commit + push**

```bash
git add README.md docs/research/progress-log.md
git commit -s -m "docs(readme): SVE 全 VL 矩阵收口行同步 issue #645 口径 + progress-log 修正条目"
git push origin fix/issue-645
```

### Task 6: 终验、开 PR、贴报告

**Files:** 无新改动(若 Task 0 Step 4 发现构建依赖受限,在此如实处理并声明)。

- [ ] **Step 1: 构建 + 回归(CLAUDE.md rule 2;#592 docs-only 先例)**

```bash
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release 2>&1 | tail -3
./scripts/build.sh 2>&1 | tail -5        # 零新警告
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -5   # 期望 exit: pass
```
若构建依赖缺失致 zstd19 不可跑:如实记录缺失清单与命令输出,按 README「缺 install/ 优雅降级」口径声明验证边界——**不得**以「应该能过」替代。

- [ ] **Step 2: 终态自检**

```bash
git status           # clean
git log --oneline origin/main..fix/issue-645   # 6 个 commit(计划/triage×3/matrix/readme+plog)
git diff origin/main..fix/issue-645 --stat     # 仅 7 个文件,零 tests/ framework/ 改动
```

- [ ] **Step 3: 开 PR(兜底 pr edit)**

```bash
gh pr create --base main --head fix/issue-645 \
  --title "fix: SVE 全 VL 矩阵验收口径改真——25 个负载几何超时格 × 128/256 计 native 边界格 (#645)" \
  --body "Closes #645 + 实现修复报告"
```
PR body 必含:审计三项主张的复现输出(Task 0)、根因(口径偷换+自相矛盾点行号)、改动清单(6 commits)、验证证据(基线可复现/GEN-OK/汇总表逐字/zstd19)、残留判断点(acl 标定保留理由;128 档设计几何未实测的外推强度声明)。若因分支已有 open PR 失败 → `gh pr edit fix/issue-645 --title … --body …`(必含 Closes #645)。

- [ ] **Step 4: issue #645 贴报告(不 resolve)**

报告 = PR body 同款(方案/根因、改动、验证证据、PR 链接),`gh issue comment 645`。

- [ ] **Step 5: 结束语**

成功:输出一行 `task-loop: done fix/issue-645`;未完成:输出一行 `task-loop: escalated <原因>`。

---

## Self-Review 记录

- **Spec 覆盖**:审计三项主张(①计数口径 ②自相矛盾 ③SDC 语境实质弱化)→ Task 1(核心)+Task 4(机械载体)全覆盖;口径扩散面(matrix/README/triage-128/triage-512/progress-log)→ Tasks 2-5 全覆盖;512 列与 acl 格的不动理由已在 Architecture 与各 task 文案中显式声明(残留判断点进 PR/报告)。
- **占位符扫描**:各 old/new 均为逐字目标文本;triage-512 三处、README 两片段、progress-log 条目均已给全文;无 TBD/「适当处理」。
- **一致性**:awk 类目键 `bnative`/`bgem5` 在 cls()/列和断言/汇总 printf 间一致;`clamplo` 既是值锁输入又是分类依据(既有断言已保证 knob∖{acl}==clamplo ⟺ timed out);数字链 163+1+1+25=190 / 164+1+25=190 / 142+39+9=190 / 469+41+1+50+9=570 自洽。
