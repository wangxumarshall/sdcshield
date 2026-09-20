# ISB

## 分类
Barrier-Hint

## 描述
Instruction Synchronization Barrier

## 详细信息

```
352 ===
ISB
Instruction Synchronization Barrier flushes the pipeline in the PE and is a context synchronization event. For more
information, see Instruction Synchronization Barrier (ISB).
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 1 CRm 1 1 0 1 1 1 1 1
opc
ISB {<option>|#<imm>}
// No additional decoding required
Assembler Symbols
<option> Specifies an optional limitation on the barrier operation. Values are:
SY
Full system barrier operation, encoded as CRm = 0b1111. Can be omitted.
All other encodings of CRm are reserved. The corresponding instructions execute as full system barrier
operations, but must not be relied upon by software.
<imm> Is an optional 4-bit unsigned immediate, in the range 0 to 15, defaulting to 15 and encoded in the
"CRm" field.
Operation
InstructionSynchronizationBarrier();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
ISB Page 349
RETIRED


```
