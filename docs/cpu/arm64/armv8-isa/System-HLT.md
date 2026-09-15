# HLT

## 分类
System

## 描述
Halt instruction

## 详细信息

```
348 ===
HLT
Halt instruction. An HLT instruction can generate a Halt Instruction debug event, which causes entry into Debug state.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 0 1 0 imm16 0 0 0 0 0
HLT #<imm>
if EDSCR.HDE == '0' || !HaltingAllowed() then UNDEFINED;
if HaveBTIExt() then
SetBTypeCompatible(TRUE);
Assembler Symbols
<imm> Is a 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field.
Operation
Halt(DebugHalt_HaltInstruction);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
HLT Page 345
RETIRED


```
