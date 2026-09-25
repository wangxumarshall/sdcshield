#!/bin/bash
# ============================================================
# SDC 单板机器扫描 —— SDC 压测战役第 0 步（新单板直接复用）
#
# 一次性收集全部影响 SDC 激发/检测战役设计的软硬件信息。产出：
#   <dir>/capabilities.env    机器能力开关（战役参数适配依据）
#   <dir>/machine_summary.md  人读摘要 + 异常标记
#   <dir>/*.txt               原始输出（dmidecode/ipmi/SEL/EDAC/...）
#
# 依据：docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md §2.3/§4.4（激发杠杆）
#      docs/superpowers/plans/2026-09-23-sdc-campaign.md（战役方案）
#
# 用法：bash scripts/run/sdc_machine_scan.sh [输出目录]
# 提权（自动探测，全部失败则降级并如实记录缺失项）：
#   root > SUDO_PW 环境变量（echo|sudo -S） > 免密 sudo -n > 降级（只采 sysfs/hwmon/journal 可读部分）
# ============================================================
set -u
OUT="${1:-scan_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "$OUT"
SUMMARY="$OUT/machine_summary.md"
CAP="$OUT/capabilities.env"
: > "$SUMMARY"; : > "$CAP"

mode=degraded
if [ "$(id -u)" = 0 ]; then mode=root
elif [ -n "${SUDO_PW:-}" ] && echo "$SUDO_PW" | sudo -S true 2>/dev/null; then mode=sudo_pw
elif sudo -n true 2>/dev/null; then mode=sudo_n; fi
as_root() {
    case "$mode" in
        root)    "$@" ;;
        sudo_pw) echo "$SUDO_PW" | sudo -S "$@" 2>/dev/null ;;
        sudo_n)  sudo -n "$@" 2>/dev/null ;;
        *)       return 1 ;;
    esac
}
sec()  { printf '\n## %s\n\n' "$1" >> "$SUMMARY"; }
line() { printf -- "- %s\n" "$1" >> "$SUMMARY"; }

# ---------- 1. 平台身份 ----------
sec "平台身份"
as_root dmidecode > "$OUT/dmidecode_full.txt" 2>/dev/null
D="$OUT/dmidecode_full.txt"
if [ -s "$D" ]; then
    line "BIOS: $(sed -n '/^BIOS Information/,/^$/p' "$D" | grep -E 'Vendor|Version|Release' | tr -s ' \t' ' ' | tr '\n' ' ')"
    line "整机: $(sed -n '/^System Information/,/^$/p' "$D" | grep -E 'Manufacturer|Product Name|Serial' | tr -s ' \t' ' ' | tr '\n' ' ')"
    line "板卡: $(sed -n '/^Base Board Information/,/^$/p' "$D" | grep -E 'Manufacturer|Product Name|Version|Serial' | tr -s ' \t' ' ' | tr '\n' ' ')"
    line "处理器: $(grep -A16 '^Processor Information' "$D" | grep -E 'Version|Max Speed|Current Speed|Voltage|Status' | sort -u | tr '\n' ' ')"
    DIMM_N=$(grep -c 'Memory Device' "$D"); DIMM_POP=$(grep -cE $'^\tSize: [0-9]+ ' "$D")
    line "内存: ${DIMM_POP}/${DIMM_N} 槽位插条；$(grep -E $'^\t(Size: [0-9]+|Speed:|Manufacturer:|Part Number:)' "$D" | sort -u | tr '\n' ' ')"
else
    line "⚠️ 无 root：dmidecode 未采集"
fi

# ---------- 2. BMC / IPMI ----------
sec "BMC 与传感器"
HAS_IPMI=0
if command -v ipmitool >/dev/null 2>&1 && timeout 10 ipmitool sel info >/dev/null 2>&1; then HAS_IPMI=1; fi
if [ "$HAS_IPMI" = 1 ]; then
    timeout 15 ipmitool mc info        > "$OUT/ipmi_mc_info.txt" 2>/dev/null
    timeout 15 ipmitool sensor list    > "$OUT/ipmi_sensors.txt" 2>/dev/null
    timeout 15 ipmitool dcmi power reading > "$OUT/ipmi_power.txt" 2>/dev/null
    timeout 20 ipmitool sel list       > "$OUT/ipmi_sel.txt" 2>/dev/null
    timeout 15 ipmitool fru            > "$OUT/ipmi_fru.txt" 2>/dev/null
    line "BMC: $(grep -E 'Firmware|Manufacturer|IPMI Version' "$OUT/ipmi_mc_info.txt" | tr '\n' ' ')"
    line "传感器总数: $(grep -c '|' "$OUT/ipmi_sensors.txt")；温度/电压/功耗/风扇样例:"
    grep -iE 'degrees C|Volts|Watts|RPM' "$OUT/ipmi_sensors.txt" | head -20 | sed 's/  */ /g; s/^/    /' >> "$SUMMARY"
    if [ "$(id -u)" != 0 ] && [ -c /dev/ipmi0 ] && [ ! -r /dev/ipmi0 ]; then
        line "提示：/dev/ipmi0 当前用户不可读，执行 sudo chmod 666 /dev/ipmi0 后可采样（战役后恢复 600）"
    fi
else
    line "⚠️ ipmitool 不可用或无 /dev/ipmi0 访问权 —— 温度/电压/SEL 通道缺失"
fi

# ---------- 3. SEL 异常分析 ----------
sec "SEL 事件日志分析"
if [ -s "$OUT/ipmi_sel.txt" ]; then
    awk -F'|' '{gsub(/^ +| +$/,"",$4); print $4}' "$OUT/ipmi_sel.txt" | sort | uniq -c | sort -rn > "$OUT/sel_distribution.txt"
    head -5 "$OUT/sel_distribution.txt" | sed 's/^/    /' >> "$SUMMARY"
    top_cnt=$(head -1 "$OUT/sel_distribution.txt" | awk '{print $1}')
    if [ "${top_cnt:-0}" -gt 50 ]; then
        line "🚨 SEL 高频重复故障（$(head -1 "$OUT/sel_distribution.txt" | sed 's/^ *[0-9]* *//') 共 ${top_cnt} 条）——周期性硬件故障信号，战役期间监控与负载相关性"
    fi
else
    line "（无 SEL 数据）"
fi

# ---------- 4. CPU / 拓扑 / 内存位置 ----------
sec "CPU 与拓扑"
NCORES=$(nproc)
lscpu > "$OUT/lscpu.txt"
HAS_SVE=0; lscpu | awk '/^Flags/' | grep -qw sve && HAS_SVE=1
line "CPU: $(grep -m1 'Model name' "$OUT/lscpu.txt" | cut -d: -f2- | tr -s ' ')，${NCORES} 核，SMT: $(grep -m1 'Thread' "$OUT/lscpu.txt" | tr -s ' ')"
line "SVE: $HAS_SVE（0=无，SVE 类测试将 clean-skip）"
lscpu -C > "$OUT/cache_topology.txt"; cat /sys/devices/system/node/node*/cpulist > "$OUT/numa_cpulist.txt"
line "缓存: $(grep -E '^L[123]' "$OUT/cache_topology.txt" | awk '{printf "%s=%s(共%s) ",$1,$2,$3}')"
NNODES=0; MEM_NODES=""
for n in /sys/devices/system/node/node[0-9]*; do
    [ -d "$n" ] || continue
    nid=$(basename "$n"); NNODES=$((NNODES+1))
    mt=$(awk '/MemTotal/{print $4}' "$n/meminfo")
    printf '%s cpus=%s memTotal=%skB\n' "$nid" "$(cat "$n/cpulist")" "$mt" >> "$OUT/numa_meminfo.txt"
    [ "${mt:-0}" -gt 0 ] && MEM_NODES="${MEM_NODES}${nid#node} "
done
line "NUMA: ${NNODES} 个 node；有本地内存的 node: ${MEM_NODES:-无}"
if [ "$NNODES" -gt 1 ] && [ "$(echo $MEM_NODES | wc -w)" -lt "$NNODES" ]; then
    line "⚠️ 存在无本地内存的 node（跨互连访存，解读 NUMA 敏感结果时必须考虑）"
fi

# ---------- 5. 频率 ----------
sec "频率"
HAS_CPUFREQ=0; [ -d /sys/devices/system/cpu/cpu0/cpufreq ] && HAS_CPUFREQ=1
CPU_FREQ_MHZ=""
[ -s "$D" ] && CPU_FREQ_MHZ=$(grep -A16 '^Processor Information' "$D" | grep -m1 'Current Speed' | grep -oE '[0-9]+')
line "cpufreq sysfs: $HAS_CPUFREQ；DMI 标称频率: ${CPU_FREQ_MHZ:-未知} MHz；cpuidle states: $(ls /sys/devices/system/cpu/cpu0/cpuidle/ 2>/dev/null | wc -l) 个"
[ "$HAS_CPUFREQ" = 0 ] && line "→ 无调频：V/F 角落扫描杠杆缺失，以负载多样性 + 并发档（电流拉载代理）补偿（synthesis §2.3）"

# ---------- 6. RAS / EDAC ----------
sec "RAS / EDAC"
HAS_EDAC=0
if [ -d /sys/devices/system/edac/mc ]; then HAS_EDAC=1
    : > "$OUT/edac.txt"
    for m in /sys/devices/system/edac/mc/mc*/; do
        echo "$(basename "$m") name=$(cat "$m/mc_name" 2>/dev/null) size=$(cat "$m/size_mb" 2>/dev/null)MB ce=$(cat "$m/ce_count" 2>/dev/null) ue=$(cat "$m/ue_count" 2>/dev/null)" >> "$OUT/edac.txt"
    done
    sed 's/^/    /' "$OUT/edac.txt" >> "$SUMMARY"
fi
RAS_CNT=$( (as_root dmesg 2>/dev/null || journalctl -k --no-pager 2>/dev/null) | grep -icE 'edac|hardware error|machine check' || true)
line "EDAC: $HAS_EDAC；内核日志 RAS 相关行: ${RAS_CNT:-0}（仅初始化行为正常，出现事件行需人工判读）"
(as_root dmesg 2>/dev/null || journalctl -k --no-pager 2>/dev/null) | grep -i 'RAS Extension' | head -1 | sed 's/^/    /' >> "$SUMMARY" || true

# ---------- 7. 温度/传感器（OS 侧） ----------
sec "OS 侧温度通道"
: > "$OUT/hwmon.txt"
for h in /sys/class/hwmon/hwmon*; do
    [ -d "$h" ] || continue
    printf '%s name=%s temps=%s\n' "$(basename "$h")" "$(cat "$h/name" 2>/dev/null)" \
        "$(cat "$h"/temp*_input 2>/dev/null | paste -sd/ -)" >> "$OUT/hwmon.txt"
done
sed 's/^/    /' "$OUT/hwmon.txt" >> "$SUMMARY"
line "thermal zones: $(cat /sys/class/thermal/thermal_zone*/type 2>/dev/null | paste -sd, -)；cooling devices: $(ls /sys/class/thermal/ 2>/dev/null | grep -c cooling)"

# ---------- 8. 软件栈 ----------
sec "软件栈"
head -5 /etc/os-release > "$OUT/os.txt"; uname -a >> "$OUT/os.txt"; cat /proc/cmdline >> "$OUT/os.txt"
: > "$OUT/tools.txt"
for t in gcc g++ gfortran make cmake ninja meson perl python3 pkg-config ipmitool numactl taskset stress-ng smartctl; do
    if command -v "$t" >/dev/null 2>&1; then
        if [ "$t" = ipmitool ]; then printf '%-12s %s\n' "$t" "$("$t" -V 2>&1 | head -1)" >> "$OUT/tools.txt"
        else printf '%-12s %s\n' "$t" "$("$t" --version 2>&1 | head -1)" >> "$OUT/tools.txt"; fi
    else printf '%-12s MISSING\n' "$t" >> "$OUT/tools.txt"; fi
done
for c in ~/tools/cmake/bin/cmake ~/tools/cmake-rpm/bin/cmake; do
    [ -x "$c" ] && printf 'cmake(user)  %s\n' "$("$c" --version | head -1)" >> "$OUT/tools.txt"
done
sed 's/^/    /' "$OUT/tools.txt" >> "$SUMMARY"
line "页大小: $(getconf PAGESIZE)；THP: $(cat /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null)；perf paranoid: $(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null)"
line "SELinux: $(getenforce 2>/dev/null)；tuned: $(tuned-adm active 2>/dev/null | head -1)"
rpm -qa 2>/dev/null | grep -iE 'openssl|isa-?l|gmp|openblas|sleef|compute.?library|ipmitool' > "$OUT/rpms.txt" || true

# ---------- 9. 干扰源 ----------
sec "运行环境与干扰源"
{ uptime; who; ps -eo user,pcpu,pmem,comm --sort=-pcpu | head -8;
  systemctl list-timers --no-pager --no-legend 2>/dev/null | head -8; } > "$OUT/env.txt" 2>&1
sed 's/^/    /' "$OUT/env.txt" >> "$SUMMARY"
line "crontab: $(crontab -l 2>&1 | head -1)"

# ---------- 10. capabilities.env ----------
{
    echo "# 机器能力开关 —— 由 sdc_machine_scan.sh 生成于 $(date '+%F %T')"
    echo "NCORES=$NCORES"
    echo "NNODES=$NNODES"
    echo "MEM_NODES=\"${MEM_NODES% }\""
    echo "HAS_SVE=$HAS_SVE"
    echo "HAS_CPUFREQ=$HAS_CPUFREQ"
    echo "HAS_IPMI=$HAS_IPMI"
    echo "HAS_EDAC=$HAS_EDAC"
    echo "CPU_FREQ_MHZ=${CPU_FREQ_MHZ:-unknown}"
    echo "SCAN_DATE=$(date +%F)"
} >> "$CAP"

sec "能力开关（capabilities.env）"
sed 's/^/    /' "$CAP" >> "$SUMMARY"
printf '\n扫描完成: %s\n' "$OUT"
