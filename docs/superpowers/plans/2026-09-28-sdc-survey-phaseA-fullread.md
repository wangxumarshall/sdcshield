# SDC 综述阶段 A:53 篇 PDF 全文精读 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 对 `docs/paper/ref/` 下 53 篇 PDF 全文精读,产出结构化精读笔记(企业合作三级证据+页码、核心结论、分类学标注),为阶段 B 综述报告提供纯一手素材。

**Architecture:** 8 个主题批(R1–R8)串行精读,每批读完落盘笔记并 commit;笔记是唯一可信载体;单篇精读按本文 SOP 执行;全部完成后汇总企业合作总表并做验收自检。

**Tech Stack:** Read 工具(PDF 按页,单次 ≤20 页)、Write/Edit(markdown 笔记)、git via Bash。

**Spec:** `docs/superpowers/specs/2026-09-28-sdc-academic-survey-design.md`(本计划执行 spec 的阶段 A;阶段 B 另行计划)

## Global Constraints(来自 spec,全任务适用)

1. **纯精读**:不复用任何旧结论(旧笔记目录已删除,不存在);一切以本次 Read 到的 PDF 原文为准。
2. **身份核实**:每篇论文身份以 PDF 页脚/内容核实;文件名可能有误导,名实不符一律记入笔记"身份核实"字段(汇入报告附录 B)。
3. **企业证据分级+页码**:一级=作者 affiliation 挂企业;二级=致谢章节资助/数据/设备;三级=首页脚注资助声明。每条带 PDF 页码;无则明写"原文无致谢章节"/"原文无脚注资助声明",不留空。
4. **数字溯源**:每个关键数字标注论文+节号/图表号;读不到的写"原文未给出"。
5. **文档类型标注**:白皮书/工业文档/观点文章如实标注类型,不冒充同行评审结论。
6. **分类学标注不硬套**:按论文实际内容归类,不适用项标 N/A。
7. **git 纪律**:feature 分支 `research/survey-fullread-20260928`(已存在,勿在 main 上工作);每批一个 commit,`git commit -s`(签名行 `Signed-off-by: wangxu <wangxumarshall@qq.com>` 为末行,**不加 Co-Authored-By**);commit 前跑占位符检查;阶段末 push。
8. **执行约束**:subagent 至多 1 个在飞;Bash 调用串行发。
9. **PDF 文件名含空格**:bash 命令中路径必须加双引号。

## 标准作业程序(SOP):单篇精读协议

每篇论文严格按以下顺序执行(所有批次任务共用):

- **Step A — 读 PDF**:Read `docs/paper/ref/<文件名>`,`pages: "1-20"`。若论文超过 20 页(Read 结果可见未读完),再 Read 剩余页(如 `"21-40"`)。
- **Step B — 提取五类信息**(全部来自本次读到的原文):
  1. **身份**:首页/页脚的论文标题、作者、会议/年份、DOI;与文件名比对;
  2. **机构与企业证据**:作者全列+各自机构(学术/企业分列);致谢章节位置与内容;首页脚注资助声明;
  3. **核心结论与关键数字**:摘要、引言贡献列表、结论;实验关键数字(带节/图表号);
  4. **分类学素材**:论文涉及的根因机理/故障模式/检测技术/处理技术类型;
  5. **业界观点**(仅工业文档/观点文章):立场、产品化路线、对学界方法的评价。
- **Step C — 写笔记**:`Write` 到 `docs/paper/ref-paper-fullread-digest/<NN>-<slug>.md`,模板如下(字段全写,不留空;不适用的如实标注):

```markdown
# [NN] <论文标题>(<会议/年份;文档类型:会议论文/期刊/白皮书/工业文档/观点文章>)

- PDF: <文件名>;页数 N(实读范围 p.1–N)
- 作者(全列):……
- 机构(学术/企业分列):……
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):……(p.X)
  - 二级(致谢章节资助/数据/设备):……(p.X)或"原文无致谢章节"
  - 三级(首页脚注资助声明):……(p.X)或"原文无脚注资助声明"
- 核心结论(3–5 条,各带节号/图表号):……
- 分类学标注(按论文实际内容归类;不适用项标 N/A,不硬套):
  - 根因机理类型:……
  - 故障模式类型:……
  - 检测技术类型:……
  - 处理技术类型:……
- 业界观点摘录(仅工业文档/观点文章;学术论文写"不适用"):……
- 关键数字(表):……
- 方法论要点:……
- 横向对比注记(与同批/前批论文的关系,读后记):……
- 身份核实:……(页脚核实结果;名实相符或勘误说明)
```

- **Step D — 异常分支**:PDF 无法读取/损坏 → 笔记只写"无法读取:<现象>"并如实报告,不猜测内容;正文跳过该篇并在 INDEX 标注 `unreadable`。

## 53 篇权威分配表(本表为 Task 0 INDEX.md 的内容来源)

| 编号 | 批 | PDF 文件名(ref/ 下) | 笔记 slug |
|---|---|---|---|
| 01 | R1 | SOSP23 Understanding Silent Data Corruptions in a Large Production CPU Population.pdf | sosp23-fleet |
| 02 | R1 | HPCA2026 PinDrop_Breaking_the_Silence_on_SDCs_in_a_Large-Scale_Fleet.pdf | pindrop |
| 03 | R1 | Fleetscanner_Ripple Detecting silent data corruptions in the wild.pdf | ripple |
| 04 | R1 | Veritas__Demystifying_Silent_Data_Corruptions_Arch-Level_Modeling_and_Fleet_Data_of_Modern_x86_CPUs.pdf | veritas |
| 05 | R1 | ASPOLOS 2026 SEVI Silent Data Corruption of Vector Instructions in Hyper-Scale Datacenters.pdf | sevi |
| 06 | R1 | ITHICA Intra-Thread Instruction Checking Approach for Defect-Induced Silent Data Corruptions.pdf | ithica |
| 07 | R1 | ASPLOS 2025 Hardware Sentinel Protecting Software Applications from Hardware Silent Data Corruptions.pdf | hwsentinel |
| 08 | R1 | Silent_Data_Corruptions_Microarchitectural_Perspectives.pdf | tc23-micropersp |
| 09 | R1 | Silent Data Corruption by 10x Test Escapes Threatens Reliable Computing.pdf | test-escapes |
| 10 | R2 | Measuring_architectural_vulnerability_factors.pdf | measuring-avf |
| 11 | R2 | A Systematic Methodology to Compute the Architectural Vulnerability Factors for a High-Performance Microprocessor.pdf | systematic-avf |
| 12 | R2 | Computing_architectural_vulnerability_factors_for_address-based_structures.pdf | addr-avf |
| 13 | R2 | A First-Order Mechanistic Model for Architectural Vulnerability Factor.pdf | first-order-avf |
| 14 | R2 | Applying Architectural Vulnerability Analysis to Hard Faults in the Microprocessor.pdf | hard-faults-ava |
| 15 | R2 | MICRO2024 DelayAVF_Calculating_Architectural_Vulnerability_Factors_for_Delay_Faults.pdf | delay-avf |
| 16 | R2 | MeRLiN Exploiting Dynamic Instruction Behavior for Fast and Accurate Microarchitecture Level Reliability Assessment.pdf | merlin |
| 17 | R2 | Demystifying_the_System_Vulnerability_Stack_Transient_Fault_Effects_Across_the_Layers.pdf | vuln-stack |
| 18 | R2 | From_Gates_to_SDCs_Understanding_Fault_Propagation_Through_the_Compute_Stack.pdf | gates-to-sdc |
| 19 | R3 | GemFI_A_Fault_Injection_Tool_for_Studying_the_Behavior_of_Applications_on_Unreliable_Substrates.pdf | gemfi |
| 20 | R3 | Chaos Controlled Hardware Fault Injector System for Gem5.pdf | chaos |
| 21 | R3 | Differential_Fault_Injection_on_Microarchitectural_Simulators.pdf | diff-fi |
| 22 | R3 | Gem5-MARVEL_Microarchitecture-Level_Resilience_Analysis_of_Heterogeneous_SoC_Architectures.pdf | gem5-marvel |
| 23 | R3 | Estimating_the_Failures_and_Silent_Errors_Rates_of_CPUs_Across_ISAs_and_Microarchitectures.pdf | cross-isa |
| 24 | R3 | Soft_Error_Effects_on_Arm_Microprocessors_Early_Estimations_versus_Chip_Measurements.pdf | arm-soft-error |
| 25 | R4 | silifuzz.pdf | silifuzz |
| 26 | R4 | Harpocrates_Automated_Functional_Program_Generation_Against_CPU_Faults_and_Silent_Data_Corruptions.pdf | harpocrates-isca24 |
| 27 | R4 | Harpocrates_Breaking_the_Silence_of_CPU_Faults_through_Hardware-in-the-Loop_Program_Generation.pdf | harpocrates-micro26 |
| 28 | R4 | ASPOLOS2024 Proactive Runtime Detection of Aging-Related Silent Data Corruptions A Bottom-Up Approach.pdf | aging-asplos24 |
| 29 | R4 | ets2024_gizopoulos.pdf | ets24 |
| 30 | R4 | Improved Silent Data Error Detection through Test Optimization using Reinforcement Learning, IRPS Improved SDE Detection 2025.pdf | irps25-rl |
| 31 | R4 | Robust_Pattern_Generation_for_Small_Delay_Faults_under_the_Impact_of_Variations.pdf | sdf-pattern |
| 32 | R4 | Strategies For Detecting Sources Of Silent Data Corruption.pdf | strategies-detect |
| 33 | R5 | SOSP2025 Orthrus Efficient and Timely Detection of Silent User Data Corruption in the Cloud with Resource-Adaptive Computation Validation.pdf | orthrus |
| 34 | R5 | Detecting Silent Data Corruption in Sparse Matrices using Hardware Performance Counter.pdf | sparse-pmc |
| 35 | R5 | Detecting Silent Data Corruption from Hardware Counters.pdf | hw-counters |
| 36 | R5 | SHOUT-Trainer_Closed-loop_Trainer_for_Silent_Data_Corruption_Hunting_and_Observation_Using_Transformers.pdf | shout-trainer |
| 37 | R5 | Efficient_Instruction_Vulnerability_Prediction_With_Heterogeneous_SDC_Propagation_Knowledge_Graph.pdf | kg-vulnpred |
| 38 | R5 | SENTRY A Dual-Layer Technique for Silent Data Corruption Detection in Deterministic Database Systems.pdf | sentry |
| 39 | R5 | Detection and Prevention of Silent Data Corruption in an Exabyte-scale Database System.pdf | exabyte-db |
| 40 | R6 | Protecting Futures against Silent Data Corruption -- Efficient Task Replication for Dynamic Data Dependencies.pdf | futures-replication |
| 41 | R6 | Parallaft Runtime-Based CPU Fault Tolerance via Heterogeneous Parallelism.pdf | parallaft |
| 42 | R6 | ParaVerser Harnessing Heterogeneous Parallelism for Affordable Fault Detection in Data Centers, dsn25.pdf | paraverser |
| 43 | R6 | Deploying Lightweight Input-Aware Selective Instruction Duplication in HPC Applications.pdf | hpc-duplication |
| 44 | R7 | Understanding_Recommendation_System_Robustness_Against_Silent_Data_Corruption_An_Empirical_Study.pdf | recommendation |
| 45 | R7 | On the Vulnerability of FHE Computation to Silent Data Corruption.pdf | fhe |
| 46 | R7 | Be Aware of Metadata Corruption in Parallel File System It can be Silent and Catastrophic.pdf | fs-metadata |
| 47 | R7 | The_Dark_Side_of_Computing_Silent_Data_Corruptions.pdf | dark-side |
| 48 | R7 | Special Issue on Silent Data Corruptions—From Silicon to Cloud Data Centers and AI Systems of Huge Scale.pdf | special-issue |
| 49 | R7 | Silent_Data_Corruptions_The_Stealthy_Saboteurs_of_Digital_Integrity.pdf | stealthy-saboteurs |
| 50 | R7 | Silent Data Corruptions in Computing Understand and Quantify iolts2024_macieira.pdf | iolts24-quantify |
| 51 | R8 | NAVIgator Exploring the Voltage Limits of AMD NAVI GPUs for Energy Efficient Computing iolts2025_trakosa.pdf | navigator-gpu |
| 52 | R8 | Reliability assessment of AMD MicroBlaze-V TMR architecture using fault injection and proton irradiation.pdf | microblaze-tmr |
| 53 | R8 | proteanTecs White Paper_Outsmarting Silent Data Corruption in AI Processors With Two-Stage Detection.pdf | proteantecs-wp |

(批次篇数:R1=9、R2=9、R3=6、R4=8、R5=7、R6=4、R7=7、R8=3,合计 53。此为 spec §3.2 预分批次表的精确落实,按标题预分、精读中可调整归属——调整时同步改 INDEX.md 并在笔记"身份核实"字段注明。)

---

### Task 0: 初始化精读目录与索引

**Files:**
- Create: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Produces: `ref-paper-fullread-digest/` 目录 + INDEX.md(后续所有批次任务按其编号/slug 写笔记、更新其状态列)

- [ ] **Step 1: 确认分支与 PDF 全集**

Run: `cd /c/Users/ubuntu/Documents/sdc/sdcshield && git branch --show-current && ls docs/paper/ref/*.pdf | wc -l`
Expected: `research/survey-fullread-20260928` 和 `53`。若 PDF 数 ≠53:停下,与用户确认范围后再继续(勿自行增删)。

- [ ] **Step 2: 写 INDEX.md**

`Write` `docs/paper/ref-paper-fullread-digest/INDEX.md`,内容:标题行 `# 53 篇 SDC 论文全文精读索引`、日期、分支名,然后上方"53 篇权威分配表"整表原样复制,再加一列 `状态`(初始全部 `pending`),表后加一行说明:`状态:pending/in-progress/done/unreadable;模板与 SOP 见计划 2026-09-28-sdc-survey-phaseA-fullread.md`。

- [ ] **Step 3: 占位符与完整性检查**

Run: `grep -cE "^\|" "docs/paper/ref-paper-fullread-digest/INDEX.md"`
Expected: ≥54(表头+分隔+53 行)。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/INDEX.md
git commit -s -m "docs(digest): 精读索引 INDEX——53 篇批次分配与状态跟踪"
```

---

### Task 1: R1 批精读——生产机队实证(01–09,9 篇)

**Files:**
- Create: `docs/paper/ref-paper-fullread-digest/01-sosp23-fleet.md` … `09-test-escapes.md`(9 个笔记,文件名见分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`(状态列)

**Interfaces:**
- Consumes: 分配表 R1 行的 PDF 文件名与 slug;SOP 模板
- Produces: 9 篇笔记(后续批次的"横向对比注记"会引用 R1 结论;模板校准结论记入 INDEX)

- [ ] **Step 1: 逐篇精读 01–09**

对分配表 01–09 每篇,按 SOP Step A–C 执行:Read PDF(pages "1-20",超 20 页续读)→ 提取五类信息 → Write 笔记 `NN-<slug>.md`(模板全文,字段不留空)。每读 3 篇至少落盘 3 篇(防上下文丢失)。

- [ ] **Step 2: 更新 INDEX 状态**

将 INDEX.md 中 01–09 行状态改为 `done`(异常篇按实情标 `unreadable` 并在该行加注)。

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/0*.md`
Expected: 无输出(exit 1)。命中则修复该笔记后重跑至干净。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R1 生产机队实证 9 篇精读笔记(01-09)"
```

- [ ] **Step 5: 模板校准(spec §6 风险条款)**

R1 读完后评估:分类学 4 字段是否够用、归类口径是否清晰、模板是否需增删字段。结论(含"不调整"或具体调整)追加到 INDEX.md 末尾 `## 模板校准记录` 小节;若调整,后续批次一律用调整后模板。

---

### Task 2: R2 批精读——AVF 理论与故障传播(10–18,9 篇)

**Files:**
- Create: `10-measuring-avf.md` … `18-gates-to-sdc.md`(9 个笔记,按分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R2 行;SOP 模板(含 Task 1 Step 5 校准后的版本);R1 笔记的结论(横向对比注记用)
- Produces: 9 篇笔记

- [ ] **Step 1: 逐篇精读 10–18**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。每 3 篇落盘。注意:本批老论文(2003–2012)页脚身份核实尤其重要,勘误记入"身份核实"字段。

- [ ] **Step 2: 更新 INDEX 状态(10–18 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/1*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R2 AVF 理论与故障传播 9 篇精读笔记(10-18)"
```

---

### Task 3: R3 批精读——故障注入与软错误实测(19–24,6 篇)

**Files:**
- Create: `19-gemfi.md` … `24-arm-soft-error.md`(6 个笔记,按分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R3 行;SOP 模板;R1/R2 笔记结论
- Produces: 6 篇笔记

- [ ] **Step 1: 逐篇精读 19–24**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。每 3 篇落盘。

- [ ] **Step 2: 更新 INDEX 状态(19–24 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/{19,20,21,22,23,24}-*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R3 故障注入与软错误实测 6 篇精读笔记(19-24)"
```

---

### Task 4: R4 批精读——测试生成与制造测试(25–32,8 篇)

**Files:**
- Create: `25-silifuzz.md` … `32-strategies-detect.md`(8 个笔记,按分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R4 行;SOP 模板;R1–R3 笔记结论
- Produces: 8 篇笔记

- [ ] **Step 1: 逐篇精读 25–32**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。每 3 篇落盘。注意 30(IRPS'25)、31(SDF 模式)、32(Strategies)可能是半导体测试界文档,文档类型按页脚如实标注。

- [ ] **Step 2: 更新 INDEX 状态(25–32 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/{25,26,27,28,29,30,31,32}-*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R4 测试生成与制造测试 8 篇精读笔记(25-32)"
```

---

### Task 5: R5 批精读——运行时检测(33–39,7 篇)

**Files:**
- Create: `33-orthrus.md` … `39-exabyte-db.md`(7 个笔记,按分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R5 行;SOP 模板;R1–R4 笔记结论
- Produces: 7 篇笔记

- [ ] **Step 1: 逐篇精读 33–39**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。每 3 篇落盘。

- [ ] **Step 2: 更新 INDEX 状态(33–39 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/{33,34,35,36,37,38,39}-*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R5 运行时检测 7 篇精读笔记(33-39)"
```

---

### Task 6: R6 批精读——运行时容错与复制(40–43,4 篇)

**Files:**
- Create: `40-futures-replication.md` … `43-hpc-duplication.md`(4 个笔记,按分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R6 行;SOP 模板;R1–R5 笔记结论
- Produces: 4 篇笔记

- [ ] **Step 1: 逐篇精读 40–43**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。4 篇一批一次落盘。

- [ ] **Step 2: 更新 INDEX 状态(40–43 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/{40,41,42,43}-*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R6 运行时容错与复制 4 篇精读笔记(40-43)"
```

---

### Task 7: R7 批精读——软件栈上层、AI 与视野(44–50,7 篇)

**Files:**
- Create: `44-recommendation.md` … `50-iolts24-quantify.md`(7 个笔记,按分配表)
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R7 行;SOP 模板;R1–R6 笔记结论
- Produces: 7 篇笔记

- [ ] **Step 1: 逐篇精读 44–50**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。每 3 篇落盘。注意 47(Dark Side)、48(Special Issue)、49(Stealthy Saboteurs)很可能是观点/导言文章——"业界观点摘录"字段为主战场,文档类型如实标注。

- [ ] **Step 2: 更新 INDEX 状态(44–50 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/{44,45,46,47,48,49,50}-*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R7 软件栈上层·AI·视野 7 篇精读笔记(44-50)"
```

---

### Task 8: R8 批精读——GPU、器件与工业白皮书(51–53,3 篇)

**Files:**
- Create: `51-navigator-gpu.md`、`52-microblaze-tmr.md`、`53-proteantecs-wp.md`
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`

**Interfaces:**
- Consumes: 分配表 R8 行;SOP 模板;R1–R7 笔记结论
- Produces: 3 篇笔记(53 号为白皮书,"业界观点摘录"为主)

- [ ] **Step 1: 逐篇精读 51–53**

按 SOP Step A–C 逐篇执行(Read "1-20",超页续读 → 提取 → Write 笔记)。53(proteanTecs)明确标注"白皮书"类型。

- [ ] **Step 2: 更新 INDEX 状态(51–53 行 → done)**

- [ ] **Step 3: 占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/{51,52,53}-*.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/
git commit -s -m "docs(digest): R8 GPU·器件·工业白皮书 3 篇精读笔记(51-53)"
```

---

### Task 9: 企业合作信息总表汇总

**Files:**
- Create: `docs/paper/ref-paper-fullread-digest/corporate-collab.md`

**Interfaces:**
- Consumes: 53 篇笔记的"机构"+"企业合作证据"字段(只从笔记抄录,页码随之;不回 PDF、不凭记忆)
- Produces: `corporate-collab.md`(阶段 B 生态章的直接素材)

- [ ] **Step 1: 汇总 53 条**

`Write` `corporate-collab.md`,按批分组,每篇一行:

```markdown
# 53 篇论文企业合作信息总表

> 来源:本目录 01–53 笔记的"机构/企业合作证据"字段(2026-09-28 精读);证据级:1=作者 affiliation,2=致谢,3=脚注资助。

| 编号 | 论文(简称) | 会议/年份 | 学术机构 | 合作企业 | 证据级 | 证据位置 | 合作形态 |
|---|---|---|---|---|---|---|---|
| 01 | SOSP23 机队实证 | SOSP'23 | … | … | 1,2 | p.1, p.15 | 联合研究 |
```

(一行一论文,53 行;无企业证据的写"未发现企业合作证据";一二级都没有的如实呈现。)

- [ ] **Step 2: 行数与一致性检查**

Run: `grep -cE "^\| [0-9]" docs/paper/ref-paper-fullread-digest/corporate-collab.md`
Expected: `53`。

- [ ] **Step 3: Commit**

```bash
git add docs/paper/ref-paper-fullread-digest/corporate-collab.md
git commit -s -m "docs(digest): 53 篇企业合作信息总表 corporate-collab"
```

---

### Task 10: 阶段 A 验收自检与推送

**Files:**
- Modify: `docs/paper/ref-paper-fullread-digest/INDEX.md`(若有补记)

**Interfaces:**
- Consumes: spec §7 验收标准;全部产物
- Produces: 阶段 A 完成的确认;推送后的 feature 分支

- [ ] **Step 1: 笔记数量检查**

Run: `ls docs/paper/ref-paper-fullread-digest/*.md | grep -v -E "INDEX|corporate" | wc -l`
Expected: `53`。

- [ ] **Step 2: 全量占位符检查**

Run: `grep -lE "TBD|TODO|【待|待补|占位|……" docs/paper/ref-paper-fullread-digest/*.md`
Expected: 无输出。

- [ ] **Step 3: 企业证据页码抽查**

Run: `grep -L "p\." docs/paper/ref-paper-fullread-digest/0*.md docs/paper/ref-paper-fullread-digest/1*.md docs/paper/ref-paper-fullread-digest/2*.md docs/paper/ref-paper-fullread-digest/3*.md docs/paper/ref-paper-fullread-digest/4*.md docs/paper/ref-paper-fullread-digest/5*.md`
Expected: 无输出(每篇笔记都含页码引用)。

- [ ] **Step 4: INDEX 一致性检查**

Run: `grep -c "| done |" docs/paper/ref-paper-fullread-digest/INDEX.md`(或当前状态列写法)
Expected: 与实际完成数一致(53 或 53 − unreadable 数)。

- [ ] **Step 5: commit 签名合规检查(spec 验收第 5 条)**

Run: `git log -15 --grep="Signed-off-by: wangxu <wangxumarshall@qq.com>" --oneline | wc -l && git log -15 --oneline | wc -l && git log -15 | grep -c "Co-Authored-By" || true`
Expected: 前两数相等(本阶段所有 commit 均带签名行);第三数为 0(无 Co-Authored-By)。

- [ ] **Step 6: 修复(如有)并 commit**

任何检查不过 → 修复对应笔记/INDEX/补签名 → 重跑至全过 → `git add` + `git commit -s -m "docs(digest): 阶段 A 验收自检修复"`。全过则跳过本步。

- [ ] **Step 7: Push**

```bash
git push -u origin research/survey-fullread-20260928
```

- [ ] **Step 8: 报告验收结果**

对照 spec §7 五条标准逐条报告通过情况(含 unreadable 清单,如有),等用户确认后进入阶段 B(基于笔记制定报告计划)。
