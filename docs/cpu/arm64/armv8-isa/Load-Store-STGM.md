# STGM

## 分类
Load-Store

## 描述
Store Tag Multiple

## 详细信息

```
674 ===
STGM
Store Tag Multiple writes a naturally aligned block of N Allocation Tags, where the size of N is identified in
GMID_EL1.BS, and the Allocation Tag written to address A is taken from the source register at
4*A<7:4>+3:4*A<7:4>.
This instruction is UNDEFINED at EL0.
This instruction generates an Unchecked access.
Integer
(FEAT_MTE2)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 1 0 0 1 1 0 1 0 0 0 0 0 0 0 0 0 0 0 Xn Xt
STGM <Xt>, [<Xn|SP>]
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
bits(64) address;
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
integer size = 4 * (2 ^ (UInt(GMID_EL1.BS)));
address = Align(address, size);
integer count = size >> LOG2_TAG_GRANULE;
integer index = UInt(address<LOG2_TAG_GRANULE+3:LOG2_TAG_GRANULE>);
for i = 0 to count-1
bits(4) tag = data<(index*4)+3:index*4>;
AArch64.MemTag[address, AccType_NORMAL] = tag;
address = address + TAG_GRANULE;
index = index + 1;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
STGM Page 671
RETIRED


```
