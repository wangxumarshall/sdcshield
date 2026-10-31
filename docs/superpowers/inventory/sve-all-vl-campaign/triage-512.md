# SVE 全 VL 验证运动 — gem5 SE 512 腿分诊表（Task 6）

- 日期：2026-10-29（Task 6 产物；2026-10-30 勘误修订：降容口径 §0/§6、
  崩溃格补录 §5.1（movbe_l2/mrn_rmw_l1d）、§1.2 结构归位、超时账目
  对齐 §4/§5.2——修复轮 F1-F4）
- 分支：`feat/sve-all-vl-verification-2026-10-28`
- 上游：plan `docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md` Task 6；
  brief `.superpowers/sdd/2026-10-28-sve-all-vl-verification-campaign/task-6-brief.md`
  + 控制器修正案 1-10（`-f no` 判定规则、-P 20、G5 显式 + md5、26 项 baked -O、
  7 用例标定集、Ruling E 证据纪律、矩阵头部标注、机器纪律、诚实分诊）。

## 0. 矩阵头部块（Task 8 汇总时逐字引用）

> **512 列采集条件**：`-f no` nofork 模式 + 26 项有界 `-O` 覆盖 + gem5 SE
> （binary md5 `3f9011f40dbe80ec1df478c62ddb0b06`，Task 5 终版 9-stub 链）+
> sdcshield `12ccda540fd4`。命令形状：
> `run_sve.sh sdcshield -e <test> -t 500 -n 1 -f no --ignore-timeout
> <26 baked -O> [gem5-overrides.txt 额外降容] --vl 4`（`--vl 4` = 512-bit）。
> 额外降容以 `gem5-overrides.txt` 为权威（现存 **16 条**）：批前预置 2 条
> （sleef nelems=1048576 生效；acl_gemm dim=512 超时 → 预置 256 仍超时 →
> 修订为 **dim=128**）+ 分诊期为超时格补设 14 条（fisttp / eigen_gemm /
> ipsec_cmac / mesh×8 / movdq2q / movq2dq 共 13 条生效转绿；mrn_rmw_diff_1m
> 1 条无效仍边界）。另曾预置 eigen_svd_cdouble mdim 条目——其 clamp 下限
> 即 300，更低 -O 被静默钳回、**无效**，条目已删；**eigen_svd_cdouble_sve
> 为 timeout 边界格**（§5.2），非降容格。
> 判定 = **进程退出码为权威** + 输出 `exit:` 行为佐证（nofork 掩蔽归因
> 细节，单测单进程；exit 行会被叠印间歇抑制，见 §3）。
> **工作量语义（本腿核心限定，见 §2）**：gem5 nofork 下 `-t` 时限预算被 shmem
> 叠印结构性禁用——每测试恰执行 ~1 轮内层循环（TEST_LOOP 粒度或 do-while
> 首轮），pass = 该轮全量 golden 字节比对成立。压力/重复维度由 native 腿
> 承担，本列语义 = 512 档功能性正确性。

## 1. 标定（7 用例，真二进制，逐例实测）

标定命令 = 批量命令逐字同形（含 26 项 baked -O；见 `gem5-512-batch.sh`）。
身份：`sdcshield-12ccda540fd4`（`--version` 实测）；
gem5 `/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt`
md5 `3f9011f40dbe80ec1df478c62ddb0b06`（启动时核对，与 Task 5 终版一致）。

| 测试 | 墙钟 | tick（sim 时长） | 判定 | 处置 |
|---|---|---|---|---|
| sleef_sve | ≥2776 s（46 min，手动终止；无界轮次未完成） | —（未完成） | 未收口 | 有界化：`-O sleef_sve.nelems=1048576`（gem5-overrides.txt），复测见 §1.1 |
| fma_tail_sve | 8 s | 5.99 ms | exit: pass (rc=0) | 直接可用 |
| mesh_upi_sve_sym | 9 s | 6.08 ms | exit: pass (rc=0) | 直接可用 |
| kreg2_sve | 9 s | 5.96 ms | exit: pass (rc=0) | 直接可用 |
| sve_rot_ldr_at_top | 630 s | 407.5 ms | exit: pass (rc=0) | 直接可用（复现 Task 5 spike 0.407 s，比率自洽） |
| eigen_svd_cdouble_sve | ≥3233 s（53.9 min，手动终止；mdim=300 无界轮次未完成，RSS 654 MB 仍在推进） | —（未完成） | 未收口 | 有界化尝试 mdim=128——后经复测证实 **clamp 下限即 300、更低值被静默钳回而无效**（§1.1/§5.2），该格终为 timeout 边界 |
| acl_gemm_sve | （首测因并行标定撞车废弃——两实例同写一 .d/.log，日志交叉污染不可作证，两者终止清理；干净复测见 §1.1） | | | |

**标定过程事件（如实记录）**：eigen 手动终止后串行标定序列自动推进到其自身的
acl 实例，与先行并行启动的 acl 实例共用 `acl_gemm_sve.d`/`acl_gemm_sve.log`
（m5out 互覆写、日志交叉）——两实例全部终止、产物删除、单独重跑，以重跑为准。
并行标定本身（2 实例 / 127 空闲核）无 CPU/内存争用，计时保真。

### 1.1 有界复测（批量内实测，= gem5-overrides.txt 各项的落地值）

| 测试 | 有界值 | 批量实测 | tick（sim） |
|---|---|---|---|
| sleef_sve | nelems=1048576 | pass @ 1384 s | 1.152 s |
| acl_gemm_sve | dim=128（两次降容：512 超时 → 256 超时 → 128） | pass @ 1186 s | 0.955 s |
| eigen_gemm_double_max_arm | mdim=1024（下限） | pass @ 1162 s | 0.702 s |
| fisttp_sve | num_elements=1024（下限） | pass @ 9 s | 6.0 ms |
| ipsec_aes128_cbc_aes_cmac_sve | data_size=1024（下限） | pass @ 24 s | 20.6 ms |
| mesh ×8（read/symm/write 族） | buffer_bytes=1048576（下限） | 全 pass @ 22-25 s | 18-20 ms |
| movdq2q / movq2dq | count=1024（下限） | pass @ 17 s / 29 s | 14-31 ms |
| mrn_rmw_diff_1m_sve | base_count=8192（下限；×16 硬比率 → 有效 131072） | **仍超时**（→ §5.2 边界） | — |
| eigen_svd_cdouble_sve | mdim=300（=clamp 下限，更低被静默钳回） | **150 min 长超时仍跑不完单轮**（→ §5.2 边界） | — |

### 1.2 标定结论（标定 7 用例 + 冒烟/批量的交叉数据）

- **单实例 RSS 实测**（smaps_rollup / ps 采样）：sleef 无界 1.2 GB 平台期、
  sleef 有界（nelems=1M）≈ 0.56 GB、eigen 0.43-0.65 GB、acl ≈ 0.28-0.32 GB、
  轻测试 ~1.2 GB（含 427 MB 二进制载入 + gem5 基线）。共享文件页仅
  ~110 MB/实例（gem5.opt 文本段多实例共享）。批量中段实测 20 车道峰值
  RSS ~1.6 GB/实例、系统 used 14 G / available 46 G——**P=20 内存推算
  成立**（修正案 2 的 ~2.7 GB/实例估计偏保守；Task 5 报告的 4.4 GB
  "Memory Usage" 为 6-stub 时代数字，9-stub 二进制实测不重现）。
- **wall/sim 比率 ≈ 1400×**（sve_rot 0.4075 s / 630 s = 1546×；sleef 有界
  1.152 s / 1360 s = 1180×；轻测试 6 ms / 8-9 s 含启动开销）。即每测试
  墙钟下限 ≈ 框架启动 ~0.1 s sim ≈ 8 s，重轮次按 sim 时长 × ~1400 折算。
- **每测试恰 ~1 轮**（§2 机制）→ 墙钟 = 启动 + 单轮成本；批量的时长由
  重轮次测试（rot 族 0.4-1.6 s sim、mesh 族 ~0.13 s sim、ipsec 族
  0.06-0.23 s sim、adcx 族 2.8 s sim）决定。

## 2. 机制发现：nofork 下 `-t` 预算结构性失效（本腿核心限定）

**现象**：标定中 fma_tail_sve / mesh_upi_sve_sym / kreg2_sve 全部在 ~6 ms sim
（8-9 s wall）收口——远小于 `-t 500` 预算；sve_rot_ldr_at_top 恰为 0.4075 s
（= Task 5 spike 复现）。native 同名测试在 `-t 2000` 下跑满预算（fma_tail 21
fracture 块 / 1.4 s 等）。

**根因（DWARF 偏移实测 + 源码链 + 实证三角）**：gem5 SE 不保留 MAP_SHARED
tmpfile 内容跨 munmap+重映射（Task 5 Ruling H 上游限制）→ 框架
`commit_shmem()` 的拆除-重映射使 `thread_data_offset` 读回 0 →
`main/test_thread_data_ptr` 均计算到 shmem+0 → `PerThreadData::Test`（64 B）
叠印 `SharedMemory` 头部（`builddir/sdcshield` gdb 实测，二者偏移对照）：

| Test 字段（叠印写入者） | 偏移 | 被叠印的 SharedMemory 字段 |
|---|---|---|
| `Common`（log_fd 等，32 B） | +0x00 | `thread_data_offset`、`TestConfig cfg`（20 B） |
| `inner_loop_count`（**每次 test_time_condition 自增**） | +0x20 | **`current_test_endtime`（8 B，deadline）** |
| `inner_loop_count_at_fail` | +0x24 | endtime 高 4 B |
| `previous_cpu`（NSDMI=`LogicalProcessor::None`=-1） | +0x28 | `current_max_loop_count`（fracture 门） |
| `tid`（worker 启动即写 gettid()） | +0x2c | `current_test_sleep_duration` |

后果链：(i) worker 第一次 `test_time_condition()` 自增 `inner_loop_count`
即把 endtime 低 4 B 踩成小值 → `wallclock_deadline_has_expired` 恒真 →
**内层时限循环在首个检查点退出**（TEST_LOOP(N) → ~N 次迭代；do-while →
1 次；前置 while → 0 次——见 §2.1 空转审计）；(ii) `previous_cpu`=-1 →
`current_max_loop_count` 读回 -1 → `<=0` → **fracture 循环也不重入**。
二者合计：**每测试恰执行 ~1 轮内层循环**，`-t` 预算完全不参与。

**对判定有效性的影响（分析 + 实证）**：
- pass 判定路径不受影响：SVE 计算与 golden 的 memcmp 走堆/栈内存，
  逐元素全量比对在当轮内完整发生（Ruling H 已证；本轮 tick 实测
  sleef 1.152 s = 整轮算完才比对，非早退）。
- **工作量语义改变**：绿格 = "1 轮字节级比对成立"，非 "500 ms 压力"。
  压力/重复维度由 native 腿承担；本格语义 = 512 档功能性正确性。
- 确定性：`previous_cpu` 只在 `reschedule()`（测试显式调用）时写入真实
  CPU 号，NSDMI -1 确定 → fracture 退出路径确定，非调度偶然。

### 2.1 前置 while 空转审计（0 迭代假绿排查）

全树仅 8 个文件用前置 `while (test_time_condition())` 形状；映射到本清单
7 例逐一核源：4 个 mesh 变体与 sve_fpcr_cartesian_arm 的前置 while 均嵌在
外层 `do {} while` 之内；sve512_pred_ops_arm 的首轮比对在 while 之前；
sve512_fmmla_arm 的单步自检 + 完整 f64/f32 链（golden 比对）在 while 之前
无条件执行。**7/7 无 0 迭代空转路径**——绿格均含至少一轮真实比对。

### 2.2 guest 能力位实测（HWCAP 探针，分诊工具）

探针二进制（getauxval 直读）在 gem5 `--vl 4` 实测：`HWCAP=0xccd0f7fb`
（**sve=1**、aes/sha 族在位）、`HWCAP2=0x7f802f80`（**sve2=0**、
**f32mm=1 f64mm=1**、sme=1 及 SME 子位、bf16=0）。对照：本机 native
`HWCAP2=0x9ff81`。推论：清单内 SVE1 门全开；sve512_fmmla_arm 的
F32MM/F64MM 门在 gem5 通过（会真跑，非 SKIP）；全树唯一 SVE2 门测试
sve2_cross_precision_arm 不在本清单。

## 3. 机制发现 II：叠印虚假 Skip 标签与 `--fatal-skips` 的废弃（sleef 假警报平反）

**经过（如实记录，含本人失误）**：为让 nofork 下不可见的 SKIP 显形，本任务
一度在批量命令中加入 `--fatal-skips`。首批量早期即现异常：fma/fmatail 全族
`exit: fail`（rc=1），而 fma_tail_sve 在标定（无该旗标）为 pass。10 秒级
对照探针（同命令 ±旗标、无并发）证实：**旗标单独即可翻转判定**（tick
5.96 ms 不变 = 工作量相同）。

**根因（源码链）**：`run_one_test_once`（sandstone_run.cpp:1235-1248）在
init 线程返回 0 后检查 `sApp->main_thread_data()->has_skipped()`——
`main_thread_data_ptr` 因叠印指向 shmem+0，`Main` 结构的 skip 标志位读到
worker `Test` 字段写入的垃圾位 → 框架把"整轮跑完且比对通过"的测试误标
`Skipped`。无 `--fatal-skips` 时该假标签被容忍（exit: pass，rc=0——测试
真实通过）；有旗标时假标签致命化 → 批量假红格。**该位是否被踩中取决于
当轮 Test 字段的具体位型 → 按测试确定**（fma 族全中，agu_stress /
bigint_mulx / eigen_svd_bidiag 不中）。

**sleef_sve 假警报（消耗 ~2.5 h 分诊，全额记录）**：sleef 有界 smoke（含
`--fatal-skips`）`exit: fail` @ tick 1.152 s——当时按"整轮跑完才失败 =
memcmp 数据失配"推理（tick 推理本身正确），走了完整分诊链：确定性复现
×2（tick 1.15210/1.15196 sim-sec 几乎逐位一致）、`--vl 2`（256-bit）同样
fail、native 256 同参 pass、隔离驱动 v1/v2（SLEEF 内核背靠背，16 族 ×
1M 元素 @512）全稳定、v3 结构复刻（先全 golden 后全 kernel、逐族独立
golden 数组）稳定。期间 v3 首版曾因驱动自身共享缓冲 bug + 误用修补前旧
二进制产生两轮假"发散"（均已更正复测）。最终 10 秒探针定案：**去掉
`--fatal-skips` 后 sleef `exit: pass`（tick 6.93 ms，同工作量）**——
"失败"完全是假标签致命化伪影，内核与比对无恙。隔离驱动链反而成为
"sleef SVE 内核在 gem5@512 逐位确定"的正面证据。

**处置**：批量命令去掉 `--fatal-skips`（回归控制器修正案 1 的原始命令
形状）；skip 可见性改由分诊期 **tick 地板审计**承担（诚实 init-skip 的
tick 停在框架初始化地板 ~3.7 ms 且无测试轮工作量；跑完被误标者 tick
正常——见 §5 审计协议）。附带发现：`exit:` 行本身也会被叠印踩掉的
`cfg.verbosity`（<0 时 logging.cpp:587 不打印）间歇抑制 → 判定以**进程
退出码为权威**，exit 行仅为佐证（提取脚本已按此实现）。

**mce_check 在 gem5 的真实行为（补充定性）**：`-e mce_check
--fatal-skips` 单独也 fail——同源假标签（非 EDAC 逻辑）。无旗标时
mce_check 平凡通过（EDAC 读数 0→0；get_mce_interrupt_counts 的 vector
按 max_cpu+1 定长非空，不触发 "Cannot detect MCE events" 移除路径）。

## 4. 批量执行记录

- **启动**：2026-10-29 22:37:23（`./gem5-512-batch.sh 20`，启动器打印身份：
  gem5 md5 `3f9011f40dbe80ec1df478c62ddb0b06`、`sdcshield-12ccda540fd4`、
  190 用例清单、P=20）。启动前空闲核查：load 2.02（仅本任务 3 个标定/
  分诊实例），无外来负载、无构建。
- **中途内存观测**（22:43，27/190 完成时，启动后 ~6 min）：`free -g` used 14 / available
  46（61 总量），20 车道全开，最大单实例 RSS 1.55 GB——与标定一致
  （单实例 ≤ ~1.6 GB），P=20 内存推算成立，无 swap 压力、无 OOM。
- **结束**：2026-10-30 01:29:17（总时长 2 h 52 m；启动器自计 `cells with
  'exit: pass': 181 / 190`；全部调用累计车道时 32.4 h——含 **18 格先后
  撞上外层超时**（batch.tsv 共 20 个 rc=124 行：3600 s ×19 + 9000 s ×1；
  其中 acl 两行、eigen_svd_cdouble 两行——后者因 .done 预置未在干净批量
  内重跑）与全部有界重跑）。
- **终局计数（gem5-512-extract.sh 机械提取，190 行校验）**：
  **pass 181 / timeout 4 / crash 5 / fail 0**；9 个非绿格全部为
  gem5 限制/仿真速边界（§5），零测试 bug。
- **过程事件（如实）**：22:22 曾以含 `--fatal-skips` 的命令启动过一次批量，
  早期即暴露 fma 全族假红格（§3 机制），于 ~30 格后终止、清除全部
  `.done`/`batch.tsv` 污染状态，22:37:23 以修正案原始命令形状重启——本表
  全部数据来自重启后的干净批量。

## 5. 非绿格分诊（逐格复现 + 指令级定位）

### 5.1 CRASH 类（gem5 "unknown instruction" panic，5 格，全部确定性复现）

| 测试 | 复现 panic（verbatim） | 定位指令 | 分类 |
|---|---|---|---|
| ipsec_aes192_gcm_sve | `panic: Attempted to execute unknown instruction (inst 0x0ef4e280)`（批量 tick 62.2 G；单测复现 tick 10.7 G） | `pmull v0.1q, v20.1d, v20.1d`（objdump 解码；OpenSSL AES-GCM 的 GHASH 多项式乘） | **gem5 ISA 模型限制**（无该 64-bit PMULL 编码） |
| ipsec_aes192_gcm_sve_wide | 同上（tick ~62 G） | 同上 | 同上 |
| l2c_cross_cache_line_sve | `panic: Attempted to execute unknown instruction (inst 0x00000000)`（批量行 rc=134 wall=12 s，tick 5.9 G；两 Phase Exec 追踪 + 反汇编定位） | `dc civac, x27` @ `l2c_cross_cache_line_sve_run+540`（0x46d80c）——测试核心机制"store → 缓存行失效 → 重载 → 比对"的失效指令；gem5 虽有 Dccivac 定义（data64.isa:553）但该路径在本 SE 配置下 raise UndefinedInstruction（panic 报 0x0 = 无载荷故障字） | **gem5 SE 模式限制**（缓存维护指令路径） |
| movbe_l2_sve | `panic: Attempted to execute unknown instruction (inst 0x00000000)`（批量行 rc=134 wall=13 s，`Program aborted at tick 6430150000`） | `dc civac` @ movbe_l1d_vs_l2_sve.cpp:192（`__asm__ volatile("dc civac, %0\n\tdsb sy")`——"L2-reload 探针：重载前逐行失效输入行"，docblock 明示 **movbe_l2_sve only**；同文件的 movbe_l1d_sve 不执行该路径，批量绿 @ 317 s） | 同上（gem5 SE 模式限制） |
| mrn_rmw_l1d_sve | `panic: Attempted to execute unknown instruction (inst 0x00000000)`（批量行 rc=134 wall=12 s，`Program aborted at tick 5821630000`） | `dc civac` @ mrn_rmw_fwd_vs_l1d_sve.cpp:188（"L1D-array read 探针：svst1 与重载之间逐行失效临时行"；同文件的 mrn_rmw_fwd_sve 不执行该路径，批量绿 @ 102 s） | 同上（gem5 SE 模式限制） |

五格均非测试 bug（指令使用合法且 native 腿全绿：ipsec GCM 两格 187 s 级、
l2c/movbe_l2/mrn_rmw_l1d 三格 1.9-4.2 s 级——见 native-256-LCG1002.summary.txt）；
修正案 10 预判的 "gem5 ARM 模型缺指令 → SIGILL 类边界"在此坐实，且按裁定
给出了可定位的失败指令。两个 `dc civac` 格的同文件兄弟变体
（movbe_l1d_sve / mrn_rmw_fwd_sve，不执行失效路径）在本批量中均绿——
指令级归因经交叉验证（文件级 grep 会过报，实际 crash 集 = 真执行该指令
的 5 格，与全清单源码预扫 + 批量实测一致）。Exec 追踪证据留工作区：`/tmp/t6probe/l2c-exec.log`（追踪窗
5768200000-panic，仅 cpu1 活动，最后一条 = `str x3,[x27]` @ +536，
下一条 0x46d80c 即 `dc civac`）。

### 5.2 TIMEOUT 类（14 格有界重跑转绿 + 4 格边界）

批量/冒烟期 timeout=3600s 标记的格子逐个降容重跑（gem5-overrides.txt 逐项
记录依据），**14 格转绿（表上半）、4 格仍为边界（表下半）**：

| 测试 | 超时原因（机制） | 有界值 | 重跑结果 |
|---|---|---|---|
| fisttp_sve | num_elements 自适应在 2 GiB guest 放大 | 1024（下限） | pass @ 9 s |
| acl_gemm_sve | dim=256 golden（naive 标量三重循环 dim³ fmaf）>60 min | 128 | pass @ 1186 s，tick 0.955 s |
| eigen_gemm_double_max_arm | 自适应 dim 过大 | 1024（下限） | pass @ 1162 s，tick 0.702 s |
| ipsec_aes128_cbc_aes_cmac_sve | data_size 自适应放大 | 1024（下限） | pass @ 24 s |
| mesh_upi_sve_{read_L3,read_only,wide_asymm,wide_asymm_distrib,wide_asymm_read_L3,wide_asymm_read_only,wide_symm,write_only}_int（8 格） | buffer_bytes 自适应在 2 GiB guest 放大（read/symm 族单轮远重于 write 族） | 1048576（下限，同 baked 的 _sym） | 全部 pass @ 22-25 s |
| movdq2q_sve / movq2dq_sve | count 自适应放大 | 1024（下限） | pass @ 17 s / 29 s |
| mrn_rmw_diff_1m_sve | base_count 自适应放大；重跑 base_count=8192（下限）**仍超时**——`base_count×16` 硬编码比率（mrn_rmw_diff_footprint_sve.cpp:58-61，显式配置亦保持）→ 最小有效工作量 131072 元素，单轮 >60 min | 不可再降 | **边界** |
| eigen_svd_cdouble_sve | **mdim clamp 下限即 300**（任何更低 -O 被静默钳回）——标定 53.9 min 手动终止 + 冒烟期 3600 s 超时（mdim=64 被钳回 300）+ **9000 s（150 min）长超时重跑仍未完成单轮**（干净批量经 .done 预置未重跑该格；native 同 mdim=300 单块 3.07 s ≈ ~9G 指令 → gem5 需 ~4-8 h） | 不可再降 | **边界** |
| movbe_slf_sweep_sve | buffer_bytes 的 clamp 下限 = 默认值本身（64 KB，`SLF_SWEEP_BUFFER_BYTES`）——已在最小几何超时；native 单块 3.78 s | 不可再降 | **边界** |
| sve512_stencil_axis_arm | 无任何几何 knob（设计定几何：双 shape/网格/RADIUS 扫）；native 单块 12.0 s | 无杠杆 | **边界** |


### 5.3 sve512_fmmla_arm 专项核验（控制器修正案 10）

批量格：rc=0、`exit: pass`、tick 5.97 ms。为排除"tick 近地板 = 可能
init-skip"的疑点，以 Exec 追踪（--debug-start=4e9）直证：**514 条
`fmmla z0, z1, z2`（SimdFloatMatMultAcc 类）真实执行于
`sve512_fmmla_arm_run`**（tick 5.609-5.610 G 区间，另 init 符号行
25,965 条）——门（F32MM/F64MM HWCAP2，§2.2 探针实证 =1）通过、真跑、
golden 比对成立。**无 SKIP、无 finding**。追踪证据：/tmp/t6probe/
fmmla-trace2.log（工作区）。

**TIMEOUT 小结（账目对齐）**：**18 格**先后撞上外层 3600 s 超时（batch.tsv
20 个 rc=124 行；acl 与 eigen_svd_cdouble 各两次——前者 512/256 两档皆超时，
后者冒烟期一次 + 长超时一次且因 .done 预置未进干净批量）。其中：
- **14 格经有界重跑转绿**（§5.2 表上半：fisttp、acl@128、eigen_gemm@1024、
  ipsec_cmac、mesh×8、movdq2q、movq2dq）；
- **4 格为硬边界**：mrn_rmw_diff_1m、eigen_svd_cdouble、movbe_slf_sweep、
  sve512_stencil_axis——knob 下限/无杠杆 + gem5 仿真速（四格 native 256 腿
  全绿，单轮指令量在 ~1400× 减速下超出任何实用仿真预算）。
- **预置降容时序（如实）**：干净批量启动时 overrides 仅 3 条——sleef
  （生效，批内绿 @1384 s）、acl dim=256（无效，批量内超时后修订 128）、
  eigen_svd_cdouble mdim=64（无效，clamp 下限 300 静默钳回，条目后删）；
  其余 14 条为分诊期随超时逐格补设。
eigen 长超时重跑行（rc=124 wall=9000s）系分诊期以直接命令 + 9000 s 外层
超时单独执行后并入 batch.tsv（命令形状与批量一致，仅超时上限不同——
如实记录）。

### 5.4 tick 地板审计（init-skip 假绿排查，全量 190 格）

方法：诚实 init-skip 的 tick 应停在"框架初始化裸地板"（无测试轮、无
mce_check 轮）；跑完被误标者 tick 正常。实测分布（gem5-512-verdicts.txt
第 5 列）：

- **最低簇 5.9-6.0 ms 共 24 格**（fma_sve 5.94 为最小）——= 框架 init +
  mce_check + 小轮的签名；其中 sve512_fmmla_arm（5.97 ms）经 §5.3 的
  Exec 追踪直证真跑，标定该簇语义。
- 裸地板参照 ~3.7 ms 仅在被废弃的 `--fatal-skips` 运行中出现过（假标签
  break 跳过 mce_check 的早退路径，§3）——干净批量中无一格低于 5.94 ms。
- 重轮次测试的 tick 与工作量相称（adcx 族 2.8 s、sleef 1.152 s、rot 族
  0.41-1.6 s、mesh 族 0.13-0.2 s、ipsec 族 0.06-0.24 s）。

**结论：零 init-skip 嫌疑格**。结合 §2.2 HWCAP 探针（SVE=1、门全开）与
native 256 腿 190/190 全绿（无规格门 SKIP），本腿绿格均为真实执行。

## 6. 结论

**gem5 SE 512 腿收口：190 格 = 181 绿 + 9 边界（5 crash + 4 timeout）+ 0 fail**。

- **绿格 181/190（95.3%）**：每格 = 1 轮全量 golden 字节比对成立
  （§2 工作量语义），含 sve512_fmmla_arm（514 条真实 `fmmla` 指令
  Exec 追踪直证，§5.3）。
- **边界格 9（4.7%）全部为 gem5 侧限制，零测试 bug**：
  - ISA/系统模型缺口（5）：`pmull v0.1q`（OpenSSL GHASH，ipsec GCM ×2）、
    `dc civac`（缓存维护路径，l2c_cross_cache_line、movbe_l2、mrn_rmw_l1d）
    ——native 256 腿对应格全绿，失败指令均已定位（§5.1）。
  - 仿真速约束（4）：knob 下限或无杠杆的单轮指令量超出实用预算
    （§5.2）——native 全绿。
- **矩阵口径（Task 8 引用 §0 头部块）**：512 列 = `-f no` nofork + 26 项
  baked `-O` + 16 项分诊降容（gem5-overrides.txt 为权威）+ gem5 SE（md5
  3f9011f4…）+ sdcshield 12ccda540fd4；判定 = 进程退出码权威 + exit 行
  佐证；工作量 = 1 轮（时限预算被叠印结构性禁用，§2）——**本列语义为
  512 档功能性正确性，压力/重复维度由 native 128/256 腿承担**。
- **移交 Task 7 的候选**（非阻断）：① gem5-fi 补 PMULL 64-bit 编码与
  SE 模式 DC civac 路径（5 格可转绿）；② shmem 叠印三重效应（时限预算/
  假 Skip 标签/exit 行抑制）的框架侧根治（每线程数据不落 shmem 头部）
  可同时恢复 gem5 腿的预算语义与日志保真；③ 4 个仿真速边界格若未来
  gem5 提速或 knob 下限放宽可再收。
