# YIELD

## 分类
Barrier-Hint

## 描述
YIELD

## 详细信息

```
814 ===
YIELD
YIELD is a hint instruction. Software with a multithreading capability can use a YIELD instruction to indicate to the PE
that it is performing a task, for example a spin-lock, that could be swapped out to improve overall system performance.
The PE can use this hint to suspend and resume multiple software threads if it supports the capability.
For more information about the recommended use of this instruction, see The YIELD instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 0 0 0 0 1 1 1 1 1 1
CRm op2
YIELD
// Empty.
Operation
Hint_Yield();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
YIELD Page 811
RETIRED


```
