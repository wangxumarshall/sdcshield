# CSETM

## 分类
Other

## 描述
Conditional Set Mask:

## 详细信息

```
315 ===
CSETM
Conditional Set Mask sets all bits of the destination register to 1 if the condition is TRUE, and otherwise sets all bits to
0.
This is an alias of CSINV. This means:
• The encodings in this description are named to match the encodings of CSINV.
• The description of CSINV gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 1 0 1 0 1 0 0 1 1 1 1 1 != 111x 0 0 1 1 1 1 1 Rd
op Rm cond o2 Rn
32-bit (sf == 0)
CSETM <Wd>, <cond>
is equivalent to
CSINV <Wd>, WZR, WZR, invert(<cond>)
and is always the preferred disassembly.
64-bit (sf == 1)
CSETM <Xd>, <cond>
is equivalent to
CSINV <Xd>, XZR, XZR, invert(<cond>)
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<cond> Is one of the standard conditions, excluding AL and NV , encoded in the "cond" field with its least
significant bit inverted.
Operation
The description of CSINV
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
CSETM Page 312
RETIRED


```
