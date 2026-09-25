# NOP

## 分类
Barrier-Hint

## 描述
No Operation

## 详细信息

```
549 ===
NOP
No Operation does nothing, other than advance the value of the program counter by 4. This instruction can be used for
instruction alignment purposes.
Note
The timing effects of including a NOP instruction in a program are not guaranteed. It can increase execution time,
leave it unchanged, or even reduce it. Therefore, NOP instructions are not suitable for timing loops.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 0 0 0 0 0 1 1 1 1 1
CRm op2
NOP
// Empty.
Operation
// do nothing
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
NOP Page 546
RETIRED


```
