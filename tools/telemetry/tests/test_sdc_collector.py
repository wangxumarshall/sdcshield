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
