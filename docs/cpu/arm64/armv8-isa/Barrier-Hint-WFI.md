# WFI

## 分类
Barrier-Hint

## 描述
Wait For Interrupt

## 详细信息

```
810 ===
WFI
Wait For Interrupt is a hint instruction that indicates that the PE can enter a low-power state and remain there until a
wakeup event occurs. For more information, see Wait For Interrupt.
As described in Wait For Interrupt, the execution of a WFI instruction that would otherwise cause entry to a low-power
state can be trapped to a higher Exception level.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 0 0 0 1 1 1 1 1 1 1
CRm op2
WFI
// Empty.
Operation
Hint_WFI(1, WFxType_WFI);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
WFI Page 807
RETIRED


```
