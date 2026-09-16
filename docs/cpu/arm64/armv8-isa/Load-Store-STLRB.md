# STLRB

## 分类
Load-Store

## 描述
Store-Release Register Byte

## 详细信息

```
681 ===
STLRB
Store-Release Register Byte stores a byte from a 32-bit register to a memory location. The instruction also has memory
ordering semantics as described in Load-Acquire, Store-Release. For information about memory accesses, see Load/
Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 0 0 0 1 0 0 0 1 0 0 (1) (1) (1) (1) (1) 1 (1) (1) (1) (1) (1) Rn Rt
size L Rs o0 Rt2
STLRB <Wt>, [<Xn|SP>{,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
boolean tag_checked = n != 31;
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
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
data = X[t];
Mem[address, 1, AccType_ORDERED] = data;
Operational information
If PSTATE.DIT is 1, the timing of this instruction is insensitive to the value of the data being loaded or stored.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
STLRB Page 678
RETIRED


```
