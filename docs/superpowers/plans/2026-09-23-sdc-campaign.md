# SDC 24h+ 全核压测战役（激发规律驱动、可复用）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在本机（RCSIT TG225 B1，TaiShan 200，2×Kunpeng 920 5250 = 96 核）执行一次基于文献激发规律的 24h+ 全核 SDC 压测战役，产出可复用的「机器扫描脚本 + 战役执行脚本 + 输出报告」，新单板可直接照此复用。

**Architecture:** 五阶段：P0 构建验证（vendored 5 库 + meson）→ P1 全量广域扫（`--quality=0` 全测试 × 全核 × fracturing seed 自动轮换）→ P2 文献优先级加权 soak（GEMM 尺寸/形态谱×三调度、crypto、压缩 level 谱、SLEEF 足迹谱、FFT 因子谱、mesh 一致性 + RNG 引擎多样性）→ P3 固定 seed 深驻留（`--max-test-loop-count=0`）→ P4 持续循环×5（广域扫重跑 `--test-list-randomize` + NUMA 拓扑/并发档 + 轮换驻留）。全程 ipmitool/EDAC/SEL 环境监测（30s 采样），fail-continue + 自动失败分类（known_benign_ulp / full_core_only / sdc_suspect）+ 可复现 suspect 自动逐核二分。

**Tech Stack:** bash（两个可复用脚本）、sdcshield（本仓库）、ipmitool/dmidecode/sysfs、GitHub cmake tarball（免 root）。

**Spec:** `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`（31 篇文献激发规律，§2.3 触发条件 / §4.4 七原则 / §7.4 战役协议）+ `docs/SDC_TEST_AUDIT_REPORT_2026-09-18.md`（假通过用例 D1/D2/D6/D11）+ 本计划（方案本体）。输出报告：`docs/superpowers/output/2026-09-23-sdc-campaign-output.md`。

## 本机扫描结论（2026-09-23 已完成，战役参数据此设计）

| 项 | 值 | 对战役的影响 |
|---|---|---|
| CPU | 2× Kunpeng 920 5250（TaiShan v110, part 0xd01），96 核无 SMT | 全核并发 = 96 |
| 频率 | **2.6GHz 固定**（Max=Current=2600MHz，无 cpufreq sysfs、无 CPPC、无 cpuidle 状态） | V/F 扫描杠杆缺失 → 用负载多样性 + 并发档（电流拉载代理）补偿 |
| 电压 | VDDAVS 0.92/0.91V（nominal 0.9V）、VDDFIX 0.80V、VDDQ 1.23V | 监测通道 |
| SVE | **无**（HWCAP 无 sve） | sleef_sve/SVE 类测试预期 clean-skip，报告如实记录 |
| 内存 | **32 槽仅 1 条 Hynix 32GB DDR4-2933，全在 node0；node1 本地内存 = 0** | node1 全部访存跨 HCCS；解读 NUMA 敏感结果时必须考虑 |
| L3 | 24MB × 4 实例 | GEMM 尺寸谱 mdim 64/256/512/1024 覆盖 L1→LLC/DRAM |
| RAS | ARM RAS Extension、ghes_edac（34 天 0 CE/UE）、dmesg 0 硬件错误、kdump 已配 | mce_check 有效；EDAC/SEL 作监测通道 |
| BMC | Hi1711 fw 3.11，IPMI 2.0，BT 接口，132 传感器；`/dev/ipmi0` 已 chmod 666（sdc 可读） | 温度/电压/功耗/风扇/SEL 全程采样 |
| **异常 1** | **SEL 350 条 `Slot/Connector #0x84 Fault` 周期断言（~8.5min 周期，始于 2026-09-22 13:24）** | 战役期间监控其与负载/温度的相关性 |
| 异常 2 | FAN3 转速 0 RPM、FAN1/4 传感器 na | 记录在案 |
| 软件 | openEuler 24.03 SP3、gcc 12.3.1、meson 1.11.1、gfortran 有、**cmake 无**（GitHub tarball 免 root 装）、numactl 无（taskset 可用）、perf paranoid=2、SELinux Enforcing、PCP/sysstat 在跑 | ACL/SLEEF 构建需先装 cmake |
| 温度传感器 | CPU1/CPU2 Core Rem（结温 56/47°C）、MEM/VRD/VDDQ Temp、Outlet/Inlet、功耗 234W（CPU 116W） | 监测 CSV 列 |

## Global Constraints

- **24h+ 预算**：战役总时长 ≥24h。**执行期修正（2026-09-23 冒烟后发现）**：`-t` 语义为**每测试**而非每次运行，原 P2/P4 档位算术失算（如 mesh 39 测试 × 8m = 5.2h）。修正后预算：构建 1h + P1 1.9h（333×20s）+ P2 ≈4.5h（GEMM 尺寸谱 4×4×7m=112min 主导）+ P3 2h（4×30m）+ P4 4 cycles×3.8h（扫 2.1h + 拓扑 4×5×2m + 驻留 1h）≈ **24.6h**；CYCLES 默认 5→4，TOPO_TIME 10m→2m（每测试）。详见「执行期修订记录」。
- **One-patch-per-unit**：仅 3 个 commit（扫描脚本 / 战役脚本 / 输出报告），构建与执行任务不产生 commit。
- **分支**：`feat/sdc-campaign`（自当前 `pr-147` HEAD 切出——战役使用含 mesh 修复的最新代码）；每 commit 验证后自动 push，**不 push main**。
- **提交信息结尾不加 `Co-Authored-By: Claude <noreply@anthropic.com>`**（CLAUDE.md 明令）。
- **x86-64 不动**：本计划只新增 `scripts/run/` 两个脚本与 `docs/` 文档，不改框架/测试代码。
- **诚实报告**：假通过用例（D1 fma / D6 vmx_* / D11 isal_crc*+crc32*）的 pass 标注「无检出意义」；降级/缺项/未知如实记录；「全 pass」只声明「当前覆盖×时长内未见 SDC」。
- **进度真源**：本文件 checkbox 是唯一进度真源；scope 变化先改本文件。

## Review Focus（最可能咬人的失败模式 → 已映射到任务步骤）

1. **YAML 结果字段名臆断**（解析器读不到 fail → 漏报）→ Task 1 步骤 8 实机检查 YAML schema；Task 3 步骤 2 `--selftest-classify` 用构造样例验证 `parse_results`。
2. **ipmitool 偶发超时拖死监测循环** → Task 3 脚本中每次 ipmitool 调用 `timeout 8` 包裹、失败写 `na` 继续；冒烟验证 monitor.csv 持续追加。
3. **24h 中途会话/进程中断** → `.done_*` 阶段标记 + `nohup setsid` 脱离会话 + 重启自动跳过已完成阶段；Task 3 冒烟含杀进程重启演练。
4. **环境差异导致硬失败**（无 SVE / 某库没构建 / -O 选项不适用）→ 全部测试集动态发现自 `--list-test-ids`，`--ignore-unknown-tests` 兜底，空集档位自动跳过。
5. **磁盘塞满 / 假通过被计入有效覆盖** → 每阶段磁盘守卫（<5G 停）；汇总用 `FAKE_PASS_RE` 单列装饰性 pass，报告中扣除。

---

### Task 1: P0 构建与框架验证（无 commit）

**Files:** 无仓库改动（builddir/ 与 third-party/*/install/ 均 gitignored）。

**Interfaces:**
- Produces: `./builddir/sdcshield`（后续所有任务的被测对象）；`--list-test-ids` 输出的测试全集（Task 3 动态发现的输入）；YAML schema 实样（Task 3 解析器依据）；`-s help` 引擎名清单（Task 3 RNG 档依据）。

- [x] **Step 1: 安装 cmake（免 root，GitHub 官方 tarball）**

```bash
mkdir -p ~/tools && cd ~/tools
curl -L -o cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-aarch64.tar.gz
tar xzf cmake.tar.gz && mv cmake-3.30.5-linux-aarch64 cmake && rm cmake.tar.gz
~/tools/cmake/bin/cmake --version   # 期望: cmake version 3.30.5
```

- [x] **Step 2: 串行构建 5 个 vendored 库（脚本幂等，可重复执行）**

```bash
cd /home/sdc/wangxu/sdcshield
export PATH=~/tools/cmake/bin:$PATH     # acl/sleef 需要 cmake
for lib in openssl sleef isa-l openblas acl; do
  echo "=== building $lib ==="
  ./third-party/$lib/build.sh || echo "!!! $lib 构建失败（战役降级继续，记录缺口）"
done
ls third-party/*/install/lib/*.a       # 期望: libcrypto.a libsleef.a libisal.a libopenblas.a libarm_compute-core.a
```
预期：openssl ~1min、sleef ~1min、isa-l ~1min、openblas ~2min、acl 最长（10-30min）。任一失败不停流程（meson 对应测试族不构建，报告记录覆盖缺口）。

- [x] **Step 3: meson setup + ninja**

```bash
cd /home/sdc/wangxu/sdcshield
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
ninja -C builddir                      # 期望: 全部目标构建成功，0 error
```

- [x] **Step 4: 冒烟——CPU 信息与测试清单**

```bash
./builddir/sdcshield --dump-cpu-info | head -20     # 期望: 96 CPUs, 2 packages, Kunpeng-920
./builddir/sdcshield --list-test-ids | wc -l        # 记录测试总数 N_total
./builddir/sdcshield --list-test-ids | grep -cE '^(sleef_sve)'   # SVE 类存在，运行时会 skip
./builddir/sdcshield --list-groups | head -20       # 记录可用 @group
```

- [x] **Step 5: 冒烟——单测试全通过 + RNG 引擎 + seed 机制**

```bash
./builddir/sdcshield -e zstd19 -t 5000 -n 1 -o /tmp/p0_zstd.yaml    # 期望: exit 0, result: pass
./builddir/sdcshield -s help                                        # 记录引擎名（Constant/LCG/AES 的准确拼写）
./builddir/sdcshield -e crc32 -t 3000 -n 1 -o /tmp/p0_a.yaml && \
./builddir/sdcshield -e crc32 -t 3000 -n 1 -o /tmp/p0_b.yaml
grep -iE 'seed|rng' /tmp/p0_a.yaml /tmp/p0_b.yaml | head -4         # 验证每次调用 seed 是否自动变化
```

- [x] **Step 6: 冒烟——crash 上下文转储可用**

```bash
./builddir/sdcshield --selftests --on-crash=context -e selftest_sigsegv -vv 2>&1 | tail -15
# 期望: crash 被捕获并打印 backtrace/context，父进程正常回收
```

- [x] **Step 7: 冒烟——`-O` 选项与 wildcards**

```bash
./builddir/sdcshield -e openblas_dgemm -O openblas_dgemm.mdim=64 -t 3000 -n 1 -o /tmp/p0_gemm.yaml   # 期望 pass
./builddir/sdcshield -e 'mesh_*' --ignore-unknown-tests -t 3000 -o /tmp/p0_mesh.yaml                 # 通配符可行
```

- [x] **Step 8: 检查真实 YAML schema（Task 3 解析器依据）**

```bash
cat /tmp/p0_zstd.yaml            # 记录: 测试 id 字段名（id/test/...）与 result 字段的准确格式、缩进
./builddir/sdcshield --quality=0 -e zstd19,vmx_io_exit -t 2000 -o /tmp/p0_q0.yaml && cat /tmp/p0_q0.yaml
# 记录 --quality=0 是否纳入 BETA 测试（对照 --list-tests 差异）
```
**若 schema 与 Task 3 假设（`id:`/`test:` + `result:` 行）不符，先改 Task 3 脚本的 `parse_results` 再继续。**

---

### Task 2: 机器扫描脚本 `sdc_machine_scan.sh`（commit #1）

**Files:**
- Create: `scripts/run/sdc_machine_scan.sh`（可执行）
- Modify: `README.md`（在 `## Vendored 计算库` 一节「模式 D」之后追加「模式 E/F」两段）

**Interfaces:**
- Consumes: 系统 DMI/IPMI/sysfs（root 或 SUDO_PW 环境变量或免密 sudo，全无则降级）。
- Produces: `<dir>/capabilities.env`（`NCORES/NNODES/HAS_SVE/HAS_CPUFREQ/HAS_IPMI/HAS_EDAC/MEM_NODES/CPU_FREQ_MHZ/...` 键值对）、`<dir>/machine_summary.md`、`<dir>/*.txt` 原始件。战役脚本（Task 3）不依赖它（自探测），但报告（Task 5）引用其产物。

- [x] **Step 1: 写入脚本（完整代码如下）**

```bash
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
    DIMM_N=$(grep -c 'Memory Device' "$D"); DIMM_POP=$(grep -A22 'Memory Device' "$D" | grep -cE 'Size: [0-9]+')
    line "内存: ${DIMM_POP}/${DIMM_N} 槽位插条；$(grep -A22 'Memory Device' "$D" | grep -E 'Size: [0-9]+|Speed|Manufacturer|Part Number' | sort -u | tr '\n' ' ')"
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
    line "提示：若以非 root 运行且 /dev/ipmi0 权限拒绝，执行: sudo chmod 666 /dev/ipmi0（战役后恢复 600）"
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
        line "🚨 SEL 高频重复故障（$(head -1 "$OUT/sel_distribution.txt" | cut -c9-) 共 ${top_cnt} 条）——周期性硬件故障信号，战役期间监控与负载相关性"
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
[ "$NNODES" -gt 1 ] && [ -z "$MEM_NODES" -o "$(echo $MEM_NODES | wc -w)" -lt "$NNODES" ] && \
    line "⚠️ 存在无本地内存的 node（跨互连访存，解读 NUMA 敏感结果时必须考虑）"

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
for h in /sys/class/hwmon/hwmon*; do
    printf '%s name=%s temps=%s\n' "$(basename "$h")" "$(cat "$h/name" 2>/dev/null)" \
        "$(cat "$h/temp*_input 2>/dev/null | paste -sd/ -)" >> "$OUT/hwmon.txt"
done
[ -f "$OUT/hwmon.txt" ] && sed 's/^/    /' "$OUT/hwmon.txt" >> "$SUMMARY"
line "thermal zones: $(cat /sys/class/thermal/thermal_zone*/type 2>/dev/null | paste -sd, -)；cooling devices: $(ls /sys/class/thermal/ 2>/dev/null | grep -c cooling)"

# ---------- 8. 软件栈 ----------
sec "软件栈"
head -5 /etc/os-release > "$OUT/os.txt"; uname -a >> "$OUT/os.txt"; cat /proc/cmdline >> "$OUT/os.txt"
: > "$OUT/tools.txt"
for t in gcc g++ gfortran make cmake ninja meson perl python3 pkg-config ipmitool numactl taskset stress-ng smartctl; do
    if command -v "$t" >/dev/null 2>&1; then printf '%-12s %s\n' "$t" "$("$t" --version 2>&1 | head -1)" >> "$OUT/tools.txt"
    else printf '%-12s MISSING\n' "$t" >> "$OUT/tools.txt"; fi
done
# cmake 可能装在 ~/tools/cmake/bin（免 root 路径）
[ -x ~/tools/cmake/bin/cmake ] && printf 'cmake(user)  %s\n' "$(~/tools/cmake/bin/cmake --version | head -1)" >> "$OUT/tools.txt"
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
```

- [x] **Step 2: 语法检查**

Run: `bash -n scripts/run/sdc_machine_scan.sh`
Expected: 无输出（语法 OK）

- [x] **Step 3: 本机全量执行（root 模式）**

```bash
cd /home/sdc/wangxu/sdcshield
SUDO_PW='SDC@2026' bash scripts/run/sdc_machine_scan.sh ~/sdc_campaign_2026-09-23/scan2
```

- [x] **Step 4: 验证产物与已知事实对照（必须全部一致）**

```bash
cat ~/sdc_campaign_2026-09-23/scan2/capabilities.env
grep -E 'SEL 高频|无本地内存|SVE|频率' ~/sdc_campaign_2026-09-23/scan2/machine_summary.md
```
Expected（与 2026-09-23 手工扫描一致）:
- `NCORES=96`、`NNODES=2`、`MEM_NODES="0"`、`HAS_SVE=0`、`HAS_CPUFREQ=0`、`HAS_IPMI=1`、`HAS_EDAC=1`、`CPU_FREQ_MHZ=2600`
- summary 含「SEL 高频重复故障」标记（#0x84）与「存在无本地内存的 node」警告

- [x] **Step 5: README 更新（模式 E，插在「模式 D」段之后）**

在 `README.md` 模式 D 代码块结束后追加：

```markdown
**模式 E：单板机器扫描 `scripts/run/sdc_machine_scan.sh`**——压测战役第 0 步，新单板可直接复用：一次性采集 DMI/BMC 传感器/SEL/NUMA 内存位置/RAS/软件栈，产出 `capabilities.env` 能力开关与异常标记（SEL 周期故障、无内存 node、无 SVE/cpufreq 等），战役参数据此自动适配：

```console
SUDO_PW=<密码> bash scripts/run/sdc_machine_scan.sh          # root 全量模式
bash scripts/run/sdc_machine_scan.sh                         # 无 root 降级模式
```
```

- [x] **Step 6: commit + push**

```bash
git checkout -b feat/sdc-campaign
git add scripts/run/sdc_machine_scan.sh README.md
git commit -m "feat(scripts): sdc_machine_scan.sh 单板软硬件全量扫描（战役第0步，可复用）"
git push -u origin feat/sdc-campaign
```

---

### Task 3: 战役执行器 `run_sdc_campaign.sh`（commit #2）

**Files:**
- Create: `scripts/run/run_sdc_campaign.sh`（可执行）
- Modify: `README.md`（模式 E 之后追加模式 F 一段）

**Interfaces:**
- Consumes: `./builddir/sdcshield`（Task 1 产物）；`--list-test-ids` 测试全集；`-s help` 引擎名（Task 1 Step 5 记录）；YAML schema（Task 1 Step 8 记录）。
- Produces: `$CAMPAIGN_DIR/{logs/*.yaml, monitor.csv, fails.log, commands.log, classifications.txt, campaign_summary.md, .done_*}`；退出码 0=全绿 / 1=有失败 / 5=有 sdc_suspect / 2=磁盘守卫触发。

- [x] **Step 1: 写入脚本（完整代码如下）**

```bash
#!/bin/bash
# ============================================================
# SDC 压测战役执行器 —— 24h+ 持续测试（文献激发规律驱动，新单板可复用）
#
# 依据：docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md §7.4 战役协议
#      docs/superpowers/plans/2026-09-23-sdc-campaign.md（方案与参数依据）
# 阶段：
#   P1 全量广域扫   --quality=$QUALITY 全测试 × 全核 × fracturing（seed 自动轮换）
#   P2 加权 soak    GEMM 尺寸/形态谱（×三调度）→ crypto → 压缩 level 谱 →
#                   SLEEF 足迹谱 → FFT 因子谱 → mesh 一致性 → 混合 × RNG 引擎
#   P3 固定 seed 驻留 --max-test-loop-count=0（关 fracturing，CORE179 单模式持续暴露）
#   P4 持续循环×N   { 广域扫重跑(--test-list-randomize，执行上下文多样性) +
#                    拓扑/并发档（全核/各 NUMA node/半 node：不同电流拉载）+
#                    轮换驻留 }
# 全程环境监测     ipmitool 传感器 + EDAC + SEL 计数 → monitor.csv（30s 采样）
# 失败处理         fail-continue：fail → 全核复跑 → -n 1 复跑 → 分类
#                  known_benign_ulp / full_core_only / sdc_suspect；
#                  可复现 suspect 自动逐核二分（CORE179 式定位）
# 断点续跑         每阶段 .done 标记；重启自动跳过已完成阶段
# 用法：
#   bash scripts/run/run_sdc_campaign.sh                    # 24h+ 正式战役
#   SMOKE=1 bash scripts/run/run_sdc_campaign.sh            # 冒烟（~10min）
#   bash scripts/run/run_sdc_campaign.sh --selftest-classify # 解析器自测
# 退出码：0 全绿 / 1 有失败 / 5 有 sdc_suspect / 2 磁盘守卫
# ============================================================
set -u

# ---------------- 参数（env 可覆盖） ----------------
SDC="${SDC:-./builddir/sdcshield}"
CAMPAIGN_DIR="${CAMPAIGN_DIR:-$(pwd)/campaign_$(date +%Y%m%d_%H%M%S)}"
SWEEP_TIME="${SWEEP_TIME:-20s}"        # P1/P4 每测试时长（SEVI: >80% 首错<10s → 2x 余量）
QUALITY="${QUALITY:-0}"                # 0 = BETA+PROD（纳入审计 D7 的 BETA 单元测试）
SOAK_TIME="${SOAK_TIME:-8m}"           # P2 主档
SOAK_TIME_S="${SOAK_TIME_S:-4m}"       # P2 次档
DWELL_TIME="${DWELL_TIME:-30m}"        # P3 每负载驻留（4 负载 ≈ 2h）
CYCLE_DWELL_TIME="${CYCLE_DWELL_TIME:-1h}"
TOPO_TIME="${TOPO_TIME:-10m}"
CYCLES="${CYCLES:-5}"
MONITOR_INTERVAL="${MONITOR_INTERVAL:-30}"
RERUN_TIME="${RERUN_TIME:-60s}"
BISECT_TIME="${BISECT_TIME:-30s}"
PHASES="${PHASES:-p1,p2,p3,p4,summary}"
TEST_FILTER="${TEST_FILTER:-}"         # 逗号分隔，支持通配（冒烟/定向用）
SMOKE="${SMOKE:-0}"
SKIP_MONITOR="${SKIP_MONITOR:-0}"
MIN_DISK_KB=$((5*1024*1024))           # 磁盘守卫阈值 5G
KNOWN_FLAKY="eigen_svd_double eigen_sparse"   # CLAUDE.md: 全核 ULP 抖动，-n 1 必过
FAKE_PASS_RE='^(vmx_|isal_crc|crc32|fma$)'    # 审计 D1/D6/D11: 装饰性 pass（无检出意义）

if [ "$SMOKE" = 1 ]; then
    SWEEP_TIME=3s; SOAK_TIME=5s; SOAK_TIME_S=3s; DWELL_TIME=5s
    CYCLE_DWELL_TIME=5s; TOPO_TIME=5s; CYCLES=1; MONITOR_INTERVAL=5
    TEST_FILTER="${TEST_FILTER:-zstd19,openssl_sha,openblas_dgemm,sleef_neon,isal_igzip,pocketfft_fft,crc32,mesh_*}"
fi

# ---------------- 小工具 ----------------
NCORES=$(nproc)
tsec() { local v="${1%?}" u="${1: -1}"; case "$u" in
    s) echo $((v));; m) echo $((v*60));; h) echo $((v*3600));;
    *) echo $(( ${1:-0} / 1000 ));; esac; }
banner() { echo; echo "============================================================"; \
    echo "$(date '+%F %T')  $*"; echo "============================================================"; }

expand() { local out="" seg a b; IFS=',' read -ra segs <<< "$1"
    for seg in "${segs[@]}"; do
        if [[ "$seg" == *-* ]]; then a=${seg%-*}; b=${seg#*-}; out+="${out:+,}$(seq -s, "$a" "$b")"
        else out+="${out:+,}$seg"; fi
    done; printf '%s' "$out"; }

half_of() { [[ "$1" =~ ^[0-9]+-[0-9]+$ ]] || return 1
    local a=${1%-*} b=${1#*-}; echo "$a-$(( (a+b)/2 ))"; }

# ---------------- 目录与日志 ----------------
mkdir -p "$CAMPAIGN_DIR"
LOG="$CAMPAIGN_DIR/logs"; RERUN_DIR="$CAMPAIGN_DIR/rerun"; BISECT_DIR="$CAMPAIGN_DIR/bisect"
mkdir -p "$LOG" "$RERUN_DIR" "$BISECT_DIR"
FAILS_LOG="$CAMPAIGN_DIR/fails.log"; CMDLOG="$CAMPAIGN_DIR/commands.log"
STDOUT_LOG="$CAMPAIGN_DIR/sdcshield_stdout.log"
: > "$CMDLOG"; : > "$FAILS_LOG"; : > "$STDOUT_LOG"
: > "$CAMPAIGN_DIR/classifications.txt"
RERUN_SEQ=0

# ---------------- YAML 结果解析 ----------------
parse_results() { # parse_results <yaml> → 每行 "<testid> <result>"
    awk '
        /^[[:space:]]*(-[[:space:]]*)?(id|test|test_id):/ {
            line=$0; sub(/^[[:space:]]*(-[[:space:]]*)?(id|test|test_id):[[:space:]]*/,"",line)
            sub(/[[:space:]]*$/,"",line); tid=line }
        /^[[:space:]]*result:/ { if (tid!="") { print tid, $2; tid="" } }
    ' "$1"
}
has_fail() { parse_results "$1" | awk '$2=="fail"{f=1} END{exit !f}'; }
record() { echo "$(date '+%F %T') [CLASS] $1 → $2：$3" >> "$FAILS_LOG"
           echo "$1 $2" >> "$CAMPAIGN_DIR/classifications.txt"; }

# ---------------- 运行与失败分类 ----------------
run_sdc() { # run_sdc <yaml> <time> [args...]  返回 sdcshield 退出码
    local yaml="$1" t="$2"; shift 2
    local slack=$(( $(tsec "$t") + 600 ))
    echo "$(date '+%F %T') [cmd] $SDC $* -t $t -o $yaml" >> "$CMDLOG"
    timeout -k 60 "$slack" "$SDC" "$@" -t "$t" -o "$yaml" >> "$STDOUT_LOG" 2>&1
    local rc=$?
    [ $rc -ne 0 ] && echo "$(date '+%F %T') [exit=$rc] $*" >> "$FAILS_LOG"
    return $rc
}
go() { # go <yaml> <time> [args...] = run + 分类 + 磁盘守卫
    local yaml="$1" t="$2"; shift 2
    run_sdc "$yaml" "$t" "$@"
    classify_run "$yaml" "$(basename "$yaml" .yaml)"
    disk_guard
}
classify_run() { local yaml="$1" label="$2" tid res
    while read -r tid res; do
        [ "$res" = "fail" ] && classify_fail "$tid" "$label"
    done < <(parse_results "$yaml" | sort -u)
}
classify_fail() { # classify_fail <test> <label>
    local t="$1" label="$2" y y1
    RERUN_SEQ=$((RERUN_SEQ+1))
    y="$RERUN_DIR/r${RERUN_SEQ}_${t}_all.yaml"
    echo "$(date '+%F %T') [FAIL] $t（来自 $label）" >> "$FAILS_LOG"
    run_sdc "$y" "$RERUN_TIME" -e "$t" -n "$NCORES" --ignore-unknown-tests
    if ! has_fail "$y"; then
        record "$t" transient "全核复跑 pass（单次失败；SEVI 长尾数据点，继续观察后续 cycle）"; return; fi
    y1="$RERUN_DIR/r${RERUN_SEQ}_${t}_n1.yaml"
    run_sdc "$y1" "$RERUN_TIME" -e "$t" -n 1 --ignore-unknown-tests
    if ! has_fail "$y1"; then
        case " $KNOWN_FLAKY " in *" $t "*)
            record "$t" known_benign_ulp "CLAUDE.md 已知全核 ULP 抖动，-n 1 复现通过";;
        *) record "$t" full_core_only "仅全核失败、单线程通过（并发路径相关，持续观察）";; esac
        return; fi
    record "$t" sdc_suspect "全核与单线程均复现失败 → 启动逐核二分"
    bisect_per_core "$t"
}
bisect_per_core() { # CORE179 式逐核定位
    local t="$1" c y bad=""
    echo "$(date '+%F %T') [BISECT] $t 开始（${NCORES} 核 × ${BISECT_TIME}）" >> "$FAILS_LOG"
    for c in $(seq 0 $((NCORES-1))); do
        y="$BISECT_DIR/${t}_core${c}.yaml"
        run_sdc "$y" "$BISECT_TIME" -e "$t" --cpuset="$c" --ignore-unknown-tests
        has_fail "$y" && bad="$bad $c"
    done
    echo "$(date '+%F %T') [BISECT] $t 失败核:${bad:-无}" >> "$FAILS_LOG"
    [ -n "$bad" ] && echo "$t 失败核:$bad" >> "$CAMPAIGN_DIR/suspect_cores.txt"
}
disk_guard() { local kb; kb=$(df -P "$CAMPAIGN_DIR" | awk 'NR==2{print $4}')
    if [ "${kb:-0}" -lt "$MIN_DISK_KB" ]; then
        echo "$(date '+%F %T') [GUARD] 磁盘剩余 ${kb}KB < ${MIN_DISK_KB}KB，战役中止" >> "$FAILS_LOG"
        monitor_stop; exit 2; fi
}

# ---------------- 环境监测 ----------------
MONITOR_PIDFILE="$CAMPAIGN_DIR/.monitor_running"
monitor_start() {
    [ "$SKIP_MONITOR" = 1 ] && return 0
    command -v ipmitool >/dev/null 2>&1 || { echo "[monitor] ipmitool 缺失，仅 sysfs 通道"; }
    touch "$MONITOR_PIDFILE"
    MONITOR_CSV="$CAMPAIGN_DIR/monitor.csv"
    echo "timestamp;CPU1CoreRem;CPU2CoreRem;CPU1MEMTemp;CPU2MEMTemp;OutletTemp;InletTemp;Power;CPUPower;MEMPower;FAN2;FAN3;VDDAVS1;VDDAVS2;SEL_Entries;EDAC_CE;EDAC_UE;acpitz" > "$MONITOR_CSV"
    (
      while [ -e "$MONITOR_PIDFILE" ]; do
        ts=$(date '+%F %T'); row=""
        if command -v ipmitool >/dev/null 2>&1 && timeout 8 ipmitool sel info >/dev/null 2>&1; then
            s=$(timeout 8 ipmitool sensor list 2>/dev/null)
            g() { echo "$s" | awk -F'|' -v pat="^$1" '$1 ~ pat {gsub(/ /,"",$2); print $2; exit}'; }
            sel=$(timeout 8 ipmitool sel info 2>/dev/null | awk '/^Entries/{print $3}')
            row="$(g 'CPU1 Core Rem');$(g 'CPU2 Core Rem');$(g 'CPU1 MEM Temp');$(g 'CPU2 MEM Temp');$(g 'Outlet Temp');$(g 'Inlet Temp');$(g '^Power ');$(g 'CPU Power');$(g 'MEM Power');$(g 'FAN2 Speed');$(g 'FAN3 Speed');$(g 'CPU1 VDDAVS');$(g 'CPU2 VDDAVS');${sel:-na}"
        else
            row="na;na;na;na;na;na;na;na;na;na;na;na;na;na"
        fi
        ce=$(cat /sys/devices/system/edac/mc/mc0/ce_count 2>/dev/null || echo na)
        ue=$(cat /sys/devices/system/edac/mc/mc0/ue_count 2>/dev/null || echo na)
        tz=$(cat /sys/class/hwmon/hwmon*/temp*_input 2>/dev/null | paste -sd/ -)
        echo "$ts;$row;$ce;$ue;${tz:-na}" >> "$MONITOR_CSV"
        sleep "$MONITOR_INTERVAL"
      done
    ) &
    echo $! > "$CAMPAIGN_DIR/.monitor_pid"
}
monitor_stop() { rm -f "$MONITOR_PIDFILE"
    [ -f "$CAMPAIGN_DIR/.monitor_pid" ] && { kill "$(cat "$CAMPAIGN_DIR/.monitor_pid")" 2>/dev/null; rm -f "$CAMPAIGN_DIR/.monitor_pid"; }
    return 0; }

# ---------------- 测试集动态发现 ----------------
discover() {
    ALL_TESTS="$("$SDC" --list-test-ids 2>/dev/null || "$SDC" --list-tests 2>/dev/null)"
    [ -n "$ALL_TESTS" ] || { echo "FATAL: 无法从 $SDC 获取测试清单" >&2; exit 1; }
    tids() { echo "$ALL_TESTS" | grep -E "$1" | paste -sd, -; }
    GEMM_OBLAS=$(tids '^openblas_(d|s|z|c)gemm$')            # 调度样本1: OpenBLAS TSV110
    GEMM_OTHER=$(tids '^(acl_gemm|eigen_[a-z_]*mxm[a-z_]*|eigen_[a-z_]*gemm[a-z_]*)$')  # 样本2/3
    CRYPTO=$(tids '^openssl_(sha|sha3|sm3sm4)$')
    IPSEC_SAMPLE=$(echo "$ALL_TESTS" | grep -E '^ipsec' | head -6 | paste -sd, -)
    COMPRESS=$(tids '^(zstd[0-9a-z]*|zlib[a-z0-9_]*)$')
    MESH=$(tids '^mesh_')
    SLEEF=$(tids '^sleef_')
    DWELL_LOADS="openblas_dgemm sleef_neon isal_igzip pocketfft_fft openssl_sha3 zstd19"
    RNG_ENGINES="${RNG_ENGINES:-$( { "$SDC" -s help 2>&1 || true; } | tr '[:upper:]' '[:lower:]' \
        | grep -oE 'constant|lcg|aes' | sort -u | tr '\n' ' ')}"
    echo "[discover] 测试总数 $(echo "$ALL_TESTS" | wc -l)"
    echo "[discover] GEMM(OpenBLAS)=${GEMM_OBLAS:-无} GEMM(其他)=${GEMM_OTHER:-无} CRYPTO=$CRYPTO"
    echo "[discover] COMPRESS=${COMPRESS:-无} MESH=${MESH:-无} SLEEF=${SLEEF:-无} RNG=${RNG_ENGINES:-无}"
}

# ---------------- 拓扑档（新板自适应） ----------------
NODE_LISTS=()
for n in /sys/devices/system/node/node[0-9]*; do
    [ -d "$n" ] && NODE_LISTS+=("$(cat "$n/cpulist")")
done
TOPO_SETS=("")                                             # 档0: 全核（不传 --cpuset）
for nl in "${NODE_LISTS[@]}"; do TOPO_SETS+=("$(expand "$nl")"); done
[ ${#NODE_LISTS[@]} -ge 1 ] && { h=$(half_of "${NODE_LISTS[0]}" 2>/dev/null) && TOPO_SETS+=("$(expand "$h")"); }

# ---------------- -e 过滤（冒烟/定向） ----------------
ENABLE_ARGS=()
if [ -n "$TEST_FILTER" ]; then
    IFS=',' read -ra _tf <<< "$TEST_FILTER"
    for t in "${_tf[@]}"; do ENABLE_ARGS+=(-e "$t"); done
fi

# ---------------- 各阶段 ----------------
phase_p1() { [ -f "$CAMPAIGN_DIR/.done_p1" ] && return 0
    banner "P1 全量广域扫（--quality=$QUALITY 全核 -t $SWEEP_TIME，fracturing=seed 自动轮换）"
    go "$LOG/p1_sweep.yaml" "$SWEEP_TIME" --quality="$QUALITY" \
       --on-crash=context -vv --ignore-os-errors --ignore-timeout \
       --ignore-unknown-tests ${ENABLE_ARGS[@]+"${ENABLE_ARGS[@]}"}
    touch "$CAMPAIGN_DIR/.done_p1"
}
phase_p2() { [ -f "$CAMPAIGN_DIR/.done_p2" ] && return 0
    banner "P2 文献优先级加权 soak"
    local m tb L e n eng
    for m in 64 256 512 1024; do   # 2a 尺寸谱（L1→LLC/DRAM 足迹，Biswas 驻留）
        [ -n "$GEMM_OBLAS" ] && go "$LOG/p2_gemm_m${m}.yaml" "$SOAK_TIME" -e "$GEMM_OBLAS" \
            -O openblas_dgemm.mdim=$m -O openblas_sgemm.mdim=$m \
            -O openblas_zgemm.mdim=$m -O openblas_cgemm.mdim=$m --ignore-unknown-tests
    done
    [ -n "$GEMM_OTHER" ] && go "$LOG/p2_gemm_others.yaml" "$SOAK_TIME" -e "$GEMM_OTHER" --ignore-unknown-tests
    for tb in 0 1 2 3; do          # 2b 形态谱（转置×β 相位）
        go "$LOG/p2_gemm_tb${tb}.yaml" "$SOAK_TIME_S" -e openblas_dgemm \
            -O openblas_dgemm.transab=$tb -O openblas_dgemm.beta_permille=500 --ignore-unknown-tests
    done
    go "$LOG/p2_crypto.yaml" "$SOAK_TIME" -e "${CRYPTO}${IPSEC_SAMPLE:+,$IPSEC_SAMPLE}" --ignore-unknown-tests
    for L in 0 1 2 3; do           # 2d 压缩 level 谱（四套 match-finder 数据结构）
        go "$LOG/p2_igzip_L${L}.yaml" "$SOAK_TIME_S" -e isal_igzip -O isal_igzip.level=$L --ignore-unknown-tests
    done
    [ -n "$COMPRESS" ] && go "$LOG/p2_compress.yaml" "$SOAK_TIME_S" -e "$COMPRESS" --ignore-unknown-tests
    for e in 1024 16384 262144; do # 2e SLEEF 足迹谱
        go "$LOG/p2_sleef_e${e}.yaml" "$SOAK_TIME_S" -e sleef_neon -O sleef_neon.nelems=$e --ignore-unknown-tests
    done
    [ -n "$SLEEF" ] && go "$LOG/p2_sleef_all.yaml" "$SOAK_TIME_S" -e "$SLEEF" --ignore-unknown-tests
    for n in 4096 4099 6144 10000; do # 2f FFT 因子谱（pow2/质数/混合 radix）
        go "$LOG/p2_fft_n${n}.yaml" "$SOAK_TIME_S" -e pocketfft_fft -O pocketfft_fft.n=$n --ignore-unknown-tests
    done
    [ -n "$MESH" ] && go "$LOG/p2_mesh.yaml" "$SOAK_TIME" -e "$MESH" --ignore-unknown-tests  # 阿里 8/27 一致性型
    for eng in $RNG_ENGINES; do    # 2g 混合多样性轮 × RNG 引擎（MeRLiN 故障等价类）
        go "$LOG/p2_mixed_${eng}.yaml" "$SOAK_TIME_S" \
            -e openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip,openssl_sha3,zstd19 \
            -s "$eng" --ignore-unknown-tests
    done
    touch "$CAMPAIGN_DIR/.done_p2"
}
phase_p3() { [ -f "$CAMPAIGN_DIR/.done_p3" ] && return 0
    banner "P3 固定 seed 深驻留（--max-test-loop-count=0 关 fracturing，每负载 $DWELL_TIME）"
    local L i=0
    for L in openblas_dgemm sleef_neon isal_igzip pocketfft_fft; do
        i=$((i+1))
        go "$LOG/p3_dwell_${i}_${L}.yaml" "$DWELL_TIME" -e "$L" \
           --max-test-loop-count=0 --on-crash=context -vv --ignore-unknown-tests
    done
    touch "$CAMPAIGN_DIR/.done_p3"
}
phase_p4() { local c cidx=0
    local loads=(openblas_dgemm sleef_neon isal_igzip pocketfft_fft openssl_sha3 zstd19)
    for c in $(seq 1 "$CYCLES"); do
        [ -f "$CAMPAIGN_DIR/.done_p4c${c}" ] && continue
        banner "P4 cycle $c/$CYCLES：随机序广域扫 + 拓扑/并发档 + 轮换驻留"
        go "$LOG/p4c${c}_sweep.yaml" "$SWEEP_TIME" --quality="$QUALITY" --test-list-randomize \
           --on-crash=context -vv --ignore-os-errors --ignore-timeout \
           --ignore-unknown-tests ${ENABLE_ARGS[@]+"${ENABLE_ARGS[@]}"}
        local i=0 t cs
        for t in "${TOPO_SETS[@]}"; do   # 不同并发=不同电流拉载（无 cpufreq 板的 V/F 代理）
            i=$((i+1)); cs=""; [ -n "$t" ] && cs="--cpuset=$t"
            go "$LOG/p4c${c}_topo${i}.yaml" "$TOPO_TIME" \
               -e openblas_dgemm,sleef_neon,isal_igzip,openssl_sha3,zstd19 $cs --ignore-unknown-tests
        done
        local L=${loads[$(( (c-1) % ${#loads[@]} ))]}
        go "$LOG/p4c${c}_dwell_${L}.yaml" "$CYCLE_DWELL_TIME" -e "$L" \
           --max-test-loop-count=0 --on-crash=context -vv --ignore-unknown-tests
        touch "$CAMPAIGN_DIR/.done_p4c${c}"
    done
}
phase_summary() {
    banner "战役汇总"
    local y p f s
    {
        echo "# 战役汇总（$(date '+%F %T')）"
        echo
        echo "## 各运行结果"
        echo "| 运行 | pass | fail | skip |"
        echo "|---|---|---|---|"
        for y in "$LOG"/*.yaml; do
            [ -e "$y" ] || continue
            p=$(parse_results "$y" | awk '$2=="pass"' | wc -l)
            f=$(parse_results "$y" | awk '$2=="fail"' | wc -l)
            s=$(parse_results "$y" | awk '$2=="skip"' | wc -l)
            echo "| $(basename "$y") | $p | $f | $s |"
        done
        echo
        echo "## 失败分类（明细见 fails.log）"
        if [ -s "$CAMPAIGN_DIR/classifications.txt" ]; then
            awk '{print $2}' "$CAMPAIGN_DIR/classifications.txt" | sort | uniq -c
            echo; cat "$CAMPAIGN_DIR/classifications.txt"
            [ -f "$CAMPAIGN_DIR/suspect_cores.txt" ] && { echo "## 失败核定位"; cat "$CAMPAIGN_DIR/suspect_cores.txt"; }
        else echo "（无失败）"; fi
        echo
        echo "## 假通过（装饰性 pass，审计 D1/D6/D11，无检出意义）"
        echo "$ALL_TESTS" | grep -E "$FAKE_PASS_RE" | paste -sd, -
        echo
        echo "## 环境监测统计（monitor.csv）"
        if [ -s "$CAMPAIGN_DIR/monitor.csv" ]; then
            awk -F';' 'NR>1{n++; for(i=2;i<=NF;i++){v=$i+0
                if(v>mx[i])mx[i]=v; if(mn[i]==""||v<mn[i]||mn[i]=="na")mn[i]=(mn[i]=="na"?v:v)}
                if(NR==2)for(i=2;i<=NF;i++)mn[i]=$i+0}
                END{printf "样本数=%d\n",n; for(i=2;i<=NF;i++) if(i!=15) printf "col%d min=%s max=%s\n",i,mn[i],mx[i]}' "$CAMPAIGN_DIR/monitor.csv"
            local selcol sel_start sel_end
            selcol=$(head -1 "$CAMPAIGN_DIR/monitor.csv" | tr ';' '\n' | grep -n '^SEL_Entries$' | cut -d: -f1)
            sel_start=$(sed -n '2p' "$CAMPAIGN_DIR/monitor.csv" | cut -d';' -f"$selcol")
            sel_end=$(tail -1 "$CAMPAIGN_DIR/monitor.csv" | cut -d';' -f"$selcol")
            echo "SEL 条目: $sel_start → $sel_end（增量 $(( ${sel_end:-0} - ${sel_start:-0} ))）"
        else echo "（监测未启用）"; fi
    } > "$CAMPAIGN_DIR/campaign_summary.md"
    cat "$CAMPAIGN_DIR/campaign_summary.md"
}

# ---------------- 解析器自测 ----------------
selftest_classify() {
    local T; T=$(mktemp -d); local rc=0
    printf -- '- id: ta\n  result: pass\n- id: tb\n  result: fail\n- id: tc\n  result: skip\n' > "$T/mix.yaml"
    printf -- 'results:\n- test: td\n  result: fail\n- test: te\n  result: pass\n' > "$T/mix2.yaml"
    echo "[selftest] mix.yaml →（期望 ta pass / tb fail / tc skip）"; parse_results "$T/mix.yaml"
    [ "$(parse_results "$T/mix.yaml" | wc -l)" = 3 ] || rc=1
    echo "[selftest] mix2.yaml →（期望 td fail / te pass）"; parse_results "$T/mix2.yaml"
    [ "$(parse_results "$T/mix2.yaml" | wc -l)" = 2 ] || rc=1
    has_fail "$T/mix.yaml"  && echo "[selftest] has_fail(mix)=YES ✓" || { echo "[selftest] has_fail(mix)=NO ✗"; rc=1; }
    has_fail "$T/mix2.yaml" && echo "[selftest] has_fail(mix2)=YES ✓" || { echo "[selftest] has_fail(mix2)=NO ✗"; rc=1; }
    printf -- '- id: tf\n  result: pass\n' > "$T/ok.yaml"
    has_fail "$T/ok.yaml"   && { echo "[selftest] has_fail(ok)=YES ✗"; rc=1; } || echo "[selftest] has_fail(ok)=NO ✓"
    rm -rf "$T"; [ $rc = 0 ] && echo "[selftest] ALL PASS" || echo "[selftest] FAILED"
    exit $rc
}
[ "${1:-}" = "--selftest-classify" ] && selftest_classify

# ---------------- main ----------------
[ -x "$SDC" ] || { echo "FATAL: $SDC 不存在（先完成构建）" >&2; exit 1; }
banner "SDC 压测战役启动：$CAMPAIGN_DIR（cores=$NCORES cycles=$CYCLES）"
discover
monitor_start
trap 'monitor_stop' EXIT
for ph in ${PHASES//,/ }; do
    case "$ph" in
        p1) phase_p1;; p2) phase_p2;; p3) phase_p3;; p4) phase_p4;;
        summary) phase_summary;; *) echo "未知阶段 $ph" >&2;;
    esac
done
monitor_stop; trap - EXIT
banner "战役结束（总时长见 commands.log 首尾时间戳）"
grep -q 'sdc_suspect' "$CAMPAIGN_DIR/classifications.txt" 2>/dev/null && exit 5
[ -s "$CAMPAIGN_DIR/classifications.txt" ] && exit 1
exit 0
```

- [x] **Step 2: 语法检查 + 解析器自测**

```bash
bash -n scripts/run/run_sdc_campaign.sh            # 期望: 无输出
bash scripts/run/run_sdc_campaign.sh --selftest-classify   # 期望: ALL PASS（若失败，按 Task 1 Step 8 的真实 schema 修正 parse_results）
```

- [x] **Step 3: 冒烟（SMOKE=1 全流程，~10min）**

```bash
cd /home/sdc/wangxu/sdcshield
CAMPAIGN_DIR=/tmp/campaign_smoke SMOKE=1 bash scripts/run/run_sdc_campaign.sh
```
Expected:
- 退出码 0（或 1 且 classifications 里只有可解释项）
- `campaign_summary.md` 生成，各运行 pass/fail/skip 计数正确
- `monitor.csv` ≥ 20 行且 ipmi 列非 na
- `crc32` 在跑但被标进假通过清单（验证 FAKE_PASS_RE）

- [x] **Step 4: 断点续跑演练（杀掉重启）**

```bash
CAMPAIGN_DIR=/tmp/campaign_smoke PHASES=p1 bash scripts/run/run_sdc_campaign.sh &   # 启动后 10 秒 kill
sleep 10; pkill -f run_sdc_campaign.sh; sleep 2
CAMPAIGN_DIR=/tmp/campaign_smoke SMOKE=1 bash scripts/run/run_sdc_campaign.sh       # 期望: 跳过已完成阶段（.done 标记），不重复跑
```

- [x] **Step 5: README 更新（模式 F）**

在模式 E 段之后追加：

```markdown
**模式 F：24h+ 压测战役执行器 `scripts/run/run_sdc_campaign.sh`**——把模式 D 的两阶段协议扩展为 24h+ 持续测试（PinDrop 模式）：全量广域扫（fracturing seed 轮换）→ 文献优先级加权 soak → 固定 seed 深驻留 → N 轮循环（随机序重扫 + NUMA 拓扑/并发档 + 轮换驻留）；全程 ipmitool/EDAC/SEL 环境监测，fail-continue + 自动失败分类（known_benign_ulp / full_core_only / sdc_suspect）+ 可复现嫌疑自动逐核二分；`.done` 标记支持断点续跑：

```console
nohup setsid bash scripts/run/run_sdc_campaign.sh > campaign.log 2>&1 &   # 24h+ 正式
SMOKE=1 bash scripts/run/run_sdc_campaign.sh                            # 冒烟 ~10min
```
```

- [x] **Step 6: commit + push**

```bash
git add scripts/run/run_sdc_campaign.sh README.md
git commit -m "feat(scripts): run_sdc_campaign.sh 24h+ SDC 压测战役执行器（§7.4 协议可执行化）"
git push
```

---

### Task 4: 战役执行与监控（无 commit，24h+）

**Files:** 无仓库改动。产物在 `$CAMPAIGN_DIR`（仓库外，建议 `~/sdc_campaign_2026-09-23/`）。

**Interfaces:**
- Consumes: Task 1 的 `builddir/sdcshield`、Task 3 的战役脚本。
- Produces: 全部战役数据（logs/*.yaml、monitor.csv、fails.log、campaign_summary.md）——Task 5 报告的数据源。

- [ ] **Step 1: 脱离会话启动（24h+ 必须防会话中断）**

```bash
cd /home/sdc/wangxu/sdcshield
mkdir -p ~/sdc_campaign_2026-09-23
CAMPAIGN_DIR=~/sdc_campaign_2026-09-23/campaign \
  nohup setsid bash scripts/run/run_sdc_campaign.sh \
  > ~/sdc_campaign_2026-09-23/campaign.log 2>&1 &
echo $! > ~/sdc_campaign_2026-09-23/campaign.pid
```

- [ ] **Step 2: 周期监控（每 ~90-120 分钟检查一次）**

每次检查执行并记录到对话（中期简报）：
```bash
tail -5 ~/sdc_campaign_2026-09-23/campaign.log                 # 当前阶段进度
ls ~/sdc_campaign_2026-09-23/campaign/.done_* 2>/dev/null       # 已完成阶段
cat ~/sdc_campaign_2026-09-23/campaign/fails.log 2>/dev/null | tail -5
tail -3 ~/sdc_campaign_2026-09-23/campaign/monitor.csv          # 温度/功耗/SEL 当前值
df -h /home | tail -1                                           # 磁盘
```
关注点：
- 温度是否明显抬升（全核满载 vs 基线 234W/56°C）——这是「温度激发杠杆」的实测数据
- SEL #0x84 断言频率是否与负载相关（对比 campaign 前后增量）
- 任何 `[FAIL]`/`[CLASS]` 条目——sdc_suspect 出现时**战役继续不中断**（可复现性证据），在对话中立即报告用户

- [ ] **Step 3: 中期基线简报（P1-P3 完成后，约 T+7h）**

在对话中给出：P1 全量扫 pass/fail/skip 全景（含假通过扣除）、P2/P3 结果、温度/功耗曲线统计、SEL 增量、eigen flake 分类结果。

- [ ] **Step 4: 战役结束条件**

P4 全部 CYCLES 完成 + summary 生成。若中途出现 sdc_suspect：等战役完整跑完（跨 cycle 持续失败 = 强指纹，PinDrop：>71% 坏机器持续失败），再进入 Task 5 报告。

---

### Task 5: 输出报告（commit #3）

**Files:**
- Create: `docs/superpowers/output/2026-09-23-sdc-campaign-output.md`

**Interfaces:**
- Consumes: Task 2 扫描产物（machine_summary.md / capabilities.env）、Task 4 战役产物（campaign_summary.md / monitor.csv / fails.log / logs/）。
- Produces: 最终交付文档（用户指定的 output 目录，文件名 = plan 同名 + `-output` 后缀）。

- [ ] **Step 1: 汇总数据**

```bash
cat ~/sdc_campaign_2026-09-23/campaign/campaign_summary.md
wc -l ~/sdc_campaign_2026-09-23/campaign/monitor.csv
head -1 ~/sdc_campaign_2026-09-23/campaign/logs/p1_sweep.yaml
awk -F';' 'NR>1{if($9+0>mx){mx=$9+0;t=$1}}END{print "峰值功耗:",mx,"W @",t}' ~/sdc_campaign_2026-09-23/campaign/monitor.csv
awk -F';' 'NR>1{if($2+0>mx){mx=$2+0;t=$1}}END{print "CPU1 峰值结温:",mx/1000,"°C @",t}' ~/sdc_campaign_2026-09-23/campaign/monitor.csv   # 若 ipmitool 已是摄氏度则去掉 /1000
```

- [ ] **Step 2: 按以下结构写报告（全部数字来自实测，禁止预测值）**

1. **执行摘要**（一段话：机器、时长、测试规模、结论）
2. **机器画像**（扫描摘要 + 三项硬件异常：SEL #0x84 周期故障 / FAN3 0RPM / node1 无内存）
3. **激发规律 → 本机杠杆映射表**（从本计划复制 + 监测通道实测结果列）
4. **战役执行记录**（各阶段时长、运行数、环境数据：温度/功耗/SEL/EDAC 曲线统计）
5. **结果全景**（pass/fail/skip 计数、假通过扣除清单、失败分类明细、skip 理由分类）
6. **异常观察**（SEL 增量与负载相关性、温度抬升、EDAC、任何 transient/full_core_only）
7. **结论与健康判定**（诚实边界：「当前覆盖 × 24h+ 时长内未见/见到 SDC」；引用 PinDrop「一次 pass 不证明健康」）
8. **新单板复用指南**（scan 脚本 → capabilities.env → 有 cpufreq 板加 V/F 档、有 SVE 板加 SVE 负载、多内存 node 板拓扑档变化；战役参数怎么调）

- [ ] **Step 3: commit + push**

```bash
mkdir -p docs/superpowers/output
git add docs/superpowers/output/2026-09-23-sdc-campaign-output.md
git commit -m "docs(campaign): TG225 B1 96核 SDC 激发战役输出报告（24h+ 基线）"
git push
```

---

## 自验证清单（每任务通用，CLAUDE.md 强制）

- 代码任务：`bash -n` 0 错误；真实运行输出引用进对话；commit 前 `git diff --stat` 核对无意外文件
- 回归：战役冒烟里 `zstd19`/`crc32`/`mesh_*` pass 证明脚本对正常路径无破坏
- x86-64 不动：diff 只含 `scripts/run/`、`README.md`、`docs/`（无框架/测试代码改动）

## 执行期修订记录（2026-09-23 冒烟后，对应 Task 3 实现修正）

冒烟（SMOKE=1 全流程）实证发现并修正：

1. **`-t` 每测试语义**（计划算术错误）：P2 档位时长全部重算——2a GEMM 尺寸谱每测试 7m（4 测试×4 尺寸=112min）；2a2 eigen+acl 8 测试×2m；2b 形态谱 4×4m；2c crypto（3 openssl+3 ipsec 抽样）×5m；2d igzip 4×4m + 压缩族 8×3m；2e sleef 3×4m+全族 2×4m；2f fft 4×4m；2g mesh 改为 **asymm_distrib 聚焦子集**（~10 测试×2m，全族 39 个已在广域扫覆盖）+ 混合×2 引擎（Constant/AES，LCG 为全程默认引擎）×6 测试×2m。P2 总计 ≈4.5h。
2. **P4 拓扑档**：TOPO_TIME 10m→**2m**（5 测试×2m=10min/档 ×4 档）；CYCLES 默认 5→**4**。总计 ≈24.6h。
3. **`--list-test-ids` 为两列输出**（id+shortid）：动态发现取第一列；eigen GEMM 实际命名为 `eigen_gemm_*`（正则由 `eigen_.*gemm$` 修正为 `eigen_gemm_[a-z0-9_]+`，8 个测试命中）。
4. **RNG 引擎名首字母大写**（Constant/LCG/AES），默认引擎 LCG；显式档用 `Engine:seed` 格式。
5. **run_sdc 外层超时 slack** 600s→420s：mesh 失败后的复跑存在 ~10 分钟挂起形态（框架内部 retry 后不退出），slack 收紧减少浪费。
6. **跨核测试协议盲区（重要发现）**：mesh 类跨核测试 `-n 1` 一律 skip → 永远无法到达 sdc_suspect/逐核二分。冒烟实测 `mesh_upi_sse_asymm_distrib_int`（2026-09-22 刚修复的 3 个 starved asymm 之一，修后"真正开始验证"）与 `mesh_upi_avx2_asymm_distrib_int` 在 3-5s 档失败、60s 复跑一过一挂，归入 full_core_only 桶。补偿：P4 拓扑档（node0/node1/全核）提供 NUMA 归因；跨 cycle 重复失败 = 强信号，由报告汇总观察。
7. **免费收获**：YAML 每测试自带 `avg-freq-mhz`（实测 ~2.5GHz）——每条日志都在回答"CPU 主频"。

