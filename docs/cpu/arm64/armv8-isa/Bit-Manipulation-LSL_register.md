# LSL (register)

## 分类
Bit-Manipulation

## 描述
Logical Shift Left (register):

## 详细信息

```
511 ===
LSL (register)
Logical Shift Left (register) shifts a register value left by a variable number of bits, shifting in zeros, and writes the
result to the destination register. The remainder obtained by dividing the second source register by the data size
defines the number of bits by which the first source register is left-shifted.
This is an alias of LSLV
. This means:
• The encodings in this description are named to match the encodings of LSLV.
• The description of LSLV gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 0 1 1 0 Rm 0 0 1 0 0 0 Rn Rd
op2
32-bit (sf == 0)
LSL <Wd>, <Wn>, <Wm>
is equivalent to
LSLV <Wd>, <Wn>, <Wm>
and is always the preferred disassembly.
64-bit (sf == 1)
LSL <Xd>, <Xn>, <Xm>
is equivalent to
LSLV <Xd>, <Xn>, <Xm>
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding a shift amount from 0 to 31 in
its bottom 5 bits, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register holding a shift amount from 0 to 63 in
its bottom 6 bits, encoded in the "Rm" field.
Operation
The description of LSLV
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
LSL (register) Page 508
RETIRED


```
