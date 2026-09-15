#!/usr/bin/env python3
# 代数复算脚本 — case 127.0.0.1-2026-09-04-10:27:58 (CPU179 sftp-server fatal, rcu_sched precursors)
# 所有运算 mod 2^64, 全部输入来自真实命令输出（dmesg / crash）
M = 1 << 64

def h(x): return hex(x)

print("=" * 78)
print("[A] 致命崩溃闭合验证: find_busiest_group+0x140 (ldr x23,[x27,#288])")
print("=" * 78)
# 寄存器 (dmesg fatal Oops, line ~2709-2710)
x1  = 0xffffd99f13ae96c0   # ldp x0,x1,[sp,#8] 载入
x20 = 0x0                  # ldr x20,[x0,w25,sxtw#3] 载入结果 (w25=0x95=149)
x27 = 0xffffd99f13ae96c0   # Oops 报告的 x27
far = 0xffffd99f13ae97e0   # FAR
print("x1 + x20 =", h((x1 + x20) % M), " == x27(Oops):", h(x27), "->", (x1+x20)%M == x27)
print("x27 + 288 =", h((x27 + 288) % M), " == FAR:", h(far), "->", (x27+288)%M == far)
print("=> 闭合: FAR == (x1 + x20) + 288, x20 == 0 (零塌缩)")
print()

print("w25 = 0x95 =", 0x95, " => 索引 = __per_cpu_offset[149]")
print("w25*8 =", 0x95*8, "(字节偏移)")
print()

print("=" * 78)
print("[B] percpu 真值对照 (crash p __per_cpu_offset[N], px runqueues)")
print("=" * 78)
off = {
0:   18446645536092250112,
148: 18446645536112861184,
149: 18446645536113000448,
150: 18446645536113139712,
178: 18446645536117039104,
179: 18446645536117178368,
180: 18446645536117317632,
}
for k in sorted(off):
    print(f"__per_cpu_offset[{k:3d}] = {h(off[k])}")
print()
rq_sym = 0xffffd99f13ae96c0   # crash sym runqueues -> (D) runqueues
print("runqueues 符号地址(静态percpu模板) =", h(rq_sym))
print("rq179(px runqueues[179]) = ffff8000817dd6c0")
print("验证: rq_sym + off[179] =", h((rq_sym + off[179]) % M),
      " == ffff8000817dd6c0 ->", (rq_sym+off[179])%M == 0xffff8000817dd6c0)
print()
correct_x27 = (rq_sym + off[149]) % M
print("[反事实] 若 x20 = __per_cpu_offset[149] (真值):")
print("  x27 = runqueues + off[149] =", h(correct_x27))
print("  FAR = x27 + 288 =", h((correct_x27 + 288) % M), "(cpu149 的 rq, vtop 确认 PTE 有效: e86037fff1ef03)")
print("  => 该地址本应有效, 不应触发任何 fault")
print()

print("=" * 78)
print("[C] 前兆事件与致命事件的通路对照")
print("=" * 78)
p1 = 0xffff604003e54458; p2 = 0xffff604003e61280
print("前兆1 FAR =", h(p1), " = sched_group(ffff604003e54420)+56 = &cpumask[0] (kmalloc-96, Node7)")
print("前兆2 FAR =", h(p2), " = sched_group(ffff604003e61240)+56 = &cpumask[1] (kmalloc-96, Node7)")
print("g2+56 =", h(0xffff604003e61240+56), "; g1+56 =", h(0xffff604003e54420+56))
print()
print("时间间隔: 前兆1@2099.552069 -> 前兆2@2117.796119: %.6f s" % (2117.796119-2099.552069))
print("时间间隔: 前兆2@2117.796119 -> 致命@3951.160261: %.6f s" % (3951.160261-2117.796119))
print("时间间隔: 前兆1 -> 致命: %.6f s" % (3951.160261-2099.552069))
print()
print("三次事件 CPU 全部 = 179; 前兆 PID=16 rcu_sched, 致命 PID=293168 sftp-server(经newidle_balance)")
print()

print("=" * 78)
print("[D] ESR 解码")
print("=" * 78)
for name, e in [("前兆", 0x96000004), ("致命", 0x96000007)]:
    ec = (e >> 26) & 0x3f; fsc = e & 0x3f
    lvl = {4:0,5:1,6:2,7:3}.get(fsc)
    print(f"{name}: ESR={h(e)} EC={h(ec)} DABT(current EL) FSC={h(fsc)} = level {lvl} translation fault")
print()
print("致命 FSC=0x07: 页表走到 L3, pte=0 (dmesg pte=0000000000000000 与 crash vtop 一致)")
print("前兆 FSC=0x04: level 0 fault —— 但 crash vtop 显示该 VA 由 1GB PUD block (e8604000000f05 VALID) 覆盖,")
print("              PGD 条目 18006057fffe2403 有效且为 boot 期建立、从未撤销 => HW 走查结果与内存真值矛盾")
print()

print("=" * 78)
print("[E] cpumask 位图解码 (前兆受害对象)")
print("=" * 78)
m = 0x0080000000000000
bits = [i for i in range(64) if (m >> i) & 1]
print("g1 cpumask[2] = 0x0080000000000000 -> 置位cpu:", [64*2+b for b in bits])
m0 = 0xffff000000ffffff
bits0 = [i for i in range(64) if (m0 >> i) & 1]
print("g2 cpumask[0] = 0xffff000000ffffff -> word0覆盖cpu 0-23,32-47 (group_weight=120=NUMA级)")
print()
print("受害核: CPU149 (w25=0x95) 的 runqueue 读取; 崩溃核: CPU179")

print()
print("=" * 78)
print("[F] Code 窗口指令人工解码 (dmesg Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377))")
print("=" * 78)
def decode_ldr_uoff(insn):
    imm12=(insn>>10)&0xfff; rn=(insn>>5)&0x1f; rt=insn&0x1f
    return f"LDR x{rt}, [x{rn}, #{imm12*8}]"
print("f9400782 ->", decode_ldr_uoff(0xf9400782))
print("f879d814 -> LDR x20, [x0, w25, SXTW #3]  (寄存器偏移载入 __per_cpu_offset[w25])")
print("2a1903e0 -> MOV w0, w25 (orr别名)")
print("8b14003b -> ADD x27, x1, x20")
print("f9409377 ->", decode_ldr_uoff(0xf9409377), " <== 致命指令(pc=+0x140)")
print()
print("指令序列语义: x20 = __per_cpu_offset[cpu]; x27 = &runqueues + x20; x23 = *(x27+288)")
print("即 percpu 访问 cpu149 的 rq->??? (+288 处字段, 崩溃时 x20 载入塌缩为 0)")
print()
print("=" * 78)
print("[G] runqueues+288 字段含义")
print("=" * 78)
# struct rq dump from crash: offset 288 belongs to which field? rd rq179+288 = 0x280/0x27f/0x53
# sched_domain_shared unrelated; +288 in struct rq on this kernel = nr_running? We printed struct rq only partially.
# rd ffff8000817dd7e0 (rq179+288): 0000000000000280 000000000000027f 0000000000000053 0000000000000000
# These are plausible: 0x280=640, 0x27f=639, 0x53=83... looks like cpulist / avg fields.
# Not critical: fault is at pte-walk stage, value never returned.
print("rq179+288 内容: 0x280 0x27f 0x53 0x0 (来自 crash rd, 供参考)")
# 精确定位: struct -o rq -> cfs@[128]; struct -o cfs_rq -> avg@[128]; struct -o sched_avg -> load_avg@[32]
print("精确字段: struct rq cfs@128 + struct cfs_rq avg@128 + struct sched_avg load_avg@32 = 288")
print("=> runqueues+288 == rq.cfs.avg.load_avg (CPU149 的 CFS 平均负载, load_balance 聚合的目标字段)")
print("   rq179 真值: load_avg=0x280(640) runnable_avg=0x27f(639) util_avg=0x53(83) — 正常繁忙队列")

print()
print("=" * 78)
print("[H] 时间线墙钟换算 (crash sys DATE=2026-09-04 10:27:14, UPTIME=01:05:51=3951s)")
print("=" * 78)
from datetime import datetime, timedelta
boot_dt = datetime(2026,9,4,10,27,14) - timedelta(seconds=3951)
print("开机墙钟 = 10:27:14 - 3951s =", boot_dt.strftime("%H:%M:%S"))
for name, ts in [("前兆1 WARNING(CPU179 rcu_sched)", 2099.552069),
                 ("前兆2 WARNING(CPU179 rcu_sched)", 2117.796119),
                 ("致命 Oops(CPU179 sftp-server)", 3951.160261),
                 ("SMP stopping / kexec", 3951.583179)]:
    t = boot_dt + timedelta(seconds=ts)
    print(f"{name}: uptime {ts:>13.6f}s -> 墙钟 {t.strftime('%H:%M:%S.%f')}")
print("目录名 10:27:58 = kdump 落盘完成时间(+44s), 与 kexec 起跳 10:27:14.58 一致")

print()
print("=" * 78)
print("[I] 致命装载的直接实锤: ldr x20,[x0,w25,sxtw#3] (find_busiest_group+0x130)")
print("=" * 78)
# crash px &__per_cpu_offset = 0xffffd99f13ee55d0; dis +104..124: x1=adrp ffffd99f13ae9000+0x6c0=&runqueues
x0 = 0xffffd99f13ee55d0   # = &__per_cpu_offset (crash px 输出)
w25 = 0x95
print("x0(&__per_cpu_offset) =", hex(x0), "; w25 =", w25)
print("装载目标地址 = x0 + 149*8 =", hex((x0 + w25*8) % M), "= &__per_cpu_offset[149]")
print()
print("寄存器结果 (dmesg Oops): x20 = 0x0000000000000000")
print("内存真值 (crash p):     __per_cpu_offset[149] = 0xffffa6616d8f8000  <- 非零!")
print("数组页映射 (crash vtop ffffd99f13ee5000): PTE = f840448c0e5f03 VALID|AF|DIRTY")
print("数组内存 (crash rd ffffd99f13ee55d0 4): [0]=ffffa6616c52e000 [1]=ffffa6616c550000 ... 非零")
print()
print("=> x20 装载结果 0 与内存真值 0xffffa6616d8f8000 矛盾: 装载通路(load path)受扰,")
print("   非内存被写坏(内存真值完好), 非页表问题(该页 PTE 有效且可读)。")
print()
print("闭合链: x20=0 -> x27=&runqueues+0=ffffd99f13ae96c0 -> FAR=x27+288=ffffd99f13ae97e0")
print("        -> 静态percpu模板页 pte=0 (L3 fault) -> do_translation_fault -> do_bad_area -> Oops")
print()
print("反事实: 若 x20=真值, FAR=ffff8000813e17e0 (cpu149 rq.cfs.avg.load_avg), vtop PTE=e86037fff1ef03 有效,")
print("        该指令在正常内核每秒执行数千次(load_balance 热路径), 从不应崩溃。")

print()
print("=" * 78)
print("[J] 前兆装载语义核对 (_find_next_and_bit+0x18: ldr x3,[x0,x4,lsl#3])")
print("=" * 78)
g1 = 0xffff604003e54420; g2 = 0xffff604003e61240
p1 = 0xffff604003e54458; p2 = 0xffff604003e61280
print("前兆1: FAR =", hex(p1), "= g1+56 =", hex(g1+56), "(g1=sched_group ffff604003e54420) -> 读 cpumask word0, x4=0")
print("前兆2: FAR =", hex(p2), "= g2+56+8 (g2=sched_group ffff604003e61240) -> 读 cpumask word1, x4=1")
print()
print("前兆1 x26 = ffff604003e545a0 (= p1+0x148); 前兆2 x26 = ffff604003e611e0 (= g2->next)")
print("致命   x26 = ffff604003e611e0 (与前兆2 相同!) => 同一 sched_group 环, 同一代码路径")
print("前兆 x3 = ffffa6616dcf4000 = __per_cpu_offset[179] (本核偏移, 前兆中该装载正确)")
print()
print("交叉结论: 3 次事件(2099s/2117s/3951s)全在 CPU179 的 load_balance->find_busiest_group")
print("位图/percpu 读通路上: 2 次 PTW 读出腐化(spurious), 1 次装载结果塌缩为 0(致命)。")
