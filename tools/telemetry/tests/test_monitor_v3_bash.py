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
    idx = lines[0].strip().split(",").index("cpu1_core_rem")   # 占位落实：idx 取自表头
    assert len(lines) == 3                                  # header + 两次运行各 1 周期
    assert lines[1].split(",")[idx] == lines[2].split(",")[idx] == "45"
