# CMN (shifted register)

## 分类
Compare

## 描述
Compare Negative (shifted register):

## 详细信息

```
105 ===
CMN (shifted register)
Compare Negative (shifted register) adds a register value and an optionally-shifted register value. It updates the
condition flags based on the result, and discards the result.
This is an alias of ADDS (shifted register). This means:
• The encodings in this description are named to match the encodings of ADDS (shifted register).
• The description of ADDS (shifted register) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 0 1 0 1 1 shift 0 Rm imm6 Rn 1 1 1 1 1
op S Rd
32-bit (sf == 0)
CMN <Wn>, <Wm>{, <shift> #<amount>}
is equivalent to
ADDS WZR, <Wn>, <Wm> {, <shift> #<amount>}
and is always the preferred disassembly.
64-bit (sf == 1)
CMN <Xn>, <Xm>{, <shift> #<amount>}
is equivalent to
ADDS XZR, <Xn>, <Xm> {, <shift> #<amount>}
and is always the preferred disassembly.
Assembler Symbols
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Rm" field.
<shift> Is the optional shift type to be applied to the second source operand, defaulting to LSL and encoded in
“shift”:
shift <shift>
00 LSL
01 LSR
10 ASR
11 RESERVED
<amount> For the 32-bit variant: is the shift amount, in the range 0 to 31, defaulting to 0 and encoded in the
"imm6" field.
For the 64-bit variant: is the shift amount, in the range 0 to 63, defaulting to 0 and encoded in the
"imm6" field.
Operation
The description of ADDS (shifted register)
gives the operational pseudocode for this instruction.
CMN (shifted register) Page 102
RETIRED


```
