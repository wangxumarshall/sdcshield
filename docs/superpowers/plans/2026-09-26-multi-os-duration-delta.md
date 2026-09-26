# multi-os-verify 耗时 delta 对比 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** multi-os-verify 的 report 汇总矩阵每格从 `结果[耗时s]` 升级为 `结果[耗时s (±Δs)]`,Δ = 相对上次成功全量运行同用例同版本的耗时变化(增加为正);结果态只展示不对比。

**Architecture:** `scripts/gha/report-summary.py` 加可选 `--previous DIR` 参数解析基线目录(同样 glob `verify-*/results/allquality.yaml`);workflow report job 用 gh CLI 下载上次成功全量运行的 verify-* artifact 作基线,有则传 `--previous`。无 `--previous` 时输出必须与现状逐字节一致(pr.yaml quick summary 无参复用此脚本)。

**Tech Stack:** Python3 纯 stdlib(argparse/glob/re)、GitHub Actions(gh CLI run list/download)、bash。

**Spec:** 任务指派书(2026-09-26,本 session 上文)——含工作流接线细节、fixture 测试要求、PR/merge/端到端验证流程。

## Global Constraints

- 无 `--previous` 时 report-summary.py 输出与旧版**逐字节一致**(diff 验证)。
- 保持纯 stdlib、流式解析、BrokenPipeError 处理、中文注释风格。
- `git commit -s`,末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,无 Co-Authored-By。
- 分支 `feat/multi-os-duration-delta`(从 origin/main),绝不推 main。
- 一 patch 一单元(脚本特性 / workflow 接线+文档,1-2 个 commit)。
- README 只动多版本/验证段(另一并行 agent 在改 ~158 行 CodeQL 安全段)。
- delta 计算仅耗时:`delta = cur_rt - prev_rt`,格式 `{delta:+.2f}s`;正负号必须显式(`+`/`-`)。
- 基线判定:smoke 运行天然无 allquality.yaml,靠"目录树里存在 allquality.yaml"即自动排除。

## Review Focus

- 无 `--previous` 输出回归(pr.yaml 复用路径)→ Task 1 的 diff 步骤。
- 负 delta 与零 delta 的符号格式(`-0.50s`/`+0.00s`)→ fixture 用例含耗时下降项。
- 当前有、基线无的用例(基线老版本未编译出)→ 维持无 delta 格式。
- 基线目录存在但 glob 无匹配(如基线 run 全是 smoke)→ 基线视同缺失,不崩、不红。
- report 步骤保持 `set +e`(汇总失败不红 job);`gh run download` 失败不红 job(基线=none 降级)。

---

### Task 1: report-summary.py 加 `--previous`

**Files:**
- Modify: `scripts/gha/report-summary.py`

**Interfaces:**
- Produces: CLI `report-summary.py [--previous DIR]`;给定 DIR 时按 `os.path.join(DIR, "verify-*", "results", "allquality.yaml")` glob 解析 `per_image_prev`;cell 渲染 `f"{token}[{rt:.2f}s ({delta:+.2f}s)]"`。

- [ ] **Step 1: 构造 fixture**(临时目录,不入库):`all-results/verify-24.03-LTS-SP3/results/allquality.yaml` 3 用例(其中 1 个基线没有、1 个耗时下降)、`prev-all-results/verify-24.03-LTS-SP3/results/allquality.yaml` 2 用例。
- [ ] **Step 2: 保存旧版输出** `python3 scripts/gha/report-summary.py > old.txt`(fixture 当前目录下)。
- [ ] **Step 3: 实现**——argparse 加 `--previous`;基线解析复用 main 里的目录遍历逻辑;表头补 Δ 说明;基线缺失打印说明行;fmt_cell 按 Review Focus 规则渲染。
- [ ] **Step 4: 验证**:
  - 无参:输出与 old.txt `diff` 逐字节一致;
  - 带 `--previous prev-all-results`:3 种 cell 形态各出现(带正 delta、带负 delta、无基线无 delta);
  - 空基线目录(无匹配 glob)不崩。
- [ ] **Step 5: Commit** `git commit -s -m "..."`(subject: 脚本加耗时 delta 对比)。

### Task 2: workflow report job 接线 + 文档同步

**Files:**
- Modify: `.github/workflows/multi-os-verify.yml`(report job)
- Modify: `README.md`(多版本/验证段)
- Modify: `docs/multi-version-build-deploy.md`(grep 定位 report 矩阵描述处)

**Interfaces:**
- Consumes: Task 1 的 `--previous DIR` 参数。

- [ ] **Step 1: report job 加 `permissions: contents: read, actions: read`。**
- [ ] **Step 2: 加"下载上次成功全量运行基线"步骤**:`gh run list --workflow multi-os-verify.yml --status success --limit 8 --json databaseId`(排除当前 run,新到旧)逐个 `gh run download <id> --pattern 'verify-*' --dir prev-all-results`;接受第一个含 allquality.yaml 的候选;全无 → 基线=none。失败降级,不红 job。
- [ ] **Step 3: summary 步骤改 tee**(矩阵进 job log 供 API 取证):有基线 `python3 scripts/gha/report-summary.py --previous prev-all-results | tee -a "$GITHUB_STEP_SUMMARY"`;无基线不带参同样 tee;保持 `set +e`。
- [ ] **Step 4: 文档** README 与 docs/multi-version-build-deploy.md 各补 delta 含义一句(只动自己段落)。
- [ ] **Step 5: 验证 workflow 语法**(actionlint 若可用 / 目视 YAML 缩进)+ 文档 grep 确认无遗漏描述处。
- [ ] **Step 6: Commit + push + API 开 PR**(PAT 不打印;PR body 末尾 Claude Code 署名行)。

### Task 3: PR 检查全绿 → merge → 端到端验证

- [ ] 轮询 10 个必过检查全绿;main 有新提交先 rebase。
- [ ] `PUT /pulls/{n}/merge` 自行 merge。
- [ ] `POST .../workflows/multi-os-verify.yml/dispatches` body `{"ref":"main","inputs":{"verify_depth":"full"}}`。
- [ ] 每 ~8 分钟轮询至完成(可等 ~90 分钟);记录 15 verify + report + benchmark-trend 结论。
- [ ] 拉 report job 日志,grep 正 delta/负 delta/无基线三种真实单元格;确认 15 列表头、用例数数百级。
- [ ] 检查 gh-pages 顶层(应只有 .nojekyll/dev/index.html);污染则如实报告不修。
- [ ] 测试 FAIL/CRASH 如实记录(与 delta 功能无关);仅 workflow 自身失败才迭代修复。
