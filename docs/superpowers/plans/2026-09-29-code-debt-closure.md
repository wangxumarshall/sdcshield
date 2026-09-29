# v5 代码级欠账收尾 plan（runbook §9.3 存量代码项）

> 目标：清零 runbook §9.3 中的**代码级**欠账（非用户决策、非时间等待项）。每项一补丁一单元、SDD subagent 执行 + 评审，最后合并 PR。战役持续运行不中断；部署类动作（monitor/driver 重启）集中在最后由用户决策窗口执行。

## Task 清单

| # | 任务 | 优先级 | 状态 |
|---|---|---|---|
| 1 | **fsync 补丁**：`tools/telemetry/sdc_eventd.py`/`tools/control/sdc_controller.py`/`tools/analysis/sdc_timeline.py` 的 `_atomic_json` 补 `f.flush()+os.fsync()`（今晨 controller 崩溃循环的根因，下次硬重启必复现） | P0（生产缺陷） | 待 |
| 2 | **驱动日汇总 loop-count token**：`daily_summary` 追加逐文件 loop-count 累计，exposure 生产端拿真值 | P1 | 待 |
| 3 | **status.sh 扩展 + README 运维入口**（补丁单元 14 尾巴）：status.sh 增 M2/M4 事件流/controller 状态/采集器丢样列 | P1 | 待 |
| 4 | **d15 kdump 只读探针演练**（M5 终审 Minor #2） | P2 | 待 |
| 5 | **systemd 单元加固**（NoNewPrivileges + ProtectSystem 埽） | P2 | 待 |
| 6 | **kdump 验收 + PR + 部署**：全部合入后重打包 81 机 | P2 | 待 |

## 约束
- 分支 `feat/sdc-excite-reproduce-m5-tail` 从 main 建
- 战役运行中：不碰运行实例/不重启服务（重启归用户决策窗口）
- 每 Task 一个 subagent + 评审 + 一 commit
