# CLREX

## 分类
Other

## 描述
Clear Exclusive

## 详细信息

```
98 ===
CLREX
Clear Exclusive clears the local monitor of the executing PE.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 1 CRm 0 1 0 1 1 1 1 1
CLREX {#<imm>}
// CRm field is ignored
Assembler Symbols
<imm> Is an optional 4-bit unsigned immediate, in the range 0 to 15, defaulting to 15 and encoded in the
"CRm" field.
Operation
ClearExclusiveLocal(ProcessorID());
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CLREX Page 95
RETIRED


```
