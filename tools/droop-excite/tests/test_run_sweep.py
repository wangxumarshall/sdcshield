#!/usr/bin/env python3
"""run_sweep 单元测试。"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import run_sweep  # noqa: E402

TH = dict(run_sweep.DEFAULT_THRESHOLDS)


def _mk_sysfs(root):
    for c, pkg in ((0, 0), (1, 0), (2, 1), (3, 1)):
        d = os.path.join(root, f"devices/system/cpu/cpu{c}/topology")
        os.makedirs(d)
        with open(os.path.join(d, "physical_package_id"), "w") as f:
            f.write(f"{pkg}\n")
    for pol, cpus in (("policy0", [0, 1]), ("policy1", [2, 3])):
        d = os.path.join(root, f"devices/system/cpu/cpufreq/{pol}")
        os.makedirs(d)
        with open(os.path.join(d, "related_cpus"), "w") as f:
            f.write(" ".join(map(str, cpus)) + "\n")
    return root


class TestTopology(unittest.TestCase):
    def test_probe_topology_fixture(self):
        with tempfile.TemporaryDirectory() as td:
            topo = run_sweep.probe_topology(_mk_sysfs(td))
        self.assertEqual(topo["ncores"], 4)
        self.assertEqual(topo["sockets"], {"0": [0, 1], "1": [2, 3]})
        self.assertEqual(topo["policies"], {"policy0": [0, 1], "policy1": [2, 3]})


class TestCores(unittest.TestCase):
    def test_mod_cores_same_and_cross(self):
        self.assertEqual(run_sweep.mod_cores(8, "same"), list(range(8)))
        self.assertEqual(run_sweep.mod_cores(8, "cross"),
                         [0, 1, 2, 3, 64, 65, 66, 67])

    def test_eigen_cores_excludes_reserved_and_mod(self):
        ec = run_sweep.eigen_cores([0, 1, 2, 3])
        self.assertNotIn(0, ec)
        self.assertNotIn(120, ec)
        self.assertEqual(len(ec), 128 - 8 - 4)


class TestGuardrails(unittest.TestCase):
    def test_temp_trips(self):
        r = run_sweep.evaluate_guardrails({"CPU1 Core Rem": "95.1"}, TH)
        self.assertIsNotNone(r)
        self.assertIn("CPU1 Core Rem", r)

    def test_voltage_trips(self):
        self.assertIsNotNone(
            run_sweep.evaluate_guardrails({"CPU2 N_VDDAVS": "0.799"}, TH))

    def test_psu_trips(self):
        self.assertIsNotNone(
            run_sweep.evaluate_guardrails({"Power": "700", "Power2": "700"}, TH))

    def test_safe(self):
        self.assertIsNone(run_sweep.evaluate_guardrails(
            {"CPU1 Core Rem": "80", "CPU2 Core Rem": "70",
             "CPU1 N_VDDAVS": "0.90", "CPU2 N_VDDAVS": "0.89",
             "Power": "500", "Power2": "480"}, TH))

    def test_missing_values_safe(self):
        self.assertIsNone(run_sweep.evaluate_guardrails({}, TH))


class TestBuilders(unittest.TestCase):
    PATHS = {"droop_mod": "DM", "sdcshield": "SS", "run_dir": "/tmp/r"}

    def test_build_mod_cmd_freq(self):
        step = {"id": "x", "dir": "/tmp/x", "mod": "droop_mod", "payload": "dep",
                "off_mode": "stall", "freq_hz": 1e8, "duty": 0.5,
                "duration": 60, "mod_cores": [0, 1]}
        cmd = run_sweep.build_mod_cmd(step, self.PATHS)
        self.assertEqual(cmd[:3], ["taskset", "-c", "0,1"])
        self.assertIn("--freq-hz", cmd)
        self.assertIn("DM", cmd)

    def test_build_mod_cmd_iters(self):
        step = {"id": "x", "dir": "/tmp/x", "mod": "droop_mod", "payload": "ilp2",
                "off_mode": "stall", "on_iters": 13, "off_iters": 13,
                "duration": 60, "mod_cores": [4]}
        cmd = run_sweep.build_mod_cmd(step, self.PATHS)
        self.assertIn("--on-iters", cmd)
        self.assertNotIn("--freq-hz", cmd)

    def test_build_mod_cmd_pvdit(self):
        step = {"id": "x", "dir": "/tmp/x", "mod": "power_virus_dit",
                "mod_cores": [0], "duration": 300}
        cmd = run_sweep.build_mod_cmd(step, self.PATHS)
        self.assertIn("power_virus_dit", cmd)
        self.assertIn("SS", cmd)

    def test_build_eigen_cmd(self):
        step = {"id": "x", "eigen_cores": [8, 9], "duration": 300}
        cmd = run_sweep.build_eigen_cmd(step, self.PATHS)
        self.assertEqual(cmd[:3], ["taskset", "-c", "8,9"])
        self.assertIn("-e", cmd)
        self.assertIn("eigen_svd_double", cmd[cmd.index("-e") + 1])


class TestPhases(unittest.TestCase):
    def test_steps_p0_shape(self):
        steps = run_sweep.steps_p0()
        self.assertEqual(len(steps), 4)
        self.assertIsNone(steps[0]["mod"])                    # 空载基线
        self.assertEqual(steps[3]["mod"], "power_virus_dit")  # 零代码冒烟
        self.assertTrue(steps[2]["eigen_cores"] or steps[2]["mod_cores"])

    def test_steps_p1_count(self):
        self.assertEqual(len(run_sweep.steps_p1()), 2 + 4 * 3 * 2)

    def test_steps_p3_dense_duty(self):
        steps = run_sweep.steps_p3(payload="dep")
        self.assertEqual(len(steps), 12 + 8)   # 12 频点 duty .5 + 4 密集点×2 额外 duty

    def test_dry_run_emits_commands(self):
        import io
        import contextlib
        import unittest.mock as mock
        with tempfile.TemporaryDirectory() as td:
            with mock.patch.object(run_sweep, "assert_topology", lambda: None):
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    rc = run_sweep.main(["--phase", "p2", "--dry-run",
                                         "--data-root", td,
                                         "--repo", "/nonexistent"])
        self.assertEqual(rc, 0)
        lines = [json.loads(x) for x in buf.getvalue().splitlines()
                 if x.startswith("{")]
        self.assertEqual(len(lines), 5)        # p2 = 5 个频点
        # -e 值是逗号连接的单个列表元素，须用子串匹配（SDD 裁定 11 修正）
        self.assertTrue(any("eigen_svd_double" in x for x in lines[0]["eigen"]))


if __name__ == "__main__":
    unittest.main()
