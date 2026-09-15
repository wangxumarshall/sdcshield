# IRG

## 分类
Other

## 描述
Insert Random Tag

## 详细信息

```
351 ===
IRG
Insert Random Tag inserts a random Logical Address Tag into the address in the first source register, and writes the
result to the destination register. Any tags specified in the optional second source register or in GCR_EL1.Exclude are
excluded from the selection of the random Logical Address Tag.
Integer
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 1 0 1 0 1 1 0 Xm 0 0 0 1 0 0 Xn Xd
IRG <Xd|SP>, <Xn|SP>{, <Xm>}
if !HaveMTEExt
() then UNDEFINED;
integer d = UInt(Xd);
integer n = UInt(Xn);
integer m = UInt(Xm);
Assembler Symbols
<Xd|SP> Is the 64-bit name of the destination general-purpose register or stack pointer, encoded in the "Xd"
field.
<Xn|SP> Is the 64-bit name of the first source general-purpose register or stack pointer, encoded in the "Xn"
field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Xm" field. Defaults to
XZR if absent.
Operation
bits(64) operand = if n == 31 then SP[] else X[n];
bits(64) exclude_reg = X[m];
bits(16) exclude = exclude_reg<15:0> OR GCR_EL1.Exclude;
bits(4) rtag;
if AArch64.AllocationTagAccessIsEnabled(AccType_NORMAL) then
if GCR_EL1.RRND == '1' then
RGSR_EL1 = bits(64) UNKNOWN;
if IsOnes(exclude) then
rtag = '0000';
else
rtag = ChooseRandomNonExcludedTag(exclude);
else
bits(4) start = RGSR_EL1.TAG;
bits(4) offset = AArch64.RandomTag
();
rtag = AArch64.ChooseNonExcludedTag(start, offset, exclude);
RGSR_EL1.TAG = rtag;
else
rtag = '0000';
bits(64) result = AArch64.AddressWithAllocationTag(operand, AccType_NORMAL, rtag);
if d == 31 then
SP[] = result;
else
X[d] = result;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
IRG Page 348
RETIRED


```
