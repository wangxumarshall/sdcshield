# DRPS

## 分类
Other

## 描述
Debug restore process state

## 详细信息

```
330 ===
DRPS
Debug restore process state
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 1 0 1 0 1 1 1 1 1 1 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0
DRPS
if !Halted() || PSTATE.EL == EL0 then UNDEFINED;
Operation
DRPSInstruction();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
DRPS Page 327
RETIRED


```
