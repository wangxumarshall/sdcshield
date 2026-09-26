import os, subprocess, sys

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")

# 真实实测 ipmitool sdr list 格式（评审根修 2026-09-26：读数带单位——抽自
# docs/superpowers/inventory/…/03_ipmitool_sdr_list.txt 的代表性行）
SDR_FIXTURE = """CPU1 Core Rem       | 65 degrees C      | ok
CPU2 Core Rem       | 48 degrees C      | ok
CPU1 VDDAVS         | 0.88 Volts        | ok
CPU1 Prochot        | 0x00              | ok
FAN2 Speed          | 11475 RPM         | ok
Power               | 276 Watts         | ok
SEL                 | 0x00              | ok
Inlet Temp          | no reading        | ns
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
    assert "cpu1_core_rem|65" in lines and "cpu2_core_rem|48" in lines
    assert "cpu1_vddavs|0.88" in lines and "fan2_speed|11475" in lines
    assert "power|276" in lines
    assert not any(l.startswith("cpu1_prochot") for l in lines)   # 离散量不入
    assert not any(l.startswith("sel|") for l in lines)
    assert not any(l.startswith("inlet_temp") for l in lines)     # no reading 不入（skip）

def test_sdr_discrete_map():
    p = _bash("sdr_discrete_map", SDR_FIXTURE)
    lines = p.stdout.strip().splitlines()
    assert "cpu1_prochot|0x00" in lines
    assert not any(l.startswith("cpu1_core_rem") for l in lines)  # 带单位读数归模拟量
    assert not any(l.startswith("inlet_temp") for l in lines)     # no reading 不入（skip）

def test_sdr_unit_reading_analog():
    # 评审根修：带单位读数（"276 Watts"/"20.52 Amps"）→ 模拟量，值提取首个数值
    sdr = "Power               | 276 Watts         | ok\nPS2 IOut     | 20.52 Amps       | ok\n"
    lines = _bash("sdr_analog_columns", sdr).stdout.strip().splitlines()
    assert "power|276" in lines and "ps2_iout|20.52" in lines
    assert not any(l.startswith("power|") for l in _bash("sdr_discrete_map", sdr).stdout.splitlines())

def test_sdr_no_reading_skipped():
    # no reading/Not Readable/纯文字无数值 → 两边都不入（skip，缺失不伪造）
    sdr = "Inlet Temp         | no reading        | ns\nFAN1 Status  | Not Readable | ns\nSystem Notice | 0x00    | ok\n"
    a = _bash("sdr_analog_columns", sdr).stdout.strip().splitlines()
    d = _bash("sdr_discrete_map", sdr).stdout.strip().splitlines()
    assert not any(l.startswith(("inlet_temp", "fan1_status")) for l in a)
    assert not any(l.startswith(("inlet_temp", "fan1_status")) for l in d)
    assert "system_notice|0x00" in d                            # 0x 离散照常入

def test_real_inventory_classification():
    # 真实盘点文件整体分类（评审指令的实跑口径）。实测构成：150 行 = 103 条 0x（离散）
    # + 40 条带单位数值（模拟量）+ 7 条 no reading/Not Readable（skip）——评审指令中
    # "103 模拟量"系把离散侧计数误标（其自身"150 条含 47 非 0x"算术：150-47=103 为 0x
    # 侧），真机模拟量为 40，断言按实测真值。
    inv = os.path.join(os.path.dirname(__file__), "..", "..", "..", "docs", "superpowers",
                       "inventory", "2102312YVY10M6000038-2026-09-23", "03_ipmitool_sdr_list.txt")
    p = _bash("sdr_analog_columns", open(inv).read())
    cols = [l for l in p.stdout.strip().splitlines() if l]
    assert len(cols) == 40
    assert any(l.startswith("power|276") for l in cols)          # 276 Watts → power|276
    p = _bash("sdr_discrete_map", open(inv).read())
    dlines = [l for l in p.stdout.strip().splitlines() if l]
    assert len(dlines) == 103
    assert all(l.split("|", 1)[1].startswith("0x") for l in dlines)   # 离散量全部 0x 状态码
    assert not any(l.startswith("inlet_temp|") for l in cols + dlines)  # no reading 两边不入

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

def test_discrete_diff_empty_reading_no_false_init(tmp_path):
    # 空读数传感器（'CPU1 Absent |  | ok' → map 行 'cpu1_absent|'）不参与 diff——
    # 防"每周期 INIT"误报（T1 评审传入项 2b，随 monitor v3 Task 2 修）
    sdr = "CPU1 Absent        |  | ok\nCPU1 Prochot        | 0x01    | ok\n"
    new = tmp_path / "new.map"
    p = _bash(f"sdr_discrete_map > {new}", sdr)
    assert p.returncode == 0
    p = _bash(f"discrete_diff {tmp_path / 'old_absent.map'} {new}")
    assert "cpu1_absent" not in p.stdout                    # 空读数：不产出 INIT 行
    assert "cpu1_prochot: INIT → 0x01" in p.stdout          # 有读数的照常 INIT
    assert p.returncode == 1
