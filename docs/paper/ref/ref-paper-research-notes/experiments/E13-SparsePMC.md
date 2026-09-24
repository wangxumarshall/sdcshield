# Detecting Silent Data Corruption in Sparse Matrices using Hardware Performance Counters(PACT'25 poster/short,UML)

> 来源 PDF: docs/paper/ref/Detecting Silent Data Corruption in Sparse Matrices using
> Hardware Performance Counter.pdf(3 页,Choi & Arias & Son)。
> 作者:Minseop Choi, Orlando Arias, Seung Woo Son(University of Massachusetts Lowell)。
> 注:这是一篇**简短论文/poster 论文**(3 页),信息密度低;其前身为
> CLUSTER 2025 论文 "Detecting Silent Data Corruption From Hardware Counters"(文中引用 [4])。
> 所有数值均标注原文图表号;读不到的写"原文未给出"。

## 1. 研究问题与核心贡献(≤5 行)

- 问题:HPC 反复调用 SpMV(稀疏矩阵-向量乘),SDC 在迭代求解器中悄悄传播放大;
  传统 ABFT 检测/纠正开销大且需按算法定制集成。
- 问题 2(与前作 [4] 的差异):前作只观察**单个**注入错误的传播;本文研究
  **多错误注入真实数据**时 PMC 模式能否检测。
- 方法:对 4 个 SuiteSparse 真实矩阵注入高斯噪声(错误幅度 × 注入率网格),
  Linux perf 收集硬件性能计数器,训练决策树分类器区分"污染 run"与"干净 run"。
- 结果:平均检测准确率约 90%(最高 95%),运行时开销 <2%,部署阶段只需 8 个 PMC。
- 本质:**数据级(注入)SDC 的检测,不是 CPU 缺陷检测**——错误是软件层注入到
  矩阵数据里的,与 CPU 硅缺陷无关。

## 2. 实验方法(插桩/检测机制、输入、负载)

### 2.1 错误传播观察(§2.1、§3、Fig. 1)

- 矩阵用 **CRS(Compressed Row Storage)** 格式存储;CRS 任一数组
  (values/col_index/row_ptr)损坏都会改变访问模式与结果。
- **注入错误位置相对非零元素的邻近度**直接决定错误在反复 SpMV 中的传播速率与范围
  (§2.1,承接前作 [4])。
- Fig. 1:400×400、稀疏度 0.9975 的矩阵中注入单个错误后,受污染条目数随
  SpMV 迭代的传播模式;不同初始错误位置(种子号标注,如 836086723)产生
  不同的传播曲线。

### 2.2 PMC 检测框架(§3、Fig. 2 "Workflow: training/deployment phases")

- 现代 CPU 通过 PMU 提供 PME(performance monitoring events),
  典型提供 **4–8 个 PMC** 硬件计数器同时记录(§3,引 Intel perfmon 与 Yasin 顶层法)。
- 工作流两阶段:
  - **训练阶段(红箭头)**:用 **perf list 列出的全部 5,670 个 PME** 收集计数器值
    作为特征,用决策树筛选最重要的计数器;
  - **部署阶段(蓝箭头)**:只监控少数关键 PMC(实际 8 个)做高效检测。

### 2.3 错误注入(§5 步骤 1)

- 复用 **ADSP(Anomaly Detection using Sparsity Profile)框架的错误注入模块**[8]。
- 两个参数的网格:
  - **错误幅度(error magnitude)e ∈ [0.1, 1.0],步长 0.1**(10 档);
  - **注入率(injection rate)i ∈ [0.01, 0.10],步长 0.01**(10 档);
  - 10×10 组合 → **100 个错误注入矩阵**。
- 扰动方式:对选中条目加**高斯噪声 N(μ, σ)**,**均值 μ = 原值**
  (即噪声中心在原值,σ 的取值规则原文未明确给出——只说 "standard deviation σ
  to simulate SDC";推测 σ 由 e 缩放,但公式原文未给出)。

### 2.4 计算与采集(§4、§5)

- 每个数据集:对 100 个错误注入矩阵各跑 **1 次 SpMV**,对无错条件也采集 **100 次**
  (保证变异性与稳健性)。
- PMC 用 **Linux perf** 工具记录。
- 分类:**决策树**,100 个标注训练对(event data, label: Error / Non-Error);
  **70/30 训练/验证划分;随机过采样(random oversampling)平衡类别**。

## 3. 实验配置(硬件/软件/参数)

| 项 | 值(原文 §4) |
|---|---|
| 平台 | CloudLab 节点 |
| CPU | **2× Intel Xeon Silver 4114(每颗 10 核,2.20 GHz)** |
| 内存 | 192 GiB ECC DDR4-2666 |
| OS | Ubuntu 22.04.5 LTS |
| 内核 | Linux 5.15.0-122 |
| 工具 | Linux perf(采集);决策树(分类,实现库原文未给出) |
| 数据集 | SuiteSparse Matrix Collection 4 个真实矩阵:494_bus、662_bus、can_62、bcspwr03 |
| 矩阵规模 | 原文未给出(只给名字;494_bus 约 494×494、662_bus 约 662×662,
  can_62=62×62、bcspwr03=118×118——这是 SuiteSparse 公开常识,论文正文未印规模) |
| 传播实验矩阵 | 400×400、sparsity 0.9975(Fig. 1) |
| PME 总数 | 5,670(perf list) |
| 部署 PMC 数 | 8 |
| 训练/验证 | 70/30,随机过采样 |
| 负载 | 单次 SpMV(非迭代;迭代只用于 Fig. 1 的传播观察) |

## 4. 实验步骤(可操作流程,编号)

1. 取 4 个 SuiteSparse 矩阵(494_bus、662_bus、can_62、bcspwr03),CRS 格式。
2. (前置观察)在 400×400、sparsity 0.9975 合成矩阵上单错误注入,
   反复 SpMV,记录污染条目数随迭代变化(Fig. 1)。
3. 生成 100 个错误注入矩阵:e ∈ {0.1..1.0} × i ∈ {0.01..0.10},
   选中条目加 N(μ=原值, σ) 高斯噪声(ADSP 注入模块)。
4. 每数据集:100 个注入矩阵各跑 1 次 SpMV;干净矩阵跑 100 次;
   每次运行用 perf 记录 PMC 值。
5. 训练阶段:全量 5,670 个 PME 做特征,决策树训练(100 对,
   label = Error/Non-Error),70/30 划分 + 随机过采样;
   从树中提取特征重要性,选出 8 个关键 PMC。
6. 部署阶段:只用这 8 个 PMC 重跑分类,报告 Accuracy/Precision/Recall/F1(Table 1)。
7. (计划中的未来工作,未完成)#pragma 指令插桩 HPC 负载做细粒度 PMC 采集,
   集中节点广播分类——实用实时系统的方向。

## 5. 实验数据(关键数值 + 图表号)

### 5.1 检测性能(原文 Table 1,四个数据集)

| Dataset | Accuracy | Precision | Recall | F1 |
|---|---|---|---|---|
| 494_bus.mtx | 0.919 | 0.919 | 0.924 | 0.920 |
| 662_bus.mtx | **0.943** | 0.950 | 0.938 | 0.943 |
| can_62.mtx | 0.811 | 0.811 | 0.822 | 0.812 |
| bcspwr03.mtx | 0.892 | 0.903 | 0.882 | 0.890 |

- 摘要口径:"average detection accuracy near 90% (up to 95%)"(95% 应指 662_bus 的
  0.943≈0.95 或 precision 0.950;准确率四值平均 = 0.891)。
- 开销:**<2% 运行时开销**,部署阶段只用 **8 个选定 PMC**(§6.1)。
- **关键 PMC 是哪些:原文未给出**——只说 "the key features varied by dataset"
  (每个数据集的关键特征不同)。
- Fig. 2:工作流图(训练/部署两阶段)。
- Fig. 1:错误传播图(不同初始错误位置的传播曲线;纵轴/横轴刻度在位图中,
  文本层未给出)。

### 5.2 传播观察(Fig. 1,定性)

- 单错误注入位置对传播速率/范围有直接影响;
  靠近非零元素 → 传播快而广(§2.1 CRS 邻近度论断)。

## 6. 实验结论(编号列出)

1. PMC 数据可以高准确率(0.811–0.943)、低开销(<2%)区分错误注入与无错的 SpMV run
   (§6.1、摘要)。
2. 决策树 + 特征筛选后,部署只需 8 个 PMC(§6.1)。
3. **每个 PME 的权重随数据集变化**(§7:"the weights of each PME varies by dataset"),
   需要进一步精化。
4. 单错误注入位置决定传播模式,靠近非零元素传播更强(§2.1、Fig. 1)。
5. 未来工作(未做):稳定 PME 跨数据集的重要性(消除时间漂移、剔除无关 PMC、
   验证小子集的可复现性);#pragma 插桩细粒度采集 + 中心节点实时分类(§7)。

## 7. 复现要点(ARM64 复现最小版本)

### 7.1 可直接复用

- **整体流程**:矩阵数据注入(高斯噪声)→ perf 采集 PMC → 决策树二分类 →
  特征筛选 → 小特征集部署。全流程 ISA 无关。
- **数据集**:SuiteSparse Matrix Collection 全开放,494_bus/662_bus/can_62/bcspwr03
  可直接下载(.mtx)。
- **注入参数网格**:e∈[0.1,1.0]×i∈[0.01,0.10] 共 100 组,70/30 划分 + 随机过采样,
  100 干净 run——可直接照抄。
- **perf 采集**:Linux perf 在 ARM64 可用;`perf list` 在 Kunpeng 920
  (ARMv8.2 Cortex-A72 衍生)上列出 PMU 事件(armv8_pmuv3 事件组:
  cycles、instructions、L1D cache refill、mem_access、stalled cycles 等)。
  ARM64 事件数远少于 x86 的 5,670(通常数十个),筛选空间自动变小。
- **决策树**:sklearn DecisionTreeClassifier 即可,论文未指定实现。
- **开销测量**:<2% 的验证方式(带 perf 采样 vs 不带的运行时间比)可直接照搬。
- ADSP 框架的错误注入思想(对稀疏数据的选择性扰动)可自行实现(几十行 Python/C)。

#### 7.1.1 ARM64 最小复现工程(SDCShield 侧具体化,自有方案)

以下为结合 SDCShield 仓库现状设计的最小复现工程(标注为自有方案,非论文内容):

1. **负载宿主**:新建 `tests/cpu/spmv_pmc` 或复用现有 pocketfft/openblas 测试骨架;
   SpMV 内核用 CSR 三数组循环即可(百行 C++),矩阵读 .mtx(Harwell-Boeing 格式,
   解析器约 50 行)。
2. **数据准备脚本**(Python,repo 外工具链):
   - 下载 4 个矩阵(与原文同款,可加 2 个 Kunpen 上更大规模的矩阵做泛化);
   - 注入脚本:对 values[] 数组按注入率 i 抽样条目,
     `value' = value + N(0, σ)`,σ = e × mean(|values|)(填补原文 σ 空白,需在文中注明);
   - 生成 100 个注入变体 + 保存干净副本与 MD5。
3. **PMC 采集**:两种模式——
   - 模式 A(perf stat 包裹):`perf stat -e <event_list> -x, ./sdcshield -e spmv_pmc ...`,
     输出 CSV 化计数;
   - 模式 B(perf_event_open 内嵌):测试自采,SDCShield 已有 per-CPU worker 结构,
     可在 `test_init` 里 `perf_event_open` 一组事件、`test_cleanup` 读出;
     模式 B 更接近论文"部署阶段"形态,开销可控在计数器读取消耗。
4. **ARM64 事件初筛清单**(Kunpeng 920 暴露的 armv8_pmuv3 典型事件,
   以 `perf list` 实际输出为准):
   - 周期/指令类:`cycles`, `instructions`, `branches`, `branch-misses`;
   - 访存类:`L1-dcache-load-misses`(armv8 ref: L1D_CACHE_REFILL)、
     `L1-dcache-loads`(L1D_CACHE)、`LLC-load-misses`、`mem_access`、
     `mem_access_rd/wr`(IMPLEMENTATION DEFINED,如可用);
   - 数据旁路类:`ld_spec`, `st_spec`(speculative load/store)、`dp_spec`
     (数据处理指令 speculative——这是"值扰动改变指令行为"最可能的信号源);
   - 总线/未对齐:`unaligned_ldrs_retired`(bus_access 相关,若暴露);
   - 停顿类:`stalled-cycles-frontend/backend`(若 PMU 映射)。
5. **分类与评估**:sklearn DecisionTree(与原文同款模型,保证可比)+
   附加 RandomForest/Logistic 做稳健性对照(超出原文范围,标注为扩展);
   同样 70/30 + 随机过采样;报告 Accuracy/Precision/Recall/F1 与原文 Table 1 对齐。
6. **对照组(超越原文的关键设计)**:
   - 干净/注入 run **交替穿插执行**(消除时间漂移伪相关——原文自己承认的威胁);
   - 固定 CPU 亲和(`taskset`/SDCShield `-n 1`)排除迁移噪声;
   - 记录 run 时间戳,事后回归检验特征 vs 时间的相关性(漂移审计)。
7. **真正对标 SDCShield 使命的扩展实验(论文没做的)**:
   在**已知故障机**(如 NUMA3 VA-path 故障机)上跑干净矩阵 SpMV,
   检验 PMC 能否把故障机与健康机分开——这是"PMC 检测硬件缺陷"的首次直接实验,
   也是 SDCShield 新实验设计相对该论文的核心增量论证点。

### 7.2 需替代/不可行

- **x86 的 5,670 个 PME 筛选**:ARM64 PMU 事件空间完全不同(Kunpeng 920 暴露
  armv8_pmuv3_0 的标准事件 + 少量 IMPLEMENTATION DEFINED 事件)。
  "5,670 选 8" 在 ARM64 上变成"约 50–100 选 8",筛选意义减弱,
  但过拟合风险也降低。需用 `perf list` + `armv8-pmuv3` 事件重做。
- **NIC 级别的细粒度采集(#pragma 计划)**:论文自己都还没做,不存在复现问题。
- **"检测 CPU 缺陷"的语义不可复现**:本文注入的是**数据错误**,
  不是硬件缺陷产生的错误。若 SDCShield 想复现"PMC 检测"路线,
  必须重新定义错误源(如:用故障机跑 SpMV 看 PMC 是否可分辨缺陷机 vs 健康机——
  这是论文**没做**的实验,但正是 SDCShield 能做的增量)。
- **σ 与 e 的关系**:原文未给出公式,复现需自行定义(如 σ = e×|原值| 或
  σ = e×矩阵值域),并说明这是对原文空白的填补。
- **ECC 干扰**:平台是 ECC 内存;注入的是数据值错误,ECC 不影响;
  但若在 SDCShield 场景下检测硬件缺陷,ECC 会纠正部分传输错误,
  PMC 可见性需重新论证。
- **多线程 SpMV**:原文未说明是否并行(2×10 核平台只用了单进程?原文未给出);
  SDCShield 复现时应明确定线程数(per 计数器归因于核,perf 默认 per-thread/per-cpu
  聚合方式会影响特征)。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield 而言,这篇论文的价值是**"PMC 侧信道检测 SDC"路线的最弱基线**:

| 对比轴 | 论文基线值 | SDCShield 可比实验 |
|---|---|---|
| 检测准确率(数据注入 SDC) | 0.811–0.943(Table 1) | 新实验若做 PMC 路线,需超过 0.94;若做 golden-value 路线,理论 100% |
| 检测开销 | <2%(§6.1) | PMC 采样 vs SDCShield 全量计算+比对的吞吐代价对比 |
| 部署特征数 | 8 个 PMC(§6.1) | 需要的遥测通道数 |
| 错误灵敏度 | 原文未给出(无按 e/i 档位的检出率分解) | 新实验可给出"最小可检错误幅度 × 注入率"曲线,直接超越 |
| 检测延迟 | 原文未给出(需跑完一次 SpMV 后离线分类) | SDCShield 在线逐轮检测 |
| 泛化性 | 关键特征随数据集漂移(§7) | 新实验的跨数据集/跨矩阵稳定性 |
| 硬件缺陷相关性 | 无(数据注入,非硬件缺陷) | SDCShield 直接测硬件:根本性差异轴 |
| 训练需求 | 100 干净 run + 100 注入 run,决策树,70/30 | 无监督 vs 有监督的工程成本对比 |
| 与 ABFT 对比 | 原文未给出(只在 §1 定性说 ABFT 开销大、需算法定制) | SDCShield golden-value 与 ABFT 校验的定量对比需自建 |

## 9. 局限与坑(详细版)

0. **体例注意**:3 页 poster/short paper,数据点总量极少
   (Table 1 一张 4 行表 + 两个定性图);任何"复现"都必须以自建实验为主体,
   原文只能提供方法骨架与四个准确率锚点。

1. **最根本:不是硬件缺陷检测**。注入的是应用层数据错误;论题是"SDC 的检测",
   但其实验模型里 CPU 是健康的。对 SDCShield(检测硅缺陷)只能当方法论参考,
   不能当缺陷检测基线引用。
2. **3 页短文,实验极薄**:
   - 无按 e(错误幅度)/i(注入率)档位的检出率分解——最小可检错误量不明;
   - 无混淆矩阵/ROC,只有 Accuracy/Precision/Recall/F1 四个聚合数;
   - 无 8 个关键 PMC 的清单("key features varied by dataset" 一句带过);
   - 无与 ABFT 的定量对比(只在引言定性贬低);
   - 无检测延迟数据;
   - σ 的定义缺失(§5 "standard deviation σ" 后无公式);
   - 矩阵规模、SpMV 是否并行、迭代次数(除 Fig. 1 外)均未给出。
3. **时间漂移被自己承认是威胁**(§7 future work: "minimizing time drift when
   collecting PMC values for error and error-free runs")——干净 run 与注入 run 的
   PMC 采集若不同时,时间漂移可能就是分类依据(伪相关)。这是该方法最大的
   方法论漏洞:决策树可能学到"时间/频率漂移"而非"错误信号"。
4. **随机过采样 + 100 对小样本**:每数据集只有 200 个样本(100 错 + 100 净),
   70/30 划分后测试集仅 60 个;0.94 准确率的置信区间宽。
5. **泛化性失败被明示**:关键特征跨数据集不迁移(can_62 只有 0.811),
   论文把"稳定特征选择"留给未来工作——意味着该路线尚未达到可部署状态。
6. **单次 SpMV 的检测语义**:错误注入在矩阵里,跑一次 SpMV 后分类——
   这检测的是"输入数据被污染"而非"计算过程出错";若错误在计算中产生
   (真正的 SDC),分类器是否同样可分,原文未验证。
7. **ECC 平台**:192 GiB ECC DDR4;数据注入不受影响,但结论不能外推到
   "PMC 能检测 ECC 纠正不了的硬件 SDC"——论文没做这个实验。
8. **前作重叠**:错误传播部分(Fig. 1、§2.1)直接引用 CLUSTER'25 前作 [4],
   本文增量只有"多错误注入 + 决策树分类"这一小块;
   引用时应区分两篇的工作边界。
9. **perf 采集 5,670 事件的可行性**:perf 单次只能开 4–8 个 PMC(论文自己 §3 说明),
   5,670 事件特征必然是多次重复运行拼接的——又回到时间漂移威胁(见第 3 条);
   论文未说明如何控制 run 间差异。
10. **对 SDCShield 的借鉴边界**:可借鉴的只有"perf PMC + 轻量分类器"这一
    检测形态(且它适合做 SDCShield 的**伴随遥测**而非主检测);
    其数值(0.9 准确率、8 PMC、2% 开销)可作为"轻量但不可靠一档"的锚点,
    反衬 golden-value 全量比对的正确性优势。

## 10. 与 SDCShield 新实验设计的对接建议(自有分析,非论文内容)

以下为服务于 SDCShield SDC 激发实验设计的对接分析:

1. **定位**:该论文在对比矩阵中应放在"被动/伴随检测"象限——
   与 SDCShield 的主动激发+golden 比对是互补关系而非竞争关系。
   建议的对比叙事:SDCShield 主检测 100% 正确(golden memcmp 语义)+
   PMC 伴随通道提供"缺陷激发期间的微架构侧写"(辅助定位与归因)。
2. **可声称的优势轴**(相对该论文):
   - 错误源真实性:SDCShield 激发真实硬件缺陷错误 vs 论文软件注入数据错误;
   - 检测正确性:golden 比对无假阳性 vs 论文 0.811–0.943 且特征漂移;
   - 检测粒度:SDCShield 逐轮逐核 vs 论文整 run 离线分类;
   - 可解释性:SDCShield 失败即错误位型 vs 论文黑盒决策树。
3. **需要诚实承认的劣势轴**:
   - 开销:PMC 采样 <2% vs SDCShield 全量计算+比对的吞吐代价;
   - 侵入性:PMC 对负载零修改 vs SDCShield 需要测试程序在场。
   若新实验能把 PMC 通道以 <2% 开销内嵌为 SDCShield 的伴随输出,
   则两条轴同时占据。
4. **复现优先级**:低。三篇中最后复现(或只做 7.1.1 的第 6/7 步对照实验);
   其价值主要是给论文写作提供"相关工作—被动检测"一段的定量锚点。
5. **引用注意**:引用其 0.9 准确率时必须同时引用其两个限定:
   (a) 数据注入而非硬件缺陷;(b) 关键特征随数据集漂移(§7)。
   否则会夸大该路线的成熟度。
6. **延伸文献线索**:本文的实质前身是同一团队的
   CLUSTER 2025 论文 "Detecting Silent Data Corruption From Hardware Counters"[4]
   与 AI4Sys'23 的 ADSP 框架[8];若需要"PMC 检测 SDC"更完整的相关工作链,
   应追这两篇而非本 3 页短文。

## 附录:原文可提取的全部定量数据清单(便于引用核对)

| 数据 | 值 | 出处 |
|---|---|---|
| 检测准确率(四矩阵) | 0.919 / 0.943 / 0.811 / 0.892 | Table 1 |
| 精确率 | 0.919 / 0.950 / 0.811 / 0.903 | Table 1 |
| 召回率 | 0.924 / 0.938 / 0.822 / 0.882 | Table 1 |
| F1 | 0.920 / 0.943 / 0.812 / 0.890 | Table 1 |
| 平均准确率 | ~90%(摘要 near 90%, up to 95%) | Abstract |
| 运行时开销 | <2% | §6.1 / Abstract |
| 部署 PMC 数 | 8 | §6.1 |
| PME 总数 | 5,670(perf list) | §6.1 |
| PMC 硬件计数器典型数 | 4–8 | §3 |
| 错误幅度范围 | 0.1–1.0(步长 0.1) | §5(1) |
| 注入率范围 | 0.01–0.10(步长 0.01) | §5(1) |
| 注入矩阵数 | 100 | §5(1) |
| 干净采集次数 | 100/数据集 | §4 |
| 训练对数 | 100 | §5(4) |
| 划分/平衡 | 70/30 + 随机过采样 | §5(4) |
| 传播实验矩阵 | 400×400,sparsity 0.9975 | §3/Fig. 1 |
| 平台 CPU | 2× Xeon Silver 4114(10C,2.20GHz) | §4 |
| 内存 | 192 GiB ECC DDR4-2666 | §4 |
| OS/内核 | Ubuntu 22.04.5 / 5.15.0-122 | §4 |
| 关键 PMC 清单 | 原文未给出 | — |
| 检测延迟 | 原文未给出 | — |
| 与 ABFT 定量对比 | 原文未给出 | — |
