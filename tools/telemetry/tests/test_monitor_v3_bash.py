import csv, os, subprocess, tempfile

# 约定（T2 评审传入）：MON_SELFTEST=1 只在周期末提前退出，联锁段照跑——因此所有 SDR
# fixture 温度必须良性（<88°C：不触发 20s 快采样；更不可触发 95°C set_pause 与
# 100°C/连续 2 采样 KILL 路径的 pkill）。本文件 fixture 最高 47°C，安全。

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
    idx = lines[0].strip().split(",").index("cpu1_core_rem")   # 占位落实：idx 取自表头
    assert len(lines) == 3                                  # header + 两次运行各 1 周期
    assert lines[1].split(",")[idx] == lines[2].split(",")[idx] == "45"

def test_v3_numa_memavail_chain(tmp_path):
    # T2 裁定传入：本机 node meminfo 无 MemAvailable 行 → 取值链 MemAvailable → MemFree
    # 近似（列名保持 numa_memavail_kb）。断言放宽：节点在 → 列非空；节点缺失 → 空列。
    # 用真实 /sys 跑（本机 node0-3 均在，node1 有 meminfo → numa1 列非空）。
    run_monitor_once(str(tmp_path), SDR)
    csvp = os.path.join(str(tmp_path), "data", "monitor", "monitor.csv")
    rows = list(csv.reader(open(csvp)))
    hdr = rows[0]
    for n in range(4):
        v = rows[1][hdr.index(f"numa{n}_memavail_kb")]
        if os.path.exists(f"/sys/devices/system/node/node{n}/meminfo"):
            assert v != "", f"node{n} 在但 numa{n}_memavail_kb 为空（取值链均未命中）"
        else:
            assert v == "", f"node{n} 缺失但 numa{n}_memavail_kb 非空"
    if os.path.exists("/sys/devices/system/node/node1/meminfo"):   # 本机 node1 有 meminfo
        assert rows[1][hdr.index("numa1_memavail_kb")] != ""

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
