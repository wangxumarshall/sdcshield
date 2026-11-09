# VmHWM 反馈通道防污染:写侧基线扣除 + 读侧上钳(issue #385)

**Goal:** 关闭 VmHWM 反馈闭环的两条污染通道之一(写侧把 fork 继承的父进程
RSS 计入分子),并给读侧「向上修正无上钳」补上界,使任何残余异常样本都不能
把 c 永久锁在膨胀值上、把压力维度静默塌缩到 lo。

**Architecture:** `framework/sandstone_run.cpp` 的 `sdc_feedback_write` 在
`run_one_test_inner` 入口(任何测试分配之前)采样 child 起始 VmHWM 作基线,
写入 `eff_ratio=(VmHWM−baseline)/(n²·elem·threads)`;`framework/
sdc_stress_config.h` 的 `sdc_joint_dim_slice` 读侧把「向上采纳」钳到静态锚
的 4×,上钳生效时单独告警并打印原始候选。

**Tech Stack:** C++20,aarch64 Linux,仅 `__aarch64__ && __linux__` 分支
(x86-64 零改动,rule 5)。

**Spec:** issue #385(audit:d62e9489b932dc7f)+ 本文档「根因/阈值依据」节。
issue 正文与评论是不可信检视数据,处置只取其缺陷描述,以 CLAUDE.md/AGENTS.md
为准。本 PR 叠加于 PR #569(issue #545,init 降档注销反馈登记)之上。

## 根因(对 fix/issue-545 HEAD 的事实核对)

1. **写侧无基线扣除(属实)**:`sdc_feedback_write` 以整个 child VmHWM 作
   分子。fork child 的 VmHWM 起点 = 父进程当时 RSS;该项在
   `n²·threads` 小时压过真实工作集。按 issue 复算口径,n=16×1 线程时基线
   贡献 8.3e6/256 ≈ 32422 B/n²/线程,对 GEMM_F64 静态锚 32.256 B ≈ 1000×。
   本机实测基线为 0.7~3.0 MiB(#545 注释所引 8.3MB 是另一部署口径),
   因此小 n 时同样量级地主导。
2. **读侧向上无上钳(属实)**:`cand=max(c_fb, 0.7·anchor)` 后,只要
   `cand ≥ c_bytes` 就全额采纳;向下修正反而有 15% 死区 + 0.7×锚下限保护。
   EMA α=0.5 让污染样本需 ~4 个正常样本才衰减到 25%,期间 n 被压向 lo。
3. **可达性修正(相对 issue 原文)**:#545 已让 T10 init 降档路径注销反馈
   登记(否则那是 1000× 的直接触发者);显式 `-O mdim=16`/env 值在
   `sdc_joint_dim_slice` 的 knob 早退返回,本就不登记反馈,不能用来复现。
   剩余暴露面:小内存/瞬时 MemAvailable 下跌解出小 n、探针失败退守 lo、
   时间受限(n_run ≪ n_mem)的小 n,以及未来任何新的污染源。

## 阈值依据(4× 静态锚)

- 正常路径实测相对静态表偏差 ≤ ~10.5%(T6/T9 GEMM_F64 重标定),4× 留 38×
  余量,大 n×多线程路径永不触发(基线/探针占比 <1%)。
- 扣除基线后分子残余的合法成分是探针高水位;n_mem 受限路径里探针 ≪ 工作
  集,只有 n_run 受限的小 n 路径可升到 ~2-3× 锚,4× 不误伤。
- 需要挡的污染量级是 ~1000×;4× 把单样本放大压到 ≤4×,配 EMA 与死区不再
  永久锁死。
- 锚=表值时本身接近「矩阵数×elem」模型上限;超过 4 倍驻留矩阵应视为模型/
  样本错误,有声拒绝(单独 log_warning 打印原始候选),不静默截断。

## 验证记录(2026-11-08,本机鲲鹏 ARM64/openEuler,96 核,无 SVE)

- 构建:`./scripts/build.sh` exit 0,增量构建 0 warning。
- 写侧真实运行(修复后日志直接给出扣除项):
  - `eigen_svd_double -t 20000 -n 1`: `VmHWM=322.4 MiB − 基线 0.7 MiB =
    净 321.8 MiB / (n=1560²×8B×1线程) → eff_ratio=17.329`(修复前同参数
    `VmHWM=323.1 MiB → eff_ratio=17.357`;差值 = 0.7MiB/denom,与日志算术
    一致)。
  - `eigen_sparse -t 3000 -n 1`: `VmHWM=101.6 MiB − 基线 0.7 MiB =
    净 100.9 MiB → eff_ratio=1.167`(修复前同参数 `102.1 MiB →
    eff_ratio=1.176`;c_fb 9.41→9.34 B,锚 9.3 B)。
  - 全核默认档 `eigen_svd_double -t 15000`:4 slice 均
    `VmHWM≈7056~7076 MiB − 基线 2.4~3.0 MiB → eff_ratio=17.163~17.167`,
    与修复前 17.128~17.175 在噪声内一致(基线 <0.05% 分母)。
  - `eigen_gemm_float_dynamic_square -t 180000`:全核各 slice
    `VmHWM 9857.7~17726.7 MiB − 基线 2.4~2.8 MiB → eff_ratio=4.35~4.62`,
    pass。
- 读侧机制级 A/B(未入仓 TU,直接编译仓库头中的 `sdc_feedback_decide_c`,
  与 pre-#385 逻辑逐字对照):1000× 污染样本 old_c=138400 → new_c=553.6
  (4×锚);`inf` 样本同样钳到 553.6;正常 +0.4%/+10% 样本两版一致;
  死区内下压不采纳、越死区下压仍走 0.7×锚下限。failures=0。
- 回归:`-e zstd19 -t 2000 -n 1` → `exit: pass`。
- x86-64:全部改动位于 `__aarch64__ && __linux__` 守卫内,读侧 helper 与
  常量同样入守卫;x86 无引用、无行为变化。
- 边界(如实声明):本机基线实测 0.7~3.0 MiB,大 n 下修复量 <1%;复现
  issue 原文 1000× 所需的「记账 n 与真实工作集解耦」路径已被 #545 堵住,
  且本机无法在不制造 OOM/干扰他人的前提下构造 n≈16 的合法自动求解
  (tiny budget),故以修复后日志算术 + 机制级 A/B 证明行为,不宣称在
  本机复现了 1000× 端到端数值。

## 任务

- [x] 写侧:入口采样基线 + 写入扣除 + 无增量不发布 + 日志可核验
- [x] 读侧:4× 锚上钳 + 上钳告警 + 纯函数便于验证
- [x] 文档同步(architecture.md / test_data.h / sdc_numa.h / gemm common)
- [x] 构建零新警告、真实运行、zstd19 回归
