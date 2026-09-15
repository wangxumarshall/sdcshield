# MNEG

## 分类
Other

## 描述
Multiply-Negate:

## 详细信息

```
520 ===
MNEG
Multiply-Negate multiplies two register values, negates the product, and writes the result to the destination register.
This is an alias of MSUB. This means:
• The encodings in this description are named to match the encodings of MSUB.
• The description of MSUB gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 1 0 0 0 Rm 1 1 1 1 1 1 Rn Rd
o0 Ra
32-bit (sf == 0)
MNEG <Wd>, <Wn>, <Wm>
is equivalent to
MSUB <Wd>, <Wn>, <Wm>, WZR
and is always the preferred disassembly.
64-bit (sf == 1)
MNEG <Xd>, <Xn>, <Xm>
is equivalent to
MSUB <Xd>, <Xn>, <Xm>, XZR
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
Operation
The description of MSUB
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
MNEG Page 517
RETIRED


```
