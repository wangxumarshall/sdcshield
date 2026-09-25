import os, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector_ras as ras

def test_keywords_filter():
    line_bad = "hns3 0000:7d:00.0: ... error"
    line_hit = "EDAC MC0: 1 Corrected error"
    line_stall = "rcu: INFO: rcu_sched self-detected stall on CPU"
    assert ras.ras_keyword(line_bad) is False          # hns3 噪声排除
    assert ras.ras_keyword(line_stall) is False        # rcu-stall 噪声排除（专属清单另计）
    assert ras.ras_keyword(line_hit) is True

def test_spurious_count():
    text = "a spurious translation fault b\nnothing\nspurious translation fault c\n"
    assert ras.count_spurious(text) == 2

def test_perdimm_columns_and_journalctl_robustness(tmp_path, monkeypatch):
    # 假 EDAC 树：mc0 下 dimm0(ce=3) 与 dimm1(全 0)，mc 级 ce=3/ue=0
    mc0 = tmp_path / "mc" / "mc0"
    for d, ce, ue in (("dimm0", "3", "0"), ("dimm1", "0", "0")):
        dd = mc0 / d
        dd.mkdir(parents=True)
        (dd / "dimm_ce_count").write_text(ce)
        (dd / "dimm_ue_count").write_text(ue)
    (mc0 / "ce_count").write_text("3")
    (mc0 / "ue_count").write_text("0")
    monkeypatch.setattr(ras, "EDAC_ROOT", str(tmp_path))
    c = ras.RasCollector(period_s=60.0, out_dir=str(tmp_path / "out"))
    assert c.HEADER == ["ts", "mc", "ce", "ue", "dimm_mc0_0_ce", "dimm_mc0_0_ue",
                        "dimm_mc0_1_ce", "dimm_mc0_1_ue"]         # 2 dimm × ce/ue = 4 dimm 列
    rows = c.collect_once()
    assert len(rows) == 1 and rows[0][1] == "mc0"
    assert rows[0][c.HEADER.index("ce")] == "3"                    # mc 行 ce=3
    assert rows[0][c.HEADER.index("dimm_mc0_0_ce")] == "3"         # per-DIMM 当前值
    assert rows[0][c.HEADER.index("dimm_mc0_1_ue")] == "0"
    assert len(rows[0]) == len(c.HEADER)                           # 宽表行宽一致
    # journalctl 健壮化：rc!=0 → journal_watch.log 告警（含 rc）且 _last_since 不推进
    fake = subprocess.CompletedProcess([], 1, stdout="", stderr="boom")
    monkeypatch.setattr(ras.subprocess, "run", lambda *a, **k: fake)
    since0 = c._last_since
    assert c.collect_once() and c._last_since == since0            # 失败不推进（EDAC 行照常产出）
    assert "journalctl rc=1" in (tmp_path / "out" / "journal_watch.log").read_text()

