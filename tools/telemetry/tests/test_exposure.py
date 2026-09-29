"""exposure.json 暴露量生产端测试（M5 Task 1，v5 §9.4 有效分母——M4 移交项）。

口径（brief + 实机格式核定 2026-09-27）：
  生产端   sdc_monitor.sh 周期采样段 condition_10m 块（10min 对齐）追加 write_exposure
           ——聚合 driver.log **今日**日汇总行（loop-count 累计 + cycle）+ online 核数
           积分，向 spool/exposure.json **追加** JSONL 快照行（与 events.jsonl 同风格）。
  核时积分 每 10min 快照 += 间隔秒 × online 核数 / 3600；首启无锚点按名义 600s 计；
           状态存 monitor/exposure_state.json（跨重启续积分，跨日归零）。
  测试钩子 MON_ONLINE_FILE=非空文件 → online 集从该文件读（生产不设 →
           /sys/devices/system/cpu/online）——与 MON_SDR_FILE 同模式。
  降级     日汇总行不含 loop-count（as-built 驱动现况）或当日尚无日汇总行 →
           valid_iterations_today=null + source 注记 "unavailable"——不伪造 0。

fixture 真实形态：driver.log 行风格逐字取自 ~/sdc-excite-reproduce/driver.log
（2026-09-27 实机）。T2 起 daily_summary() 在 driver.log 日汇总行补记
「loop-count 日累计: N（YAML 文件数 M）」token（N=今日 logs/YYYYMMDD/ 逐文件
'loop-count:' 行数之和）→ valid_iterations_today 得真值；旧档/as-built 的日汇总行
（如「日汇总完成（历史 YAML 已压缩）」）无该 token → 如实降级 null，报告侧保留
"不可得"路径。monitor 解析兼容 `loop-count=N`（旧档）与 `loop-count 日累计: N`（T2）。

消费接线：sdc_report.py 读 spool/exposure.json **最后一个有效快照**的
valid_iterations_today / core_hours_today → rate_table 分母（缺/坏/null → 不可得）。
"""
import datetime
import json
import os
import subprocess
import sys
import time

MON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                   "scripts", "sdc-excite-reproduce", "sdc_monitor.sh")
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..",
                                "tools", "analysis"))
import sdc_report as rep

TODAY = datetime.date.today().isoformat()          # 与 monitor 内 date +%F 同源
TODAY_COMPACT = TODAY.replace("-", "")

# 良性 SDR（<88°C：不触发快采样/联锁——test_monitor_v3_bash.py 同约定）
SDR = "CPU1 Core Rem       | 65 degrees C      | ok\nCPU1 Prochot        | 0x00              | ok\n"

# ---- driver.log fixture（行风格 = 实机 2026-09-27 逐字）----
DRIVER_LOG_WITH_LOOPCOUNT = f"""\
[{TODAY} 00:17:06] 战役启动 mode=full bin=/home/sdc/wangxu/sdcshield/builddir/sdcshield cycle起点=7 NPROC=128 热联锁=95C(监控侧) mdim全核上限=1024
[{TODAY} 07:30:00] 日汇总完成：cycle=8 loop-count=123456（历史 YAML 已压缩）
[{TODAY} 09:32:34] L4 深驻留：6 个对象 × 13174s（关 fracturing 固定模式）
"""
# as-built 真实形态：日汇总行在，但无 loop-count 累计（daily_summary 不产此数）
DRIVER_LOG_AS_BUILT = f"""\
[{TODAY} 03:59:56] 日汇总完成（历史 YAML 已压缩）
[{TODAY} 09:32:34] L4 深驻留：6 个对象 × 13174s（关 fracturing 固定模式）
"""
# 当日尚无日汇总行（日汇总在周期尾写；凌晨/上午的常态）
DRIVER_LOG_NO_SUMMARY = f"""\
[{TODAY} 00:17:09] [cycle 8] 冷机首轮（L1 代表集）。基线温度: 35,72
[{TODAY} 00:47:21] [cycle 8] L2 谱系扫档（复用 run_sdc_spectrum.sh + ipsec/memcpy 扩展）
"""
# T2：daily_summary() 补记 token 后的 driver.log 日汇总行真形（`loop-count 日累计: N`）
DRIVER_LOG_DAILY_ACCUMULATE = f"""\
[{TODAY} 00:17:06] 战役启动 mode=full bin=/home/sdc/wangxu/sdcshield/builddir/sdcshield cycle起点=7 NPROC=128 热联锁=95C(监控侧) mdim全核上限=1024
[{TODAY} 07:30:00] 日汇总完成 loop-count 日累计: 98765（YAML 文件数 42）（历史 YAML 已压缩）
[{TODAY} 09:32:34] L4 深驻留：6 个对象 × 13174s（关 fracturing 固定模式）
"""
# 日汇总行在、含 cycle 但无 loop-count token（旧档/as-built 混合；cycle 仍应可解析）
DRIVER_LOG_SUMMARY_WITHOUT_TOKEN = f"""\
[{TODAY} 07:30:00] 日汇总完成：cycle=9（历史 YAML 已压缩）
"""


def run_monitor(tmp, driver_log, online="0-3", state=None, sdr=SDR):
    """隔离数据根跑 monitor 一周期（MON_SELFTEST）→ subprocess 结果。

    tmp 下铺：data/driver.log（fixture）、data/logs/<YYYYMMDD>/（2 yaml + 1 gz）、
    online.txt（固定 online 集）、可选 data/monitor/exposure_state.json（state dict）。
    """
    data = os.path.join(str(tmp), "data")
    os.makedirs(os.path.join(data, "logs", TODAY_COMPACT), exist_ok=True)
    os.makedirs(os.path.join(data, "monitor"), exist_ok=True)
    open(os.path.join(data, "driver.log"), "w").write(driver_log)
    for i in (1, 2):
        open(os.path.join(data, "logs", TODAY_COMPACT, f"0{i}0000-l2_x_c8.yaml"), "w").write(
            "tests:\n- test: x\n  result: pass\n")
    open(os.path.join(data, "logs", TODAY_COMPACT, "030000-l2_y_c8.yaml.gz"), "wb").write(b"\x1f\x8b")
    if state is not None:
        open(os.path.join(data, "monitor", "exposure_state.json"), "w").write(json.dumps(state))
    sdr_f = os.path.join(str(tmp), "sdr.txt"); open(sdr_f, "w").write(sdr)
    onl_f = os.path.join(str(tmp), "online.txt"); open(onl_f, "w").write(online + "\n")
    env = dict(os.environ, MON_SELFTEST="1", MON_SDR_FILE=sdr_f, MON_ONLINE_FILE=onl_f,
               SDC_EXCITE_REPRODUCE_DIR=data)
    env.pop("SDC_ROOT_PW", None)
    return subprocess.run(["bash", MON], env=env, capture_output=True, text=True, timeout=60)


def _snaps(tmp):
    p = os.path.join(str(tmp), "data", "spool", "exposure.json")
    lines = [l for l in open(p).read().splitlines() if l.strip()]
    return [json.loads(l) for l in lines]      # 每行必须独立可解析（JSONL）


def test_snapshot_fields_from_driver_summary_and_online(tmp_path):
    # 含 loop-count 日汇总行 + 固定 online 集（0-3 = 4 核）→ 字段断言
    p = run_monitor(tmp_path, DRIVER_LOG_WITH_LOOPCOUNT)
    assert p.returncode == 0, p.stderr
    snaps = _snaps(tmp_path)
    assert len(snaps) == 1
    s = snaps[0]
    assert s["valid_iterations_today"] == 123456          # driver 日汇总 loop-count 累计
    assert s["cycle"] == 8                                # 日汇总行 cycle 信息
    assert s["online_cpus"] == 4
    assert abs(s["core_hours_today"] - 600 * 4 / 3600) < 1e-9   # 首启名义 600s × 4 核
    assert s["source_files"]["yaml_today"] == 2
    assert s["source_files"]["yaml_gz_today"] == 1
    assert s["date"] == TODAY and s["ts"].startswith(TODAY)
    assert "日汇总" in s["source"] and "unavailable" not in s["source"]


def test_null_when_summary_line_lacks_loopcount(tmp_path):
    # as-built 真实形态：日汇总行在但无 loop-count → null + unavailable 注记（不伪造 0）
    run_monitor(tmp_path, DRIVER_LOG_AS_BUILT)
    s = _snaps(tmp_path)[0]
    assert s["valid_iterations_today"] is None
    assert s["cycle"] is None
    assert s["source"].startswith("unavailable")
    # core_hours 不受牵连：online 积分照常
    assert abs(s["core_hours_today"] - 600 * 4 / 3600) < 1e-9


def test_null_when_no_summary_line_today(tmp_path):
    # 当日尚无日汇总行（周期尾才写）→ 同 null + 注记
    run_monitor(tmp_path, DRIVER_LOG_NO_SUMMARY)
    s = _snaps(tmp_path)[0]
    assert s["valid_iterations_today"] is None
    assert s["source"].startswith("unavailable")
    assert s["core_hours_today"] is not None


def test_valid_iterations_from_loopcount_daily_accumulate_token(tmp_path):
    # T2：driver.log 日汇总行含 `loop-count 日累计: N` → valid_iterations_today = N（真值）
    run_monitor(tmp_path, DRIVER_LOG_DAILY_ACCUMULATE)
    s = _snaps(tmp_path)[0]
    assert s["valid_iterations_today"] == 98765
    assert "unavailable" not in s["source"]             # 有两口径真值 → 非降级注记
    assert s["source"] == "driver.log日汇总loop-count + online核数积分"
    assert s["core_hours_today"] is not None            # 核时口径不受牵连


def test_null_when_summary_line_lacks_daily_accumulate_token(tmp_path):
    # 日汇总行在、含 cycle 但无 loop-count token → valid_iterations 降级 null；
    # cycle 仍独立解析（token 缺失不牵连其它字段）
    run_monitor(tmp_path, DRIVER_LOG_SUMMARY_WITHOUT_TOKEN)
    s = _snaps(tmp_path)[0]
    assert s["valid_iterations_today"] is None
    assert s["cycle"] == 9
    assert s["source"].startswith("unavailable")


def test_jsonl_append_and_core_hours_accumulate(tmp_path):
    # 两次运行（重启语义）→ 追加两行（不覆盖），核时在锚点上续积分
    run_monitor(tmp_path, DRIVER_LOG_WITH_LOOPCOUNT)
    run_monitor(tmp_path, DRIVER_LOG_WITH_LOOPCOUNT)
    snaps = _snaps(tmp_path)
    assert len(snaps) == 2                                # JSONL 追加，与 events.jsonl 同风格
    assert snaps[1]["valid_iterations_today"] == 123456
    assert snaps[1]["core_hours_today"] > snaps[0]["core_hours_today"]     # 实际间隔 > 0
    assert snaps[1]["core_hours_today"] - snaps[0]["core_hours_today"] < 0.5   # 且为秒级间隔


def test_core_hours_reset_on_date_rollover(tmp_path):
    # 状态 date=昨日 → 今日从 0 重新积分（不把昨日累计带入）
    yday = (datetime.date.today() - datetime.timedelta(days=1)).isoformat()
    st = {"date": yday, "core_hours": 99.5, "last_epoch": time.time() - 600}
    run_monitor(tmp_path, DRIVER_LOG_WITH_LOOPCOUNT, state=st)
    s = _snaps(tmp_path)[0]
    assert abs(s["core_hours_today"] - 600 * 4 / 3600) < 0.01    # ≈600s×4 核，非 99.5+


# ---- M4 rate_table 消费接线（sdc_report.py 读最后一个有效快照）----

def _report_root(base, exposure_text):
    root = os.path.join(str(base), "rep_root")
    os.makedirs(os.path.join(root, "spool"), exist_ok=True)
    open(os.path.join(root, "spool", "exposure.json"), "w").write(exposure_text)
    return root


def test_report_consumes_last_valid_snapshot(tmp_path):
    # 坏行容忍 + 最后有效快照生效：n=2000 / h=42.7 进 rate_table 分母
    root = _report_root(tmp_path / "a", "{ 坏行不炸\n" + json.dumps({
        "ts": TODAY + " 12:00:00", "date": TODAY, "valid_iterations_today": 2000,
        "core_hours_today": 42.7, "source": "driver.log日汇总loop-count + online核数积分"}) + "\n")
    md = rep.generate_report(root)
    assert "n=2000" in md and "h=42.7" in md            # 分母在场
    assert "n=不可得" not in md


def test_report_degrades_when_iterations_null(tmp_path):
    # 最后快照 valid_iterations_today=null → 迭代口径"不可得"降级；核时口径保留
    root = _report_root(tmp_path / "b", json.dumps({
        "ts": TODAY + " 12:00:00", "date": TODAY, "valid_iterations_today": None,
        "core_hours_today": 42.7,
        "source": "unavailable(valid_iterations_today:driver.log今日日汇总行无loop-count累计)"}) + "\n")
    md = rep.generate_report(root)
    assert "n=不可得" in md                             # 迭代分母降级
    assert "h=42.7" in md                               # 核时分母不受牵连
