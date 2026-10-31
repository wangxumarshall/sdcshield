import os, subprocess

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")
DRIVER = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc-excite-reproduce.sh")

# 定向复测条件保真（PR #331 检视轮，2026-10-31）：复测命令的四维条件旗标
# （线程 -n / 每测试时长 -t / CPU 拓扑 --cpuset / 测试旋钮 -O）必须沿用原命令。
# 历史条件失真 → 假通过台账的两次实证：
#   - 硬编码 -n 0（2026-10-30 retest1.out）：多线程用例 SKIP 却记 rc=0 fail/crash=0；
#   - 硬编码 -t 120s（同日 mesh 事件检视）：169GB 缓冲整轮 ~117s，相位 2 跑不完即沿
#     EXIT_SUCCESS 空过——时长维度与 -n 0 是同族缺陷。


def _bash(body):
    return subprocess.run(["bash", "-c", f"source '{COMMON}'\n{body}"],
                          capture_output=True, text=True, timeout=15)


def _flags(cmd):
    return _bash(f'extract_retest_flags {cmd!r}').stdout.strip()


# ---------------- flag_time_to_s：框架 string_to_millisecs 同口径 ----------------

def test_flag_time_to_s_framework_parity():
    # 裸数=毫秒（framework/sandstone_chrono.cpp:30 实测语义，非秒）
    out = _bash("for v in 5000 120s 2m 30min 1h 999ms; do printf '%s=%s\\n' $v $(flag_time_to_s $v); done")
    assert out.stdout.strip() == "5000=5\n120s=120\n2m=120\n30min=1800\n1h=3600\n999ms=1", out.stdout

def test_flag_time_to_s_rejects_garbage():
    p = _bash("flag_time_to_s abc || echo RC1")
    assert p.stdout.strip() == "RC1"
    p = _bash("flag_time_to_s '' || echo RC1")
    assert p.stdout.strip() == "RC1"


# ---------------- extract_retest_flags：四维旗标提取 ----------------

def test_extract_dwell_form():
    # L4 dwell 实际形态（2026-10-30 mesh 事件）：只有 -t（可达小时级）
    assert _flags("/b/sdcshield -e mesh_upi_sse_sym -t 72000s "
                  "--max-test-loop-count=0 -vvv --on-crash=context") == "-t 72000s"

def test_extract_cpuset_form():
    # l5_dom 阶段（run_bounded 构造）：--cpuset 必须沿用，否则 cpuset 受限相位
    # 复测跑成全核，有效线程集与跨核拓扑均已改变（thread_count() 尊重 --cpuset）
    assert _flags("/b/sdcshield -T 1800s -t 60s --strict-runtime -e a,b --cpuset=0-31 -vvv") \
        == "-t 60s --cpuset=0-31"

def test_extract_knob_and_threads_form():
    # l5_numa 阶段：-O 旋钮 + -n 线程 + -t（检视 #10 点名 -O 被丢弃）
    assert _flags("/b/sdcshield -e openblas_dgemm -O openblas_dgemm.mdim=2048 -n 64 -t 1350s -vvv") \
        == "-O openblas_dgemm.mdim=2048 -n 64 -t 1350s"

def test_extract_multiple_knobs():
    # l3 阶段：多个 -O 全部沿用（其他测试的旋钮对定向单测只是查不到，无副作用）
    assert _flags("/b/sdcshield -T 21600s -t 60s -e a,b "
                  "-O openssl_sm3sm4.data_size=1048576 -O eigen.mdim=1024 -vvv") \
        == "-t 60s -O openssl_sm3sm4.data_size=1048576 -O eigen.mdim=1024"

def test_extract_attached_and_equals_forms():
    # 检视 #10 点名 "-n8" 短参连写（getopt 合法）不认即静默退化全核
    assert _flags("/b/sdcshield -n8 -t120s -e zstd19") == "-n 8 -t 120s"
    assert _flags("/b/sdcshield --threads=16 --time=90s --test-option=a.b=1 --cpuset 0-3") \
        == "-n 16 -t 90s -O a.b=1 --cpuset=0-3"

def test_extract_total_time_not_confused():
    # -T（总时长）是不同选项：大小写敏感，不得误提（cold_c4 事件命令形态）
    assert _flags("/b/sdcshield -T 1800s -t 60s --strict-runtime -e memcpy_rewr,cachebounce") \
        == "-t 60s"

def test_extract_drops_invalid_and_absent():
    assert _flags("/b/sdcshield -e zstd19 -vvv") == ""            # 无条件旗标
    assert _flags("/b/sdcshield -n abc -t xyz -O nokey -e zstd19") == ""  # 值非法 → 丢弃


# ---------------- retest_t_budget：预算三档 + 看门狗伸缩 ----------------

def test_budget_inherited_under_cap():
    out = _bash("retest_t_budget 480s").stdout.split()
    assert out == ["480s", "inherited", "570"]      # 预算沿用，看门狗=480+90

def test_budget_capped_for_dwell_scale():
    # dwell 类小时级 -t：封顶 900s（wholecmd 路径 900s 封顶同款先例）
    out = _bash("retest_t_budget 72000s").stdout.split()
    assert out == ["900s", "capped(72000s)", "990"]

def test_budget_default_when_original_has_no_t():
    out = _bash("retest_t_budget ''").stdout.split()
    assert out == ["120s", "default", "210"]

def test_budget_watchdog_floor():
    # 短预算（-t 60s）看门狗不小于旧硬编码 150s
    out = _bash("retest_t_budget 60s").stdout.split()
    assert out == ["60s", "inherited", "150"]

def test_budget_env_overrides():
    p = _bash("RETEST_T_CAP_S=300 RETEST_T_DEFAULT_S=60 retest_t_budget 999s")
    assert p.stdout.split() == ["300s", "capped(999s)", "390"]
    p = _bash("RETEST_T_DEFAULT_S=60 retest_t_budget ''")
    assert p.stdout.split() == ["60s", "default", "150"]


# ---------------- 驱动接线（源码级守护，同 test_driver_memguard 风格）----------------

def test_driver_wires_condition_fidelity():
    src = open(DRIVER).read()
    # 四维提取 + 预算策略接线
    assert 'extract_retest_flags "$cmd"' in src
    assert 'retest_t_budget "$orig_t"' in src
    # 旧的两个条件失真源已灭：复测命令行不再有硬编码时长；旧 -n 提取正则已删
    assert '-t 120s --max-test-loop-count=0' not in src
    assert "(-n|--threads)[= ]+[0-9]+" not in src
    assert "nthr_args" not in src
    # 复测命令行携带四维条件旗标与独立 -t 预算
    assert '"${cond_flags[@]}" -t "$retest_t"' in src
    # 看门狗不再硬编码 150s（随预算伸缩）
    assert '"${retest_guard_s}s"' in src
    assert 'retest_guarded "retest$i" "$evdir/retests.txt" 150s' not in src

def test_driver_ledger_line_honest_labels():
    # 台账行三要素：skip 计数（SKIP 不得读作复测通过）、t=预算(来源)、elapsed
    src = open(DRIVER).read()
    assert "skip=$(grep -Ec 'result: *skip'" in src
    assert "t=$retest_t($retest_t_src)" in src
    assert "elapsed=$((SECONDS - t0))s" in src
