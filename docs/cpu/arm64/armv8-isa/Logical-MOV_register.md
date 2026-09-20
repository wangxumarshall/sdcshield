# MOV (register)

## 分类
Logical

## 描述
Move (register):

## 详细信息

```
525 ===
MOV (register)
Move (register) copies the value in a source register to the destination register.
This is an alias of ORR (shifted register). This means:
• The encodings in this description are named to match the encodings of ORR (shifted register).
• The description of ORR (shifted register) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 0 1 0 1 0 0 0 0 Rm 0 0 0 0 0 0 1 1 1 1 1 Rd
opc shift N imm6 Rn
32-bit (sf == 0)
MOV <Wd>, <Wm>
is equivalent to
ORR <Wd>, WZR, <Wm>
and is always the preferred disassembly.
64-bit (sf == 1)
MOV <Xd>, <Xm>
is equivalent to
ORR <Xd>, XZR, <Xm>
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wm> Is the 32-bit name of the general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xm> Is the 64-bit name of the general-purpose source register, encoded in the "Rm" field.
Operation
The description of ORR (shifted register)
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
MOV (register) Page 522
RETIRED


```
