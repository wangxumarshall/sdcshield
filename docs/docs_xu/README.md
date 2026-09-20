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

## 进度日志

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
