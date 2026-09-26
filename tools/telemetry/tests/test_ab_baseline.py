import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import ab_baseline

FAKE_YAML = """tests:
- test: zstd19
  result: pass
  threads:
  - thread: main
  - thread: 0
    loop-count: 1200
  - thread: 1
    loop-count: 1180
"""

def test_parse_loop_total():
    assert ab_baseline.parse_loop_total(FAKE_YAML) == 2380

def test_parse_loop_total_missing_is_zero():
    assert ab_baseline.parse_loop_total("tests:\n- test: zstd19\n  result: pass\n") == 0

def test_zero_loop_total_rejected(tmp_path, monkeypatch):
    # T2⑤ 零值防御：run_once 被（退化或替换后）返回 0 → main 必须拒绝写锚点文件（防假 PASS）
    import pytest
    out = tmp_path / "ab.json"
    monkeypatch.setattr(ab_baseline, "run_once", lambda *a, **k: 0)
    monkeypatch.setattr(sys, "argv", ["ab_baseline.py", "--out", str(out)])
    with pytest.raises(RuntimeError):
        ab_baseline.main()
    assert not out.exists()                              # 锚点文件不得落盘
