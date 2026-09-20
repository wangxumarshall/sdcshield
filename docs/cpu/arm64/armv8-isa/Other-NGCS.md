# NGCS

## 分类
Other

## 描述
Negate with Carry, setting flags:

## 详细信息

```
548 ===
NGCS
Negate with Carry, setting flags, negates the sum of a register value and the value of NOT (Carry flag), and writes the
result to the destination register. It updates the condition flags based on the result.
This is an alias of SBCS. This means:
• The encodings in this description are named to match the encodings of SBCS.
• The description of SBCS gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 1 1 0 1 0 0 0 0 Rm 0 0 0 0 0 0 1 1 1 1 1 Rd
op S Rn
32-bit (sf == 0)
NGCS <Wd>, <Wm>
is equivalent to
SBCS <Wd>, WZR, <Wm>
and is always the preferred disassembly.
64-bit (sf == 1)
NGCS <Xd>, <Xm>
is equivalent to
SBCS <Xd>, XZR, <Xm>
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wm> Is the 32-bit name of the general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xm> Is the 64-bit name of the general-purpose source register, encoded in the "Rm" field.
Operation
The description of SBCS
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
NGCS Page 545
RETIRED


```
