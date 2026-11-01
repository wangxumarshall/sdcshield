# SVE 全 VL 验证运动 — native 128 腿分诊表（Task 3）

- 日期：2026-10-29（Task 3 产物）
- 分支：`feat/sve-all-vl-verification-2026-10-28`
- 二进制：`./builddir/sdcshield`（version `sdcshield-12ccda540fd4`，与 Task 1/2 同源）
- 上游：plan `docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md` Task 3；
  brief `.superpowers/sdd/2026-10-28-sve-all-vl-verification-campaign/task-3-brief.md`
  + 控制器裁定（**两条腿自始烧入全部 26 项有界覆盖**（逐字拷贝自 seed2 重跑命令行）+
  `--ignore-timeout` 兜底；裁定 E=日志不入库、summary 入库；stdout 跑完立即回拷 campaign 目录）
- 主机状态：128 核 / 62 GiB（node0-only），127 CPU 在线，两腿启动前全核空闲
  （seed1 前 load 0.30；seed2 前 load 6.07 且无 sdcshield/扫频进程）。本 boot 始于
  ~09:01（与 triage-256 环境事件 2 记录的重启同一 boot，up 序列吻合）。
  **环境事件（如实记录）**：seed1 完成与 seed2 启动之间（11:04–11:07），出现外来
  单测负载（用户 sdc：`./builddir/sdcshield -e mesh_upi_sve_asymm_write_int -t 25000
  -o /tmp/aw7.yaml`，全核）。按环境纪律**等待其退出后**（~11:07 消失）才启动 seed2
  ——两腿均未与任何外来负载重叠，无污染。
- **降档机制（Step 1 前置确认，实拍）**：
  `SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --dump-cpu-info` →
  `ARM64 spec: SVE yes (max VL 128 bits) | SVE2 no | SME no | SME2 no`。
  env 前缀不是 argv 的一部分，故日志 command-line 头**不含**它——本节即其权威出处，
  框架据此把 spec max 降到 128 并在 test_init 前钉住控制线程 VL（Task 1 已实证机制）。

## 1. 全量跑（2 seeds，真实命令与结果）

```bash
LIST=docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt   # 190 用例
LOG=docs/superpowers/inventory/sve-all-vl-campaign/native-128-LCG1001.log
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --test-list-file $LIST \
    -t 2000 -s LCG:1001 -o $LOG -Y --ignore-timeout \
    -O acl_gemm_sve.dim=512 -O agu_stress_2src_sve.count=1024 \
    -O agu_stress_store_only_sve.count=1024 -O arm0102_fma_f32_sve.count=1024 \
    -O arm0102_kreg_logic_sve.count=1024 -O arm0102_kreg_mask_sve.count=1024 \
    -O arm0102_kreg_not_sve.count=1024 -O arm0102_kreg_select_sve.count=1024 \
    -O eigen_svd_bidiag_sve.mdim=64 -O eigen_svd_cdouble_sve.mdim=300 \
    -O gmp_bigadd_sve.limbs=256 \
    -O ipsec_3des_docsis_aes_cmac_sve_wide.data_size=1024 \
    -O ipsec_3des_docsis_aes_xcbc_sve_wide.data_size=1024 \
    -O ipsec_aes192_cbc_xcbc_96_sve.data_size=1024 \
    -O ipsec_aes192_ctr_xcbc_sve.data_size=1024 \
    -O mem_disambig_alu_sve.buffer_bytes=4096 \
    -O mesh_upi_sve_sym.buffer_bytes=1048576 -O movmskpspd_sve.count=1024 \
    -O sve_rot_ldr_at_top.slots=1024 -O sve_rot_ldr_at_top_rowmajor.slots=1024 \
    -O sve_rot_ldr_at_top_rowmajor_k3res.slots=4096 \
    -O sve_rot_ldr_at_top_rowmajor_k5inter_rand.slots=1024 \
    -O sve_rot_ldr_at_top_rowmajor_k7p_hiham.slots=1024 \
    -O sve_rot_ldr_at_top_rowmajor_k7p_one.slots=1024 \
    -O sve_rot_ldr_at_top_rowmajor_k7p_rand.slots=1024 \
    -O sve_rot_ldr_at_top_rowmajor_k7p_zero.slots=1024
# seed2：同命令改 -s LCG:1002 → native-128-LCG1002.log
```

26 项 `-O` 逐字拷贝自 256 腿 seed2 重跑命令行（`native-256-LCG1002.stdout.txt` 头，
含 k3res slots=4096 这一已源码核实的 clamp 下限）。`-O` 生效有日志实拍（如
`[压力映射] ipsec_aes192_ctr_xcbc_sve.data_size = 1024 (来源: 命令行/配置文件, 有效
1024..16777216)`）。

| seed | 覆盖 | pass | timeout | fail | crash | skip | 退出 | 时长 |
|---|---|---|---|---|---|---|---|---|
| LCG:1001 | 190/190 (+mce_check) | 189 | 0 | 0 | 0 | **1**（§2） | `exit: pass`（exit=0） | 10:16:30–10:55:55 ≈ 39m25s |
| LCG:1002 | 190/190 (+mce_check) | 189 | 0 | 0 | 0 | **1**（§2） | `exit: pass`（exit=0） | 11:08:20–11:47:52 ≈ 39m32s |

- 两腿迭代块数：seed1 2486 块 / seed2 2792 块（块数随 seed 几何波动，正常）；
  `--ignore-timeout` 兜底**均未被触发**（全程 0 次 TimedOut）——256 腿的 25 个自适应
  几何超时格在本腿全部以腿内有界值直绿，佐证 triage-256 §4 的根因判定（环境/边界类，
  与 seed 无关亦与 VL 无关）。两腿时长 39m25s/39m32s，与控制器预期的 ~40 min 吻合。
- 原始 yaml 日志（383/358 MB）不入库（裁定 E）：`.gitignore` 屏蔽 `*.log`，
  代之以 `extract-summary.sh` 机械提取的 `native-128-LCG<seed>.summary.txt`
  （各 190 行，行数校验通过）；stdout（含 `exit: pass` 行）跑完即回拷 campaign 目录。

## 2. SKIP 格（1 个，架构性合法——本腿最重要的发现）

**`sve512_fmmla_arm`**：`skip-category: CpuNotSupported`，理由原文：
`sve512_fmmla_arm: SVE vector length 128 bits is outside the supported 256..1024-bit range`。
**两 seed 同格同理确定性复现**（seed1：4.6 ms 即出块；seed2：2.6 ms；summary 行均为
`skip 1`）。

- **合法性判定：合法（架构性），非分发 bug、非缺陷**。源码
  `tests/cpu/arm64/sve512_fmmla_arm.cpp:196`：`arm64_require_sve_vl(test, 256, 1024)`，
  其上注释明确 "F64MM needs a 256-bit segment to hold its 2x2 double matrix: at
  [128-bit] the instruction is UNDEFINED there, not a no-op). Gate from 256."
  ——`svmmla_f64` 在 VL<256 时指令 UNDEFINED（CLAUDE.md 已记录的基线探测结论同源），
  128 档唯一诚实的结局就是 SKIP。
- **交叉证据**：同一测试在 256 腿两 seed 均 pass（seed2：104 块/573.7 ms）——门只挡
  128，不误伤更高档。
- **矩阵记账**：(sve512_fmmla_arm, 128) 格 = **合法 SKIP（架构下界）**，128 腿证据
  矩阵 = 189 直绿格 + 1 合法 SKIP 格；其余 189 用例无任何 SKIP——本机 128 档
  SVE1 门（`ARM64_REQUIRES_SVE` + 128..1024 范围门）全开，无 HWCAP 类意外 SKIP。
- Task 1 枚举未单列此格的 128 档预期（enumeration.md 无 fmmla 条目）——本腿以实测
  补记：该格是**预期内的档位能力边界**，Task 8 汇总矩阵应把它记为 SKIP(legal) 而非
  空格或红格。

## 3. watchpoint 核查（brief 特别关注项）

1. **eigen per-tier 分发 @128——通过，128 变体被真实执行**：
   `eigen_svd_cdouble_sve` 与 `eigen_gemm_double_max_arm` 的每个 worker 各打一条
   分发实据（`log_info`，**两 seed 各 127 条/测试**，与 127 在线 CPU 一一对应）：
   - `eigen_gemm_double_max_arm: Eigen SVE gemm variant = 128-bit` × 127（×2 seed）
   - `eigen_svd_cdouble_sve: Eigen SVE svd variant = 128-bit` × 127（×2 seed）
   两测试结果均 pass（gemm：seed1 1 块/16.6 s、seed2 1 块/15.7 s；svd：seed1
   1 块/3.25 s、seed2 1 块/3.24 s，mdim=300 有界）——分发器选中 128 变体并跑完，
   **无 SKIP/FAIL，非分发 bug**（若 1024 位才会触发 ptranspose 上游缺陷的合法
   SKIP，本档不涉及）。
2. **sleef_sve 的 128..1024 门——通过**：`arm64_require_sve_vl(test, 128, 1024)`
   生效（源码下限即 128，本档恰在下限上），128 档被执行：pass，seed1 1 块/12.9 s、
   seed2 1 块/10.2 s。未出现越门 SKIP。
3. **26 项有界覆盖腿内实测**（两 seed）：全部 pass、全部秒级（2.0±0.2 s 量级；
   最长 acl_gemm_sve 21.3/20.1 s、eigen_svd_cdouble_sve 3.25/3.24 s），零超时零
   上调——与 256 腿 seed2 的同格数字同量级（如 mem_disambig_alu_sve 128 腿
   1.94/1.91 s vs 256 腿 1.99 s；eigen_svd_bidiag_sve 128 腿 77/83 块 vs 256 腿
   82 块）。
4. **非法 SKIP**：0（唯一 SKIP 见 §2，合法；两 seed 同格同理，确定性复现）。

## 4. FAIL / CRASH / TIMEOUT

无（两 seed fail=0 crash=0 timeout=0；机械核验：日志 `result: fail|crash|skip|
interrupted|operating system error|timed out` 计数——skip 仅 §2 的 1 条，其余全 0）。

## 5. 结论

**native 128 档两腿收口：189/190 直绿 × 2 seed + 1 个架构性合法 SKIP 格（fmmla@128）
+ mce_check 绿 × 2，`exit: pass` × 2，`--ignore-timeout` 未触发。**

- **绿格（矩阵 190 清单格 × 2 seed = 380 格）**：seed1 189 直绿（26 格为腿内有界）；
  seed2 189 直绿（同）。两 seed 合计 378 绿格。
- **合法 SKIP 格：2 格**（两 seed 各 1，同一格 sve512_fmmla_arm@128，§2 架构下界）。
- **边界格 = 0、代码 bug 候选 = 0**：256 腿的 25 个超时格在 128 腿零超时（有界覆盖
  生效），无 fail/crash；eigen 分发锚点实据 128 变体被执行。
- **与 256 腿的对照结论**：190 用例中 189 个 VL 敏感执行路径在 128/256 两档全绿；
  唯一档位差 = fmmla 的 F64MM 架构下界（256 起步）——这正是全 VL 矩阵要刻画的东西。
- **证据文件**：`native-128-LCG1001.summary.txt` / `native-128-LCG1002.summary.txt`
  （extract-summary.sh 机械提取，各 190 行，exit 行在内）；`native-128-LCG1001.stdout.txt`
  / `native-128-LCG1002.stdout.txt`（腿 stdout，含 `exit: pass`；跑完即时回拷）。
  原始 yaml 日志留工作区不入库（`.gitignore`）。
