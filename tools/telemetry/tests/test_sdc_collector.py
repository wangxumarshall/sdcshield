import os, sys, csv
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector as sc

class EchoCollector(sc.SdcCollector):
    NAME, HEADER = "echo", ["ts", "value"]
    def collect_once(self):
        return [[self.now_iso(), "42"]]

def test_base_class_loop_and_selfmon(tmp_path):
    c = EchoCollector(period_s=0.2, out_dir=str(tmp_path), selfmon_path=str(tmp_path / "self.csv"),
                      max_cycles=2)
    c.run()
    rows = list(csv.reader(open(tmp_path / "echo.csv")))
    assert rows[0] == ["ts", "value"] and len(rows) == 3          # header + 2 行
    selfmon = list(csv.DictReader(open(tmp_path / "self.csv")))
    assert len(selfmon) == 2
    assert selfmon[-1]["samples_total"] == "2" and selfmon[-1]["samples_dropped"] == "0"

def test_dispatch_unknown_raises():
    import pytest
    with pytest.raises(SystemExit):
        sc.main(["nosuch"])

def test_main_entry_dispatches_registered_submodule(tmp_path, monkeypatch):
    # C1 回归：脚本作为 __main__ 运行时，子模块的 register 必须到达分派表。
    # stub 首个周期后 SystemExit(0) 自然退出（BaseException 不被基类 except Exception 吞）。
    # 隔离运行（对裁定测试的两处必要修正）：
    #  (a) main() 只 import 固定三名 percore/pmu/ras，桩必须占用其一，否则永远不被导入；
    #  (b) 入口按当前字节复制到 tmp_path 执行——脚本目录优先于 PYTHONPATH，若直接跑仓库
    #      脚本，Task 4-6 落地真 sdc_collector_percore.py 后会遮蔽桩，本测试将来误报。
    import os, shutil, subprocess, sys
    shutil.copy(os.path.join(os.path.dirname(os.path.dirname(__file__)), "sdc_collector.py"),
                tmp_path / "sdc_collector.py")
    stub = tmp_path / "sdc_collector_percore.py"
    stub.write_text(
        "from sdc_collector import SdcCollector, register\n"
        "@register\n"
        "class T(SdcCollector):\n"
        "    NAME, HEADER = 'stubtest', ['ts']\n"
        "    DEFAULT_PERIOD_S = 0.01\n"
        "    def collect_once(self):\n"
        "        raise SystemExit(0)\n")
    env = dict(os.environ, PYTHONPATH=str(tmp_path),
               SDC_EXCITE_REPRODUCE_DIR=str(tmp_path / "data"))
    p = subprocess.run([sys.executable, str(tmp_path / "sdc_collector.py"),
                        "stubtest", "--period-s", "0.01"],
                       capture_output=True, text=True, cwd=str(tmp_path), env=env, timeout=30)
    assert p.returncode == 0, f"exit={p.returncode} stderr={p.stderr}"

def test_sigterm_prompt_exit_long_period(tmp_path):
    # 长周期下 SIGTERM 应 ~1-2s 退出（分片睡），而非睡满 60s 撞 TimeoutStopSec
    import os, subprocess, sys
    here = os.path.dirname(os.path.dirname(__file__))
    code = ("import sys, time, threading, os\n"
            f"sys.path.insert(0, {here!r})\n"
            "import sdc_collector as sc\n"
            "class Long(sc.SdcCollector):\n"
            "    NAME, HEADER = 'longsleep', ['ts']\n"
            "    def collect_once(self):\n"
            "        return [[self.now_iso()]]\n"
            f"c = Long(period_s=60, out_dir={str(tmp_path)!r}, selfmon_path={str(tmp_path / 'self.csv')!r})\n"
            "threading.Timer(1.0, lambda: os.kill(os.getpid(), 15)).start()\n"
            "t0 = time.monotonic(); c.run(); print(int(time.monotonic() - t0))\n")
    p = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=30)
    assert p.returncode == 0, p.stderr
    assert int(p.stdout.strip()) <= 3    # 修复前睡满 60s（本测试 timeout=30 会先杀掉它）
