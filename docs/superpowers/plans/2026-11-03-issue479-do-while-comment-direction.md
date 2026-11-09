# Issue #479: do-while 单迭代条件方向写反的三处注释修正 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 gemm/svd 两文件里三处把「do-while 恰好单迭代的条件」方向写反的注释改为正确方向(dur 应**不高于**子进程实际单次迭代耗时),并使措辞与框架真实语义、同文件实测记录一致。

**Architecture:** 纯注释修正,零代码行为变更。依据:框架 `test_time_condition()`(framework/sandstone_run.cpp:633)=「截止**未**过期才继续」;两侧 run 体都是 `do {…} while (test_time_condition(test))`(tests/cpu/eigen_gemm/gemm_double_max_arm_body.cpp:80-103、tests/cpu/eigen_svd/sandstone_eigen_common.h:389-413)。⇒ 恰好单迭代 ⟺ 迭代 #1 结束时截止已过期 ⟺ dur ≤ 实际单迭代耗时。×0.95(−5%)margin 的存在意义正是把 dur 压到模型迭代时间**之下**(被删旧注释 bb0e148a 原文即此方向)。

**Tech Stack:** C++ 注释;无构建面变更。

**Spec:** issue #479(audit,Minor/可维护性)+ CLAUDE.md 规则 2/4。范围严格限定 issue 点名的三处;AR 表 5.0 vs 实测 ~15 的数值后果归 issue #474(escalated),127 硬编码归 #478(escalated),本修复只引用既有 CAVEAT、不新开论断。

## Global Constraints

- 一个补丁一个单元:三处注释修正 + 本计划文档 = 单个 commit。
- `git commit -s`,DCO 最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`。
- 推 `fix/issue-479` 分支,绝不推 main、绝不 merge、绝不 resolve。
- 注释语言风格随原注释(英文技术注释 + 少量中文),保持既有 `dur`/`n_hi`/`T1024` 符号体系与 ~80 列折行。

## 复现结论(修复前提,已完成)

1. `framework/sandstone_run.cpp:633`:`return !wallclock_deadline_has_expired(...)` — do-while 在截止未过期时**继续**。
2. `gemm_double_max_arm_body.cpp:80-103` 与 `sandstone_eigen_common.h:389-413`:run 体均 `do {…} while (test_time_condition(test))`。
3. ⇒ 单迭代 ⇔ dur ≤ 实际迭代;三处注释却声称 "dur at or above the child's real iteration / parent's timing expectation at or above the child's choice" 是 do-while 所期望的方向 —— 反了。
4. 数值复核:`dur = 5×2×0.1260×(3348/1024)³×0.95 = 41.8 s ≈ 42 s` < 同文件 :255-256 实测全核单次乘法 ~55-60 s(N=3364,有效 AR≈15 vs 表 5,CAVEAT 已录)——本参考机上 dur 本就低于实际(单迭代成立),但成立原因与注释声称方向相反。
5. 被删旧注释(bb0e148a diff)为正确方向:"minus 5%: the do-while time condition is then already expired when iteration #1 finishes -> exactly one iteration"。
6. 全仓 grep "at or above":仅此三处为方向反述(另两处命中为无关文件)。

---

### Task 1: 修正三处注释

**Files:**
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:157-170`(preinit 注释块内反述句)
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:349-355`(DECLARE_TEST 尾注)
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:388-394`(DECLARE_TEST 尾注)

**Interfaces:** 无(纯注释)。

- [ ] **Step 1.1: gemm preinit 块(:157-161)替换**

旧文(被反述句):
```
 * The AR × 2 mirrors the joint solver's run_budget semantics (fill + one
 * solve per iteration, all-core slowdown factor), so dur models the
 * FULL-CORE single-iteration time at the memory ceiling: child n <= n_hi
 * keeps dur at or above the child's real iteration, which is what the
 * do-while and the derived timeout test_timeout() = 5×duration + 30 s
 * (300 s floor) expect.
```

新文:
```
 * The AR × 2 mirrors the joint solver's run_budget semantics (fill + one
 * solve per iteration, all-core slowdown factor), so dur models the
 * FULL-CORE single-iteration time at the memory ceiling, and the ×0.95
 * margin sits dur 5% BELOW that model (issue #479: this sentence, and the
 * DECLARE_TEST comment below, previously stated the direction backwards).
 * The run-side do-while — do … while (test_time_condition()), which
 * CONTINUES while the deadline has not expired — executes exactly one
 * iteration only when dur is at or BELOW the child's real single-iteration
 * time: the deadline is then already expired when iteration #1 finishes,
 * which is what the −5% margin is for; a dur above the real time would
 * instead start a second, un-budgeted iteration. On this host class dur
 * sits on that side twice over: the −5% margin, plus the table AR (5)
 * understating the measured all-core slowdown (~15, CAVEAT in the init
 * comment) — dur ≈ 42 s vs the ~55-60 s measured multiply. The derived
 * timeout test_timeout() = 5×duration + 30 s (300 s floor) needs the
 * OPPOSITE relation — real iteration <= timeout — and child n <= n_hi is
 * what keeps it: the real iteration stays within the ceiling model times
 * the model's own error (~3× measured), so one iteration fits the
 * timeout.
```

- [ ] **Step 1.2: gemm DECLARE_TEST 尾注(:349-355)替换**

旧文:
```
  /* desired_duration is written by test_preinit (parent, before the
   * framework computes test_duration for the first run) as an UPPER-BOUND
   * estimate from the memory ceiling + measured GEMM iteration model
   * (T6: the child sizes the dimension itself via the joint solver; this
   * only keeps the parent's timing expectation at or above the child's
   * choice, so the derived timeout (5*duration+30s, 300s floor) scales
   * along). */
```

新文:
```
  /* desired_duration is written by test_preinit (parent, before the
   * framework computes test_duration for the first run) as an UPPER-BOUND
   * estimate from the memory ceiling + measured GEMM iteration model
   * (T6: the child sizes the dimension itself via the joint solver,
   * always <= that ceiling. The run-side do-while — which continues while
   * the deadline has NOT expired — exits after exactly one iteration when
   * dur sits at or below the child's real single-iteration time, the
   * direction the −5% margin takes (issue #479; analysis and measured
   * numbers in the preinit comment); the derived timeout
   * (5*duration+30s, 300s floor) scales along). */
```

- [ ] **Step 1.3: svd DECLARE_TEST 尾注(:388-394)替换**

旧文:
```
  /* desired_duration is written by test_preinit (parent, before the
   * framework computes test_duration for the first run) as an UPPER-BOUND
   * estimate from the memory ceiling (P0-1: the child sizes the dimension
   * itself via the joint solver; this only keeps the parent's timing
   * expectation at or above the child's choice, so exactly one run
   * iteration executes and the derived timeout (5*duration+30s, 300s
   * floor) scales along). */
```

新文:
```
  /* desired_duration is written by test_preinit (parent, before the
   * framework computes test_duration for the first run) as an UPPER-BOUND
   * estimate from the memory ceiling (P0-1: the child sizes the dimension
   * itself via the joint solver, always <= that ceiling. The run-side
   * do-while — which continues while the deadline has NOT expired —
   * exits after exactly one iteration when dur sits at or below the
   * child's real single-iteration time; at the memory-bound ceiling the
   * real iteration carries the AR factor on top of the same quiet fit,
   * so it sits above the −5%-discounted dur (issue #479: the previous
   * "at or above" wording had this backwards); a dur above the real time
   * would instead start a second, un-budgeted iteration. The derived
   * timeout (5*duration+30s, 300s floor) scales along). */
```

- [ ] **Step 1.4: 自检 diff 只有注释行**

Run: `git diff` — 三处全部落在注释内(`/* … */`),无代码 token 变更;全仓 `grep -rn "at or above" tests/cpu/eigen_gemm tests/cpu/eigen_svd` 零命中。

### Task 2: 验证(CLAUDE.md 规则 2)

- [ ] **Step 2.1: 构建零新警告**

Run: `./scripts/build.sh 2>&1 | tee /tmp/build479.log; grep -iE "warning|error" /tmp/build479.log`
Expected: 构建成功,无新 warning/error(改动为纯注释)。

- [ ] **Step 2.2: 受影响行为(本机 no-SVE → 显式 -e 走 preinit + 干净 SKIP)**

Run: `./builddir/sdcshield -e eigen_gemm_double_max_arm -t 2000 -n 1; echo rc=$?`
`./builddir/sdcshield -e eigen_svd_cdouble_sve -t 2000 -n 1; echo rc=$?`
Expected: 两个测试干净 SKIP(SVE gate),整 run `exit: pass`(注释零行为变更的实证;数值断言 42s/55-60s 引用文件内 T1024 记录与 CAVEAT,不重测参考机)。
注:本机无 SVE,单迭代数值性质在参考机;本修复为注释正确性修正,不改任何数值路径,gem5-fi 不适用(无受影响的行为)。

- [ ] **Step 2.3: 回归**

Run: `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
x86-64 非回归:改动均在 `#if defined(__aarch64__)`/aarch64-only 文件内,纯声明性影响 —— 检视声明(inspection)即可。

### Task 3: 提交/推送/PR/报告

- [ ] **Step 3.1:** `git add` 两个源文件 + 本计划文档;`git commit -s`(消息 `fix(eigen): 三处 do-while 单迭代条件方向反述注释正写 (#479)` + 正文含根因/改动/验证)。
- [ ] **Step 3.2:** `git push origin fix/issue-479`(绝不推 main)。
- [ ] **Step 3.3:** `gh pr create --base main --head fix/issue-479 --title "fix: 三处注释把 do-while 恰好单迭代的条件方向写反 (#479)" --body "Closes #479 + 报告"`;若 auto-pr 抢先则 `gh pr edit` 兜底。
- [ ] **Step 3.4:** 报告贴 issue #479;结束回到干净 main(`git checkout main && git status` clean)。
