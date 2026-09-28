# 2026-09-28 cn23154 cpu139 SDC — sdcshield(SVE/SME) 复现与微架构诊断

> 计划正文（活文档，随执行更新）：/home/share/suke/wangxu/sdcshield-sve-repro/planning/task_plan.md
> 战役根目录：/home/share/suke/wangxu/sdcshield-sve-repro/（jobs/logs/env/runs/planning）
> 本文件为仓库侧入口（规则 4 计划先行），完成后随代码/文档一并提交。

## 目标
1. 构建 /home/share/suke/wangxu/sdcshield（SVE+SME 支持）于 cn23154。
2. 用 sdcshield 用例（优先 SVE，参数/选项/改代码均可）复现 cpu139 SDC；
   608 核每核研究；全程 cpu139 ~2GHz（1.55GHz 不复现，root tuned 已锁 2.0GHz）。
3. 复现后微架构级诊断（FU 级定位，参照 CORE179 路径）。

## 前置事实（详见战役目录 findings.md）
- cpu139 唯一故障核、byte6 签名 17/17、频率相关（0903/0923/0924 战役已证）。
- 9 类合成负载 + mini_laplacian 全 clean → 触发需 XLSDFT 级上下文；
  P5 缺口=SVE gather/多成分寻址序列未复刻；崩溃点含 SME za st1d。
- sdcshield sve512 家族（gather_scatter/SVD/stencil_axis）从未在该节点跑过。

## 阶段（详细复选框见战役 task_plan.md）
- P0 环境探测 + 故障存在性 oracle（F 臂必须仍崩，否则止损上报）
- P1 构建（SVE 家族可用；SME 视 HW/编译器实证，诚实降级）
- P2 608 核全核基线（全节点运行 + 139/140 差分 soak）
- P3 复现升级阶梯（参数→上下文→新测试 T1/T2/T3，三振原则）
- P4 触发器 608 核对比（证 139 选择性，频率门控）
- P5 微架构诊断（签名/单元隔离/PMC/综合定位）
- P6 文档同步 + 收尾（dkill 复核，无遗留作业）

## 纪律
- 一 patch 一单元、DCO、feature 分支 research/sdcshield-sve-sme-cn23154-0928
- 频率协议：139 窗口 ≥1800 VALID/1500-1799 GREY/<1500 EXCLUDED；freqmon 10s
- 预期外结果如实记录；placeholder 诚实（EXIT_SKIP 带原因）
- 访问边界：仅 /home/share/suke/wangxu/ + 系统文件

## Phase 3 新测试设计（2026-09-28 21:10 补，T1/T2/T3）

### T3 `sme_za_tileview_arm`（已完成，待 build9 验证后提交）
SME ZA tile 语义 golden 比对：za0h 装载→h 读回（恒等）、v 读出（转置）、
向量选择 imm=1（行 (r+1)%8）、za1h 独立 round-trip、za0 隔离性。
F15 实测语义做 golden；SVL≠64B 或无 HWCAP2_SME 诚实 SKIP。
meson：`-march=armv9-a+sve+sme+sme-f64f64` 独立静态库，has_argument 探测，
gcc 无 SME 支持时自动剔除（x86/参考构建零影响）。

### T1 `sve512_xlswave_ctx_arm` — 复合上下文（XLSDFT 成分合成）
依据：F3 缺口「SVE gather/多成分寻址序列未复刻」+ 9 类合成负载与
mini_laplacian 全 clean → 触发需多成分复合。
设计：单测试内三相位轮转（TEST_LOOP 内 A→B→C 循环），
- A 相位（stencil 轴核）：svdup 系数 ld1rd + RADIUS=6 双 svmla 串行链
  （移植 sve512_stencil_axis_arm 内核）；
- B 相位（SVD 块游走 + gather）：16×16 块在 L2 溢出工作集上按 2-D 置换
  索引 gather 装载 + svmla（移植 gather_scatter_svd 寻址拓扑）；
- C 相位（校验）：VA[63:48] 指针金丝雀 + 轮转子集 memcmp（每 N 迭代全量）。
golden 由框架侧标量 C 代码独立计算（不依赖 SVE）。
构建：与 sve512 家族同（armv8.2-a+sve 静态库 + HWCAP_SVE 探测）。
验收：zstd19 回归 + 新测试 -t 5000 -n 1 双核冒烟 + 零新告警。

### T2 `libomp_ctx_arm` — libomp 上下文仿真（T1 后）
依据：F3「运行时内核交互」上下文要素（libomp 元数据损坏 5/17）。
设计：测试线程内模拟 fork/join 停放模式——周期性 pthread barrier 同步
（模拟 parallel region 边界）+ 停放窗口内 SVE 空转/微活动 + 活动窗口
SVE FMA 链；金丝雀布放于模拟「元数据」结构（含函数指针表 + 计数器），
每轮 memcmp。诚实边界：真实 libomp TLS/元数据布局无法在测试内复刻，
本测试只仿真访问模式，报告须如实标注。

### 顺序与依赖
T3 验证提交 → 规则 7 文档同步 → push → T1 编码提交 → T2 →
全触发电池（T1+T2+T3+sve512 全族）排 2GHz 哨兵窗口自动触发。
