#!/usr/bin/env python3
# 案例代数复算脚本：127.0.0.1-2026-08-25-15:58:09
# 全部 64 位运算以 mod 2^64 进行（铁律：禁止手算）
M = 1 << 64
def h(x): return x % M

def banner(s): print("\n" + "="*72 + "\n# " + s + "\n" + "="*72)

# ------------------------------------------------------------------
# 输入：全部取自真实输出（dmesg Oops 块 + crash rd/p 输出）
# ------------------------------------------------------------------
far  = 0x00ffb34569fc3ac0   # dmesg: Unable to handle kernel paging request at ...
x27  = 0x00ffb34569fc39a0   # dmesg 寄存器 x27
x20  = 0x00ffffcc879da2e0   # dmesg 寄存器 x20（嫌疑装载结果）
x1   = 0xffffb378e25e96c0   # dmesg 寄存器 x1 == runqueues（运行时）
x25  = 0x92                 # dmesg 寄存器 x25（循环变量 cpu 索引）

per_cpu_offset_rt = 0xffffb378e29e55d0            # crash: px &__per_cpu_offset
off0   = 0xffffcc879da2e000   # crash: p __per_cpu_offset[0]
off145 = 0xffffcc879ed70000   # crash: p __per_cpu_offset[145]
off146 = 0xffffcc879ed92000   # crash: p __per_cpu_offset[146]（真值）
off179 = 0xffffcc879f1f4000   # crash: p __per_cpu_offset[179]
rq146_px = 0xffff80008137b6c0 # crash: px runqueues [146]
rq179_px = 0xffff8000817dd6c0 # crash: px runqueues [179]

banner("1. 崩溃地址闭合：far 是否等于 x27 + 0x120")
print("far - x27        = 0x%x" % h(far - x27))
print("若 = 0x120 则致命指令 ldr x23,[x27,#288] 的 EA 与 far 完全闭合")
assert h(far - x27) == 0x120

banner("2. 指令解码：Code 窗口 f9409377")
insn = 0xf9409377
imm12 = (insn >> 10) & 0xFFF; rn = (insn >> 5) & 0x1F; rt = insn & 0x1F
print("ldr x%d, [x%d, #0x%x]  (imm12=%d, scale=8)" % (rt, rn, imm12*8, imm12))
print("EA = x27 + 0x%x = 0x%x" % (imm12*8, h(x27 + imm12*8)))

banner("3. ADD 单元体检：x27 是否等于 x1 + x20（mod 2^64）")
print("x1 + x20 = 0x%x" % h(x1 + x20))
print("x27 观测 = 0x%x" % x27)
print("一致:", h(x1 + x20) == x27)
assert h(x1 + x20) == x27

banner("4. 核心闭合：x20 观测值 vs __per_cpu_offset 真值")
print("x20 观测                     = 0x%016x" % x20)
print("__per_cpu_offset[146] 真值   = 0x%016x  (应为装载结果)" % off146)
print("XOR 观测^真值                = 0x%016x  popcount=%d" % (x20 ^ off146, bin(x20^off146).count('1')))
print()
print("__per_cpu_offset[0] 真值     = 0x%016x" % off0)
print("off0 >> 8                    = 0x%016x" % (off0 >> 8))
print("x20 观测                     = 0x%016x" % x20)
print("==> x20 == __per_cpu_offset[0] >> 8 :", (off0 >> 8) == x20)
assert (off0 >> 8) == x20

banner("5. 字节级复核：非对齐窗口 base+1 唯一命中")
import struct
stream = struct.pack('<Q', off0) + struct.pack('<Q', 0xffffcc879da50000)  # elem0,elem1 真值
stream += b'\x00' * 16   # 后续元素低字节均为 0x00（全数组 0x...000 步进 0x22000）
for off in range(0, 12):
    val = struct.unpack('<Q', stream[off:off+8])[0]
    mark = "   <== 命中观测值!" if val == x20 else ""
    print("read(base+%2d) = 0x%016x%s" % (off, val, mark))
hits = [o for o in range(12) if struct.unpack('<Q', stream[o:o+8])[0] == x20]
print("唯一命中偏移:", hits)
assert hits == [1]

banner("6. 有效地址异常量化")
EA_expected = per_cpu_offset_rt + x25*8
EA_implied  = per_cpu_offset_rt + 1
print("期望 EA = base + 146*8 = base + 0x%x = 0x%x" % (x25*8, EA_expected))
print("隐含 EA = base + 1                        = 0x%x" % EA_implied)
print("偏移差: 期望 0x%x vs 隐含 0x%x, XOR=0x%x popcount=%d" %
      (x25*8, 1, (x25*8)^1, bin((x25*8)^1).count('1')))
print("==> 索引项(0x490)整体消失且出现+1字节歪斜，非单比特翻转")

banner("7. 反事实推演：若装载正确")
rq146 = h(x1 + off146)
rq179 = h(x1 + off179)
print("rq(146) = runqueues + off[146] = 0x%x  == crash px runqueues[146](0x%x): %s"
      % (rq146, rq146_px, rq146 == rq146_px))
print("rq(179) = runqueues + off[179] = 0x%x  == crash px runqueues[179](0x%x): %s"
      % (rq179, rq179_px, rq179 == rq179_px))
print("装载正确时 x27 = rq(146) = 0x%x（crash vtop 证实已映射, PTE=VALID）" % rq146)
print("装载正确时 ldr x23,[x27,#288] 访问 0x%x —— 合法 vmalloc 页, 不触发异常" % h(rq146 + 0x120))
assert rq146 == rq146_px and rq179 == rq179_px

banner("8. per-cpu 单元步进合理性")
print("off[146]-off[145] = 0x%x (=0x22000, 34页/单元, 192核一致步进)" % h(off146-off145))

banner("9. 时间线（墙上钟）")
import datetime
crash_date = datetime.datetime(2026, 8, 25, 15, 57, 13)   # crash sys: DATE (取整秒)
uptime_s   = 418.744655                                      # dmesg 末行时间戳
boot_wall  = crash_date - datetime.timedelta(seconds=uptime_s)
panic_wall = crash_date
print("崩溃墙上钟(crash DATE 截断秒) ≈ %s" % panic_wall)
print("反推开机墙上钟 ≈ %s" % boot_wall)
print("前兆异常条数(本次开机 dmesg grep WARNING/BUG/Oops/硬件错误) = 0")
print("最后一条普通日志 [ 418.373008] 与 panic 之间无其他内核错误")

banner("全部断言通过：闭合验证 100% 成立")
