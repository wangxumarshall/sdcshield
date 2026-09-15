# BICS (shifted register)

## 分类
Logical

## 描述
Bitwise Bit Clear (shifted register), setting flags

## 详细信息

```
66 ===
BICS (shifted register)
Bitwise Bit Clear (shifted register), setting flags, performs a bitwise AND of a register value and the complement of an
optionally-shifted register value, and writes the result to the destination register. It updates the condition flags based
on the result.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 0 1 0 1 0 shift 1 Rm imm6 Rn Rd
opc N
32-bit (sf == 0)
BICS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
64-bit (sf == 1)
BICS <Xd>, <Xn>, <Xm>{, <shift> #<amount>}
integer d = UInt
(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
if sf == '0' && imm6<5> == '1' then UNDEFINED;
ShiftType
shift_type = DecodeShift(shift);
integer shift_amount = UInt(imm6);
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Rm" field.
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
BICS (shifted register) Page 63
RETIRED


```
