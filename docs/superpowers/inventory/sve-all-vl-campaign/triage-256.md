# SVE 全 VL 验证运动 — native 256 腿分诊表（Task 2）

- 日期：2026-10-29（Task 2 产物）
- 分支：`feat/sve-all-vl-verification-2026-10-28`
- 二进制：`./builddir/sdcshield`（version `sdcshield-12ccda540fd4`，与 Task 1 清单同源）
- 上游：plan `docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md` Task 2；
  brief `.superpowers/sdd/2026-10-28-sve-all-vl-verification-campaign/task-2-brief.md`
  + 控制器裁定（`--ignore-timeout`、acl dim 标定、timeout-cell 协议、裁定 D=seed2 重跑
  内置全部 26 项有界覆盖、裁定 E=日志不入库改为机械 summary 入库）
- 主机状态：128 核 / 62 GiB（node0-only），全核空闲（无外来负载），本 boot 频率基线 1.7 GHz（udev cap）。
  **环境事件（如实记录）**：
  1. 首次尝试的 256-LCG1001 跑（00:26-01:41）与 root-xupeng 的
     `run-freq-sweep-neon-20261028.sh`（定频 1.0-2.9 GHz 满载扫频，23:37-03:47）重叠，
     环境被污染 → **整跑废弃**（跑至 41/190，日志已删；其 stdout 曾留存
     `/tmp/native-256-LCG1001.DISCARDED.stdout`，后随重启丢失，不入库）。本表 seed1 数据
     全部来自机器恢复空闲（03:47 定频扫频结束、全机回 1.7 GHz）后的干净重跑。
  2. **seed2 首跑死亡 + 主机重启（2026-10-29 晨）**：前一会话的 seed2 全量跑（无有界覆盖，
     与 seed1 同命令）于 `mem_disambig_alu_sve` 首迭代中途死亡——尸体日志最后一条为
     `state: { iteration: 0 }` / `time-at-start: 2026-10-29T00:40:15Z`（本地 08:40:15），
     无 `exit:` 行，`result:` 悬空；机器随后于 ~09:01 重启（up 14 min @ 09:15 实测），
     /tmp 清空（seed1/seed2 两跑的 /tmp stdout 一并丢失；seed1 的 `exit: pass` 证据改由
     其日志内 `exit: pass` 行承担）。死亡时刻恰落在 §4 #15 已知的超时杀灭迟滞窗口内
     （seed1 实测该测试超时收尸耗时 1280 s ≈ 21.3 min；08:40:15 + 21.3 min ≈ 09:01 =
     重启时刻）——相关性如实记录，因果未证。按控制器裁定 D，seed2 以 26 项有界覆盖
     全量重跑（含 `mem_disambig_alu_sve.buffer_bytes=4096`，不再进入超时杀灭路径），
     本表 seed2 数据全部来自该重跑；尸体已被重跑覆盖（其结局以本条为准）。

## 1. 全量跑（2 seeds，真实命令与结果）

```bash
LIST=docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt   # 190 用例
LOG=docs/superpowers/inventory/sve-all-vl-campaign/native-256-LCG1001.log
./builddir/sdcshield --test-list-file $LIST -t 2000 -s LCG:1001 -o $LOG -Y \
    -O acl_gemm_sve.dim=512 --ignore-timeout > /tmp/native-256-LCG1001.stdout 2>&1
# seed2 重跑（裁定 D）：同命令 -s LCG:1002 → native-256-LCG1002.log，另加 §5 表全部
# 25 项 -O 有界覆盖（acl_gemm_sve.dim=512 保留，共 26 项 -O，多 -O 累积：
# framework/sandstone_opts.cpp opts_map['O'] 为 vector，逐项 set_knob_from_key_value_string）
```

- 控制器裁定落地：`--ignore-timeout`（TimedOut 不中止全程）；`-O acl_gemm_sve.dim=512`
  （acl 标定：dim=512 全核 11.6 s 稳绿；dim=1024 单线程 131.7 s = 300 s 预算 44%，
  全核竞争下有超时风险 → 取 512；seed1 实测 11.1 s 绿）。
- 152 MB 原始 yaml 日志**不入库**（裁定 E）：本目录 `.gitignore` 屏蔽 `*.log`，代之以
  `extract-summary.sh` 机械提取的 `native-256-LCG<seed>.summary.txt`（每用例一行
  `<test>\t<result>\t<iterations>\t<test-runtime-ms>` + 命令行/exit 头，190 行校验）。

| seed | 覆盖 | pass | timeout | fail | crash | skip | 退出 | 时长 |
|---|---|---|---|---|---|---|---|---|
| LCG:1001 | 190/190 (+mce_check) | 166 | 25 | 0 | 0 | 0 | `exit: pass`（exit=0） | 03:55-07:06 ≈ 3h11m |
| LCG:1002（26 项有界重跑） | 190/190 (+mce_check) | 191 | 0 | 0 | 0 | 0 | `exit: pass`（exit=0） | 09:24-10:01 ≈ 37m |

- 两 seed 的 pass 计数均含 mce_check（seed1：165 清单直绿 + 25 超时格走 §5 有界重跑 +
  mce_check = 191 执行 166 绿；seed2：190 清单全直绿（其中 25 格为腿内有界）+ mce_check
  = 191 执行 191 绿）。
- seed2 时长对照：seed1 3h11m 中约 2h33m 是 25 格超时的 300 s 预算燃烧（含 #15 的 1280 s）；
  seed2 有界化后同规模全量仅 37 m——超时格的"环境边界"性质再获一侧证。

（+mce_check：框架在每次运行末尾自动追加的 EDAC 检查，两 seed 均 pass，非清单用例。）

## 2. 非法 SKIP 核查

两 seed 的 SKIP 数 = **0**（机械核验：两份 summary 的 result 列只有 pass / timed out，
日志中 `result: skip` 计数为 0）。本机 spec = SVE-256（`--dump-cpu-info`：`ARM64 spec:
SVE yes (max VL 256 bits)`），清单内所有 SVE1 门（`ARM64_REQUIRES_SVE` + 128..1024
范围门）本档全开，无 HWCAP 类意外 SKIP——符合预期（sve512_* 家族为 All-VL 自适应
几何，非 min-512 门，正常执行）。

## 3. 7 个无 tag ipsec 变体（VL 不敏感性注记，一次说明）

`ipsec_3des_docsis_aes_cmac_sve_wide` / `ipsec_3des_docsis_aes_xcbc_sve_wide` /
`ipsec_aes128_cbc_aes_cmac_sve` / `ipsec_aes192_cbc_xcbc_96_sve` /
`ipsec_aes192_ctr_xcbc_sve` / `ipsec_aes192_gcm_sve` / `ipsec_aes192_gcm_sve_wide`
—— 无 SVE 数据面（OpenSSL EVP 内部分派，Task 1 枚举 §2.2 已核），三档 VL 行为完全相同，
其格不构成 VL 敏感性证据，只构成"同条件可跑绿"证据。其中 4 件因 600‰ 负载几何 TIMEOUT
（见 §4/§5）——那是负载边界现象，与 VL 无关，不改变本注记。

## 4. TIMEOUT 格（25 个，全部同一根因类）

**现象**：`result: timed out`，`state.iteration: 0`（首个框架迭代即未完成），
runtime 317-414 s（超时预算 300 s + 杀线程收敛延迟）。**根因（环境/边界类，非代码 bug）**：
600‰（或 650‰/900‰）内存自适应几何把单次迭代的工作量放大到全核带宽/吞吐无法在 300 s
内完成一轮——与 Task 1 已证实的 `acl_gemm_sve` 超时同一机制（auto-dim 撞物理内存比例）。
几何由物理内存比例与全机线程数推导（日志 `[压力映射] … (来源: 内存自适应 600‰, 全机
127 线程)`），**与 RNG seed 无关** → 25 格集合跨 seed 确定是结构性的；seed1 全量实测
（本表），废弃的 seed2 尸体在其死亡点（第 15 格）之前的超时格与前一会话比对一致
（尸体已按裁定 D 被重跑覆盖，该比对结论以此记录为准）。**处置**：按控制器裁定，
逐格以有界 knob 单测重跑取真实结果（§5）。

| # | 测试 | knob（clamp） | 自适应值（日志实拍） | runtime ms (seed1) | 有界重跑结果（seed1 单测 + seed2 腿内，§5） |
|---|---|---|---|---|---|
| 1 | agu_stress_2src_sve | count [1024,32M] | 7,757,172 → 36.7 GiB | 332,666 | pass @ count=1024 |
| 2 | agu_stress_store_only_sve | count [1024,32M] | 7,757,172 → 36.7 GiB | 331,776 | pass @ count=1024 |
| 3 | arm0102_fma_f32_sve | count [1024,64M] | 15,514,344 → 36.7 GiB | 335,096 | pass @ count=1024 |
| 4 | arm0102_kreg_logic_sve | count [1024,64M] | 15,514,344 → 36.7 GiB | 336,061 | pass @ count=1024 |
| 5 | arm0102_kreg_mask_sve | count [1024,64M] | 同族 | 333,493 | pass @ count=1024 |
| 6 | arm0102_kreg_not_sve | count [1024,64M] | 同族 | 335,840 | pass @ count=1024 |
| 7 | arm0102_kreg_select_sve | count [1024,64M] | 同族 | 413,431 | pass @ count=1024 |
| 8 | eigen_svd_bidiag_sve | mdim [64,2048] | 2048（撞上限）→ 23.8 GiB | 331,759 | pass @ mdim=64 |
| 9 | eigen_svd_cdouble_sve | mdim [300,6000] | ≈1277（自有 0.65 RAM 模型） | 325,054 | pass @ mdim=300 |
| 10 | gmp_bigadd_sve | limbs [256,1M] | 1,048,576 → 4.0 GiB | 317,632 | pass @ limbs=256 |
| 11 | ipsec_3des_docsis_aes_cmac_sve_wide | data_size [1024,16M] | 16,777,216 → 11.9 GiB | 321,174 | pass @ data_size=1024 |
| 12 | ipsec_3des_docsis_aes_xcbc_sve_wide | data_size [1024,16M] | 16,777,216 → 11.9 GiB | 321,169 | pass @ data_size=1024 |
| 13 | ipsec_aes192_cbc_xcbc_96_sve | data_size [1024,16M] | 16,777,216 → 11.9 GiB | 321,053 | pass @ data_size=1024 |
| 14 | ipsec_aes192_ctr_xcbc_sve | data_size [1024,16M] | 16,777,216 → 11.9 GiB | 321,048 | pass @ data_size=1024 |
| 15 | mem_disambig_alu_sve | buffer_bytes [4096,1TB] | 60% 物理内存 ÷2 消费者 | **1,279,904** | pass @ buffer_bytes=4096 |
| 16 | mesh_upi_sve_sym | buffer_bytes [1M,16TB] | 90% 物理内存 ÷2 路 mesh | 320,799 | pass @ buffer_bytes=1M |
| 17 | movmskpspd_sve | count [1024,64M] | 9,696,465 → 36.7 GiB | 322,724 | pass @ count=1024 |
| 18 | sve_rot_ldr_at_top | slots [1024,1M] | 1,048,576 → 23.8 GiB | 333,131 | pass @ slots=1024 |
| 19 | sve_rot_ldr_at_top_rowmajor | slots [1024,1M] | 同族 | 332,961 | pass @ slots=1024 |
| 20 | sve_rot_ldr_at_top_rowmajor_k3res | slots [4096,1M] | 同族 | 329,613 | pass @ slots=4096 |
| 21 | sve_rot_ldr_at_top_rowmajor_k5inter_rand | slots [1024,1M] | 同族 | 331,102 | pass @ slots=1024 |
| 22 | sve_rot_ldr_at_top_rowmajor_k7p_hiham | slots [1024,1M] | 同族 | 316,077 | pass @ slots=1024 |
| 23 | sve_rot_ldr_at_top_rowmajor_k7p_one | slots [1024,1M] | 同族 | 317,381 | pass @ slots=1024 |
| 24 | sve_rot_ldr_at_top_rowmajor_k7p_rand | slots [1024,1M] | 同族 | 365,804 | pass @ slots=1024 |
| 25 | sve_rot_ldr_at_top_rowmajor_k7p_zero | slots [1024,1M] | 同族 | 318,058 | pass @ slots=1024 |

（k3res 的 slots clamp 下限为 4096——源码 `sve_rot_ldr_at_top_rowmajor_k3res.cpp:75`
`[4096, 1<<20]`，本表初稿误记 [1024,1M]，已按源码更正；其余 24 项 clamp 均已逐一
对照源码核实。）

**异常子类（#15 mem_disambig_alu_sve）**：超时杀灭耗时 1280 s（预算的 4.3 倍），
`avg-freq-mhz: 0.0`，cpu-mask 有空洞（部分 CPU 未标记）——工作线程对超时杀灭响应迟滞
（随机数据依赖地址的 asm 循环），框架最终仍收尸并记录 TimedOut。作为边界候选记入，
其有界重跑见 §5；杀灭迟滞本身单列一条观察（不阻断，供 Task 7 评估是否需框架侧修复）。
**另注**：seed2 首跑尸体恰好死亡于此格（见头部环境事件 2——死亡时刻落在本格已知
1280 s 杀灭迟滞窗口内，相关但因果未证），佐证该观察值得 Task 7 关注。

## 5. TIMEOUT 格有界重跑（控制器裁定）

**协议（裁定 D 值协议）**：机器空闲、逐测单独跑（全核）、同 seed 复现矩阵格：
`-e <test> -t 2000 -s LCG:1001 -O <test>.<knob>=<有界值> -Y -o /tmp/bounded1001/<test>.log`；
取值从 knob 的 **clamp 下限**起步，若 FAIL 则 ×2 逐级上调直至 pass（记录每级原因）。
**实测：25 项全部在下限一次通过，零上调**——每测日志的
`[压力映射] <test>.<knob> = <val> (来源: 命令行/配置文件, 有效 <lo>..<hi>)` 行
（eigen_svd_cdouble_sve 为 `I> M_DIM 300` 行）证明 -O 覆盖确实生效。

| # | 测试 | 有界值（=clamp 下限） | seed1 重跑 | 迭代块数 | runtime ms |
|---|---|---|---|---|---|
| 1 | agu_stress_2src_sve | count=1024 | exit: pass | 2 | 2,022.9 |
| 2 | agu_stress_store_only_sve | count=1024 | exit: pass | 3 | 2,044.4 |
| 3 | arm0102_fma_f32_sve | count=1024 | exit: pass | 5 | 2,019.7 |
| 4 | arm0102_kreg_logic_sve | count=1024 | exit: pass | 6 | 2,019.4 |
| 5 | arm0102_kreg_mask_sve | count=1024 | exit: pass | 6 | 2,018.1 |
| 6 | arm0102_kreg_not_sve | count=1024 | exit: pass | 6 | 2,017.1 |
| 7 | arm0102_kreg_select_sve | count=1024 | exit: pass | 6 | 2,018.6 |
| 8 | eigen_svd_bidiag_sve | mdim=64 | exit: pass | 196 | 1,985.6 |
| 9 | eigen_svd_cdouble_sve | mdim=300 | exit: pass | 2 | 3,148.1 |
| 10 | gmp_bigadd_sve | limbs=256 | exit: pass | 9 | 2,015.1 |
| 11 | ipsec_3des_docsis_aes_cmac_sve_wide | data_size=1024 | exit: pass | 3 | 2,044.3 |
| 12 | ipsec_3des_docsis_aes_xcbc_sve_wide | data_size=1024 | exit: pass | 2 | 2,186.8 |
| 13 | ipsec_aes192_cbc_xcbc_96_sve | data_size=1024 | exit: pass | 2 | 2,287.8 |
| 14 | ipsec_aes192_ctr_xcbc_sve | data_size=1024 | exit: pass | 2 | 2,132.5 |
| 15 | mem_disambig_alu_sve | buffer_bytes=4096 | exit: pass | 3 | 2,027.5 |
| 16 | mesh_upi_sve_sym | buffer_bytes=1048576 | exit: pass | 7 | 2,018.5 |
| 17 | movmskpspd_sve | count=1024 | exit: pass | 2 | 2,051.5 |
| 18 | sve_rot_ldr_at_top | slots=1024 | exit: pass | 3 | 2,026.1 |
| 19 | sve_rot_ldr_at_top_rowmajor | slots=1024 | exit: pass | 3 | 2,048.1 |
| 20 | sve_rot_ldr_at_top_rowmajor_k3res | slots=4096 | exit: pass | 2 | 2,162.9 |
| 21 | sve_rot_ldr_at_top_rowmajor_k5inter_rand | slots=1024 | exit: pass | 3 | 2,038.4 |
| 22 | sve_rot_ldr_at_top_rowmajor_k7p_hiham | slots=1024 | exit: pass | 3 | 2,040.6 |
| 23 | sve_rot_ldr_at_top_rowmajor_k7p_one | slots=1024 | exit: pass | 3 | 2,030.6 |
| 24 | sve_rot_ldr_at_top_rowmajor_k7p_rand | slots=1024 | exit: pass | 3 | 2,044.3 |
| 25 | sve_rot_ldr_at_top_rowmajor_k7p_zero | slots=1024 | exit: pass | 3 | 2,047.7 |

**结论**：25/25 在 clamp 下限全绿、零上调。§4 的根因判定（内存自适应几何放大，
非代码 bug）由此坐实：同一二进制、同 seed、同 VL，仅把几何收到有界值即稳定通过，
无任何 fail/crash。

**seed2 侧（裁定 D 落地）**：上述 25 项有界值 + `acl_gemm_sve.dim=512` 共 26 项 `-O`
全部烧入 seed2 全量重跑命令（`--ignore-timeout` 兜底）——seed2 的这 25 格为**腿内
有界实测**（与 seed1 的"腿外单测重跑"互补，两腿各自真实执行）。重跑命令与结果见 §1
表 seed2 行与 summary 文件。

seed2 腿内有界 25 格结果：**25/25 pass，0 timeout / 0 fail**（机械提取自
`native-256-LCG1002.summary.txt`，迭代块数/runtime ms 逐格如下）：
agu_stress_2src_sve 1 块/2,043.0；agu_stress_store_only_sve 1/2,004.5；
arm0102_fma_f32_sve 4/1,962.3；arm0102_kreg_logic_sve 5/1,893.7；
arm0102_kreg_mask_sve 5/1,963.3；arm0102_kreg_not_sve 5/1,902.3；
arm0102_kreg_select_sve 4/1,848.9；eigen_svd_bidiag_sve 82/858.7；
eigen_svd_cdouble_sve 1/3,073.8；gmp_bigadd_sve 7/1,747.6；
ipsec_3des_docsis_aes_cmac_sve_wide 2/2,003.7；ipsec_3des_docsis_aes_xcbc_sve_wide 1/2,184.6；
ipsec_aes192_cbc_xcbc_96_sve 1/2,299.5；ipsec_aes192_ctr_xcbc_sve 1/2,154.5；
mem_disambig_alu_sve 2/1,989.9；mesh_upi_sve_sym 6/1,948.0；movmskpspd_sve 1/2,048.2；
sve_rot_ldr_at_top 2/2,029.6；sve_rot_ldr_at_top_rowmajor 2/2,021.5；
sve_rot_ldr_at_top_rowmajor_k3res 1/2,154.6；…_k5inter_rand 2/2,019.2；
…_k7p_hiham 2/1,989.8；…_k7p_one 2/2,002.1；…_k7p_rand 2/1,997.4；…_k7p_zero 2/2,031.1。
`--ignore-timeout` 兜底未被触发（全程 0 次 TimedOut）。

## 6. FAIL / CRASH / 意外 SKIP

无（两 seed fail=0 crash=0 skip=0；机械核验：两日志 `result: fail|crash|skip|interrupted|
operating system error` 计数全 0，两 summary result 列无此类值）。无 crash 类需复现
定性的项目。

## 7. 结论

**native 256 档两 seed 全绿，Task 2 收口**：

- **绿格（矩阵 190 清单格 × 2 seed = 380 格）**：
  - seed1（LCG:1001）：165 格腿内直绿 + 25 格有界重跑绿（§5 表，全部 clamp 下限一次
    通过、零上调）= **190/190**；另有框架追加 mce_check 绿。
  - seed2（LCG:1002，26 项有界重跑腿）：190 格腿内直绿（25 格以腿内有界值实测）=
    **190/190**；mce_check 绿。`exit: pass`（exit=0），`--ignore-timeout` 未触发。
- **边界格 = 0**（25 个超时格全部成功有界化，无"找不到有界值"的残留）。
- **代码 bug 候选 = 0**：两 seed 无 fail/crash；25 个超时格经有界重跑全部转绿，坐实
  根因为内存自适应负载几何（环境/边界类），非功能缺陷。
- **移交 Task 7 的观察项（非阻断）**：① mem_disambig_alu_sve 超时杀灭迟滞（1280 s，
  cpu-mask 空洞、avg-freq 0.0）——框架侧可评估改进；② seed2 首跑死亡时刻与该迟滞
  窗口重合（环境事件 2，因果未证）；③ 自适应几何与 300 s 超时预算的组合在 62 GiB
  node0-only 主机上天然产生 25 格超时——后续腿（128/512）如遇同类格直接按本表
  clamp 下限协议有界化即可。
- **证据文件**：`native-256-LCG1001.summary.txt` / `native-256-LCG1002.summary.txt`
  （extract-summary.sh 机械提取，各 190 行，exit 行在内）；`native-256-LCG1002.stdout.txt`
  （重跑腿 stdout，含 exit: pass）；152/376 MB 原始 yaml 日志留工作区不入库
  （`.gitignore`）。seed1 的 /tmp stdout 已随重启丢失，其 `exit: pass` 证据由日志内
  exit 行 + summary 头承担。
