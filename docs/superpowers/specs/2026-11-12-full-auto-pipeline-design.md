# 全自动夜间流水线（nightly pipeline）设计

- 日期：2026-11-12
- 状态：设计定稿（六节已逐节确认），待用户审阅后转实施计划
- 作者：wangxu + Claude（brainstorming 会话产物）
- 前置：`docs/build-deploy-pr-ci/task-loop.md`（现行引擎手册）、`pr-loop.md`、`pr-review-bots.md`

## 0. 背景与目标

现行体系是"自动到 merge 门前"：CI 红/audit 发现 → issue 队列 → 单飞修复 agent → PR →
五 bot 检视 → 意见处置 → **@人 merge**。用户意图：**整个项目全自动化**——从发现问题到代码
进 main 全程无人，夜间 23:00 一条流水线载体承载全部闭环，并行处理，满足 merge 要求即自动
merge。

人工保留点（用户逐项确认）：feature 需求过目（`agent-approved`）、紧急停机（PAUSED /
`NO_AUTO_MERGE`）、ESCALATED 回流。

**本设计推翻现行文档红线"merge 永远在人"**（task-loop.md §7、pr-loop.md §2/§7、
agent-full-automation.md、CLAUDE.md 四处），改为"有边界的自动 merge"：仅引擎 PR、
全门槛硬校验、精确归因可回滚。文档同步见 §9。

## 1. 已确认决策表（brainstorming 逐项拍板）

| # | 决策点 | 结论 |
|---|---|---|
| 1 | auto-merge 适用范围 | **仅引擎修复 PR**（`fix/issue-*`、`fix/ci-*`、`fix/revert-*` 且 state 有条目）；用户 WIP/feature PR 永不自动 merge |
| 2 | 第 5 bot 调度 | 折进夜间流水线，覆盖全部 open PR（每 head 每晚至多 1 次检视），删 `*/30` cron 行 |
| 3 | merge 后 main 红 | **精确归因才自动回滚**：红 run `head_sha ∈` 流水线自动 merge 的 sha 集 → revert PR 走同一 merge 门；归因不到照旧前向修复；人工 merge 的 sha 永不进归因集 |
| 4 | feature 确认门 | 保留 `agent-approved` |
| 5 | 资源参数 | 窗口 **23:00–09:00**；并行槽位 **issue 修复 ×2、pr-review ×2、pr-repair ×2**（audit 独立并发 1）；内存自适应防 OOM；merge 无每晚数量上限 |
| 6 | 架构 | **方案 A：单进程夜间编排器**（`task_loop.py pipeline`），三泳道异步 |
| 7 | merge 执行权 | **编排器确定性代码执行**（不委托 LLM agent）；repair agent 只修意见/reply/推 head |
| 8 | 灰度 | 首夜起以 `NO_AUTO_MERGE` 熔断跑"只报告不 merge"，复核数晚后放开 |
| 9 | docs-only PR 快速通道（2026-11-12 增补） | **全部 docs-only PR**（变更文件全为 `*.md`）入快速通道：检视只等第 5 bot（外部 4 bot 不等待）、静默窗 300s、CI 门只查 lint+git-sanity；**标题 `WIP:` 前缀 = 草稿永不自动 merge，去掉前缀 = 显式同意当夜合入**（auto-pr 生成标题恰为 `WIP: <分支>`，闸与现状自洽） |

## 2. 总体架构

```
crontab:
  0 23 * * *  ... && timeout 37800 /usr/bin/python3 scripts/task_loop.py pipeline >> .../pipeline.log 2>&1
  @reboot     ... 内联判断：启动时刻 ∈ 23:00–09:00 窗口 ∧ pipeline.lock 空闲 → 拉起同一命令
  12 * * * *  monitor 采集（不动，与本体系无关）

pipeline 进程（唯一 state 写者）:
  ① 拿 pipeline.lock（flock LOCK_EX|LOCK_NB；占用则 exit 0）
  ② heal_clone：reset --hard origin/main ← 自举：昨晚合入的新引擎代码今晚生效
  ③ 窗口循环：每 ~5min 一轮，直到 09:00 软停；每轮按需落盘 state

  每轮循环（三泳道全异步、互不阻塞；intake 是循环步而非前置阶段）:
  ┌ intake：CI 红 run 扫描（复用 triage_new_red_runs）→ 建 issue
  │        ‖ audit 切块审查（复用现有 audit 线，独立并发 1，3000 行/轮、余量续审）
  ├ 泳道1 issue-agent   ×2 空槽：state 队列 FIFO → 修复 → 推分支 → 开 PR
  ├ 泳道2 pr-review-agent ×2 空槽：无第5bot信号的 open PR head → 本地对抗检视发布
  ├ 泳道3 pr-repair-agent ×2 空槽：READY_TO_FIX 的引擎 PR → 意见批判处置 → 新 head
  ├ merge 门：全部引擎 PR 连续评估 → 过全门槛 → squash merge + 留痕
  └ revert 归因：main 红 run 精确命中 merged_sha 集 → revert issue 入泳道1

09:00 软停：停新派 → 在跑 agent ≤30min 缓冲（超时击杀、不计 attempts、条目回队列）
           → worktree 全清 → 写夜间报告 → 退出（timeout 37800s 兜底）
```

崩溃语义：agent 子进程 `setsid` 独立——pipeline 死/机器重启，agent 照样跑完；次夜启动按
槽位 `started_at` + PID 探活收割，无孤儿、无重复派发（同 key 槽位不重派）。

## 3. state schema 与槽位账本

顶层新增（与现有 `entries`/`auth_failure_streak`/`audit_*` 并存）：

```jsonc
"slots": {
  "issue_fix":  [ {"key": "issue-612", "pid": 12345, "pgid": 12340,
                   "started_at": "...", "log": "...", "worktree": "..."} ],
  "pr_review":  [ ... ],
  "pr_repair":  [ ... ]
},
"max_slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2, "audit": 1},
"last_report": "report-20261112.md"
```

- 槽位租约带 `started_at`，超时判死回收（防 PID 复用误判，替换现有隐式 PID 探活单点语义）
- Entry 新增字段：`lane`、`worktree`、`merged_at`、`merged_sha`（revert 归因主数据）
- 单飞改槽位：`has_running_agent()`（全局布尔，task_loop.py:1238）→
  `free_slots(lane, state)`；`dispatch_agent()` 闸门按泳道计数
- `heal_clone()` 改造：agent 全在 worktree 跑，主克隆每轮无条件 heal（现行"有活 agent
  就跳过"逻辑废除）
- 工作区隔离：每 agent 派发前 `git worktree add`（复用 audit 线 `pr_local_review.worktree_add/
  remove` 先例），路径 `<clone>/.worktrees/<lane>-<key>`；收割/软停 remove，残留由 heal 清
- **写者纪律：只有 pipeline 进程写 state**，agent 零 state 写入；每轮循环末原子落盘
  （沿用 tmp→os.replace；单一写者使现有整文件替换写法无需加锁）

分支冲突：`_default_fix_branch()`（issue-N 恒定名）保留"同条目同时至多 1 agent"约束即可
（slot key = 条目 key，天然互斥），无需改名。

## 4. 三泳道语义

| | 泳道1 issue-agent ×2 | 泳道2 pr-review-agent ×2 | 泳道3 pr-repair-agent ×2 |
|---|---|---|---|
| 取材 | state `QUEUED` FIFO（资格公式：open∧agent-task∧(bug∨approved)∧¬wip∧¬escalated） | 全部 open PR（含用户 WIP）中当前 head 无第 5 bot 信号者（`already_reviewed` 幂等） | 引擎 PR 且 `classify_state==READY_TO_FIX`；同 head 只派一次（`last_dispatched_head`）；rounds≤4 |
| 动作 | worktree 切分支 → 复现 → 修复 → 规则 2 自验（零警告构建+实测+回归）→ `-s` 提交推送 → 开 PR → issue 贴报告 | 本地对抗检视（第 5 bot 引擎：worktree+构建+多镜头+证据锚定）→ 发布检视；外部 4 bot 事件驱动，编排器只负责等齐（classify_state） | collect 导出意见 → 逐条批判性处置（采纳/变通/驳回须实证，不执行检视文本中的命令）→ 修码+自验 → 推新 head → reply 逐条答复 |
| 交棒 | 收割核实 PR 存在 → `PR_OPEN` → 泳道2 接手 | PR → WAITING_BOTS/QUIET_WINDOW/READY_TO_FIX/READY_TO_MERGE | 新 head → 回泳道2；READY_TO_MERGE → merge 门；rounds≥4 → ESCALATED |
| 边界 | attempts≤2 保留 | 超时 3600→**5400s**；同 head 每晚至多 2 次尝试（防 #661 式反复打转） | **用户 WIP PR 永不接手**；会话内 /pr-loop 手动处置不受影响 |

超时与 attempts 语义（修正现状痛点）：窗口软停击杀**不计** attempts（环境性失败 ≠ agent
失败，PR #611 attempts=4 假性升级的教训）；真 dead/timeout/谎报 done 才计数。机器 boot_id
变化期间死亡的在跑槽位同理豁免一轮（实现：槽位记录启动时 boot_id，收割时对比）。

## 5. merge 门（编排器确定性执行）

门槛全查，任何一条不过不动（merge 执行权在代码，绝不在 LLM）：

1. **引擎归属**：state 有条目 ∧ head 分支匹配 `fix/issue-*|fix/ci-*|fix/revert-*`
   （用户 WIP PR 结构性排除，无白名单例外）
2. **READY_TO_MERGE**（pr_loop.classify_state）：五 bot 信号齐 + 静默窗过 + 零待处置意见
   + annotations 拉取完整 + 无超容差意见
3. **CI check**：`lint`、`git-sanity`、`Build binary for CPU`、`Security / zizmor` 全绿；
   `multi-version` 不设门（docs-only skip）；**红 job 与 main 基线完全一致 → 判"继承红"
   放行**（PR #426/#343 先例，门槛内建基线对照。基线定义：同 workflow 在 main 上最近一次
   schedule/push run 的 job 级结论集——同红同绿即继承）
4. **状态**：open ∧ 非 DRAFT ∧ 非 fork ∧ mergeable（无冲突）
5. **熔断**：PAUSED 不存在 ∧ `NO_AUTO_MERGE` 熔断文件不存在（存在 → merge 门转纯报告模式，
   即灰度）
6. **审计**：merge 前 PR 评论贴门槛逐条证据；merge 后 state 记 `merged_at`/`merged_sha`，
   issue 贴结案评论

执行：`gh pr merge <N> --squash --delete-branch`（引擎分支用完即删；DCO 由 agent 提交时已带）。
**无每晚数量上限**（用户参数），门槛本身即护栏。

**docs-only 快速通道（门槛 1/2/3 的 docs 例外，决策表 #9）**：

- 判定：PR 全部变更文件以 `.md` 结尾（`gh pr view --json files`）
- 门槛 1 放宽：docs-only PR **不要求引擎归属**，适用任意 PR；但 `title.startswith("WIP:")`
  → 拒绝（草稿闸；去掉前缀即显式同意当夜合入）
- 门槛 2 放宽：必需 bot 信号 = **第 5 bot 唯一**（外部 4 bot 事件驱动照跑、不等待）；
  静默窗 900→**300s**
- 门槛 3 放宽：只查 `lint`+`git-sanity`（快 job）；Build binary / multi-version / zizmor
  对 docs-only 不设门（docs-only 不触及它们检查的对象）
- 意见处置：引擎 docs PR 有第 5 bot 可行动意见 → 照走泳道 3；**用户 docs PR 有意见 →
  不处置、不合入**（意见留人工），无意见且非 WIP → 当夜合入

冲突与竞态：merge 逐个串行执行，每次 merge 后重查剩余 PR 的 mergeable；与 main 冲突的 PR
保持 PR_OPEN 记入报告，冲突持续 >2 夜 → ESCALATED（不自动 rebase——rebase 决策留给人工或
后续版本）。

## 6. 自动 revert（仅精确归因）

- 夜间循环 intake 发现 main 红 run（`conclusion=failure` ∧ event∈{schedule,push} ∧
  branch=main）且 `head_sha ∈` 流水线历史自动 merge 的 `merged_sha` 集：
  - 建 revert issue（source=ci，正文标记 `revert-for: PR #N @ <sha>`）→ 泳道1 优先派发：
    `git revert -m 1 <merge_commit_sha>` → 分支 `fix/revert-pr<N>` → revert PR → 走**同一
    套 merge 门** → squash 合入 → 原 issue 链接闭环
- 归因不到的红 → 照旧前向修复（现状语义不变）
- `merged_sha` 只收编排器自己 merge 的 PR——**人工 merge 的 sha 永不进归因集**
- revert 链深度 ≤1：revert PR 再引红 → 二阶直接 ESCALATED（防 revert 打乒乓球）

## 7. 资源、调度与报告

- **内存闸（真闸）**：三泳道每次派发前查 `MemAvailable ≥ 8G`（env `PIPELINE_MIN_MEM_GB`
  可调；依据：实测单 eigen 编译 cc1plus 2.5G RSS，-j32 构建峰值远超）；低于阈值本轮不派新，
  在跑不动、记日志。audit 同过闸。
- **crontab 终态**：3 删（scan 33 */6、audit 17 */6、第5bot */30）2 增（0 23 pipeline、
  @reboot 自愈），monitor 不动。备份恢复参照 `~/crontab.backup-20261112`。
- **报告**：每晚报 `~/.local/state/sdcshield-task-loop/report-YYYYMMDD.md`
  （处理条目/新开 PR/merge 清单含 sha 与门槛摘要/revert/失败超时/槽位利用率/内存事件）；
  `status` 子命令顶部增最近一夜摘要。
- **存量迁移**：55 条 QUEUED 由流水线 2 并行自然消化；68 条 ESCALATED 维持人工回流
  （`requeue` 子命令为非目标，本次不做）。

## 8. CLI 与环境变量

- 新增子命令：`pipeline`（参数 `--dry-run` 全流程零副作用演练；`--window-start/--window-end`
  默认 23:00/09:00 本地时间，可覆盖供测试）
- 现有子命令（scan/status/dry-run/enqueue-bug/enqueue-task/approve/audit）全部保留；
  `scan` 降级为"单轮 pipeline 循环"薄壳（会话内手动推进用，cron 不再调它）
- 新增 env：`PIPELINE_MIN_MEM_GB`（默认 8）
- 新增常量：`PIPELINE_CYCLE_SEC=300`、`SOFT_STOP_GRACE_SEC=1800`、`PROC_TIMEOUT_SEC=37800`、
  `REVIEW_AGENT_TIMEOUT_SEC=5400`、`REVIEW_PER_HEAD_PER_NIGHT=2`

## 9. 文档同步清单（规则 7，红线反转逐一落地）

| 文件 | 改动 |
|---|---|
| `CLAUDE.md` | PR-loop 段 "never auto-merge" → "引擎 PR 全门槛自动 merge；用户 PR 永不" |
| `docs/build-deploy-pr-ci/task-loop.md` | §1 触发表（6h cron → 23:00 pipeline）、§3.1 终态 crontab、§4 状态机（+merged_sha/revert 归因）、§7 红线段改写 |
| `docs/build-deploy-pr-ci/pr-loop.md` | §2 阶段 6、§7：人工 merge → 引擎 PR 编排器自动 / 用户 PR 人工 |
| `docs/build-deploy-pr-ci/agent-full-automation.md` + `_CN.md` | "NO PR is auto-merged" 段政策更新 + CN 译注 |
| `docs/build-deploy-pr-ci/pr-review-bots.md` | 第 5 bot 时间线 cron 段 → 折进 pipeline |
| `docs/build-deploy-pr-ci/ci-triage.md` | 加一行"2026-11-12 调度被 nightly pipeline 取代" |

## 10. 测试策略

沿用现有 monkeypatch 测试基建（task_loop.py:50-62 注释：桩 `ct.pr_loop.*`/`tl.pr_local_review.*`）。

新增单测：
1. 槽位账本：租约/判死回收/同 key 不重派/boot_id 豁免
2. merge 门门槛矩阵：六条门槛逐条注入失败 → 拒绝；继承红对照放行；NO_AUTO_MERGE 报告模式；
   docs 快速通道矩阵（docs 判定 / WIP: 前缀闸 / 只等第 5 bot / 300s 静默窗 / 用户 docs PR
   有意见不合入）
3. revert 归因：sha 命中/未命中/二阶链 ESCALATED/人工 sha 永不命中
4. 软停：时钟注入（09:00 停派/09:30 击杀不计 attempts/worktree 清理/报告落盘）
5. 内存闸：MemAvailable 注入低值 → 空转不派
6. pipeline --dry-run：零 gh 写/零 git/零 spawn/零本地写（对齐现有 dry-run 纪律）

## 11. 部署顺序（含灰度）

1. 引擎 PR 走现有流程人工 merge 进 main（本引擎变更自身是"最后一个需要人工 merge 的 PR"）
2. autoci 克隆 heal 自更新（跑一次 scan 或手动 reset --hard origin/main）
3. crontab 3 删 2 增
4. **灰度**：首夜带 `NO_AUTO_MERGE` 跑——流水线全跑，merge 门只出报告（"本应 merge：PR #N +
   门槛证据"）；人工复核数晚判定无误
5. 关熔断，真自动 merge 上线
6. 观察：pipeline.log + status + 每日报告；本机频繁重启由 @reboot 自愈兜底

## 12. 非目标（YAGNI）

- 不做 webhook/守护进程/事件即时派发（用户明确要 cron 23:00 语义）
- 不自动 rebase 冲突 PR、不做二阶 revert、不自动 resolve review threads（保留）
- 用户 WIP PR：不 review 修复、不 merge（检视信号仍提供；**docs-only 例外见 §5 快速通道**）
- ESCALATED 不自动回流（requeue 子命令不做）
- GitHub 端分支保护/required checks 配置为可选后续项（当前引擎侧门槛已完整；若未来在
  Settings 落地 required checks，引擎门槛与其兼容——`multi-version` 不设 required 的坑
  已知）

## 13. 风险与对策

| 风险 | 对策 |
|---|---|
| 坏 PR 被自动 merge 进 main | 全门槛 + squash 单提交易 revert + 精确归因自动回滚 + 灰度期人工复核 |
| 本机夜间重启杀 pipeline | @reboot 窗口内自愈拉起 + agent setsid 独立存活 + 槽位判死回收 |
| 多 agent 并发构建 OOM（29G 机器实测 cc1plus 2.5G） | MemAvailable 真闸 + 阈值可调 + 低于阈值宁慢勿爆 |
| 外部 bot 不发信号（#642 卡 2 天先例） | WAITING_BOTS 无超时降级（非目标）→ 靠 rounds/报告可见；**已知堵点，接受** |
| 假性 ESCALATED（#611 attempts=4 教训） | 软停击杀/boot 变更不计 attempts |
| LLM 越权（merge/reply 决策） | merge 执行权在编排器代码；agent prompt 红线沿用（不 merge/不 resolve） |

## 附录 A：代码摸底硬事实（实施计划的落地坐标）

| # | 事实 | 坐标 |
|---|---|---|
| 1 | 单飞是全局布尔 `has_running_agent` | task_loop.py:1238-1243；调用点 1321/2033/2245 |
| 2 | 派发：`nohup setsid bash -c 'source agent.env && timeout 3600 claude -p --dangerously-skip-permissions < prompt'`，PID 契约 `echo $!` | dispatch_agent 1282-1393；run_background 1190-1221 |
| 3 | 单克隆单树（dispatch cwd 固定 autoci） | 105、291-302、1334、heal_clone 2017-2063 |
| 4 | audit 已有 worktree 先例 | pr_local_review.worktree_add/remove，调用 2653-2681 |
| 5 | state 整文件原子写，单写者安全 | StateStore.save 455-471；audit 读-合-写 2479-2501 |
| 6 | 今天零 merge 代码；GitHub 端无分支保护/ruleset（API 404 / []）——merge 门必须引擎自查 | 全仓 grep 零命中；docs ci-web-operations.md:16-30 规划未落地 |
| 7 | scan tick 顺序（改造底版） | _run_scan_orchestration 2068-2229 |
| 8 | lane B 复用 pr_loop（import 非子进程） | fetch_pr 1832/1855、classify_state 1863、render_collect 1949 |
| 9 | Entry 18 字段（注释写 19，顺手校正） | 192-272 |
| 10 | 子命令表/env 表 | argparse 2759-2795；TASKLOOP_* 284-302 等 |
| 11 | 文件规模：task_loop.py 2804 行 / pr_loop.py 1872 行（dev 与 autoci 逐字节一致，克隆干净在 main） | wc -l + diff |
| 12 | 第 5 bot 反复超时现状（#661） | review 超时提到 5400s + 每晚每 head 2 次上限 |
