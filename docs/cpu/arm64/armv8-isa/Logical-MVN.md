# MVN

## 分类
Logical

## 描述
Bitwise NOT:

## 详细信息

```
541 ===
MVN
Bitwise NOT writes the bitwise inverse of a register value to the destination register.
This is an alias of ORN (shifted register). This means:
• The encodings in this description are named to match the encodings of ORN (shifted register).
• The description of ORN (shifted register) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 0 1 0 1 0 shift 1 Rm imm6 1 1 1 1 1 Rd
opc N Rn
32-bit (sf == 0)
MVN <Wd>, <Wm>{, <shift> #<amount>}
is equivalent to
ORN <Wd>, WZR, <Wm>{, <shift> #<amount>}
and is always the preferred disassembly.
64-bit (sf == 1)
MVN <Xd>, <Xm>{, <shift> #<amount>}
is equivalent to
ORN <Xd>, XZR, <Xm>{, <shift> #<amount>}
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wm> Is the 32-bit name of the general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xm> Is the 64-bit name of the general-purpose source register, encoded in the "Rm" field.
<shift> Is the optional shift to be applied to the final source, defaulting to LSL and encoded in “shift”:
shift <shift>
00 LSL
01 LSR
10 ASR
11 ROR
<amount> For the 32-bit variant: is the shift amount, in the range 0 to 31, defaulting to 0 and encoded in the
"imm6" field.
For the 64-bit variant: is the shift amount, in the range 0 to 63, defaulting to 0 and encoded in the
"imm6" field,
Operation
The description of ORN (shifted register)
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
MVN Page 538
RETIRED


```
