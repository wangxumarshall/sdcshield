#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
代数复算脚本 — 案例 127.0.0.1-2026-08-25-15:42:24
所有 64 位运算按模 2^64 执行。
数据来源:
  - vmcore-dmesg.txt 第 2792-2847 行 (致命崩溃寄存器现场)
  - vmcore-dmesg.txt 第 2580-2624 行 (t=1707s 前兆 WARNING 寄存器现场)
  - crash 会话实际输出 (p __per_cpu_offset[176], rd, vtop, sym runqueues)
运行: python3 algebra.py | tee algebra_out.txt
"""
M = 1 << 64

def h(x):
    return f"0x{x:016x}"

def add(*vals):
    s = 0
    for v in vals:
        s = (s + v) % M
    return s

def sub(a, b):
    return (a - b) % M

print("=" * 78)
print("A. KASLR 基址推算 (来自 crash dis 的 adrp 注释与 nm 符号表)")
print("=" * 78)
# nm: node_data = ffff800081f64dd0; crash dis: adrp x24, 0xffffa5aa9b9a5000 <node_data+560>
# node_data+560 的链接地址 = ffff800081f64dd0 + 0x230 = ffff800081f65000 (4K页对齐)
node_data_link = 0xffff800081f64dd0
node_data_p560_page_link = (node_data_link + 560) & ~0xFFF
x24_runtime = 0xffffa5aa9b9a5000
S = sub(x24_runtime, node_data_p560_page_link)
print(f"nm:  node_data                    = {h(node_data_link)}")
print(f"     node_data+560 所在页(链接)     = {h(node_data_p560_page_link)}")
print(f"crash: x24 (runtime)              = {h(x24_runtime)}")
print(f"KASLR 偏移 S                      = {h(S)}")
# 交叉验证: runqueues 链接 ffff800081b696c0 + S
runqueues_link = 0xffff800081b696c0
runqueues_rt = add(runqueues_link, S)
print(f"验证: runqueues 链接 {h(runqueues_link)} + S = {h(runqueues_rt)}")
print(f"crash sym runqueues 实测           = 0xffffa5aa9b5a96c0  → 一致: {runqueues_rt == 0xffffa5aa9b5a96c0}")
pco_link = 0xffff800081f655d0
pco_rt = add(pco_link, S)
print(f"验证: __per_cpu_offset 链接 {h(pco_link)} + S = {h(pco_rt)}")
print(f"crash sym __per_cpu_offset 实测    = 0xffffa5aa9b9a55d0  → 一致: {pco_rt == 0xffffa5aa9b9a55d0}")

print()
print("=" * 78)
print("B. 崩溃现场寄存器 (dmesg 2814-2823 行原文转录)")
print("=" * 78)
regs = {
    'x29': 0xffff8001f03fb8a0, 'x28': 0xffff8001f03fb830, 'x27': 0xffffa5aa9b5a96c0,
    'x26': 0xffff604003e99780, 'x25': 0x00000000000000b0, 'x24': 0xffffa5aa9b9a5000,
    'x23': 0x0000000000000400, 'x22': 0xffff604003e99780, 'x21': 0xffffa5aa9b99fcb0,
    'x20': 0x0000000000000000, 'x19': 0xffff8001f03fb930, 'x1':  0xffffa5aa9b5a96c0,
    'x0':  0x00000000000000b0,
}
for k in ['x27', 'x25', 'x24', 'x22', 'x21', 'x20', 'x19', 'x1']:
    print(f"  {k} = {h(regs[k])}")
FAR = 0xffffa5aa9b5a97e0
print(f"  FAR (dmesg 2792行)             = {h(FAR)}")

print()
print("=" * 78)
print("C. 闭合等式 1【实锤】: x27 == x1 + x20  (add x27,x1,x20 @find_busiest_group+316)")
print("=" * 78)
lhs = add(regs['x1'], regs['x20'])
print(f"  x1 + x20 = {h(regs['x1'])} + {h(regs['x20'])} = {h(lhs)}")
print(f"  x1 + x20 == x27 ? {lhs == regs['x27']}   (x20=0 → 加法退化为恒等, 等式成立)")
print(f"  结论: 异常帧中 x27 == x1 == &runqueues (percpu 模板地址), x20 在加法中贡献了 0")

print()
print("=" * 78)
print("D. 闭合等式 2【实锤】: FAR == x27 + 0x120  (ldr x23,[x27,#288] @find_busiest_group+320)")
print("=" * 78)
lhs2 = add(regs['x27'], 0x120)
print(f"  x27 + 0x120 = {h(lhs2)}  == FAR {h(FAR)} ? {lhs2 == FAR}")
print(f"  0x120 = 288 字节 = struct rq 内字段偏移 (由 Code: f9409377 反汇编译码: LDR x23,[x27,#288])")

print()
print("=" * 78)
print("E. 闭合等式 3【实锤】: x20 应为 __per_cpu_offset[176], 寄存器值与内存真值对照")
print("=" * 78)
# crash 实测 (会话 full_forensics): p __per_cpu_offset[176] → 0xffffda55e61ce000
pco176_truth = 0xffffda55e61ce000
# 装载指令: ldr x20,[x0,w25,sxtw#3], x0=&__per_cpu_offset, w25=0xb0=176
x0 = pco_rt
load_addr = add(x0, 176 * 8)
print(f"  装载地址 = &__per_cpu_offset + 176*8 = {h(load_addr)} (内核 .data, 已映射: PTE f8205091da5f03 VALID)")
print(f"  内存真值 __per_cpu_offset[176]       = {h(pco176_truth)}   (crash p 实测, 非零)")
print(f"  异常帧寄存器 x20                     = {h(regs['x20'])}   (全零!)")
diff = pco176_truth ^ regs['x20']
print(f"  真值 XOR 寄存器值                    = {h(diff)}  → 64 位全部塌缩为 0")
print(f"  塌缩位数 = {bin(pco176_truth).count('1')} 个 '1' 位全部丢失")

print()
print("=" * 78)
print("F. 反事实推演【实锤】: 若 x20 正确, x27 = rq(176), 装载不会缺页")
print("=" * 78)
correct_x27 = add(runqueues_rt, pco176_truth)
correct_far = add(correct_x27, 0x120)
print(f"  正确 x27 = &runqueues + __per_cpu_offset[176] = {h(correct_x27)}")
print(f"  正确 FAR = x27 + 0x120                        = {h(correct_far)}")
print(f"  crash vtop {h(correct_far)} 实测: PTE = e86057ffe68f03 (VALID|SHARED|AF|NG|PXN|UXN|DIRTY),")
print(f"  物理页 = 0x6057ffe68000 (NUMA 节点7) → 该地址本已映射, 装载本应成功")
print(f"  实际 FAR = {h(FAR)} (percpu 模板页, 位于 __init_begin..__init_end 内, post-boot 已解除映射,")
print(f"  vtop 实测 PTE = 0) → L3 translation fault (FSC=0x07)")
# 附: 崩溃时 env 真值 (crash p *(struct lb_env *)0xffff8001f03fb930)
dst_rq_179 = 0xffff8000817dd6c0
off179 = add(pco176_truth, 3 * 0x22000)
rq179 = add(runqueues_rt, off179)
print(f"  旁证: env->dst_rq(内存真值) = {h(dst_rq_179)}, 而 &runqueues+off[179] = {h(rq179)} → 一致,")
print(f"  说明 percpu 加法公式在本内核处处成立, 唯独本次 x20 装载塌缩")

print()
print("=" * 78)
print("G. 前兆事件 (t=1707.36s, CPU179, pmdalinux) 与致命事件的通路段核对")
print("=" * 78)
# 前兆寄存器 (dmesg 2591-2600): x3 = ffffda55e6234000
prec_x3 = 0xffffda55e6234000
print(f"  前兆帧 x3 = {h(prec_x3)}")
print(f"  __per_cpu_offset[179] = off[176] + 3*0x22000 = {h(off179)}  → 完全相等: {prec_x3 == off179}")
print(f"  (percpu 块间距 0x22000 = 34页*4096, 与 dmesg 'percpu: 34 4K pages/cpu' 一致)")
print(f"  前兆缺页地址 ffff00204f4e24a8 → 物理帧 0x204f4e24a8 → NUMA 节点3 (SRAT: 0x204000000000-0x2057ffffffff)")
print(f"  前兆与致命事件同在 CPU 179, 间隔 {(76809.255750-1707.360942)/3600:.2f} 小时")

print()
print("=" * 78)
print("H. 时间线 (墙上时间由 crash sys DATE=2026-08-25 15:41:38 与 uptime 秒数反推)")
print("=" * 78)
import datetime
panic_wall = datetime.datetime(2026, 8, 25, 15, 41, 38)
events = [
    (0,           "开机 (wall 2026-08-24 18:21:28)"),
    (1707.360942, "前兆: spurious kernel translation fault @ffff00204f4e24a8, CPU179, pmdalinux 读 /proc/interrupts"),
    (3155.085736, "audit backlog 超限开始 (持续~96s, 系统高负载期)"),
    (51302.385527, "l1d_disable 故障注入模块首次加载 (目标: cpu 179)"),
    (63485.448202, "l1d_disable 最后一次卸载"),
    (76809.255750, "致命: x20 零塌缩 → find_busiest_group L3 缺页 → panic → kdump"),
]
for t, ev in events:
    wall = panic_wall - datetime.timedelta(seconds=(76809.255750 - t))
    print(f"  uptime {t:>13.6f}s  wall≈{wall.strftime('%Y-%m-%d %H:%M:%S')}  {ev}")
print()
print(f"  前兆→致命间隔 = {76809.255750-1707.360942:.3f}s = {(76809.255750-1707.360942)/3600:.3f}h")
print(f"  l1d_disable 首次加载→致命 = {76809.255750-51302.385527:.3f}s = {(76809.255750-51302.385527)/3600:.3f}h")
print(f"  l1d_disable 最后卸载→致命 = {76809.255750-63485.448202:.3f}s = {(76809.255750-63485.448202)/3600:.3f}h")

print()
print("=" * 78)
print("I. 异常帧其余寄存器的自洽性核对 (排除寄存器文件大面积损坏的替代假设)")
print("=" * 78)
# x19 = &env (load_balance 栈帧): env->sd 读出 0xffff6040042e3c00 (crash p 实测)
# x22 = x26 = sd->groups = 0xffff604003e99780: crash rd 实测该处为合法 sched_group
#   {next=0xffff604003e99720, ref=1, group_weight=24, cores=24, sgc=0xffff604003e90c60, cpumask=...}
# x25 = 0xb0 = 176 ∈ 节点7 CPU 集合 (168-191), 与 sched_group cpumask 一致
# x24 = 0xffffa5aa9b9a5000 = __per_cpu_offset 所在页基址, 与 adrp 指令一致
# x21 = 0xffffa5aa9b99fcb0 = __cpu_online_mask 相关 (adrp+add 0xcb0)
print("  x19 = 0xffff8001f03fb930 → crash p *(struct lb_env *) 实测为合法 env (dst_cpu=179, sd=0xffff6040042e3c00)")
print("  x22 = x26 = 0xffff604003e99780 → crash rd 实测为合法 sched_group (ref=1, group_weight=24)")
print("  x25 = 176 → 节点7 (CPU168-191) 内合法 CPU 号, 与 x22 组的 cpumask 一致")
print("  x24 = 0xffffa5aa9b9a5000 → 与 adrp x24, node_data+560 页基址一致")
print("  结论: 除 x20 外, 其余寄存器与内存真值全部吻合 → 损伤面收敛到单次装载结果")
