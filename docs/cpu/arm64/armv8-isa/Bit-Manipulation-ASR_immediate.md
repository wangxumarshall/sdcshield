# ASR (immediate)

## 分类
Bit-Manipulation

## 描述
Arithmetic Shift Right (immediate):

## 详细信息

```
41 ===
ASR (immediate)
Arithmetic Shift Right (immediate) shifts a register value right by an immediate number of bits, shifting in copies of
the sign bit in the upper bits and zeros in the lower bits, and writes the result to the destination register.
This is an alias of SBFM. This means:
• The encodings in this description are named to match the encodings of SBFM.
• The description of SBFM gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 0 0 1 1 0 N immr x 1 1 1 1 1 Rn Rd
opc imms
32-bit (sf == 0 && N == 0 && imms == 011111)
ASR <Wd>, <Wn>, #<shift>
is equivalent to
SBFM <Wd>, <Wn>, #<shift>, #31
and is always the preferred disassembly.
64-bit (sf == 1 && N == 1 && imms == 111111)
ASR <Xd>, <Xn>, #<shift>
is equivalent to
SBFM <Xd>, <Xn>, #<shift>, #63
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<shift> For the 32-bit variant: is the shift amount, in the range 0 to 31, encoded in the "immr" field.
For the 64-bit variant: is the shift amount, in the range 0 to 63, encoded in the "immr" field.
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
ASR (immediate) Page 38
RETIRED


```
