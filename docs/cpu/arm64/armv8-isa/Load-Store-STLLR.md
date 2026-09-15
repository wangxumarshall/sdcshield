# STLLR

## 分类
Load-Store

## 描述
Store LORelease Register

## 详细信息

```
677 ===
STLLR
Store LORelease Register stores a 32-bit word or a 64-bit doubleword to a memory location, from a register. The
instruction also has memory ordering semantics as described in Load LOAcquire, Store LORelease. For information
about memory accesses, see Load/Store addressing modes.
No offset
(FEAT_LOR)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 0 0 1 0 0 0 1 0 0 (1) (1) (1) (1) (1) 0 (1) (1) (1) (1) (1) Rn Rt
size L Rs o0 Rt2
32-bit (size == 10)
STLLR <Wt>, [<Xn|SP>{,#0}]
64-bit (size == 11)
STLLR <Xt>, [<Xn|SP>{,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
integer elsize = 8 << UInt(size);
boolean tag_checked = n != 31;
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xt> Is the 64-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
Operation
bits(64) address;
bits(elsize) data;
constant integer dbytes = elsize DIV 8;
if HaveMTE2Ext
() then
SetTagCheckedInstruction(tag_checked);
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
data = X[t];
Mem[address, dbytes, AccType_LIMITEDORDERED] = data;
Operational information
If PSTATE.DIT is 1, the timing of this instruction is insensitive to the value of the data being loaded or stored.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
STLLR Page 674
RETIRED


```
