# TSB CSYNC

## 分类
Other

## 描述
Trace Synchronization Barrier

## 详细信息

```
789 ===
TSB CSYNC
Trace Synchronization Barrier. This instruction is a barrier that synchronizes the trace operations of instructions.
If FEAT_TRF is not implemented, this instruction executes as a NOP.
System
(FEAT_TRF)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 1 0 0 1 0 1 1 1 1 1
CRm op2
TSB CSYNC
if !HaveSelfHostedTrace() then EndOfInstruction();
Operation
TraceSynchronizationBarrier();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
TSB CSYNC Page 786
RETIRED


```
