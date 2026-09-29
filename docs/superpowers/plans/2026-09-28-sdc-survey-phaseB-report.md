# SDC 学术综述报告(阶段 B)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于 53 篇精读笔记(`docs/paper/ref-paper-fullread-digest/`)撰写以分类学为骨架的 SDC 学术综述报告 `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(正文 12000–20000 字,汇报/展示材料受众)。

**Architecture:** 三层叙事(对象层 Ch2–4 → 对策层 Ch5–6 → 两界层 Ch7–9)+ 执行摘要与启示包夹(Ch0/Ch10)+ 两附录(A 逐篇档案 / B 身份勘误)。每章执行"素材提取(grep/awk 到临时文件)→ 撰写 → 自检 → 分组 commit"循环;素材**只从笔记提取**,不回 PDF、不凭记忆写数字。

**Tech Stack:** Markdown + mermaid 时间线图;`py -X utf8` 一行脚本(中文字数统计/引用覆盖检查);git(`commit -s`,签名行末行,无 Co-Authored-By)。

**Spec:** `docs/superpowers/specs/2026-09-28-sdc-academic-survey-design.md` —— §1 决策表与 §4 章节骨架为权威;诚实纪律 §3.5 全程沿用。

## Global Constraints

- **字数**: 正文(第 0–10 章,不含表格行、代码块、附录)12000–20000 中文字符;计数法见 Task 15。
- **引用格式**: 行文用 `[NN]`(01–53)指笔记编号;**每个关键数字带 `[NN]` + 节/图表号**(笔记已含,照抄不重算);白皮书(53)的主张一律标"厂商主张";文档类型(会议论文/期刊/白皮书)在首次引用处标明。
- **素材唯一来源**: `ref-paper-fullread-digest/` 笔记 + `corporate-collab.md` + `INDEX.md`。禁止回 PDF、禁止凭记忆补数字;笔记标注"原文未给出/无法确证"的,报告如实沿用(如 48 的 Meta ASPLOS'24 论文指向未决)。
- **语言**: 中文正式书面语,汇报/展示材料定位(管理层/合作方),控制技术细节密度;每章末分类学章(2–6)设**规律小结**(共识/分歧/适用条件)。
- **git**: 分支 `research/survey-fullread-20260928`;`git commit -s` 末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,无 Co-Authored-By;每组任务一 commit 并 push;commit 前占位符 grep 零命中。
- **写作顺序**: 附录 A/B 先行(为正文提供引用锚点)→ 正文 1→10 → 执行摘要最后 → 全文验收。

## 素材提取命令(各章共用;TEMP 提取文件丢失时先重建)

```bash
D=/c/Users/ubuntu/Documents/sdc/sdcshield/docs/paper/ref-paper-fullread-digest
# 分类学标注(53 篇全量;旧格式 01–29 顶层 bullet,新格式 30–53 ## 标题)
awk '/^- 分类学标注/{p=1;print "### " FILENAME; next} /^- /&&p{p=0} p' $D/0*.md $D/1*.md $D/2*.md > "$TEMP/tax-old.txt"
awk '/^## 分类学标注/{p=1;print "### " FILENAME; next} /^## /&&p{p=0} p' $D/3*.md $D/4*.md $D/5*.md > "$TEMP/tax-new.txt"
# 核心结论(新格式)
awk '/^## 核心结论/{p=1;print "### " FILENAME; next} /^## /&&p{p=0} p' $D/3*.md $D/4*.md $D/5*.md > "$TEMP/concl-new.txt"
# 关键数字表(新格式)
awk '/^## 关键数字/{p=1;print "### " FILENAME; next} /^## /&&p{p=0} p' $D/3*.md $D/4*.md $D/5*.md > "$TEMP/nums-new.txt"
# 业界观点摘录(仅工业文档与观点文章有)
grep -A6 "^## 业界观点摘录" $D/3*.md $D/4*.md $D/5*.md > "$TEMP/views-new.txt"
```

旧格式(01–29)对应段落用 `- 核心结论`/`- 关键数字`/`- 业界观点摘录` 顶层 bullet 定位,同法 awk。单篇细读直接 Read 该笔记(笔记是唯一可信载体)。

---

### Task 1: 报告骨架与写作规约落盘

**Files:**
- Create: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`

**Interfaces:**
- Produces: 报告文件骨架(spec §4 的 10 章 + 附录 A/B 标题树、引用规约注释块);后续所有任务向此文件追加/填充。

- [ ] **Step 1: 写骨架文件**

包含: 标题(`SDC 学术综述:静默数据损坏的研究版图、分类学与产业-学术生态(2003–2026)`)、日期、素材声明(基于 53 篇全文精读笔记,编号 [01]–[53] 对应 `ref-paper-fullread-digest/`)、Ch0–Ch10 与附录 A/B 的标题树(每章标题下暂只写一行"本章由 Task N 填充"——该占位在 Task 15 验收时必须清零)。

- [ ] **Step 2: 自检 + commit**

`grep -c "^## " SDC_ACADEMIC_SURVEY_CN.md` ≥ 12(10 章+2 附录)。commit `docs(survey): 综述报告骨架与写作规约`。

### Task 2: 附录 A——53 篇逐篇档案表

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(附录 A 节)

**Interfaces:**
- Consumes: 各笔记 H1/`## 标题与出处`(类型、venue)+ `corporate-collab.md`(机构/企业/证据级)+ 分类学标注(归类)+ 核心结论第 1 条(一句话结论)。
- Produces: 53 行档案表,列 = `| 编号 | 简称(venue,年份) | 类型 | 学术机构 | 合作企业(证据级) | 分类学一句话 | 核心结论一句话 |`。

- [ ] **Step 1: 逐批提取生成表行**——数据只抄笔记;分类学一句话压缩自标注四字段(根因/故障/检测/处理),不适用项略。
- [ ] **Step 2: 行数与一致性自检**——`grep -cE "^\| [0-9]{2} " = 53`;抽查 3 行(01/27/53)与笔记及 corporate-collab.md 一致。
- [ ] **Step 3: commit** `docs(survey): 附录 A——53 篇逐篇档案表`。

### Task 3: 附录 B——身份勘误与引用注意清单

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(附录 B 节)

**Interfaces:**
- Consumes: 各笔记 `身份核实`/`文本层问题` 段中已记录的勘误与更正(全部已在阶段 A 落定,无需新裁)。

- [ ] **Step 1: 汇总勘误表**——已知清单(以笔记为准,此处为索引): 01/27/05/33/47/50/53 的 361 DPPM 单位与归属终裁(3.61/万=361 DPPM,Alibaba);27 的 slug/venue 交换史(ISCA'24 原文);44 的 He ISCA'23 归属 Google(非 Meta);48 的 Table 1 三行值列漂移(坐标复核)与 Meta ASPLOS'24 论文指向未决;36/37/43 的 Didehban 引文链(gZDC TDSC vol.21 no.1 pp.78–92,语料外);52 的 47×/65× 能量归属转置与 §4.3.3 指数笔误;29 的 mojibake 修复;各篇表格文本层交错记录。表格列 = `| 编号 | 勘误/注意事项 | 依据与处置 |`。
- [ ] **Step 2: 自检**——每行含 [NN];commit `docs(survey): 附录 B——身份勘误与引用注意清单`。

### Task 4: 第 1 章 引言与研究版图

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch1)

**Interfaces:**
- Consumes: `INDEX.md`(批次/venue/类型映射)、corporate-collab.md(四类合作形态统计:纯产业 7/联合署名 19/仅资助 6/零企业 21)、笔记的类型标注。
- Produces: 53 篇全景分类表(批×主题×篇数)+ 证据强度分级说明(文档类型 × 合作证据级 1/2/3)+ 方法论谱系总览(实测 fleet/仿真注入/测试生成/运行时/白皮书五类)。

- [ ] **Step 1: 撰写**(约 1200–1500 字)——SDC 定义一句引入(静默性=无任何机制检出,01/47);语料构成(53 篇 = 8 批 R1–R8,会议论文/期刊/白皮书类型分布,时间跨度 2003–2026);证据强度分级框架;报告结构导览。
- [ ] **Step 2: 自检**(引用覆盖本章 ≥ 10 个 [NN])。暂不单独 commit(与 Task 5–7 同组)。

### Task 5: 第 2 章 SDC 核心特征

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch2)

**Interfaces:**
- Consumes: 笔记 47/48/49/50(特征总述)、10/11(SDC/DUE 分流)、08(四分)、12(false DUE)、19(correct vs strictly correct——应用固有容差)、21(六类)、50(SDC/DCE/UOE)、46(partial failure 同构)、45/20(双重静默)、51(应用级定义+SDC 先于崩溃)、48④(AI 域再定义难题)。
- Produces: 定义谱系演变表(2003 辐射原始定义→分类学细化→2021+ 产业定义→应用域再定义);静默性根源四条;边界分类学(SDC/DUE/Masked/Crash/false DUE/UOE)。

- [ ] **Step 1: 撰写**(约 1200–1500 字)——2.1 定义边界与谱系;2.2 静默性根源(检测机制未覆盖、检测机制自身用脆弱单元 01 §6.2、认识论不可测量只可估计 47、加密/主存双重静默 45/20);2.3 与 crash/masked/DUE 的边界分类学;2.4 规模必然性与架构倾向(49/50,数据并行>控制流 48)。
- [ ] **Step 2: 规律小结**(共识/分歧/适用条件——分歧焦点: 位级定义 vs 应用级定义、strictly correct 是否算 SDC)。

### Task 6: 第 3 章 根因机理分类学

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch3)

**Interfaces:**
- Consumes: 47 的三分法框架(先天逃逸/后天老化/个体涨落)+ 四条物理主线证据: 制造缺陷/逃逸线(01/02/04/09/14/18/25/26/29/32/50)、边际缺陷→SDF 线(02/09/15/31/50)、老化线(28 BTI/NBTI 唯一门级物理机理、29、50 寿命退化)、辐射线(10/11/12/13/24 束流实测、52 FPGA 配置存储器)、电压/功率线(30 上电瞬态、32 电源噪声、51 Vmin);设计 bug(25/26/32);因果链三段(物理根因→电路表现→微架构部位);部位谱(FU/RF/L1D/L1I/TLB/配置存储器)。
- Produces: 根因×部位矩阵表;因果链总图(文字版);产业证词 vs 学术重构的张力记录(32 Lerner"时序缺陷最大贡献+老化证据少" vs 28/31 的老化/SDF 重构——并列呈现不裁决)。

- [ ] **Step 1: 撰写**(约 1500–1800 字)——3.1 权威框架(47 三分法+辐射第四主线);3.2 各主线证据与量化(每条带 [NN]+节号);3.3 因果链:电路表现形态(stuck-at/SDF/SEU/timing violation);3.4 微架构部位谱与结构×根因倾向;3.5 张力与未决(32 vs 28/31;08 的"瞬态统一建模"方法论立场)。
- [ ] **Step 2: 规律小结**——共识: 制造缺陷为主因、多因素调制;分歧: 老化贡献度、瞬态 vs 永久建模口径;适用条件: fleet 线(制造缺陷)vs AVF 线(辐射)的方法论分野。

### Task 7: 第 4 章 故障模式分类学

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch4)

**Interfaces:**
- Consumes: 八个模式维度证据——①位分布(01 尾数主导 vs 05 指数/符号位高幅 10240× vs 08 低位字节——**跨 fleet 分歧,如实并列**);②单/多位(02 多比特主导 vs 24 >95% 单比特——分歧并记录口径差异);③值域形态(46 swap_similar=合法错值最难检、05 错误偏移读取 76%);④一致性型(01 cache/事务内存、06 不一致错误);⑤频率谱(01 每分钟数百次 vs 05 <10⁻⁵ 低频隐藏 vs 间歇自愈 02);⑥触发条件(温度 01/05、电压/任务态 droop 50/51、输入依赖 03/06/43、时间退化);⑦结构×模式规律五篇三级证据链(19/21/22/23 仿真→24 束流实测:L1D→SDC 主导、L1I→Crash、TLB→DUE、PRF→低 SDC;29 扩展到 DSA:索引型→Crash/数据型→SDC/覆写掩蔽);⑧特殊模式(ESC 直接输出损坏 17/08、被覆写 SDC 38/39、放大机制 45 FHE 10³⁵–10¹²⁵/07 OS 放大/44 inf-nan 爆炸、控制面污染 38 调度层)。
- Produces: 模式维度×代表论文×关键数字表;结构×模式规律汇总表(三级证据链标注)。

- [ ] **Step 1: 撰写**(约 1500–1800 字)——按八维度展开,每个维度给代表数字(带 [NN]+图表号)。
- [ ] **Step 2: 规律小结**——共识: 结构×模式规律五源一致、触发条件多因素;分歧: 位分布(尾数 vs 指数)与单/多位占比(**口径差: fleet 深测 vs 束流实验 vs 仿真注入**);适用条件: 各证据形态的外推边界。
- [ ] **Step 3: 对象层四字(Ch1–4 连 Task 4–7)一组 commit** `docs(survey): 对象层——引言/核心特征/根因/故障模式四章` 并 push。

### Task 8: 第 5 章 检测技术分类学(全文最重章)

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch5)

**Interfaces:**
- Consumes: 按"原理族×栈层×生命周期"组织的三族证据——
  **A 设计期评估族**(不产出检测器,产出脆弱性地图): A1 解析建模(10/11/12 ACE-AVF、13 一阶、14 H-AVF、15 DelayAVF);A2 统计故障注入 SFI(16 Merlin、17 vuln-stack、19 GeFIN、20 Chaos、21 diff-Fi、22 gem5-MARVEL、23 cross-ISA);A3 开发期 ML 预测(36 SHOUT-Trainer、37 KG-vulnpred)。
  **B 激励式测试族**(制造/in-field/fleet): B1 fuzzing 代理(25 SiliFuzz);B2 硬件在环迭代生成(26/27 Harpocrates——30× SiliFuzz 有效指令率);B3 物理机理+形式验证(28 Vega 老化感知);B4 库移植(OpenDCDiag 谱系: 01 交叉验证、26/27 基线、50 产业自述);B5 RL 参数优化(30 IRPS25,与 B1–B4 正交);B6 DFT/ATPG 鲁棒化(31 SDF-pattern);B7 制造全栈扫描+SLT(32 七级故障模型/电压裕量>频率>温度、50 path-delay/ATE);B8 fleet 周期扫描部署形态(01 Farron 三级优先、02 PinDrop 持续测试、03 Ripple 在产微测试/影子测试、25 SiliFuzz 产线筛查)。
  **C 运行时在线族**: C1 软件层选择性冗余(06 ITHICA、33 Orthrus 混合式、41 ParallaFT 二进制级、43 编译器 IR 级输入感知);C2 硬件微架构冗余(42 ParaVer;42 明确 BIST vs 冗余执行分类学——与 B8 互补);C3 运行时系统复制(40 AMT 孪生);C4 硬件计数器+ML(34 决策树/35 七分类器,注入端验证 20);C5 数据系统层(38 Sentry 事务依赖图、39 Exabyte 校验和体系/语义不变式、46 元数据 verify-on-access);C6 应用算法级 ABFT(05 in-application、45 NTT 线性不变量、**44 否定性结论: 高 BER 不适用**)与遥测关联(07);C7 片上传感+预测性维护(50 产业框架、53 RTHM 厂商主张)。
- Produces: 检测技术全景表(族×原理×栈层×生命周期阶段×代表论文×开销/检出关键数字);路线对照叙事(fuzzing vs 在环 vs 物理机理;BIST vs 冗余执行;侵入性谱系: 硬件 42 > 源码 43 > 二进制 41 > 零修改 34/35)。

- [ ] **Step 1: 撰写**(约 2000–2500 字)——5.1 三族总览与"原理×栈层×生命周期"三维定位法;5.2 A 族;5.3 B 族(七条生成路线谱系+部署形态);5.4 C 族;5.5 横向规律(BIST/冗余执行互补 42、覆盖≠检测的软件掩蔽 27、ABFT 适用边界 05/44/45)。
- [ ] **Step 2: 规律小结**——共识: 检测左移+全生命周期连续化、单一技术无银弹;分歧: 侵入性 vs 覆盖的权衡、ML 方法成熟度;适用条件: 每族给一句"何时选用"。

### Task 9: 第 6 章 处理技术分类学

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch6)

**Interfaces:**
- Consumes: 47 权威框架(更换=唯一负责任动作/降级运行/冗余容错昂贵/SDC 率定价)+ 六组证据——①退役/更换梯度(01 细粒度核级退役 Farron、39 机器弹出+惯犯流程、02 部件更换);②降级运行(50 坏核禁用带病运行+电压裕量动态调整、51 Vmin 三电压区受控降级=能效视角、53 调压调频补偿[厂商主张]、52 lockstep 降级+恢复窗口 2× 定量);③在线修复(38 Sentry 精准回滚仅受影响事务、40 选择性重算三选二表决、45 DMR 重执行);④调度规避(39 核级调度屏蔽、01 workload backoff 0.864 s/h、02 预约扣留→深测→回归);⑤容忍不检测(44 激活裁剪/SBP 应用层容忍、51 Unsafe 区 ML 容错利用);⑥结构性容错(52 TMR 4.5× LUT/清洗频率按失败概率优化省 50% 功耗、40/41/42/43 复制谱系——与 Ch5 C 族的技术重叠须交叉引用不重复展开);⑦经济学处理(47 SDC 率定价、48 成本归属"以上皆是")。
- Produces: 处理技术分类表(类×代表×开销×适用条件)。

- [ ] **Step 1: 撰写**(约 1200–1500 字)——按 47 框架展开六组;检测触发处置链(隔离池 03/06/25、fail-fast 46、检出即停 33/41)作为"处理"的入口分类。
- [ ] **Step 2: 规律小结**。
- [ ] **Step 3: 对策层两章(Ch5–6)一组 commit** `docs(survey): 对策层——检测/处理技术分类学两章` 并 push。

### Task 10: 第 7 章 学术界 vs 产业界:各维度规律与观点对比

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch7)

**Interfaces:**
- Consumes: 47(两界纲领: 成本与付费方贯穿性)、48(制度化全景: 9 企业专刊+资助年表+媒介谱系 arXiv→顶会→X 帖)、49(学界方法学×Meta 一手经验联合文本)、50(三厂商第一人称自述 vs 学术组织)、51(无厂商独立测量——与 50 构成互补对)、53(白皮书话语: 软错误/SDC 混同、借权威结构)、32(产业证词)、`views-new.txt`(03/07/09/25/30/39/53 业界观点摘录)。
- Produces: 两界对比表(维度: 问题定义权/证据形态/时间线[产业先知 2–3 年 48]/方法论分工/话语口径/资助与数据流/张力点);张力点专节(Lerner 证词 vs 学术重构、50 厂商自述 vs 51 独立测量、Meta 应用级容忍主张 32 vs 检测优先路线、53 营销话语 vs 同行评审)。

- [ ] **Step 1: 撰写**(约 1500–2000 字)。

### Task 11: 第 8 章 演进趋势(2003–2026)

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch8)

**Interfaces:**
- Consumes: 时间线锚点——2003 AVF/辐射范式(10/11)→2006 硬故障泛化(14)→2012 一阶解析(13)→2013–2017 SFI 工具成熟(16–23)→2021 产业披露潮(Meta arXiv 02/01 引 Dixit、Google HotOS)→2022 Meta RFP 5 校(48 Table 2)→2023 SOSP23 Alibaba 01/ISCA He/AMD 合作→2024 检测生成爆发(25–31)+运行时检测(33–39)+OCP 6 校→2025 运行时容错收口+AI 域(40–44)+DeepSeek/Amazon/Tesla 披露(48)→2026 FHE/存储/专刊/白皮书(45–53)。
- Produces: mermaid timeline 图(语法必须渲染校验);三条演进轴叙事: 方法论(解析建模→SFI→硬件在环→AI/ML 30/36/37)、规模(单核仿真→百万级 fleet 01)、栈层(微架构→软件→AI/加密/存储上移 44/45/46);受众媒介轴(arXiv→顶会→社交媒体 48);"从神话到 1/1000"叙事线(47)。

- [ ] **Step 1: 撰写**(约 1200–1500 字,含 mermaid timeline 代码块)。
- [ ] **Step 2: mermaid 语法自检**(节点 ID 无中文/无括号,年份用引号文本)。

### Task 12: 第 9 章 产业-学术合作生态

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch9)

**Interfaces:**
- Consumes: `corporate-collab.md` 全表(53 行)+ 统计速览(四类: 纯产业一手 7 篇 03/07/09/25/30/39/53、联合署名 19、仅资助 6、零企业 21;高频节点: Athens Gizopoulos 组 17 篇、Meta 14 篇、Intel/AMD;资助演化 2022-06 Meta RFP 5 校→2023-10 AMD 2 校→2024-06 OCP 6 校;陷阱注记: 45 华为 Li 人名、32 双 Gurumurthi、42 Arm 单向依赖、48 专刊枢纽间接聚合)。
- Produces: 企业×论文矩阵表(行=企业,列=证据级 1/2/3+间接层计数);逐企业叙事(Meta/Google/Intel/AMD/Arm/IBM/NVIDIA/Alibaba/proteanTecs);合作模式分类(署名/资助/数据/评测对象/生态机制 OCP);耦合动因(数据访问、工程规模、发表渠道、合法性互赋)。

- [ ] **Step 1: 撰写**(约 1000–1500 字 + 矩阵表)。

### Task 13: 第 10 章 系统性启示:全软硬件栈 × 全生命周期

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch10)

**Interfaces:**
- Consumes: 全部笔记的启示面;生命周期轴锚点 48(五阶段: design/manufacturing/testing/deployment/operation,53 篇中最完整)+ 50(Arm 全寿命监控产业表述)。
- Produces: 栈×周期矩阵(栈: 器件物理→电路→微架构→指令/ISA→运行时→数据系统→AI 应用 × 周期: 设计→制造测试→burn-in→部署→运行→老化),每格 1–3 条启示带 [NN];五条系统性横贯规律(检测左移/数据闭环/经济学约束/定义随域演化/无银弹组合防御);SDCShield 定位一小节(仅作落点之一: B4 库移植线+B8 fleet 扫描部署形态,OpenDCDiag 谱系 01 §2.3/26/27/50,不展开实现)。

- [ ] **Step 1: 撰写**(约 1000–1300 字 + 矩阵)。
- [ ] **Step 2: 两界层四章(Ch7–10)一组 commit** `docs(survey): 两界层——对比/趋势/生态/启示四章` 并 push。

### Task 14: 第 0 章执行摘要(最后写)

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(Ch0)

- [ ] **Step 1: 撰写**(600–800 字)——四段式: 问题是什么(1/1000 量级+静默性)、学界知道了什么(根因四主线/结构×模式规律/检测三族)、产业在做什么(披露-资助-专刊-白皮书)、怎么办(全栈×全生命周期组合防御);仅用正文已立结论,不引入新数字。

### Task 15: 全文验收自检、文档同步与推送

**Files:**
- Modify: `docs/paper/SDC_ACADEMIC_SURVEY_CN.md`(全文)
- Modify: `README.md`(文档地图行)、`CLAUDE.md`(Documentation map 一行——大颗粒度文档新增的同步义务)

**Interfaces:**
- Consumes: spec §4 骨架与 §1 决策表;Task 1 骨架占位清单。
- Produces: 验收通过的最终报告 + 推送后的 feature 分支。

- [ ] **Step 1: 字数统计**——`py -X utf8` 统计正文(剔除 `|` 表格行、``` 代码块、附录 A 起之后)中文字符数;期望 12000–20000。
- [ ] **Step 2: 引用覆盖**——01–53 每个 [NN] 在全文(含附录 A)出现 ≥1 次(循环 grep);期望 53/53。
- [ ] **Step 3: 占位符清零**——`grep -nE "TBD|TODO|【待|待补|占位|本章由 Task" SDC_ACADEMIC_SURVEY_CN.md` 无输出。
- [ ] **Step 4: 诚实性抽查**——随机抽 10 个关键数字回查笔记(编号+节号+数值一致);白皮书主张均有"厂商主张"标注;未决事项(48 Meta ASPLOS'04 指向等)如实保留。
- [ ] **Step 5: mermaid 渲染自检**(时间线语法)。
- [ ] **Step 6: 文档同步**——README.md 与 CLAUDE.md 文档地图加入报告条目(一行,格式仿 SDC_RESEARCH_SYNTHESIS_CN.md 条目)。
- [ ] **Step 7: 最终 commit** `docs(survey): SDC 学术综述报告完成——执行摘要+验收自检+文档同步` 并 push(含 Task 14)。
- [ ] **Step 8: 对照 spec §1/§4 报告验收结果**(字数/章构/引用覆盖/诚实纪律四项),请用户审阅报告。

---

## Self-Review 记录

- **Spec 覆盖**: §4 章节树 10 章+2 附录 → Task 1–14 全对应;§1 汇报受众与白皮书标注 → Global Constraints;§3.5 诚实纪律 → Global Constraints + Task 15 Step 4。
- **占位符扫描**: 各任务内容大纲均含具体分类族与论文编号(读后填实,非 TBD);唯一占位(骨架"本章由 Task N 填充")在 Task 15 Step 3 机械清零。
- **类型一致性**: [NN] 引用格式、附录 A 列名、commit 消息前缀 `docs(survey):` 全文一致;数字一律"[NN]+节号"。
