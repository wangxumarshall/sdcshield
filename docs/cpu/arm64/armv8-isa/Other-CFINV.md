# CFINV

## 分类
Other

## 描述
Invert Carry Flag

## 详细信息

```
94 ===
CFINV
Invert Carry Flag. This instruction inverts the value of the PSTATE.C flag.
System
(FEAT_FlagM)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 1 0 0 (0) (0) (0) (0) 0 0 0 1 1 1 1 1
CRm
CFINV
if !HaveFlagManipulateExt() then UNDEFINED;
Operation
PSTATE.C = NOT(PSTATE.C);
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CFINV Page 91
RETIRED


```
