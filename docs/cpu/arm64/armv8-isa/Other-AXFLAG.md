# AXFLAG

## 分类
Other

## 描述
Convert floating-point condition flags from Arm to external format

## 详细信息

```
52 ===
AXFLAG
Convert floating-point condition flags from Arm to external format. This instruction converts the state of the
PSTATE.{N,Z,C,V} flags from a form representing the result of an Arm floating-point scalar compare instruction to an
alternative representation required by some software.
System
(FEAT_FlagM2)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 1 0 0 (0) (0) (0) (0) 0 1 0 1 1 1 1 1
CRm
AXFLAG
if !HaveFlagFormatExt
() then UNDEFINED;
Operation
bit Z = PSTATE.Z OR PSTATE.V;
bit C = PSTATE.C AND NOT(PSTATE.V);
PSTATE.N = '0';
PSTATE.Z = Z;
PSTATE.C = C;
PSTATE.V = '0';
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
AXFLAG Page 49
RETIRED


```
