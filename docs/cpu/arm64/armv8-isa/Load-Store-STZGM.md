# STZGM

## 分类
Load-Store

## 描述
Store Tag and Zero Multiple

## 详细信息

```
755 ===
STZGM
Store Tag and Zero Multiple writes a naturally aligned block of N Allocation Tags and stores zero to the associated
data locations, where the size of N is identified in DCZID_EL0.BS, and the Allocation Tag written to address A is taken
from the source register bits<3:0>.
This instruction is UNDEFINED at EL0.
This instruction generates an Unchecked access.
Integer
(FEAT_MTE2)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 1 0 0 1 0 0 1 0 0 0 0 0 0 0 0 0 0 0 Xn Xt
STZGM <Xt>, [<Xn|SP>]
if !HaveMTE2Ext
() then UNDEFINED;
integer t = UInt(Xt);
integer n = UInt(Xn);
Assembler Symbols
<Xt> Is the 64-bit name of the general-purpose register to be transferred, encoded in the "Xt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Xn" field.
Operation
if PSTATE.EL == EL0 then
UNDEFINED;
bits(64) data = X[t];
bits(4) tag = data<3:0>;
bits(64) address;
if n == 31 then
CheckSPAlignment
();
address = SP[];
else
address = X[n];
integer size = 4 * (2 ^ (UInt(DCZID_EL0.BS)));
address = Align(address, size);
integer count = size >> LOG2_TAG_GRANULE;
for i = 0 to count-1
AArch64.MemTag[address, AccType_NORMAL] = tag;
Mem[address, TAG_GRANULE, AccType_NORMAL] = Zeros(8 * TAG_GRANULE);
address = address + TAG_GRANULE;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
STZGM Page 752
RETIRED


```
