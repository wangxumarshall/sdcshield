# SXTW

## 分类
Other

## 描述
Sign Extend Word:

## 详细信息

```
781 ===
SXTW
Sign Extend Word sign-extends a word to the size of the register, and writes the result to the destination register.
This is an alias of SBFM. This means:
• The encodings in this description are named to match the encodings of SBFM.
• The description of SBFM gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 0 0 1 1 0 1 0 0 0 0 0 0 0 1 1 1 1 1 Rn Rd
sf opc N immr imms
64-bit
SXTW <Xd>, <Wn>
is equivalent to
SBFM <Xd>, <Xn>, #0, #31
and is always the preferred disassembly.
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
Operation
The description of SBFM
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
SXTW Page 778
RETIRED


```
