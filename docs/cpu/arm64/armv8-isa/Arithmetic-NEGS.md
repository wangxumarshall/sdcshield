# NEGS

## 分类
Arithmetic

## 描述
Negate, setting flags:

## 详细信息

```
545 ===
NEGS
Negate, setting flags, negates an optionally-shifted register value, and writes the result to the destination register. It
updates the condition flags based on the result.
This is an alias of SUBS (shifted register). This means:
• The encodings in this description are named to match the encodings of SUBS (shifted register).
• The description of SUBS (shifted register) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 0 1 0 1 1 shift 0 Rm imm6 1 1 1 1 1 != 11111
op S Rn Rd
32-bit (sf == 0)
NEGS <Wd>, <Wm>{, <shift> #<amount>}
is equivalent to
SUBS <Wd>, WZR, <Wm> {, <shift> #<amount>}
and is always the preferred disassembly.
64-bit (sf == 1)
NEGS <Xd>, <Xm>{, <shift> #<amount>}
is equivalent to
SUBS <Xd>, XZR, <Xm> {, <shift> #<amount>}
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wm> Is the 32-bit name of the general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xm> Is the 64-bit name of the general-purpose source register, encoded in the "Rm" field.
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
The description of SUBS (shifted register)
gives the operational pseudocode for this instruction.
NEGS Page 542
RETIRED


```
