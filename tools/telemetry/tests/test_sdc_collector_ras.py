import os, sys
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
