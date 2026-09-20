# XAFLAG

## 分类
Other

## 描述
Convert floating-point condition flags from external format to Arm format

## 详细信息

```
812 ===
XAFLAG
Convert floating-point condition flags from external format to Arm format. This instruction converts the state of the
PSTATE.{N,Z,C,V} flags from an alternative representation required by some software to a form representing the
result of an Arm floating-point scalar compare instruction.
System
(FEAT_FlagM2)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 1 0 0 (0) (0) (0) (0) 0 0 1 1 1 1 1 1
CRm
XAFLAG
if !HaveFlagFormatExt
() then UNDEFINED;
Operation
bit N = NOT(PSTATE.C) AND NOT(PSTATE.Z);
bit Z = PSTATE.Z AND PSTATE.C;
bit C = PSTATE.C OR PSTATE.Z;
bit V = NOT(PSTATE.C) AND PSTATE.Z;
PSTATE.N = N;
PSTATE.Z = Z;
PSTATE.C = C;
PSTATE.V = V;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
XAFLAG Page 809
RETIRED


```
