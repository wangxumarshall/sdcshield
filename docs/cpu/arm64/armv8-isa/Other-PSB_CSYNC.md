# PSB CSYNC

## 分类
Other

## 描述
Profiling Synchronization Barrier

## 详细信息

```
571 ===
PSB CSYNC
Profiling Synchronization Barrier. This instruction is a barrier that ensures that all existing profiling data for the
current PE has been formatted, and profiling buffer addresses have been translated such that all writes to the profiling
buffer have been initiated. A following DSB instruction completes when the writes to the profiling buffer have
completed.
If the Statistical Profiling Extension is not implemented, this instruction executes as a NOP.
System
(FEAT_SPE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 1 0 0 0 1 1 1 1 1 1
CRm op2
PSB CSYNC
if !HaveStatisticalProfiling
() then EndOfInstruction();
Operation
ProfilingSynchronizationBarrier();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
PSB CSYNC Page 568
RETIRED


```
