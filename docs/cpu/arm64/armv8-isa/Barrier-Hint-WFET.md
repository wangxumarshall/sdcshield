# WFET

## 分类
Barrier-Hint

## 描述
Wait For Event with Timeout

## 详细信息

```
809 ===
WFET
Wait For Event with Timeout is a hint instruction that indicates that the PE can enter a low-power state and remain
there until either a local timeout event or a wakeup event occurs. Wakeup events include the event signaled as a result
of executing the SEV instruction on any PE in the multiprocessor system. For more information, see
Wait For Event
mechanism and Send event.
As described in Wait For Event mechanism and Send event, the execution of a WFET instruction that would otherwise
cause entry to a low-power state can be trapped to a higher Exception level.
System
(FEAT_WFxT)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 0 1 0 0 0 0 0 0 0 Rd
WFET <Xt>
if !HaveFeatWFxT() then UNDEFINED;
integer d = UInt(Rd);
Assembler Symbols
<Xt> Is the 64-bit name of the general-purpose source register, encoded in the "Rd" field.
Operation
integer localtimeout = UInt(X[d, 64]);
if Halted() && ConstrainUnpredictableBool(Unpredictable_WFxTDEBUG) then
EndOfInstruction();
Hint_WFE(localtimeout, WFxType_WFET);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
WFET Page 806
RETIRED


```
