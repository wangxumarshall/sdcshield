# docs_xu — xu 的研究工作区

> **归属**：本目录是 xu 的**专属**研究工作区（多人共用此机器/仓库，"将文档放入 docs_xu"的指令只来自 xu 本人）。其他人的研究内容请勿放入本目录。
> **用途**：记录后续研究内容（2026-09-18 建立）。
> **动机**：机器可能自己关机 / 网络不稳定导致对话中断，所有关键内容及时落盘到仓库，新会话可凭此恢复上下文。
> **配套**：sdcshield 通用参考文档（使用指南 / 如何添加测试用例）在 [../research/](../research/)，两处互补。

## ⚡ 当前活动任务（高优先级）

**[122 号核心 SDC 故障狩猎](2026-09-19-cpu122-sdc-hunt-mission.md)** — CPU 122 存在 SDC 故障（唯一故障核心）。目标：写出能快速检出它的测试用例 + 分析什么负载最快触发。**强制流程**：每次新写/改写测试 → ① 热插拔 offline 122 → ② 其余全核跑 60s 必须 100% pass（fail=我的代码 bug）→ ③ online 122 → ④ 全核跑验证 fail 精确锁定 cpu 122。任务纲领详见链接文档。

## 约定

- 每个新研究主题单独建一个 `.md` 文件，并登记到下表索引。
- 每次会话的关键进展追加到「进度日志」，断点续传以本文件为入口。

## 索引

| 文档 | 主题 | 状态 |
|---|---|---|
| [2026-09-19-cpu122-sdc-hunt-mission.md](2026-09-19-cpu122-sdc-hunt-mission.md) | 【高优先级·活动】122 号核心 SDC 狩猎任务纲领：热插拔两阶段验证流程、核心判据、待研究问题 | **活动** |
| [2026-09-18-sve-capability-research.md](2026-09-18-sve-capability-research.md) | SVE 指令测试可行性：本机 SVE 实测（cortex x3b，VL=256bit）、FMMLA/BFDOT/SDOT/USMMLA/FCMLA 逐条 golden 验证、FCMLA 语义澄清（差点误判硬件 SDC 的完整证据链） | 完成 |
| [2026-09-19-avx-named-tests-audit.md](2026-09-19-avx-named-tests-audit.md) | 53 个 avx 命名测试源码审计：三层证据证明全部无 SVE 代码（旧机器 NEON 版），`tests/cpu/sve/` 覆盖缺口分析 | 完成 |
| [2026-09-19-avx53-workload-environments.md](2026-09-19-avx53-workload-environments.md) | 53 个测试的负载环境实录（框架模式+FMA 11+Mesh 18+IPSec 23 逐一参数表）——SVE 版移植的负载基准；含 OpenSSL SVE 路径调查（仅 ChaCha20 有 SVE）与 3 个开放决策问题 | 完成 |
| [2026-09-20-95tests-sve-improvability-v2.md](2026-09-20-95tests-sve-improvability-v2.md) | 95 个测试的 SVE 可转换性分析 v2（修正口径：计算负载视角而非微架构特征视角） | 完成 |
| [2026-09-20-64tests-batch-plan.md](2026-09-20-64tests-batch-plan.md) | **64 个可转换未转换测试的名单 + 7 批次转换计划**（kreg谓词→NEON直换→换算→进位链→core-179向量版→访存谓词优势→库自实现） | **待执行** |

## 进度日志

- **2026-09-21（会话 12，批次 2 完成）**：**批次 2 完成并推送**（13 个 NEON 直换，比计划 12 多出 insert_extract）。FMA/FPU/misc/vector 四域：neon_add/fma/fpu_special_values/power_virus_dit/movdq2q/movq2dq/movmskpspd/fsu_byteexact + kreg1/4/7 + swizzle(svtbl 字节表置换)+insert_extract。**4 个 bug 被纪律抓住**：①浮点符号比较漏 -0.0（-0.0<+0.0 为 false）→整数符号位比较；②谓词极性反了（svcmplt 选的是符号位=0 的 lane）→svcmpge；③kreg7 掩码承载类型太窄（64 元素 u8 装进 uint16 截断）→按元素数定宽；④`mov x, z.d[0]` 汇编器不收 → svlastb 向量→标量通道。**两阶段**：阶段1（!122, 30s）=30882/0；阶段2 首轮 29755/1——**1 个未复现 fail 且我删日志太早丢了 cpu-mask 归因证据**（操作失误如实记录），4 轮复跑共 ~10 万结果 0 fail，122 每轮参与未触发（数据点 #7）。教训：阶段2 出 fail **必须先提取 cpu-mask 再删日志**。剩余 44 个（批次 3-7）。

- **2026-09-21（会话 11，批次 1 完成）**：**64 转换计划批次 1 完成并推送**（kreg 7 个：软件仿真 → SVE 谓词真指令）。映射全部独立探针验证后落地：KSHIFTL/R→svlsl/svlsr、KORTEST/KTEST→svptest_any、KUNPCK→svzip1、掩码展开→svsel、KMOV→谓词 lane+svcntp、arm0102 配方→ldr z/str z+谓词比较扩展。**核心价值升级**：原版 sw==hw 同义反复 → SVE 版独立标量 golden（谓词指令真算错才 fail）。**两个 bug 被纪律抓住**：kreg9 谓词 lane 数硬编码 16（VL=256 实际 8）→svcnt* 动态；arm0102 store 用 svptrue 8-lane 写 4 元素子块 → 越界 16B/迭代砸堆元数据（851 次后 double free）→4-lane whilelt。**两阶段**：阶段1=7954/0（另短窗复核 exit pass）；阶段2=7359/0（**数据点 #6：谓词指令负载不触发 122**）。教训：kreg 系 stderr 每迭代多行 fprintf × 126 核 × 60s 会打 9.4GB 日志——后续批次对高频 fprintf 测试改用 -t 短窗或考虑框架日志限流。剩余 57 个待转（批次 2-7）。

- **2026-09-20（会话 10，Task 7 收尾）**：**全部 53 个 SVE 测试完成并推送（分支 feat/sve-port-avx53，8 个 commit）**。
  **最终验证**：53 个全量同跑（全核，10s/测试）= **16909 pass / 0 fail / exit: pass**；总测试数 342 = 289 原有 + 53 新增（精确匹配，无破坏）；README 测试表新增 SVE avx53 套件行；计划文件 35 个勾选框全部完成。
  **任务总结**：53 个 avx 命名 NEON 测试 → SVE1 移植（负载环境不变）。分域：FMA 10（纯指令替换）、Mesh 18（两框架协议保真）、eigen 1（自写双对角化）、ipsec 17（EVP+多流 SVE HMAC，内核 OpenSSL 比对 5200+1360 全对）+ 7（EVP-only 如实标注）。SVE SHA/HMAC 内核成为仓库新资产。
  **两阶段纪律战果**：全程抓住我 9 个逻辑缺陷（mesh 重置竞态、共享 work、static 数组共享、SHA 维度反转、ROTR/ROTL、GCC 12 intrinsic 缺失、HMAC 外层长度×2、3DES padding）——每一个都是在 60s 干净基线或 OpenSSL 比对阶段暴露的，没有一个带病提交。
  **122 狩猎数据点汇总（60s 窗口，5 类负载全不触发）**：①短向量 SVE FMA 逐位比对 ②mesh 多核互联协同 ③4MB 内存带宽扫描 ④SVD 式密集 SVE FMA ⑤EVP 加密+多流 SVE HMAC。**待研究**：122 的 SDC 需要什么条件触发？（候选方向：更长时窗、特定数据模式、温度/电压边界、更底层通路如 LSE 原子/SVE2 专属指令、或跨核一致性特定序列）
- **2026-09-20（会话 9）**：**Task 6 完成并推送 → 53/53 全部完成**。
  **ipsec 域 24 个**两层实现：① HMAC/SHA 系 17 个 = 加密保持 OpenSSL EVP + MAC 换多流 SVE HMAC（sve_hmac.h，OpenSSL 验证 1360/1360；lane0=原密文语义不变 + 1..N-1 变体消息填 lane，golden 全预计算，SHA1-96 保持 12 字节截断）；② EVP-only 系 7 个（GCM×2/CMAC/XCBC×5）= 无 SHA 可 SVE 化（上游 libcrypto 无 SVE AES 路径，反汇编 1012 对象实证只有 ChaCha20 有），负载保真移植+描述如实。
  **HMAC 层多修 2 个 bug**：SHA-224/384 外层消息长度错（截断摘要应为 92B/176B 不是 96B/192B——RFC 4231 向量抓出）；3DES-CBC 缺 set_padding(0) → 解密尾部块错（阶段1 前快验抓出）。
  **两阶段验证**：阶段1（!122）=3645 pass/0 fail；阶段2（含122）=3701 pass/0 fail。**122 数据点 #5**：EVP 加密+多流 SVE HMAC 负载 60s 未触发。
  **53 个负载类型对 122 的触发能力总结（60s 窗口全不触发）**：①短向量 SVE FMA ②mesh 互联协同 ③4MB 内存带宽扫描 ④SVD 式密集 FMA ⑤EVP+多流 HMAC。下一步 Task 7 收尾（全量回归+文档）。
- **2026-09-20（会话 8）**：**Task 4 + Task 5 完成并推送**。
  **Task 4（eigen_svd_bidiag_sve）**：自写 Householder 双对角化 SVE 负载（n=128 标定 0.92ms/次，确定性验证通过，golden=init 首算+run 逐字节比对，fracture_loop_count=5 对齐原版）。**两个 bug 被两阶段流程抓住**：① 共享 d->work 被 127 线程互踩 → per-thread[cpu] 分配；② **函数内 static 局部数组 v[]/w[] 跨线程共享**（独立单线程验证程序暴露不出）→ 改栈数组。验证：阶段1=5387 pass/0 fail；阶段2=5411 pass/0 fail（122 数据点 #4：SVD 式密集 SVE FMA 60s 未触发）。
  **Task 5（SVE SHA 内核）**：SHA-1/224/256/384/512 五内核（SHA-256 系 8×32bit lane、SHA-512 系 4×64bit lane，跨消息并行）。**OpenSSL 全向量比对：5200/5200 ALL MATCH**（含 "abc" 标准向量）。修掉 3 个我自己的 bug：① sha512/sha1 out 参数维度写反（[流][字] vs [字][流]）→ 越界写段错误；② SHA-1 消息调度 ROTR 写成应为 ROTL（一字母差）；③ GCC 12 无 svror/svrol intrinsic → 移位组合宏。经验：SVE 类型不能做数组元素（W 表驻留内存往返）、verifier 需 -fstack-clash-protection。
  **进度 31/53**。剩余：Task 6（ipsec 24 个，哈希换 SVE 内核+多流变体消息填 lane，加密保 EVP）+ Task 7（收尾）。
- **2026-09-20（会话 7）**：**Task 2 + Task 3 完成并推送**（7de817e0 + 框架B commit）。Mesh 域 18 个全部完成：框架 A 12 个（协同写+栅栏+读校验协议逐版本保真：read_done 轮次/reset_lock+readers_done 屏障各按原版）+ 框架 B 6 个（avx2 三连 4MB 扫描/写回读；avx512 三连 16 元素块）。**过程中发现并修复的问题**：(1) 我生成的 wide_asymm_distrib_int 用了"首个读者立即重置"错误协议 → 竞态（8 线程 5 fail）→ 按原版补 readers_done 屏障后 6252 pass/0 fail——正是两阶段流程抓到的逻辑缺陷；(2) 原版 asymm_write 系有越界读栈 UB（读者对 8 元素栈数组循环到 1024）——SVE 版改为块偏移写入+全量读（commit 披露）；(3) shortid 生成器要求 meson reconfigure 后才认识新测试名。**两阶段验证**：18 个 mesh 阶段1（!122）=183935 pass/0 fail；阶段2（含122）=168516 pass/0 fail。**122 数据点 #2/#3**：mesh 互联协同负载与 4MB 内存带宽扫描负载 60s 内均未触发 122 SDC。剩余：Task 4（eigen 1）+ Task 5（SVE SHA 内核）+ Task 6（ipsec 24）+ Task 7（收尾）。
- **2026-09-20（会话 6）**：**Task 0 + Task 1 完成并推送**（分支 feat/sve-port-avx53）。命名按用户拍板 **B 方案**（`_avx2→_sve`、`_avx512→_sve_wide`）。Task 0 = meson 骨架（47a7b344）。Task 1 = FMA 域 10 个 SVE 版（a181b940）：负载参数逐字保留（VECTOR_SIZE/随机域/特殊值注入/INNER_ITERATIONS/memcmp+store-reload），指令换 svld1/svmla_x/svst1 + svwhilelt 谓词（VL 无关），HWCAP_SVE 探测照 sleef_sve 先例。**两阶段验证实测**：阶段1（排除122，126核，60s）= **5488 pass / 0 fail**（可靠性证明：代码逻辑正确）；阶段2（全核含122，60s）= **6219 pass / 0 fail**——122 参与运行但**短向量 SVE FMA 逐位比对负载 60s 内未触发其 SDC**（122 故障特性数据点 #1：FMA 计算型负载 × 60s = 不触发）。回归：zstd19 + NEON 原版 fma_tail_avx2/avx512 均 pass。下一步 Task 2：Mesh 框架 A 12 个。
- **2026-09-19（会话 5）**：**实施计划落盘** `docs/superpowers/plans/2026-09-19-sve-port-avx53.md`（Task 0-7，one-patch-per-unit）。**关键环境实测**：(1) sudo 需密码 → 物理热插拔 122 不可自动执行，**等效方案 `--cpuset='!122'` 已验证**（框架 device 枚举 127→126 且无 122 条目；注意 cpuset 不支持 `0-31` 范围语法，只支持逗号单值+`!` 前缀）；(2) SHA 家族单消息本质串行（块间链依赖），ipsec SVE 多流 = 跨消息并行（主消息 MAC 语义不变 + 变体消息填 lane），Task 5 的 SVE SHA 内核必须先与 OpenSSL 全向量比对证明正确后才用作 golden 基；(3) **发现命名撞车**：`_avx2` 和 `_avx512` 都映射 `_sve` 会冲突（如 fma_tail_avx2 和 fma_tail_avx512），计划暂按用户原话 `_avx2→_sve`、`_avx512→_sve2`（但实际全用 SVE1 指令，sve2 后缀名不副实）——**待用户确认**；(4) 从 main 切新分支 feat/sve-port-avx53（当前 eigen 分支的 Eigen 改动与本任务无关）。计划待用户批准后开始 Task 0。
- **2026-09-19（会话 4，续）**：**用户三项拍板**：(1) ipsec 选**方案 2**（哈希自写 SVE 多流并行实现，AES 保 OpenSSL）；(2) `eigen_svd_cdouble_sve` 在本机超时——实测确认 SIGSEGV 崩溃（feat/eigen-sve-double-packets 分支的 Eigen5 SVE double packet 在 300×300 BDCSVD 场景崩溃，maxrss 2.4GB），用户指示**另写一个正常的** SVE 负载替代；(3) 命名统一 `avx→sve`（SVE1，本机 VL=256 无 SVE2），53 个新名字全部无冲突（实测验证）。**计数修正**：ipsec avx 命名实际 24 个（此前误记 23），总数 53 不变。实施基线定稿：FMA 10 无损替换 + eigen 1 个自写 + Mesh 18 无损替换 + IPSec 24 哈希 SVE 多流 = 53 个全进 `tests/cpu/sve/`。当前分支 `feat/eigen-sve-double-packets`（新工作应切新分支）。
- **2026-09-19（会话 4）**：完成 53 个 avx 测试**负载环境实录**（`2026-09-19-avx53-workload-environments.md`）——FMA 11（VECTOR_SIZE/随机域/特殊值/嵌套结构逐项参数表）、Mesh 18（两框架：协同读写 A / 顺序扫描 B，工作集 1024 元素~4MB）、IPSec 23（全走 OpenSSL EVP，golden 预计算）。**关键发现**：vendored OpenSSL 1012 对象中仅 ChaCha20 有 SVE 实现，ipsec 用的 AES/SHA/3DES 全是 NEON 汇编——ipsec 的"SVE 版"需要用户决策（改名保负载 vs 自写 SVE 多流哈希）。eigen_svd_cdouble_noavx512 的 SVE 后端版已存在（eigen_svd_cdouble_sve）。FMA 10 + Mesh 18 = **28 个可无损 SVE 移植**。命名规则待确认（avx→sve）。等用户拍板 3 个开放问题后开始写测试。
- **2026-09-19（会话 3，续）**：用户下达**高优先级任务**：CPU 122 号核心存在 SDC 故障（唯一故障核）。任务 = 写出快速检出 122 的测试用例 + 分析什么负载最快触发。强制验证流程已固化为任务纲领（offline 122 → 干净基线 60s 必须全 pass → online 122 → 全核验证 fail 精确锁定 122）。待开始。
- **2026-09-19（会话 3）**：创建 `tests/cpu/sve/` 空目录（用户指定，待放新 SVE 测试）。审计 53 个 avx 命名测试：**全部无 SVE**（源码 0 命中 + 53 个 .o 反汇编 0 个 z 寄存器操作数 + 真实现三分类：FMA/Mesh=NEON 128-bit、IPSec=纯 C 调 OpenSSL、Eigen=模板）。确认为"无 SVE 机器时代产物"，向量宽度覆盖缺口 = `tests/cpu/sve/` 的移植/新写机会。等用户下一步指令。
- **2026-09-18（会话 2）**：SVE 研究。**关键发现**：(1) 这台机器已不是旧文档记载的无 SVE 鲲鹏 920——实测 cortex x3b（MIDR 0x480fd020）、127 核、SVE 256bit 可用，11 个现有 sve512_*/sleef_sve 测试全部 pass；(2) 用户贴的 flags 分析（608 核/SVE2/SME）与本机实测不符，以实测为准；(3) FMMLA/BFDOT/SDOT/USMMLA/FCMLA 全部实测可执行且计算正确；(4) FCMLA 语义澄清——每条只算半复乘、#0+#90 成对用，GCC 向量化仲裁证明硬件无缺陷（教训：新指令测试的 golden 模型必须先仲裁再怀疑硬件）。**下一步候选**：5 个 SVE 新测试设计（见研究文档 §5），等用户定方向后按仓库纪律写 plan 再实现。
- **2026-09-18**：`docs/docs_xu/` 目录建立。后续研究内容（实验记录、分析报告、调研笔记等）从下一主题开始落盘于此。
