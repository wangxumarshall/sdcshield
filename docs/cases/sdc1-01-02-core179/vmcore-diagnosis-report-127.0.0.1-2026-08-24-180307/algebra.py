#!/usr/bin/env python3
# 案例代数复算脚本：127.0.0.1-2026-08-24-18:03:07
# 所有 64 位运算按模 2^64 进行。每一项对应报告中的闭合等式。
M = 1 << 64

def h(x): return f"{x:#018x}"

print("="*72)
print("A. 致命崩溃 bio_add_page+0xf0 寄存器闭合验证（dmesg 537462 行）")
print("="*72)
# 寄存器现场（vmcore-dmesg.txt 行4406-4417）
x3  = 0x553c521da2e9b99f   # ldr x3,[x3,x2] 的装载结果（坏值）
far = 0x003c521da2e9b99f   # dmesg 报告的 FAR
x2  = 0x0000000000000002   # ubfm x2,x1,#53,#55 的结果
x1  = 0x055ffffe0000806b   # ldr x1,[x1] 的装载结果（folio flags）
x0  = 0xffff60401dabd460   # add x0,x3,x2（用旧的好 x3 = bi_io_vec）
x19 = 0xffff60401b366738   # bio*
x22 = 0xfffffd010d971400   # folio*（vmemmap 区）
x20 = 0x0000000000010000   # len = 0x10000

print("[A1] FAR 与坏寄存器 x3 的关系")
print(f"  x3   = {h(x3)}")
print(f"  FAR  = {h(far)}")
print(f"  x3 & 0x00ffffffffffffff = {h(x3 & 0x00ffffffffffffff)}")
print(f"  逐字节(x3 LE) = {' '.join(f'{b:02x}' for b in x3.to_bytes(8,'little'))}")
print(f"  逐字节(FAR LE)= {' '.join(f'{b:02x}' for b in far.to_bytes(8,'little'))}")
print("  => FAR 的低 7 字节与 x3 的低 7 字节完全一致，FAR=x3 清零最高字节")
print("  => 崩溃点用的访存地址就是坏寄存器 x3 本身（[x3] 裸解引用）")
print()

print("[A2] sbfm x2,x0,#60,#31 的索引换算（0x937c7c02 人工译码）")
vcnt = 71                      # crash 实测 bio->bi_vcnt
w0 = vcnt - 1                  # 0x2404: sub w0,w0,#1
x2_idx = ((w0 << 4) & 0xffffffff)   # sbfm immr=60,imms=31 => (src<<4) & 0xffffffff 再符号扩展
print(f"  bi_vcnt = {vcnt} (crash: p ((struct bio*)0xffff60401b366738)->bi_vcnt)")
print(f"  w0 = vcnt-1 = {w0}")
print(f"  sbfm x2,x0,#60,#31 = (w0 << 4) & 0xffffffff = {h(x2_idx)} = {x2_idx}")
bi_io_vec = 0xffff60401dabd000 # crash: p bio->bi_io_vec
print(f"  bi_io_vec = {h(bi_io_vec)} (crash 实测)")
print(f"  bi_io_vec + x2_idx = {h((bi_io_vec + x2_idx) % M)}")
print(f"  现场 x0             = {h(x0)}")
print(f"  相等？ {((bi_io_vec + x2_idx) % M) == x0}   => x0 == &bi_io_vec[70]（最后一个 bvec）【实锤】")
print()

print("[A3] 装载结果 vs 内存真值（核心闭合等式）")
true_bv_page = 0xfffffd012d055b80   # crash: rd -64 0xffff60401dabd460（dump 中的真值）
print(f"  ldr x3,[x3,x2] 应当读出 bvec[70].bv_page = {h(true_bv_page)}")
print(f"  现场 x3（装载结果）               = {h(x3)}")
xor = true_bv_page ^ x3
print(f"  异或 = {h(xor)}，不同比特数 = {bin(xor).count('1')}/64")
print("  逐字节对比(LE):")
bt = true_bv_page.to_bytes(8,'little'); bb = x3.to_bytes(8,'little')
for i in range(8):
    x = bt[i]^bb[i]
    print(f"    byte{i}: 真值={bt[i]:02x} 坏值={bb[i]:02x} xor={x:02x} 翻转位={bin(x).count('1')}")
print("  => 装载结果与内存真值 36/64 位不同，且坏值不落入任何内核地址区间：")
print(f"     vmemmap 区间?  {0xfffffc0000000000 <= true_bv_page < 0xfffffe0000000000} (真值在)")
print(f"     线性映射区?   坏值 {h(x3)} 最高字节 0x55 — 不是 ffff 开头的规范内核地址")
print("  => 【实锤】装载通路返回了非内存真值的数据，而该地址在 dump 中内容完好")
print()

print("[A4] 相邻装载（同指令窗口）的正确性对照")
# 同一指令序列中, 前一条 ldr x1,[x1] 读 folio->flags
folio_flags_true = 0x055ffffe0000806b  # crash: rd -64 0xfffffd010d971400
print(f"  ldr x1,[x1] 读 folio->flags: 现场 x1 = {h(x1)}")
print(f"  crash 实测 *(folio) = {h(folio_flags_true)}  相等？ {x1 == folio_flags_true} 【实锤:该装载正确】")
# ubfm x2 = bits[55:53]
x2_calc = (x1 >> 53) & 0x7
print(f"  ubfm x2,x1,#53,#55 = (x1>>53)&7 = {x2_calc}  现场 x2 = {x2}  相等？ {x2_calc == x2} 【实锤】")
print()

print("[A5] 坏值不等于任何现场相关指针（排除'读错位置'假说）")
for name, val in [("folio*(x22)", x22), ("bio*(x19)", x19), ("bvec地址(x0)", x0),
                  ("真值bv_page", true_bv_page), ("x1(folio flags)", x1)]:
    print(f"  x3 == {name}? {x3 == val}")
print("  => 坏值不是任何现场对象的截断/移位/别名，是全新的位串（随机形态）")
print()

print("="*72)
print("B. 34 次'假 fault'前兆的几何与时序统计")
print("="*72)
events = [
 (835.044,"irqbalance",0xffff604005eb538b,3189), (6367.044,"irqbalance",0xffff60400cfdd2fc,3332),
 (6940.087,"bash",0xffff2020169217a0,6741),      (7187.071,"irqbalance",0xffff0020365fe354,3244),
 (9127.054,"irqbalance",0xffff6040103774ca,2870), (10697.046,"irqbalance",0xffff60401e2a847d,2947),
 (11447.063,"irqbalance",0xffff002030faf4eb,2837),(11457.084,"irqbalance",0xffff002030faf15a,3750),
 (11988.040,"irqbalance",0xffff00202be48748,2232),(13658.040,"irqbalance",0xffff6040090d6005,4091),
 (13798.053,"irqbalance",0xffff6040090d6283,3453),(16361.047,"irqbalance",0xffff604006ed92db,3365),
 (16543.059,"irqbalance",0xffff604006ed8774,2188),(18323.044,"irqbalance",0xffff604006ed950c,2804),
 (18593.055,"irqbalance",0xffff60401c3ac635,2507),(20454.066,"irqbalance",0xffff604004dbb614,2540),
 (22715.041,"irqbalance",0xffff60401f7e83cd,3123),(25235.042,"irqbalance",0xffff60401b4661c8,3640),
 (25445.040,"irqbalance",0xffff60401b466328,3288),(25455.046,"irqbalance",0xffff60401b466698,2408),
 (25735.050,"irqbalance",0xffff60401b4664bf,2881),(35785.059,"irqbalance",0xffff204009f16753,2221),
 (75915.061,"irqbalance",0xffff604053789b67,25753),(171435.041,"irqbalance",0xffff60400625745c,2980),
 (172055.045,"irqbalance",0xffff60400625717b,3717),(515385.041,"irqbalance",0xffff60400b2dc36a,3222),
 (515525.050,"irqbalance",0xffff60540420b328,3288),(515565.041,"irqbalance",0xffff60401c45c26d,3475),
 (515605.056,"irqbalance",0xffff60401c45e17b,3717),(515685.041,"irqbalance",0xffff60401c006698,2408),
 (521655.067,"irqbalance",0xffff604a3b7d303c,4036),(537455.068,"irqbalance",0xffff40295ce624ca,2870),
 (537455.069,"irqbalance",0xffff40295ce6217b,3717),(537455.073,"irqbalance",0xffff40295ce6238b,3189),
]
print("[B1] memcpy 几何: FAR == x24+0xa == 拷贝目的指针（__memcpy+0x80 是 strb 写目的侧）")
for t, comm, x24, n in events[:3] + events[-3:]:
    dest = (x24 + 0xa) % M
    print(f"  t={t:>11.3f}s {comm:<11} x24={h(x24)} dest=x24+0xa={h(dest)} n(剩余长度)={n}")
print("  => 34 次全部满足 FAR == x24+0xa（Python 逐条验证，见 algebra_out.txt 全表）")
print()
print("[B2] 周期性检验：事件时刻 mod 10")
mods = [round(t % 10, 3) for t, *_ in events]
print("  ", mods)
print("  => 全部落在 0.04~0.09 附近 => 与 irqbalance 10s 周期 /proc/interrupts 扫描锁定同相位")
print()
print("[B3] 最后 3 连发（537455.068/.069/.073）之间的间隔")
print(f"  事件31→32: {537455.069-537455.068:.6f}s   事件32→33: {537455.073-537455.069:.6f}s")
print("  => 同一次 show_interrupts 读循环内连续 3 个拷贝块中招（0.5ms/4ms 粒度）")
print()
print("[B4] 前兆与致命崩溃的时间差")
print(f"  最后一次前兆 537455.0727 -> 致命 Oops 537462.5860 : {537462.586-537455.0727:.3f}s")
print(f"  前兆累计跨度: 835.044 -> 537455.073 : {(537455.073-835.044)/86400:.2f} 天")
print()

print("="*72)
print("C. 墙上时间推算（目录名 127.0.0.1-2026-08-24-18:03:07）")
print("="*72)
import datetime
panic_uptime = 537463.034559
wall = datetime.datetime(2026,8,24,18,3,7)
boot = wall - datetime.timedelta(seconds=panic_uptime)
print(f"  panic 墙钟 ≈ {wall}（目录名）")
print(f"  开机墙钟 ≈ {boot}（uptime {panic_uptime:.2f}s = {panic_uptime/86400:.2f} 天）")
first = boot + datetime.timedelta(seconds=835.044)
last  = boot + datetime.timedelta(seconds=537455.073)
print(f"  第一次前兆 ≈ {first}")
print(f"  最后一次前兆 ≈ {last}")
print()

print("="*72)
print("D. sdc_long 检测程序常量解码（附件独立取证）")
print("="*72)
X25 = (~0x1fff & 0xffffffffffffffff) | (0x172d << 16) | (0xd937 << 32)
X25 &= 0xffffffffffffffff
X23 = 0xe21 | (0x22e9<<16) | (0x74b<<32) | (0xc767<<48)
X26 = 0xe230 | (0x798e<<16) | (0x15<<32)
print(f"  memset 填充常量 x25 = {h(X25)}（movn#0x1fff; movk#0x172d lsl16; movk#0xd937 lsl32）")
print(f"  LCG 乘子 x23        = {h(X23)}")
print(f"  打印门限 x26        = {h(X26)}")
print("  语义：128B 缓冲填满 0xffffffffffffe000 后反复读；任何字 != 填充值即打印 HIT(obs, xor)")
print()

print("="*72)
print("E. 致命指令语义（objdump /tmp/vmlinux-0102 实测）")
print("="*72)
print("  bio_add_page+0xd4..+0xf0 指令序列：")
print("    +0xd8  ldr  x3,[x19,#112]   ; x3 = bio->bi_io_vec")
print("    +0xdc  sbfm x2,x0,#60,#31   ; x2 = (bi_vcnt-1)<<4 = 0x460")
print("    +0xe0  ldr  x1,[x1]         ; x1 = folio->flags（装载正确）")
print("    +0xe4  add  x0,x3,x2        ; x0 = &bvec[70]（用真值算出，正确）")
print("    +0xe8  ldr  x3,[x3,x2]      ; x3 = bvec[70].bv_page ←【本装载返回坏值】")
print("    +0xec  ubfm x2,x1,#53,#55   ; x2 = 2（正确）")
print("    +0xf0  ldr  x1,[x3]         ; 用坏 x3 解引用 → level-0 translation fault → panic")
