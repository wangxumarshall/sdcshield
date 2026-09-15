# SDC（静默数据损坏）前沿研究系统性总结

> 整理日期：2026-09-15
> 文献基础：本目录 `ref/` 下 31 篇论文（2019–2026，含 SOSP/HPCA/ISCA/MICRO/ASPLOS/DATE/DSN/ITC/TC 等），全部逐篇精读
> 目的：为 SDCShield（ARM64 服务器 CPU SDC 压测工具）的**负载设计与依赖库选择**建立文献依据
> 交叉验证：与本仓库 `docs/CORE179_SDC_REPORT_CN.md`（192 核鲲鹏 920 实测缺陷，36 样本/562 bit）互证

---

## 0. 执行摘要（一页版）

**SDC 是什么规模的问题**：Meta PinDrop（HPCA'26，数百万服务器、4 年、5 亿次测试）实测 **0.035% 的机器（≈1/2850）终生至少一次 SDC 测试失败**；阿里 SOSP23（100 万+ CPU、28 数据中心、32 个月）实测总患病率 **3.61‱**。两者独立测得同量级。burn-in 后**每季度仍有 0.0024% 机器新发故障**（PinDrop），且 73.5% 的 SDC 出现在已服役 CPU 上（阿里）——SDC 检测是持续性命题，不是出厂一次性检查。

**SDC 挂在哪里**（跨论文最强共识）：**向量 FP 乘加单元（FMA）是第一大 SDC 源**——SEVI（ASPLOS'26）实测 **>92% 的 SDC 事故、>75% 的 case 由 FMA 指令贡献**；Veritas（HPCA'25）实测向量单元 SDC 率比标量高**数个数量级**；ITHICA（Stanford+Google）实测 **92.9% 的指令级错误来自 FP+vector 指令**。机理：乘法器门数约为加法器 30×、无 ECC 保护（时序关键路径加不了 ECC）、结果只进数据流不进控制流（错了不 crash、只静默错）。其次：**L1 D-cache 数据阵列**（永久故障 SDC 化率 Arm 53.3%、x86 64.7%，gem5-MARVEL）；**压缩/加密/哈希数据通路**（ITHICA 中 Zlib 检出最多）。反向结论：标量整数加法、控制流、ROB/LQ/SQ 的故障倾向于 crash 而非 SDC——压测预算不应浪费在那里。

**什么负载最能抓出 SDC**（故障传播理论 + 注入实验 + 生产实证三方收敛）：
1. **数据流型负载**：计算结果只进比对、绝不作地址/指针/分支条件（WD 型传播 → SDC；WI/WOI 型 → Crash）；
2. **位扩散型负载**（哈希/CRC/压缩）：From Gates to SDCs（DATE'25）定量实测 **sha 类负载掩蔽最少、SDC 最多**——"每一位都重要"；
3. **高熵随机操作数**：DelayAVF（MICRO'24）证明翻转率直接决定边际时延缺陷的可见性（md5 ≫ libstrstr）；SEVI 发现 37% case 有输入位偏置；
4. **长驻留、低覆写**的访存模式：覆写是天然掩蔽器（GeFIN/MARVEL 定量）；
5. **负载多样性 > 单负载强度**：PinDrop 实测 **31% 的测试曾在某机器上是唯一检出者**；阿里 633 个厂商 testcase 中 **560 个从未检出任何错误**；
6. **长时间重复**：>80% 坏机器首错在 1–10 秒内（SEVI/PinDrop），但存在 <10⁻⁵ 低频长尾；ITHICA 证明长程序检出 71 台 vs 短测试 42 台；
7. **全核并发 + 温度**：温度与 SDC 频率 log-线性正相关（阿里，Pearson>0.75）；全核同测升温是标准手段。

**对依赖库选择的一句话结论**：文献共同验证"**高度优化的第三方库 = 人工写好的数据流型 FU 饱和序列**"是有效负载形态（Harpocrates 基线中 OpenDCDiag 的 MxM/SVD 是非生成式负载里 FP 单元检出最高的；Veritas 直接用 Eigen matmul/SVD/sparse 作为机队 FP-heavy 探针）。优先级：**向量 FMA/GEMM 库（Eigen+OpenBLAS）> 哈希/加密（OpenSSL 默认开启）> 压缩（zstd/zlib 已有）> SVE 超越函数（SLEEF）**；盲区（IRF/LSQ 阵列、微架构边界序列）需生成式方法（SiliFuzz/Harpocrates 式）补足。

---

## 1. 文献全景与分组

本目录 31 篇论文按研究视角分七组，恰好构成一条完整的证据链：

| 组 | 视角 | 论文 | 回答的问题 |
|---|---|---|---|
| A. 生产实证 | 真实机队测了什么、发现了什么 | SOSP23（阿里）、PinDrop（Meta）、Veritas（雅典+Meta）、Fleetscanner/Ripple（Meta）、SEVI（CMU+Meta）、TC23 微架构视角（雅典） | SDC 有多普遍？根因是什么？什么测试检出了？ |
| B. 测试生成 | 怎么自动生成更有效的测试 | SiliFuzz（Google）、Harpocrates ISCA'24/IEEE Micro'26（雅典+AMD）、ETS'24、IOLTS'23 | 什么样的指令序列最能暴露缺陷？ |
| C. 运行时检测 | 业务运行中怎么防 | Orthrus（SOSP'25）、ITHICA（Stanford+Google）、Hardware Sentinel（ASPLOS'25，Meta）、Vega（ASPLOS'24）、稀疏矩阵+PMC（UMass） | 在线系统揭示了哪些高危计算模式？ |
| D. AVF 经典理论 | 故障为什么会/不会变成可见错误 | Mukherjee MICRO'03（AVF 开山）、Biswas ISCA'05（地址类结构）、Nair HPCA'12（一阶机理模型）、DelayAVF MICRO'24（时延故障 AVF） | 什么负载让单次故障最大概率变成可见 SDC？ |
| E. 故障传播 | 门级→输出的衰减漏斗 | Bower SIGMETRICS'06（H-AVF）、MeRLiN ISCA'17、From Gates to SDCs DATE'25、Demystifying ISCA'21（纯 Arm 实验） | 掩蔽发生在哪几层？怎么消除？ |
| F. 故障注入工具 | 用注入实验量化负载有效性 | GemFI DSN'14、CHAOS arXiv'26、MaFIN/GeFIN IISWC'15、gem5-MARVEL HPCA'24 | 同一故障在不同负载下 SDC 率差多少？ |
| G. ARM 软错误实测 | ARM 芯片的真实粒子照射 | TC'22（Cortex-A5/A9 中子束）、ITC'23 SLM（跨 ISA） | ARM 特有的脆弱性分布？ |

**证据强度分级**（诚实声明）：A 组是真实硅、真实机队，证据最硬；G 组是真实硅 + 加速器照射（但测的是瞬态粒子错误，与永久缺陷不同类）；F/E 组是模拟器注入（故障模型是 bit-flip/stuck-at，非真实物理缺陷，但"负载决定故障可见性"的结论与 A 组互证）；D 组是分析框架。所有结论下文均标注来源组别。

---

## 2. SDC 的规模、根因与物理机理（A 组生产实证）

### 2.1 患病率与时间分布

| 测量 | 数字 | 来源 |
|---|---|---|
| 阿里 100 万+ CPU、32 个月 | 总患病率 3.61‱；产前测试捕获 3.262‱（90.4%），常规运行期测试仅 0.348‱ | SOSP23 |
| Meta 数百万服务器、4 年 | 0.035% 机器终生 ≥1 次 SDC 失败；架构间 0.014%–0.178% | PinDrop |
| burn-in 后新发 | 每季度 0.0024% 机器新发（与阿里常规测试率同量级） | PinDrop |
| 已服役占比 | 73.5% 的 SDC 出现在已服役 CPU；BTI 退化 ~70% 发生在服役第一年 | 阿里 / Vega |
| 长尾 | 有机器测试近 4 年后才首次失败；>71% 坏机器持续失败多年 | PinDrop |
| 核间分布 | 62% 坏机器仅单物理核坏；24% ≥10 核 | PinDrop |
| 持续测试的增益 | 持续高频测试（PinDrop 模式）比快照式分组测试（阿里模式）多数量级地抓出缺陷 | PinDrop vs SOSP23 对比 |

**对压测的启示**：压测必须是**周期性、持续性**的；一次 pass 不证明健康；坏机器多数集中在单核（core 级定位与隔离是合理策略——与 CORE179 单核锁定 100% 一致）。

### 2.2 根因分类学

**PinDrop 根因框架**：marginal defects（边际缺陷）→ small delay faults（亚周期时延增量），仅在特定温度/频率/电压/程序/输入组合下发作，是厂商出厂测试逃逸（test escape）的主因；另有 wearout（老化）；明确**不是**粒子翻转。

**阿里 27 颗坏 CPU 深度分类**：计算型 19 颗（ALU / VecUnit / FPU，单线程可测）+ 一致性型 8 颗（cache coherence、transactional memory，**必须多线程才能测出**）。FP 数据类型（f32/f64/f64x）最易受影响。

**Veritas 单元排序**（gem5 门级注入 6750 万次 + Meta 机队 6 年双轨）：
- scalar int adder：几乎不产 SDC（结果喂控制流 → crash）；
- int multiplier：比 adder 高 **2 个数量级**；
- **vector 单元（int/FP）：比 scalar 高数个数量级**；
- 机队实测：Vector(FP) 是所有 CPU 的首要嫌疑单元；某 CPU 的 VFMUL 相对 SDC 率 4580 vs 标量 ADD 21；
- 机队中 crash 比 SDC 多 2–3 倍（大部分缺陷表现为崩，能静默化的是少数——但这少数正是最难抓的）。

**ITHICA 不一致错误洞察**（Stanford+Google，3000+ 台隔离服务器、100 台缺陷机）：最易逃逸出厂测试的缺陷导致 **inconsistent errors**——同一线程内同一指令、相同架构输入，两次执行产生**不同**错误输出，取决于 execution context（微架构+电气状态完整快照）。24% 的检出中原始与验证指令**都错但错得不同**。

**与 CORE179 的互证（极强）**：ITHICA 证明"错误是否显现由**前置指令序列塑造的 execution context** 决定，指令执行频率在 59% 案例中不是判别器"；CORE179 探针 H/X 实测**热路径加一条语义 no-op ALU 指令就把触发率从 ~100% 崩塌到 ~10–20%**。两者独立得到同一结论：**SDC 是相位/上下文敏感的，不是指令频率敏感的**。这直接解释了为什么"负载多样性"（不同库 = 不同指令调度）比"单负载轰炸"更有效。

### 2.3 触发条件（激活窗口）

| 条件 | 证据 | 来源 |
|---|---|---|
| **温度** | SDC 频率随温度**指数（log-线性）增长**，Pearson>0.75；温度是低复现 SDC 的关键触发器 | 阿里 SOSP23 |
| 电压/频率/输入组合 | marginal defect 仅在特定组合下发作；SiliFuzz 隔离机复现需数周并扫描电压/频率/温度组合 | PinDrop / SiliFuzz |
| 数据模式 | SEVI：37% FMA case 有显著输入位偏置（某位为 0 概率>0.8）；matmul 输入值域不同 SDC 率差 **245×** | SEVI |
| 指令序列上下文 | 见 2.2 ITHICA；SiliFuzz 的 FCOS 缺陷：输入操作数某一特定 bit 为 0 时几乎必错，为 1 则正常 | ITHICA / SiliFuzz |
| 并发压力 | CORE179：单进程单核不触发，全系统满载触发（192×单核进程并发同样触发） | 本仓库实测 |
| 时间 | SEVI：>80% case 首次失败在 1 秒内，但长尾需 42 小时长测 | SEVI |

**平台限制（本机现实）**：鲲鹏 920 开发板无 cpufreq、无 CPU thermal zone——业界抓边际缺陷的主杠杆（V/F/温度角落扫描，DelayAVF 明确指出"扫频扫压等价于扫描未知时延量 d"）在本机不可用。**我们的替代杠杆**：指令组合多样性 × 全核并发热功耗压力 × 足迹谱（L1/L2/LLC/DRAM）× 长时间 × seed 多样性。这决定了负载广度在本文档所有建议中的权重被相应抬高。

### 2.4 各论文间的矛盾与修正（诚实记录）

- 阿里 SOSP23 称"FP SDC 位翻转集中在 fraction 段、精度损失小（99.9% f64 损失<0.02%）"；**SEVI 用更大样本推翻**：存在 LELM/LEHM/HEHM 三种输出模式，HEHM（178 case）翻 exponent 位、相对误差可达 10240，65 case 翻符号位。**修正结论：不能假设 SDC 只动尾数；字节精确全位宽比对是必须的。**（CORE179 实测：float 尾数 85%/double 93%、符号位几乎免疫——介于两者之间，说明位分布是缺陷个案属性，不可先验假设。）
- 阿里"约半数坏 CPU 全核受影响"被 PinDrop 质疑为误报（PinDrop：62% 仅单物理核）。采信 PinDrop（样本与时长更大）。
- SOSP23 那篇是**清华+阿里云**的工作（常被误记为 Meta）；Veritas 是**雅典大学+Meta**（HPCA'25）；Google 的对应论文是 HotOS'21 "Cores that don't count" 与 SiliFuzz。引用时注意归属。

---

## 3. SDC 的微架构部位分布（哪些单元最危险）

### 3.1 综合排序（跨 A/E/F 组收敛）

| 部位 | SDC 倾向 | 证据 | 压测优先级 |
|---|---|---|---|
| **向量 FP 乘加单元（FMA/FMMLA/dot）** | 极高 | SEVI：>92% 事故是 FMA；Veritas：vector ≫ scalar；ITHICA：92.9% 错误来自 FP+vector | **最高** |
| **向量整数乘法单元** | 高 | Veritas：int multiplier 比 adder 高 2 个数量级；ETS'24：VFMUL 在 GEMM 49.6% SDC 概率 | 高 |
| 标量 FP 单元 | 中高 | Veritas：比 scalar int 高一个数量级；ITHICA：fp80 fmul 可单指令复现 | 中 |
| **L1 D-cache 数据阵列** | 高（永久故障） | gem5-MARVEL：permanent fault SDC 化率 Arm 53.3%/x86 64.7%；MeRLiN：L1D SDC 14.5–20% vs RF 仅 0.5–1.5%；TC22：L1D SDC AVF ≈30% | **高**（但有 ECC 的服务器核部分被兜底） |
| 主存/大容量存储 | 高（几乎不 crash） | CHAOS：主存 fault 几乎不 crash，结局二分为 SDC/Masked | 中 |
| decode 后的操作数值通路 | 中 | GemFI：decode 阶段 fault 通常 SDC | 中 |
| 标量整数加法器 | **低**（→crash） | From Gates to SDCs：加法器故障 Crash>80%、SDC 0–18%；Veritas：几乎不产 SDC | 低 |
| L1 I-cache / PC / BTB / tag | **极低**（→crash/Assert） | MARVEL：L1I SDC 仅总 AVF 的 1/9–1/17；GemFI：PC fault 几乎必死 | 极低 |
| ROB / LQ / SQ | **零 SDC** | TC23 微架构视角：依赖图检查 → 必 crash | 零 |
| 物理寄存器堆（PRF） | 永久故障零 SDC（→crash）；瞬态 0.5–6.9% | ITC'23：PRF permanent SDC 概率为零 | 低 |

**FPU 老化机理补充**（Vega，ASPLOS'24，电路级）：RTL 综合至 28nm 后做 aging-aware STA，**FPU 有 1366 条 aging-prone 路径，ALU 仅 11 条（124×）**——浮点数据通路长、组合逻辑深，BTI 应力下最先越界。老化型缺陷集中在长组合路径、低翻转率（SP 极端）的电路——即 FMA/深流水宽向量单元 + "罕见操作数组合才走到的路径"。

### 3.2 宽向量与指令代际（对 SVE 的直接指导）

- **SEVI：256-bit FMA 比 128-bit 多出低频（<10⁻⁵）SDC case**——更宽通路 + 2× 功耗致电压裕量波动。ARM 的 SVE（可变 128–512 bit）对应此风险面，且更长向量需要更长循环才能命中低频窗口。
- **PinDrop Obs12：指令在其首次引入的架构代失败率最高，后续代下降**。ARM 的新扩展（SVE/SVE2/SME/crypto 扩展）是最高风险区——**最新指令 = 最少验证积累 = 最易有逃逸缺陷**。
- SEVI gather 类发现：**76% 是错偏移读**（地址计算逻辑坏，读有效数据但地址错）、24% 脏读，且绝不 crash；因依赖 cache 状态，输入级复现率≈0——gather/scatter 必须专门验证"读偏移正确性"（本仓库 `sve512_gather_scatter_arm` 方向正确）。
- **98.5% SDC 事故只影响单个 vector lane**；多 lane 时 96% 仅相邻 lane（SEVI）——逐 lane 比对可精确定位；CORE179 的多 bit 整体破坏（cache line 级）则是另一形态，两者并存。

### 3.3 一致性/多线程类缺陷

阿里 8/27 颗坏 CPU 是一致性型（cache coherence、transactional memory），**必须多线程冲突场景才能测出**；Meta Hardware Sentinel 发现"锁不被线程共享、cache line valid bit 损坏"类缺陷。ARM 无 TSX，重点应放在 **cache 一致性 + 原子操作 + 锁路径**（本仓库 spinlock 系列测试方向正确）。

---

## 4. 故障传播与掩蔽理论（什么负载让故障"可见"）

### 4.1 统一框架：故障可见性方程

$$P(\text{可见 SDC}) = P(\text{故障激活}|\text{输入向量}) \times P(\text{变成错误}|\text{驻留/覆写}) \times P(\text{传播到输出且未被掩蔽})$$

四篇传播论文（Bower H-AVF、MeRLiN、From Gates to SDCs、Demystifying）+ AVF 经典组分别量化三个因子：

**因子 1 —— 激活依赖输入向量**（Bower）：stuck-at 是否激活是输入的函数；常量输入只激活一小部分 fault site。→ **操作数必须高熵随机且逐次变化**。
- MeRLiN 补充：**同输入的重复迭代属同一故障等价类，不增加覆盖**——逐次变化的数据才扩展覆盖。CORE179 实测（固定单一 seed 触发率反而高）看似矛盾，实则是两个窗口：seed 轮换扩覆盖（因子 1），seed 驻留最大化单一激活窗口的命中次数（因子 3 的时间维度）。**两档都要跑**。

**因子 2 —— 驻留与覆写**（GeFIN/MARVEL/Biswas）：
- 覆写/替换是天然掩蔽器：**同一结构、同一故障，脆弱性随负载差 19 倍（2.5%→47.3%）**，机理可定量追踪到 load 发射数与写命中率；
- MARVEL：GEMM DSA 的**输入** SPM AVF 远高于持续被覆写的**输出** SPM（58% vs 低）；FU 从 32 减到 2，SPM AVF 反升到 80.4%（访问变慢→更少覆写）；
- Biswas：**write-back cache 一旦写脏一个 byte，整行所有位从 fill 到 eviction 全部变 ACE**（脏行 AVF 25% vs write-through 6%）；高翻转/flushing 会把 AVF 砍半——压测要反着做：**长驻留、重复读**；
- → **负载的访存模式：写脏整行 → 反复读 → 少替换；working set 铺满目标结构但控制 miss**。

**因子 3 —— 传播路径决定 SDC vs Crash**（From Gates to SDCs + Demystifying + GemFI 三方一致）：
- 结果流向**数据比对** → SDC；结果流向**地址/指针/循环变量/分支条件** → Crash；
- Demystifying 的 FPM 分类：**WD（Wrong Data）→ 主要 SDC；WI（Wrong Instruction）/WOI（Wrong Operand/Immediate）→ 主要 Crash**；
- From Gates to SDCs：控制流/栈指令（ret/call/push/pop）的错误**从不产生 SDC**；
- → **被测数据只进比对；控制流用独立常量计数器**。

### 4.2 位扩散型负载的定量优势

From Gates to SDCs（DATE'25，雅典+Meta，门级故障注入 >10 万次，17 负载——**其中 3 个直接取自 OpenDCDiag，即本工具上游**）实测：**两个 sha 实现掩蔽最少、SDC 最多**，原文归因："hashing 用随机多样数据重度使用整数加法器，**every calculation result matters / 每一位都重要**"。

三重叠加优势：
1. 随机多样输入 → 降低门级逻辑掩蔽、提高 stuck-at 激活率；
2. 每个运算结果都重要 → 无外部掩蔽；
3. 全部位被消耗 → 无"高位丢弃"类掩蔽（对比：64×64 乘法高 64 位常被软件丢弃）。

算法自愈性是反面（GemFI）：收敛性迭代（Jacobi 对角占优必收敛"洗掉"错误）、适应度选择（遗传算法淘汰坏个体）、随机平均（Monte Carlo）都会吞掉故障。**SVD/稀疏求解类迭代算法天然是软掩蔽大户**——尽管它们 FMA 密度高（ETS'24：VFMUL 在 SVD 45.6% SDC 概率），但收敛吸收小扰动。CORE179 实测 SVD 多为单 bit（中位 3 bit、5/11 恰 1 bit）正是这个机理：迭代把单点扰动放大成恰好 1 ULP 差异——**仍然可检（memcmp 字节精确），但信噪比低于 GEMM 的多 bit 整体破坏**。

### 4.3 AVF 经典理论的设计准则

**Mukherjee（MICRO'03，AVF 开山）**：AVF = 结构中 ACE bits 平均驻留比例；默认所有 bit 都是 ACE 除非能证明 un-ACE。实测平均仅 45% 动态指令是 ACE（NOP 26%、dead 20%）。指令队列 AVF 28%（14–47%）、执行单元仅 9%（4–27%）；**浮点程序 AVF 显著高于整数程序**（长延迟指令多、分支误预测少、窗口占用高）。
→ 准则：杜绝 dead code、杜绝逻辑掩蔽、分支少、直线代码、每条指令结果都被消费。

**Nair（HPCA'12，一阶机理模型）**：**AVF ≈ correct-path 状态占用率 × ACE 比例**。"**高 IPC + 少 miss + 少分支 + 长依赖链**"的负载（namd 型）使 ROB/IQ/LQ/SQ/FU 同时满载且全为 correct-path——AVF 谱最广。聚合指标（IPC/miss rate）与 AVF 不相关。宽发射机器 SER 天然更高（4-wide 比 2-wide +81%）——**在鲲鹏 920 这类宽核上收益放大**。

**DelayAVF（MICRO'24，MIT+AMD）——最贴近 CORE179 类缺陷**：把 AVF 扩展到 small delay faults（边际缺陷）。关键结论：
1. **toggle 依赖**：信号不翻转则延迟无害——**ALU 的 DelayAVF 高达寄存器堆 5×**（尽管 regfile 静态可达路径更多，但每周期只有两根 wordline 翻转）；
2. **负载差异巨大：md5（数据随机、翻转率高）DelayAVF 远高于 libstrstr（规整数据）**——数据随机性 → 高翻转率 → 高 SDF 可见性；
3. 多位错普遍（SDF 引起的 state element 错误约 50% 产生多 bit 错误）——**整块 golden memcmp，单位抽检会漏**；
4. **ECC 对 SDF 有盲区**：wordline 延迟导致读错行的数据仍是"合法"数据，ECC 校验通过——功能级 golden 比对不可被硬件校验替代；
5. 扫频/扫压等价于扫描未知时延量 d（本机无 cpufreq，此杠杆缺失，见 2.3）。

**Bower（SIGMETRICS'06，H-AVF）**：永久故障版本——64-bit 加法器 H-AVF 0.149、寄存器堆 0.084、L1D 仅 0.0049。**小而高利用率的组合逻辑比大阵列对硬故障更脆弱**；负载要用足量多样化操作数持续打满功能单元。

### 4.4 汇总：负载设计七原则（文献推导 + CORE179 互证）

| # | 原则 | 文献依据 | CORE179 互证 |
|---|---|---|---|
| 1 | **结果只进比对，不进控制流**（WD 型） | Gates2SDC/Demystifying/GemFI | GEMM 写回→memcmp 触发；movbe reload 比较触发 |
| 2 | **高熵随机操作数，逐次变化** | Bower/DelayAVF/SEVI/MeRLiN | seed 敏感；自动 seed 必需 |
| 3 | **长驻留、低覆写、写脏整行再回读** | GeFIN/MARVEL/Biswas | store 与 reload 同 LLC domain（探针 E） |
| 4 | **全位宽消耗 + 字节精确 memcmp** | Gates2SDC（sha 最低掩蔽）/SEVI（exponent 也翻）/DelayAVF（多 bit 常见） | memcmp_or_fail 设计正确 |
| 5 | **高 IPC + 少分支 + 长依赖链 + 少 miss** | Nair/Mukherjee | tight FMA 循环触发最频繁 |
| 6 | **多样性 > 单负载强度**（不同指令调度样本） | PinDrop（31% 唯一检出者）/ITHICA（execution context）/SiliFuzz（随机与覆盖引导互补） | 探针 H/X：一条 no-op 改变一切 |
| 7 | **长时间 + 重复**（低频长尾） | SEVI/PinDrop/ITHICA（长 71 vs 短 42） | ttf 0.27–250s 波动，15m+ 窗口 |

---

## 5. 检测系统与部署策略（C 组反哺）

### 5.1 在线系统的共同假设（反证离线压测的正当性）

Orthrus（SOSP'25）、ITHICA、Hardware Sentinel 全部依赖三个假设：**缺陷持久（不自愈）、核心局部（不漂移）、指令相关（可定向）**。这三个假设正是离线压测的优势区：满核、长时间、golden 字节比较。在线系统反过来证明：压测检出即有效（缺陷不会自愈）。

### 5.2 覆盖率尚未饱和的证据

- **Hardware Sentinel（纯日志分析）比 Fleetscanner+Ripple 合并还多检出 41%**；SC-8 平台上两种专用测试全部失效，唯日志分析检出——测试覆盖远未饱和；
- 稀有内核异常在 SDC 机上的发生率远超机队均值：doublefault 59.35×、stack segment 20.77×、nx 20.03×——"稀有异常聚集于单核"是未识别的 SDC 指纹（对 SDCShield 的启示：可加 crash 事件的核聚集统计作辅助信号）；
- ITHICA 比工业 baseline 多 39% 检出，其中 **8 台仅在 Zlib/OpenSSL 库代码内部检出**（native 最终输出检查看不到——库内部错误会被 logical masking 掩蔽）→ **离线测试必须做库输出的全量 golden 比对，而非只查返回值**；
- CHAOS 发现被判 SDC/Masked 的运行在硬件性能计数器上可有数万百分点扰动——**PMC 可作廉价哨兵**，但主存类故障连 PMC 都静默，golden 比对不可替代；稀疏矩阵+PMC 论文（UMass）实测检测精度仅 ~90% 且关键特征跨数据集不稳定——PMC 只能做辅助通道，不能做主判据。

### 5.3 部署节奏（Ripple/Fleetscanner 二分）

| 模式 | 粒度 | 频率 | 覆盖 | 适用 |
|---|---|---|---|---|
| Fleetscanner（产外深测） | 分钟级 | 维护窗口，5–6 个月全机队一轮 | 93%（对该缺陷族） | 长时深测 |
| Ripple（在产高频） | 毫秒级片段，每日累计 ~30s | 每日 | 77%，**15 天即达 70% 等效覆盖** | 高频短测 |

Harpocrates++ 前缀实验补充：**对 FU 的 permanent 故障，最优序列前 10%（数百条指令）检出率几乎不降**——短而密的 FU 压测是有效的；但**间歇性/marginal 故障（温度、电压触发）仍需长序列**。→ SDCShield 应支持两种节奏：默认 `-t` 长窗口 + 可选快速扫描模式。

### 5.4 测试生成研究的补充结论

- **SiliFuzz**（Google，libFuzzer 作者团队）：fuzzing by proxy（XED/Unicorn/ifuzz 三代理）+ snapshot 差分重放。关键经验："**任何足够非平凡的执行，在足够大规模上都会挖出一些缺陷**"——纯随机指令序列（ifuzz）也独占发现 20% 缺陷；随机与覆盖引导**互补**。约 70% 缺陷只影响一个物理核；45% 的发现是其他工具没找到的。**高度优化的第三方库正是"人工写好的非平凡执行"。**
- **Harpocrates**（雅典+AMD，ISCA'24/IEEE Micro'26）：遗传算法闭环（Generator/Mutator/gem5 Evaluator），**覆盖率提升与检出能力强相关**（全文最关键实证）。生成测试在 IRF 上检出比其他框架高 **10x**；**OpenDCDiag 的 FP-heavy 测试（MxM、SVD）是所有非生成式负载里 SSE FP 单元检出最高的（adder 98.5%）——但 SSE FP multiplier 上 OpenDCDiag 只有 58.2%**，暴露通用负载的盲区。FU permanent 故障约 1000 轮收敛，位阵列（L1D）需 2000–5000+ 轮、30K 指令。
- **ITHICA 方法论结论**：短测试几乎无法复现（14 台中仅 1 台可单指令复现）；**长、多样、真实的程序优于短小定向测试**；同一缺陷常跨指令类型致错（44% 服务器），速率差 6 个数量级。

---

## 6. ARM 特有发现（G 组 + 纯 Arm 实验）

| 发现 | 来源 | 对 SDCShield 的意义 |
|---|---|---|
| **Arm 的 L1D/L1I 瞬态 SDC AVF 为三 ISA（x86/Arm/RISC-V）中最高，x86 最低** | ITC'23（gem5 三 ISA 对照） | x86 上的可靠性直觉不能直接搬到 Arm；负载必须在 ARM 上独立校准——印证本仓库"ARM64 并行移植、x86 不动"原则 |
| SDC 率"只属于 CPU 核执行用户代码本身"：SoC 集成使 DUE 升 97.7× 而 SDC 仅 1.3×；OS 使 SDC 升 2.1–2.4× | TC'22（Cortex-A5/A9 真实中子束，1800 万年自然暴露当量） | **绑核用户态计算 + golden 比对恰好是 SDC 的最小充分探测面**；无需测 SoC 互连（那是 DUE 的事，崩溃自会暴露） |
| 负载间 SDC FIT 差约 1 个量级；Qsort 型（数据长驻 cache 等待取回）SDC FIT 最高 | TC'22 | 长驻留负载暴露窗口最大（与 4.1 因子 2 一致） |
| Arm Cortex-A72 类 OoO 核：L1D data 53.4% / L1D tag 38.0% / L2 data 36.9% SDC 概率；**ROB/LQ/SQ 为零** | TC23 微架构视角（22 万次注入，唯一 ARM 微架构 SDC 传播研究） | 数据通路 cache 是 ARM 上最脆弱阵列；数据密集、控制流轻的负载最易把故障传到输出 |
| 存在"不可见 SDC"（ESC）：故障击中驻留 cache、不再被读、直接 DMA 写回的输出数据，绕过一切防护，概率随输出尺寸增大而升高（>3MB 显著） | TC23/Demystifying | 最终比对要覆盖所有流向输出的数据 |
| 内核指令仅占 ≤10% 执行量却贡献不成比例的 SDC（L1I tag 的 SDC 中 77% 来自内核指令） | TC23 | 用户态压测为主是合理的（内核态难控制） |
| x86↔Arm 的 AVF 差异平均 <2 个百分点，微架构实现比 ISA 更重要 | IISWC'15（MaFIN/GeFIN 差分） | gem5 Arm 模型预研结论对真实 ARM 芯片有参考性 |

**瞬态软错误 vs 永久缺陷的边界（诚实声明）**：G 组测的是粒子引起的瞬态错误（>95% 单 bit 翻转），SDCShield 抓的是永久/边际缺陷。两者的**表现形态与掩蔽分析完全共享**（高 SDC AVF 的"部位×负载"组合就是压测该花时间的地方），但物理起源不同：任何压测工具的 pass 都不能证明软错误率低；压测 pass 只对永久/边际缺陷是有意义的筛选证据。

---

## 7. 对 SDCShield 依赖库选择的直接指导

### 7.1 文献对"库负载"这个形态的定位

**正面背书（三方独立收敛）**：
1. Harpocrates 基线测量：OpenDCDiag 的 FP-heavy 测试（MxM/SVD）是**所有非生成式负载中 FP 单元检出最高的**；
2. Veritas 机队探针直接用 **Eigen matmul/SVD/sparse** 作为 FP-heavy 负载；
3. ITHICA 中 **Zlib 检出最多（23 台）、OpenSSL 次之**；Google cpu-check 的构成就是 compression/encryption/checksum/hash。

**形态定位**：高度优化的第三方库 = **人工调优的数据流型 FU 饱和序列**——恰好命中"高 IPC + 少分支 + 长依赖链 + 结果全消费"的 AVF 最优负载形态（Nair/Mukherjee）。

**结构性上限（必须自知）**：
- software masking 是通用负载通病（Harpocrates：SSE FP mul 上 OpenDCDiag 仅 58.2% 检出）——**库负载必须把最终结果完整传播并做 golden 全量比对**（本工具 memcmp_or_fail 正是如此）；
- IRF/LSQ 类阵列结构所有常规负载检出近 0（Harpocrates++：OpenDCDiag 在 IRF 上普遍 <5%）——这是负载选择无法弥补的盲区，需生成式方法补足（远期方向）。

### 7.2 库优先级排序（文献证据加权）

| 优先级 | 库 | 计算类型 | 覆盖目标 | 文献依据 |
|---|---|---|---|---|
| **1** | **向量 FMA/GEMM**：Eigen（已有）+ **OpenBLAS（建议新增，BSD-3，openEuler 有 RPM）** | NEON/SVE `fmla` 背靠背 + 分块写回 | 向量 FP 乘加单元（第一大 SDC 源） | SEVI 92%/Veritas 数量级/ETS'24 GEMM vector FP adder SDC 98.7% |
| **2** | **哈希/加密**：**OpenSSL libcrypto（建议战役构建默认 `-Dssl_link_type=dynamic`）** | SHA-2/3、AES-GCM avalanche | 整数加法器/乘法器数据通路 + 位扩散兜底 | Gates2SDC（sha 最低掩蔽）/ITHICA（OpenSSL 次之）/cpu-check 构成 |
| **3** | **压缩**：zstd/zlib（已有）+ **isa-l igzip 往返（零新依赖，建议新增测试）** | match 搜索 + 熵编码 | 整数乘法器 + 数据依赖分支 | ITHICA（Zlib 最多）/Veritas（Compression&Hashing 显著） |
| **4** | **SVE 超越函数**：**SLEEF（建议新增，Boost-1.1，vendor 到 third-party）** | SVE 多项式 FMA 链 | SVE 宽向量 + 新指令代际 | PinDrop Obs12（新代最高风险）/SEVI（256-bit>128-bit）/DelayAVF（宽通路翻转） |
| **5** | CRC：isa-l（已有） | 无进位乘 | 整数向量 | Veritas/crc32 实证 |
| 6 | 大整数：GMP（已有） | 长进位链 | 进位传播路径 | Bower（操作数组合产生长进位传播） |
| 7 | FFT：pocketfft（BSD-3 vendor）或自写 NEON 蝶形 | 位反转 scatter + twiddle FMA | store→load 转发 + 迭代放大 | ETS'24（FFT VFMUL 35%）/CORE179（scatter 结构正中判别条件）。**反推荐 FFTW**：GPLv2+ 与 Apache-2.0 冲突 + MEASURE 非确定 |

**不选**：标量整数加法循环（→crash 倾向，Veritas/Gates2SDC）；memcpy/带宽型作主探测器（CORE179 实测 memcpy1 从不触发——无 ALU+reload 复合结构）；任何运行时自适应/autotuning 库（违反逐位复现）；多线程 BLAS 的 SMP 模式（非确定归约——Eigen 192 核 ULP flake 的教训，`OPENBLAS_NUM_THREADS=1` 必须钉死，框架负责 192 核并行）。

### 7.3 测试设计模式（从文献固化为工程规范）

1. **copy→compute→verify 每迭代**（CORE179 double14 模式，实测最频繁触发器；等价于"每 bit 都参与最终比对"）；
2. **golden 在 init 一次算好，字节精确 memcmp**（SEVI：exponent/符号位也翻；DelayAVF：多 bit 常见；TC'22：>95% 单 bit 但不能假设）；
3. **dump 变体纪律**：热循环逐字节不动，dump 全放 cold 失败分支（ITHICA execution context + CORE179 探针 H/X：热路径加任何东西都可能杀触发）；
4. **双档 seed**：默认 fracturing 广域扫（MeRLiN：同输入重复不扩覆盖）+ `--max-test-loop-count=0` 固定 seed 驻留（CORE179：单一模式持续暴露触发率最高）；
5. **同数学多调度**：Eigen NEON / OpenBLAS NEON / ACL NEON 三份 GEMM = 三个相位样本（PinDrop 31% 唯一检出者 + SiliFuzz 随机/覆盖互补 → 调度多样性是免费检出率）；
6. **足迹旋钮**：L1/L2/LLC/DRAM 全谱（Biswas：不同结构的 ACE 驻留；负载须持续跑满 `-t` 窗口，eigen_sparse 秒退教训）;
7. **全核并发**是硬前提（CORE179 + PinDrop all-cores + 阿里全核升温）；
8. **lane 级比对**（SEVI 98.5% 单 lane）+ 整块 memcmp（CORE179 多 bit 整体破坏）双形态都保留；
9. 可选增强：**PMC 偏差哨兵**（CHAOS：SDC/Masked 运行的 HPC 可有数万百分点扰动）+ **crash 事件核聚集统计**（Hardware Sentinel：稀有异常 59× 聚集是 SDC 指纹）——均作辅助信号，主判据永远是 golden memcmp。

### 7.4 战役协议

- **两阶段**：全组合 + seed 轮换广域扫（覆盖异构缺陷——不同批次缺陷挂在不同单元）→ 可疑核/测试固定 seed 驻留（CORE179 ttf 0.27–250s 波动，15m+ 窗口）；
- **持续化**：PinDrop 证明持续测试比快照测试多数量级地抓出缺陷——SDCShield 应作为周期性部署手段（对应仓库 scripts/run/ 下的循环脚本方向）;
- **长 + 短双节奏**：默认长窗口 + 快速扫描模式（Harpocrates++：FU permanent 数百条指令即检出，marginal 需长序列）。

---

## 8. 与 CORE179 实测的全面互证表

| 文献结论 | CORE179 实测 | 一致性 |
|---|---|---|
| ITHICA：execution context（前置指令序列）决定错误显现，指令频率 59% 案例不判别 | 探针 H/X：一条语义 no-op ALU 指令把触发率 ~100%→~10–20% | **强互证**（同一现象的软件侧/硬件侧表述） |
| 多位翻转多于单位（PinDrop/SEVI/DelayAVF 50% 多 bit） | 每次 fail 翻 10–39 bit，无一单 bit（movbe/GEMM 路径） | 一致 |
| SEVI：SVD 型迭代输出 1 bit（HEHM/LELM 分型） | eigen_svd 中位 3 bit、5/11 恰 1 bit；svd_double2 恰 1 ULP | 一致（迭代放大机制） |
| 阿里：温度 log-线性触发 | 无法验证（本机无 thermal zone）——待有传感器的板子 | 未检验 |
| 全核同测是标准（PinDrop/阿里） | 单进程单核 0 fail，全核满载触发；OFHC 192×单核并发也触发 | 一致 |
| SEVI：98.5% 单 lane | CORE179：cache line 级多 bit 整体破坏 | **不同型**——CORE179 缺陷在 LSU/store-buffer，非向量 lane；两形态并存正是"负载组合"的依据 |
| PinDrop：62% 坏机器仅单物理核 | 全部失败锁定 core 179，从不漂移 | 一致 |
| Bower：激活依赖输入向量 | seed 敏感（同 seed 重试 ttf 波动百倍） | 一致 |
| GeFIN：覆写是掩蔽器 | 探针 E：store 与 reload 须同 LLC domain | 互补解释 |
| SiliFuzz：缺陷需扫描 V/F/T 组合复现 | 本机无 cpufreq——主杠杆缺失，以负载多样性补偿 | 平台限制，已声明 |

---

## 9. 收录文献清单（31 篇）

### A. 生产实证（6）
1. **SOSP23** Understanding Silent Data Corruptions in a Large Production CPU Population — 清华+阿里云（100 万+ CPU）
2. **HPCA'26** PinDrop: Breaking the Silence on SDCs in a Large-Scale Fleet — Meta+MIT（数百万服务器、4 年）
3. **HPCA'25** Veritas: Demystifying SDCs — Arch-Level Modeling and Fleet Data of Modern x86 CPUs — 雅典大学+Meta（6750 万次门级注入 + 6 年机队）
4. **arXiv'22** Fleetscanner/Ripple: Detecting silent data corruptions in the wild — Meta
5. **ASPLOS'26** SEVI: Silent Data Corruption of Vector Instructions in Hyper-Scale Datacenters — CMU+Meta（246 用例穷尽 AVX2/FMA3/BMI，78 万亿轮）
6. **IEEE TC'23** Silent Data Corruptions: Microarchitectural Perspectives — 雅典大学（Arm Cortex-A72 类核，22 万次注入）

### B. 测试生成（5）
7. **arXiv'21** SiliFuzz: Fuzzing CPUs by proxy — Google
8. **ISCA'24** Harpocrates: Breaking the Silence of CPU Faults through Hardware-in-the-Loop Program Generation — 雅典+AMD
9. **IEEE Micro'26** Harpocrates: Automated Functional Program Generation Against CPU Faults and SDCs — 雅典+AMD（期刊扩展）
10. **ETS'24** Silent Data Corruptions in Computing Systems: Early Predictions and Large-Scale Measurements — 雅典+Meta
11. **IOLTS'23** Silent Data Corruptions: The Stealthy Saboteurs of Digital Integrity — 雅典+Meta

### C. 运行时检测（5）
12. **SOSP'25** Orthrus: Efficient and Timely Detection of Silent User Data Corruption in the Cloud — 中科院大学/UCLA/Berkeley/北大/清华
13. **arXiv'26** ITHICA: Intra-Thread Instruction Checking Approach for Defect-Induced SDCs — Stanford+Google（3000+ 台隔离服务器）
14. **ASPLOS'25** Hardware Sentinel: Protecting Software Applications from Hardware SDCs — Meta（6 年运维日志挖掘）
15. **ASPLOS'24** Vega: Proactive Runtime Detection of Aging-Related SDCs — A Bottom-Up Approach — UMich/Technion/UW/HebrewU（RTL 级老化分析）
16. （短文）Detecting Silent Data Corruption in Sparse Matrices using Hardware Performance Counter — UMass Lowell

### D. AVF 经典理论（5）
17. **MICRO'03** A Systematic Methodology to Compute the AVF for a High-Performance Microprocessor — Intel+UMich（AVF 开山）
18. **IEEE Micro'03** Measuring Architectural Vulnerability Factors — 同上（Top Picks 杂志版）
19. **ISCA'05** Computing AVFs for Address-Based Structures — Intel+Sun+Princeton
20. **HPCA'12** A First-Order Mechanistic Model for AVF — UT Austin+Ghent
21. **MICRO'24** DelayAVF: Calculating AVFs for Delay Faults — MIT+AMD（边际时延缺陷，最贴近 SDC 物理根因）

### E. 故障传播（4）
22. **SIGMETRICS'06** Applying Architectural Vulnerability Analysis to Hard Faults — IBM+Duke（H-AVF）
23. **ISCA'17** MeRLiN: Exploiting Dynamic Instruction Behavior for Fast and Accurate Microarchitecture Level Reliability Assessment — 雅典+UPC
24. **DATE'25** From Gates to SDCs: Understanding Fault Propagation Through the Compute Stack — 雅典+Meta（17 负载含 3 个 OpenDCDiag 负载）
25. **ISCA'21** Demystifying the System Vulnerability Stack — 雅典（**纯 Arm 实验**：Cortex-A9/A15/A57/A72 + eMAG 实机）

### F. 故障注入工具（4）
26. **DSN'14** GemFI: A Fault Injection Tool — Thessaly+Northwestern
27. **arXiv'26** CHAOS: Controlled Hardware Fault Injector System for gem5 — Catania（开源）
28. **IISWC'15** Differential Fault Injection on Microarchitectural Simulators（MaFIN/GeFIN）— 雅典（30 万次注入）
29. **HPCA'24** gem5-MARVEL: Microarchitecture-Level Resilience Analysis of Heterogeneous SoC — 雅典（25 万次注入，三 ISA）

### G. ARM 软错误实测（2）
30. **IEEE TC'22** Soft Error Effects on Arm Microprocessors: Early Estimations versus Chip Measurements — UFRGS/雅典/Torino（Cortex-A5/A9 真实中子束）
31. **ITC'23 SLM** Estimating the Failures and Silent Errors Rates of CPUs Across ISAs and Microarchitectures — 雅典（x86/Arm/RISC-V 三 ISA 对照）

（另有 CHAOS.docx、Orthrus.docx 为对应 PDF 的 Word 副本；Google HotOS'21 "Cores that don't count" 与 Meta arXiv 2102.11245 未收录本目录，正文引用处已注明。）

---

## 10. 结论：SDCShield 的理论定位与行动清单

**定位**：文献证明离线 golden-value 压测不可被替代——在线系统（Orthrus/ITHICA）依赖"缺陷持久、核心局部、指令相关"三假设，这些假设的成立恰以离线压测的检出为实证；硬件校验（ECC）对 SDF 有系统性盲区（DelayAVF）；PMC 精度 ~90% 且特征不稳（UMass）；日志挖掘（Hardware Sentinel）只能事后。SDCShield 的"绑核用户态计算 + 全核并发 + 长时间 + 字节精确 golden 比对"正是文献推导出的最小充分探测面（TC'22：SDC 只属于 CPU 核执行用户代码本身）。

**行动清单（按证据强度排序）**：
1. **新增 OpenBLAS GEMM 测试**（dgemm/sgemm/zgemm，copy→compute→verify 每迭代，单线程钉死，多矩阵尺寸扫足迹）——命中第一大 SDC 源，与 Eigen/ACL 构成三调度样本；
2. **战役构建默认启用 OpenSSL**（`-Dssl_link_type=dynamic`）并扩充 SHA/AES 大缓冲测试——位扩散兜底，ITHICA 实证次高产出；
3. **新增 isa-l igzip 压缩往返测试**——零新依赖，补第三种压缩调度样本；
4. **vendor SLEEF 到 third-party，加 SVE 超越函数测试**——覆盖新指令代际最高风险区（PinDrop Obs12）；
5. **保持并强化 seed 双档机制**（fracturing 广域 + 驻留定点）；
6. **可选增强**：PMC 偏差哨兵、crash 事件核聚集统计（均辅助信号）；
7. **远期**：SiliFuzz 式 snapshot 差分（192 核互为 golden）与 Harpocrates 式生成测试补 IRF/LSQ 盲区；有条件时上 V/F/温度扫描（DelayAVF 的"扫 d"杠杆）。

**最后一条方法论（来自所有 31 篇的共识）**：SDC 检测没有银弹——**多样性（负载 × 指令调度 × 数据 × 时间 × 足迹）就是检出率**。PinDrop 的 31% 唯一检出者、SiliFuzz 的 45% 独占发现、HWSentinel 的 41% 增量，都在说同一件事：每多一种"足够非平凡的执行"，就多一批只有它能抓到的缺陷。
