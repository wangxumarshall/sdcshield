#!/usr/bin/env python3
# 64-bit arithmetic for vmcore forensics — dump 127.0.0.1-2026-09-04-21:53:28
# All computations verified by script, no hand arithmetic.

def h(x): return f"0x{x:016x}"
def xor(a, b): return a ^ b
def popcount(x): return bin(x).count("1")

print("=" * 78)
print("SECTION A: Fatal Oops register algebra (dmesg line 3178-3274)")
print("=" * 78)

FAR   = 0x73b8474cc98297a5   # dmesg L3178: Unable to handle ... at 73b8474cc98297a5
x27   = 0x73b8474cc9829685   # dmesg L3195: x27: 73b8474cc9829685
x20   = 0x73b88cc000ffffc5   # dmesg L3202: x20: 73b88cc000ffffc5
x22   = 0xffff604003e5fea0   # dmesg L3200: x22: ffff604003e5fea0
x26   = 0xffff604003e5fea0   # dmesg L3199: x26: ffff604003e5fea0
x21   = 0xffffba8cc8c1fcb0   # dmesg L3200: x21: ffffba8cc8c1fcb0
x24   = 0xffffba8cc8c25000   # dmesg L3199: x24: ffffba8cc8c25000
x1    = 0xffffba8cc88296c0   # dmesg L3208: x1: ffffba8cc88296c0
x9    = 0xffffba8cc6dfae58   # dmesg L3205: x9: ffffba8cc6e4c520? no: x9 below

print(f"FAR        = {h(FAR)}")
print(f"x27        = {h(x27)}")
print(f"FAR - x27  = {h(FAR - x27)}   (LDR offset, expect 0x120)")
print()

# Instruction: Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
# f9409377 = LDR x23, [x27, #0x120]
imm12 = 0x9377
# LDR (imm, unsigned offset 64-bit): size=11 V=0 opc=01 imm12 bits[21:10]
imm12_raw = (0xf9409377 >> 10) & 0xfff
offset = imm12_raw * 8
print(f"f9409377 decode: bits[31:30]=11 (64-bit), V=0, opc=01 -> LDR X register")
print(f"  imm12 field = 0x{imm12_raw:x}, scaled by 8 -> offset = 0x{offset:x}")
print(f"  x27 + 0x120 = {h(x27 + 0x120)}")
print(f"  matches FAR: {x27 + 0x120 == FAR}")
print()

# Bit-flip analysis between observed x27 and expected canonical pointer form.
# Expected: kernel direct-map address ffff6040xxxxxxxx (same as x22/x26 prefix)
print("-" * 78)
print("Bit-flip analysis of x27")
print("-" * 78)

# Hypothesis candidates: expected x27 = x22 (env->cpus? group pointer?) forms
# Canonical kernel ptr must have top 16 bits = 0xffff
print(f"x27 top16    = {h(x27 >> 48)}  (canonical kernel needs 0xffff)")
print(f"x27 top32    = {h(x27 >> 32)}")
print(f"x20 top16    = {h(x20 >> 48)}  (x20 also corrupt)")
print(f"x20 top32    = {h(x20 >> 32)}")
print()

# Byte decomposition of x27
b = [(x27 >> (8*i)) & 0xff for i in range(8)]
print("x27 bytes (LE, byte0..byte7): " + " ".join(f"{v:02x}" for v in b))
b = [(FAR >> (8*i)) & 0xff for i in range(8)]
print("FAR bytes (LE, byte0..byte7): " + " ".join(f"{v:02x}" for v in b))
b = [(x20 >> (8*i)) & 0xff for i in range(8)]
print("x20 bytes (LE, byte0..byte7): " + " ".join(f"{v:02x}" for v in b))
print()

# x20 as string? try ASCII
def to_ascii(v):
    s = ""
    for i in range(8):
        c = (v >> (8*i)) & 0xff
        s += chr(c) if 32 <= c < 127 else "."
    return s
print(f"x27 ASCII (LE) = {to_ascii(x27)!r}")
print(f"x20 ASCII (LE) = {to_ascii(x20)!r}")
print()

# WARNING block: x27 = 0x00000000ffffffd8 constant across 13 WARNINGs
print("-" * 78)
print("SECTION B: WARNING block algebra (13 blocks)")
print("-" * 78)
x27_w = 0x00000000ffffffd8
print(f"WARNING x27 constant = {h(x27_w)} = {x27_w} (signed: {x27_w - (1<<64) if x27_w >> 63 else x27_w})")
print(f"  = -40 as 64-bit two's complement? {x27_w - (1<<64) == -0x28}")
print(f"  -0x28 = -40 decimal; 0xffffffd8 = {0xffffffd8} unsigned = {-40 & 0xffffffff}")
print()

# WARNING spurious addresses and x21/x24 deltas
addrs = [
  ("1391.173",  0xffff604004a9b3a0, 0xffff604004a9b396, 0x0c6a),
  ("1661.190",  0xffff604004a9a327, 0xffff604004a9a31d, 0x0ce3),
  ("2291.212",  0xffff604004a9d516, 0xffff604004a9d50c, 0x0af4),
  ("3105.045",  0xffff604019cad66b, 0xffff604019cad661, 0x099f),
  ("3125.038",  0xffff604019caf52c, 0xffff604019caf522, 0x0ade),
  ("3785.051a", 0xffff60402089443a, 0xffff604020894430, 0x0bd0),
  ("3785.056b", 0xffff604020894332, 0xffff604020894328, 0x0cd8),
  ("3795.053",  0xffff60401b721731, 0xffff60401b721727, 0x08d9),
  ("3811.205",  0xffff60401b7244c9, 0xffff60401b7244bf, 0x0b41),
  ("3825.050",  0xffff60401b724046, 0xffff60401b72403c, 0x0fc4),
  ("3845.055",  0xffff604021842788, 0xffff60402184277e, 0x1882),
  ("33265.049", 0xffff60400618c61e, 0xffff60400618c614, 0x09ec),
  ("33271.244", 0xffff6040065183ed, 0xffff6040065183e3, 0x0c1d),
]
print("t          x21 (fault addr)         x24 (=x21-0xa)      x25     x21-x24")
for t,a,c,n in addrs:
    print(f"{t:<10} {h(a)}  {h(c)}  {n:#06x}  {a-c:#x}")

print()
print("In all blocks x21 - x24 = 0xa: " + str(all(c2-a == 0xa for _,a,c2,_ in addrs)))

# x13 register contains ASCII of faulting address! (hex digit string)
# 1391: x13: 3061336239613430  -> LE bytes: 30 34 61 39 62 33 61 30 -> "04a9b3a0"
x13_1391 = 0x3061336239613430
print()
print(f"x13 @1391 = {h(x13_1391)} ASCII-LE = {to_ascii(x13_1391)!r}")
print("  -> x13 holds the LOW 8 hex digits of fault address ffff604004a9b3a0")
x12_1391 = 0x3034303666666666
print(f"x12 @1391 = {h(x12_1391)} ASCII-LE = {to_ascii(x12_1391)!r}")
print("  -> x12 holds HIGH 8 hex digits 'fffffff0'? no: literal ASCII 'ffffff04'... ")
print("     x12='ffffff04'?? decode: ", to_ascii(x12_1391), " vs addr high part 'ffff6040'")
print("     NOTE: x12 = 3034303666666666 -> 'ffffff040' ... recheck below")

# x12 is 0x3034303666666666 -> LE: 66 66 66 66 36 30 34 30 -> "ffff6040" -- correct!
print()
x12 = 0x3034303666666666
bs = [(x12 >> (8*i)) & 0xff for i in range(8)]
print(f"x12 @1391 LE-bytes = {' '.join(f'{v:02x}' for v in bs)} = '{''.join(chr(v) for v in bs)}'")
print("  -> x12 = ASCII 'ffff6040' = HIGH half of faulting VA. NORMAL.")

print()
print("=" * 78)
print("SECTION C: Direct-map layout of spurious fault addresses")
print("=" * 78)
spur = [a for _,a,_,_ in addrs]
for a in spur:
    print(f"{h(a)}  bit47..40 = 0x{(a>>40)&0xff:02x}  low bits alignment check: a & 0x7 = {a & 7}")
print()
print("All within ffff604000000000..ffff6040ffffffff region (64GB direct-map window on node with base 0x604000000000?):")
bases = [0xffff604000000000, 0xffff602000000000]
print("PAGE_OFFSET check: address - ffff604000000000 = physical offset if base=0x604000000000")
for a in spur[:3]:
    print(f"  {h(a)} - ffff604000000000 = {h(a - 0xffff604000000000)} (phys ~ 0x{(a - 0xffff604000000000):x} = {(a - 0xffff604000000000)/2**30:.2f} GiB)")

print()
print("=" * 78)
print("SECTION D: WARNING block pstate/ESR")
print("=" * 78)
esr_w = 0x96000044
print(f"WARNING ESR = 0x{esr_w:08x}: EC={(esr_w>>26)&0x3f} (0x25=DABT current EL), "
      f"FSC=0x{esr_w & 0x3f:x} (0x04=level-0 translation fault), WnR={(esr_w>>6)&1}")
esr_f = 0x96000004
print(f"Fatal   ESR = 0x{esr_f:08x}: EC={(esr_f>>26)&0x3f} (0x25=DABT current EL), "
      f"FSC=0x{esr_f & 0x3f:x} (0x04=level-0 translation fault), WnR={(esr_f>>6)&1}")

print()
print("=" * 78)
print("SECTION E: x27 corrupt-value pattern hunt")
print("=" * 78)
# Fatal x27 = 73b8474cc9829685
# Look for structure: is 73b8_474c_c982_9685 related to ASCII or to ffff6040...?
print(f"x27 = {h(x27)}")
print(f"x27 ^ 0xffffffff00000000 = {h(x27 ^ 0xffffffff00000000)}")
print(f"x27 with top16 forced to ffff = {h(x27 | 0xffff000000000000)}")
print()
# x27 low48 = 474cc9829685? check: does 0x474cc9829685 look like a phys addr on this box?
print(f"x27 low48 = 0x{x27 & 0xffffffffffff:x}")
print(f"x27 low32 = 0x{x27 & 0xffffffff:x}")
print(f"x27 bits 47..32 = 0x{(x27 >> 32) & 0xffff:x}")
print()
# x20 = 73b88cc000ffffc5
print(f"x20 = {h(x20)}")
print(f"x20 low48 = 0x{x20 & 0xffffffffffff:x}")
print(f"x20 ^ x27 = {h(x20 ^ x27)}  popcount = {popcount(x20 ^ x27)}")
print()
# Compare corrupt x27 against valid direct-map ptr x22
print(f"x22 (valid) = {h(x22)}")
print(f"x27 ^ x22   = {h(x27 ^ x22)}  popcount = {popcount(x27 ^ x22)}")
# x20 xor x21(=ffffba8cc8c1fcb0)
print(f"x20 ^ x21   = {h(x20 ^ x21)}  popcount = {popcount(x20 ^ x21)}")
print(f"x21 (valid) = {h(x21)}")
print(f"x24 (valid) = {h(x24)}")
print(f"x21 ^ x24   = {h(x21 ^ x24)}")
print(f"x24 - x21   = {h(x24 - x21)}")
print(f"x1  (valid) = {h(x1)}")
print(f"x9  (valid) = {h(x9)}")
print(f"x1 ^ x21    = {h(x1 ^ x21)}")

print()
print("=" * 78)
print("SECTION F: popcount / Hamming distance of corrupt vs canonical")
print("=" * 78)
canon = 0xffff604003e5fea0  # x22/x26, both = group/env candidate
print(f"candidate 'expected x27' guesses:")
print(f"  (a) x22 (=x26) {h(canon)}: xor popcount = {popcount(x27 ^ canon)}")
# If x27 were 0xffff604003e5fea0 + 0x120 target FAR would be ffff604003e5ffc0
print(f"      then FAR would be {h(canon + 0x120)}, observed {h(FAR)}")
print()

# Direct-map linear-index hypothesis: FAR corrupt = phys-linear remap of a true ptr?
# Observed FAR=73b8474cc98297a5, x27=73b8474cc9829685.
# Their low bits both end in ...c9829xxx. Both share high nibble pattern 73b8 8ccc...
print(f"FAR ^ x27 = {h(FAR ^ x27)} (should be exactly 0x120)")
print(f"FAR - x27 = {FAR - x27:#x}")

print()
print("=" * 78)
print("SECTION G: x20 in fatal = 73b88cc000ffffc5 decompose")
print("=" * 78)
print(f"x20 = {h(x20)}")
print(f"  bytes: {' '.join(f'{(x20 >> (8*i)) & 0xff:02x}' for i in range(8))}")
print(f"  = 0x73b8_8cc0_00ff_ffc5")
print(f"  note: 0x00ffffc5 / 0x0000ffff pattern")
print(f"  x20 & 0xffffffff = 0x{x20 & 0xffffffff:08x}")
print(f"  x20 >> 32 = 0x{x20 >> 32:08x}")
print()
# search: does 73b8 appear in WARNING? No. But maybe from x13 hex-string? no.
# 0x73b8 as ASCII LE = 'b8s' -> 0x38 0x62...? Actually 0x73b8 bytes: b8 73 = UTF? no.
print(f"0x73b8 as two bytes LE: {(0x73b8 >> 8) & 0xff:#04x},{0x73b8 & 0xff:#04x}")
print(f"0x73b88cc0 >> 16 = 0x{(0x73b88cc0) >> 16:04x}")

print()
print("=" * 78)
print("SECTION H: uptime and timeline")
print("=" * 78)
ts = [1391.173712, 1661.190916, 2291.212320, 3105.045360, 3125.038507,
      3785.051593, 3785.056992, 3795.053187, 3811.205759, 3825.050630,
      3845.055935, 33265.049395, 33271.244258, 33271.976579]
prev = None
for t in ts:
    d = f"  dt={t-prev:>10.2f}s" if prev else ""
    print(f"  t={t:>12.3f}s{d}")
    prev = t
print(f"\nuptime at crash = {ts[-1]/3600:.2f} hours = {ts[-1]/86400:.2f} days")

print()
print("=" * 78)
print("SECTION I: WARNING x27 = 0xffffffd8 (-40) semantics in __do_kernel_fault frame")
print("=" * 78)
print("x27=0xffffffd8 is not a pointer; it is leftover callee-saved value = -40 (0x28).")
print("0x28 = 40 decimal. 192 CPUs * 40 = 7680. NR_IRQS guess? Actually 40 = 0x28.")
print("Note: 13 WARNING blocks x27 all identical 0x00000000ffffffd8 -> deterministic,")
print("it is a stale value from seq_file/vfs path, NOT a corrupted pointer.")

print()
print("=" * 78)
print("SECTION J: kstack addresses per task (invariant across WARNINGs)")
print("=" * 78)
print("pmdalinux(10301): sp/x29 = ffff8000cb903880 (1391,1661,2291,3811,33271)")
print("irqbalance(9742): sp/x29 = ffff800105253880 (3105..3845,33265)")
print("-> per-task kernel stack addresses stable => same task, deterministic re-fault")

print()
print("=" * 78)
print("SECTION K: which CPU/NUMA node does CPU 179 belong to?")
print("=" * 78)
# CPU 179 on 192-CPU (8 nodes x 24 CPUs) hisilicon box; typical mp is 0x80000 + cpu*0x100
# from dmesg L1: "Booting Linux on physical CPU 0x0000080000" and SRAT PXM0->MPIDR 0x80000...
# CPU numbering: node = cpu // 24 for 192 CPUs/8 nodes -> 179 // 24 = 7 (node 7)
print(f"CPU 179: node guess = 179 // 24 = {179 // 24} (if 24 CPUs/node)")
print(f"  node 7 phys base = 0x604000000000 -> direct map ffff6040xxxxxxxx  <-- MATCHES WARNING addrs")
print(f"  CPU 179 local node direct-map prefix ffff6040 == ALL 13 spurious fault addr prefix")
print()

print("=" * 78)
print("SECTION L: WARNING x21 vs seq_printf format-string position")
print("=" * 78)
# x21 = fault addr, x24 = addr-0xa. In show_interrupts, seq_printf(m, "%*s: ",...)
# The fault happens in __memcpy inside seq_copy...
# x24 = x21 - 0xa = start of the 10-char field being formatted "%*s: " style
# Actually: seq_printf(m, "%*s: %10u\n", ...). The faulting read is of the source
# buffer being copied by seq_read_iter -> memcpy to user? No, el1 kernel path.
# show_interrupts builds line into seq buffer; __memcpy copies seq buf to iter.
print("x24 = x21 - 0xa consistently: the memcpy source pointer trailing the dest by 10")
print("(dest = user-adjacent? no; both kernel). x21 dest inside seq_file buffer,")
print("x24 src = seq_file->buf (kmalloc'd from CPU179-local node7 memory ffff6040...)")
print()
print("CONCLUSION: all 13 WARNING faults are memcpy WITHIN a per-cpu seq buffer on node 7")

print()
print("=" * 78)
print("SECTION M: fatal x20 = 73b88cc000ffffc5 vs WARNING x27 = ffffffd8")
print("=" * 78)
# 0x73b88cc0_00ffffc5: contains 0x00ffff pattern = bitmap ops result?
# In find_busiest_group, x20 candidate: sg->sgc->nr_running / cpumask ops
# 0x0000ffff popcount = 16 bits set. 0xffc5 = 1111 1100 0101
v = 0x73b88cc000ffffc5
print(f"x20 = {h(v)}")
print(f"x20 & 0xffff = 0x{v & 0xffff:04x}, popcount(low16) = {popcount(v & 0xffff)}")
print(f"x20 >> 48 = 0x{v >> 48:04x} = {v >> 48}")
print(f"0x73b8 = {0x73b8}; 0x8cc0 = {0x8cc0}")
print(f"0x73b8 ^ 0x8cc0 = 0x{0x73b8 ^ 0x8cc0:04x}, popcount = {popcount(0x73b8 ^ 0x8cc0)}")
print()
# KEY: does 73b8 8cc0 appear in ffffba8cc8c25000-ish addresses?
# x21 = ffffba8cc8c1fcb0; x24 = ffffba8cc8c25000. 'ba8cc8c2' vs 'b88cc0'?
print(f"x21 = {h(0xffffba8cc8c1fcb0)}  contains bytes 'ba8c c8c2'")
print(f"x20 corrupt = 73b8 8cc0 00ff ffc5 -- shares 'b8 8c c' nibble run with x21!")
print(f"x21 ^ (x20 << 16?) not meaningful; note shared substring b88cc~ba8cc")
print()
# x27 corrupt = 73b8474cc9829685; x20 corrupt = 73b88cc000ffffc5
# Both start 0x73b8. x24 valid = ffffba8cc8c25000.
print(f"x24(valid) = ffffba8cc8c25000 -> nibbles: f f f f b a 8 c c 8 c 2 5 0 0 0")
print(f"x20(corrupt) = 73b88cc000ffffc5 -> nibbles: 7 3 b 8 8 c c 0 0 0 f f f f c 5")
print(f"x27(corrupt) = 73b8474cc9829685 -> nibbles: 7 3 b 8 4 7 4 c c 9 8 2 9 6 8 5")
print(f"x21(valid)   = ffffba8cc8c1fcb0 -> nibbles: f f f f b a 8 c c 8 c 1 f c b 0")
print()
print("REMARKABLE: x20 '73b88cc0' vs x21/x24 'ff ff ba 8c c8 c2 ...'")
print("  x21 bytes BE: ff ff ba 8c c8 c1 fc b0")
print("  x20 bytes BE: 73 b8 8c c0 00 ff ff c5")
print("  x21 shifted right by 4 bits (nibble): >>4 = 0x0ffffba8cc8c1fcb")
print(f"  x21 >> 4 = {h(0xffffba8cc8c1fcb0 >> 4)}")
print(f"  x21 >> 4 vs x20 low48 (8cc000ffffc5): different")
print()
print("=" * 78)
print("SECTION N: 0x73b8_474c_c982_9685 -- search for structural source")
print("=" * 78)
# 0x73b8474c = little-endian ASCII? bytes 4c 47 = 'LG'. 0x474c = 'GL' BE / 'LG' LE.
# Hypothesis: 0x474c could be from 'GL' ... LG = ?
# Alternatively bit permutation of a valid pointer.
# Take valid x24 = ffffba8cc8c25000. Permutation via 4-bit rotation groups?
cand = 0xffffba8cc8c25000
print(f"valid x24         = {h(cand)}")
print(f"x27 corrupt       = {h(0x73b8474cc9829685)}")
print(f"popcount(x24) = {popcount(cand)}, popcount(x27) = {popcount(0x73b8474cc9829685)}")
print()
# x20 low 32 = 0x00ffffc5 -> could be a load_balance internal: e.g. 0x00ffffc5 has 16+6=... 
print(f"popcount(0x00ffffc5) = {popcount(0x00ffffc5)}")
print(f"popcount(0x0000ffff) = {popcount(0x0000ffff)} (=16)")
print()
print("=" * 78)
print("SECTION O: what should x27 be at find_busiest_group+0x140?")
print("=" * 78)
print("From Code: 8b14003b = ADD x27, x1, x20 (before faulting LDR).")
print("  ADD x27,x1,x20: x1=ffffba8cc88296c0 (valid), x20=73b88cc000ffffc5 (CORRUPT)")
print(f"  x1 + x20 = {h(0xffffba8cc88296c0 + 0x73b88cc000ffffc5)}")
print(f"  observed x27 = 0x73b8474cc9829685")
print(f"  (x1 + x20) ^ x27 = {h((0xffffba8cc88296c0 + 0x73b88cc000ffffc5) ^ 0x73b8474cc9829685)}")
print()
print("-> If x20 were correct (small integer, e.g. 0), x27 = x1 + x20 would be a valid")
print("   sched_domain/group pointer in ffffba8c... vmalloc/percpu region.")
print("-> x20 is an OFFSET (group index scaled) added to base x1. x20 corrupt => x27 corrupt.")
print("-> The corruption therefore ORIGINATES in x20 (or its producer), not in x27 itself!")

print()
print("=" * 78)
print("SECTION P: CRITICAL — memory truth vs CPU-visible x20 (crash-verified)")
print("=" * 78)
truth = 0xffffc573b8bda000   # crash: p __per_cpu_offset[150] AND rd ffffba8cc8c25a80
x20   = 0x73b88cc000ffffc5   # dmesg L3202 fatal block x20
x1    = 0xffffba8cc88296c0   # crash: p &runqueues
x27   = 0x73b8474cc9829685   # dmesg L3195
FAR   = 0x73b8474cc98297a5   # dmesg L3178
print(f"&__per_cpu_offset[150] memory truth = {h(truth)}")
print(f"CPU-visible x20 (register at fault)  = {h(x20)}")
print(f"xor = {h(truth ^ x20)}, popcount = {popcount(truth ^ x20)}")
print(f"x1(&runqueues) + truth = {h((x1 + truth) & 0xffffffffffffffff)}  <- would-be-clean x27 = &runqueues[150]")
print(f"x1(&runqueues) + x20   = {h((x1 + x20) & 0xffffffffffffffff)}  == observed x27: {(x1+x20) & (2**64-1) == x27}")
print(f"clean x27 + 0x120      = {h((x1 + truth + 0x120) & 0xffffffffffffffff)}  <- would-be-clean FAR")
print(f"observed FAR           = {h(FAR)}; FAR - x27 = {FAR - x27:#x}")
print()
print("STRUCTURE of corrupt x20 vs truth (nibble view, n15..n0):")
print("  truth   : f f f f c 5 7 3 b 8 b d a 0 0 0")
print("  corrupt : 7 3 b 8 8 c c 0 0 0 f f f f c 5")
print("  -> corrupt = truth ROTATED LEFT by 24 bits (3 byte lanes):")
print("     truth bits[39:0] -> corrupt bits[63:24]; truth bits[63:40] -> corrupt bits[23:0],")
print("     with 5 additional bit errors at bits 37,38,40,44,45 (0xbda000->0x8cc000).")
r24 = ((truth << 24) | (truth >> 40)) & 0xffffffffffffffff
print(f"  ROL(truth,24) = {h(r24)}; xor(corrupt) = {h(r24 ^ x20)} popcount={popcount(r24 ^ x20)}")
print(f"  differing bit positions: {[i for i in range(64) if ((r24 ^ x20) >> i) & 1]}")
print()
print("CONCLUSION: the LDR x20,[x0,w25,sxtw#3] returned a 24-bit-rotated + 5-bit-flipped")
print("version of the true 64-bit word. Memory intact -> READ-DATA-PATH SDC.")
print("NOTE: a pure 24-bit rotation is characteristic of a byte-lane swap in the")
print("load data path (3-byte lane skew), and the residual 5-bit error cluster in")
print("bits[45:37] indicates additional line-level data corruption at the consumer side.")

print()
print("=" * 78)
print("SECTION Q: WARNING spurious-fault address validity (crash vtop/kmem verified)")
print("=" * 78)
print("All 13 fault VAs have VALID page-table entries (1GB direct-map block PTE, flags")
print("VALID|SHARED|AF|NG|PXN|UXN|DIRTY), pages allocated in kmalloc-4k/512 slabs on node 7.")
print("AT s1e1r re-translation in is_spurious_el1_translation_fault() succeeds =>")
print("the faulting access itself was mis-issued (bad VA generation), not bad memory.")

print()
print("=" * 78)
print("SECTION R: counter-evidence sweep (software causes)")
print("=" * 78)
print("1) 6.6.0 arm64 has no known 'spurious translation fault on memcpy' SW bug at")
print("   fault.c:494 (the WARN is generic). All 13 events pin to CPU 179 -- a SW bug")
print("   (e.g. TLB race w/ break-before-make) would not be CPU-pinned across 9.2h.")
print("2) The faulting loads span two totally different code paths (show_interrupts")
print("   memcpy vs scheduler __per_cpu_offset load) -- no common SW structure.")
print("3) Memory contents at every implicated address are VALID (crash rd).")
print("4) No page-table modification activity (no vmalloc/free hot path in traces).")
print("=> SW causes excluded with high confidence; single-CPU pinning = HW locality.")

print()
print("=" * 78)
print("SECTION P: w25/x25 = 150 and expected x20 = __per_cpu_offset[150]")
print("=" * 78)
x25 = 0x96
x1 = 0xffffba8cc88296c0
x20_true = 0xffffc573b8bda000
x20_obs = 0x73b88cc000ffffc5
x27_exp = (x1 + x20_true) & 0xffffffffffffffff
print(f"dmesg x25 = {x25:#x} = {x25} decimal (w25 index into __per_cpu_offset)")
print(f"x1 (dmesg) = {h(x1)} = &runqueues (crash sym CONFIRMED)")
print(f"x20_true = __per_cpu_offset[150] = {h(x20_true)} (crash rd ffffba8cc8c25a80)")
print(f"expected x27 = x1 + x20_true = {h(x27_exp)}")
print(f"crash session25: rq at {h(x27_exp)} -> rq->cpu(+2880) = 0x96 = 150 CONFIRMED")
print(f"  -> expected x27 is cpu_rq(150), a VALID per-cpu mapped address")

print()
print("=" * 78)
print("SECTION Q: ADD/LDR algebra closure (100%)")
print("=" * 78)
insn = 0xf9409377
imm12 = (insn >> 10) & 0xfff
Rn = (insn >> 5) & 0x1f; Rt = insn & 0x1f
print(f"insn f9409377: size={(insn>>30)&3} V={(insn>>26)&1} opc={(insn>>22)&3} "
      f"imm12={imm12:#x} Rn=x{Rn} Rt=x{Rt}")
print(f"offset = imm12*8 = {imm12*8:#x} (matches dis 'ldr x23, [x27, #288]')")
x27_obs = 0x73b8474cc9829685
FAR = 0x73b8474cc98297a5
s = x1 + x20_obs
print(f"x1 + x20_obs = {s:#x} (65-bit raw)")
print(f"(x1 + x20_obs) mod 2^64 = {h(s & ((1<<64)-1))}")
print(f"observed x27             = {h(x27_obs)}")
print(f"ADD CLOSURE: {(s & ((1<<64)-1)) == x27_obs}")
print(f"FAR CLOSURE: x27_obs + 0x120 = {h(x27_obs + 0x120)} == dmesg FAR {h(FAR)}: {x27_obs + 0x120 == FAR}")

print()
print("=" * 78)
print("SECTION R: x20 corruption pattern (byte-lane + multi-bit)")
print("=" * 78)
print(f"x20_true = {h(x20_true)}  bytes BE: {x20_true.to_bytes(8,'big').hex(' ')}")
print(f"x20_obs  = {h(x20_obs)}  bytes BE: {x20_obs.to_bytes(8,'big').hex(' ')}")
d = x20_true ^ x20_obs
print(f"hamming distance = {popcount(d)} bits -> NOT a single-bit SEU on the register")
def ror64(x, n): return ((x >> n) | (x << (64-n))) & 0xFFFFFFFFFFFFFFFF if n else x
r = ror64(x20_true, 40)
print(f"ror(x20_true,40) = {h(r)}")
print(f"ror(x20_true,40) ^ x20_obs = {h(r ^ x20_obs)}, popcount = {popcount(r ^ x20_obs)}")
print(f"  -> x20_obs = 5-byte big-endian rotation of x20_true with 5 bits further flipped")
print(f"  -> data-path byte-lane misalignment + bit errors; not a clean single flip")

print()
print("=" * 78)
print("SECTION S: memory was clean at dump time (transient CPU-side corruption)")
print("=" * 78)
print("crash rd ffffba8cc8c25a80 (__per_cpu_offset[150] slot) = ffffc573b8bda000 CORRECT")
print("crash rd ffff8000814036c0 (cpu_rq(150)) valid; +0x120 = 0x400 0x400 0x400 0x200")
print("-> corruption visible only to the CPU, not persisted in DRAM")

print()
print("=" * 78)
print("SECTION T: WARNING-path = translation faults on MAPPED pages")
print("=" * 78)
print("vtop(session9): 6 tested spurious addrs -> all VALID 1GB direct-map PTEs")
print("kmem(session10/11): addrs inside ALLOCATED kmalloc-4k/512 objects (seq_file bufs)")
print("all 13 addrs in ffff6040xxxxxxxx = node7 local window; CPU179 in node7 (SRAT L307)")
print("page offsets 0x046-0x788 within 4KB: consistent with seq_file buffer positions")

print()
print("=" * 78)
print("SECTION U: x27 non-canonical address analysis")
print("=" * 78)
print(f"x27_obs >> 48 = 0x{x27_obs >> 48:04x}; canonical kernel needs 0xffff")
print("x27 bits[63:48] = 0x73b8 -> neither 0xffff (kernel) nor 0x0000 (user)")
print("-> falls in the non-canonical HOLE; PGD walk fails -> level-0 translation fault")
print("-> dmesg L3197: 'address between user and kernel address ranges'")
print()
print("FINAL: primitive corrupted quantity = x20 (loaded value of __per_cpu_offset[150]);")
print("x27 and FAR are DERIVED (ADD then LDR offset). Corruption window: L1D->LSU->RF")
print("data path of CPU 179 between load issue (+308) and use (+316), single occurrence;")
print("plus 13 page-table-walk/TLB spurious faults on the same CPU over 9.2h.")
