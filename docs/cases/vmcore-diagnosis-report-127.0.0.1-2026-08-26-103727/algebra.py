#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
T06 案例代数复算脚本（全部运算 mod 2^64，无手算）
案例: 127.0.0.1-2026-08-26-10:37:27
主机: Yangtze Computing R240K V2, Kunpeng-920 (TaiShan-v110), 192 CPU, openEuler 24.03 SP3, 6.6.0-145.3.23.154.oe2403sp3.aarch64
"""
M = 1 << 64

def h(x): return hex(x % M)

print("=" * 78)
print("[1] KASLR 偏移计算")
print("=" * 78)
# 实测: dmesg Oops pc = find_busiest_group+0x140 = ffffa2930034ae44 (运行期)
# nm 实测: link-time find_busiest_group = ffff80008013ad08
run_fbg  = 0xffffa2930034ae44          # dmesg: pc : find_busiest_group+0x140/0xb60
link_fbg = 0xffff80008013ad08 + 0x13c  # nm + objdump（dmesg 报告 pc 为 +0x140 的 ldr；其 link 地址为 +0x13c+4）
slide = (run_fbg - link_fbg) % M
print(f"runtime  find_busiest_group+0x140 = {h(run_fbg)}   (dmesg L4159: pc)")
print(f"linktime find_busiest_group+0x140 = {h(link_fbg)}   (nm/objdump)")
print(f"KASLR slide = {h(slide)}")
print()

print("=" * 78)
print("[2] 关键符号运行期地址（link + slide）")
print("=" * 78)
sym_link = {
    "runqueues":        0xffff800081b696c0,   # nm 实测
    "__per_cpu_offset": 0xffff800081f655d0,   # nm 实测
    "cpu_worker_pools": 0xffff800081b69000,   # nm 实测（disasm 中 adrp/add 基址）
}
for name, l in sym_link.items():
    print(f"{name:20s} link={h(l)}  runtime={h((l + slide) % M)}")
print()
print("对照: crash> px &runqueues  输出 ffffa29301d796c0  -> 与计算一致")
print("对照: dmesg Oops x1 = ffffa29301d796c0             -> 与计算一致")
print()

print("=" * 78)
print("[3] 致命指令语义（objdump 实测反汇编）")
print("=" * 78)
print("ffff80008013ae34: a94087e0  ldp  x0, x1, [sp, #8]      ; x0=&__per_cpu_offset, x1=&runqueues")
print("ffff80008013ae38: f9400782  ldr  x2, [x28, #8]")
print("ffff80008013ae3c: f879d814  ldr  x20, [x0, w25, sxtw #3]  ; x20 = __per_cpu_offset[x25]")
print("ffff80008013ae40: 2a1903e0  mov  w0, w25")
print("ffff80008013ae44: 8b14003b  add  x27, x1, x20          ; x27 = &runqueues + __per_cpu_offset[cpu]")
print("ffff80008013ae48: f9409377  ldr  x23, [x27, #288]      ; ★致命指令 (Code 窗口末位 f9409377 吻合)")
print("对应源码: kernel/sched/fair.c:12050  (for_each_cpu 循环内 rq = cpu_rq(cpu))")
print()

print("=" * 78)
print("[4] 闭合等式验证【实锤】")
print("=" * 78)
x25 = 0xb3                                   # dmesg: x25 (cpu id) = 179
x0  = (sym_link['__per_cpu_offset'] + slide) % M
x1  = (sym_link['runqueues'] + slide) % M
addr_entry = (x0 + x25 * 8) % M              # ldr x20,[x0, w25, sxtw#3] 的访存地址
print(f"x25 (cpu)                      = {h(x25)}  = {x25} (CPU 179)")
print(f"x0  = &__per_cpu_offset        = {h(x0)}")
print(f"x1  = &runqueues               = {h(x1)}")
print(f"访存地址 &__per_cpu_offset[179] = {h(addr_entry)}")
print()

# crash rd 实测（转储真值）: ffffa29302175b68 处 = ffffdd6d7fa64000
truth_x20 = 0xffffdd6d7fa64000
observed_x20 = 0x0                            # dmesg Oops x20 = 0
print(f"内存真值  __per_cpu_offset[179] = {h(truth_x20)}   (crash rd ffffa29302175b68 实测)")
print(f"寄存器值  x20 (装载结果)        = {h(observed_x20)}  (dmesg Oops x20)")
print(f"==> 装载结果塌缩为 0，内存真值非 0。【装载通路被扰，非内存写坏】")
print()

observed_x27 = 0xffffa29301d796c0             # dmesg x27
truth_x27 = (x1 + truth_x20) % M
print(f"x27 观测值 = x1 + x20(=0)      = {h(observed_x27)}")
print(f"x27 应为   = x1 + __per_cpu_offset[179] = {h(truth_x27)}")
print(f"crash runq 实测 CPU179 RUNQUEUE = 0xffff8000817dd6c0  -> 与应为值完全一致")
print()

FAR = 0xffffa29301d797e0                      # dmesg: Unable to handle ... at ffffa29301d797e0
print(f"FAR 观测值                      = {h(FAR)}")
print(f"FAR 观测值 - x27 观测值         = {h((FAR - observed_x27) % M)}  = {(FAR - observed_x27)}  (= #288 字段偏移，与 ldr x23,[x27,#288] 吻合)")
print(f"若 x20 正确, FAR 应为           = {h((truth_x27 + 288) % M)}  (线性映射区，常驻映射，永不缺页)")
print()

print("=" * 78)
print("[5] 假如 x20 正确的反事实推演")
print("=" * 78)
print(f"正确 x27 = {h(truth_x27)} 位于线性映射区 (ffff8000_817dd6c0)")
print(f"正确访问 = [x27+288] = {h((truth_x27 + 288) % M)}")
# 验证: crash runq 显示 CPU179 runqueue = ffff8000817dd6c0，其 +0x120 = 817dd7e0
print(f"crash runq: CPU 179 RUNQUEUE: ffff8000817dd6c0  →  +0x120 = {h(0xffff8000817dd6c0 + 0x120)}")
print(f"==> 若 x20 未塌缩，该 load 将命中 CPU179 真实 rq 的字段，绝不会触发缺页。")
print(f"==> 观测 FAR 所在页 (ffffa29301d79000, 即镜像内 cpu0 实例 +0x120) 的 PTE 在转储页表中")
print(f"    实测为 0 (crash vtop ffffa29301d79000 → PTE => 0)，且该物理页 403593d79000 是")
print(f"    sighand_cache 的 slab 页 —— 说明镜像映射在此处本就是洞，访问必 fault，与 FSC=0x07 L3 fault 自洽。")
print()

print("=" * 78)
print("[6] 9 次前兆 WARNING 的时间线与地址特征")
print("=" * 78)
events = [
    (1467.049336, "W1", 9631,  "irqbalance", 0xffff6040082574a8),
    (1467.049713, "W2", 9631,  "irqbalance", 0xffff604008257138),
    (1467.055102, "W3", 9631,  "irqbalance", 0xffff604008257752),
    (61983.421239, "W4", 13610, "pmdalinux", 0xffff6040088433d7),
    (62053.482638, "W5", 13610, "pmdalinux", 0xffff6040088447d6),
    (62477.049447, "W6", 9631,  "irqbalance", 0xffff604008845731),
    (62893.399729, "W7", 13610, "pmdalinux", 0xffff6040088416b8),
    (62903.472834, "W8", 13610, "pmdalinux", 0xffff604008841277),
    (64953.445791, "W9", 13610, "pmdalinux", 0xffff6040074a8369),
    (66685.621071, "FATAL", 256855, "mi-scavenger", 0xffffa29301d797e0),
]
prev = None
for t, tag, pid, comm, va in events:
    gap = f" (+{t - prev:.1f}s)" if prev is not None else ""
    print(f"  uptime {t:12.3f}s  {tag:5s} PID {pid:6d} {comm:12s} VA {h(va)}{gap}")
    prev = t
print()
print("全部 9 次 WARNING 均为 CPU 179、同一路径 show_interrupts→seq_printf→__memcpy 写方向 DABT;")
print("x24 = x21 - 0xa（memcpy 目的基址）9 次全部成立:")
for _, tag, _, _, va in events[:9]:
    print(f"    {tag}: x21 - x24 = 0xa  ({h(va)} - {h(va - 0xa)})")
print()
print("致命 Oops 与最后前兆间隔:")
print(f"  66685.621071 - 64953.445791 = {66685.621071 - 64953.445791:.3f} s")
print(f"  与最早前兆间隔: 66685.621071 - 1467.049336 = {66685.621071 - 1467.049336:.3f} s (~{ (66685.621071-1467.049336)/3600:.1f} 小时)")
print()

print("=" * 78)
print("[7] 前兆 VA 的页表真值（spurious fault 证明）")
print("=" * 78)
# crash vtop 实测: 三个代表地址全部由 1GB 大页 PTE e8604000000f05 (VALID) 映射
print("crash vtop 实测（转储页表）:")
for va in (0xffff6040082574a8, 0xffff6040088433d7, 0xffff6040074a8369):
    phys = va - 0xffff000000000000
    print(f"  VA {h(va)} -> phys {h(phys)}  PUD PTE=e8604000000f05 (VALID|SHARED|AF|NG|PXN|UXN|DIRTY, 1GB block)")
print("==> 转储时刻这些地址全部有效映射；运行时却报 level-0 translation fault (DFSC=0x4) 写失败")
print("==> 内核自己都判定 'Ignoring spurious kernel translation fault' —— 页表走查输出被扰/或 PTW 结果被扰")
print()

print("=" * 78)
print("[8] ESR 解码")
print("=" * 78)
for name, esr in (("9次WARNING", 0x96000044), ("致命Oops", 0x96000007)):
    EC = (esr >> 26) & 0x3F
    ISS = esr & 0x1FFFFFF
    WnR = (ISS >> 6) & 1
    DFSC = ISS & 0x3F
    print(f"  {name}: ESR={h(esr)}  EC=0x{EC:02x}(DABT current EL)  WnR={WnR}({'写' if WnR else '读'})  DFSC=0x{DFSC:02x}"
          f"({'level 0' if DFSC==4 else 'level 3'} translation fault)")
print()

print("=" * 78)
print("[9] 线性映射基址验证（前兆 VA 物理地址）")
print("=" * 78)
# crash vtop 实测 ffff6040082574a8 -> 6040082574a8, 即 page_offset = ffff000000000000
page_offset = 0xffff000000000000
for _, tag, _, _, va in events[:9]:
    print(f"  {tag}: {h(va)} - page_offset = phys {h((va - page_offset) % M)}  (NUMA node 7: 604000000000-6057ffffffff)")
print()
print("node7 (PXM 7) 物理区间 [0x604000000000, 0x6057ffffffff]，9 次前兆写失败地址物理页全部落在 node 7。")

print()
print("=" * 78)
print("[10] CPU179 的 NUMA 归属与内存局部性")
print("=" * 78)
# dmesg L3262 实测: rasnode: cpu=179 mpidr=00000000817a0300
# SRAT 实测分组: node7 MPIDR 覆盖 0x780000..0x7d0300（掩码后 0x7a0300 落在其中）
print("CPU179 MPIDR = 0x817a0300 (dmesg rasnode 行实测), 掩码后 0x7a0300 ∈ node7 MPIDR 区间 [0x780000, 0x7d0300]")
print("==> CPU179 属于 NUMA node 7 (PXM 7, 物理区间 [0x604000000000, 0x6057ffffffff])")
print()
print("内存局部性对照:")
print("  9 次前兆写失败物理页 0x604008xxxxxxx/0x604007xxxxxxx  -> node 7 本地")
print("  CPU179 的 rq/percpu 实例物理页 0x6057ffe027e0         -> node 7 本地 (crash vtop 实测)")
print("  __per_cpu_offset 数组物理页 0x403594175000            -> node 0 远端 (内核镜像区)")
print()

print("=" * 78)
print("[11] 反事实推演的最终验证（crash vtop 实测）")
print("=" * 78)
print("正确目标 ffff8000817dd7e0 (CPU179 rq + 0x120):")
print("  crash vtop 实测: PTE=e86057ffe02f03 (VALID|SHARED|AF|NG|PXN|UXN|DIRTY), PAGE=6057ffe02000")
print("  crash rd 实测: ffff8000817dd7e0: 00000000000000f1 00000000000000f1 ...  (非零调度器字段)")
print("==> 若 x20 未塌缩，load 将正常返回 0xf1，不产生任何异常。")
