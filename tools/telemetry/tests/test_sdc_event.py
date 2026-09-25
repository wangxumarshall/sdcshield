import json, os, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_event

VALID = {
    "schema_version": "1.0", "event_id": "0199-test-0001",
    "event_type": "sdc_mismatch", "severity": "red", "confidence": "observed",
    "sdc-excite-reproduce_id": "20260924T002215Z-taishan2280-full-6c76ea63a7a9",
    "run_id": "0199-run-0001",
    "test": {"id": "mesh_upi_sse_asymm_distrib_int", "family": "mesh",
             "workload_type": "integer", "instruction_width_bits": 128,
             "seed": "AES:ebcafcfb", "iteration": 1, "retry": False,
             "knobs": {}},
    "result": {"verdict": "FAIL", "t_start_realtime_ns": 1, "t_end_realtime_ns": 2,
               "duration_ns": 1, "cpu_cycles": 0, "cntvct_delta": 0},
    "location": {"logical_cpu": 0, "core_id": 0, "socket_id": 0,
                 "die_id": 0, "cluster_id": 138, "numa_node": 0},
    "time": {"realtime_ns": 1, "monotonic_raw_ns": 1, "uptime_ns": 1,
             "cntvct": 1, "cntfrq_hz": 100000000, "bmc_time_raw": None,
             "mapping_error_ns": 0},
    "mismatch": {"type": "uint8", "byte_offset": 736, "suboffset": 0,
                 "lane": 92, "actual_hex": "0xff", "expected_hex": "0x7f",
                 "xor_mask_hex": "0x80", "popcount": 1},
    "environment": {"temperature_c": None, "frequency_khz": None,
                    "voltage_v": None, "package_power_w": None,
                    "pmu_window_id": None},
    "artifacts": [],
    "classification": {"primary": "candidate_hardware_sdc",
                       "alternatives": ["test_race"], "status": "open"},
}

def test_valid_event_passes():
    assert sdc_event.validate_event(VALID) == []

def test_missing_required_field_fails():
    bad = json.loads(json.dumps(VALID)); del bad["time"]
    assert any("time" in e for e in sdc_event.validate_event(bad))

def test_bad_enum_fails():
    bad = json.loads(json.dumps(VALID)); bad["severity"] = "purple"
    assert any("severity" in e for e in sdc_event.validate_event(bad))

def test_bad_hex_fails():
    bad = json.loads(json.dumps(VALID)); bad["mismatch"]["xor_mask_hex"] = "zz"
    assert any("xor_mask_hex" in e for e in sdc_event.validate_event(bad))

def test_make_event_defaults():
    ev = sdc_event.make_event(event_type="note", event_id="e1", severity="green",
                              confidence="inferred",
                              **{"sdc-excite-reproduce_id": "c1", "run_id": "r1"})
    assert ev["schema_version"] == "1.0" and ev["artifacts"] == []
    assert sdc_event.validate_event(ev) == []

def test_cli_validate_jsonl_stdin():
    """CLI 子进程回归：validate-jsonl 从 stdin 读（曾把 TextIOWrapper 传给 open() 崩溃）。"""
    cli = os.path.join(os.path.dirname(__file__), "..", "sdc_event.py")
    good = "\n".join(json.dumps(VALID) for _ in range(2)) + "\n"
    r = subprocess.run([sys.executable, cli, "validate-jsonl"],
                       input=good, capture_output=True, text=True)
    assert r.returncode == 0, f"rc={r.returncode} stderr={r.stderr}"
    bad = good + "{not-json\n"
    r2 = subprocess.run([sys.executable, cli, "validate-jsonl"],
                        input=bad, capture_output=True, text=True)
    assert r2.returncode != 0 and r2.stderr != ""
