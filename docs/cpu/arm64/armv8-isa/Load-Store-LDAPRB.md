# LDAPRB

## 分类
Load-Store

## 描述
Load-Acquire RCpc Register Byte

## 详细信息

```
363 ===
LDAPRB
Load-Acquire RCpc Register Byte derives an address from a base register value, loads a byte from the derived address
in memory, zero-extends it and writes it to a register.
The instruction has memory ordering semantics as described in Load-Acquire, Load-AcquirePC, and Store-Release,
except that:
• There is no ordering requirement, separate from the requirements of a Load-AcquirePC or a Store-Release,
created by having a Store-Release followed by a Load-AcquirePC instruction.
• The reading of a value written by a Store-Release by a Load-AcquirePC instruction by the same observer does
not make the write of the Store-Release globally observed.
This difference in memory ordering is not described in the pseudocode.
For information about memory accesses, see
Load/Store addressing modes.
Integer
(FEAT_LRCPC)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 0 1 1 1 0 0 0 1 0 1 (1) (1) (1) (1) (1) 1 1 0 0 0 0 Rn Rt
size Rs
LDAPRB <Wt>, [<Xn|SP> {,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
boolean tag_checked = n != 31;
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be loaded, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
Operation
bits(64) address;
bits(8) data;
if HaveMTE2Ext() then
SetTagCheckedInstruction(tag_checked);
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
data = Mem[address, 1, AccType_ORDERED];
X[t] = ZeroExtend(data, 32);
Operational information
If PSTATE.DIT is 1, the timing of this instruction is insensitive to the value of the data being loaded or stored.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
LDAPRB Page 360
RETIRED


```
