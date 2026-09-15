#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
代数复算脚本 — case 127.0.0.1-2026-09-03-18:25:12
所有 64 位运算均按模 2^64 计算, 不做任何手算。
数据来源:
  - vmcore-dmesg.txt 致命 Oops 寄存器块 (dmesg 行号见注释)
  - crash 会话真实输出: p __per_cpu_offset[12]/[179], px &runqueues,
    rd -64 ffffc9e8a40d5630 (per_cpu_offset[12] 内存真值), vtop, search
"""
M = 1 << 64

def hx(v):
    return f"0x{v & (M - 1):016x}"

def add(a, b):
    return (a + b) % M

def sub(a, b):
    return (a - b) % M

def bits_diff(a, b):
    d = (a ^ b) & (M - 1)
    return [63 - i for i in range(64) if (d >> i) & 1], bin(d).count('1')

print("=" * 78)
print("[1] 致命 Oops 寄存器闭合验证 (find_busiest_group+0x140, CPU179, rcu_sched)")
print("=" * 78)
# dmesg 原文寄存器 (行号 322246.4xx 块)
x1  = 0xffffc9e8a3cd96c0   # lr 上下文里的 per-cpu 基址 = &runqueues (crash px &runqueues 证实)
x20 = 0x00ffffb617dd3940   # dmesg: x20: 00ffffb617dd3940
x27 = 0x00ffc99ebbaad000   # dmesg: x27: 00ffc99ebbaad000
far = 0x00ffc99ebbaad120   # dmesg: Unable to handle kernel paging request at ...

print(f"x1  (=&runqueues, crash 证实)      = {hx(x1)}")
print(f"x20 (dmesg 观测值)                 = {hx(x20)}")
print(f"x27 (dmesg 观测值)                 = {hx(x27)}")
print(f"验证 x27 == x1 + x20 (mod 2^64):   {hx(add(x1, x20))}  → {'一致' if add(x1,x20)==x27 else '不一致'}")

# 指令解码: Code 窗口最后一条 (f9409377) = ldr x23, [x27, #288]
w = 0xf9409377
imm12 = (w >> 10) & 0xFFF
rn = (w >> 5) & 0x1F
rt = w & 0x1F
print(f"\nCode 窗口致命指令 (f9409377) 解码: ldr x{rt}, [x{rn}, #{imm12*8}]  → ldr x23, [x27, #288]")
print(f"验证 FAR == x27 + 288:             {hx(add(x27, 288))}  → {'一致' if add(x27,288)==far else '不一致'}")

print()
print("=" * 78)
print("[2] x20 的真值对照 (装载指令: ldr x20, [x0, w25, sxtw #3] @ find_busiest_group+312)")
print("=" * 78)
# 反汇编: x0 = x24 + 0x5d0 = ffffc9e8a40d55d0; w25 = x25 = 0xc = 12 (dmesg 寄存器)
x24 = 0xffffc9e8a40d5000
w25 = 0xc
load_addr = add(x24, 0x5d0) + w25 * 8
print(f"x24 (dmesg)                        = {hx(x24)}")
print(f"w25 (dmesg x25)                    = {w25}")
print(f"装载地址 = x24+0x5d0+w25*8         = {hx(load_addr)}")
print(f"crash rd 该地址的内存真值           = 0xffffb617dc4d6000  (=__per_cpu_offset[12], crash p 证实)")
x20_true = 0xffffb617dc4d6000
print(f"__per_cpu_offset[12] (crash p)     = {hx(x20_true)}")

pos, hd = bits_diff(x20, x20_true)
print(f"\nx20 观测值 vs 内存真值: 海明距离 = {hd} 位, 差异位(63..0) = {pos}")

print()
print("=" * 78)
print("[3] 反事实推演: 若 x20 正确")
print("=" * 78)
x27_correct = add(x1, x20_true)
print(f"x27_correct = x1 + __per_cpu_offset[12] = {hx(x27_correct)}")
print(f"crash vtop 0xffff8000801af6c0 → PA 0x37ffe2e6c0, PTE e80037ffe2ef03 (VALID|SHARED|AF|NG|PXN|UXN|DIRTY)")
print(f"→ 正确 x27 是【已映射】的内核地址 (CPU12 的 runqueue 实例), ldr x23,[x27,#288] 本应成功")
print(f"实际 x27 = {hx(x27)} (bits[63:48]=0x00ff, 介于用户/内核区之间) → level 0 translation fault")

print()
print("=" * 78)
print("[4] ALU 通路完好性验证")
print("=" * 78)
print(f"add x27,x1,x20 结果精确等于 x1+x20 (mod 2^64) → 加法器/ALU 输出与输入一致, ALU 通路无故障")
bad_exists_anywhere = False
print(f"crash search -t 16 00ffffb617dd3940 全转储内存搜索: 无命中 → 坏值不存在于任何已转储内存页")

print()
print("=" * 78)
print("[5] x20 坏值的结构分析")
print("=" * 78)
pc12  = 0xffffb617dc4d6000
pc179 = 0xffffb617ddb04000
pc119 = 0xffffb617dd30c000
for name, v in [("x20 观测值", x20), ("per_cpu_offset[12] 真值", pc12),
                ("per_cpu_offset[119] (栈上残留)", pc119), ("per_cpu_offset[179] (前兆x3值)", pc179)]:
    b = v.to_bytes(8, 'big')
    print(f"{name:32s} {hx(v)}  字节序: {' '.join(f'{x:02x}' for x in b)}")
print()
r179 = pc179 >> 8
print(f"per_cpu_offset[179] >> 8           = {hx(r179)}")
pos2, hd2 = bits_diff(x20, r179)
print(f"x20 vs (pc[179]>>8): 高 48 位完全一致, 仅低 16 位差异 (b040 vs 3940), 海明距离 = {hd2} 位")
print(f"x20 vs (pc[12]>>8) = {hx(pc12>>8)}: 高 40 位一致, 低 24 位差异")
pos3, hd3 = bits_diff(x20, pc12 >> 8)
print(f"  海明距离 = {hd3} 位")
print()
print("结论: x20 坏值 ≈ (某高址 per_cpu_offset 表项 >> 8) 的字节右移形态叠加低位多位翻转;")
print("      它既不等于任何表项, 也不是任何表项的纯移位 — 是装载通路返回的非真值数据。")

print()
print("=" * 78)
print("[6] 前兆事件物理地址聚类 (35 次 spurious translation fault)")
print("=" * 78)
far_list = [
    0xffff6040629fe5d1, 0xffff604017b3d164, 0xffff604017b3d0d5, 0xffff604017b3d3ab,
    0xffff604017b3d79f, 0xffff604017b3d2e5, 0xffff604017b3e450, 0xffff604017b3e584,
    0xffff604017b3e6fa, 0xffff604017b3b608, 0xffff604017b3b471, 0xffff604017b3e773,
    0xffff604017b3e4d4, 0xffff604017b3c00f, 0xffff604017b3c3d7, 0xffff604017b3c629,
    0xffff604017b3e7b5, 0xffff6040195f507d, 0xffff6040195f5450, 0xffff6040195f51fe,
    0xffff6040195f50ca, 0xffff6040195f52c4, 0xffff6040195f010c, 0xffff6040195f01b1,
    0xffff6040195f05a5, 0xffff6040195f44c9, 0xffff6040195f40d5, 0xffff6040195f4676,
    0xffff6040195f4818, 0xffff6040195f422a, 0xffff604083c486b8, 0xffff604083c48122,
    0xffff604083c48185, 0xffff604013441935, 0xffff604061374839,
]
print(f"前兆总数 = {len(far_list)}, 全部唯一 = {len(set(far_list))}")
node7 = all(0xffff604000000000 <= a <= 0xffff6057ffffffff for a in far_list)
print(f"全部落在 Node 7 线性映射区 (ffff6040_00000000..ffff6057_ffffffff): {node7}")
# 64MB 聚类
import collections
clusters = collections.defaultdict(list)
for a in far_list:
    clusters[a >> 26].append(a)
for k in sorted(clusters):
    base = k << 26
    print(f"  64MB 区 PA {base:#x}: {len(clusters[k]):2d} 次, 区内偏移 {min(clusters[k])-base:#x}..{max(clusters[k])-base:#x}")

print()
print("=" * 78)
print("[7] 时间线计算 (崩溃墙上时间 = 2026-09-03 18:24:21, uptime 322246s)")
print("=" * 78)
import datetime
crash_time = datetime.datetime(2026, 9, 3, 18, 24, 21)
boot = crash_time - datetime.timedelta(seconds=322246)
print(f"开机时刻 = {boot}")
events = [
    (71822.056502,  "首次前兆 (irqbalance, ffff6040629fe5d1)"),
    (142792.045508, "前兆簇A 6次 (irqbalance)"),
    (142835.314727, "前兆 3 次 (pmdalinux)"),
    (146862.038919, "前兆簇B 5次 (irqbalance)"),
    (159162.040683, "前兆簇C 3次 (irqbalance)"),
    (322215.057943, "前兆 (irqbalance, ffff604013441935)"),
    (322225.050797, "末次前兆 (irqbalance, ffff604061374839)"),
    (322246.221818, "致命 Oops (rcu_sched, find_busiest_group)"),
]
for t, desc in events:
    print(f"  uptime {t:>12.2f}s  墙钟 {boot + datetime.timedelta(seconds=t)}  {desc}")
print(f"\n首次前兆距开机: {(71822.056502)/3600:.2f} 小时")
print(f"致命崩溃距开机: {322246.221818/86400:.3f} 天")
print(f"末次前兆 → 致命崩溃间隔: {322246.221818 - 322225.050797:.2f} 秒")
print(f"前兆总窗口: {(322225.050797 - 71822.056502)/3600:.2f} 小时")

print()
print("=" * 78)
print("[8] 调度组 CPU 覆盖解码 (struct sched_group 0xffff604003e9e3c0 的 cpumask)")
print("=" * 78)
vals = [0xffff000000ffffff, 0x00ffffff000000ff, 0xffffffffffff0000, 0]
mask = 0
for i, v in enumerate(vals):
    mask |= v << (64 * i)
cpus = [i for i in range(256) if (mask >> i) & 1]
ranges = []
start = prev = None
for c in cpus:
    if start is None:
        start = prev = c
    elif c == prev + 1:
        prev = c
    else:
        ranges.append((start, prev)); start = prev = c
if start is not None:
    ranges.append((start, prev))
print(f"组内 CPU 数 = {len(cpus)}, 区间 = {ranges}")
print(f"组含 CPU12: {12 in cpus}, 含 CPU119: {119 in cpus}, 含 CPU179: {179 in cpus}")
print("→ 崩溃时循环遍历的调度组覆盖偶数节点 {0,2,4,6} (CPU 0-23/48-71/96-119/144-191),")
print("  属 NUMA 级域; CPU179 (Node7, 奇数节点) 在对侧 socket, 正在向该组做 newidle 负载均衡。")
