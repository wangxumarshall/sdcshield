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
