# SUBS (extended register)

## 分类
Arithmetic

## 描述
Subtract (extended register), setting flags

## 详细信息

```
765 ===
SUBS (extended register)
Subtract (extended register), setting flags, subtracts a sign or zero-extended register value, followed by an optional
left shift amount, from a register value, and writes the result to the destination register. The argument that is
extended from the <Rm> register can be a byte, halfword, word, or doubleword. It updates the condition flags based
on the result.
This instruction is used by the alias CMP (extended register)
.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 0 1 0 1 1 0 0 1 Rm option imm3 Rn Rd
op S
32-bit (sf == 0)
SUBS <Wd>, <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
64-bit (sf == 1)
SUBS <Xd>, <Xn|SP>, <R><m>{, <extend> {#<amount>}}
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
ExtendType extend_type = DecodeRegExtend(option);
integer shift = UInt(imm3);
if shift > 4 then UNDEFINED;
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn|WSP> Is the 32-bit name of the first source general-purpose register or stack pointer, encoded in the "Rn"
field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn|SP> Is the 64-bit name of the first source general-purpose register or stack pointer, encoded in the "Rn"
field.
<R> Is a width specifier, encoded in “option”:
option <R>
00x W
010 W
x11 X
10x W
110 W
<m> Is the number [0-30] of the second general-purpose source register or the name ZR (31), encoded in the
"Rm" field.
<extend> For the 32-bit variant: is the extension to be applied to the second source operand, encoded in “option”:
option <extend>
000 UXTB
001 UXTH
010 LSL|UXTW
011 UXTX
100 SXTB
101 SXTH
110 SXTW
111 SXTX
SUBS (extended register) Page 762
RETIRED


```
