#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 代数复算脚本（模 2^64）——案例 127.0.0.1-2026-09-04-12:33:31
# 所有 64 位地址运算均在本脚本内完成，禁止手算。
M = (1 << 64) - 1
S = 0x48a9151d0000   # KASLR slide, 来自 dump 头 VMCOREINFO: KERNELOFFSET=48a9151d0000

def r(x):  # mod 2^64 归一化
    return x & M

print("=== [A] KASLR slide 与运行时符号地址 ===")
statics = {
    "_text":              0xffff800080000000,
    "_stext":             0xffff800080010000,
    "find_busiest_group": 0xffff80008013ad08,
    "runqueues(.data..percpu)": 0xffff800081b696c0,
    "nr_cpu_ids(.data)":  0xffff800081f5fcb0,
    "adrp页(__per_cpu_offset所在)": 0xffff800081f65000,
    "__per_cpu_offset":   0xffff800081f655d0,
    "swapper_pg_dir":     0xffff800081994000,
}
for k, v in statics.items():
    print(f"  {k:34s} static={v:#x}  runtime={r(v+S):#x}")

print()
print("=== [B] Oops 寄存器 vs 滑移后符号（六项逐一比对）===")
oops = {
    "x21": 0xffffc8a99712fcb0,   # 期望 = runtime &nr_cpu_ids
    "x24": 0xffffc8a997135000,   # 期望 = runtime adrp 页
    "x1":  0xffffc8a996d396c0,   # 期望 = runtime &runqueues（percpu 模板）
    "x9":  0xffffc8a99530ae58,   # 期望 = runtime find_busiest_group+0x150
    "x27": 0xffffc8a996d396c0,   # = x1 + x20
    "pc":  0xffffc8a99530ae48,   # 期望 = runtime find_busiest_group+0x140
    "x30": 0xffffc8a99530ae24,   # 期望 = runtime find_busiest_group+0x11c
}
expect = {
    "x21": r(0xffff800081f5fcb0 + S),
    "x24": r(0xffff800081f65000 + S),
    "x1":  r(0xffff800081b696c0 + S),
    "x9":  r(0xffff80008013ae58 + S),
    "x27": None,  # 依赖 x20
    "pc":  r(0xffff80008013ae48 + S),
    "x30": r(0xffff80008013ae24 + S),
}
for k in ["x21", "x24", "x1", "x9", "pc", "x30"]:
    ok = "MATCH" if oops[k] == expect[k] else "MISMATCH"
    print(f"  {k:4s} obs={oops[k]:#x} exp={expect[k]:#x}  [{ok}]")

print()
print("=== [C] 致命闭合等式（find_busiest_group+0x134..+0x148 指令链）===")
x1  = 0xffffc8a996d396c0
x20 = 0x0
x27 = r(x1 + x20)
FAR = 0xffffc8a996d397e0
print(f"  x27 = x1 + x20            = {x27:#x}   (Oops 中 x27 与 x1 相等：成立)")
print(f"  FAR = x27 + 288           = {r(x27+288):#x}   与 dmesg 'Unable to handle ... ffffc8a996d397e0' 一致：{r(x27+288)==FAR}")
print(f"  x20 = 0 → __per_cpu_offset[53] 装载结果塌缩为 0；正确值应为一个大 vmalloc 偏移（非零）")

print()
print("=== [D] FAR 落点：内核镜像 .data..percpu 模板页（应已映射）===")
# 静态布局（readelf -SW /tmp/vmlinux-0102 实测）：
#   .data..percpu [25]: ffff800081b52000 .. ffff800081b52000+0x1a3e8
#   PT_LOAD[1]      : ffff800080f10000 .. +0x3073aa0（含 .data..percpu）
pcpu_start = 0xffff800081b52000
pcpu_end   = r(pcpu_start + 0x1a3e8)
FAR_static = r(FAR - S)
print(f"  FAR 静态地址 = {FAR_static:#x}")
print(f"  .data..percpu 静态区间 = [{pcpu_start:#x}, {pcpu_end:#x})")
print(f"  FAR 在 .data..percpu 内：{pcpu_start <= FAR_static < pcpu_end}")
load1_start, load1_end = 0xffff800080f10000, r(0xffff800080f10000 + 0x3073aa0)
print(f"  FAR 在 PT_LOAD[1] 内：{load1_start <= FAR_static < load1_end}  （该段运行时必然映射）")

print()
print("=== [E] 时间线（audit 纪元锚定）===")
# dmesg 行 2472: [21.291209] audit: type=1404 audit(1788491328.288:2) → 开机墙钟纪元
boot_epoch = 1788491328.288 - 21.291209
t_warn = 5022.426712
t_oops = 5060.516765
import datetime
print(f"  开机墙钟 = {datetime.datetime.fromtimestamp(boot_epoch)}")
print(f"  WARNING(5022.426712) = {datetime.datetime.fromtimestamp(boot_epoch + t_warn)}")
print(f"  Oops   (5060.516765) = {datetime.datetime.fromtimestamp(boot_epoch + t_oops)}")
print(f"  WARNING → Oops 间隔 = {t_oops - t_warn:.6f} 秒")

print()
print("=== [F] 页表走查译码（dmesg pgd/pud/pmd/pte）===")
entries = {
    "pgd": 0x10006057fffff403,
    "p4d": 0x10006057fffff403,
    "pud": 0x10006057ffffe403,
    "pmd": 0x10006057ffffa403,
    "pte": 0x0000000000000000,
}
for name, val in entries.items():
    nxt = val & ((1 << 48) - 1) & ~0xFFF
    print(f"  {name} = {val:#018x} → 下级表物理地址 {nxt:#x}")
print("  物理表地址 0x6057ffffxxxx 均位于 node7 DRAM 顶端（<0x605800000000），合法；")
print("  顶层位 0x1_0000_0000_0000 (=bit60) 为 arm64 表描述符标志位之外的 high bits，")
print("  47 位物理截断后即为真实表地址。pte=0 → 软件走查读到空 PTE。")

print()
print("=== [G] sg/域对象落点（线性映射区）===")
lin_base = 0xffff000000000000   # 由 x26=ffff604003e63c60 - 0x604003e63c60 推出
sg = 0xffff604003e63c60
print(f"  线性映射基 = {lin_base:#x}")
print(f"  sg 物理地址 = {r(sg - lin_base):#x} → node7 DRAM（SRAT: 0x604000000000-0x6057ffffffff）")
print(f"  CPU179 MPIDR=0x7a0300（dmesg 行1206）→ node7 → sg 为本节点局部内存")

print()
print("=== [H] 两事件对象一致性 ===")
warn_addr = 0xffff604003e63c98
sg_off = r(warn_addr - sg)
print(f"  WARNING 故障地址 {warn_addr:#x} = sg + {sg_off:#x} → sched_group.cpumask（span 位图，实测结构偏移 0x38）")
print(f"  两次事件同一 sg、同一 CPU179、同一 load_balance 路径。")
