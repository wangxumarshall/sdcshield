# EXTR

## 分类
Bit-Manipulation

## 描述
Extract register

## 详细信息

```
343 ===
EXTR
Extract register extracts a register from a pair of registers.
This instruction is used by the alias ROR (immediate).
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 0 0 1 1 1 N 0 Rm imms Rn Rd
32-bit (sf == 0 && N == 0 && imms == 0xxxxx)
EXTR <Wd>, <Wn>, <Wm>, #<lsb>
64-bit (sf == 1 && N == 1)
EXTR <Xd>, <Xn>, <Xm>, #<lsb>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
integer lsb;
if N != sf then UNDEFINED;
if sf == '0' && imms<5> == '1' then UNDEFINED;
lsb = UInt
(imms);
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Rm" field.
<lsb> For the 32-bit variant: is the least significant bit position from which to extract, in the range 0 to 31,
encoded in the "imms" field.
For the 64-bit variant: is the least significant bit position from which to extract, in the range 0 to 63,
encoded in the "imms" field.
Alias Conditions
Alias Is preferred when
ROR (immediate)
Rn == Rm
Operation
bits(datasize) result;
bits(datasize) operand1 = X
[n];
bits(datasize) operand2 = X[m];
bits(2*datasize) concat = operand1:operand2;
result = concat<lsb+datasize-1:lsb>;
X
[d] = result;
Operational information
If PSTATE.DIT is 1:
EXTR Page 340
RETIRED


```
