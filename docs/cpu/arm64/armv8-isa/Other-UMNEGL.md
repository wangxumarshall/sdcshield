# UMNEGL

## 分类
Other

## 描述
Unsigned Multiply-Negate Long:

## 详细信息

```
802 ===
UMNEGL
Unsigned Multiply-Negate Long multiplies two 32-bit register values, negates the product, and writes the result to the
64-bit destination register.
This is an alias of UMSUBL. This means:
• The encodings in this description are named to match the encodings of UMSUBL.
• The description of UMSUBL gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 1 0 1 1 1 0 1 Rm 1 1 1 1 1 1 Rn Rd
U o0 Ra
UMNEGL <Xd>, <Wn>, <Wm>
is equivalent to
UMSUBL <Xd>, <Wn>, <Wm>, XZR
and is always the preferred disassembly.
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
Operation
The description of UMSUBL
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
UMNEGL Page 799
RETIRED


```
