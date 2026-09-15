# SEVL

## 分类
Barrier-Hint

## 描述
Send Event Local

## 详细信息

```
645 ===
SEVL
Send Event Local is a hint instruction that causes an event to be signaled locally without requiring the event to be
signaled to other PEs in the multiprocessor system. It can prime a wait-loop which starts with a WFE instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 0 0 1 0 1 1 1 1 1 1
CRm op2
SEVL
// Empty.
Operation
SendEventLocal();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SEVL Page 642
RETIRED


```
