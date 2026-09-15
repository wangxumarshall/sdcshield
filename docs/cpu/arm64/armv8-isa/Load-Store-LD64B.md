# LD64B

## 分类
Load-Store

## 描述
Single-copy Atomic 64-byte Load

## 详细信息

```
353 ===
LD64B
Single-copy Atomic 64-byte Load derives an address from a base register value, loads eight 64-bit doublewords from a
memory location, and writes them to consecutive registers, Xt to X(t+7). The data that is loaded is atomic and is
required to be 64-byte aligned.
Integer
(FEAT_LS64)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 1 1 1 0 0 0 0 0 1 1 1 1 1 1 1 1 0 1 0 0 Rn Rt
LD64B <Xt>, [<Xn|SP> {,#0}]
if !HaveFeatLS64
() then UNDEFINED;
if Rt<4:3> == '11' || Rt<0> == '1' then UNDEFINED;
integer n = UInt(Rn);
integer t = UInt(Rt);
boolean tag_checked = n != 31;
Assembler Symbols
<Xt> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
Operation
CheckLDST64BEnabled();
bits(512) data;
bits(64) address;
bits(64) value;
acctype = AccType_ATOMICLS64
;
if HaveMTE2Ext() then
SetTagCheckedInstruction(tag_checked);
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
data = MemLoad64B(address, acctype);
for i = 0 to 7
value = data<63+64*i:64*i>;
if BigEndian
(acctype) then value = BigEndianReverse(value);
X[t+i] = value;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
LD64B Page 350
RETIRED


```
