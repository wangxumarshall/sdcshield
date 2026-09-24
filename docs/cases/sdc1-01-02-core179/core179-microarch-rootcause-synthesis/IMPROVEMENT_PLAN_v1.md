# 改进方案 v1（基于 REVIEW_v1.md + 源码/实跑独立验证）

原则：CLAUDE.md 补丁纪律，一单元一补丁、自验证、feature 分支、不推 main。所有修订须有真实命令/源码支撑，不杜撰。

## 已完成的独立验证（本会话，非声称）

| 验证项 | 方法 | 结果 |
|---|---|---|
| H5 golden 回归 | 本机 `gem5.opt`（2026-08-29 14:59 重建）实跑 `o3_chaos_smoke.py --no-fi` | `ptr_corrupt=0 val_mismatch=0 fails=0`，tick 30677000 退出 ✓ 与 §5.1 一致 |
| H5 byte_lane_skew oops 链 | 实跑 `--lsq-structural byte_lane_skew --lsq-skew 1 --lsq-fwd-prob 0.05` | 触发 `panic: Page table fault when accessing virtual address 0x7fbffefc48` ✓ 与 §5.1 一致 |
| 三注入器钩点源码 | grep `CHAOS/gem5/src/` | D1 `lsq_unit.cc:1498`、D2 `lsq.cc:1146`、D3 `table_walker.cc:1959` 全部命中 ✓ |
| rng-init-order 修复 | 源码 | `rng(rng_seed != 0 ? rng_seed : [](){...}())` lambda 形式已就位 ✓ |
| conditionalValidBit 模式 | `CHAOSPTW.cc:114-117` | 仅对 block desc（low2=0b01）bit0 单 XOR，gate 前 ECC 提前 return ✓ |
| D1 15:58 Hamming-0 | Python 复算 | `ror1(slot0)==x20`，Hamming=0 ✓ |
| D1 位翻转不可达性（增强） | XOR 距离分析 | `slot0 ^ x20` popcount = 30 → 纯位翻转模型需翻 30 bit；结构化置换需 0 bit |
| D1 0814 Hamming-0 | Python 复算 | best=ror6(slot1)，XOR 距离=6 bit，非 0 ✗ 论文过宣称 |
| opendcdiag YAML 交叉确认 | `ls /home/sdc/wangxu/opendcdiag-arm/*.yaml` | 本机不存在 ✗ 不可复现 |
| run_H7.sh 复现能力 | 读脚本 | 用 `o3_chaos_smoke.py`（SE），不能复现 FS 5-seed 表 ✗ |
| mmu.cc 翻译分派行号 | `sed -n` mmu.cc | 分派在 1226-1227（`translateMmuOff` 调用），非论文正文多处简写的 "1213" |

## 修订项（按严重度+ROI 排序，A=本机可立即执行）

### Patch 1（CRITICAL→MAJOR）：§3.2/§5.2 位翻转穷举边界闭合 + 0814 降级
F2 + F1 合并，二者同属 D1 签名论证，逻辑上一单元。

- §3.2：把"no single-byte bit-flip on slot[0] produces the observed value (exhaustive 8-byte × 256-mask test)"强化为更强且边界闭合的表述：
  - 新增 XOR 距离事实：`slot0 ^ x20` 的 popcount = 30，即任何纯位翻转（不引入字节置换）模型需翻整整 30 bit 才能从真值产生该签名；而一次结构化字节置换（ror1）以 0 bit 翻转达成。这是比"单字节不可达"更强、且穷举上界明确（30 bit，全空间）的论证。
  - 据此把 §5.2 的可证伪主张边界明确：主张适用于"位翻转模型（任意 k-bit，k<30）无法复现字节置换签名"；对 k-bit 翻转，k 的穷举上界为 30（全空间 XOR 距离）。
- §3.2/§MICROARCH_SUPPLEMENT §2.2 的 0814 案例：从"rol6(slot[1]) Hamming 距离 0，唯一匹配"降级为"最近候选为 ror6(slot[1])，XOR 距离 6 bit（跨字节分布）；0814 标【强推】而非【实锤】，未达 15:58 的 Hamming-0"。
- `reproduce_d1_forensic.sh`：注释更新说明仅复现 15:58（实锤），0814 因非 Hamming-0 未纳入复现脚本。

自验证：Python 复算脚本输出（已在本会话产出）作为证据嵌入注释/补充。

### Patch 2（MAJOR）：摘要/§1.2 贡献前置 single-case 限定 + D2 分级 + "localized"措辞降级
F3 + F4 合并，均为贡献陈述的措辞诚实化，一单元。

- 摘要：`localized the defect to three specific microarchitectural data paths` → `identified signatures consistent with three specific microarchitectural data paths (mechanism silicon-unverified; D2 silicon-evidence confounded by D1)`
- §1.2 贡献 1：每条加"（single-case study; method migration not demonstrated）"前缀限定；D2 在该条中标"simulation-exercisable candidate whose silicon evidence is confounded by D1"
- §1.3 与 §1.1 首句：点明 78 事件均同核多次转储（F10）

### Patch 3（MAJOR）：H7 措辞精确化（verified → directionally verified + caveat）
- §5.4/§7/摘要：H7 从"verified"加内部效度保留：ECC-on 路径未被注入扰动（gate 前 return），5/5 方向稳定但严格同路径对照未闭合。改为"directionally verified (ECC-on path undisturbed by injection; strict same-path contrast is future work)"

### Patch 4（MAJOR）：可复现性脚本诚实标注
- `run_H7.sh` 顶部注释：标注"此脚本在 SE 模式下运行，仅复现 SE null；论文 §5.4 的 FS 5-seed ECC 对照表需手工 `o3_chaos_fs.py` 长跑，无自动化脚本"（F6）
- §7：opendcdiag YAML 交叉确认标注本机不可达（F7）；BESTPAPER_PLAN 阶段4 状态改 ⚠️

### Patch 5（MINOR）：架构精确化
- §2.2/§5.3：澄清 gem5 `faults.cc:1087` 将完整 faultAddr 写入 FAR、不屏蔽 [63:60]，故 D2 仿真复现验证的是 byte7 清零→非规范 VA→translation fault 因果，非硅片 FAR 高 nibble 架构行为（F8）
- §3.3：TBI1 objdump 论证补"静态分析仅覆盖 `__cpu_setup` 初始化路径；runtime TCR_EL1 实测未做"（F9）
- mmu.cc 行号：正文多处"mmu.cc:1213"精确化为"mmu.cc:1226-1227（translateMmuOff 分派）"；但 §2.4 图注的 `mmu.cc:1213` 若指函数内大体位置可保留，需统一。

### Patch 6（MINOR）：数据完整化
- §7：把 BESTPAPER_PLAN 的 5 次 seed=0 spur 分布（1/1/1/2/1）移入正文（F11）

## 不在本次执行范围（future work，诚实保留）
- H6 guest-visible oops 谱（O3 fetch-stall 架构限制）
- 跨案例迁移（需第二台故障机）
- H7 严格同路径对照（需 non-cascading 故障模型）
- §2.5 耦合故障经典文献引用（F12，需文献检索）

## 执行顺序
Patch 1 → Patch 2 → Patch 3 → Patch 4 → Patch 5 → Patch 6，每个：编辑双语同步 → 自验证（grep 确认措辞落地、Python 复算嵌入）→ commit → push。
