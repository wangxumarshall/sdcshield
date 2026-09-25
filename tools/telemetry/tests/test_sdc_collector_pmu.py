import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector_pmu as pm

PERCORE_LINE = "0.500525610,S36-D0-C0,1,7976565,,cycles,502963210,100.00,,"
PERCORE_MUX = "1.500525610,S36-D0-C1,1,1234,,cycles,502963210,42.00,,"
PLAIN_LINE = "0.500525610,287719,,hisi_sccl1_ddrc0/flux_rd/,693440,100.00,,"

def test_parse_percore_line():
    ts, core, val, ev, pct = pm.parse_percore(PERCORE_LINE)
    assert (ts, core, val, ev, pct) == ("0.500525610", "S36-D0-C0", 7976565, "cycles", 100.0)

def test_is_degraded():
    assert pm.is_degraded(100.0) is False and pm.is_degraded(42.0) is True

def test_parse_plain_line():
    ts, val, ev, pct = pm.parse_plain(PLAIN_LINE)
    assert ev == "hisi_sccl1_ddrc0/flux_rd/" and pct == 100.0

def test_groups_shape():
    assert set(pm.GROUPS["core_base"]) <= {"cpu_cycles", "inst_retired", "inst_spec",
        "exe_stall_cycle", "stall_frontend", "stall_backend"}
    assert len(pm.GROUPS["l3c"]) == 4
