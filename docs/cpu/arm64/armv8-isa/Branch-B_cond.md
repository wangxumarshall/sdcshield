# B.cond

## 分类
Branch

## 描述
Branch conditionally

## 详细信息

```
54 ===
B.cond
Branch conditionally to a label at a PC-relative offset, with a hint that this is not a subroutine call or return.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 0 1 0 1 0 0 imm19 0 cond
B.<cond> <label>
bits(64) offset = SignExtend(imm19:'00', 64);
Assembler Symbols
<cond> Is one of the standard conditions, encoded in the "cond" field in the standard way.
<label> Is the program label to be conditionally branched to. Its offset from the address of this instruction, in
the range +/-1MB, is encoded as "imm19" times 4.
Operation
if ConditionHolds(cond) then
BranchTo(PC[] + offset, BranchType_DIR, TRUE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
B.cond Page 51
RETIRED


```
