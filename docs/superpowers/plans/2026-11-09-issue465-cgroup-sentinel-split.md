# issue #465: cgroup 预算源哨兵拆分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 拆掉 `sdc_cgroup_limit_bytes` 的哨兵合流——「限额已耗尽(lim≤usage)」不再与「读取失败/无限」共用返回值 0，耗尽作为真实余量 0 参与预算 min，使耗尽的 cgroup 把预算压到 0（下游退守规模下限/诚实 SKIP），而不是被当无限无视后大维度分配 bad_alloc → std::terminate。

**Architecture:** 全部改动收敛在 `framework/sdc_stress_config.h` 的 cgroup 读侧四级管线（`sdc_cgroup_level_avail` → `sdc_cgroup_walk` → `sdc_cgroup_limit_bytes` → `sdc_budget_bytes` 消费点），采纳第四源 `sdc_commit_headroom_bytes` 已确立的哨兵约定：**UINT64_MAX = 不约束/不可知（跳过），真实值含 0 = 约束生效（0 = 耗尽，诚实参与 min）**。UINT64_MAX 是 min 的恒等元，旧 `sdc_cgroup_limit_bytes` 的「0=未挂载」特判自然溶解。下游契约无需改动：mesh_upi 系已按「budget==0(限额/承诺耗尽)→ init 诚实 SKIP」书写，联合求解 `avail≤0 → n_mem=0 → n=lo`。

**Tech Stack:** C++20 header-only（static inline），无新依赖。

**Spec:** `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.2（四源 min 契约；「任一来源读失败即跳过」修复后仍成立；第四源 v3 增补确立了 0=耗尽/UINT64_MAX=跳过 的先例）。

## Global Constraints

- 一个补丁一个逻辑单元：单 commit（CLAUDE.md 规则 1），`git commit -s`，尾行 `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`，其后无内容。
- 构建零新警告：`./scripts/build.sh`（PKG_CONFIG_PATH=./third-party/eigen5）。
- x86-64 非回归论证：cgroup 四函数无 arch 守卫、被 x86 用例（mesh_upi_avx*）与 x86 fallback 联合求解共享——属「确实共享的代码路径」；改动方向纯保守（仅 lim≤cur 的语义从「被无视」变为「预算 0」），无耗尽/无限/正常限额三类输入在两 arch 上行为均不变或变保守。在 PR 正文论证。
- 不动 `:684-685` node_cpus 区域（issue #515 范围）；不动 svd_cdouble_sve.cpp make_data（已核实：make_data 只 new 小结构体，重分配在 run() 模板内且已被 T10 窄 catch(std::bad_alloc)→线程级 SKIP 覆盖）。
- 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- 无 root 无法新建受限 cgroup：功能验证以「真实读侧数据走查 + 下游消费方逐家核查 + 逻辑论证」为最高可达级，如实声明边界（fleet 契约验证阶梯 1/3 级，2 级视 systemd 用户切片真实限额可用性现场探测）。

## Review Focus

1. **耗尽级(leaf)**：lim≤cur → walk 必须把它当 min 胜者（0），不是跳过——核心修复本体。
2. **耗尽祖先**：叶子健康但祖先耗尽 → walk 逐级 min 必须取到 0。
3. **walk 路径超长**：`snprintf` 截断旧返 0 → 新语义下必须是 UINT64_MAX（不可知≠耗尽，否则一次截断把预算误压到 0）。
4. **v2 memory.max 垃圾/空内容**：`strtoull` 无校验会把垃圾解析成 0 → 伪造「耗尽」判决；须 endptr 校验（v1 侧 `fscanf("%lu")` 已有同等校验）。
5. **budget=0 下游安全**：联合求解 `avail≤0→n_mem=0→n=lo`（无 0 除）；mesh 系 `b==0→诚实 SKIP`；svd/gemm preinit `n_hi→MDIM_MIN` 下限钳制——已逐家核查现存代码路径，无需改动。

以上 1-4 由代码走查（真实数值代入各输入类）钉住；5 由下游消费方代码核查钉住。

---

### Task 1: 哨兵拆分（单 commit）

**Files:**
- Modify: `framework/sdc_stress_config.h:97-102`（头部契约注释）、`:175-177`（level_avail 契约注释）、`:178-209`（sdc_cgroup_level_avail）、`:214-231`（sdc_cgroup_walk）、`:233-247`（sdc_cgroup_limit_bytes）、`:297-303`（sdc_budget_bytes 消费点 + 耗尽诊断日志）

**Interfaces:**
- Consumes: `sdc_phys_mem_bytes()`、`sdc_cgroup_self()`（签名不变）。
- Produces: `sdc_cgroup_level_avail(dir, v2) → uint64_t`（UINT64_MAX=不约束；0=耗尽；lim−cur=正常）；`sdc_cgroup_walk(base, rel, v2) → uint64_t`（同约定，逐级 min）；`sdc_cgroup_limit_bytes() → uint64_t`（同约定，v1/v2 取 min）；`sdc_budget_bytes(permille)` 签名不变，返回值新增「0=某真实源耗尽」语义（第四源已可产生）。

- [ ] **Step 1: 修改 `sdc_cgroup_level_avail`**——五个跳过分支（v1/v2 limit 文件缺失、读失败、"max"、lim≥物理、usage 文件缺失/读失败）返回 `UINT64_MAX`；v2 数值解析加 `endptr==buf` 校验；末尾 `lim>cur ? lim-cur : 0`（耗尽=真实 0）。契约注释同步改写。
- [ ] **Step 2: 修改 `sdc_cgroup_walk`**——路径超长返 `UINT64_MAX`；`best` 初值 `UINT64_MAX`；收纳条件改 `if (av < best)`。
- [ ] **Step 3: 修改 `sdc_cgroup_limit_bytes`**——删「0=未挂载」特判，直接 `return a1 < a2 ? a1 : a2;`（恒等元语义）；注释记录 v2 荒谬大值与哨兵别名的无害边界。
- [ ] **Step 4: 修改 `sdc_budget_bytes` 消费点**——`if (cg > 0 && cg < b)` 改 `if (cg != UINT64_MAX && cg < b)`；`cg==0` 时打一次性 `[cgroup 预算]` 诊断日志（仿承诺源的 `static bool logged` 每 TU 一次式样）。头部 :99 契约注释同步。
- [ ] **Step 5: 构建**——`PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release && ./scripts/build.sh`，零新警告。
- [ ] **Step 6: 真实数据走查**——`cat /proc/self/cgroup` 定位本进程 cgroup，读真实 limit/usage 数值，对四类输入（未挂载/无限/正常/耗尽）逐一引用真实数值走查修复后函数的返回语义；探测 systemd 用户切片是否可注入真实限额（`systemd-run --user` MemoryMax），可用则跑 eigen 用例引用预算日志变化。
- [ ] **Step 7: 回归**——`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- [ ] **Step 8: Commit**——`git checkout -b fix/issue-465`（从 main）后 `git add framework/sdc_stress_config.h && git commit -s`。

### Task 2: 推送与 PR（机械步骤）

- [ ] `git push origin fix/issue-465`；API 建 PR（title `fix: <一句话> (#465)`，body 含分析/方案/验证实录，`Closes #465`）。
- [ ] 向 issue #465 贴处置评论（修复摘要 + PR 链接 + 验证实录）。
