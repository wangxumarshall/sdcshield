# [33] Orthrus:云中静默用户数据损坏的资源配置自适应计算验证(SOSP 2025)

## 标题与出处

- **标题**: Orthrus: Efficient and Timely Detection of Silent User Data Corruption in the Cloud with Resource-Adaptive Computation Validation
- **会议**: ACM SOSP 2025(2025-10-13/16,首尔);DOI: 10.1145/3731569.3764832;ISBN 979-8-4007-1870-0/2025/10;CC-BY 4.0,版权"held by the owner/author(s)"。
- **类型**: 学术论文(系统顶会全文,含附录 A 故障注入框架 + 附录 B artifact;开源 github.com/ICTPLSys/Orthrus)。

## PDF 与实读范围

- **文件**: `ref/SOSP2025 Orthrus Efficient and Timely Detection of Silent User Data Corruption in the Cloud with Resource-Adaptive Computation Validation.pdf`
- **页数**: 20 页(论文正文 pp.286–304,第 20 页空白),**全文实读,无截断**。

## 作者全列(p.1)

Chenxiao Liu, Zhenting Zhu(与 Liu 同等贡献), Quanxi Li, Yanwen Xia, Yifan Qiao, Xiangyun Deng, Youyou Lu, Tao Xie, Harry Xu, Zidong Du, Huimin Cui, Chenxi Wang(通讯作者:Chenxi Wang、Harry Xu;导师型作者 Harry Xu/UCLA)

## 机构分列

- **学术机构**(5 家): University of Chinese Academy of Sciences(中国科学院大学,主体);Tsinghua University(清华大学);UCLA;UC Berkeley;Peking University(北京大学)。致谢补充:Tao Xie 兼属北大高可信软件技术重点实验室、复旦大学系统先进计算研究院、上海开放计算系统研究院。
- **企业机构**: 无(纯学术团队:中国学术共同体 + 美国两校系统组合作)。
- **作者-机构映射说明**: 文本层丢失上标角标,逐人映射部分为推断——确凿:Tao Xie=北大(致谢明示)、Harry Xu=UCLA(通讯作者);版面/常识推断:Youyou Lu=清华、Huimin Cui/Zidong Du=国科大(中科院计算所,GitHub 组织 ICTPLSys=ICT 并行系统实验室)、Chenxi Wang 对应 UC Berkeley(与 ref.[69] Panthera PLDI'19 作者谱系一致)——详见身份核实。

## 企业合作证据(三级)

- **一级(作者机构)**: 无企业作者。
- **二级(致谢/资助)**: 资助全部为政府科研经费——国家重点研发计划(2024YFE0204100)、中科院先锋人才计划 B(E545030000)、国家自然科学基金(92464301)、美国 NSF(CNS-2403254/2330831/2106838);致谢仅感谢评审与 shepherd(Wyatt Lloyd)。无企业资助。
- **三级(版权页)**: ACM/CC-BY,非企业版权。
- **间接层**(产业界以引用/数据/通信形式深度介入,虽无合作身份): ① **Alibaba Cloud**:个人通信 [11]("Anonymous. Personal communication with Alibaba Cloud, 2025",关于阿里云要求开发者对用户数据每次读写后显式做校验和的实践);故障注入分布 1:2:2:1(ALU:SIMD:FPU:cache)直接镜像其 SOSP'23 生产环境分布 [71];"3.61" 检出率数字出自其百万处理器测试 [71]。② **Google**:"mercurial" 话语 [42](Hochschild HotOS'21)、离线 CPU-check 测试集 [38]、云数据完整性指南 [40]。③ **Meta**:Dixit 等 [30] 作 SDC 分布与指令相关性依据;**Meta CacheLib 生产缓存负载** [1][14] 作 Memcached 评测数据集(前 20% 对象占 80% 请求)。④ **AWS** S3 对象完整性文档 [9]、**MongoDB** 数据损坏管理博客 [48] 作云端 SDC 威胁动机。⑤ Oak Ridge [34]/Los Alamos [52] 国家实验室 fleet 实测作旁证。

## 核心结论

1. **问题定位(§1)**: 安装后(post-installation)CPU 错误静默污染云应用用户数据;现行主流是离线 fleet 测试(约每 3 个月一轮 [30][71]),但它**检出的是 mercurial core 而非其已污染的用户数据**——测试执行前账户可能早已被破坏;检出及时性是云厂商(Google/AWS/Alibaba 均视为 catastrophic)的核心诉求。
2. **现有在线技术不可产线部署(§1)**: 复制式验证 RBV 需在独立服务器跑副本+状态机复制,>100% CPU 与内存开销、TCP/IP 跨服务器同步;指令级验证 ILV 逐周期同步比较,50× 减速且需专用硬件——二者均不实用。
3. **目标与非目标(§1)**: 低开销 best-effort **在线验证助手**;明示不做——检测 mercurial core、覆盖保证、容错机制;检出即中止应用,阻止污染数据返回客户端。
4. **核心洞察(§1, §2.2, Listing 3)**: 云应用普遍存在控制路径(调度/分发,不修改用户数据)与数据路径(用户数据算子:get/set/map/reduce/insert/update)的清晰分离;数据路径逻辑简单、可整体重执行——Memcached 数据算子全部集中在 4 个文件,控制路径代码量 >20× 数据路径;该模式在全部被评应用中一致出现。
5. **混合验证机制(§1, Fig.1, §3.3–3.4)**: 数据路径——每个被 `#pragma closure` 注释的算子在**异核**重执行(VAL)并与原执行(APP)按位比较;控制路径——用户数据进出数据路径边界处用 **16-bit CRC 校验和**验证完整性(每对象版本一份,~1% 开销,几十个周期,SSE4.2 加速)。
6. **版本化内存与闭包日志(§3.1, §3.3)**: APP/VAL 分进程,共享版本化用户数据空间;每次写产生新版本(OrthrusPtr.store 原子就地外更新);闭包日志自包含(输入版本/输出/系统调用结果),VAL 可**乱序、异步、独立**重放——把 RBV 的同步复制转化为共享内存日志流水,消除 APP-VAL 依赖。
7. **编译器支持(§3.2)**: LLVM IR 级两遍变换(类型推断 + 逃逸分析):用户数据类继承 OrthrusObj(含校验和字段)、分配点替换为 OrthrusNew、指针替换为 OrthrusPtr;不逃逸对象留在私有堆免版本化;开发者仅加 `#pragma user-data`/`#pragma closure`,四个真实应用移植各 **<20 行代码**。
8. **资源自适应调度(§3.5)**: NUMA 感知调度器把验证任务放到应用空闲核;基于 Shenango 用户线程实现**微秒级**动态扩缩;**采样策略直接编码 fleet 规律**——优先近期未验证闭包(错误可复现、与特定指令相关 [33][42])、按 (closure, caller) 对区分调用上下文、对含 fp/vector 指令的闭包提权(错误高发单元 [30][39][71]);验证核与 APP 核不共享 ALU/FPU/向量单元(Intel 核私有单元下同 NUMA 内共置安全,提升 L3 命中)。
9. **内存回收(§3.6, Fig.4–5)**: 版本化推高内存;以"版本可见窗口/闭包活跃窗口"时间重叠判定 + 组合队列最早起点近似算法批量回收,绕开传统 GC/引用计数在非托管语言与乱序验证下的高开销——总内存开销压至 20%(时间开销可忽略)。
10. **性能结果(§4.2–4.3, Fig.6–8)**: 总体 **4% 时间 / 25% 内存**开销,较 RBV 快 1.9×(逐应用:Memcached 1.6×、Masstree 2.9×、Phoenix 1.5×、LSMTree 快 54%);RBV 为 2.0× 时间 / 2.1× 内存;验证延迟逐应用 1.6µs(Memcached)/22.6µs(Masstree)/7.7µs(LSMTree)/234ms(Phoenix),比 RBV 低 2–3 个数量级(RBV 因跨服务器同步与复杂结构等价比较,尾延迟差至 1000×)。
11. **SDC 覆盖(§4.4, Table 2, Fig.9–10)**: 1/2/4 个验证核检出 **87%/91%/96%**;自适应采样较无引导随机采样高 1.41×;满核上界(与应用同核数验证)下按错误类型(arithmetic/floating-point/vector/cache)RBV 98.3–99.8% vs Orthrus 97.2–98.9%——差距几乎全部源于 RBV 重执行整个控制路径(可捕获控制流分支错误),Orthrus 漏检的 RBV 独有检出中仅 3 例为系统调用错误(write 1、mutex 2);内存受限(至 5% 附加)下 Phoenix 检出率仍 ~92.7%。
12. **故障注入方法(§4.1, 附录 A)**: LLVM **MIR 级**(机器码级,较 IR 级准确,基于 REFINE)注入;故障分布 **1:2:2:1**(ALU:SIMD:FPU:cache,附录 A.3.2 扩展含 TSX 为 1:2:2:1:1)镜像 Alibaba 生产实测 [71];注入类型 bitflip/stuck-at-0/stuck-at-1/nop;Clang 16.0.6 后端 4376 行 C++ + 2118 行 Python 测试平台;错误按输出分为 fail-stop/masked/SDC 三类,仅统计 SDC。
13. **能力边界(§2.3 What Orthrus Cannot Detect, 五条明示)**: masked error(不影响结果,无需检出);非确定性系统调用(记录重放、不验证——但 profiling 显示此类仅占执行指令 0.04%);控制-数据交互错误(控制路径比较错误调错算子,校验和不能全检);APP 与 VAL 双核以完全相同方式损坏(极小概率);采样漏检(资源不足时)——best-effort 定位的诚实边界声明。
14. **对同核重放类方法的否定(§5)**: PASC/SEI 等利用错误瞬态性的同核重放验证,对**持久性、可复现、核内固定**的硬件错误失效(重放同样出错)——必须在异核验证;这是与瞬态错误检测技术的故障模型分野。
15. **实验平台(§4.1, 附录 B)**: 3 台服务器,各 2× Intel Xeon Gold 6342,100Gbps Mellanox ConnectX-5 InfiniBand;Ubuntu 18.04/内核 5.14,关 Turbo/C-states/调频;负载:Memcached(Meta CacheLib 150M ops)/Masstree(ALEX 200M ops)/LSMTree(YCSB 合成 50M ops 写密集)/Phoenix(WMT24 新闻 15GB word count)。

## 分类学标注

- **根因机理**: 不做根因研究;故障模型为"安装后 CPU 错误"(mercurial core 谱系 [42]),按单元分布 1:2:2:1(ALU:SIMD:FPU:cache)注入——对根因谱系(结构/老化/逃逸)不区分,一律以"指令级静默计算错误"对待。
- **故障模式**: masked error 与 SDC 的指令级二分(Listing 1 分支计算错但分支结果不变 vs Listing 2 哈希计算错返回错数据);静默计算错误四类型:算术/访存/向量/跳转 [30][38][44][71];控制路径错误经控制流污染数据(调错数据算子)——控制路径错误的间接数据污染模式。
- **检测技术**: **运行时在线检测——软件层(编译器+运行时)选择性冗余**:混合式(数据路径闭包异核重执行 = 空间冗余子集 + 控制路径 CRC = 校验和);资源自适应采样是开销-覆盖的调节器;生命周期定位:**部署运行期**(post-installation、in-production、与业务共栈运行)。
- **处理技术**: 检出即中止应用(阻止污染数据外流给客户端)——检测触发的最小处置;可选 strict safe mode(结果验证完成前扣留,仅对会外曝数据的算子生效,开销 <2% 时间)。

## 业界观点摘录

- "Microprocessors have advanced to a point where they are no longer entirely reliable. As Google describes, they have become 'mercurial' [42]."(§1)
- SDC "considered catastrophic by cloud providers such as Google [40], AWS [9], and Alibaba [71]"——银行账户余额返回错误值的赔偿/联邦监管罚款/诉讼场景推演(§1)。
- "Alibaba Cloud requires developers to explicitly implement data integrity verification (with checksums) after every read or write operation on user data [11, 71]. However, this manual process is labor-intensive and checksums cannot be used to detect instances where data payloads are corrupted by CPU errors during data updates."(§1——业界现行应用层实践及其局限的第一手转述)
- "cloud providers usually schedule validation at a (low) frequency (e.g. once a few weeks) and damages may have occurred during an interval [30, 71]."(§5——离线测试窗口期风险)
- "Google reported that these errors manifested monthly, often long after installation, and were isolated to specific CPU cores rather than entire chips or families of components [42]."(§1)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 2%–6% / 4% / 25% | 摘要开销区间 / 典型时间开销 / 内存开销 | Abstract, §4.2 |
| 87% / 91% / 96% | 1/2/4 个验证核的 SDC 检出率 | Abstract, Fig.9 |
| 40µs | 验证延迟(摘要/导语口径;逐应用 1.6µs/22.6µs/7.7µs/234ms,见身份核实) | §1, §4.3 |
| 1.9× | 总体快于 RBV(逐应用 1.6×/2.9×/1.5×/+54%) | §1, §4.2 |
| >100% / 2.0× / 2.1× | RBV 的 CPU+内存 / 时间 / 内存开销 | §1, §4.2 |
| 50× | ILV 性能减速(需专用硬件) | §1 |
| 97.2–98.9% vs 98.3–99.8% | 满核上界下 Orthrus vs RBV 按错误类型检出率 | §4.4, Table 2 |
| 1.41× | 自适应采样相对随机采样的检出率优势(单核) | §4.4 |
| 16-bit / ~1% / 几十周期 | 控制路径 CRC 每对象版本 / 开销 / 计算代价 | §3.4 |
| <20 行 | 单个应用移植所需代码修改量 | §4.1 |
| >20× | 控制路径相对数据路径的代码量(Memcached) | §2.2 |
| 1:2:2:1(:1) | 故障注入单元分布 ALU:SIMD:FPU:cache(:TSX),镜像 Alibaba 生产 | 附录 A.1/A.3.2 |
| 3.61 | Alibaba 百万处理器测试的 SDC CPU 检出率(单位符号文本层丢失;交叉证据倾向 per 10,000,见身份核实) | §2.1 [71] |
| 0.04% | 非确定性系统调用指令占执行指令比例 | §2.3 |
| 20% | 版本化内存经 GC 后的总内存开销 | §3.6 |

## 方法论要点

- **按执行路径定制验证强度**: 控制路径(20× 代码量、不含数据计算)用 O(数据量) 的校验和,数据路径(简单闭包)用重执行——冗余量与被保护对象的计算密度匹配,而非全程序复制。
- **版本化内存把同步问题转化为吞吐问题**: APP-VAL 依赖被版本快照彻底解除,乱序验证 + 共享内存日志替代 RBV 的线性化跨机复制——同步开销换成内存开销,再用可见窗口近似 GC 把内存开销压回 20%。
- **fleet 规律直接编码为调度启发式**: 错误可复现→优先未验证闭包;错误与指令类型相关→fp/vector 闭包提权;错误与执行单元相关→异核验证且不共享 FPU/向量单元——产业实测规律(不建模)驱动的工程决策。
- **故障注入分布镜像真实生产**(1:2:2:1 源自 Alibaba [71])而非均匀注入——评测外效度的方法论自觉;MIR 级注入较 LLVM IR 级更接近真实机器码。
- **best-effort 系统的边界声明范式**: Goals and Non-Goals + 专门 §2.3 五条"不能检出什么"——覆盖保证的缺位以诚实边界声明补偿。

## 横向对比注记

- **与 [01] Alibaba SOSP'23**: 直接继承其故障模型(1:2:2:1)与 3.61 检出率;定位互补——离线 fleet 测试检出坏 CPU(mercurial core),Orthrus 在线检出坏数据(用户数据损坏);Orthrus §5 明确离线测试存在窗口期损害盲区。3.61 单位口径的交叉证据见身份核实。
- **与 [06] ITHICA / [07] Hardware Sentinel**: 硬件层线程内检查/哨兵 vs **纯软件**(编译器+运行时,零硬件支持,可即时部署于存量云);ITHICA/Sentinel 保护通用计算流,Orthrus 只保护用户数据(数据路径);三者同属"运行时检测"大类但栈层不同(微架构 vs 应用/运行时)。
- **与 [03] Fleetscanner/Ripple、[38] Google cpu-check**: 被引为"离线测试"生态代表;Orthrus 是其在线补全——同一 SDC 防线的两个时段(定期体检 vs 持续监护)。
- **与 [39] exabyte-db(Google Spanner,本批待读)**: 数据库层校验和/防 SDC vs 通用云应用层数据路径验证——同为"用户数据完整性在线防护",R5 内部按"被保护对象与检查点位置"细分对比。
- **与 [34][35][36](本批待读,PMC/ML 检测)**: 确定性重执行(精确、有开销)vs 硬件计数器/学习模型(低开销、启发式)——运行时检测谱系的确定性一翼;[37] kg-vulnpred 的指令脆弱性预测与 Orthrus 的 fp/vector 提权同为"按指令类型分流验证资源"思想。
- **与 R6 [40]–[43](futures-replication/parallaft/paraverser/hpc-duplication)**: 同属软件冗余家族——Orthrus=采样式选择性重执行(覆盖换开销),Parallaft/ParaVerser=异构并行全量检测;R6 读时对比开销/覆盖数字。
- **与 [32] Lerner 四层金字塔**: Orthrus 是"workloads 显式数据检查"层的运行时/应用内实现样本,亦即 Sankar"asymptotically to zero... applications fault tolerant"结论的云应用侧落地——产业界"应用层防线"主张的学术对应物。
- **与 [04] Veritas / [18] gates-to-sdc 等微架构传播分析**: 只**利用**其结论(指令类型相关性)而不建模传播——应用层工程与微架构分析的知识流向(建模→利用)样本。
- **共同体谱系**: 继 Alibaba [01](工业界 fleet 实测)后,国科大/清华/北大 + UCLA/UC Berkeley 纯学术系统团队进入 SDC 运行时检测(SOSP'25)——SDC 研究从产业 fleet 报告扩散至学术系统社区的标志事件;评测依赖 Meta CacheLib 负载、Alibaba 故障分布、Google 话语体系——三巨头以数据/规律/话语形式支撑学术工作(间接层企业影响的典型结构)。

## 身份核实

- **venue/DOI 核实**: SOSP '25(首尔 2025-10-13/16)+ DOI 10.1145/3731569.3764832 + ISBN + 页码 286–304 连续一致,CC-BY 4.0,确凿。
- **作者-机构映射损失**: 文本层丢失作者上标角标;5 家机构确凿,逐人映射仅 2 项有文内确证(Tao Xie=北大,致谢明示;Harry Xu=UCLA,通讯作者),其余(Youyou Lu=清华、Huimin Cui/Zidong Du=国科大、Chenxi Wang=UC Berkeley)为版面布局 + 社区谱系(ref.[69] Panthera 作者行含 Chenxi Wang/Huimin Cui/Guoqing Harry Xu)推断,已逐项标注,不作断言。
- **"3.61" 单位口径(跨笔记分歧,如实记录)**: 本文文本层"3.61 of them"单位符号丢失;源论文 [71](=本集 [01] Alibaba SOSP'23)在笔记 01 中同样丢失并曾**推断为‰**;但 [27] Harpocrates Micro'26 引用"**3.61 CPUs per 10,000**"(=361 DPPM)。量级一致性(Google/Meta ~1000 DPPM [27] Fig.1)支持 per 10,000 而非‰(3.61‰=3610 DPPM,偏高一个量级);且 [27] 将该数字归属 Meta 2023.10、本文归属 Alibaba [71],归属亦存分歧——两种口径与两种归属均记录,倾向 per 10,000。**2026-09-28 Task 10 终裁已落定**:单位=3.61/万(=361 DPPM),归属=Alibaba(2023.10=SOSP'23=[01]);[27] 原文 Fig.1 该数字出自其参考文献 [3]=S. Wang et al. SOSP'23,正文按引用编号转述,笔记 27 原记"Meta 2023.10"系提取错误,笔记 01(单位)与 27(归属)均已更正;[47] Table 1、[53] §1、[02] PinDrop(0.348→"0.0035%"=3.5/万)四源一致。
- **40µs 内部张力**: 摘要/导语"validation latency of 40µs ... three orders of magnitude lower than RBV"与 §4.3 逐应用数字(1.6µs/22.6µs/7.7µs/234ms)不重合——40µs 或为某平均口径或笔误;"三个数量级"仅对 Memcached(1.6µs vs RBV 90µs=56×)近似成立。两种读法记录,不强行调和。
- **Table 2 文本层交错**: 表格数字在提取层错行,经行内自洽(检出数/总数→百分比)交叉恢复;恢复后与正文 97.2–98.9%/98.3–99.8% 一致。
- **分布一致性**: 1:2:2:1(§A.1 四单元)与 1:2:2:1:1(§A.3.2 五单元含 TSX)为同一分布的附录扩展,非矛盾。
- **无下载戳**(ACM 直取,非 IEEE Xplore),无文本层大面积乱码。
