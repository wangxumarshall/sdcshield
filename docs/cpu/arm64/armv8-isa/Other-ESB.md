# ESB

## 分类
Other

## 描述
Error Synchronization Barrier

## 详细信息

```
342 ===
ESB
Error Synchronization Barrier is an error synchronization event that might also update DISR_EL1 and VDISR_EL2.
This instruction can be used at all Exception levels and in Debug state.
In Debug state, this instruction behaves as if SError interrupts are masked at all Exception levels. See Error
Synchronization Barrier in the Arm(R) Reliability, Availability, and Serviceability (RAS) Specification, Armv8, for
Armv8-A architecture profile.
If the RAS Extension is not implemented, this instruction executes as a NOP.
System
(FEAT_RAS)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 1 0 0 0 0 1 1 1 1 1
CRm op2
ESB
if !HaveRASExt
() then EndOfInstruction();
Operation
SynchronizeErrors();
AArch64.ESBOperation();
if PSTATE.EL IN {EL0, EL1} && EL2Enabled() then AArch64.vESBOperation();
TakeUnmaskedSErrorInterrupts();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
ESB Page 339
RETIRED


```
