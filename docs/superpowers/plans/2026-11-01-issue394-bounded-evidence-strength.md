# 256 腿 25 个有界格「证据强度限定」语义修正 Implementation Plan（issue #394）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> 本会话为 task-loop 派发的单 agent 缺陷修复任务，按工作契约顺序**本会话内联执行**（executing-plans）。

**Goal:** 修正 triage-256.md 把「clamp 下限起步、首次通过即止」的 25 个有界重跑格呈现为与直绿格等价绿格的正确性缺陷——在分诊表、生成器产物矩阵与 README 三处为这 25 格补上**最小激发强度**的证据强度限定，使「native 256 档两 seed 全绿 / 边界格 = 0」结论不再把最小激发强度证据当满强度证据。

**Architecture:** 纯文档/生成器文本修改，零代码行为变更。语义修正的主阵地是 triage-256.md（audit 指认文档：§5 增「证据强度限定」块含量化缩比表，§7 标题与三条 bullet 带限定，§1 加指针）；matrix.md 由 matrix-gen.sh 机械生成（**不许手填**），故矩阵侧改生成器两处 printf 后再生成；README 运动总结句同口径加一从句。修复形状对齐先例 39a3c17e（PR #307 #35：同为该 25 格的证据语义精确化，彼次修机械证据源维度，本次修激发强度维度）。

**Tech Stack:** Markdown 文档、bash/awk 生成器（matrix-gen.sh，LC_ALL=C，内置 FATAL 断言）。

**Spec:** issue #394（gh issue view 394，task-loop:audit:cc0f8836c8a9944d）——「25/190（13%）矩阵格以『clamp 下限起步、首次通过即止』的有界重跑计绿……把最小激发强度的证据当成满强度证据……标题未携带该限定」。

> **⚠ 执行勘误（2026-11-01 PR #404 检视轮次后补，先于下方原文）**：本计划入库后，
> 其交付文本经 PR #404 检视处置修正（commit `aef95657`/`ebb6edb4`，2026-11-02 两轮
> rebase 线性化后对应 `e9269b17`/`8625c5e5`——旧 SHA 系 rebase 前形态，本分支历史
> 已不可达，核验以新 SHA 为准）——**下方 Task 2/3/4
> 原文中的下列口径已被证伪，重执行者不得按原文逐字回退**，以入库交付文本
> （triage-256.md / matrix-gen.sh / matrix.md / README.md）为准：
> 1. **「165 格默认自适应」实为 164 默认自适应 + 1 格 `acl_gemm_sve.dim=512` 标定
>    baked（Ruling B）**——Task 2 Step 4 的 §7 标题/seed1/seed2 bullet 与 Task 3
>    Step 1 的对账行均须按 164+1 拆分写；acl 的 dim clamp 实为 [64,4096]（源码
>    `sdc_knob_int_clamped(test,"dim",0,64,4096)`），512 是标定中间档、非下限。
> 2. **Task 3 Step 2 @128 legend 子句「26 项 = 同一组 clamp 下限值」为假**——26 项
>    baked = 25 项 clamp 下限 + 1 项 acl 标定（128 腿命令行第一项即
>    `-O acl_gemm_sve.dim=512`，逐字拷贝自 256 腿 seed2）。
> 3. **Task 4 Step 1 README 从句**同口径：主语收窄为 25 格，26 项 baked 须拆明
>    「25 clamp 下限 + 1 acl 标定」。
> 4. **Task 3 Step 3 的自验关卡自相矛盾**：`git diff matrix.md | grep -c '^[+-]|'`
>    期望 0——但 legend 行与表体数据行同为 `|` 起始，Step 2 自己改写的 legend 行
>    在 diff 中必呈 `+|`/`-|` 命中该模式，计数恒 ≥1，该关卡按写定方式永不为 0。
>    实际执行的等价验证（已过）：`bash -n matrix-gen.sh` + `./matrix-gen.sh` exit 0
>    （内置 FATAL 断言全过）+ 再生成 diff 逐 hunk 判读恰为预期文本改动、190 行表体
>    零变化。
> 5. **缩比口径「从 4.0~36.7 GiB 级收到 KB~MiB 级」（Task 2 Step 3 引导句）与
>    「工作集较默认自适应几何缩小数个数量级」（Task 3 Step 1 printf / Task 4
>    Step 1 README 从句）已被按族分档证伪**——两个 dense SVD 族收缩最浅
>    （bidiag ≈23 MiB 级、cdouble ≈2.2 GiB 级，维数缩比仅 ×32/×4.3），重执行须按
>    交付文本 triage-256 §5 的按族量级表述；交付侧「数个数量级」已删（README/
>    matrix-gen.sh/matrix.md/triage-256.md 四文件 grep 零命中）。
> 6. **「2 s 定时压测跑满」（Task 2 Step 3 结论段）为假**——交付实测非每格打满
>    （50 次中 2 格显著低于预算、最浅 858.7 ms ≈ 43%；10 次预算的 92%~99%；38 次
>    打满），重执行按 triage-256 §5 现行分布描述改写，不得回退为「跑满」。

## Global Constraints

- 一个补丁一个单元：本计划全部改动 = 一个 commit（计划文档随补丁入库，先例 6902536c）。
- DCO：`git commit -s`，最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无内容。
- 分支纪律：`git checkout -B fix/issue-394 origin/main`，只推 fix/issue-394，绝不推 main、绝不 merge、绝不 resolve。
- matrix.md 只能由 `./matrix-gen.sh > matrix.md` 再生成，不手填数据行；再生成 diff 必须恰好等于生成器文本改动的镜像，表体数据行零变化。
- 所有量化数字必须机械派生自 triage-256.md 自身的 §4/§5 两表（已逐一核算，见 Task 2 的核对表），不得引用 issue 正文里未经复核的数字（issue 的「7568×」实为 ×7,575，以文档实测值为准）。
- 验证边界如实声明：无编译产物受影响（不触碰 tests/ framework/），受影响行为 = matrix.md 生成 → 以再生成 diff + 生成器内置断言验证；不伪造 build/test 证据。
- 不扩展到 triage-128.md / triage-512.md 的正文改写（未被 audit 指认，若需是独立单元）；128 腿同值事实仅在 README 从句与矩阵 legend 从句中如实提及。

---

### Task 1: 基线再生成零 diff 复现（生成器侧的「复现」步骤）

**Files:**
- 无改动（只读验证）。

**Interfaces:**
- Produces: 「当前 HEAD 上 matrix-gen.sh 再生成 == 入库 matrix.md」的实证，作为 Task 3 改后 diff 判读的基线。

- [x] **Step 1: 从当前生成器再生成并 diff**

```bash
cd docs/superpowers/inventory/sve-all-vl-campaign
bash -n matrix-gen.sh && ./matrix-gen.sh > /tmp/matrix.baseline.md
diff /tmp/matrix.baseline.md matrix.md && echo BASELINE-REPRODUCIBLE
```

Expected: `BASELINE-REPRODUCIBLE`（零 diff；生成器全部 FATAL 断言通过，exit 0）。
若非零 diff：停止，如实报告 escalated（生成器与产物已漂移，先修漂移再谈语义）。

### Task 2: triage-256.md 证据强度限定（audit 主目标）

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md`（§1 :52 后、§5 :132 与 :167 后、§7 :197-211）

**Interfaces:**
- Produces: 「§5 证据强度限定」锚点（matrix-gen.sh 与 README 的限定语引用该节名）。

- [x] **Step 1: §1 pass 计数 bullet 后追加指针行**

在「- 两 seed 的 pass 计数均含 mce_check（……= 191 执行 191 绿）。」之后新增一行：

```markdown
- 「有界」的语义与证据强度：本表与 §5/§7 的「有界（重跑）」一律 = knob **clamp 下限**的
  最小激发几何（协议首次通过即止、零上调）——功能正确性证据成立，满强度激发证据不成立，
  量化与限定见 §5「证据强度限定」。
```

- [x] **Step 2: §5 协议行加前向指针**

:132 「取值从 knob 的 **clamp 下限**起步，若 FAIL 则 ×2 逐级上调直至 pass（记录每级原因）。」
句尾追加：「**该协议只确立「存在可通过的有界几何」**——证据强度含义见本节末尾「证据强度限定」。」

- [x] **Step 3: §5 结论段后插入「证据强度限定」块（含量化缩比表）**

在 「**结论**：25/25 在 clamp 下限全绿、零上调。……无任何 fail/crash。」段之后插入：

```markdown
**证据强度限定（如实声明）**：上表的「绿」是**存在性证据**——存在可通过的有界几何，且
实测该几何就是各 knob 的 clamp 下限（协议的 ×2 上调阶梯零触发，即从未向上搜索最大可
通过几何，下限与自适应值之间的中间档也一概未验证）。对照 §4 同格的自适应值，clamp
下限把单迭代工作集从 4.0~36.7 GiB 级收到 KB~MiB 级：

| 测试族（§4 行号） | 自适应值（§4 实拍） | 有界值（§5） | 缩比（knob 值） |
|---|---|---|---|
| count 族：agu ×2 / arm0102 ×5 / movmskpspd（#1-7,17） | 7,757,172~15,514,344 → 36.7 GiB | 1024 | ×7,575~15,151（agu ×7,575 / movmskpspd ×9,469 / arm0102 ×15,151） |
| ipsec data_size ×4（#11-14） | 16,777,216 → 11.9 GiB | 1024 | ×16,384 |
| gmp_bigadd limbs（#10） | 1,048,576 → 4.0 GiB | 256 | ×4,096 |
| sve_rot slots ×7 + k3res（#18-25） | 1,048,576 → 23.8 GiB | 1024 / 4096 | ×1,024 / ×256 |
| eigen_svd_bidiag mdim（#8） | 2048（撞上限）→ 23.8 GiB | 64 | 维数 ×32（dense 工作集 ∝ mdim² → ×1,024；单迭代 FLOPs ∝ mdim³ → ×32,768） |
| eigen_svd_cdouble mdim（#9） | ≈1277（自有 0.65 RAM 模型） | 300 | 维数 ×4.3（工作集 ∝ mdim² → ≈×18） |
| mem_disambig buffer_bytes（#15） | 60% 物理内存 ÷2（GB 级） | 4096 B | GB→KiB（~×10⁶） |
| mesh_upi buffer_bytes（#16） | 90% 物理内存 ÷2 路（GB 级） | 1 MiB | GB→MiB（~×10⁴） |

对以 SDC 激发为目的的本运动，这意味着这 25 格（seed1 腿外单测 + seed2 腿内有界，两
seed 各一次）的 VL=256 绿 = **最小激发强度**证据：功能正确性成立（同二进制、golden
字节比对全过、2 s 定时压测跑满），但单块地址跨度/数据多样性与工作集同比例缩小，
**不构成**默认自适应几何（满强度）的激发与通过证据——满强度几何在本腿的实测结果是
超时（§4，环境/预算边界，非缺陷）。§7 的收口口径以此为限定：这 25 格与 165 个直绿格
**不等价**。
```

- [x] **Step 4: §7 标题与 bullets 限定**

标题 :197 改为：

```markdown
**native 256 档两 seed 全绿（165 格默认自适应直绿 + 25 格 clamp 下限最小强度有界绿，
两者证据强度不等价——§5「证据强度限定」），Task 2 收口**：
```

seed1 bullet 改为：

```markdown
  - seed1（LCG:1001）：165 格腿内直绿（默认自适应几何；唯 acl_gemm_sve 为 dim=512
    标定，Ruling B/§1）+ 25 格有界重跑绿（§5 表，全部 clamp 下限一次通过、零上调 =
    最小激发强度）= **190/190**；另有框架追加 mce_check 绿。
```

seed2 bullet 改为：

```markdown
  - seed2（LCG:1002，26 项有界重跑腿）：190 格腿内绿（165 格默认自适应直绿 + 25 格
    腿内 clamp 下限有界）= **190/190**；mce_check 绿。`exit: pass`（exit=0），
    `--ignore-timeout` 未触发。
```

「边界格 = 0」bullet 改为：

```markdown
- **边界格 = 0**（25 个超时格全部找到可通过的有界值——即 clamp 下限，无"找不到
  有界值"的残留；注意其语义是**最小可通过几何的存在性证明**，满强度自适应几何在该
  25 格未被建立——其腿内实测为超时，见 §4/§5）。
```

观察项 ③ 句尾「直接按本表 clamp 下限协议有界化即可。」改为「直接按本表 clamp 下限
协议有界化即可（所得为最小强度证据，报告口径须携带 §5 同款限定）。」

- [x] **Step 5: 数字机械核对（缩比表 vs §4/§5 原表）**

核对表（执行时逐项 grep §4/§5 原值复核）：

| 分子（§4 自适应） | 分母（§5 有界） | 期望缩比 |
|---|---|---|
| 7,757,172 | 1024 | 7,575.4 → ×7,575 |
| 9,696,465 | 1024 | 9,469.2 → ×9,469 |
| 15,514,344 | 1024 | 15,150.7 → ×15,151 |
| 16,777,216 | 1024 | 16,384（精确） |
| 1,048,576 | 256 | 4,096（精确） |
| 1,048,576 | 1024 | 1,024（精确） |
| 1,048,576 | 4096 | 256（精确） |
| 2048 | 64 | 32（精确；²=1,024、³=32,768） |
| ≈1277 | 300 | 4.26（²≈18.1） |

### Task 3: matrix-gen.sh 两处措辞 + matrix.md 再生成

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/matrix-gen.sh:265`（§5 256 列对账行）、`:320`（§2 legend @128/@256 子句）
- Regenerate: `docs/superpowers/inventory/sve-all-vl-campaign/matrix.md`

**Interfaces:**
- Consumes: Task 2 的「§5 证据强度限定」节名（限定语引用它）。
- Produces: 与 triage-256.md 同口径的矩阵侧限定。

- [x] **Step 1: :265 256 列对账行 printf 文本改为**

```
- **256 列 = %d pass** —— 对账 triage-256 §7（seed1 165 腿内直绿 + 25 格 §5 有界重跑 = 190/190；seed2 190 格腿内绿（165 直绿 + 25 格腿内 clamp 下限有界））；%d 个有界格 = %d 格 §5 有界重跑（seed1 腿内 to → 同 seed 单测 clamp 下限重跑绿——**clamp 下限 = 最小激发强度**，工作集较默认自适应几何缩小数个数量级，证据强度弱于直绿格、不构成满强度激发证据，triage-256 §5「证据强度限定」）+ %d 格 baked 直绿（acl dim=512，Ruling B / triage-256 §1）。
```

（printf 格式符与参数 `p256 + q256, q256, n_to, q256 - n_to` 不变。）

- [x] **Step 2: :320 legend 两处子句改为**

@128 子句：「26 项腿内 baked（triage-128 §1：逐字拷贝自 256 腿 seed2——即同一组 clamp 下限值，最小激发强度语义同 @256；两 seed 腿内有界实测）」；
@256 子句：「25 格 seed1 有界重跑（triage-256 §5：clamp 下限一次通过、零上调——**clamp 下限 = 最小激发强度**，证据强度限定见彼处）」。

- [x] **Step 3: 再生成并验证 diff 恰为预期**

```bash
cd docs/superpowers/inventory/sve-all-vl-campaign
bash -n matrix-gen.sh && ./matrix-gen.sh > matrix.md && git diff --stat matrix.md
git diff matrix.md | grep -c '^[+-]|' || true   # 表体数据行变化数，期望 0
```

Expected: diff 只落在 §5 的 256 列对账行与 §2 legend 行（两处文本扩写）；`^[+-]|` 计数 = 0（190 行表体零变化）；生成器 exit 0（全部 FATAL 断言通过）。

### Task 4: README 运动总结句同口径

**Files:**
- Modify: `README.md:374`（运动总结段「三档采集」句）

- [x] **Step 1: 在「……见矩阵 §2 种子语义）；」与「512 = gem5 SE」之间插入从句**

```markdown
这 25 格（及 128 腿逐字拷贝同值的 26 项 baked）以 **clamp 下限最小激发几何**取证——工作集较默认自适应几何缩小数个数量级，功能正确性成立、**不构成满强度激发证据**（triage-256 §5「证据强度限定」）；
```

- [x] **Step 2: grep 验证限定语落位**

```bash
grep -c "最小激发" README.md docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md \
  docs/superpowers/inventory/sve-all-vl-campaign/matrix.md
```

Expected: 三文件均 ≥1。

### Task 5: 终验 + 提交 + 推送 + PR + issue 报告

- [x] **Step 1: 终验**：`git diff --stat` 只含 5 个文件（计划、triage-256、matrix-gen.sh、matrix.md、README）；Task 1-4 全部 Expected 达成；x86-64 非回归 = 纯文档/脚本文本改动、不触碰 tests/ framework/（如实声明，非「检查通过」）。
- [x] **Step 2: 提交**（一个 commit，含计划文档）：

```bash
git checkout -B fix/issue-394 origin/main   # 已在则跳过
git add docs/superpowers/plans/2026-11-01-issue394-bounded-evidence-strength.md \
        docs/superpowers/inventory/sve-all-vl-campaign/{triage-256.md,matrix-gen.sh,matrix.md} \
        README.md
git commit -s -m "docs(campaign): 256 腿 25 个有界格证据强度限定——clamp 下限=最小激发强度 (issue #394)"
```

- [x] **Step 3: 推送 + 开 PR**（兜底：分支已有 open PR 则 `gh pr edit` 改写标题正文）：
  标题 `fix: 256 腿 25 个有界格以 clamp 下限计绿的证据强度限定 (#394)`，正文含 Closes #394 + 根因 + 改动点 + 验证证据。
- [x] **Step 4: 报告贴 issue #394**；工作区回干净 main。
