# LDG

## 分类
Load-Store

## 描述
Load Allocation Tag

## 详细信息

```
400 ===
LDG
Load Allocation Tag loads an Allocation Tag from a memory address, generates a Logical Address Tag from the
Allocation Tag and merges it into the destination register. The address used for the load is calculated from the base
register and an immediate signed offset scaled by the Tag granule.
Integer
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 1 0 0 1 0 1 1 imm9 0 0 Xn Xt
LDG <Xt>, [<Xn|SP>{, #<simm>}]
if !HaveMTEExt
() then UNDEFINED;
integer t = UInt(Xt);
integer n = UInt(Xn);
bits(64) offset = LSL(SignExtend(imm9, 64), LOG2_TAG_GRANULE);
Assembler Symbols
<Xt> Is the 64-bit name of the general-purpose register to be transferred, encoded in the "Xt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Xn" field.
<simm> Is the optional signed immediate offset, a multiple of 16 in the range -4096 to 4080, defaulting to 0 and
encoded in the "imm9" field.
Operation
bits(64) address;
bits(4) tag;
if n == 31 then
CheckSPAlignment
();
address = SP[];
else
address = X[n];
address = address + offset;
address = Align(address, TAG_GRANULE);
tag = AArch64.MemTag[address, AccType_NORMAL];
X[t] = AArch64.AddressWithAllocationTag(X[t], AccType_NORMAL, tag);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
LDG Page 397
RETIRED


```
