# DGH

## 分类
Other

## 描述
Data Gathering Hint

## 详细信息

```
327 ===
DGH
Data Gathering Hint is a hint instruction that indicates that it is not expected to be performance optimal to merge
memory accesses with Normal Non-cacheable or Device-GRE attributes appearing in program order before the hint
instruction with any memory accesses appearing after the hint instruction into a single memory transaction on an
interconnect.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 0 0 1 1 0 1 1 1 1 1
CRm op2
DGH
if !HaveDGHExt
() then EndOfInstruction();
Operation
Hint_DGH();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
DGH Page 324
RETIRED


```
