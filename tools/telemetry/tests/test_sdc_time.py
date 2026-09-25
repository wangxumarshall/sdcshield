import os, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_time

def test_quintet_fields_and_sanity():
    q = sdc_time.read_quintet()
    for k in ("realtime_ns", "monotonic_raw_ns", "uptime_ns", "cntvct", "cntfrq_hz"):
        assert isinstance(q[k], int) and q[k] > 0, k

def test_cntvct_rate_matches_wallclock():
    # 不假设 CNTVCT 纪元=开机（固件可设偏移）；只验证计数速率与墙钟一致
    a = sdc_time.read_quintet(); time.sleep(0.5); b = sdc_time.read_quintet()
    dt_wall = (b["realtime_ns"] - a["realtime_ns"]) / 1e9
    dt_cnt = (b["cntvct"] - a["cntvct"]) / a["cntfrq_hz"]
    assert abs(dt_cnt - dt_wall) < 0.1
    assert b["cntvct"] > a["cntvct"]

def test_interpolate_midpoint():
    a = {"realtime_ns": 0, "cntvct": 1000, "mapping_error_ns": 0}
    b = {"realtime_ns": 2000, "cntvct": 3000, "mapping_error_ns": 0}
    est, err = sdc_time.interpolate([a, b], 1000)
    assert est == 2000 and err >= 0
