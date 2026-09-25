import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_legacy_adapter as ada

FIX = os.path.join(os.path.dirname(__file__), "fixtures")

def test_parse_ledger_all_rows():
    rows = ada.parse_ledger(os.path.join(FIX, "ledger.csv"))
    assert len(rows) == 8
    mesh = [r for r in rows if "mesh_upi" in r.get("test", "")]
    assert len(mesh) == 1 and mesh[0]["seed"] == "AES:ebcafcfb"

def test_convert_validates_against_schema():
    import sdc_event
    evs = ada.convert_ledger(os.path.join(FIX, "ledger.csv"),
                             "20260924T002215Z-taishan2280-full-6c76ea63a7a9")
    assert len(evs) == 8
    for ev in evs:
        assert sdc_event.validate_event(ev) == [], ev.get("event_id")

def test_mesh_event_classification_from_note():
    evs = ada.convert_ledger(os.path.join(FIX, "ledger.csv"), "c0")
    mesh = [e for e in evs if "mesh_upi" in (e.get("test") or {}).get("id", "")]
    assert mesh[0]["classification"]["status"] == "resolved"
    assert mesh[0]["classification"]["primary"] == "test_race"

def test_parse_yaml_fail_block():
    blocks = ada.parse_yaml_fail_blocks(open(os.path.join(FIX, "stdout_summary.txt")).read())
    assert len(blocks) >= 2                      # memcpy_rewr + cachebounce
    assert blocks[0]["test"] == "memcpy_rewr" and blocks[0]["result"] == "crash"
    assert "seed" in blocks[0]["fail"]           # fail 行含 seed（连字符键 cpu-mask/time-to-fail 也要能解析）
    assert "cpu-mask" in blocks[0]["fail"]
