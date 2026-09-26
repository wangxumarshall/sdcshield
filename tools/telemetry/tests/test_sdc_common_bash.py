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
