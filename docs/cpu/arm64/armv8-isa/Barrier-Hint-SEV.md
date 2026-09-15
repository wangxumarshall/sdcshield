# SEV

## 分类
Barrier-Hint

## 描述
Send Event

## 详细信息

```
644 ===
SEV
Send Event is a hint instruction. It causes an event to be signaled to all PEs in the multiprocessor system. For more
information, see Wait for Event mechanism and Send event.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 0 0 1 0 0 1 1 1 1 1
CRm op2
SEV
// Empty.
Operation
SendEvent();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SEV Page 641
RETIRED


```
