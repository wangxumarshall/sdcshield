#!/usr/bin/env python3
"""analyze 单元测试。"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import analyze  # noqa: E402

HEADER = ("ts,CPU1 N_VDDAVS,CPU2 N_VDDAVS,CPU1 VDDAVS,CPU2 VDDAVS,"
          "Power,Power2,PS2 IOut,CPU1 Core Rem,CPU2 Core Rem")


def _mk_step(root, sid, sdc=False):
    d = os.path.join(root, "steps", sid)
    os.makedirs(d)
    rows = [HEADER]
    def emit(ts, v, p):
        # p 表示整机 PSU 总功率；双 PSU 均分（analyze 按 Power+Power2 求和）
        rows.append(f"{ts},{v},{v},{v},{v},{p / 2},{p / 2},10,60,50")
    emit(0.0, 0.91, 300)                            # pre（0-10s）
    emit(20.0, 0.89, 500)                           # dwell（10-40s）
    emit(30.0, 0.89, 400)
    emit(50.0, 0.91, 300)                           # post
    with open(os.path.join(d, "bmc.csv"), "w") as f:
        f.write("\n".join(rows) + "\n")
    summary = {"id": sid, "timeline": {"pre_start": 0, "dwell_start": 10,
                                       "dwell_end": 40, "post_end": 60},
               "payload": "dep", "freq_hz": 0.5, "duty": 0.5,
               "mod_cores": [0], "note": "", "sdc_suspect": sdc,
               "aborted": None, "ras_delta": 0, "sel_delta": 0,
               "eigen_rc": 0, "mod_rc": 0}
    with open(os.path.join(d, "summary.json"), "w") as f:
        json.dump(summary, f)
    return d


class TestStepMetrics(unittest.TestCase):
    def test_metrics_math(self):
        with tempfile.TemporaryDirectory() as td:
            d = _mk_step(td, "s1")
            m = analyze.step_metrics(d)
        self.assertEqual(m["i_p2p_w"], 100.0)          # 500-400
        self.assertEqual(m["v_sag_sock0_mv"], 20.0)    # (0.91-0.89)*1000
        self.assertEqual(m["n_samples"], 2)
        self.assertEqual(m["t_max_c"], 60.0)

    def test_rank_tables_marks_events(self):
        with tempfile.TemporaryDirectory() as td:
            _mk_step(td, "s_ok")
            _mk_step(td, "s_bad", sdc=True)
            md = analyze.rank_tables([td])
        self.assertIn("s_bad", md)
        self.assertIn("s_ok", md)
        self.assertIn("| s_bad | True |", md)          # 事件表标记 SDC 疑似
        with tempfile.TemporaryDirectory() as td2:
            _mk_step(td2, "s_neg")
            md2 = analyze.rank_tables([td2])
        self.assertIn("阴性结果", md2)                 # 无事件→如实记阴性

    def test_baseline_slice_uses_idle_tail_ruling13(self):
        # 裁定 13：probe 突发防污染——基线必须取 pre 窗口的空闲尾部
        # [max(pre_start, dwell_start-15), dwell_start]，而非整窗均值：
        # refine probe 突发落在窗口头部（SDD T3 审查发现⑤）。
        # 本夹具：尾部 [15,30] 只含 ts=25（0.91）→ V_sag=20.0；
        # 若误用整窗均值 (0.95+0.91)/2=0.93 → V_sag=40.0，断言即失败。
        with tempfile.TemporaryDirectory() as td:
            d = os.path.join(td, "steps", "s_tail")
            os.makedirs(d)
            rows = [HEADER]

            def emit(ts, v):
                rows.append(f"{ts},{v},{v},{v},{v},150,150,10,60,50")

            emit(5.0, 0.95)     # pre 窗口头部：refine probe 突发（污染源）
            emit(25.0, 0.91)    # pre 窗口空闲尾部（基线）
            emit(40.0, 0.89)    # dwell
            emit(50.0, 0.89)    # dwell
            with open(os.path.join(d, "bmc.csv"), "w") as f:
                f.write("\n".join(rows) + "\n")
            summary = {"id": "s_tail", "timeline": {"pre_start": 0,
                                                    "dwell_start": 30,
                                                    "dwell_end": 60,
                                                    "post_end": 90},
                       "payload": "dep", "freq_hz": 0.5, "duty": 0.5,
                       "mod_cores": [0], "note": "", "sdc_suspect": False,
                       "aborted": None, "ras_delta": 0, "sel_delta": 0,
                       "eigen_rc": 0, "mod_rc": 0}
            with open(os.path.join(d, "summary.json"), "w") as f:
                json.dump(summary, f)
            m = analyze.step_metrics(d)
        self.assertEqual(m["v_sag_sock0_mv"], 20.0)   # 尾部 0.91，非整窗 0.93
        self.assertEqual(m["v_sag_sock1_mv"], 20.0)


if __name__ == "__main__":
    unittest.main()
