#!/bin/bash
# collect_inventory.sh — 幂等软硬件画像采集（新单板接入入口，可重复运行）
#
# 用法:
#   SDC_ROOT_PW='...' ./collect_inventory.sh [输出根目录]    # su/pty 通道（密码经 env，不落盘）
#   ./collect_inventory.sh [输出根目录]                       # 若已配置 sudo -n 则免密
#   ./collect_inventory.sh [输出根目录]                       # 无 root → 降级非root采集并记录缺口
#
# 产物: <输出根目录>/<系统SN>-<日期>/{00_gaps.txt,01..15_*.txt}
# 规格同构: docs/superpowers/inventory/2102312YVY10M6000038-2026-09-23/
# 依据 plan: docs/superpowers/plans/2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-plan.md Task 4
set -u
OUT_ROOT="${1:-$(pwd)/inventory}"
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT

# ---------------- root 通道三级探测 ----------------
ROOT_MODE=none
if sudo -n true 2>/dev/null; then
    ROOT_MODE=sudo
elif [ -n "${SDC_ROOT_PW:-}" ]; then
    cat > "$TMPD/su_run.py" <<'PYEOF'
#!/usr/bin/env python3
import os, pty, sys, select, time
PW = os.environ['SDC_ROOT_PW']
def su_run(cmd, timeout=120):
    pid, fd = pty.fork()
    if pid == 0:
        os.execvp('su', ['su', 'root', '-c', cmd]); os._exit(127)
    out = b''; sent = False; end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.5)
        if r:
            try: d = os.read(fd, 65536)
            except OSError: break
            if not d: break
            out += d
        if not sent and (b'assword' in out or '密码'.encode() in out):
            os.write(fd, (PW + '\n').encode()); sent = True
    try: os.close(fd)
    except OSError: pass
    try: os.waitpid(pid, 0)
    except ChildProcessError: pass
    return out
sys.stdout.write(su_run(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 120)
                 .decode('utf-8', 'replace').replace(PW, '<REDACTED>'))
PYEOF
    chmod 700 "$TMPD/su_run.py"
    ROOT_MODE=supty
fi

runc() { # runc <输出文件> <命令>（root 类；无 root 通道时记录缺口）
    local out="$1"; shift
    case $ROOT_MODE in
        sudo)  timeout 120 sudo -n "$@" > "$out" 2>&1 ;;
        supty) SDC_ROOT_PW="$SDC_ROOT_PW" timeout 150 python3 "$TMPD/su_run.py" "$*" 2>&1 \
                   | tr -d '\r' | sed '1{/^密码：* *$/d;}' > "$out" ;;
        none)  echo "SKIP(no-root): $*" > "$out"; echo "no-root: $*" >> "$GAPS" ;;
    esac
}

# ---------------- SN 探测（决定目录名）----------------
SN=""
case $ROOT_MODE in
    sudo)  SN=$(sudo -n dmidecode -s system-serial-number 2>/dev/null | tr -d ' \n') ;;
    supty) SN=$(SDC_ROOT_PW="$SDC_ROOT_PW" timeout 60 python3 "$TMPD/su_run.py" "dmidecode -s system-serial-number" | tr -d '\r' | tail -1 | tr -d ' \n') ;;
esac
[ -z "$SN" ] && SN="unknown-$(hostname)"
DATE=$(date +%F)
OUT="$OUT_ROOT/${SN}-${DATE}"
mkdir -p "$OUT"
GAPS="$OUT/00_gaps.txt"; : > "$GAPS"
echo "root 通道: $ROOT_MODE → $OUT"

# ---------------- 非root 全集（01）----------------
{
echo "### root-mode: $ROOT_MODE"
echo "### cmd: lscpu"; lscpu
echo; echo "### cmd: nproc"; nproc
echo; echo "### cmd: uname -a"; uname -a
echo; echo "### cmd: /etc/os-release"; cat /etc/os-release
echo; echo "### cmd: /proc/cmdline"; cat /proc/cmdline
echo; echo "### cmd: free -h"; free -h
echo; echo "### cmd: /proc/cpuinfo (cpu0 block + count)"; sed -n '1,40p' /proc/cpuinfo; grep -c ^processor /proc/cpuinfo
echo; echo "### cmd: NUMA nodes"; for n in /sys/devices/system/node/node*; do echo "-- $n"; cat "$n/cpulist" 2>/dev/null; head -2 "$n/meminfo" 2>/dev/null; done
echo; echo "### cmd: cpu0 cache hierarchy"; for c in /sys/devices/system/cpu/cpu0/cache/index*; do echo "-- $c"; cat "$c/level" "$c/type" "$c/size" "$c/coherency_line_size" "$c/shared_cpu_list" 2>/dev/null; done
echo; echo "### cmd: cpufreq presence"; ls /sys/devices/system/cpu/cpu0/cpufreq/ 2>/dev/null || echo "NO cpufreq sysfs"
echo; echo "### cmd: governor"; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors 2>/dev/null
echo; echo "### cmd: thermal zones + trips"; for z in /sys/class/thermal/thermal_zone*; do echo "-- $z type=$(cat "$z/type" 2>/dev/null) temp=$(cat "$z/temp" 2>/dev/null) trips:$(cat "$z"/trip_point_*_temp 2>/dev/null | tr '\n' ' ') types:$(cat "$z"/trip_point_*_type 2>/dev/null | tr '\n' ' ')"; done
echo; echo "### cmd: hwmon names"; for h in /sys/class/hwmon/hwmon*; do echo "-- $h: $(cat "$h/name" 2>/dev/null)"; done
echo; echo "### cmd: sensors"; sensors 2>/dev/null || echo "lm-sensors 未安装"
echo; echo "### cmd: EDAC"; ls /sys/devices/system/edac/mc/ 2>/dev/null; for d in /sys/devices/system/edac/mc/mc*; do echo "-- $d"; cat "$d/ce_count" "$d/ue_count" "$d/size_mb" "$d/mem_type" 2>/dev/null; done
echo; echo "### cmd: rasdaemon service"; systemctl is-active rasdaemon 2>&1; systemctl is-enabled rasdaemon 2>&1
echo; echo "### cmd: kdump service"; systemctl is-active kdump 2>&1; systemctl is-enabled kdump 2>&1
echo; echo "### cmd: 工具盘点"; for t in stress-ng rasdaemon ipmitool sensors lstopo-no-graphics numactl turbostat cpupower dmidecode perf gcc g++ cmake meson ninja; do printf "%-20s" "$t:"; command -v "$t" >/dev/null 2>&1 && echo "$( "$t" --version 2>/dev/null | head -1 )" || echo "NOT FOUND"; done
echo; echo "### cmd: lsblk"; lsblk 2>/dev/null | head -25
echo; echo "### cmd: lspci summary"; lspci 2>/dev/null | head -30 || echo "lspci not available"
echo; echo "### cmd: ip -brief a"; ip -brief a 2>/dev/null
echo; echo "### cmd: df -h"; df -h | grep -vE 'tmpfs|overlay'
echo; echo "### cmd: 背景负载 top10"; ps aux --sort=-%cpu | head -11
} > "$OUT/01_nonroot_all.txt" 2>&1

# ---------------- root 全集（02–14）----------------
runc "$OUT/02_ipmitool_mc_info.txt"    ipmitool mc info
runc "$OUT/03_ipmitool_sdr_list.txt"   ipmitool sdr list
runc "$OUT/04_ipmitool_dcmi_power.txt" ipmitool dcmi power reading
runc "$OUT/05_ipmitool_fru.txt"        ipmitool fru
runc "$OUT/06_ipmitool_sel_list.txt"   ipmitool sel list
runc "$OUT/07_ipmitool_sel_info.txt"   ipmitool sel info
runc "$OUT/08_dmidecode_system.txt"    dmidecode -t system
runc "$OUT/09_dmidecode_baseboard.txt" dmidecode -t baseboard
runc "$OUT/10_dmidecode_bios.txt"      dmidecode -t bios
runc "$OUT/11_dmidecode_processor.txt" dmidecode -t processor
runc "$OUT/12_dmidecode_memory.txt"    dmidecode -t memory
runc "$OUT/13_dmesg_ras.txt"           sh -c "dmesg | grep -iE 'mce|edac|ghes|apei|thermal|throttl|error|fault|ras|ddr' | tail -80"
runc "$OUT/14_journal_errors.txt"      sh -c "journalctl -k -b 0 --no-pager | grep -iE 'error|fault|fail|edac|thermal' | tail -50"

# ---------------- 基线采样（15，3×10s 间隔）----------------
: > "$OUT/15_baseline_samples.txt"
for i in 1 2 3; do
    echo "--- sample $i @ $(date '+%F %T') ---" >> "$OUT/15_baseline_samples.txt"
    runc "$TMPD/b$i.txt" sh -c "ipmitool sdr type Temperature; ipmitool sdr type Fan; ipmitool dcmi power reading"
    tr -d '\r' < "$TMPD/b$i.txt" >> "$OUT/15_baseline_samples.txt"
    [ $i -lt 3 ] && sleep 10
done

# ---------------- 汇总 ----------------
echo "=== 画像完成: $OUT"
echo "=== 文件数: $(ls "$OUT" | wc -l)"
echo "=== root 通道: $ROOT_MODE；缺口数: $(grep -c . "$GAPS" || true)"
[ -s "$GAPS" ] && { echo "=== 缺口清单（须人工评估）==="; cat "$GAPS"; }
echo "=== 下一步: 按本机 Tjmax 与内存预算推导联锁阈值（见 NEW_BOARD_ONBOARDING.md）==="
