# SVE 全 VL 验证运动（128/256/512 三档真实证据）— 设计文档

日期：2026-10-28。前置讨论：本会话头脑风暴（方案 A′/A/C 对比获批）。
计划文档（后续）：docs/superpowers/plans/（writing-plans 产出）。

## 1. 目标与非目标

**目标**：188 个 SVE1 用例在 VL = 128 / 256 / 512 三档全部获得**真实通过
证据**（564 格矩阵），验证过程中发现的缺陷即修（每个修复独立 commit
并在三档重验）。

**非目标（范围外，如实声明）**：
- 1024 档——用户矩阵限定 128/256/512；既有 gem5 kernel 证据覆盖 1024
  的（sve_arith_bounds/agu_2src/kreg_mask/sve_rot_vla/Phase1 三件/
  sleef/eigen）引用即可。
- SVE2/SME 4 件（sme_fmopa_stress_arm、sme_fmopa_za_arm、
  sme_za_tileview_arm、sve2_cross_precision_arm）——已有 Phase 1 /
  PR #242 gem5 四档实证记录，不新增工作。
- CHAOS 故障注入研究（双 kernel 深化路径）——方案 C 出圈，留待后续。

## 2. 事实基线（2026-10-28 本会话实测，全部真实输出）

| 探针 | 结果 |
|---|---|
| 本机（cortex x3b, 48-d02-00） | SVE-256 原生；无 SVE2/SME；页大小 4096 |
| crash 家族基线 | 当前 main 构建后 8 个 `sve_rot_ldr_*` 于原生 VL=256 全部 `exit: pass`——用户报告的 crash 来自 all-VL Phase 2 P5 迁移（2026-10-24）前的旧二进制 |
| 128 档机制 | `SDCSHIELD_ARM64_SPEC_MAX_VL=128` 实证：`ARM64 spec: SVE yes (max VL 128 bits)`，控制线程按 spec pin、worker 继承+复读校验 |
| 512 档 SE 真二进制 | **死于 `fatal: syscall prctl (#167) unimplemented`**（框架 `arm64_spec_get` 的 `PR_SVE_GET_VL`）——此即仓内"同源双 kernel"模式的根因 |
| FS 资产 | 本机 `gem5-fi/gem5-fs/` 为空；eigen FS 先例跑在别处 |
| 双 kernel 存量 | 本机 `gem5-fi/workloads/sdcshield/` 仅 17 个 kernel，未覆盖其余用例 |
| 用例全集 | 本机默认集 479 测试中 188 个 sve 命名；源码 SVE2/SME 命名仅 4 件 |

## 3. 用例枚举与清单工件

- **枚举原则**：tag 驱动——`arm64_requires` HWCAP_SVE 标签 ∪ 默认集
  sve 命名。171 个已打标签 vs 188 个 sve 命名的差异**必须显式调和**
  （如 5 个 `sve_rot_ldr_*` 走 `compiler_minimum_device` 路径），调和
  结论写入计划文档，不许静默取并集或交集。
- **清单工件**：`--list-tests` 输出过滤生成 `sve-test-list.txt`
  （`--test-list-file` 消费），入库到
  `docs/superpowers/inventory/sve-all-vl-campaign/`，与证据矩阵同目录，
  作为 564 格矩阵的行定义。
- **对账**：`scripts/check-arm64-requires.py` 跑通零差异后才算枚举完成。

## 4. native 腿执行规格（128 + 256，本机）

- **256 腿**：`./builddir/sdcshield --test-list-file sve-test-list.txt
  -t 2000`（全核；eigen 多线程已修复，不要求 `-n 1`）。
- **128 腿**：`SDCSHIELD_ARM64_SPEC_MAX_VL=128` 同规格。
- **多种子**：每档 ≥2 个 `-s <STATE>`（LCG/AES 状态串）。
- **运行预算**：每轮预计 1–3 小时（大缓冲 init 占大头），过夜跑；
  eigen 类用例按实测 mdim 表（memory: eigen-mdim-memory-measurement）
  或 `-O <test>.mdim` 控制内存。
- **判据**：每用例 `exit: pass`；异常 SKIP（HWCAP/能力类，本机不应
  出现）视为 tag 或探测 bug，进入 §6 修复循环。

## 5. gem5 512 腿（方案 A′ 主线，方案 A 兜底）

### 5a. prctl stub（gem5-fi 仓库）
syscall_emul 补 `prctl` 的 `PR_SVE_GET_VL`/`PR_SVE_SET_VL`：读写
ArmISA `sve_vl_se`，保证框架 pin→readback 校验链通过。重建
`gem5.opt`（~20–40 min），spike 复跑：
`G5=.../gem5.opt run_sve.sh builddir/sdcshield -e sve_rot_ldr_at_top
-t <短时> -n 1 -O sve_rot_ldr_at_top.slots=1024 --vl 4`。

### 5b. stub 链迭代
每个 spike 揭示下一个 fatal syscall 即补 stub（预估：
sched_setaffinity、forkfd/signalfd、futex 一类）。**时间盒约束：
累计 ~5–6 个 stub 仍不闭合 → 放弃 A′，切方案 A**（补齐 FS 镜像资产
+ `configs/se/arm_chaos_fs.py` 批量跑，eigen FS 先例）。

### 5c. 批量执行
64 路并行（gem5 单线程进程 × 本机 128 核），每用例一条
`run_sve.sh` 命令 + `-O` 降容（slots/mdim 按用例 knob 清单）；仿真
速率先用 3–5 个代表用例标定，再定 `-t`（预算：每用例仿真墙钟
≤30 min）。判据：`exit: pass` @ VL=512。

**仓库归属**：gem5-fi 侧改动（stub commit 序列、重建产物不入库）走
gem5-fi 仓库自己的规则与 commit 流；sdcshield 仓只入清单工件与测试
修复。

## 6. 发现-修复循环

- 每个发现 = 独立 commit（one patch per unit），修复后该用例**三档
  重验** + 同构族抽查——PR #294 教训：修复传播不完整（kreg 四兄弟、
  mask 尾批 slack 均为漏网案例）是真实风险。
- 预判残留类型：尾批全宽 ldr/str slack 漏网、VL 相关几何类、
  `arm64_requires` tag 类。
- 本机不可达路径（若有）如实声明验证边界，不许"SKIP 理由不变"冒充
  功能验证（CLAUDE.md rule 2 gem5-fi 条款）。

## 7. 验收判据

564 格矩阵（188 × 3）全绿；每格有可引用的真实命令与输出（native：
`exit: pass`；gem5：`exit: pass` @ `--vl 4`）。允许带**显式理由**的
边界格（如某用例经论证在本档不可达），理由必须是机制性的，不接受
"没跑完"。

## 8. 文档同步（大颗粒度修改触发）

- README：验证矩阵结论（188 × 3 全绿 + 证据索引）。
- CLAUDE.md 勘误两处：gem5-fi 本机实为 `/home/sdc/wangxu/gem5-fi`
  （文档写 operator/，以实测为准）；"`tests_sve` compiles at fixed
  128-bit; sleef_sve stays there by design" 已过时（sleef_sve 现在
  tests_sve_vla 真 VLA）。
- docs/research/progress-log.md：运动全程入档。

## 9. 风险与边界

| 风险 | 处置 |
|---|---|
| A′ stub 链深度未知 | 时间盒 ~5–6 个 stub，超限切方案 A（FS） |
| FS 资产本机缺失 | 方案 A 启动条件 = 镜像建设/搬运，先估算再动 |
| 仿真墙钟超预算 | 3–5 用例先标定；单用例 >30 min 降 `-t`/降容，再不行边界格如实声明 |
| 多线程路径 gem5 不可达 | 512 腿统一 `-n 1`（与 eigen FS 先例口径可并存：native 腿全核已覆盖多线程） |
| eigen mdim 内存 | 按实测表/`-O` 控制 |

## 10. 交付物

1. `sve-test-list.txt` 清单 + 调和结论（sdcshield 仓，campaign 分支，
   `docs/superpowers/inventory/sve-all-vl-campaign/`）。
2. gem5-fi prctl/syscall stub commit 序列（gem5-fi 仓）。
3. 564 格证据矩阵（MD 表 + 原始输出归档路径，同目录）。
4. 修复 commit 序列（发现即修，三档重验）。
5. 文档同步三件（README / CLAUDE.md 勘误 / progress-log）。
