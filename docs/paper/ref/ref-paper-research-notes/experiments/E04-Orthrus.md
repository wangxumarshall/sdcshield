# Orthrus: Efficient and Timely Detection of Silent User Data Corruption in the Cloud with Resource-Adaptive Computation Validation(SOSP 2025,中科院计算所/国科大 + UCLA + UC Berkeley + 北京大学 + 清华)

- 作者:Chenxiao Liu、Zhenting Zhu(共同一作)、Quanxi Li、Yanwen Xia、Yifan Qiao、Xiangyun Deng、Youyou Lu、Tao Xie、Huimin Cui、Zidong Du、Harry Xu、Chenxi Wang
- 出处:SOSP '25,2025 年 10 月,Seoul。DOI: 10.1145/3731569.3764832
- 开源:**完整可用**(实测 2026-09-21 clone):https://github.com/ICTPLSys/Orthrus(运行时+性能评估)+ https://github.com/ICTPLSys/Orthrus-FaultInjection(llvm 注入框架);要求 ≥48 核、Ubuntu 18.04/20.04、CMake≥3.20、gcc-13/llvm-16。

## 1. 研究问题与核心贡献(≤5 行)

面向**云应用用户数据的实时 SDC 检测**(不是检出坏 CPU,而是检出被算坏的数据):利用云应用"控制路径/数据路径"清晰分离的结构,对数据路径算子(closure)做**异核异步重执行验证** + 控制路径用 16-bit CRC 校验和。编译器(LLVM)自动改写 + 资源自适应采样。开销 2%-6%(均值 ~4%),验证时延 1.6µs-234ms;注入实验:1 核验证检出 87%,2 核 91%,4 核 96%(等核上限 97%-99%)。

## 2. 实验方法(他们怎么做的)

### 2.1 核心观察:代码分区(数据流/非数据流判定算法,重点)

- **观察基础(Sec. 1 Insights / Sec. 2.2)**:典型云应用的代码库天然分离为两条路径:
  - **控制路径 control path**:实现调度、分发等控制逻辑,不操作用户数据——网络 IO(tcp_read/tcp_write/new_socket)、事件处理(event_handler)、命令解析分发(try_read_command_ascii/process_command_ascii)、统计(get_stats);代码量大:**Memcached 原始实现中控制路径代码量是数据路径的 20×+**(Sec. 2.2);该模式在全部被评估应用中一致出现;
  - **数据路径 data path**:对用户数据计算的算子(MapReduce 的 map/reduce/shuffle;KV/DB 的 insert/read/update;数据结构为 hash 表/B+ 树);**实现逻辑简单**——大应用的算子也多是简单函数,其实现代码大多可直接重执行(Sec. 1 Challenge 1)。
- **判定算法(人工注解 + 编译器自动推导的混合,不是全自动分区)**:
  1. 开发者标注**两类注解**(Sec. 3.1,Listing 5):`#pragma user-data`(类/结构体,代表用户数据结构:KVPair、Hashmap、Tree、Phoenix 的 hash_container/array_container)与 `#pragma closure`(数据算子函数:set/get/remove/map/reduce);
  2. 开发者显式指定**重执行范围**(closure 的作用域);**closure 的输入/输出由编译器自动识别**:输入 = 参数 + 可能访问的全局变量;输出 = 版本化内存中新生成的数据版本集合 + 返回值(Sec. 3.1);
  3. 编译器(LLVM IR 级,Sec. 3.2)两个 pass:Pass1 **类型推断**——找 user-data 类型变量,指针替换为 OrthrusPtr,读写改写为 load/store;Pass2 **逃逸分析**——生命周期超出 closure 的对象才进版本化内存,不逃逸的临时对象留私有堆(其损坏若不传播到版本化用户数据则不被检测,明确权衡);
  4. closure 必须遵循**单线程执行模型**(应用可多线程调算子,但每个算子实现本身单线程——多数真实应用已如此);多 closure 经 OrthrusPtr 访问共享数据:先获锁者创建新版本并记录 closure ID,后者再创建另一版本——两次访问与其输入版本都被记录以保忠实重执行(Sec. 3.1);
  5. 论文对"边界天然清晰"的依据:Memcached 的全部数据算子位于 4 个文件(assoc.c、hash.c、item.c、cache.c),只有它们操作用户数据(Sec. 2.2);移植 4 个应用平均只需修改 **<20 行代码**(Sec. 4.1)。
- **验证点插入位置(重点)**:
  1. **数据路径算子出口(核心验证点)**:每个 closure 执行完产生 **closure log**(Listing 6 结构:closure_id、core_id、closure_class 指针、inputs 指针、outputs 指针、可选 self_defined_validation、reserved(记录 syscall 结果)、start_time)→ 调度器**选一个与原执行不同的核**,把 log 推入该核的日志队列 → 验证器出队、在**相同输入与相同初始内存状态**(版本化快照)上重执行 closure → 比对输出(用户可重载 OrthrusPtr 的 `==` 做语义比较,否则**逐位比较**内存区域)(Sec. 3.3);
  2. **控制↔数据路径交界(校验和验证点)**:每个数据对象版本生成时(OrthrusNew 或 OrthrusPtr.store)**计算 16-bit CRC 存入版本头**;数据跨路径流动时校验——典型流(Fig. 3):客户端 generate_kv_pair 闭包生成 KVPair 并附 CRC → 网络传输(过控制路径不改数据)→ 服务端 set 闭包**首次 OrthrusPtr.load 时校验 CRC** → item.store 插入哈希表时生成**新 CRC** → 返回用户数据句柄给控制路径;任何中途损坏都会 CRC 失配(Sec. 3.4);
  3. **系统调用边界**:closure 内的 syscall(socket/随机数/时间戳/mutex)有副作用或非确定,**不重执行**——执行时拦截并把结果记入 log 的 reserved 字段,验证时直接重放记录值(Sec. 3.1/3.3);
  4. **Safe mode 验证点(可选)**:默认验证不阻塞结果返回;严格模式下会外漏数据的操作(如 Memcached 的 get;Phoenix 只在执行结束 reveal)等验证完成才返回(Sec. 3.5 Safe Mode)。

### 2.2 版本化内存(versioned memory)

- **内存组织(Fig. 2)**:应用堆分两区——**私有堆**(普通 new/delete 对象,不版本化)与**用户数据空间**(OrthrusNew/OrthrusDelete 分配,版本化);APP 与 VAL 是**两个进程**,各自私有堆 + **共享用户数据空间**(VAL 读共享空间、写只进自己私有堆);
- **OrthrusPtr(Sec. 3.1,Listing 4)**:类似 C++ 智能指针;`load()` 返回**不可变**数据;`store()` **原子、异地(out-of-place)更新**——创建新版本并自动记入 closure 的 closure log;
- **版本化的两大收益**:(1) 算子可在与原执行**完全相同的堆快照**上重执行;(2) 消除原执行与验证间的依赖 → **乱序验证**(日志自包含,无需与 APP 同步)→ 大幅提升验证吞吐;
- **验证读写规则(Sec. 3.3)**:验证器出队日志后,跟随输入/输出 OrthrusPtr 取数据;closure 重执行中 load 读共享用户数据空间或验证器私有堆;**store 一律写入验证器私有堆**(若原地址属共享空间,在私有空间找新位置写入并原子更新指针)——避免污染应用堆;验证器私有堆不版本化(无需再验证);
- **结果比较(Sec. 3.3)**:用户可重载输出 OrthrusPtr 的 `==` 做语义等价比较;未定义则**逐位比较**两内存区域;
- **日志分配器**:cache-locality-aware 的 log allocator 优化日志性能(细节因篇幅省略,Sec. 3.1 末);
- **CRC 细节(Sec. 3.4)**:16-bit CRC 存于**版本头**;每次版本生成(OrthrusNew 或 store)时计算;数据对象跨控制/数据路径流动时校验;选 16-bit 的理由:仅用于完整性校验而非纠错,足够;开销:每对象几十个 CPU 周期,评估中 **~1%** 性能开销(SSE4.2 加速)。

### 2.3 编译器(Sec. 3.2)

- LLVM IR 级转换:找 user-data 注解类的全部分配点 → 继承 OrthrusObj(内含 checksum 字段)→ 替换为 OrthrusNew;
- 每个 closure 两个 pass:Pass1 类型推断(user-data 类型变量指针 → OrthrusPtr,读写 → load/store);Pass2 逃逸分析。

### 2.4 资源自适应机制(Sec. 3.5 + 3.6,重点)

#### 2.4.1 调度与核放置

- 验证器基于 **Shenango** 用户态线程(微秒级核重分配);**NUMA 感知**;
- **异核放置的依据**:静默计算错误会在相同硬件组件(ALU/FPU/向量单元)上复发(引 [30][39][71]),因此调度器**防止验证任务与 APP 线程共享这些计算单元**;在 Intel 平台(ALU/FPU/向量单元核私有)同 CPU(NUMA 节点)内不同核即安全;同 CPU 共置还便于日志共享(微秒级访问、提升 L3 命中、提高验证吞吐降时延)(Sec. 3.5 Scheduling Policy);
- 调度策略用户可配置(如检测缓存一致性问题),前提是检测可通过结果比较实现。

#### 2.4.2 采样策略(资源不足时)

- **目标函数**:最大化**代码覆盖**(验证尽可能多的不同代码路径);
- **机制**(三层优先级信号):
  1. **时间戳新鲜度**:每 closure 维护"上次被验证时间戳",超过定义阈值者优先(依据 [33][42]:CPU 错误高可复现、与特定指令相关而非瞬态——多次验证无误的算子未来执行大概率也无 SDC);
  2. **(closure, caller) 对粒度**:单一 closure 可被不同调用者以不同上下文调用、走不同控制路径——按唯一 (closure, caller) 对维护时间戳,近期高频调用的对被选中概率降低;
  3. **指令类型加权**:编译期检测并**标记含 fp/vector 指令的 closure**,验证优先级升高(依据 [30][38][71]:CPU 错误与 fp/vector 指令强相关);
- **自适应降档**:Orthrus 初始**验证全部 closure**;验证进程以低执行优先级运行,先试图用满全部空闲核(可被 APP 线程随时抢占);若**排队时延超过预定义阈值**,以增量逐步降采样率直到时延回到可接受范围(Sec. 3.5 Sampling)。

#### 2.4.3 扩缩容与负载均衡

- 单验证线程起步,周期扫描全部日志队列;
- **扩容触发**:每个 closure 跟踪其平均验证时延(最近 **8** 个已验证日志上的均值,即 enqueue 到验证完成的时延);若该 closure 特定时延超过**全部日志平均时延的 50%**(即 `> 1.5×全局均值`),通知调度器**再起一个 Shenango 线程分担其一半日志队列**(Sec. 3.5 Dynamic Scaling);
- **work-stealing**:乱序验证提升吞吐但引入尾时延(滞留日志的错误会向依赖其后继传播而后继验证测不到——输入假定正确),窃取平衡负载消尾时延。

#### 2.4.4 Safe mode

- 默认:验证不阻塞结果返回(SDC 稀有,避免不必要时延;代价是可能外漏已损坏输出);
- 严格安全模式(可选):会外漏数据的操作等验证完成;只有少数操作需要(每应用几个),验证开销 <2% 总执行时间。

#### 2.4.5 内存回收(Sec. 3.6)

- 问题:版本化内存膨胀;传统 GC 不适用(C/C++ 非托管),引用计数原子操作开销高(内存密集负载可达 13%);
- **可见窗口**(数据版本):从 OrthrusPtr.store 创建到同对象下一版本创建(或 delete)——前向执行不再见旧版本;
- **活跃窗口**(closure):从 APP 开始执行到验证完成;
- **近似算法**:验证器每完成一个 closure 验证,在"正在执行的 + 正在验证的 + 等待验证的"**合并队列**中找**最早开始时间 t**(队列按开始时间排序,最早的在尾);**可见窗口结束于 t 之前的所有数据版本可安全回收**;异步批量回收;
- 效果:内存开销压到 **~20%**(Sec. 3.6 末,引 Sec. 4.2 各应用 2.6%-35% 实测)。

### 2.5 故障注入框架(附录 A,重点)

- **为什么自研**:真实 SDC 硬件市面上买不到;现有框架(LLFI、REFINE)随机注入指令级故障且只按程序输出分类 SDC;Orthrus 基于 **REFINE(SC'17)** 扩展,并把故障模型扩展到覆盖 Alibaba SOSP23 [71] 报告的全部四类脆弱 CPU 特性(Sec. A 开头);
- **注入层级**:**LLVM Machine-IR(MIR)级**(比 IR 级准——REFINE 证明 IR 级因看不到动态指令而不准);实现为 **Clang 16.0.6 X86 后端的 fault-injection machine function pass(C++ ~4376 行)+ Python 自动化测试平台(~2118 行)**(Sec. A.4);
- **配置驱动**:外部配置文件指定目标机器指令与故障类型;无配置时默认全故障类型均匀随机注入(Sec. A.3.1);
- **注入故障类型(4 种)**:bitflip(位取反)、stuckat0(置 0)、stuckat1(置 1)、nop(移除指令)(Sec. A.1 末,引 [32]);
- **单元分布模型(对齐真实世界)**:按 Alibaba 生产环境观察到的 SDC 分布,执行单元配比 **ALU:SIMD:FPU:cache = 1:2:2:1**(含 TSX 时 1:2:2:1:1)(Sec. A.2);**示例**:60 个故障注入含 1000 条 ALU + 1000 条 SIMD 指令的程序 → ALU 20 个、SIMD 40 个(Sec. A.3.2);
- **三阶段工作流(Sec. A.3.2)**:
  1. **Inspection**:编译程序,自定义 LLVM 后端 pass 从配置文件加载目标函数,提取指令明细;
  2. **Profiling**:确定哪些指令**实际被执行**——把目标函数指令替换为 INT3,执行改造二进制,触发预期 SIGTRAP 即标记"已执行";同时按 **x86 MIR opcode/operand 规则**做硬件单元分类:(1) 原子原语之间或 lock 调用之间的指令 = **cache 一致性单元**;(2) SSE/X87/MMX 等 opcode = **FPU**;(3) AVX/CRC = **vector**;(4) 其余 = **ALU**;
  3. **Injection**:按单元配额插入故障(如 bitflip 的 MIR 实现:PUSH rax / MOV rax, imm32(掩码)/ XOR edi, rax / POP rax,见 Listing 10),构建注入二进制,执行,按**返回码 + 输出**分类三型:Fail-Stop(崩溃/终止)/ Masked(不影响输出)/ SDC(改输出且无异常)(Sec. A.1);评估只关心 SDC;
- 注入**同时打进控制路径和数据路径**(Sec. 4.4:"injecting instruction-level errors into both the control and data paths")。

### 2.6 与 RBV / ILV 的对比定位(Sec. 1 State of the Art)

- **RBV(复制式验证)**:主实例 + 副本跑在隔离环境(如独立服务器),消费 **>100% CPU 与内存**;跨服务器同步引入额外开销;验证需按序(数据依赖时)——Orthrus 实验 RBV 时间 2.0×、内存 2.1×;
- **ILV(指令级验证)**:逐周期同步与比较,**~50× 性能减速**且需专用硬件(云环境没有)——被排除出实验评估(Sec. 4.1 Baselines);
- **Orthrus 的位置**:两者之间的实用点——开销 ~4%,检出率略低于 RBV(97.2-98.9% vs 98.3-99.8%),时延低 2-3 个数量级;offline 测试(如 Google cpu-check、Alibaba/Meta 舰队测试)检的是坏 CPU 而非已坏的数据,损害发生在测试间隔内——Orthrus 检的是数据本身(Sec. 1 / Sec. 5)。

## 3. 实验配置

| 项 | 配置(原文) |
|---|---|
| 硬件 | 3 台服务器集群,每台 **2× Intel Xeon Gold 6342**(24 核/颗,共 48 核/台),**100 Gbps Mellanox ConnectX-5 InfiniBand** 互联(Sec. 4.1) |
| OS | Ubuntu 18.04,内核 5.14(正文);artifact 推荐 Ubuntu 20.04(Sec. 4.1 / 附录 B) |
| 性能控制 | **关闭 Turbo Boost、C-states、CPU 频率调节**(循 Shenango 惯例)(Sec. 4.1) |
| 编译器 | C/C++;注入框架为改版 Clang 16.0.6;运行时要求 gcc-13/llvm-16(init.sh) |
| 应用 | Memcached(内存对象缓存)、Masstree(多核 KV)、LSMTree(LSM 树,类 RocksDB)、Phoenix(MapReduce 框架)(Table 1) |
| 数据集(Table 1) | Memcached:Meta **CacheLib**,150 MOP,倾斜+churn;Masstree:**ALEX** 负载,200 MOP,读密集;LSMTree:**YCSB synthetic**,50 MOP,写密集;Phoenix:**WMT 2024 英文新闻子集**,15 GB,word count |
| 线程配置 | Memcached:客户端 32 线程(独立机)/服务器 4 线程;Masstree:服务器 4 核/客户端 16 核;Phoenix:map+reduce 各 16 核;LSMTree:写密集(大内存缓冲抑制刷盘) |
| 注入量(Table 2 合计) | Memcached 471(132+0+217+122)、Masstree 518(145+0+163+210)、LSMTree 818(126+337+157+198)、Phoenix 684(227+244+213+0)个 SDC(按 4 类错误型分列) |
| 验证核数 | 性能/时延实验固定 2 核验证;覆盖实验变 1/2/4 核 + 等核上限(Sec. 4.1 CPU Resources) |
| 内存约束实验 | 固定 2 核,允许超 vanilla 足迹 5%-40% 额外内存,采样触发器从时延切到内存容量(Sec. 4.4) |

## 4. 实验步骤(可操作流程)

**性能实验(Fig. 6-8,对应仓库 justfile 命令,实测 docs/exp-*.md 可查每步时长):**
1. 三台服务器:client / 应用服务器 / RBV 副本服务器;部署 vanilla、Orthrus、RBV 三版本应用;
   - RBV 基线构造(Sec. 4.1 Baselines):副本跑独立服务器(避免共享硬/软件状态);全部 client 请求发给 primary,primary 批量按处理序转发请求+输出给副本;副本执行请求并与 primary 结果比对;失配即发信号中断 primary;**RBV 分配与应用相同核数**(远多于 Orthrus 验证器);
   - 各应用部署参数:Memcached(client 32 线程/服务器 4 线程,CacheLib 数据集);Masstree(服务器 4 核 / client 16 核,ALEX 负载 50% range query + 50% update);Phoenix(map+reduce 各 16 核,WMT 2024 英文新闻 word count);LSMTree(100% 随机写,大内存缓冲抑制 SSTable 刷盘,只测内存层 skip list);
2. 系统设置:关闭 Turbo Boost、C-states、CPU 调频(循 Shenango);
3. 吞吐/时长测量:Memcached/Masstree/LSMTree 测吞吐(Kops),Phoenix 测总时长(s)(Fig. 6);
4. 尾时延:95% tail latency 随吞吐扫描(Fig. 7);
5. 验证时延分布:closure 执行完成 → 验证完成的时延 CDF,Orthrus vs RBV(Fig. 8);
6. 内存:vanilla 对照测应用内存开销(Memcached +29%、Phoenix +2.6%、Masstree +35%、LSMTree +34%);
7. 开销分解:checksum 开销单独测(<1%);OrthrusPtr 数据管理(内存 +7%/运行时 +3%);1 核限制下内存消耗平均再降 36%;
8. 仓库复现命令:`just test-all`(全套 ~7 小时)或 docker-compose;单实验如 `just test-memcached-throughtput`(~20 分钟)。

**覆盖实验(Table 2 / Fig. 9 / Fig. 10):**
9. 用注入框架对每个应用做 Inspection(提取指令)→ Profiling(INT3/SIGTRAP 标记实际执行指令 + x86 MIR 四类单元分类)→ Injection(按 ALU:SIMD:FPU:cache=1:2:2:1 配额注入 bitflip/stuckat0/stuckat1/nop);
10. 运行注入二进制,按返回码+输出分类出 SDC(排除 Fail-Stop/Masked);
11. **等核上限实验**:验证器核数=应用核数(承认生产不可行,数字是上界);统计 RBV vs Orthrus 各错误型(Arithmetic/Floating point/Vector/Cache)检出数与百分比(Table 2);
12. **限核实验**:验证器 1/2/3/4 核(Phoenix 1/3/5/7 核),Orthrus 自适应采样 vs **无引导随机采样**对照(Fig. 9);
13. **限内存实验**:固定 2 核,允许超 vanilla 足迹 5%-40% 额外内存;采样触发器从检测时延切换到可用内存容量;写入突发超限即激活采样(Fig. 10);
14. 仓库复现:`table2_setup.sh` 装 LLVM + Python 环境;`table2_fastcheck.sh` 快速版 / `table2_full.sh` 完整版(数天);预生成注入数据集 fault_injection.tar.gz(releases tag data-fi)。

## 5. 实验数据(关键数值)

### 5.1 性能(Fig. 6,Sec. 4.2)

| 应用 | Vanilla | Orthrus | RBV | Orthrus 相对 |
|---|---|---|---|---|
| Memcached 吞吐(Kops) | 407 | 389(**-4.4%**) | 244 | **1.6×** vs RBV |
| Masstree 吞吐(Kops) | 596 | 559(-6.2%) | 193 | **2.9×** vs RBV(RBV 比 Orthrus 低 65%) |
| LSMTree 吞吐(Kops) | 188 | 179(**-5%**) | 116 | RBV 落后 Orthrus 54% |
| Phoenix 时长(s) | 28.9 | 29.4(**+2%内**) | 43.7(RBV 吞吐 -51%) | 1.5× vs RBV |

- 总体:**时间开销 ~4%,内存 ~25%**(摘要口径 2%-6%);RBV 时间 **2.0×**、内存 **2.1×**;
- **Memcached**(读密集,GET 为主不产生版本):Orthrus 仅 -4.4%(日志生成开销);内存 +29%(= RBV 内存开销的 1/7.3);RBV 43% CPU 用于通信(即使批处理);RBV 尾时延最坏比 Orthrus 差 **1000×**(排队时延,Fig. 7);
- **Phoenix**(大批量数据 → 日志条目与版本对象少):开销 <2%;内存仅 +2.6%(RBV 2.1×);RBV -51% 吞吐(数据密集:大结果集网络传输 + 复杂结构等价性检查贵);
- **Masstree**(乐观并发 + 复杂树结构):RBV 需记录执行顺序、副本被迫串行处理 → 吞吐比 Orthrus 低 65%、长尾恶化;Orthrus 乱序验证 → 2.9× 优势;Orthrus 内存 +35%;
- **LSMTree**(100% 随机写,极端压力场景,论文自认"不现实但故意选来压测"):Orthrus 达 vanilla 95% 吞吐;内存 +34%;RBV 落后 54%;
- 开销分解:checksum **<1%**(SSE4.2);OrthrusPtr 数据管理平均内存 +7%、运行时 +3%(可用池分配器缓解碎片,粗粒度注解合并对象,留作 future work)。

### 5.2 验证时延(Fig. 8,Sec. 4.3;时延定义 = closure 执行完成 → 验证完成)

| 应用 | Orthrus 平均 | RBV 平均 | 差距 |
|---|---|---|---|
| Memcached | **1.6 µs** | 90 µs | 56× |
| Phoenix | 234 ms(RBV 平均 513 ms,尾 1100 ms) | 513 ms | ~2.2× |
| Masstree | 22.6 µs | — | 21× |
| LSMTree | 7.7 µs | — | 8× |

- 摘要口径:平均时延 40µs,比 RBV 低**三个数量级**(延迟关键应用上 2-3 个数量级);
- 三个来源:(1) RBV 数据依赖时必须按序验证,Orthrus 版本化 → 乱序;(2) Orthrus 只验证数据路径,RBV 重执行整个程序;(3) Orthrus 共享内存日志传递 vs RBV 网络同步;
- Memcached:请求小,RBV 请求级验证 90µs 已算低,但仍为 Orthrus 的 56×;倾斜负载下操作间有数据依赖,RBV 每操作需跨服务器通信同步;
- Phoenix:单操作(map/reduce)可长达 240ms;RBV 要等大输出序列化并传给副本 → 平均 513ms、尾 1100ms;Orthrus 共享用户数据空间 + 逐位比较 → 平均 234ms;
- Masstree:RBV 需严格同步副本执行顺序(乐观并发),通信开销 + 并行受限 + 树结构比较贵 → Orthrus 21× 优势;
- LSMTree:差距最小(8×)——随机写数据依赖少,RBV 同步负担轻。

| 应用 | Orthrus 平均 | RBV 平均 | 差距 |
|---|---|---|---|
| Memcached | **1.6 µs** | 90 µs | 56× |
| Phoenix | 234 ms(尾 1100ms 以下;RBV 平均 513 ms) | 513 ms | ~2.2× |
| Masstree | 22.6 µs | — | 21× |
| LSMTree | 7.7 µs | — | 8× |

(摘要口径:平均时延 40µs,比 RBV 低三个数量级。)

### 5.3 覆盖率(Table 2,Fig. 9/10,重点)

**等核上限(Table 2,验证核数=应用核数)**——四应用 × 四错误型(Arithmetic/Floating point/Vector/Cache)的 Total SDCs 与检出数(%):

| 应用 | 错误型 | Total SDCs | RBV 检出 | Orthrus 检出 |
|---|---|---|---|---|
| Memcached | Arithmetic | 132 | 130(98%) | 126(95%) |
| Memcached | Floating point | 0 | 0 | 0 |
| Memcached | Vector | 217 | 216(99%) | 213(98%) |
| Memcached | Cache | 122 | 122(100%) | 120(98%) |
| Masstree | Arithmetic | 145 | 143(99%) | 140(97%) |
| Masstree | Floating point | 0 | 0 | 0 |
| Masstree | Vector | 163 | 163(100%) | 160(98%) |
| Masstree | Cache | 210 | 209(99%) | 209(99%) |
| LSMTree | Arithmetic | 126 | 123(98%) | 123(98%) |
| LSMTree | Floating point | 337 | 334(99%) | 330(98%) |
| LSMTree | Vector | 157 | 155(99%) | 151(96%) |
| LSMTree | Cache | 198 | 198(100%) | 195(98%) |
| Phoenix | Arithmetic | 227 | 223(98%) | 219(96%) |
| Phoenix | Floating point | 244 | 244(100%) | 237(97%) |
| Phoenix | Vector | 213 | 212(99%) | 208(98%) |
| Phoenix | Cache | 0 | 0 | 0 |

- **按错误型聚合(四应用合并)**:RBV **98.3%(Arithmetic)/99.5%(FP)/99.5%(Vector)/99.8%(Cache)**;Orthrus **97.2%/97.6%/97.6%/98.9%**;
- **差距解剖**:RBV 比 Orthrus 多检出的 SDC 中,除 **3 个**(syscall 内错误:write 一个、mutex 两个——对应 §2.3 局限 2)外**全部起源于控制路径**——Orthrus 的 checksum 机制设计上不覆盖(对应 §2.3 局限 3);RBV 能抓控制路径分支错误导致程序提前返回/跳过本应执行的数据操作;
- 注意 Memcached/Masstree 的 FP=0、Phoenix 的 Cache=0:注入分类决定哪些应用命中哪些错误型(负载路径决定),空格是负载属性不是漏检。

**限核(Fig. 9,Sec. 4.4)**:
- **1 核:平均检出 86.7%(摘要口径 87%);2 核 91%;4 核 96%**;
- 1 核时 Orthrus 自适应采样仍比**无引导随机采样**高 **1.41×**(同图对照线);
- **Memcached** 1 核不降(不到 1 核算力就足够验证全部执行);
- **Phoenix** 掉最狠(1 核 **78.2%**):16 线程高并行 → closure 洪峰;word count 内存密集 → 验证时用户数据比较耗算力;
- **Masstree** 最稳(4→1 核只降 **8.6%**):访问倾斜使大量 closure 共享相似调用上下文 → 自适应采样(按 (closure,caller) 对)效率高;
- 限核下**内存消耗平均降 36%**(closure 更快被"验证或跳过",日志不积压、回收不延迟);应用性能与验证时延在采样下基本不变。

**限内存(Fig. 10,Sec. 4.4,2 核 + 5%-40% 额外内存)**:
- **Phoenix 最稳**:平均 91%(额外内存 15% 时);压到 5% 仍平均 **92.7%**(读密集 → 版本少;数据依赖比树结构简单 → 验证/回收吞吐高);
- **Masstree 最敏感**:检出率随内存收紧**持续下降**——树结构下少量写也触发大量版本更新;未验证 closure 依赖拖慢回收;高采样率 + 2 核验证器跟不上 → 长尾验证时延 + 内存堆积 → 采样率进一步降;
- **Memcached**:单核验证吞吐已高,内存限制对采样率影响轻微。

## 6. 实验结论(论文自述)

1. 控制路径/数据路径分离 + 数据路径重执行 + 控制路径校验和的混合方案,把 SDC 检测开销降到 ~4%(RBV 是 2.0×),实用可部署(Sec. 4.2)。
2. 版本化内存 + 乱序验证使验证时延比 RBV 低 2-3 个数量级(1.6µs 级)(Sec. 4.3)。
3. 资源自适应采样有效:1 核即达 87%,4 核 96%,显著优于随机采样(1.41×)(Sec. 4.4)。
4. 与 RBV 的覆盖差距(97.2-98.9% vs 98.3-99.8%)几乎全部来自控制路径错误与系统调用内错误——设计边界内的已知损失(Sec. 4.4)。
5. masked error 不影响用户数据,无需检测;syscall 占执行指令仅 0.04%,被错误命中概率极低(Sec. 2.3)。
6. 对持久性硬错误,同核重放(PASC/SEI 类)无效,必须异核验证——设计动机之一(Sec. 5 Related Work)。
7. 明确的 Non-goals:不检测 mercurial 核本身、不提供覆盖保证、不容忍错误(检出即中止应用,防脏数据外泄)(Sec. 1)。

## 7. 复现要点(ARM64 单机/小集群最小可行版本)

### 可直接复用

| 组件 | 说明 |
|---|---|
| **官方开源 artifact(实测完整)** | Orthrus 仓库含运行时(include/lib)、4 应用、justfile 全套命令、docker-compose;FaultInjection 仓库含改造 LLVM + Python 平台 + 预生成注入数据集。x86 机器可直接跑 `just test-all`(~7h)与 table2 注入实验 |
| 代码分区方法论(控制/数据路径 + closure 注解) | 语言无关:任何有"算子"结构的应用(memcached/redis/数据库 KV 路径)都可手工分区;ARM64 上同样成立 |
| 数据路径异核重执行验证 | 单机即可:APP 线程与验证线程绑不同核(sched_setaffinity),闭包输入/输出落日志,验证线程重放比对——最小可行版不需要版本化内存,可用"输入快照拷贝"简化(牺牲乱序验证) |
| 16-bit CRC 控制路径校验 | ARM64 有 CRC32 指令(ARMv8.0 CRC32/CRC32X),比 x86 SSE4.2 更原生;开销同样 <1% 量级 |
| (closure, caller) 对采样 + fp/vector 优先级 | 纯策略代码,直接移植;SDCShield 场景可改造成"哪些测试/指令段优先重验" |
| 故障注入的单元配比(1:2:2:1)与 4 故障类型 | 直接引用 Alibaba 真实分布;在 ARM64 上用 MIR 级(LLVM aarch64 后端)复刻——方法论声明"架构无关"(附录 A.4) |
| 关闭 Turbo/C-states/调频的实验纪律 | Kunpeng 无 cpufreq(已知 quirk),但 C-state/高性能 governor 可控;复刻其性能控制声明 |
| 资源自适应(时延驱动采样 + work stealing) | 单机多核可完整复刻;Shenango 可换成普通线程池(牺牲微秒级扩缩容) |

### 需替代/不可行

| 组件 | 障碍 | 替代方案 |
|---|---|---|
| x86 注入框架直接用 | Clang 后端 pass 是 x86 专属(单元分类规则按 x86 MIR:SSE/X87/MMX/AVX/CRC) | 按 aarch64 MIR 重写分类规则:NEON/SVE 指令=Vector,FP 指令=FPU,LDXR/STXR/原子=LSE/cache 一致性,其余=ALU;LLVM aarch64 后端同构可行 |
| 3 台 100Gbps IB 服务器 | 单机可跑大部分:Orthrus 本身是同机双进程;只有 RBV 基线需要跨机(避免共享硬件状态)→ 用 2 台普通以太网机替代,或接受 RBV 基线降级为同机隔离(cgroup/VM)——注明与论文口径差异 |
| ≥48 核 | Kunpeng 920(192 核)满足且超额 | 无需替代,反而是优势 |
| Ubuntu 18.04/20.04 + gcc-13/llvm-16 | 环境是 openEuler 24.03 | 容器化(docker-compose 已提供);或源码构建 llvm-16 |
| Shenango 微秒级线程运行时 | 对 ARM64 支持未验证 | 换 std::thread + 条件变量;丢微秒级扩缩容,标注性能差异来源 |
| CacheLib/ALEX/WMT 数据集 | CacheLib 数据集是 Meta 内部 traces | 用 YCSB 替代 Memcached 负载;ALEX 有开源;WMT 是公开的 |
| 版本化内存全套(OrthrusPtr/逃逸分析/可见窗口 GC) | 工程量大(编译器 pass + 运行时) | 最小版:闭包输入深拷贝 + 同步验证(不做乱序),先验证概念再谈性能 |

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | Orthrus 基线数值(图表号) | 你的实验可怎么比 |
|---|---|---|
| 时间开销 | ~4%(2%-6%);各应用 -4.4%/-6.2%/-5%/+2%(Fig. 6);ABFT-SEVI 是 1.35%-11% | 你的在线检测机制开销;或你的离线测试"等效检出下的机器时间成本" |
| 检出率-资源曲线 | 1 核 87% / 2 核 91% / 4 核 96% / 等核 97.2-98.9%(Fig. 9,Table 2);随机采样对照低 1.41× | 你的套件在注入 SDC 上的检出率 vs 占用核数曲线;自适应选取 vs 随机选测试的增益 |
| 错误型分解覆盖 | Arithmetic 97.2%/FP 97.6%/Vector 97.6%/Cache 98.9%(等核)(Table 2 聚合) | 你的注入按同四类分解的检出率——**最可直接对齐的口径**(Alibaba 四类 + 1:2:2:1 配比) |
| 验证时延 | 1.6µs-234ms,均值 40µs;比 RBV 低 2-3 个数量级(Fig. 8) | 你的"触发→报告"时延分布 |
| 内存开销 | ~25% 均值;2.6%-35% 按应用(Fig. 6 讨论) | 你的测试内存足迹 |
| RBV 参照点 | RBV:2.0× 时间/2.1× 内存/98.3-99.8% 检出 | RBV 是你实验的"资源无限制上界"参照;SDCShield 式离线测试是"无在线开销"参照——你的方案在两者之间找帕累托点 |
| 控制路径盲区 | RBV-Orthrus 差距几乎全在控制路径错误 + 3 个 syscall 错误(Sec. 4.4) | 你的方案若能覆盖控制路径错误(如分支错误)即填补其盲区 |

**最有对比价值的三点**:(1) Table 2 的四错误型 × 检出率矩阵——注入口径完全可复制(1:2:2:1 配比),你的 ARM64 注入实验可直接同表对比;(2) 1/2/4 核的检出率-资源曲线(Fig. 9)——你的"测试强度 vs 检出"曲线可叠加其上;(3) 4% 开销/40µs 时延——若你的方案是离线测试范式,可比"占用机器时间总量÷检出数"的成本效益。

## 9. 局限与坑(论文自认 + 读出的坑)

**论文自认局限(Sec. 2.3 "What Orthrus Cannot Detect" 五条 + 其它):**
1. 检不出 masked error(不影响结果的错误)——按定义不关心;
2. 系统调用/同步原语/外设交互内的错误不能直接验证(只重放记录结果);profiling 显示此类指令仅占 0.04%,但这是统计论证非保证;
3. 控制路径错误改变控制流(如比较指令错导致调错算子,该 get 时调 insert)时 CRC 可能漏检;
4. APP 与 VAL 两个核产生"完全相同方式"的损坏时无法检出(概率论证);
5. 采样必然漏掉未选中闭包的错误——best-effort 定位,不提供覆盖保证(Sec. 1 Goals/Non-goals 明示:不检测 mercurial 核本身、不提供覆盖保证、不容忍错误);
6. 逃逸分析放置在私有堆的临时对象损坏不被检测,除非传播到版本化用户数据(Sec. 3.2);
7. 等核检出率是上界,生产不可能给这么多核(Sec. 4.4 自己声明);
8. 单线程闭包执行模型是前提(多数应用满足,但非全部)(Sec. 3.1);
9. 资源不足时检出率由采样决定——资源-覆盖的权衡是内生属性(Fig. 9/10 全景展示)。

**读出的坑:**
1. **注入模型的代表性依赖单点来源**:1:2:2:1 配比与四错误型全部取自 Alibaba SOSP23 [71] 单家观察;x86 注入模式(REFINE)与真实 SDC 的位翻分布的等价性未验证——SEVI(Obs. 8)实测真实 FMA SDC 的位翻是 LELM/LEHM/HEHM 模式(还有符号位翻转),与 bitflip/stuckat/nop 注入的差异未被讨论。
2. **MIR 级注入仍是软件模拟**:注入改变的是指令语义结果,不触及真实微架构状态(时序、电压、cache 状态)——与 PinDrop 强调的 marginal defect(温度/频率/电压组合触发)机制不同层;用它证明"检出能力"对真实延迟缺陷的外推有限。
3. **LLVM 16.0.6 x86 后端专属**:分类规则(如 CRC 归入 Vector、AVX+CRC 同桶)是 x86 便利选择,移植需重写;论文称"methodology is architecture-independent"但未给 ARM 实现。
4. **表 2 的样本量不大**(每格 0-337 个 SDC),部分格(Memcached FP=0、Phoenix Cache=0)无数据;百分比无置信区间。
5. **RBV 基线的实现强度**:RBV 用 TCP/IP 跨服务器通信 + 顺序化验证,是其性能差的机制来源;更优的 RBV(共享内存、宽松同步)会缩小差距——基线选择对 1.5-2.9× 的优势数字影响大(Memcached-RBV 43% CPU 用于通信是具体实现属性)。
6. **验证时延的口径差**:Phoenix 234ms 平均被"操作本身可达 240ms"合理化,但摘要的"40µs 平均"混合了跨 5 个数量级的分布,引用时要按应用分列。
7. **CRC 16-bit 的碰撞**:控制路径只防数据损坏,16-bit CRC 对多 bit 翻转的漏检率 ~2⁻¹⁶,在多位翻为主的真实 SDC(PinDrop Obs. 11)下漏检概率不可忽略——论文未讨论多位翻对 16-bit CRC 的具体影响。
8. **SOSP 口径的"检出率"依赖应用负载路径覆盖**:注入只命中被执行的指令(Profiling 阶段以 INT3 实际执行为准),死代码注入不产生 SDC——检出率数字与负载选择强耦合。
9. **资源自适应的最优性无理论保证**:采样启发式(时间戳阈值 + 指令类型加权 + 50% 时延阈值 + 最近 8 个日志均值)是一堆魔数,无收敛性/覆盖率界分析;1.41× vs 随机采样的提升只在其实验负载上成立。
10. **开源 artifact 的硬件门槛**(≥48 核、双 socket、IB)与 Ubuntu 20.04 依赖:跨环境复现需要容器化改造(仓库已提供 docker-compose,算是缓解)。
11. **四应用全是"数据路径清晰"的精选应用**:Memcached/Masstree/LSMTree/Phoenix 都是 KV/MapReduce 型;数据路径与控制路径纠缠的应用(如 Web 服务、编译器、游戏)的注解成本未评估——"<20 行修改"的外推性存疑。
12. **2 核验证器的性能/时延结论依赖应用核数少**(4-16 核):在 192 核 Kunpeng 上验证洪峰与队列时延特性会完全不同;50% 时延阈值的扩容触发在众核下是否够快无数据。
13. **版本化内存的写放大**:LSMTree 100% 随机写下内存 +34%、Masstree +35%——写密集负载的版本创建是 O(写次数) 的额外分配;论文用"压力场景"话术带过,但生产写密集负载(如 Cassandra)正是其主要目标场景。

## 9b. 与 SDCShield 的关系定位(给论文写作的坐标)

- Orthrus 解决的是"**数据被算坏了怎么及时发现**"(在线、应用内、best-effort);SDCShield 解决的是"**这个核/这片硅有没有缺陷**"(离线、系统级、确定性 golden 比对)——两者是互补层,不是竞争关系;
- 你的新激发实验若声称优于离线测试基线,Orthrus 不是直接对手;但它提供了:(1) 注入空间的标准配比(1:2:2:1 + 四错误型)——你的注入实验应采用同口径以便互引;(2) "检出率 vs 资源"曲线的呈现范式(Fig. 9);(3) fp/vector 优先级的采样思想(可反向用于测试调度:优先跑最易 SDC 的指令族);
- Orthrus 的 ARM64 空缺(aarch64 MIR 注入 + NEON/SVE 单元分类)本身就是一个可发表的贡献点。
