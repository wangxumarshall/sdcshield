# BC.cond

## 分类
Other

## 描述
Branch Consistent conditionally

## 详细信息

```
55 ===
BC.cond
Branch Consistent conditionally to a label at a PC-relative offset, with a hint that this branch will behave very
consistently and is very unlikely to change direction.
19-bit signed PC-relative branch offset
(FEAT_HBC)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 0 1 0 1 0 0 imm19 1 cond
BC.<cond> <label>
if !HaveFeatHBC() then UNDEFINED;
bits(64) offset = SignExtend(imm19:'00', 64);
Assembler Symbols
<cond> Is one of the standard conditions, encoded in the "cond" field in the standard way.
<label> Is the program label to be conditionally branched to. Its offset from the address of this instruction, in
the range +/-1MB, is encoded as "imm19" times 4.
Operation
if ConditionHolds(cond) then
BranchTo(PC[] + offset, BranchType_DIR, TRUE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
BC.cond Page 52
RETIRED


```
