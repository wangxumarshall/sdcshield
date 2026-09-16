# BRK

## 分类
Branch

## 描述
Breakpoint instruction

## 详细信息

```
75 ===
BRK
Breakpoint instruction. A BRK instruction generates a Breakpoint Instruction exception. The PE records the exception
in ESR_ELx, using the EC value 0x3c, and captures the value of the immediate argument in ESR_ELx.ISS.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 0 0 1 imm16 0 0 0 0 0
BRK #<imm>
if HaveBTIExt() then
SetBTypeCompatible(TRUE);
Assembler Symbols
<imm> Is a 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field.
Operation
AArch64.SoftwareBreakpoint(imm16);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
BRK Page 72
RETIRED


```
