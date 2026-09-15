# GMI

## 分类
Other

## 描述
Tag Mask Insert

## 详细信息

```
345 ===
GMI
Tag Mask Insert inserts the tag in the first source register into the excluded set specified in the second source
register, writing the new excluded set to the destination register.
Integer
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 1 0 1 0 1 1 0 Xm 0 0 0 1 0 1 Xn Xd
GMI <Xd>, <Xn|SP>, <Xm>
if !HaveMTEExt() then UNDEFINED;
integer d = UInt(Xd);
integer n = UInt(Xn);
integer m = UInt(Xm);
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Xd" field.
<Xn|SP> Is the 64-bit name of the first source general-purpose register or stack pointer, encoded in the "Xn"
field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Xm" field.
Operation
bits(64) address = if n == 31 then SP[] else X[n];
bits(64) mask = X[m];
bits(4) tag = AArch64.AllocationTagFromAddress(address);
mask<UInt(tag)> = '1';
X[d] = mask;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
GMI Page 342
RETIRED


```
