# droop-excite — 瞬态电压骤降（droop/di/dt）激发实验工具

Spec: `docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md`
（0103 专用；设计/护栏/测量边界以 spec 为准）

**与 sdcshield 解耦**：仅经 CLI 调用 `builddir/sdcshield` 二进制（`--repo`
指定），零构建/代码依赖；无 sdcshield 时调制器/记录器/分析器仍可独立运行。

## 组成
| 文件 | 职责 |
|---|---|
| droop_mod.c | 周期级负载调制器（aarch64 NEON；指令计数门控，理论 0.1Hz-325MHz、0103 实测硬顶 ~228MHz[最小 iters]，高频直射欠靶由驱动 refine 闭环修正） |
| bmc_logger.py | BMC 传感器轮询 → CSV（≥3s 节奏；in-band ipmitool，root） |
| run_sweep.py | 战役驱动（P0-P4 阶段、护栏、心跳、SDC 停机取证） |
| analyze.py | 分层排序表（ΔI / V_sag / 事件） |

## 构建（仅 aarch64；x86 编译期 #error，不进 meson）
    gcc -O2 -Wall -Wextra -Werror -std=c17 -pthread -lm -o droop_mod droop_mod.c

## 快速开始（0103，root）
    python3 run_sweep.py --phase p2 --dry-run        # 先看命令
    sudo python3 run_sweep.py --phase p0 --data-root /home/sdc/droop-excite
    sudo python3 run_sweep.py --phase p1
    python3 analyze.py /home/sdc/droop-excite/p1_*

## droop_mod 参数
| 参数 | 说明 |
|---|---|
| --cores 0-15 | 逗号/区间核列表（0103: socketA=0-63，socketB=64-127） |
| --threads N | 线程数（默认=cores 数） |
| --duration S | 运行秒数 |
| --freq-hz F --duty D | 目标频率/占空比（独立校准换算；高频绝对精度有限，摘要报告实测值，驱动闭环修正） |
| --on-iters/--off-iters N | 精确迭代数（相位的真相源） |
| --payload dep\|ilp2 | 依赖 FMA 链（最大翻转）/ 4 独立链（最大吞吐） |
| --off-mode stall\|light | yield 停顿（全摆幅）/ GPR 链（部分深度） |
| --resync N | 每 N 周期票据自旋重同步（0=关） |
| --calibrate / --self-test | 校准打印 / 功能自检 |

退出码：0 正常；1 用法/内部错误；**2 载荷校验失败（疑似 SDC）**。

## 护栏（run_sweep，触发即中止当步）
CPU Core Rem ≥95℃（BMC 临界 105）｜N_VDDAVS ≤0.80V（BMC 下限 0.770）｜
双 PSU 总和 ≥1300W（P0 实测后写 `data-root/thresholds.json` 收紧）｜
SDC 疑似 → 停机取证（证据在 `steps/<id>/`）。

## 测量边界（如实）
BMC ~3s 采样：能验证满载 DC 压降、低频（0.1-1Hz）min/max 电平差与 ΔI、
温度/功率护栏；**不能**测 50-150MHz 纹波幅度（差 7 个数量级，需示波器探
VRD 测点，超出远程能力）。高频段证据 = 功能层（eigen golden SDC / 调制器
exit 2 / RAS / 稳定性）。多核相位相干度不可远程测量。
"最大 dv/dt 配置"判据 = 低频 ΔI 标定 + DC 压降深度 + 功能事件，分层呈现。
0103 V 通道实测（P0，2026-10-28）：N_VDDAVS 轨量化平坦（满载 +144W 仍恒
0.910/0.890），VDDAVS 轨响应负载（0.890→0.860，30mV）——V_sag 看 VDDAVS 轨
（analyze 的 vddavs_sag_* 指标；他板的 -0.02V 先例对应的是响应轨）。
