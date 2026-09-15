# CMN (immediate)

## 分类
Compare

## 描述
Compare Negative (immediate):

## 详细信息

```
103 ===
CMN (immediate)
Compare Negative (immediate) adds a register value and an optionally-shifted immediate value. It updates the
condition flags based on the result, and discards the result.
This is an alias of ADDS (immediate). This means:
• The encodings in this description are named to match the encodings of ADDS (immediate).
• The description of ADDS (immediate) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 1 0 0 0 1 0 sh imm12 Rn 1 1 1 1 1
op S Rd
32-bit (sf == 0)
CMN <Wn|WSP>, #<imm>{, <shift>}
is equivalent to
ADDS WZR, <Wn|WSP>, #<imm> {, <shift>}
and is always the preferred disassembly.
64-bit (sf == 1)
CMN <Xn|SP>, #<imm>{, <shift>}
is equivalent to
ADDS XZR, <Xn|SP>, #<imm> {, <shift>}
and is always the preferred disassembly.
Assembler Symbols
<Wn|WSP> Is the 32-bit name of the source general-purpose register or stack pointer, encoded in the "Rn" field.
<Xn|SP> Is the 64-bit name of the source general-purpose register or stack pointer, encoded in the "Rn" field.
<imm> Is an unsigned immediate, in the range 0 to 4095, encoded in the "imm12" field.
<shift> Is the optional left shift to apply to the immediate, defaulting to LSL #0 and encoded in “sh”:
sh <shift>
0 LSL #0
1 LSL #12
Operation
The description of ADDS (immediate)
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
CMN (immediate) Page 100
RETIRED


```
