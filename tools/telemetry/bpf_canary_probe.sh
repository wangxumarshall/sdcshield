#!/bin/bash
# bpf_canary_probe.sh — spurious translation fault canary 能力探测（v5 §6.5，M2 T6）
#
# 探测不落实现（T6 裁定）：探测结果决定后续升级路径——
#   SPURIOUS_CANARY=bpf   bpftrace 可用且 kprobe:do_translation_fault 可挂载
#   SPURIOUS_CANARY=dmesg 否则——诚实降级，非缺失：journal_watch.log 的
#                         dmesg/RAS 轮询路径 M1 起已运行（collector@ras 同管道）
# v5 §6.5：挂载前必须通过 BTF/kallsyms 能力探测目标符号和函数原型，不把某一
# 内核版本的 do_translation_fault 签名硬编码——探测不过即降级，绝不硬挂。
#
# 参考板实测（2026-09-26，Kunpeng920 / openEuler 6.6 / bpftrace v0.19.1）：
# bpftrace 仅 root 可用；BTF(/sys/kernel/btf/vmlinux) 在；do_translation_fault
# 在 kallsyms 与 available_filter_functions 均在，但 bpftrace -l 不列出该 kprobe
# （其余 kprobe 如 do_sys_openat2 可列）→ 不可安全挂载 → dmesg。
#
# 用法: sudo bash bpf_canary_probe.sh [数据根]    # 数据根缺省 ~/sdc-excite-reproduce
#   root 运行且 <数据根>/capabilities.env 存在时幂等更新其 SPURIOUS_CANARY= 行；
#   非 root 只打印结果不落盘（bpftrace 仅 root——非 root 探测恒 dmesg，落盘失真）。
#   探测结果永远 exit 0（结果本身是数据，不是失败）。
set -u
DATA_ROOT="${1:-${SDC_EXCITE_REPRODUCE_DIR:-$HOME/sdc-excite-reproduce}}"
CAPE="$DATA_ROOT/capabilities.env"

result=dmesg
reason=""
if ! command -v bpftrace >/dev/null 2>&1; then
    reason="bpftrace 未安装"
elif [ "$(id -u)" -ne 0 ]; then
    reason="非 root 运行（bpftrace 仅 root 可用）——root 复核后方可定论，不落盘"
elif ! bpftrace --info >/dev/null 2>&1; then
    reason="bpftrace --info 异常（工具不可用）"
elif bpftrace -l 'kprobe:do_translation_fault' 2>/dev/null | grep -q 'kprobe:do_translation_fault'; then
    result=bpf
    reason="kprobe:do_translation_fault 可挂载（BTF/kallsyms 能力探测通过）"
else
    reason="kprobe:do_translation_fault 不可挂载（bpftrace -l 不列出——v5 §6.5 能力探测不过）"
fi

line="SPURIOUS_CANARY=$result  # $(date '+%F') bpf_canary_probe: $reason"
echo "$line"

if [ "$(id -u)" -eq 0 ] && [ -f "$CAPE" ]; then
    sed -i '/^SPURIOUS_CANARY=/d' "$CAPE"        # 幂等：先删旧行再追加
    echo "$line" >> "$CAPE"
    echo "已更新: $CAPE"
fi
exit 0
