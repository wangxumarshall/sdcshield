import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector_percore as pc

STAT_A = "cpu0 100 0 100 1000 0 0 0 0 0\n"
STAT_B = "cpu0 100 0 100 1500 0 0 0 0 0\n"   # idle +500 拍 = 5s 全闲 → util 0%
STAT_C = "cpu0 400 0 100 1500 0 0 0 0 0\n"   # user +300 → util 300/(300+500)=37.5%

def test_util_delta():
    assert pc.util_pct(STAT_A, STAT_B) == [0.0]          # idle +500 → 全闲 0%
    u = pc.util_pct(STAT_A, STAT_C)                       # user 100→400(+300), idle 1000→1500(+500)
    assert abs(u[0] - 37.5) < 0.1                         # busy 300 / total 800

def test_bucket_100mhz():
    assert pc.freq_bucket_khz(2599999) == 2500000
    assert pc.freq_bucket_khz(2600000) == 2600000
