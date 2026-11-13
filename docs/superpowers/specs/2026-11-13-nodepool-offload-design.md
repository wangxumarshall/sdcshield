# 跨机卸载（nodepool offload）设计——本机资源不足/常态分载到 SDC 板卡

- 日期：2026-11-13
- 状态：设计定稿（六节逐节确认），待用户审阅后转实施计划
- 前置：`2026-11-12-full-auto-pipeline-design.md`（nightly pipeline，已合入 main @ 060812ea 并部署）
- 板卡事实源：`~/.claude/skills/accessing-sdc-boards`（机器注册表/网络拓扑）

## 0. 背景与目标

nightly pipeline 现为单机（0103，30G）执行：三泳道 2+2+2 槽 + audit 线，内存闸
MemAvailable ≥ 8G。用户意图：**把 SDC 板卡舰队纳入执行面**。摸底实测
（2026-11-13 20:38）：0102（192 核/754G，激发全停，gh 已登录 + claude + SSH push
钥匙全在位）开箱即用；0101/126 核/30G 与 0201/96 核/29G 几乎全闲（缺 gh）；
0104 水银核缺陷机、0202/0203 异网隧道断——排除。

## 1. 已确认决策表

| # | 决策点 | 结论 |
|---|---|---|
| 1 | 调度语义 | **常态分载**：每轮派发把本机+三板当一个资源池按 MemAvailable 择优——0102 几乎永远是主力，本机变兜底。55 条 QUEUED 积压的消化速度按节点数倍增 |
| 2 | gh 面 | **板上全流程**：远程 agent 与本机 agent 完全同款（同 prompt 同契约：修→验→push→gh pr create→贴报告→结束语 `task-loop: done <branch>`），只是执行地点变了。新代码只剩传输层 |
| 3 | 板池 | **0102 + 0101 + 0201 三板全入**；0104 水银核永久排除（缺陷机算出的验证结果不可信）；0202/0203 排除（Xshell 反向隧道不可依赖） |
| 4 | 架构 | **独立 nodepool 模块**（scripts/task_loop_nodepool.py）：板注册表 + SSH 封装 + 探测 + pick_node；pipeline 外科手术式接入 |
| 5 | 板克隆路径 | `/home/sdc/wangxu/sdcshield-autoci`（对齐板上既有 `~/wangxu/sdcshield` 命名空间），独立于用户的开发克隆 |
| 6 | 连接 | 0103 生成专用 SSH 密钥分发三板 `sdc@` authorized_keys（免密、无密码进 argv/进程表） |
| 7 | 激发冲突 | 板上 sdc 激发战役 active → **探测即跳过，绝不自动停**（SDC 检测实验优先于夜间队列） |
| 8 | audit | 留本机（其 state 写入与本机 state.json 耦合，远程会写错地方；且无构建、内存轻） |

## 2. 总体架构

```
本机 0103（编排器，唯一 state 写者；merge 门/revert/issue 收尾/报告零改动）
 cycle_once 三泳道派发点 → pick_node(泳道, state)
   候选 = [local, 0102, 0101, 0201]（注册表序）
   过滤: SSH 连通 ∧ 激发 inactive ∧ load15 < 核数×0.5
         ∧ 该节点 MemAvailable ≥ PIPELINE_MIN_MEM_GB ∧ 该节点该泳道有空槽
   择优: MemAvailable 降序
 spawn → local: 现行路径原样；remote: nodepool SSH 面
   ssh <板> 'cd /home/sdc/wangxu/sdcshield-autoci && nohup setsid bash -c
             "<source agent.env && timeout 3600 claude -p ...>" >> <板上日志> 2>&1 & echo $!'
 探活/收割/软停 → 按 host 分流（local 原路径 / remote SSH 同形命令）
```

崩溃语义：板上 agent 是板上 setsid 独立进程——**本机 pipeline 死/重启不影响板上
在途工作**（比纯本机形态更抗本机重启习惯）；次夜启动对账时 ssh 探活收割。

## 3. 板注册表与环境

### 3.1 注册表（task_loop_nodepool.py 常量；凭据不入库）

```python
BOARD_NODES = [
    {"host_id": "0102", "ssh": "sdc@172.168.160.42:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 192, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
    {"host_id": "0101", "ssh": "sdc@172.168.177.97:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 126, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
    {"host_id": "0201", "ssh": "sdc@172.168.178.81:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 96, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
]
```

SLOT_KINDS 语义升级为按节点：local 保持现值 2/2/2，三板 v1 统一 2/2/2（后续想给
0102 更多槽只改注册表一行）。

### 3.2 一次性 setup（实施时无人值守，全部幂等，脚本化 `scripts/nodepool_setup.sh`）

1. 0103 生成 `~/.ssh/id_ed25519_nodepool` → 三板 `~/.ssh/authorized_keys` 追加
2. 三板各建 `/home/sdc/wangxu/sdcshield-autoci` 独立克隆（remote 用 https+gh 凭据
   ——`gh auth setup-git` 后 https push 可用；0102 亦可沿用其既有 SSH 钥匙，统一
   https 简化注册表）
3. 0101/0201 补 gh：下载 gh 二进制（板到 github 443 通，实测）→
   `gh auth login --with-token`（用户 token，与 0102 现状同级）→ `gh auth setup-git`
4. vendored 依赖 build（openssl/openblas/sleef/isa-l/acl，~30 分钟/板）+ meson
   release 构建 + `--list-tests` + zstd19 冒烟各板各跑一次，输出留档
5. 板上建 `~/sdcshield-autoci-state/` 日志目录

### 3.3 每夜运行时刷新（cycle 派发前，对"将被派发的板"）

- `scp agent.env → 板:~/sdcshield-autoci-state/agent.env`（0600；防 token 轮换）
- 板克隆 `git fetch origin && git reset --hard origin/main`（同本机 heal 模式；
  与板上 `.worktrees/` 隔离，reset 主克隆不动 worktree）

## 4. 槽位账本与派发

槽位条目增加 host 字段（StateStore 序列化兼容：旧 state 无新键 → host="local"）；
日志/工作树路径字段沿用现有键（agent_log/worktree），其值对远程槽是**板上路径**
——按 host 决定访问方式（local 直接读 / remote 经 ssh 读）：

```jsonc
{"key": "issue-612", "host": "0102", "pid": 12345, "pgid": 12340,
 "started_at": "...", "boot": "...",          // boot=本机 boot_id（照记，远程豁免不用它）
 "log": "<板上日志路径>", "worktree": "<板上 worktree 路径>"}
```

- `free_slot_count(state, lane)` → 按节点分账（host 维度）
- `dispatch_agent` / `dispatch_review`：派发前 `nodepool.pick_node(lane, state)`；
  派发命令形态 local/remote 同款，remote 仅包进 ssh（含本机同款 `timeout 3600`
  内嵌）；spawn 失败（SSH 不通）→ 换下一节点，不占槽不燃 attempts
- `slot_acquire` 记 host；同 key 互斥语义跨节点成立（同一 issue 不会同时在两台跑）

## 5. 探活/收割/软停分流（nodepool SSH 面）

| 操作 | remote 形态 |
|---|---|
| 探活 | `ssh <板> 'kill -0 <pid>'` 退出码；**SSH 不通 → unreachable：槽保留、attempts 不燃** |
| 日志收割 | `ssh <板> 'tail -c 4000 <remote_log>'` → 结束语契约/认证失败正则**同款零改动**（check_agent_outcome 只消费文本） |
| 软停击杀 | `ssh <板> 'kill -TERM -<pgid>'` → grace 后 `-KILL`；worktree 清理 `ssh ... worktree remove` + prune；unreachable 槽不击杀，留次夜 reconcile（agent 自带 timeout 有界自灭） |
| 次夜对账 | 远程槽：ssh 通 → PID 活则继续等/死则读日志判结局；**板重启豁免**：板上 `uptime -s` 早于 `started_at` → 环境性死亡不燃 attempts（本机 boot_id 豁免的同族语义） |

reap 结算、issue 收尾、merge 门、revert 归因、夜间报告**零改动**——只消费日志尾与
PR 状态，远程/本地同构。

## 6. 错误边界（每处 SSH 失败有定义好的降级）

| 场景 | 行为 |
|---|---|
| 探测 SSH 不通 | 该板本轮剔除，其余候选继续；全挂 → 本机；本机也不足 → 维持现行"本轮不派" |
| 派发 SSH 失败 | 换下一节点；不占槽不燃 attempts |
| 探活/收割 SSH 不通 | unreachable：槽保留、attempts 不燃、次夜 reconcile |
| 软停 SSH 不通 | 不击杀；agent timeout 自灭有界 |
| 板重启杀 agent | uptime 豁免不燃 attempts |
| 磁盘满/真崩/gh token 失效 | 日志如实 → 照常燃 attempts/升级（真实失败诚实计；claude auth 熔断 streak 照常保护） |
| 激发 active | 探测即跳过，绝不自动停 |

安全面：0103 专用私钥仅授权三板 sdc 用户（可随时 rm authorized_keys 行撤销）；
agent.env scp 目标 0600；板上不落本机 state；密码不进任何 argv/代码。

## 7. 测试与实施路线

- **nodepool 单测**（tools/excite/tests/test_task_loop_nodepool.py）：SSH 命令形态
  argv 级锚（spawn/probe/kill/logtail/heal/refresh）；探测解析（free/uptime/
  systemctl 输出形态桩）；pick_node 过滤×排序矩阵（不可达/激发中/load 高/内存低/
  满槽/全挂→本机兜底/多板择优降序）
- **pipeline 集成测**：槽位 host 字段往返与旧 state 升格；探活分流（monkeypatch
  nodepool）；软停双路径；unreachable 豁免不燃 attempts；同 key 跨节点互斥
- **真实冒烟（终验）**：对 0102 真派一个可弃测试 issue（enqueue 一个已知可修的
  小 bug）→ 远程修→push→gh pr create 全链路实证（引用真实输出）
- **实施顺序**：nodepool 模块（TDD）→ setup 脚本 + 三板真实 setup（含 vendored
  build）→ pipeline 接入（TDD）→ 全量 → 真实冒烟 → 合入 main（autoci heal
  自动生效）→ 首夜观察并入既有 4 夜观察窗
- **实施模式**：本会话实施、新分支 `feat/nodepool-offload`、人工 merge（同上
  feature 模式）

## 8. 非目标（YAGNI）

- 不做 0202/0203 隧道板（不可依赖）；0104 永不入选
- 不做动态槽位配额/自动扩缩容（注册表静态，0102 加槽=改一行配置）
- 不自动启停板上激发战役；不代理 gh（板上全流程已定）
- 不做跨板分布式锁/选主（本机仍是唯一编排器与 state 写者）
