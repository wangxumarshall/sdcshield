# SVC

## 分类
System

## 描述
Supervisor Call

## 详细信息

```
771 ===
SVC
Supervisor Call causes an exception to be taken to EL1.
On executing an SVC instruction, the PE records the exception as a Supervisor Call exception in ESR_ELx, using the
EC value 0x15, and the value of the immediate argument.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 0 0 0 imm16 0 0 0 0 1
SVC #<imm>
// Empty.
Assembler Symbols
<imm> Is a 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field.
Operation
AArch64.CheckForSVCTrap(imm16);
AArch64.CallSupervisor(imm16);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SVC Page 768
RETIRED


```
