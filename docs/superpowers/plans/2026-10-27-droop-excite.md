# droop-excite（瞬态电压骤降激发实验）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 0103 上交付"eigen 稳态满载 + 周期级负载调制器"的 droop/di/dt 激发实验独立工具（`tools/droop-excite/`，与 sdcshield 解耦），并执行 P0-P4 战役，产出最大 dv/dt 参数配置分层排序表与 SDC 狩猎结果。

**Architecture:** 方案 A（spec §5）——独立调制器 `droop_mod`（C17/NEON，指令计数门控，框架零改动）与 sdcshield 二进制 taskset 分核并行（仅 CLI 级耦合，无构建/代码依赖；无 sdcshield 时调制器/记录器/分析器仍可独立运行）；`bmc_logger` 以 ≥3s 节奏采集电压/电流/功耗/温度做反馈验证与护栏；`run_sweep` 驱动 P0-P4 阶段；`analyze` 产出分层排序表。BMC 测不到 50-150MHz 纹波幅度——边界全程如实声明。

**Tech Stack:** C17 + aarch64 NEON + pthread（gcc 12.3.1）；Python 3.11 标准库（unittest 测试）；ipmitool in-band（root）；sdcshield 现有二进制（零改动，仅 CLI 调用）。

**Spec:** `docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md`

## Global Constraints

- 仅在 0103 执行（spec D1）；代码只进 `tools/droop-excite/`（tools/ 下新目录，D5），**框架与现有测试零改动**（D2），x86 构建面不受影响（不进 meson）。
- 分支 `feat/droop-excite-2026-10-27`（已存在，spec 提交 60a38766）；**一补丁一单元**，`git commit -s`（末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By），每个验证过的提交即 push，永不 push main。提交前 `git branch --show-current` 自检。
- C 编译：`gcc -O2 -Wall -Wextra -Werror -std=c17 -pthread -lm`，零警告；非 aarch64 编译期 `#error`。
- Python 仅标准库；测试 `python3 -m unittest`。
- BMC 轮询间隔 ≥3s；护栏默认 {CPU 温 ≥95℃ 中止, N_VDDAVS ≤0.80V 中止, 双 PSU 总和 ≥1300W 中止}；SDC 疑似事件（eigen 非零退出 / 调制器 exit 2 / RAS 新事件）→ **停机取证**（spec D4），证据保全后等用户裁定，不得继续。
- 实验原始数据只落 `/home/sdc/droop-excite/`（仓库外）；代码与文档入仓库（D5）。
- 0103 拓扑（2026-10-27 实测固化，驱动运行时断言校验）：socket A = cpu0-63（physical_package_id=36），socket B = cpu64-127（=8442），4 个 cpufreq policy（0-31/32-63/64-95/96-127）全部 performance@2.6GHz；保留核 120-127（OS/驱动/记录器/会话）。
- 传感器名（0103 iBMC 6.70 in-band 实测）：`CPU1 N_VDDAVS`/`CPU2 N_VDDAVS`/`CPU1 VDDAVS`/`CPU2 VDDAVS`/`Power`/`Power2`/`PS2 IOut`/`CPU1 Core Rem`/`CPU2 Core Rem`。
- 每个行为验证必须引用真实运行输出（CLAUDE.md 规则 2）；大颗粒度文档同步（规则 7）。

## 文件结构

```
tools/droop-excite/
├── droop_mod.c                          # 调制器（Task 2）
├── bmc_logger.py                        # BMC 遥测轮询 → CSV（Task 1）
├── run_sweep.py                         # 战役驱动（Task 3/4）
├── analyze.py                           # 指标 + 分层排序表（Task 5）
├── README.md                            # 构建/运行/参数/边界（Task 6）
└── tests/
    ├── fixtures/sensor_list_0103.txt    # 0103 实测 ipmitool 输出（Task 1）
    ├── test_bmc_logger.py               # Task 1
    ├── test_droop_mod.sh                # Task 2（aarch64 编译+自测；x86 SKIP）
    ├── test_run_sweep.py                # Task 3/4
    └── test_analyze.py                  # Task 5
```

责任边界：`droop_mod.c` 只管"在指定核上按指令计数门控摆动电流"（含自校验与自校准）；`bmc_logger.py` 只管"把 BMC 遥测变成带时间戳的 CSV"；`run_sweep.py` 只管"步骤编排 + 护栏 + 落盘"（不解析传感器的业务含义，只调 `evaluate_guardrails`）；`analyze.py` 只管"从落盘数据算指标排座次"。四者只经 CSV/JSON 文件与 CLI 接口交互。

---

### Task 1: bmc_logger.py（BMC 遥测记录器）

**Files:**
- Create: `tools/droop-excite/.gitignore`
- Create: `tools/droop-excite/bmc_logger.py`
- Create: `tools/droop-excite/tests/fixtures/sensor_list_0103.txt`
- Test: `tools/droop-excite/tests/test_bmc_logger.py`

**Interfaces:**
- Consumes: 无（独立工具；in-band `ipmitool sensor list`，需 root）。
- Produces: `parse_sensor_list(text:str)->dict[str,str]`；`read_once(timeout=30)->dict`；`poll_loop(out_path, interval_min=3.0, max_samples=None)->int`；`CSV_COLUMNS`（`["ts"] + SENSORS`）。CSV 列名 = 传感器原名（后续任务依赖这些确切名字）。

- [ ] **Step 1: 写失败测试**

`tools/droop-excite/tests/test_bmc_logger.py`：

```python
#!/usr/bin/env python3
"""bmc_logger 单元测试：python3 -m unittest discover -s tools/droop-excite/tests"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import bmc_logger  # noqa: E402

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "fixtures", "sensor_list_0103.txt")


class TestParseSensorList(unittest.TestCase):
    def test_parses_real_0103_output(self):
        with open(FIXTURE) as f:
            vals = bmc_logger.parse_sensor_list(f.read())
        self.assertEqual(vals["CPU1 N_VDDAVS"], "0.910")
        self.assertEqual(vals["CPU2 N_VDDAVS"], "0.890")
        self.assertEqual(vals["CPU1 VDDAVS"], "0.890")
        self.assertEqual(vals["Power"], "270.000")
        self.assertEqual(vals["Power2"], "264.000")
        self.assertEqual(vals["PS2 IOut"], "19.980")
        self.assertEqual(vals["CPU1 Core Rem"], "63.000")

    def test_skips_na_and_malformed(self):
        text = ("Inlet Temp       | na         | degrees C  | na | x\n"
                "Bad line without pipes\n"
                "CPU1 Core Rem    | 63.000     | degrees C  | ok | x\n")
        vals = bmc_logger.parse_sensor_list(text)
        self.assertNotIn("Inlet Temp", vals)
        self.assertNotIn("Bad line without pipes", vals)
        self.assertEqual(vals["CPU1 Core Rem"], "63.000")

    def test_csv_columns_start_with_ts(self):
        self.assertEqual(bmc_logger.CSV_COLUMNS[0], "ts")
        for s in ("CPU1 N_VDDAVS", "CPU2 N_VDDAVS", "Power", "Power2"):
            self.assertIn(s, bmc_logger.CSV_COLUMNS)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 建 fixture（0103 实测原样）**

`tools/droop-excite/tests/fixtures/sensor_list_0103.txt`——内容为 2026-10-27 在 0103 上 `ipmitool sensor list` 的真实输出（grep Volts/Watts/degrees/Amp 子集），关键行如下（逐字）：

```
Outlet Temp      | 35.000     | degrees C  | ok    | na        | na        | na        | 75.000    | na        | na        
CPU1 Core Rem    | 63.000     | degrees C  | ok    | na        | na        | na        | 105.000   | na        | na        
CPU2 Core Rem    | 45.000     | degrees C  | ok    | na        | na        | na        | 105.000   | na        | na        
CPU1 MEM Temp    | 35.000     | degrees C  | ok    | na        | na        | na        | 95.000    | na        | na        
CPU2 MEM Temp    | 28.000     | degrees C  | ok    | na        | na        | na        | 95.000    | na        | na        
Power            | 270.000    | Watts      | ok    | na        | na        | na        | na        | na        | na        
Power2           | 264.000    | Watts      | ok    | na        | na        | na        | na        | na        | na        
PS2 IIn          | 1.200      | Amps       | ok    | na        | na        | na        | na        | na        | na        
PS2 IOut         | 19.980     | Amps       | ok    | na        | na        | na        | na        | na        | na        
PS2 POut         | 238.000    | Watts      | ok    | na        | na        | na        | na        | na        | na        
CPU1 N_VDDAVS    | 0.910      | Volts      | ok    | na        | 0.770     | na        | na        | 0.990     | na        
CPU2 N_VDDAVS    | 0.890      | Volts      | ok    | na        | 0.770     | na        | na        | 0.990     | na        
CPU1 VDDAVS      | 0.890      | Volts      | ok    | na        | 0.730     | na        | na        | 1.150     | na        
CPU2 VDDAVS      | 0.890      | Volts      | ok    | na        | 0.730     | na        | na        | 1.150     | na        
CPU1 VDDFIX      | 0.800      | Volts      | ok    | na        | 0.720     | na        | na        | 0.880     | na        
CPU2 VDDFIX      | 0.800      | Volts      | ok    | na        | 0.720     | na        | na        | 0.880     | na        
CPU1 HVCC        | 1.200      | Volts      | ok    | na        | 1.080     | na        | na        | 1.320     | na        
CPU2 HVCC        | 1.190      | Volts      | ok    | na        | 1.080     | na        | na        | 1.320     | na        
```

- [ ] **Step 3: 运行测试确认失败**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: FAIL/ERROR（`No module named 'bmc_logger'`）。

- [ ] **Step 4: 实现 .gitignore 与 bmc_logger.py**

先建 `tools/droop-excite/.gitignore`（防编译产物/字节码入库——后续任务会在本目录编译 droop_mod 二进制）：

```
/droop_mod
__pycache__/
*.pyc
```

再建 `tools/droop-excite/bmc_logger.py`：

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BMC 传感器轮询记录器（in-band ipmitool → CSV）。

0103 (iBMC 6.70) 实测传感器名（tests/fixtures/sensor_list_0103.txt）。
节奏保持 >= interval_min 秒（实测 <3s 会把 BMC 采样打超时）。
用法: sudo python3 bmc_logger.py --out /path/bmc.csv [--interval 3] [--max-samples N]
停止: SIGTERM/SIGINT（优雅退出）。与 sdcshield 解耦，可独立运行。
"""
import argparse
import csv
import signal
import subprocess
import sys
import time

SENSORS = [
    "CPU1 N_VDDAVS", "CPU2 N_VDDAVS", "CPU1 VDDAVS", "CPU2 VDDAVS",
    "Power", "Power2", "PS2 IOut", "CPU1 Core Rem", "CPU2 Core Rem",
]
CSV_COLUMNS = ["ts"] + SENSORS


def parse_sensor_list(text):
    """ipmitool sensor list 输出 → {传感器名: 读数}。'na'/空读数跳过。"""
    out = {}
    for line in text.splitlines():
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 2 and parts[0]:
            if parts[1] and parts[1] != "na":
                out[parts[0]] = parts[1]
    return out


def read_once(timeout=30):
    """单次全量读取（in-band，需 root）。失败抛 CalledProcessError/TimeoutExpired。"""
    r = subprocess.run(["ipmitool", "sensor", "list"], capture_output=True,
                       text=True, timeout=timeout)
    r.check_returncode()
    return parse_sensor_list(r.stdout)


def poll_loop(out_path, interval_min=3.0, max_samples=None):
    """轮询主循环。返回写入样本数。SIGTERM/SIGINT 停止。"""
    stop = {"flag": False}

    def _stop(signum, frame):
        stop["flag"] = True

    signal.signal(signal.SIGTERM, _stop)
    signal.signal(signal.SIGINT, _stop)
    n = 0
    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(CSV_COLUMNS)
        f.flush()
        while not stop["flag"]:
            t0 = time.monotonic()
            try:
                vals = read_once()
                w.writerow([f"{time.time():.3f}"] + [vals.get(s, "") for s in SENSORS])
            except Exception as e:  # BMC 偶发超时不致命：记录后继续
                w.writerow([f"{time.time():.3f}"] + [f"ERR:{e}"] * len(SENSORS))
            f.flush()
            n += 1
            if max_samples is not None and n >= max_samples:
                break
            time.sleep(max(0.0, interval_min - (time.monotonic() - t0)))
    return n


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", required=True, help="输出 CSV 路径")
    ap.add_argument("--interval", type=float, default=3.0,
                    help="最小轮询间隔秒（默认 3.0，勿低于 3）")
    ap.add_argument("--max-samples", type=int, default=None)
    args = ap.parse_args(argv)
    if args.interval < 3.0:
        print("WARNING: interval < 3s 可能打超时 BMC（实测教训）", file=sys.stderr)
    n = poll_loop(args.out, args.interval, args.max_samples)
    print(f"bmc_logger: wrote {n} samples to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 5: 运行测试确认通过**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: 3 tests PASS。

- [ ] **Step 6: 真实行为验证（规则 2）**

Run: `sudo python3 tools/droop-excite/bmc_logger.py --out /tmp/bmc_smoke.csv --max-samples 2 && cat /tmp/bmc_smoke.csv`
Expected: 2 行数据，`CPU1 N_VDDAVS` ≈ 0.9x，`Power`+`Power2` ≈ 530W 量级。引用真实输出进提交说明/进展日志。

- [ ] **Step 7: Commit**

```bash
git branch --show-current   # 必须是 feat/droop-excite-2026-10-27
git add tools/droop-excite/
git commit -s -m "feat(droop-excite): BMC 遥测记录器（ipmitool→CSV，≥3s 节奏）"
git push
```

### Task 2: droop_mod.c（周期级负载调制器，完整交付）

一个补丁一个单元：调制器（含多线程）是单一功能单元（CLAUDE.md 规则 1），一次交付完整文件。

**Files:**
- Create: `tools/droop-excite/droop_mod.c`
- Create: `tools/droop-excite/tests/test_droop_mod.sh`

**Interfaces:**
- Consumes: 无（独立工具；CNTVCT/CNTFRQ EL0 可读）。
- Produces: 可执行 `droop_mod`；CLI：`--cores LIST --threads N --duration S (--freq-hz F --duty D | --on-iters N --off-iters M) --payload dep|ilp2 --off-mode stall|light --resync N --core-mhz M --json-summary P --calibrate --self-test`；摘要 JSON（stdout + 可选文件）字段：`mode/payload/off_mode/threads/on_iters/off_iters/freq_target_hz/duty/freq_measured_hz_mean|_min|_max/duration_s/gate_periods_total/checksum/checksum_failures/rt_priority/stopped_by_signal/cyc_per_iter_on/cyc_per_iter_off`。**退出码 0=正常，1=用法/内部错误，2=载荷校验失败（疑似 SDC）**——run_sweep 依赖 exit 2 语义。

- [ ] **Step 1: 写失败测试（aarch64 编译+功能自测脚本）**

`tools/droop-excite/tests/test_droop_mod.sh`：

```bash
#!/usr/bin/env bash
# droop_mod 编译 + 功能自测。非 aarch64 → SKIP（exit 0）。
# 用法: bash tools/droop-excite/tests/test_droop_mod.sh
set -euo pipefail
cd "$(dirname "$0")/.."
ARCH="$(uname -m)"
if [ "$ARCH" != "aarch64" ]; then
    echo "SKIP: droop_mod is aarch64-only (host=$ARCH)"
    exit 0
fi
gcc -O2 -Wall -Wextra -Werror -std=c17 -pthread -lm -o droop_mod droop_mod.c
echo "== 1/4 self-test =="
./droop_mod --self-test
echo "== 2/4 calibrate =="
./droop_mod --calibrate --payload dep --off-mode stall > /tmp/dm_cal.json
cat /tmp/dm_cal.json
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_cal.json"))
assert d["cyc_per_iter_on"] > 0 and d["cyc_per_iter_off"] > 0, d
assert 1 < d["cyc_per_iter_on"] < 100, d      # dep 每 iter ~4-10 周期
print("calibration sane")
EOF
echo "== 3/4 高频门控（100MHz 目标，2s，2 核） =="
./droop_mod --cores 0-1 --threads 2 --duration 2 --freq-hz 100e6 --duty 0.5 \
    --json-summary /tmp/dm_gate.json
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_gate.json"))
assert d["threads"] == 2, d
assert d["checksum_failures"] == 0, d
f = d["freq_measured_hz_mean"]
assert 5e7 < f < 1.5e8, f      # ±50% 容差：--freq-hz 走独立校准，高频有相位开销偏差，
print("gate ok, f_meas=%.4g" % f)  # 精确频率由 run_sweep 的 refine_iters 闭环修正
EOF
echo "== 4/4 低频门控（0.5Hz 目标）+ SIGTERM 路径 =="
./droop_mod --cores 0 --duration 10 --freq-hz 0.5 --duty 0.5 \
    --json-summary /tmp/dm_gate2.json &
P=$!
sleep 2
kill -TERM $P
wait $P
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_gate2.json"))
assert d["stopped_by_signal"] == "true", d
assert d["checksum"] == "off", d   # 低频大相位校验自动关闭（防软件参考拖垮门控）
print("sigterm ok")
EOF
echo "ALL PASS"
```

- [ ] **Step 2: 运行确认失败**

Run: `bash tools/droop-excite/tests/test_droop_mod.sh`
Expected: `gcc: error: droop_mod.c: No such file or directory`（失败原因=文件不存在）。

- [ ] **Step 3: 实现 droop_mod.c（完整源码）**

`tools/droop-excite/droop_mod.c`：

```c
/*
 * droop_mod.c — 周期级负载调制器（瞬态电压骤降 / di/dt 激发实验）
 *
 * Spec: docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md
 *
 * 原理：在选定核上以指令计数门控交替"最大电流载荷"与"低电流停顿"。
 * 相位以迭代数表达（--on-iters/--off-iters）；--freq-hz/--duty 由启动时独立
 * 校准换算（高频段绝对精度有限——每相位调用/循环开销，摘要如实报告实测频率，
 * 战役驱动用实测值闭环修正 iters）。
 *
 * 载荷技术复用 tests/cpu/arm64/power_virus_dit.cpp：
 *   - dep  = 依赖 NEON FMA 链背靠背 + 高汉明距离操作数交替（最大翻转活动）
 *   - ilp2 = 4 条独立链交织（饱和双 NEON 管的吞吐路径）
 *   - 断电流相 = yield 停顿（stall）或 GPR 依赖乘加链（light，部分深度）
 *   - bit-exact 软件参考校验（fmaf 单舍入与 NEON FMA 一致）
 *
 * 构建（aarch64）：
 *   gcc -O2 -Wall -Wextra -Werror -std=c17 -pthread -lm -o droop_mod droop_mod.c
 * 仅 aarch64（非 aarch64 编译期 #error）；不进 meson，x86 构建面不受影响。
 *
 * 退出码：0 正常；1 用法/内部错误；2 载荷校验失败（疑似 SDC 证据，
 *         触发战役停机取证流程）。
 */
#define _GNU_SOURCE 1

#if !defined(__aarch64__)
#error "droop_mod.c is aarch64-only (NEON gate loops); not part of the meson build"
#endif

#include <arm_neon.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------------- 计时（EL0 恒可读，无系统调用） ---------------- */
static inline uint64_t cntvct(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

static inline uint32_t cntfrq_hz(void)
{
    uint32_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}

static inline double ticks_to_secs(uint64_t ticks)
{
    return (double)ticks / (double)cntfrq_hz();
}

/* ---------------- 配置 ---------------- */
enum payload_kind { PAYLOAD_DEP = 0, PAYLOAD_ILP2 = 1 };
enum off_kind { OFF_STALL = 0, OFF_LIGHT = 1 };

/* 校验仅在小相位启用：低频档单相位可达 1e9+ 次迭代，软件参考会拖垮门控
 * 节奏（低频段 oracle 是并行的 eigen 用例，不缺这一层）。 */
#define CHECKSUM_MAX_ITERS 1000000ULL
/* 启动校准样本量（各模式 ~5-30ms，CNTVCT 精度足够） */
#define CAL_N 10000000ULL

struct config {
    uint64_t on_iters;
    uint64_t off_iters;
    double freq_hz;              /* >0：由 freq/duty 计算 iters */
    double duty;                 /* (0,1]；1.0 → off_iters=0 */
    enum payload_kind payload;
    enum off_kind off_mode;
    uint64_t resync;             /* 每 N 门控周期重同步（0=关） */
    double duration;             /* 秒 */
    cpu_set_t cpus;
    int nthreads;                /* 0 = cores 计数 */
    int self_test;
    int calibrate_only;
    const char *json_summary_path;
    /* 运行期派生（不来自命令行） */
    double core_mhz;
    double ips_on, ips_off;
    double cyc_per_iter_on, cyc_per_iter_off;
};

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ---------------- 载荷常量（power_virus_dit 同款高汉明距离操作数对） ---------------- */
#define BURST_A0 1.0f     /* 0x3f800000 */
#define BURST_A1 (-2.0f)  /* 0xc0000000 */
#define BURST_B0 4.0f     /* 0x40800000 */
#define BURST_B1 0.125f   /* 0x3e000000 */

/* 非静态全局：外部可见，编译器不能消掉消费它的载荷循环 */
float32x4_t g_sink_vec;
uint64_t g_sink_u64;

/* dep：依赖 FMA 链 + 操作数交替。iters 向上取整为偶（一对 = 2 FMA）。
 * 每次调用从 0 起链（一个 ON 相位一条链，软件参考只依赖 iters）。 */
static float32x4_t payload_dep(uint64_t iters)
{
    float32x4_t a0 = vdupq_n_f32(BURST_A0);
    float32x4_t a1 = vdupq_n_f32(BURST_A1);
    float32x4_t b0 = vdupq_n_f32(BURST_B0);
    float32x4_t b1 = vdupq_n_f32(BURST_B1);
    float32x4_t acc = vdupq_n_f32(0.0f);
    uint64_t pairs = (iters + 1) / 2;
    for (uint64_t i = 0; i < pairs; ++i) {
        acc = vfmaq_f32(acc, a0, b0);
        acc = vfmaq_f32(acc, a1, b1);
    }
    return acc;
}

/* ilp2：4 条独立链交织（吞吐受限路径）。每次调用从 0 起链。
 * lane0/2 用 (A0,B0)，lane1/3 用 (A1,B1)——交替翻转。 */
static void payload_ilp2(uint64_t iters, float32x4_t out[4])
{
    float32x4_t a0 = vdupq_n_f32(BURST_A0);
    float32x4_t a1 = vdupq_n_f32(BURST_A1);
    float32x4_t b0 = vdupq_n_f32(BURST_B0);
    float32x4_t b1 = vdupq_n_f32(BURST_B1);
    float32x4_t x0 = vdupq_n_f32(0.0f), x1 = vdupq_n_f32(0.0f);
    float32x4_t x2 = vdupq_n_f32(0.0f), x3 = vdupq_n_f32(0.0f);
    for (uint64_t i = 0; i < iters; ++i) {
        x0 = vfmaq_f32(x0, a0, b0);
        x1 = vfmaq_f32(x1, a1, b1);
        x2 = vfmaq_f32(x2, a0, b0);
        x3 = vfmaq_f32(x3, a1, b1);
    }
    out[0] = x0; out[1] = x1; out[2] = x2; out[3] = x3;
}

/* 软件 bit-exact 参考：fmaf 单舍入与 NEON FMA 一致（power_virus_dit 论证） */
static float ref_dep(uint64_t iters)
{
    uint64_t pairs = (iters + 1) / 2;
    float acc = 0.0f;
    for (uint64_t i = 0; i < pairs; ++i) {
        acc = fmaf(BURST_A0, BURST_B0, acc);
        acc = fmaf(BURST_A1, BURST_B1, acc);
    }
    return acc;
}

static float ref_chain(uint64_t steps, float a, float b)
{
    float acc = 0.0f;
    for (uint64_t i = 0; i < steps; ++i)
        acc = fmaf(a, b, acc);
    return acc;
}

/* ---------------- 断电流相 ---------------- */
static void off_stall(uint64_t iters)
{
    for (uint64_t i = 0; i < iters; ++i)
        __asm__ volatile("yield" ::: "memory");
}

static uint64_t off_light(uint64_t iters, uint64_t seed)
{
    /* GPR 依赖乘加链：低但非零的电流（部分深度调制） */
    uint64_t x = seed | 1u;
    for (uint64_t i = 0; i < iters; ++i)
        x = x * 3u + 1u;
    return x;
}

/* ---------------- 校准包装（统一签名；同一函数既校准又门控） ---------------- */
static void cal_dep(uint64_t n)   { g_sink_vec = payload_dep(n); }
static void cal_ilp2(uint64_t n)  { float32x4_t a[4]; payload_ilp2(n, a); g_sink_vec = a[0]; }
static void cal_stall(uint64_t n) { off_stall(n); }
static void cal_light(uint64_t n) { g_sink_u64 ^= off_light(n, g_sink_u64); }

static double calibrate_fn(void (*fn)(uint64_t), double core_mhz, double *cyc_per_iter)
{
    uint64_t t0 = cntvct();
    fn(CAL_N);
    uint64_t t1 = cntvct();
    double secs = ticks_to_secs(t1 - t0);
    double ips = (double)CAL_N / secs;
    *cyc_per_iter = core_mhz * 1e6 / ips;
    return ips;
}

static double read_core_mhz_from_sysfs(void)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq", "r");
    if (!f)
        return 2600.0;
    double khz = 0.0;
    int n = fscanf(f, "%lf", &khz);
    fclose(f);
    return (n == 1 && khz > 0.0) ? khz / 1000.0 : 2600.0;
}

static void startup_calibration(struct config *cfg)
{
    void (*on_fn)(uint64_t) = cfg->payload == PAYLOAD_DEP ? cal_dep : cal_ilp2;
    void (*off_fn)(uint64_t) = cfg->off_mode == OFF_STALL ? cal_stall : cal_light;
    cfg->ips_on = calibrate_fn(on_fn, cfg->core_mhz, &cfg->cyc_per_iter_on);
    cfg->ips_off = calibrate_fn(off_fn, cfg->core_mhz, &cfg->cyc_per_iter_off);
}

/* ---------------- 每线程上下文 ---------------- */
struct thread_ctx {
    int cpu;
    uint64_t periods, checksum_fails;
    int failed_saved;
    uint32_t fail_bits[4];
    uint64_t light_state;
    uint64_t t_start, t_end;
};

/* ---------------- 门控相位 ---------------- */
static struct config *g_cfg;
static float32x4_t g_ref_dep;
static float32x4_t g_ref_ilp2[4];
static int g_checksum_on;

static inline uint32_t bits_of_lane(float32x4_t v, int lane)
{
    uint32_t u;
    float f = vgetq_lane_f32(v, lane);
    memcpy(&u, &f, sizeof(u));
    return u;
}

static void note_fail(struct thread_ctx *ctx, float32x4_t acc)
{
    ctx->checksum_fails++;
    if (!ctx->failed_saved) {
        ctx->failed_saved = 1;
        for (int l = 0; l < 4; ++l)
            ctx->fail_bits[l] = bits_of_lane(acc, l);
    }
}

static void on_phase(struct thread_ctx *ctx)
{
    if (g_cfg->payload == PAYLOAD_DEP) {
        float32x4_t acc = payload_dep(g_cfg->on_iters);
        if (g_checksum_on) {
            uint32x4_t cmp = vceqq_f32(acc, g_ref_dep);
            uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                          & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
            if (mask == 0)
                note_fail(ctx, acc);
        } else {
            g_sink_vec = acc;
        }
    } else {
        float32x4_t acc[4];
        payload_ilp2(g_cfg->on_iters, acc);
        if (g_checksum_on) {
            for (int j = 0; j < 4; ++j) {
                uint32x4_t cmp = vceqq_f32(acc[j], g_ref_ilp2[j]);
                uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                              & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
                if (mask == 0) {
                    note_fail(ctx, acc[j]);
                    break;
                }
            }
        } else {
            g_sink_vec = acc[0];
        }
    }
}

static void off_phase(struct thread_ctx *ctx)
{
    if (g_cfg->off_iters == 0)
        return;                          /* duty=1.0：连续满载 */
    if (g_cfg->off_mode == OFF_STALL)
        off_stall(g_cfg->off_iters);
    else
        ctx->light_state ^= off_light(g_cfg->off_iters, ctx->light_state);
}

/* ---------------- 门控主循环 ----------------
 * 检查窗口 g_check_every 个周期做一次 stop/时限检查与（可选）重同步；
 * 门控周期内部零系统调用、零计时器读取（高频段的周期完整性优先）。
 * 单线程（nthreads==1）跳过所有 barrier。 */
static uint64_t g_check_every = 1;
static uint64_t g_resync_snapped = 0;
static uint64_t g_deadline_ticks;

static void sbarrier_wait(void);   /* 前向声明：定义在下方多线程同步节 */

static uint64_t gate_loop(struct thread_ctx *ctx)
{
    uint64_t periods = 0, since_resync = 0;
    while (!g_stop) {
        for (uint64_t p = 0; p < g_check_every; ++p) {
            on_phase(ctx);
            off_phase(ctx);
        }
        periods += g_check_every;
        if (cntvct() >= g_deadline_ticks)
            break;
        if (g_resync_snapped) {
            since_resync += g_check_every;
            if (since_resync >= g_resync_snapped) {
                since_resync = 0;
                sbarrier_wait();
            }
        }
    }
    ctx->periods = periods;
    return periods;
}

/* ---------------- 多线程同步 ---------------- */
struct sbarrier {
    uint32_t n;                  /* 参与线程数（>1 才启用） */
    _Atomic uint32_t count;
    _Atomic uint32_t gen;
};
static struct sbarrier g_sbar;
static int g_rt_ok = 1;

/* 票据自旋 barrier（无 futex）。deadline 逃逸时置 g_stop 并返回，避免
 * 兄弟线程悬挂（count 残留一个增量，但 g_stop 已置位，全员随即退出——
 * 有界实验工具的取舍，注释留档）。 */
static void sbarrier_wait(void)
{
    if (g_sbar.n <= 1)
        return;
    uint32_t g = atomic_load_explicit(&g_sbar.gen, memory_order_acquire);
    if (atomic_fetch_add_explicit(&g_sbar.count, 1, memory_order_acq_rel)
            == g_sbar.n - 1) {
        atomic_store_explicit(&g_sbar.count, 0, memory_order_relaxed);
        atomic_store_explicit(&g_sbar.gen, g + 1, memory_order_release);
        return;
    }
    while (atomic_load_explicit(&g_sbar.gen, memory_order_acquire) == g) {
        if (g_stop || cntvct() >= g_deadline_ticks) {
            g_stop = 1;
            return;
        }
        __asm__ volatile("yield");
    }
}

static void try_rt_priority(void)
{
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = 1;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0)
        g_rt_ok = 0;    /* EPERM 等 → 降级普通调度，摘要如实报告 */
}

static void *thread_main(void *arg)
{
    struct thread_ctx *ctx = arg;
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET((unsigned)ctx->cpu, &one);
    if (sched_setaffinity(0, sizeof(one), &one) != 0) {
        fprintf(stderr, "droop_mod: setaffinity cpu%d: %s\n", ctx->cpu,
                strerror(errno));
        g_stop = 1;
        return NULL;
    }
    try_rt_priority();
    sbarrier_wait();   /* 起始对齐（一次性；自旋内含 g_stop/deadline 逃逸，
                          部分线程创建失败时 main 置 g_stop 即可全员释放，无悬挂） */
    ctx->t_start = cntvct();
    gate_loop(ctx);
    ctx->t_end = cntvct();
    return NULL;
}

/* ---------------- 自检 ---------------- */
static int self_test(void)
{
    int fails = 0;
    /* 1) dep 校验通过 */
    {
        uint64_t n = 2000;
        float32x4_t acc = payload_dep(n);
        float32x4_t ref = vdupq_n_f32(ref_dep(n));
        uint32x4_t cmp = vceqq_f32(acc, ref);
        uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                      & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
        if (mask == 0) {
            fprintf(stderr, "self-test FAIL: dep checksum mismatch\n");
            fails++;
        }
    }
    /* 2) 注错必须被抓（防"恒真"校验——power_virus_dit 的 veorq no-op 教训） */
    {
        uint64_t n = 2000;
        float32x4_t wrong = vaddq_n_f32(payload_dep(n), 1.0f);
        float32x4_t ref = vdupq_n_f32(ref_dep(n));
        uint32x4_t cmp = vceqq_f32(wrong, ref);
        uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                      & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
        if (mask != 0) {
            fprintf(stderr, "self-test FAIL: corrupted acc NOT detected\n");
            fails++;
        }
    }
    /* 3) ilp2 校验通过（lane0/2=(A0,B0)，lane1/3=(A1,B1)） */
    {
        uint64_t n = 2000;
        float32x4_t acc[4];
        payload_ilp2(n, acc);
        float r0 = ref_chain(n, BURST_A0, BURST_B0);
        float r1 = ref_chain(n, BURST_A1, BURST_B1);
        float32x4_t refs[4] = { vdupq_n_f32(r0), vdupq_n_f32(r1),
                                vdupq_n_f32(r0), vdupq_n_f32(r1) };
        for (int j = 0; j < 4; ++j) {
            uint32x4_t cmp = vceqq_f32(acc[j], refs[j]);
            uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                          & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
            if (mask == 0) {
                fprintf(stderr, "self-test FAIL: ilp2 lane %d mismatch\n", j);
                fails++;
            }
        }
    }
    /* 4) 校准值有限且正（真实 sysfs/2600MHz） */
    {
        struct config cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.payload = PAYLOAD_DEP;
        cfg.off_mode = OFF_STALL;
        cfg.core_mhz = read_core_mhz_from_sysfs();
        startup_calibration(&cfg);
        if (!(cfg.ips_on > 0.0 && cfg.ips_off > 0.0)
                || !(cfg.cyc_per_iter_on > 0.0 && cfg.cyc_per_iter_off > 0.0)) {
            fprintf(stderr, "self-test FAIL: calibration not positive\n");
            fails++;
        } else {
            printf("self-test calibrate: dep %.3f cyc/iter, stall %.3f cyc/iter"
                   " @ %.0f MHz\n", cfg.cyc_per_iter_on, cfg.cyc_per_iter_off,
                   cfg.core_mhz);
        }
    }
    printf("self-test: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

/* ---------------- CLI ---------------- */
static void usage(FILE *out, const char *prog)
{
    fprintf(out,
        "用法: %s [选项]\n"
        "  (--freq-hz F --duty D | --on-iters N --off-iters M)  门控参数（二选一）\n"
        "  --cores LIST        逗号/区间核列表（默认全部。0103: socketA=0-63 socketB=64-127）\n"
        "  --threads N         线程数（默认 = cores 数）\n"
        "  --duration S        运行秒数（默认 60）\n"
        "  --payload dep|ilp2  载荷（默认 dep）\n"
        "  --off-mode stall|light  断电流模式（默认 stall）\n"
        "  --resync N          每 N 门控周期自旋重同步（0=关，默认 1024）\n"
        "  --core-mhz M        核时钟 MHz（默认读 policy0/scaling_cur_freq）\n"
        "  --json-summary P    摘要 JSON 文件路径（也总是打到 stdout）\n"
        "  --calibrate         只校准并打印 JSON，不门控\n"
        "  --self-test         功能自检\n"
        "退出码: 0 正常 | 1 用法/内部错误 | 2 载荷校验失败（疑似 SDC）\n",
        prog);
}

static const char *need_val(int argc, char **argv, int *i)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "missing value for %s\n", argv[*i]);
        usage(stderr, argv[0]);
        exit(1);
    }
    ++*i;
    return argv[*i];
}

static int parse_cpu_list(const char *s, cpu_set_t *set)
{
    CPU_ZERO(set);
    char *buf = strdup(s);
    if (!buf)
        return -1;
    int rc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        unsigned long a, b;
        if (sscanf(tok, "%lu-%lu", &a, &b) == 2) {
            for (unsigned long c = a; c <= b; ++c) {
                if (c >= (unsigned long)CPU_SETSIZE) { rc = -1; break; }
                CPU_SET((unsigned)c, set);
            }
        } else if (sscanf(tok, "%lu", &a) == 1) {
            if (a >= (unsigned long)CPU_SETSIZE)
                rc = -1;
            else
                CPU_SET((unsigned)a, set);
        } else {
            rc = -1;
        }
        if (rc)
            break;
    }
    free(buf);
    return rc;
}

int main(int argc, char **argv)
{
    struct config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.payload = PAYLOAD_DEP;
    cfg.off_mode = OFF_STALL;
    cfg.resync = 1024;
    cfg.duration = 60.0;
    cfg.duty = 0.5;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    for (long c = 0; c < ncpu && c < CPU_SETSIZE; ++c)
        CPU_SET((unsigned)c, &cfg.cpus);

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--self-test"))
            cfg.self_test = 1;
        else if (!strcmp(a, "--calibrate"))
            cfg.calibrate_only = 1;
        else if (!strcmp(a, "--cores")) {
            if (parse_cpu_list(need_val(argc, argv, &i), &cfg.cpus) != 0) {
                fprintf(stderr, "bad --cores list\n");
                return 1;
            }
        }
        else if (!strcmp(a, "--threads")) cfg.nthreads = atoi(need_val(argc, argv, &i));
        else if (!strcmp(a, "--on-iters")) cfg.on_iters = strtoull(need_val(argc, argv, &i), NULL, 10);
        else if (!strcmp(a, "--off-iters")) cfg.off_iters = strtoull(need_val(argc, argv, &i), NULL, 10);
        else if (!strcmp(a, "--freq-hz")) cfg.freq_hz = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--duty")) cfg.duty = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--payload")) {
            const char *v = need_val(argc, argv, &i);
            if (!strcmp(v, "dep")) cfg.payload = PAYLOAD_DEP;
            else if (!strcmp(v, "ilp2")) cfg.payload = PAYLOAD_ILP2;
            else { fprintf(stderr, "--payload dep|ilp2\n"); return 1; }
        }
        else if (!strcmp(a, "--off-mode")) {
            const char *v = need_val(argc, argv, &i);
            if (!strcmp(v, "stall")) cfg.off_mode = OFF_STALL;
            else if (!strcmp(v, "light")) cfg.off_mode = OFF_LIGHT;
            else { fprintf(stderr, "--off-mode stall|light\n"); return 1; }
        }
        else if (!strcmp(a, "--resync")) cfg.resync = strtoull(need_val(argc, argv, &i), NULL, 10);
        else if (!strcmp(a, "--duration")) cfg.duration = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--core-mhz")) cfg.core_mhz = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--json-summary")) cfg.json_summary_path = need_val(argc, argv, &i);
        else {
            fprintf(stderr, "unknown option: %s\n", a);
            usage(stderr, argv[0]);
            return 1;
        }
    }
    if (cfg.duty <= 0.0 || cfg.duty > 1.0) {
        fprintf(stderr, "--duty 必须 (0,1]\n");
        return 1;
    }

    if (cfg.self_test)
        return self_test();

    /* 亲和性先行：校准与门控都发生在目标核上 */
    if (sched_setaffinity(0, sizeof(cfg.cpus), &cfg.cpus) != 0) {
        fprintf(stderr, "droop_mod: sched_setaffinity: %s\n", strerror(errno));
        return 1;
    }
    if (cfg.core_mhz <= 0.0)
        cfg.core_mhz = read_core_mhz_from_sysfs();

    g_cfg = &cfg;
    startup_calibration(&cfg);

    if (cfg.calibrate_only) {
        printf("{\"mode\":\"calibrate\",\"core_mhz\":%.1f,"
               "\"payload\":\"%s\",\"cyc_per_iter_on\":%.4f,"
               "\"off_mode\":\"%s\",\"cyc_per_iter_off\":%.4f}\n",
               cfg.core_mhz, cfg.payload == PAYLOAD_DEP ? "dep" : "ilp2",
               cfg.cyc_per_iter_on,
               cfg.off_mode == OFF_STALL ? "stall" : "light",
               cfg.cyc_per_iter_off);
        return 0;
    }

    if (cfg.freq_hz > 0.0) {
        double period = 1.0 / cfg.freq_hz;
        double on_s = period * cfg.duty;
        double off_s = period * (1.0 - cfg.duty);
        cfg.on_iters = (uint64_t)(on_s * cfg.ips_on);
        cfg.off_iters = (uint64_t)(off_s * cfg.ips_off);
    }
    if (cfg.on_iters == 0 && cfg.off_iters == 0) {
        fprintf(stderr, "需要 --freq-hz/--duty 或 --on-iters/--off-iters\n");
        usage(stderr, argv[0]);
        return 1;
    }

    /* 校验参考缓存（iters 此时定型） */
    g_checksum_on = (cfg.on_iters > 0 && cfg.on_iters <= CHECKSUM_MAX_ITERS);
    if (g_checksum_on) {
        g_ref_dep = vdupq_n_f32(ref_dep(cfg.on_iters));
        float r0 = ref_chain(cfg.on_iters, BURST_A0, BURST_B0);
        float r1 = ref_chain(cfg.on_iters, BURST_A1, BURST_B1);
        g_ref_ilp2[0] = vdupq_n_f32(r0);
        g_ref_ilp2[1] = vdupq_n_f32(r1);
        g_ref_ilp2[2] = vdupq_n_f32(r0);
        g_ref_ilp2[3] = vdupq_n_f32(r1);
    }

    /* 检查窗口：min(10ms 周期数, 4096)；resync snapped 到窗口整数倍 */
    {
        double period_s = (double)cfg.on_iters / cfg.ips_on
                        + (double)cfg.off_iters / cfg.ips_off;
        double f_est = period_s > 0.0 ? 1.0 / period_s : 1e9;
        uint64_t per_10ms = (uint64_t)(f_est * 0.01);
        g_check_every = per_10ms < 1 ? 1 : (per_10ms > 4096 ? 4096 : per_10ms);
    }
    if (cfg.resync) {
        uint64_t r = ((cfg.resync + g_check_every - 1) / g_check_every)
                     * g_check_every;
        g_resync_snapped = r < 1 ? g_check_every : r;
    }

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    int cores[CPU_SETSIZE], ncores = 0;
    for (int c = 0; c < CPU_SETSIZE; ++c)
        if (CPU_ISSET(c, &cfg.cpus))
            cores[ncores++] = c;
    if (ncores < 1) {
        fprintf(stderr, "no usable cores\n");
        return 1;
    }
    int nthreads = (cfg.nthreads > 0 && cfg.nthreads <= ncores) ? cfg.nthreads
                                                                : ncores;

    static struct thread_ctx ctxs[CPU_SETSIZE];
    pthread_t tids[CPU_SETSIZE];
    memset(ctxs, 0, sizeof(ctxs));
    g_sbar.n = (uint32_t)nthreads;
    atomic_store(&g_sbar.count, 0);
    atomic_store(&g_sbar.gen, 0);

    g_deadline_ticks = cntvct()
                     + (uint64_t)(cfg.duration * (double)cntfrq_hz());

    int created = 0;
    for (int t = 0; t < nthreads; ++t) {
        ctxs[t].cpu = cores[t];
        if (pthread_create(&tids[t], NULL, thread_main, &ctxs[t]) != 0) {
            fprintf(stderr, "pthread_create failed at %d: %s\n", t,
                    strerror(errno));
            g_stop = 1;
            break;
        }
        created++;
    }
    if (created < nthreads)
        g_stop = 1;    /* 部分创建失败：已建线程从起始对齐自旋中逃逸后即退 */
    for (int t = 0; t < created; ++t)
        pthread_join(tids[t], NULL);

    /* 汇总 */
    uint64_t total_periods = 0, total_fails = 0;
    double f_min = 1e30, f_max = 0.0, f_sum = 0.0;
    for (int t = 0; t < created; ++t) {
        total_periods += ctxs[t].periods;
        total_fails += ctxs[t].checksum_fails;
        double secs = ticks_to_secs(ctxs[t].t_end - ctxs[t].t_start);
        double f = secs > 0.0 ? (double)ctxs[t].periods / secs : 0.0;
        f_sum += f;
        if (f < f_min) f_min = f;
        if (f > f_max) f_max = f;
    }
    double f_mean = created > 0 ? f_sum / created : 0.0;
    if (f_min > f_max) f_min = f_max;   /* created==0/空跑保护 */

    char summary[2048];
    int m = snprintf(summary, sizeof(summary),
        "{\"mode\":\"gate\",\"payload\":\"%s\",\"off_mode\":\"%s\",\"threads\":%d,"
        "\"on_iters\":%llu,\"off_iters\":%llu,"
        "\"freq_target_hz\":%.9g,\"duty\":%.4f,"
        "\"freq_measured_hz_mean\":%.9g,\"freq_measured_hz_min\":%.9g,"
        "\"freq_measured_hz_max\":%.9g,"
        "\"duration_s\":%.3f,\"gate_periods_total\":%llu,"
        "\"checksum\":\"%s\",\"checksum_failures\":%llu,"
        "\"rt_priority\":\"%s\",\"stopped_by_signal\":%s,"
        "\"cyc_per_iter_on\":%.4f,\"cyc_per_iter_off\":%.4f}\n",
        cfg.payload == PAYLOAD_DEP ? "dep" : "ilp2",
        cfg.off_mode == OFF_STALL ? "stall" : "light",
        created,
        (unsigned long long)cfg.on_iters,
        (unsigned long long)cfg.off_iters,
        cfg.freq_hz, cfg.duty, f_mean, f_min, f_max,
        cfg.duration, (unsigned long long)total_periods,
        g_checksum_on ? "on" : "off",
        (unsigned long long)total_fails,
        g_rt_ok ? "fifo" : "degraded",
        g_stop ? "true" : "false",
        cfg.cyc_per_iter_on, cfg.cyc_per_iter_off);
    (void)m;
    fputs(summary, stdout);
    if (cfg.json_summary_path) {
        FILE *f = fopen(cfg.json_summary_path, "w");
        if (f) {
            fputs(summary, f);
            fclose(f);
        }
    }
    return total_fails ? 2 : 0;
}
```

- [ ] **Step 4: 运行测试确认通过**

Run: `bash tools/droop-excite/tests/test_droop_mod.sh`
Expected: 4 段全过，末行 `ALL PASS`。真实输出引用进提交说明。

- [ ] **Step 5: 真实行为抽验（规则 2，x86 非回归靠检查）**

Run: `cd tools/droop-excite && ./droop_mod --cores 0-3 --duration 3 --freq-hz 50e6 --duty 0.5 --json-summary /tmp/dm50.json && cat /tmp/dm50.json`
Expected: `freq_measured_hz_mean` 在 2.5e7-7.5e7（50MHz 目标的独立校准偏差带）。
Run: `git status --porcelain framework/ tests/ meson.build` → 空输出（框架零改动证据）。

- [ ] **Step 6: Commit**

```bash
git branch --show-current
git add tools/droop-excite/
git commit -s -m "feat(droop-excite): 周期级负载调制器 droop_mod（0.1Hz-325MHz 门控，NEON 载荷+自校验）"
git push
```

### Task 3: run_sweep.py 核心层（拓扑/核心规划/护栏/命令构建/步骤执行）

**Files:**
- Create: `tools/droop-excite/run_sweep.py`
- Test: `tools/droop-excite/tests/test_run_sweep.py`

**Interfaces:**
- Consumes: Task 1 的 CSV 列名（传感器原名）；Task 2 的 `droop_mod` CLI 与摘要字段（`on_iters/off_iters/freq_measured_hz_mean`）。
- Produces: `probe_topology(sysfs_root)->dict`；`assert_topology()`；`mod_cores(m, placement)->list`；`eigen_cores(mod)->list`；`build_mod_cmd(step, paths)->list`；`build_eigen_cmd(step, paths)->list`；`evaluate_guardrails(sample, thresholds)->None|str`；`DEFAULT_THRESHOLDS`；`run_step(step, paths, thresholds)->step dict`（落盘 summary.json，字段含 `timeline{pre_start,dwell_start,dwell_end,post_end}`、`mod_rc`、`eigen_rc`、`sel_delta`、`ras_delta`、`aborted`、`sdc_suspect`）。

- [ ] **Step 1: 写失败测试**

`tools/droop-excite/tests/test_run_sweep.py`：

```python
#!/usr/bin/env python3
"""run_sweep 单元测试。"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import run_sweep  # noqa: E402

TH = dict(run_sweep.DEFAULT_THRESHOLDS)


def _mk_sysfs(root):
    for c, pkg in ((0, 0), (1, 0), (2, 1), (3, 1)):
        d = os.path.join(root, f"devices/system/cpu/cpu{c}/topology")
        os.makedirs(d)
        with open(os.path.join(d, "physical_package_id"), "w") as f:
            f.write(f"{pkg}\n")
    for pol, cpus in (("policy0", [0, 1]), ("policy1", [2, 3])):
        d = os.path.join(root, f"devices/system/cpu/cpufreq/{pol}")
        os.makedirs(d)
        with open(os.path.join(d, "related_cpus"), "w") as f:
            f.write(" ".join(map(str, cpus)) + "\n")
    return root


class TestTopology(unittest.TestCase):
    def test_probe_topology_fixture(self):
        with tempfile.TemporaryDirectory() as td:
            topo = run_sweep.probe_topology(_mk_sysfs(td))
        self.assertEqual(topo["ncores"], 4)
        self.assertEqual(topo["sockets"], {"0": [0, 1], "1": [2, 3]})
        self.assertEqual(topo["policies"], {"policy0": [0, 1], "policy1": [2, 3]})


class TestCores(unittest.TestCase):
    def test_mod_cores_same_and_cross(self):
        self.assertEqual(run_sweep.mod_cores(8, "same"), list(range(8)))
        self.assertEqual(run_sweep.mod_cores(8, "cross"),
                         [0, 1, 2, 3, 64, 65, 66, 67])

    def test_eigen_cores_excludes_reserved_and_mod(self):
        ec = run_sweep.eigen_cores([0, 1, 2, 3])
        self.assertNotIn(0, ec)
        self.assertNotIn(120, ec)
        self.assertEqual(len(ec), 128 - 8 - 4)


class TestGuardrails(unittest.TestCase):
    def test_temp_trips(self):
        r = run_sweep.evaluate_guardrails({"CPU1 Core Rem": "95.1"}, TH)
        self.assertIsNotNone(r)
        self.assertIn("CPU1 Core Rem", r)

    def test_voltage_trips(self):
        self.assertIsNotNone(
            run_sweep.evaluate_guardrails({"CPU2 N_VDDAVS": "0.799"}, TH))

    def test_psu_trips(self):
        self.assertIsNotNone(
            run_sweep.evaluate_guardrails({"Power": "700", "Power2": "700"}, TH))

    def test_safe(self):
        self.assertIsNone(run_sweep.evaluate_guardrails(
            {"CPU1 Core Rem": "80", "CPU2 Core Rem": "70",
             "CPU1 N_VDDAVS": "0.90", "CPU2 N_VDDAVS": "0.89",
             "Power": "500", "Power2": "480"}, TH))

    def test_missing_values_safe(self):
        self.assertIsNone(run_sweep.evaluate_guardrails({}, TH))


class TestBuilders(unittest.TestCase):
    PATHS = {"droop_mod": "DM", "sdcshield": "SS", "run_dir": "/tmp/r"}

    def test_build_mod_cmd_freq(self):
        step = {"id": "x", "dir": "/tmp/x", "mod": "droop_mod", "payload": "dep",
                "off_mode": "stall", "freq_hz": 1e8, "duty": 0.5,
                "duration": 60, "mod_cores": [0, 1]}
        cmd = run_sweep.build_mod_cmd(step, self.PATHS)
        self.assertEqual(cmd[:3], ["taskset", "-c", "0,1"])
        self.assertIn("--freq-hz", cmd)
        self.assertIn("DM", cmd)

    def test_build_mod_cmd_iters(self):
        step = {"id": "x", "dir": "/tmp/x", "mod": "droop_mod", "payload": "ilp2",
                "off_mode": "stall", "on_iters": 13, "off_iters": 13,
                "duration": 60, "mod_cores": [4]}
        cmd = run_sweep.build_mod_cmd(step, self.PATHS)
        self.assertIn("--on-iters", cmd)
        self.assertNotIn("--freq-hz", cmd)

    def test_build_mod_cmd_pvdit(self):
        step = {"id": "x", "dir": "/tmp/x", "mod": "power_virus_dit",
                "mod_cores": [0], "duration": 300}
        cmd = run_sweep.build_mod_cmd(step, self.PATHS)
        self.assertIn("power_virus_dit", cmd)
        self.assertIn("SS", cmd)

    def test_build_eigen_cmd(self):
        step = {"id": "x", "eigen_cores": [8, 9], "duration": 300}
        cmd = run_sweep.build_eigen_cmd(step, self.PATHS)
        self.assertEqual(cmd[:3], ["taskset", "-c", "8,9"])
        self.assertIn("-e", cmd)
        self.assertIn("eigen_svd_double", cmd)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 运行确认失败**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: ERROR（`No module named 'run_sweep'`），bmc_logger 的测试仍 PASS。

- [ ] **Step 3: 实现 run_sweep.py 核心层**

`tools/droop-excite/run_sweep.py`（本任务先写到 `run_step` 为止，阶段编排与 CLI 由 Task 4 追加在同一文件）：

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""droop 激发战役驱动（0103 专用；root 运行——ipmitool/SCHED_FIFO 需要）。

Spec: docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md
与 sdcshield 解耦：仅经 CLI 调用 builddir/sdcshield 二进制（--repo 指定）；
无 sdcshield 时纯调制步骤仍可运行。

用法（阶段编排见文末 main）:
    sudo python3 run_sweep.py --phase p0 --data-root /home/sdc/droop-excite
    python3  run_sweep.py --phase p2 --dry-run
"""
import csv
import glob
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- 0103 拓扑（2026-10-27 实测固化；assert_topology 运行时校验，漂移即拒绝） ----
NCPUS = 128
RESERVED_CPUS = list(range(120, 128))       # OS/驱动/记录器/会话
SOCKET_A_CPUS = list(range(0, 64))          # physical_package_id=36
SOCKET_B_CPUS = list(range(64, 128))        # physical_package_id=8442

DEFAULT_THRESHOLDS = {
    "cpu_temp_abort": 95.0,        # ℃（BMC 临界 105）
    "nvddavs_min": 0.80,           # V（BMC 下限 0.770）
    "psu_total_w_abort": 1300.0,   # W（P0 实测满载后收紧，写 thresholds.json）
}

EIGEN_TESTS_DEFAULT = ("eigen_svd_double,eigen_sparse,"
                       "eigen_gemm_double_dynamic_square")
EIGEN_TESTS_FULL = ("eigen_svd_double,eigen_svd_double2,eigen_sparse,"
                    "eigen_gemm_double_dynamic_square,"
                    "eigen_gemm_cdouble_dynamic_square,"
                    "eigen_gemm_float_dynamic_square,eigen_gemm_double14,"
                    "eigen_svd,eigen_svd_cdouble,eigen_svd_cdouble_noavx512,"
                    "eigen_svd_fvectors")


def probe_topology(sysfs_root="/sys"):
    """读 sysfs：{"ncores":N,"sockets":{pkg_id:[cpus]},"policies":{name:[cpus]}}。"""
    pat = re.compile(r"cpu(\d+)$")
    sockets, cpus = {}, []
    for d in sorted(glob.glob(os.path.join(sysfs_root,
                                           "devices/system/cpu/cpu[0-9]*")),
                    key=lambda p: int(pat.search(p).group(1))):
        c = int(pat.search(d).group(1))
        try:
            with open(os.path.join(d, "topology/physical_package_id")) as f:
                pkg = f.read().strip()
        except OSError:
            pkg = "?"
        sockets.setdefault(pkg, []).append(c)
        cpus.append(c)
    policies = {}
    for d in sorted(glob.glob(os.path.join(sysfs_root,
                                           "devices/system/cpu/cpufreq/policy*"))):
        try:
            with open(os.path.join(d, "related_cpus")) as f:
                policies[os.path.basename(d)] = [int(x) for x in f.read().split()]
        except OSError:
            pass
    return {"ncores": len(cpus), "sockets": sockets, "policies": policies}


def assert_topology(sysfs_root="/sys"):
    topo = probe_topology(sysfs_root)
    if topo["ncores"] != NCPUS or len(topo["sockets"]) != 2:
        raise RuntimeError(f"0103 拓扑漂移，拒绝运行: {topo}")
    vals = {frozenset(v) for v in topo["sockets"].values()}
    want = {frozenset(SOCKET_A_CPUS), frozenset(SOCKET_B_CPUS)}
    if vals != want:
        raise RuntimeError(f"0103 socket 划分与固化值不符: {topo}")
    return topo


def mod_cores(m, placement):
    """调制器核心列表。same=socketA 头部；cross=两 socket 各半。"""
    if placement == "same":
        return SOCKET_A_CPUS[:m]
    if placement == "cross":
        return SOCKET_A_CPUS[: m // 2] + SOCKET_B_CPUS[: m - m // 2]
    raise ValueError(placement)


def eigen_cores(mod):
    used = set(RESERVED_CPUS) | set(mod)
    return [c for c in range(NCPUS) if c not in used]


def fmt_cpulist(cs):
    return ",".join(str(c) for c in sorted(cs))


def build_mod_cmd(step, paths):
    """step["mod"]: "droop_mod" 或 "power_virus_dit"（P0 零代码冒烟用现成测试）。"""
    if step.get("mod") is None:
        return None
    mc = fmt_cpulist(step["mod_cores"])
    if step["mod"] == "power_virus_dit":
        return ["taskset", "-c", mc, paths["sdcshield"],
                "-e", "power_virus_dit", "-t", f"{step['duration']}s",
                "--strict-runtime"]
    cmd = ["taskset", "-c", mc, paths["droop_mod"]]
    if step.get("on_iters") is not None:
        cmd += ["--on-iters", str(step["on_iters"]),
                "--off-iters", str(step["off_iters"])]
    else:
        cmd += ["--freq-hz", repr(step["freq_hz"]),
                "--duty", repr(step["duty"])]
    cmd += ["--payload", step.get("payload", "dep"),
            "--off-mode", step.get("off_mode", "stall"),
            "--duration", str(step["duration"]),
            "--resync", str(step.get("resync", 1024)),
            "--json-summary", os.path.join(step["dir"], "mod_summary.json")]
    return cmd


def build_eigen_cmd(step, paths):
    if not step.get("eigen_cores"):
        return None
    return ["taskset", "-c", fmt_cpulist(step["eigen_cores"]), paths["sdcshield"],
            "-e", step.get("eigen_tests", EIGEN_TESTS_DEFAULT),
            "-t", "60s", "-T", f"{step['duration']}s", "--strict-runtime"]


def evaluate_guardrails(sample, thresholds):
    """单 BMC 样本 → None(安全) 或 停机原因字符串。"""
    def num(key):
        try:
            return float(sample.get(key, "") or "")
        except ValueError:
            return None
    for key in ("CPU1 Core Rem", "CPU2 Core Rem"):
        v = num(key)
        if v is not None and v >= thresholds["cpu_temp_abort"]:
            return f"guardrail: {key}={v:.1f}C >= {thresholds['cpu_temp_abort']}C"
    for key in ("CPU1 N_VDDAVS", "CPU2 N_VDDAVS"):
        v = num(key)
        if v is not None and v <= thresholds["nvddavs_min"]:
            return f"guardrail: {key}={v:.3f}V <= {thresholds['nvddavs_min']}V"
    p1, p2 = num("Power"), num("Power2")
    if p1 is not None and p2 is not None \
            and p1 + p2 >= thresholds["psu_total_w_abort"]:
        return f"guardrail: PSU={p1 + p2:.0f}W >= {thresholds['psu_total_w_abort']}W"
    return None


def last_csv_row(path):
    try:
        with open(path) as f:
            rows = list(csv.DictReader(f))
        return rows[-1] if rows else None
    except OSError:
        return None


def sel_count():
    r = subprocess.run(["ipmitool", "sel", "list"], capture_output=True,
                       text=True)
    return len(r.stdout.splitlines())


def ras_error_count(text):
    return len(re.findall(r"Machine check|EDAC|Hardware Error|mce:", text, re.I))


def dmesg_text():
    r = subprocess.run(["dmesg"], capture_output=True, text=True)
    return r.stdout


def refine_iters(step, paths):
    """高频目标（>=1kHz）先 3s mod-only 探测，按实测频率修正 iters。
    spec：--freq-hz 独立校准在高频有相位开销偏差，驱动闭环修正。"""
    if step.get("on_iters") is not None or not step.get("freq_hz"):
        return
    if step["freq_hz"] < 1e3:
        return
    probe = dict(step)
    probe["duration"] = 3
    probe["dir"] = os.path.join(step["dir"], "probe")
    os.makedirs(probe["dir"], exist_ok=True)
    cmd = build_mod_cmd(probe, paths)
    with open(os.path.join(probe["dir"], "mod.log"), "w") as lf:
        subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, check=False)
    with open(os.path.join(probe["dir"], "mod_summary.json")) as f:
        s = json.load(f)
    fm = s["freq_measured_hz_mean"]
    scale = fm / step["freq_hz"]          # 实测偏低 → iters 缩小
    step["on_iters"] = max(1, int(s["on_iters"] * scale))
    step["off_iters"] = max(0, int(s["off_iters"] * scale))


def run_step(step, paths, thresholds, pre_secs=15.0, post_secs=15.0,
             poll_interval=3.0):
    """执行一个战役步骤。pre 15s 空载采样 → dwell（eigen+mod）→ post 15s →
    summary.json 落盘。返回 step dict。"""
    t = {"pre_start": time.time()}
    step["dir"] = os.path.join(paths["run_dir"], "steps", step["id"])
    os.makedirs(step["dir"], exist_ok=True)
    bmc_csv = os.path.join(step["dir"], "bmc.csv")
    sel0, dm0 = sel_count(), dmesg_text()
    logger = subprocess.Popen(
        [sys.executable, os.path.join(HERE, "bmc_logger.py"),
         "--out", bmc_csv, "--interval", "3"],
        stdout=subprocess.DEVNULL)
    refine_iters(step, paths)               # 高频：mod-only 探测修正（3s）
    time.sleep(pre_secs)
    t["dwell_start"] = time.time()
    mod_cmd = build_mod_cmd(step, paths)
    eig_cmd = build_eigen_cmd(step, paths)
    with open(os.path.join(step["dir"], "commands.json"), "w") as f:
        json.dump({"mod": mod_cmd, "eigen": eig_cmd}, f, indent=1)
    procs = {}
    if eig_cmd:
        procs["eigen"] = subprocess.Popen(
            eig_cmd,
            stdout=open(os.path.join(step["dir"], "eigen.log"), "w"),
            stderr=subprocess.STDOUT)
    if mod_cmd:
        procs["mod"] = subprocess.Popen(
            mod_cmd,
            stdout=open(os.path.join(step["dir"], "mod.log"), "w"),
            stderr=subprocess.STDOUT)
    abort_reason = None
    if not procs:
        time.sleep(step["duration"])        # 纯采样步骤（空载基线）
    else:
        deadline = time.time() + step["duration"] + 60.0
        while time.time() < deadline:
            if all(p.poll() is not None for p in procs.values()):
                break
            time.sleep(poll_interval)
            sample = last_csv_row(bmc_csv)
            if sample:
                abort_reason = evaluate_guardrails(sample, thresholds)
                if abort_reason:
                    for p in procs.values():
                        p.terminate()
                    break
    for name, p in procs.items():
        step[f"{name}_rc"] = p.wait(timeout=60)
    t["dwell_end"] = time.time()
    time.sleep(post_secs)
    logger.terminate()
    logger.wait(timeout=15)
    t["post_end"] = time.time()
    sel1, dm1 = sel_count(), dmesg_text()
    step["sel_delta"] = sel1 - sel0
    step["ras_delta"] = ras_error_count(dm1) - ras_error_count(dm0)
    with open(os.path.join(step["dir"], "dmesg_tail.txt"), "w") as f:
        f.write(dm1[-8000:])
    step["timeline"] = t
    step["aborted"] = abort_reason
    step["sdc_suspect"] = bool(
        step.get("mod_rc") == 2
        or (step.get("eigen_rc") not in (0, None))
        or step["ras_delta"] > 0)
    with open(os.path.join(step["dir"], "summary.json"), "w") as f:
        json.dump(step, f, indent=1, default=str)
    return step
```

- [ ] **Step 4: 运行测试确认通过**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: Task 1 + Task 3 全部 PASS（15 项）。

- [ ] **Step 5: Commit**

```bash
git branch --show-current
git add tools/droop-excite/
git commit -s -m "feat(droop-excite): 战役驱动核心层（拓扑断言/核心规划/护栏/命令构建/步骤执行）"
git push
```

### Task 4: run_sweep.py 阶段编排（P0-P4 步生成 + CLI + 心跳 + dry-run）

**Files:**
- Modify: `tools/droop-excite/run_sweep.py`（追加阶段函数与 main）
- Test: `tools/droop-excite/tests/test_run_sweep.py`（追加 TestPhases）

**Interfaces:**
- Consumes: Task 3 全部；Task 2 的 droop_mod。
- Produces: CLI `run_sweep.py --phase p0|p1|p2|p3|p3b|p4 [--dry-run] [--data-root D] [--repo R] [--step-duration S] [--payload P] [--freq F] [--duty D]`；步骤 dict 生成器 `steps_p0() ... steps_p4(...)`；心跳 `heartbeat.json`。

- [ ] **Step 1: 追加失败测试**

在 `tests/test_run_sweep.py` 追加（注意 `main` 会调用 `assert_topology`，用 mock 绕开使其可在任何主机跑）：

```python
class TestPhases(unittest.TestCase):
    def test_steps_p0_shape(self):
        steps = run_sweep.steps_p0()
        self.assertEqual(len(steps), 4)
        self.assertIsNone(steps[0]["mod"])                    # 空载基线
        self.assertEqual(steps[3]["mod"], "power_virus_dit")  # 零代码冒烟
        self.assertTrue(steps[2]["eigen_cores"] or steps[2]["mod_cores"])

    def test_steps_p1_count(self):
        self.assertEqual(len(run_sweep.steps_p1()), 2 + 4 * 3 * 2)

    def test_steps_p3_dense_duty(self):
        steps = run_sweep.steps_p3(payload="dep")
        self.assertEqual(len(steps), 12 + 8)   # 12 频点 duty .5 + 4 密集点×2 额外 duty

    def test_dry_run_emits_commands(self):
        import io
        import contextlib
        import unittest.mock as mock
        with tempfile.TemporaryDirectory() as td:
            with mock.patch.object(run_sweep, "assert_topology", lambda: None):
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    rc = run_sweep.main(["--phase", "p2", "--dry-run",
                                         "--data-root", td,
                                         "--repo", "/nonexistent"])
        self.assertEqual(rc, 0)
        lines = [json.loads(x) for x in buf.getvalue().splitlines()
                 if x.startswith("{")]
        self.assertEqual(len(lines), 5)        # p2 = 5 个频点
        self.assertIn("eigen_svd_double", lines[0]["eigen"])
```

（文件头部需补 `import json`。）

- [ ] **Step 2: 运行确认失败**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: TestPhases ERROR（`AttributeError: module 'run_sweep' has no attribute 'steps_p0'`）。

- [ ] **Step 3: 追加实现（同文件末尾）**

```python
# ---------------- 阶段步生成 ----------------
def steps_p0(duration=300):
    m24 = mod_cores(24, "same")
    return [
        {"id": "p0_1_idle_baseline", "mod": None, "mod_cores": [],
         "eigen_cores": [], "duration": 60, "note": "空载基线"},
        {"id": "p0_2_eigen_full", "mod": None, "mod_cores": [],
         "eigen_cores": eigen_cores([]), "duration": duration,
         "note": "eigen 满载基线+热轨迹（功率护栏收紧数据）"},
        {"id": "p0_3_pvdit_full", "mod": "power_virus_dit",
         "mod_cores": eigen_cores([]), "eigen_cores": [],
         "duration": duration, "note": "现有 di/dt 载荷对照（~数百kHz 点）"},
        {"id": "p0_4_eigen_plus_pvdit", "mod": "power_virus_dit",
         "mod_cores": m24, "eigen_cores": eigen_cores(m24),
         "duration": duration, "note": "分核 co-run 冒烟"},
    ]


def steps_p1(duration=90, payloads=("dep", "ilp2")):
    m = mod_cores(24, "same")
    steps = []
    for pl in payloads:      # 载荷裁决：100% duty 满载功率对比（无 eigen，隔离测量）
        steps.append({"id": f"p1_payload_{pl}", "mod": "droop_mod",
                      "payload": pl, "freq_hz": 1.0, "duty": 1.0,
                      "mod_cores": m, "eigen_cores": [], "duration": 120,
                      "note": "payload 裁决（100% duty 功率对比）"})
    for f in (0.1, 0.25, 0.5, 1.0):
        for duty in (0.25, 0.5, 0.75):
            for pl in payloads:
                steps.append({"id": f"p1_f{f:g}_d{duty}_{pl}",
                              "mod": "droop_mod", "payload": pl,
                              "freq_hz": f, "duty": duty, "mod_cores": m,
                              "eigen_cores": eigen_cores(m),
                              "duration": duration,
                              "note": "低频标定：BMC 电平差 → ΔI"})
    return steps


def steps_p2(duration=120):
    m = mod_cores(24, "same")
    return [{"id": f"p2_f{f:g}", "mod": "droop_mod", "payload": "dep",
             "freq_hz": f, "duty": 0.5, "mod_cores": m,
             "eigen_cores": eigen_cores(m), "duration": duration,
             "note": "中频过渡"} for f in (1e3, 1e4, 1e5, 1e6, 1e7)]


def steps_p3(payload="dep", duration=120):
    m = mod_cores(24, "same")
    freqs = [1e6, 2e6, 5e6, 1e7, 2e7, 5e7, 7.5e7, 1e8, 1.25e8, 1.5e8,
             1.75e8, 2e8]
    dense = {2e7, 5e7, 1e8, 1.5e8}
    steps = []
    for f in freqs:
        for d in ((0.3, 0.5, 0.7) if f in dense else (0.5,)):
            steps.append({"id": f"p3_f{f:g}_d{d}", "mod": "droop_mod",
                          "payload": payload, "freq_hz": f, "duty": d,
                          "mod_cores": m, "eigen_cores": eigen_cores(m),
                          "duration": duration, "note": "目标频段扫频"})
    return steps


def steps_p3b(freq, duty, payload="dep", duration=120):
    steps = []
    for place in ("same", "cross"):
        for mcount in (8, 16, 32, 64):
            m = mod_cores(mcount, place)
            steps.append({"id": f"p3b_m{mcount}_{place}",
                          "mod": "droop_mod", "payload": payload,
                          "freq_hz": freq, "duty": duty,
                          "mod_cores": m, "eigen_cores": eigen_cores(m),
                          "duration": duration, "note": "M/放置扫描"})
    return steps


def steps_p4(freq, duty, payload, duration=2400, placement="same", m=24):
    mcores = mod_cores(m, placement)
    return [{"id": "p4_best", "mod": "droop_mod", "payload": payload,
             "freq_hz": freq, "duty": duty, "mod_cores": mcores,
             "eigen_cores": eigen_cores(mcores), "duration": duration,
             "eigen_tests": EIGEN_TESTS_FULL,
             "note": "最优配置强化驻留：全家族 eigen + 最强调制（40min）"}]


# ---------------- 心跳 ----------------
def start_heartbeat(data_root, phase):
    import threading
    state = {"phase": phase, "step": "", "n": 0}
    stop = threading.Event()
    hb = os.path.join(data_root, "heartbeat.json")

    def loop():
        while not stop.wait(10.0):
            state["n"] += 1
            state["ts"] = time.time()
            try:
                with open(hb, "w") as f:
                    json.dump(state, f)
            except OSError:
                pass
    th = threading.Thread(target=loop, daemon=True)
    th.start()
    return stop, state


# ---------------- CLI ----------------
def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--phase", required=True,
                    choices=["p0", "p1", "p2", "p3", "p3b", "p4"])
    ap.add_argument("--data-root", default="/home/sdc/droop-excite")
    ap.add_argument("--repo",
                    default="/home/sdc/wangxu/operator/sdcshield")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--step-duration", type=float, default=None)
    ap.add_argument("--payload", default="dep",
                    help="p3/p3b/p4 的调制载荷（P1 裁决结果）")
    ap.add_argument("--freq", type=float, default=None,
                    help="p3b/p4 的频率（P3/P3b 最优结果）")
    ap.add_argument("--duty", type=float, default=None,
                    help="p3b/p4 的占空比")
    args = ap.parse_args(argv)

    if not args.dry_run:
        assert_topology()
    run_id = time.strftime("%Y%m%dT%H%M%S")
    run_dir = os.path.join(args.data_root, f"{args.phase}_{run_id}")
    paths = {"droop_mod": os.path.join(HERE, "droop_mod"),
             "sdcshield": os.path.join(args.repo, "builddir", "sdcshield"),
             "run_dir": run_dir}

    if args.phase == "p0":
        steps = steps_p0()
    elif args.phase == "p1":
        steps = steps_p1()
    elif args.phase == "p2":
        steps = steps_p2()
    elif args.phase == "p3":
        steps = steps_p3(payload=args.payload)
    elif args.phase == "p3b":
        if args.freq is None or args.duty is None:
            sys.exit("p3b 需要 --freq/--duty（来自 p3 最优）")
        steps = steps_p3b(args.freq, args.duty, payload=args.payload)
    else:
        if None in (args.freq, args.duty):
            sys.exit("p4 需要 --freq/--duty/--payload（来自 p3/p3b 最优）")
        steps = steps_p4(args.freq, args.duty, args.payload)
    if args.step_duration:
        for s in steps:
            s["duration"] = args.step_duration

    if args.dry_run:
        for s in steps:
            s["dir"] = "(dry)"
            print(json.dumps(
                {"id": s["id"],
                 "mod": build_mod_cmd(s, paths) if s.get("mod") else None,
                 "eigen": build_eigen_cmd(s, paths)
                 if s.get("eigen_cores") else None},
                ensure_ascii=False))
        return 0

    if os.geteuid() != 0:
        sys.exit("需要 root（ipmitool / SCHED_FIFO）")
    if any(s.get("mod") == "droop_mod" for s in steps) \
            and not os.path.exists(paths["droop_mod"]):
        sys.exit(f"missing {paths['droop_mod']}（先编译：见 README）")
    if any(s.get("eigen_cores") or s.get("mod") == "power_virus_dit"
           for s in steps) \
            and not os.path.exists(paths["sdcshield"]):
        sys.exit(f"missing {paths['sdcshield']}（--repo 指定仓库）")
    os.makedirs(run_dir, exist_ok=True)

    thresholds = dict(DEFAULT_THRESHOLDS)
    th_path = os.path.join(args.data_root, "thresholds.json")
    if os.path.exists(th_path):      # P0 之后可收紧持久化
        with open(th_path) as f:
            thresholds.update(json.load(f))
    hb_stop, hb_state = start_heartbeat(args.data_root, args.phase)
    try:
        for s in steps:
            hb_state["step"] = s["id"]
            print(f"=== {s['id']} ({s.get('note', '')}) ===", flush=True)
            run_step(s, paths, thresholds)
            if s.get("sdc_suspect"):
                print("SDC 疑似事件 —— 停机取证（spec D4）。证据目录：",
                      s["dir"], flush=True)
                break
            if s.get("aborted"):
                print("护栏触发，中止当步：", s["aborted"], flush=True)
    finally:
        hb_stop.set()
    print("run_dir:", run_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: 运行测试确认通过**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: 全部 PASS（19 项）。
Run: `python3 tools/droop-excite/run_sweep.py --phase p0 --dry-run`
Expected: 4 行 JSON 命令（引用真实输出）。

- [ ] **Step 5: Commit**

```bash
git branch --show-current
git add tools/droop-excite/
git commit -s -m "feat(droop-excite): 战役阶段编排 P0-P4（步生成/CLI/心跳/护栏中止/SDC 停机取证）"
git push
```

### Task 5: analyze.py（指标 + 分层排序表）

**Files:**
- Create: `tools/droop-excite/analyze.py`
- Test: `tools/droop-excite/tests/test_analyze.py`

**Interfaces:**
- Consumes: Task 3/4 落盘的 `steps/<id>/bmc.csv`（列名同 Task 1）与 `summary.json`（`timeline`、`eigen_rc`、`mod_rc`、`aborted`、`ras_delta`、`sel_delta`、`sdc_suspect` 等）。
- Produces: `step_metrics(step_dir)->dict`（`i_p2p_w/psu_min_w/psu_max_w/v_sag_sock0_mv/v_sag_sock1_mv/t_max_c/n_samples/...`）；`collect_run(run_dir)->list[dict]`；`rank_tables(runs)->str`（markdown 三层表：ΔI / V_sag / 事件）；CLI `python3 analyze.py <run_dir>...`。

- [ ] **Step 1: 写失败测试**

`tools/droop-excite/tests/test_analyze.py`：

```python
#!/usr/bin/env python3
"""analyze 单元测试。"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import analyze  # noqa: E402

HEADER = ("ts,CPU1 N_VDDAVS,CPU2 N_VDDAVS,CPU1 VDDAVS,CPU2 VDDAVS,"
          "Power,Power2,PS2 IOut,CPU1 Core Rem,CPU2 Core Rem")


def _mk_step(root, sid, sdc=False):
    d = os.path.join(root, "steps", sid)
    os.makedirs(d)
    rows = [HEADER]
    def emit(ts, v, p):
        rows.append(f"{ts},{v},{v},{v},{v},{p},{p},10,60,50")
    emit(0.0, 0.91, 300)                            # pre（0-10s）
    emit(20.0, 0.89, 500)                           # dwell（10-40s）
    emit(30.0, 0.89, 400)
    emit(50.0, 0.91, 300)                           # post
    with open(os.path.join(d, "bmc.csv"), "w") as f:
        f.write("\n".join(rows) + "\n")
    summary = {"id": sid, "timeline": {"pre_start": 0, "dwell_start": 10,
                                       "dwell_end": 40, "post_end": 60},
               "payload": "dep", "freq_hz": 0.5, "duty": 0.5,
               "mod_cores": [0], "note": "", "sdc_suspect": sdc,
               "aborted": None, "ras_delta": 0, "sel_delta": 0,
               "eigen_rc": 0, "mod_rc": 0}
    with open(os.path.join(d, "summary.json"), "w") as f:
        json.dump(summary, f)
    return d


class TestStepMetrics(unittest.TestCase):
    def test_metrics_math(self):
        with tempfile.TemporaryDirectory() as td:
            d = _mk_step(td, "s1")
            m = analyze.step_metrics(d)
        self.assertEqual(m["i_p2p_w"], 100.0)          # 500-400
        self.assertEqual(m["v_sag_sock0_mv"], 20.0)    # (0.91-0.89)*1000
        self.assertEqual(m["n_samples"], 2)
        self.assertEqual(m["t_max_c"], 60.0)

    def test_rank_tables_marks_events(self):
        with tempfile.TemporaryDirectory() as td:
            _mk_step(td, "s_ok")
            _mk_step(td, "s_bad", sdc=True)
            md = analyze.rank_tables([td])
        self.assertIn("s_bad", md)
        self.assertIn("s_ok", md)
        self.assertIn("阴性结果", md)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 运行确认失败**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: ERROR（`No module named 'analyze'`）。

- [ ] **Step 3: 实现 analyze.py**

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""droop 战役分析：分层排序表（ΔI 标定 / V_sag / 事件）。

用法: python3 analyze.py /home/sdc/droop-excite/p1_2026... [更多 run_dir...]
判据边界（spec §8）：ΔI 来自低频档 min/max 电平差（非波形重建）；
V_sag = pre 均值 − dwell 最小值；高频纹波幅度 BMC 不可测——不虚构。
"""
import csv
import glob
import json
import os
import sys


def load_bmc_csv(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def _num(row, key):
    try:
        return float(row.get(key, "") or "")
    except ValueError:
        return None


def window(rows, t0, t1):
    return [r for r in rows if t0 <= float(r["ts"]) <= t1]


def psu_total_series(rows):
    out = []
    for r in rows:
        a, b = _num(r, "Power"), _num(r, "Power2")
        if a is not None and b is not None:
            out.append(a + b)
    return out


def _vmin(rows, key):
    vals = [v for r in rows if (v := _num(r, key)) is not None]
    return min(vals) if vals else None


def _vmean(rows, key):
    vals = [v for r in rows if (v := _num(r, key)) is not None]
    return sum(vals) / len(vals) if vals else None


def step_metrics(step_dir):
    with open(os.path.join(step_dir, "summary.json")) as f:
        s = json.load(f)
    rows = load_bmc_csv(os.path.join(step_dir, "bmc.csv"))
    t = s["timeline"]
    pre = window(rows, t["pre_start"], t["dwell_start"])
    dwell = window(rows, t["dwell_start"], t["dwell_end"])
    psu = psu_total_series(dwell)
    temps = [v for r in dwell
             for k in ("CPU1 Core Rem", "CPU2 Core Rem")
             if (v := _num(r, k)) is not None]
    metrics = {
        "id": s["id"], "note": s.get("note", ""),
        "payload": s.get("payload", "-"),
        "freq_hz": s.get("freq_hz"), "duty": s.get("duty"),
        "mod_ncores": len(s.get("mod_cores", [])),
        "n_samples": len(dwell),
        "i_p2p_w": (max(psu) - min(psu)) if psu else None,
        "psu_min_w": min(psu) if psu else None,
        "psu_max_w": max(psu) if psu else None,
        "v_sag_sock0_mv": None, "v_sag_sock1_mv": None,
        "t_max_c": max(temps) if temps else None,
        "eigen_rc": s.get("eigen_rc"), "mod_rc": s.get("mod_rc"),
        "aborted": s.get("aborted"),
        "ras_delta": s.get("ras_delta"), "sel_delta": s.get("sel_delta"),
        "sdc_suspect": s.get("sdc_suspect"),
    }
    for sock, key in ((0, "CPU1 N_VDDAVS"), (1, "CPU2 N_VDDAVS")):
        base, low = _vmean(pre, key), _vmin(dwell, key)
        if base is not None and low is not None:
            metrics[f"v_sag_sock{sock}_mv"] = round((base - low) * 1000.0, 1)
    return metrics


def collect_run(run_dir):
    steps = []
    for d in sorted(glob.glob(os.path.join(run_dir, "steps", "*"))):
        if os.path.exists(os.path.join(d, "summary.json")):
            steps.append(step_metrics(d))
    return steps


def _fmt(v):
    return "-" if v is None else f"{v:.6g}"


def rank_tables(runs):
    allsteps = []
    for r in runs:
        allsteps.extend(collect_run(r))
    lines = ["# droop 战役分层排序表", ""]

    def hdr(cols):
        return ("| " + " | ".join(cols) + " |",
                "| " + "|".join(["---"] * len(cols)) + " |")

    by_i = sorted([s for s in allsteps if s["i_p2p_w"] is not None],
                  key=lambda s: -s["i_p2p_w"])
    h, sep = hdr(["step", "payload", "freq_hz", "duty", "i_p2p_w",
                  "psu_max_w", "t_max_c"])
    lines += ["## 1. 低频 ΔI 摆幅（i_p2p_w 降序；仅低频档有意义）", "", h, sep]
    for s in by_i:
        lines.append(f"| {s['id']} | {s['payload']} | {_fmt(s['freq_hz'])} | "
                     f"{_fmt(s['duty'])} | {_fmt(s['i_p2p_w'])} | "
                     f"{_fmt(s['psu_max_w'])} | {_fmt(s['t_max_c'])} |")
    by_v = sorted([s for s in allsteps if s["v_sag_sock0_mv"] is not None],
                  key=lambda s: -(s["v_sag_sock0_mv"] or 0))
    h, sep = hdr(["step", "v_sag_sock0_mv", "v_sag_sock1_mv", "psu_max_w"])
    lines += ["", "## 2. V_sag（mV，socket0 降序）", "", h, sep]
    for s in by_v:
        lines.append(f"| {s['id']} | {_fmt(s['v_sag_sock0_mv'])} | "
                     f"{_fmt(s['v_sag_sock1_mv'])} | {_fmt(s['psu_max_w'])} |")
    ev = [s for s in allsteps
          if s.get("sdc_suspect") or s.get("aborted")
          or (s.get("ras_delta") or 0) > 0 or (s.get("sel_delta") or 0) > 0]
    h, sep = hdr(["step", "sdc_suspect", "aborted", "ras_delta",
                  "sel_delta", "mod_rc", "eigen_rc"])
    lines += ["", "## 3. 事件（SDC 疑似/护栏/RAS/SEL）", "", h, sep]
    for s in ev:
        lines.append(f"| {s['id']} | {s.get('sdc_suspect')} | "
                     f"{s.get('aborted')} | {s.get('ras_delta')} | "
                     f"{s.get('sel_delta')} | {s.get('mod_rc')} | "
                     f"{s.get('eigen_rc')} |")
    if not ev:
        lines.append("| （无事件——阴性结果，如实记录） | | | | | | |")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv[1:]) == 0:
        sys.exit(__doc__)
    print(rank_tables(sys.argv[1:]))


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: 运行测试确认通过**

Run: `python3 -m unittest discover -s tools/droop-excite/tests -v`
Expected: 全部 PASS（21 项）。

- [ ] **Step 5: Commit**

```bash
git branch --show-current
git add tools/droop-excite/
git commit -s -m "feat(droop-excite): 分析器（step_metrics/rank_tables 分层排序，判据边界如实）"
git push
```

### Task 6: README.md + 合规验证

**Files:**
- Create: `tools/droop-excite/README.md`

**Interfaces:**
- Consumes: Task 1-5 全部。
- Produces: 文档；合规证据（zstd19 回归真实输出、`--list-tests` 计数不变、框架零改动 git 证据）。

- [ ] **Step 1: 写 README.md**

```markdown
# droop-excite — 瞬态电压骤降（droop/di/dt）激发实验工具

Spec: `docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md`
（0103 专用；设计/护栏/测量边界以 spec 为准）

**与 sdcshield 解耦**：仅经 CLI 调用 `builddir/sdcshield` 二进制（`--repo`
指定），零构建/代码依赖；无 sdcshield 时调制器/记录器/分析器仍可独立运行。

## 组成
| 文件 | 职责 |
|---|---|
| droop_mod.c | 周期级负载调制器（aarch64 NEON；指令计数门控 0.1Hz-325MHz） |
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
BMC ~3s 采样：能验证满载 DC 压降（先例 -0.02V）、低频（0.1-1Hz）min/max
电平差与 ΔI、温度/功率护栏；**不能**测 50-150MHz 纹波幅度（差 7 个数量级，
需示波器探 VRD 测点，超出远程能力）。高频段证据 = 功能层（eigen golden
SDC / 调制器 exit 2 / RAS / 稳定性）。多核相位相干度不可远程测量。
"最大 dv/dt 配置"判据 = 低频 ΔI 标定 + DC 压降深度 + 功能事件，分层呈现。
```

- [ ] **Step 2: 合规验证（规则 2）**

```bash
python3 -m unittest discover -s tools/droop-excite/tests -v   # 全 PASS
bash tools/droop-excite/tests/test_droop_mod.sh               # ALL PASS
./builddir/sdcshield --list-tests | wc -l                     # 与实验前一致（484）
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1  # exit: pass（CLAUDE.md 基线命令）
git status --porcelain framework/ tests/ meson.build          # 空输出=框架零改动
```
Expected: 全部符合；真实输出引用到进展日志。

- [ ] **Step 3: Commit**

```bash
git add tools/droop-excite/README.md
git commit -s -m "docs(droop-excite): README（构建/参数/护栏/测量边界）"
git push
```

---

## 战役执行任务（真实运行；每步引用真实输出；全程 root 于 0103）

> 通用纪律：每阶段开跑前 `cat /home/sdc/droop-excite/heartbeat.json` 确认无残留战役；
> 阶段结束把关键数字（引用 analyze 真实输出）追加进 `docs/research/progress-log.md`
> 并单独提交；护栏触发或 SDC 疑似 → 停、保全 `steps/<id>/`、报告用户。

### Task 7: P0 冒烟战役

- [ ] 编译并 `sudo python3 tools/droop-excite/run_sweep.py --phase p0 --data-root /home/sdc/droop-excite`
- [ ] 检查 4 步全部完成：`python3 tools/droop-excite/analyze.py /home/sdc/droop-excite/p0_*`（p0_2/p0_3/p0_4 的 psu_max_w 应显著高于 p0_1；p0_4 相对 p0_2 的 v_sag 变化记录在案）
- [ ] 收紧功率护栏：取 p0_2/p0_3/p0_4 中最大 psu_max_w 记为 P_full，写 `/home/sdc/droop-excite/thresholds.json`：`{"psu_total_w_abort": <P_full*1.15 取整>}`（cat 引用真实值）
- [ ] progress-log 追加 P0 结果并 commit -s + push
- Expected: 4 步 `sdc_suspect=false`、无 aborted；温度 <90℃（CPU1 起 63℃）；若温度逼近护栏 → 如实记录并缩短后续步长

### Task 8: P1 低频标定战役

- [ ] `sudo python3 tools/droop-excite/run_sweep.py --phase p1`（26 步 ≈ 1.2h；跨会话可凭 heartbeat 续跑：重跑同命令新 run-id，只补缺失步骤可手动单步）
- [ ] `python3 tools/droop-excite/analyze.py /home/sdc/droop-excite/p1_* > /tmp/p1_rank.md`，确认：p1_payload_dep vs ilp2 的 psu_max_w 分出高下（载荷裁决）；各 f/duty 的 i_p2p_w 成表
- [ ] 载荷裁决结论（dep/ilp2 谁的 ΔI 大）写入 progress-log 并 commit；此结论决定 Task 10 的 --payload
- Expected: 低频步 i_p2p_w 非零（调制真实可见）；eigen_rc=0；若 i_p2p_w≈0 → 调制器有问题，停下按 systematic-debugging 排查，不得进入 P3

### Task 9: P2 中频过渡战役

- [ ] `sudo python3 tools/droop-excite/run_sweep.py --phase p2`（5 步 ≈ 20min）
- [ ] `analyze.py` 检查：psu_max_w 随频率变化连续（门控未崩的证据）；measured freq（各步 mod_summary.json 的 freq_measured_hz_mean）与目标偏差 <30%（refine_iters 生效证据，引用真实值）
- [ ] 金丝雀（canary）go/no-go 决策点：默认 **no-go**（不实现，spec §6.4 标注探索性）——在 progress-log 记录该决策与理由；若 P3 出现异常事件再议
- [ ] progress-log + commit
- Expected: 5 步全绿；ras_delta=0

### Task 10: P3 + P3b 扫频战役

- [ ] `sudo python3 tools/droop-excite/run_sweep.py --phase p3 --payload <P1裁决>`（20 步 ≈ 55min）
- [ ] `analyze.py /home/sdc/droop-excite/p3_* > /tmp/p3_rank.md`；v_sag_sock0_mv 排序 + 事件表审查
- [ ] 选最优（判据分层：① 事件优先；② v_sag 最深；③ P1 标定 ΔI 最大）→ 记录 (freq, duty)
- [ ] `sudo python3 tools/droop-excite/run_sweep.py --phase p3b --freq <F> --duty <D> --payload <P>`（8 步 ≈ 25min）→ M/放置结论
- [ ] progress-log + commit
- Expected: 全部 measured freq 落在目标 ±30%；无 sdc_suspect（若有 → D4 停机取证，跳到 Task 12 报告分支）

### Task 11: P4 最优强化战役（SDC 狩猎）

- [ ] `sudo python3 tools/droop-excite/run_sweep.py --phase p4 --freq <F> --duty <D> --payload <P>`（1 步 40min，全家族 eigen + 最强调制）
- [ ] 结束后 `analyze.py` + 检查 `steps/p4_best/`：eigen.log 尾部 `exit:` 行、mod_summary.json 的 checksum_failures
- [ ] 两种结果都如实：(a) **命中**（eigen_rc≠0 / mod_rc=2 / ras_delta>0）→ 停机取证已由驱动自动执行；补 `ipmitool sel elist > steps/p4_best/sel_elist.txt`；报告用户等裁定；(b) **未命中** → 阴性结果，写入报告
- [ ] progress-log + commit
- Expected: 驻留 40min 无护栏触发；无论命中与否结论如实落盘

### Task 12: 实验报告与收尾

- [ ] 写 `docs/sdc-excite-reproduce/droop-excitation-experiment.md`：目的（含"50-150MHz 是仓库外假设、本实验即检验"的定位）→ 方法（仪器/阶段/护栏）→ 仪器标定结果（P1 引用真实数字）→ 分层排序表（引用 analyze 输出）→ 事件记录 → **测量边界声明**（BMC 不可测高频纹波；相干度不可测；判据为代理指标）→ 结论（最大 dv/dt 参数配置 + 证据强度）→ 后续（方案 B 固化、示波器实测、金丝雀）
- [ ] `git add docs/sdc-excite-reproduce/droop-excitation-experiment.md docs/research/progress-log.md && git commit -s -m "docs(droop-excite): 0103 瞬态电压骤降激发实验报告（参数排序+边界声明）" && git push`
- [ ] 收尾汇报：分支提交列表、实验数据位置（/home/sdc/droop-excite/）、PR 建议（走 /pr-loop 六阶段，人工 merge）

---

## Self-Review 记录（写计划时已核）

1. **Spec 覆盖**：§5 架构=Task 2-4；§6 四组件=Task 1-5（canary 按 spec §6.4 可选，Task 9 决策点默认 no-go）；§7 P0-P4=Task 7-11；§8 边界=README+报告（Task 6/12）；§9 护栏=run_sweep evaluate_guardrails（Task 3）；§10 交付=Task 1-6/12；§11 合规=Task 6 Step 2；D5 解耦=run_sweep --repo/懒检查 sdcshield（Task 4）。无缺口。
2. **占位符**：全文无 TBD/TODO；所有代码完整给出。
3. **类型/命名一致性**：传感器名在 bmc_logger.SENSORS / evaluate_guardrails / analyze.step_metrics 三处一致；mod_summary 字段与 refine_iters/Task 2 摘要一致；`steps_p*` 字段与 build_*_cmd 消费一致；退出码 2 语义在 droop_mod/run_step/sdc_suspect 三处一致。
4. **自审中修复的缺陷（已改入上文代码）**：(a) `gate_loop` 前向调用 `sbarrier_wait` 缺前向声明；(b) 缺 `#include <stdatomic.h>`；(c) 部分线程创建失败时 `pthread_barrier` 永久悬挂 → 改用带 g_stop 逃逸的 `sbarrier_wait` 做起始对齐；(d) `build_*_cmd` 内的 sdcshield 存在性检查会使 dry-run 在无仓库环境退出 → 移至 main 非 dry-run 分支；(e) `steps_p3b` 残留未定义变量 `d` 的笔误。



