# BR

## 分类
Branch

## 描述
Branch to Register

## 详细信息

```
72 ===
BR
Branch to Register branches unconditionally to an address in a register, with a hint that this is not a subroutine return.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 1 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 Rn 0 0 0 0 0
Z op A M Rm
BR <Xn>
integer n = UInt(Rn);
Assembler Symbols
<Xn> Is the 64-bit name of the general-purpose register holding the address to be branched to, encoded in
the "Rn" field.
Operation
bits(64) target = X[n];
// Value in BTypeNext will be used to set PSTATE.BTYPE
if InGuardedPage then
if n == 16 || n == 17 then
BTypeNext = '01';
else
BTypeNext = '11';
else
BTypeNext = '01';
BranchTo(target, BranchType_INDIR, FALSE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
BR Page 69
RETIRED


```
