# 计划：pr_local_review.py 检视范围收窄 + 失败加固（2026-11-04）

## 用户指令
调用 subagent 对检视循环范围做优化，缩短耗时（2026-11-04；subagent 分析已完成，
本计划为其 P1/P2 提案的实施计划；P3 静默窗已免改码落地 fleet 模板）。

## 实证基线（subagent 三方实证：transcript + gh api + 本地 git）
- 每轮第 5 bot 耗时 29-59 min ≈ **100% GLM-5.3 推理**（工具 <30s、前后置 <1min）；
  prompt 37.6K 字符中全量 diff 占 83%。
- **重审冗余**：r10 重审 613 行，其中 89% 与 r9 已审内容字节相同（真实增量
  68 行/1 文件）；r8→r9 是 rebase（非祖先）→ raw delta 15,543 行含 main 变更
  （须回退全量）。
- **失败成本**：_parse_findings 非 JSON / claude exit 1 → GhError → 整轮 30-60min
  浪费（pr341 ×2、pr391 ×4 实证），且**原始 stdout 被丢弃**（仅存 stderr 前
  300 字符，r7b 实证为空）——失败形状不可诊断。
- 锚点（subagent 给的行号基于当前 main 工作树）：prompt 构建 :87-151、
  already_reviewed :154、run_review :177-208、claude 命令 :338-344、解析
  :368-419、发布 :543-616、_fetch_pr_diff :255、_lens_scale 按 diff 行数缩
  放镜头（与 delta 收窄天然协同）。

## 实施（一补丁一单元，feature 分支，绝不推 main）

### P2 commit：失败重试 + 原始输出转储（先行——P1 的观察工具）
- `_run_claude`：stdout/stderr 全量保留；失败（非零退出或后续解析失败）时
  落盘 `pr<N>-raw-output-<timestamp>.log`（debug 目录，与 localreview.log 同
  侧）——消除取证盲区。
- 解析失败路径：带修复后缀（「上一次输出无法解析为 JSON 数组，只输出 JSON
  数组本身」）进程内重试一次；重试产物仍走同一幂等门禁与锚定。
- 近零风险：重试不改判据；转储纯观测性。

### P1 commit：增量检视模式
- `_last_reviewed_head`：同一次 reviews 分页里取 max(submitted_at) 的
  marker review `commit_id`（锚现成，不新增 API 调用）。
- delta 路径：`git fetch origin <prev_sha>` → `git merge-base --is-ancestor
  <prev> <head>` 判定 → 快进则 `git diff prev..head` 作为审阅 diff，并给
  prompt 加 prior_context 段（上轮 findings + 处置答复 + 复核指令——对抗性
  对闭环核心反而强化：专抓不实处置）。
- **五情形回退全量**：轮 1（无已审锚）/ 非祖先（rebase）/ fetch 失败 /
  base 变更 / delta 为空。
- 行号口径坑（subagent 提示）：findings 行号须按 PR 全量 diff 口径，否则
  `_diff_new_positions` 校验不过降级 body JSON（已有 C2 降级路径，非阻断）。
  处置：prompt 中明确要求行号基于全量 diff 行号；delta diff 旁附全量 diff
  行号映射说明。

### 验证（规则 2，全部真实输出）
- 单测（python -c 导入函数，用 pr341 真实 git 历史）：
  52ed7ab9→cfa2a2e2 判定快进 + delta=68 行；44d0f02f→52ed7ab9 判定 rebase
  回退全量；无锚（轮 1）回退；P2 转储/重试用合成垃圾输入触发并引用产物。
- 端到端：选一个「上一 head 已审、当前 head 未审且快进」的 open PR（不在
  fleet active-pool，避免撞车）跑一次真实增量检视，引用贴出的 review 与
  耗时对比。
- x86-64 非回归：纯 scripts/ Python 改动，如实声明零影响。

### PR
- 分支 `feat/pr-local-review-incremental`，P2、P1 两个 commit（一单元一补丁），
  push 开 PR（其自身走检视循环）。

## 停止条件
- 端到端增量运行失败或行级锚定大面积降级（C2 降级率异常）→ 停止如实报告，
  不带病合入。
- P1 验证边界：单次端到端样本不足以证明所有 PR 形态——PR 描述如实声明
  「已验证形态/未验证形态」。
