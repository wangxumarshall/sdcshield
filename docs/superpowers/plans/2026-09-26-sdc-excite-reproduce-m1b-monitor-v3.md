# sdc-excite-reproduce M1b（monitor v3 + 驱动加固）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 补丁单元 4（monitor v3：发现式列集/离散态 diff/EDAC-vmstat-NUMA 列/UE 告警/collector 看门狗）+ RCA 驱动加固四项（fail|crash 提取修复/复测内存护栏/control 孤儿清理/runtime 单位注记），部署到正在运行的战役。

**Architecture:** monitor 侧把"SDR 发现式解析 + 离散态 diff"抽为 `sdc_common.sh` 纯函数（pytest 经 bash 子进程可测），主循环仅替换 CSV 列集装配段——**安全联锁段（热/风扇/内存/磁盘/SEL/cmd 代理）逐字不动**（v5 §13.4）；driver 侧是 `handle_failure` 内的四处精确修改。全部为 bash，Python 只做测试驱动。

**Tech Stack:** bash 4+（现有风格 `set -u` + 函数）、python3+pytest（bash 函数的子进程测试）、ipmitool、EDAC sysfs、systemctl。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5 §2.2 12 维矩阵 / §6.4 归一化 / §6.6 RAS / §8.6 联锁；RCA 建议=output 文档附3）。范围：M1b = v5 §14.2 单元 4 + RCA 加固；ring/burst/BPF 属 M2，81 机属 M5。

## Global Constraints

- **一补丁一单元**：每 Task 一 commit；`git commit -s` 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>` 其后无任何内容、无 Co-Authored-By；分支 `feat/sdc-excite-reproduce-m1b`（从 main 建）；`git branch --show-current` 核实后才 commit；**绝不提交/推送 main**（集成经 PR，API token 在环境）。
- **战役正在运行**（cycle 5，sdc-monitor + sdc-excite-reproduce active）：代码改动落 repo 不影响运行中进程；**部署步骤（Task 6）才重启服务**——monitor 重启=秒级联锁间隙（systemd 拉起）；driver 重启=state.json 断点续跑（当前 phase 会重跑，如实记录）。实现/验证期间**不得**重启任何服务、不得干扰 M1 采集器（24h 门运行中至 2026-09-27 00:24）。
- **安全联锁不可削弱**：sdc_monitor.sh 的 :184-293（温度/风扇/内存/磁盘/SEL/cmd 代理）与 campaign.env 阈值语义**逐字保留**；monitor v3 只动 :38-115（表头与采样装配）并新增段。
- **测试纪律**：新 bash 函数必须有 pytest 子进程测试（fixture 驱动，不依赖真实 BMC/EDAC——环境变量可注入假数据源）；全套既有 38 测试不得回归。
- **零第三方依赖**；`env -u SDC_ROOT_PW` 用于非 root 验证；root 经 `/tmp/sdc_inv_stage/su_run.py`。
- **术语**：sdc-excite-reproduce（无 campaign 新引入）。
- x86/框架零改动（不碰 framework/ tests/ meson.build）。

---

### Task 1: SDR 发现式解析与离散态 diff 纯函数（sdc_common.sh + pytest）

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc_common.sh`（文件尾追加两个函数，不动既有内容）
- Create: `tools/telemetry/tests/test_sdc_common_bash.py`

**Interfaces:**
- Consumes: 无（纯函数；输入为文本）
- Produces（后续 Task 2/3 直接调用）:
  - `sdr_analog_columns <<<"$SDR"` → stdout 每行 `规范化名|数值`（仅模拟量：读数含数字；名规范化 `tr -c 'a-zA-Z0-9' '_'` + 小写，例 `CPU1 Core Rem` → `cpu1_core_rem`；同 SDR 两种格式 3/5 字段通吃——沿用 :46-48 注释的实测口径 `$5 空则 $2`）
  - `sdr_discrete_map <<<"$SDR"` → stdout 每行 `规范化名|原始读数`（读数非纯数字的传感器，例 `CPU1 Prochot|0x00`）
  - `discrete_diff <旧map文件> <新map文件>` → stdout 每行 `ts=不写(调用方加)|name: 旧 → 新`（仅变化的行；退出码=变化行数）；无旧文件时输出全部为 `INIT name: → 新`

- [ ] **Step 1: 写失败测试** `tools/telemetry/tests/test_sdc_common_bash.py`：

```python
import os, subprocess, sys

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")

# 真实实测过的两种 ipmitool sdr list 行格式（3 字段 / 5 字段），含模拟量与离散量
SDR_FIXTURE = """CPU1 Core Rem       | 45      | ok
CPU2 Core Rem       | 47      | ok
CPU1 VDDAVS         | 0.88    | ok
CPU1 Prochot        | 0x00    | ok
FAN2 Speed          | 11475   | ok
Power               | 300     | ok
SEL                 | 0x00    | ok
"""
DISCRETE_ONLY = """CPU1 Prochot        | 0x00    | ok
PSU Redundancy      | 0x00    | ok
"""

def _bash(body, stdin=""):
    return subprocess.run(["bash", "-c", f"source '{COMMON}'\n{body}"],
                          input=stdin, capture_output=True, text=True, timeout=15)

def test_sdr_analog_columns():
    p = _bash("sdr_analog_columns", SDR_FIXTURE)
    lines = p.stdout.strip().splitlines()
    assert p.returncode == 0
    assert "cpu1_core_rem|45" in lines and "cpu2_core_rem|47" in lines
    assert "cpu1_vddavs|0.88" in lines and "fan2_speed|11475" in lines
    assert "power|300" in lines
    assert not any(l.startswith("cpu1_prochot") for l in lines)   # 离散量不入
    assert not any(l.startswith("sel|") for l in lines)

def test_sdr_discrete_map():
    p = _bash("sdr_discrete_map", SDR_FIXTURE)
    lines = p.stdout.strip().splitlines()
    assert "cpu1_prochot|0x00" in lines
    assert not any(l.startswith("cpu1_core_rem") for l in lines)  # 模拟量不入

def test_discrete_diff_transition(tmp_path):
    old = tmp_path / "old.map"; new = tmp_path / "new.map"
    old.write_text("cpu1_prochot|0x00\npsu_redundancy|0x00\n")
    new.write_text("cpu1_prochot|0x01\npsu_redundancy|0x00\n")
    p = _bash(f"discrete_diff {old} {new}")
    assert p.returncode == 1 and "cpu1_prochot: 0x00 → 0x01" in p.stdout
    assert "psu_redundancy" not in p.stdout

def test_discrete_diff_init(tmp_path):
    new = tmp_path / "new.map"
    new.write_text("cpu1_prochot|0x00\n")
    p = _bash(f"discrete_diff {tmp_path / 'absent.map'} {new}")
    assert p.returncode == 1 and "cpu1_prochot: INIT → 0x00" in p.stdout
```

- [ ] **Step 2: 跑测试确认失败**

```bash
env -u SDC_ROOT_PW python3 -m pytest tools/telemetry/tests/test_sdc_common_bash.py -q
```
Expected: 4 FAIL（函数未定义，bash source 后调用报 command not found → returncode≠0/断言失败）。

- [ ] **Step 3: 实现**（`sdc_common.sh` 文件尾追加）：

```bash
# ---------------- monitor v3: SDR 发现式解析（v5 §6.4 归一化 + 单元 4）----------------
# 输入: stdin = 一次 ipmitool sdr list 全量输出（两种实测格式通吃: 3 字段 name|reading|status
#       / 5 字段 name|id|status|x|reading——取 $5 空则退 $2，与 sdc_monitor.sh 既有 sdr_raw 同口径）
_sdr_norm() { tr -c 'a-zA-Z0-9\n' '_' <<<"$1" | tr 'A-Z' 'a-z' | sed 's/_*$//;s/^_*//'; }
sdr_analog_columns() {
    awk -F'|' 'NF>=3 {
        v=$5; if (v ~ /^[ ]*$/) v=$2; gsub(/^ +| +$/,"",v); gsub(/^ +| +$/,"",$1)
        if (v ~ /[0-9]/) print $1 "|" v
    }' | while IFS='|' read -r name val; do
        [ -n "$name" ] && printf "%s|%s\n" "$(_sdr_norm "$name")" "$(grep -oE '[0-9]+(\.[0-9]+)?' <<<"$val" | head -1)"
    done
}
sdr_discrete_map() {
    awk -F'|' 'NF>=3 {
        v=$5; if (v ~ /^[ ]*$/) v=$2; gsub(/^ +| +$/,"",v); gsub(/^ +| +$/,"",$1)
        if (v !~ /[0-9]/ && $1 != "") print $1 "|" v
    }' | while IFS='|' read -r name val; do
        printf "%s|%s\n" "$(_sdr_norm "$name")" "$val"
    done
}
# discrete_diff <旧map> <新map>: 输出变化行（旧无=INIT），退出码=变化数（0=无变化）
discrete_diff() {
    local old="$1" new="$2" rc=0 line name val
    [ -f "$old" ] || touch "$old"
    while IFS='|' read -r name val; do
        [ -z "$name" ] && continue
        prev=$(grep -m1 "^${name}|" "$old" | cut -d'|' -f2-)
        if [ -z "$prev" ]; then echo "${name}: INIT → ${val}"; rc=$((rc+1))
        elif [ "$prev" != "$val" ]; then echo "${name}: ${prev} → ${val}"; rc=$((rc+1)); fi
    done < "$new"
    return $rc
}
```

（注意 awk 判定离散量用 `v !~ /[0-9]/`：`0x00` 含数字会误入模拟量——修正：模拟量判定改为 `v ~ /^[ ]*[0-9]+(\.[0-9]+)?[ ]*$/`（纯数值），其余含 `0x`/文字的为离散量。以测试为准实现，两函数判定互补不重不漏。）

- [ ] **Step 4: 跑测试通过** → 4 passed（既有 38 不回归，全套 42）
- [ ] **Step 5: Commit** `scripts: SDR 发现式解析与离散态 diff 纯函数（monitor v3 地基，v5 单元 4）`

---

### Task 2: monitor v3 主体——发现式列集 + monitor.csv v3 + RAS/OS 扩列 + 看门狗

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc_monitor.sh:38-115`（表头/采样装配段重写；**联锁段 :184-293 与 cmd 代理逐字不动**——联锁仍用既有 `sdr_raw/sdr_val` 从同一 `$SDR` 取值，不受列集重构影响）
- Test: `tools/telemetry/tests/test_monitor_v3_bash.py`

**Interfaces:**
- Consumes: Task 1 的 `sdr_analog_columns`/`sdr_discrete_map`/`discrete_diff`
- Produces: `monitor/monitor.csv` v3（列 = `ts` + 发现式模拟量列（首次启动 SDR 全量发现，序持久化于 `monitor/sensors_v3.json`，重启列集一致；列集漂移→归档旧文件+alert）+ 固定尾列 `mc_ce_total,mc_ue_total,oom_kill,pgmajfault,numa_memavail_kb(numa0..3),disk_pct,sel5m,bmc_ok`）；`monitor/sensors_v3.json`；`monitor/discrete_events.log`（Task 3 填充 diff 内容，本任务建文件与空转）；`monitor/discrete_state.map`（当前离散态）
- 测试注入点：`MON_SDR_FILE`（非空→SDR 从该文件读，不调 ipmitool）、`MON_SELFTEST=1`（跑一个采样周期后退出——主循环 `while :; do` 改 `while :; do ... [ "${MON_SELFTEST:-0}" = 1 ] && break; sleep` 位置在周期末）

- [ ] **Step 1: 写失败测试**（`test_monitor_v3_bash.py`，fixture 复用 Task 1 的 SDR 文本）：

```python
import csv, os, subprocess, tempfile

MON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                   "scripts", "sdc-excite-reproduce", "sdc_monitor.sh")

SDR = open(os.path.join(os.path.dirname(__file__), "fixture_sdr.txt")).read() if os.path.exists(
    os.path.join(os.path.dirname(__file__), "fixture_sdr.txt")) else (
    "CPU1 Core Rem       | 45      | ok\nCPU1 Prochot        | 0x00    | ok\nPower | 300 | ok\n")

def run_monitor_once(tmp, sdr_text):
    f = os.path.join(tmp, "sdr.txt"); open(f, "w").write(sdr_text)
    env = dict(os.environ, MON_SELFTEST="1", MON_SDR_FILE=f,
               SDC_EXCITE_REPRODUCE_DIR=os.path.join(tmp, "data"),
               SDC_ROOT_PW="")
    env.pop("SDC_ROOT_PW", None)
    p = subprocess.run(["bash", MON], env=env, capture_output=True, text=True, timeout=60)
    return p

def test_v3_discovery_columns_and_ras_tail(tmp_path):
    p = run_monitor_once(str(tmp_path), SDR)
    csvp = os.path.join(str(tmp_path), "data", "monitor", "monitor.csv")
    rows = list(csv.reader(open(csvp)))
    hdr = rows[0]
    assert "cpu1_core_rem" in hdr and "power" in hdr      # 发现式模拟量列
    assert hdr[-1] == "bmc_ok" and "mc_ue_total" in hdr   # 固定尾列
    assert "oom_kill" in hdr and "numa0_memavail_kb" in hdr
    assert len(rows) == 2                                  # header + 1 周期
    assert rows[1][hdr.index("cpu1_core_rem")] == "45"

def test_v3_restart_column_stability(tmp_path):
    run_monitor_once(str(tmp_path), SDR)
    run_monitor_once(str(tmp_path), SDR)                   # 二次启动（复用 sensors_v3.json）
    csvp = os.path.join(str(tmp_path), "data", "monitor", "monitor.csv")
    lines = [l for l in open(csvp) if l.strip()]
    assert len({l.split(",")[1] for l in lines[1:]}) == 1   # 列集稳定（cpu1_core_rem 同位）
    assert sum(1 for l in lines[1:] if l.split(",")[hdr_i(csvp)]) >= 2
def hdr_i(p): return 1   # 辅助占位——实现时按真实断言写：两数据行的 cpu1_core_rem 值都为 45
```

（`hdr_i` 占位说明：第二个测试的真实断言 = `lines[1].split(",")[idx] == lines[2].split(",")[idx] == "45"`，idx 取自表头；实现者按此写，不留占位。）

- [ ] **Step 2: 确认失败**（MON_SELFTEST 语义尚不存在 → 测试超时/断言失败）
- [ ] **Step 3: 实现**（要点，逐条落进 :38-115 重写）：
  1. `CSV_HDR` 生成：首次启动 `SDR`（MON_SDR_FILE 注入或 ipmitool）→ `sdr_analog_columns` 得有序列名 → `ts,` + 列名 + 固定尾列（上表）→ 写 `sensors_v3.json`（列序数组）；重启读 json 重建表头——**列集漂移防御**：若当次发现列集 ≠ json 持久序 → 归档旧 CSV（沿用 :40-43 的 v1 归档模式，后缀 `_v3drift_时间戳`）+ alert + 以新列集开新文件；
  2. 周期采样：`analog=$(sdr_analog_columns <<<"$SDR")` → 按持久列序取值（缺失列空）；尾列取值：`mc_ce_total/mc_ue_total` = `cat /sys/devices/system/edac/mc/mc*/{ce,ue}_count` 求和（缺失=空）；`oom_kill/pgmajfault` = `/proc/vmstat` 对应行；`numa{0..3}_memavail_kb` = `/sys/devices/system/node/nodeN/meminfo` 的 MemAvailable 行（缺失节点=空）；其余沿用既有（disk/sel5m/bmc）；
  3. **UE 即时告警**：`mc_ue_total>0` → `alert "EDAC UE>0 total=... （v5 §8.6：即时告警不自动 PAUSE——用户决策点）"`（每周期至多一条，UE 清零后重置）；
  4. **collector 看门狗**：每周期 `for c in percore pmu ras; do systemctl is-active sdc-collector@$c >/dev/null || collector_fail[$c]++`，连续 3 周期 fail → alert（30min 内不重复告警：记录上次告警时间戳）；
  5. `MON_SELFTEST`/`MON_SDR_FILE` 注入点（测试钩子写进代码注释：生产不设即走 ipmitool）；
  6. **离散态**：每周期 `sdr_discrete_map > discrete_state.map.new` + Task 3 的 diff 调用点（本任务留调用位 + 注释）。
- [ ] **Step 4: 测试通过**（新 2 + Task 1 的 4 + 全套 38 = 44±，如实报告）；`bash -n` 过
- [ ] **Step 5: Commit** `scripts: monitor v3——发现式列集/RAS-OS 扩列/UE 告警/collector 看门狗（v5 单元 4，联锁段不动）`

---

### Task 3: monitor v3 离散态 diff + known_faults 白名单

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc_monitor.sh`（Task 2 留下的 diff 调用位）
- Test: `tools/telemetry/tests/test_monitor_v3_bash.py`（追加用例）

**Interfaces:**
- Consumes: Task 1 `discrete_diff`；`configs/sdc-excite-reproduce/known_faults.csv`（格式 `source,type,description,whitelist_action`；`whitelist_action=ignore`→不告警只记录，`log_only`→记录+一行标注）
- Produces: `monitor/discrete_events.log` 行格式 `[ts] TRANSITION name: 旧 → 新 [known_fault:log_only]`；断言告警规则 = 新值非 `0x00` 且非 `ok`（且非白名单 ignore）→ `alert "离散态断言 name: 旧 → 新"`

- [ ] **Step 1: 失败测试**（追加）：

```python
def test_discrete_transition_logged_and_alerted(tmp_path):
    # 首轮 0x00 → 二轮 0x01：discrete_events.log 有 TRANSITION 行，alerts.log 有断言告警
    run_monitor_once(str(tmp_path), "CPU1 Prochot        | 0x00    | ok\n")
    run_monitor_once(str(tmp_path), "CPU1 Prochot        | 0x01    | ok\n")
    d = os.path.join(str(tmp_path), "data", "monitor")
    dev = open(os.path.join(d, "discrete_events.log")).read()
    assert "cpu1_prochot: 0x00 → 0x01" in dev
    alerts = open(os.path.join(d, "alerts.log")).read()
    assert "离散态断言" in alerts and "cpu1_prochot" in alerts

def test_known_fault_whitelist(tmp_path):
    # known_faults 白名单（source=psu_redundancy, action=ignore）→ 有 transition 记录但无告警
    kf = os.path.join(str(tmp_path), "data", "known_faults.csv")
    os.makedirs(os.path.dirname(kf), exist_ok=True)
    open(kf, "w").write("source,type,description,whitelist_action\npsu_redundancy,discrete,test,ignore\n")
    env_note = ""   # monitor 每周期重读 known_faults.csv 于数据根（实现约定：$EXCITE_REPRODUCE_DIR/known_faults.csv，缺省回退 repo configs/——实现时二选一并保持测试一致）
    run_monitor_once(str(tmp_path), "PSU Redundancy      | 0x00    | ok\n")
    run_monitor_once(str(tmp_path), "PSU Redundancy      | 0x01    | ok\n")
    d = os.path.join(str(tmp_path), "data", "monitor")
    assert "psu_redundancy: 0x00 → 0x01" in open(os.path.join(d, "discrete_events.log")).read()
    assert "psu_redundancy" not in open(os.path.join(d, "alerts.log")).read()
```

- [ ] **Step 2-4: 红→实现→绿**。实现要点：周期内 `discrete_diff "$MON_DIR/discrete_state.map" "$MON_DIR/discrete_state.map.new"`，输出非空→逐行追加 `[ts] TRANSITION ...` 到 discrete_events.log + `mv new→state`；断言判定：新值非 `0x00` 且非 `ok`/`nr`/`0x0` 类正常值（实现成 `case "$new" in 0x00|ok|OK) 跳过告警;; *) 告警;; esac`）；known_faults 读取 `$EXCITE_REPRODUCE_DIR/known_faults.csv`（缺失回退 `repo/configs/sdc-excite-reproduce/known_faults.csv`，路径由脚本相对定位），`ignore` 行的 source 匹配（规范化名）→ 只记录。
- [ ] **Step 5: Commit** `scripts: monitor v3 离散态 diff→discrete_events.log + known_faults 白名单（v5 §6.4 全量离散 diff）`

---

### Task 4: driver 修复——fail|crash 提取（RCA 根因）+ runtime 单位注记

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh:141,144`（handle_failure 内两处）
- Test: `tools/telemetry/tests/test_driver_extract.py`（把提取三行抽成可测函数？bash 内联 awk 不便单测——**最小方案**：提取逻辑已极简（两行 awk/grep），测试用 bash 子进程直接对 fixture .out 文件跑同款 awk 断言 crash 也能提取；若实现者选择把提取抽到 sdc_common.sh 函数 `extract_failed_test <outsum>`/`extract_fail_seed <outsum>` 则更好，二选一）

**Interfaces:**
- Produces: `handle_failure` 的 targeted 复测路径对 `result: crash` 事件同样生效（RCA：cold_c4 因 crash 不入提取→走了 wholecmd 重放泄漏命令的路径——修复后 crash+seed 走 120s 定向复测）

- [ ] **Step 1: 失败测试**（fixture = cold_c4 真实 stdout_summary 形态：`- test: memcpy_rewr` + `  result: crash` + `  fail: { ... seed: 'AES:...' }`）：

```python
import os, subprocess

DRIVER = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc-excite-reproduce.sh")

CRASH_OUT = """tests:
- test: memcpy_rewr
  result: crash
  result-details: { crashed: true, code: 9, reason: 'Killed' }
  fail: { cpu-mask: null, time-to-fail: null, seed: 'AES:87cbf86c' }
- test: cachebounce
  result: pass
"""

def extract(out_text, fn):
    # 驱动 :141 原文提取逻辑（修复后应含 crash）——直接跑驱动源里的函数或同款命令
    with open("/tmp/_drv_out", "w") as f: f.write(out_text)
    p = subprocess.run(["bash", "-c",
        "source '%s' 2>/dev/null; %s" % (DRIVER, fn)], capture_output=True, text=True)
    return p.stdout.strip()

def test_crash_test_extracted():
    # 修复判据：crash 事件的测试名与种子都能进入 targeted 复测路径
    t = extract(CRASH_OUT, "awk '/^- test:/{t=$3} /^  result: *(fail|crash)/{print t; exit}' /tmp/_drv_out")
    assert t == "memcpy_rewr"
```

（实现若抽函数则测试改为调函数；断言核心不变：**crash 也必须提取出 test 名与 AES seed**。）

- [ ] **Step 2-4: 红→修→绿**。修改（`:141`）：
  `failed_test=$(awk '/^- test:/{t=$3} /^  result: *fail/{print t; exit}' "$outsum")` →
  `failed_test=$(awk '/^- test:/{t=$3} /^  result: *(fail|crash)/{print t; exit}' "$outsum")`；
  `:144` 同理 `'result: *fail'` → `'result: *(fail|crash)'`；
  `:142` 的 seed 提取行保持（`fail: {` 行 crash 事件同样存在）。
  runtime 单位注记：`yaml_extract.txt` 头部注释追加一行 `# 注：threads[].runtime 字段单位为毫秒（RCA 2026-09-25 实验证实），分析勿按秒读`。
- [ ] **Step 5: Commit** `scripts: handle_failure 提取修复——crash 事件同样走定向复测（RCA 附3 根因）+ runtime 单位注记`

---

### Task 5: driver 加固——复测内存护栏 + control 孤儿清理

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh`（handle_failure 复测段 :170-188 + 脚本 cleanup 段 :413 附近）
- Test: `tools/telemetry/tests/test_driver_memguard.py`

**Interfaces:**
- Produces:
  - `retest_guarded <timeout参数> <命令...>`（bash 函数，handle_failure 内定义或 sdc_common.sh）：前置 MemAvailable 检查（< 6GB → 等待 ≤60s；仍低 → 返回 250 并由调用方记 `retest skipped(memguard)`）；运行期看门狗（每 5s 查 MemAvailable < 2.5GB → `kill -KILL` 复测进程组）——**护栏针对复测，不动主战役负载**（主负载的内存防线是 monitor 联锁，v5 §8.6 分工）
  - `cleanup_orphans()`：`pkill -KILL -x control -u "$(id -un)"`（清"control"孤儿切片——RCA：其 comm 名使 pkill -x sdcshield 失效）；调用点 = handle_failure 复测前、阶段超时 kill 路径（:111 的 timeout 兜底后）、脚本 EXIT trap（:413 附近既有清理段追加）

- [ ] **Step 1: 失败测试**（memguard 逻辑抽到 sdc_common.sh 函数 `memgate_wait <min_kb> <wait_s>` → 返回 0/1；看门狗 `memwatch_bg <pid> <threshold_kb>` 后台函数——两者 pytest 子进程可测；fixture 用假 /proc/meminfo 不可注入→函数参数化：阈值与当前值都以参数传入纯判定函数 `mem_below <cur_kb> <threshold_kb>`——**纯函数化以便测试**，取数在调用方）：

```python
import os, subprocess
COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")
def bash(body): return subprocess.run(["bash", "-c", f"source '{COMMON}'\n{body}"],
                                      capture_output=True, text=True, timeout=15)
def test_mem_below():
    assert bash("mem_below 100 200 && echo Y").stdout.strip().endswith("Y")
    assert bash("mem_below 300 200 || echo N").stdout.strip().endswith("N")
    assert bash("mem_below '' 200 || echo N").stdout.strip().endswith("N")   # 空值=不拦
def test_orphan_cleanup_targets_own_user_only():
    # cleanup_orphans 的 pkill 必须限定 -u 当前用户（不能误杀他人 control 进程）
    src = open(COMMON).read()
    assert 'pkill -KILL -x control -u' in src
```

- [ ] **Step 2-4: 红→实现→绿**（`mem_below`/`cleanup_orphans` 入 sdc_common.sh；`retest_guarded` 在 handle_failure 内组合：`ma=$(awk /MemAvailable/ /proc/meminfo)`；两处复测循环的 `timeout ... "$BIN" ...` 包进 guard：先 `mem_below "$ma" 6291456 &&` 否则 sleep 60 重取一次再判，仍低→`echo "retest$i skipped(memguard ma=${ma}kB)" >> retests.txt; continue`；运行期看门狗：`"$BIN" ... & rpid=$!; while kill -0 $rpid 2>/dev/null; do sleep 5; ma=...; mem_below "$ma" 2621440 && { kill -KILL $rpid 2>/dev/null; echo "retest$i killed(memwatch ma=${ma}kB)" >> retests.txt; break; }; done; wait $rpid`——注意 timeout 包装层保留）。
- [ ] **Step 5: Commit** `scripts: 复测内存护栏（前置门+看门狗）+ control 孤儿清理（RCA 附3 建议 1/2/3）`

---

### Task 6: 部署与冒烟（root 重启两服务 + 全通道验证）

**Files:**
- Modify: `scripts/sdc-excite-reproduce/NEW_BOARD_ONBOARDING.md`（v3 列集/离散 diff/memguard 三段说明）
- Ops: 服务重启

- [ ] **Step 1: root 部署**（经 su_run.py；顺序：先 monitor 后 driver；**记录重启前后 driver.log 行**）：
```bash
timeout 200 python3 /tmp/sdc_inv_stage/su_run.py "systemctl restart sdc-monitor && sleep 5 && systemctl is-active sdc-monitor && journalctl -u sdc-monitor -n 5 --no-pager | tail -5"
# monitor 稳定后再重启 driver（state.json 断点续跑——当前 phase 会重跑，如实记录）
timeout 200 python3 /tmp/sdc_inv_stage/su_run.py "systemctl restart sdc-excite-reproduce && sleep 5 && systemctl is-active sdc-excite-reproduce && tail -3 /home/sdc/sdc-excite-reproduce/driver.log"
```
- [ ] **Step 2: 冒烟验证**（等待 ≥3 个监控周期后）：
```bash
head -1 /home/sdc/sdc-excite-reproduce/monitor/monitor.csv | tr ',' '\n' | wc -l   # v3 列数（发现式 ~40+尾列）
tail -2 /home/sdc/sdc-excite-reproduce/monitor/monitor.csv | cut -c1-120          # 数值形态
ls /home/sdc/sdc-excite-reproduce/monitor/discrete_events.log && head -3 同文件    # INIT 行应已出现
tail -5 /home/sdc/sdc-excite-reproduce/monitor/alerts.log                          # 无误告警
python3 -m pytest tools/telemetry/tests/ -q                                          # 全套绿
```
Expected: v3 列集落盘、discrete_events.log 有 INIT 行、alerts 无假阳性、战役 phase 推进正常（driver.log）。
- [ ] **Step 3: Commit（文档）+ push** `scripts: M1b 部署文档（v3 列集/离散 diff/复测护栏说明）`
- [ ] **Step 4: 集成 PR**（API，合并留用户；标题 `feat: M1b monitor v3 + 驱动加固（v5 单元 4 + RCA 建议）`）

---

## 与 v5/RCA 的对照（自审用）

| 要求 | 落点 |
|---|---|
| 单元 4：BMC 单轮询重构 | 已是现状（:70 单次 SDR 共享），v3 保持 |
| 单元 4：发现式列集 monitor.csv v3 | T2 |
| 单元 4：离散态 diff → discrete_events.log | T1+T3 |
| 单元 4：EDAC/vmstat/NUMA 列 | T2 尾列 |
| 单元 4：UE 告警 | T2（不自动 PAUSE——v5 §8.6 决策） |
| 单元 4：collector 看门狗 | T2 |
| RCA 建议 1：复测内存护栏 | T5 |
| RCA 建议 2：control 孤儿清理 | T5 |
| RCA 建议 3：fail/crash 提取修复 | T4（根因：:141/:144 只匹配 fail） |
| RCA 建议 4：runtime 单位修正 | T4 注记（框架 YAML 格式不动——x86 共享） |
| 12 维矩阵 #5/#6（逐核温度/电压） | 硬件边界不变（per-socket 聚合，v5 §15.7-2） |
