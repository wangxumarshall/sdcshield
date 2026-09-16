# UDF

## 分类
Other

## 描述
Permanently Undefined

## 详细信息

```
799 ===
UDF
Permanently Undefined generates an Undefined Instruction exception (ESR_ELx.EC = 0b000000). The encodings for
UDF used in this section are defined as permanently UNDEFINED.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 imm16
UDF #<imm>
// The imm16 field is ignored by hardware.
UNDEFINED;
Assembler Symbols
<imm> is a 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field. The PE ignores
the value of this constant.
Operation
// No operation.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
UDF Page 796
RETIRED


```
