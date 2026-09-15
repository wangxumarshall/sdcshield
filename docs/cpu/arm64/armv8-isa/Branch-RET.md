# RET

## 分类
Branch

## 描述
Return from subroutine

## 详细信息

```
574 ===
RET
Return from subroutine branches unconditionally to an address in a register, with a hint that this is a subroutine
return.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 1 0 0 1 0 1 1 1 1 1 0 0 0 0 0 0 Rn 0 0 0 0 0
Z op A M Rm
RET {<Xn>}
integer n = UInt(Rn);
Assembler Symbols
<Xn> Is the 64-bit name of the general-purpose register holding the address to be branched to, encoded in
the "Rn" field. Defaults to X30 if absent.
Operation
bits(64) target = X[n];
// Value in BTypeNext will be used to set PSTATE.BTYPE
BTypeNext = '00';
BranchTo(target, BranchType_RET, FALSE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
RET Page 571
RETIRED


```
