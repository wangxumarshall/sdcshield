# CINC

## 分类
Conditional-Select

## 描述
Conditional Increment:

## 详细信息

```
96 ===
CINC
Conditional Increment returns, in the destination register, the value of the source register incremented by 1 if the
condition is TRUE, and otherwise returns the value of the source register.
This is an alias of CSINC. This means:
• The encodings in this description are named to match the encodings of CSINC.
• The description of CSINC gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 0 1 0 0 != 11111 != 111x 0 1 != 11111 Rd
op Rm cond o2 Rn
32-bit (sf == 0)
CINC <Wd>, <Wn>, <cond>
is equivalent to
CSINC <Wd>, <Wn>, <Wn>, invert(<cond>)
and is the preferred disassembly when
Rn == Rm.
64-bit (sf == 1)
CINC <Xd>, <Xn>, <cond>
is equivalent to
CSINC <Xd>, <Xn>, <Xn>, invert(<cond>)
and is the preferred disassembly when
Rn == Rm.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" and "Rm" fields.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" and "Rm" fields.
<cond> Is one of the standard conditions, excluding AL and NV , encoded in the "cond" field with its least
significant bit inverted.
Operation
The description of CSINC
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
CINC Page 93
RETIRED


```
