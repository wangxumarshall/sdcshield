# CMP (extended register)

## 分类
Compare

## 描述
Compare (extended register):

## 详细信息

```
107 ===
CMP (extended register)
Compare (extended register) subtracts a sign or zero-extended register value, followed by an optional left shift
amount, from a register value. The argument that is extended from the <Rm> register can be a byte, halfword, word,
or doubleword. It updates the condition flags based on the result, and discards the result.
This is an alias of SUBS (extended register)
. This means:
• The encodings in this description are named to match the encodings of SUBS (extended register).
• The description of SUBS (extended register) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 0 1 0 1 1 0 0 1 Rm option imm3 Rn 1 1 1 1 1
op S Rd
32-bit (sf == 0)
CMP <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
is equivalent to
SUBS WZR, <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
and is always the preferred disassembly.
64-bit (sf == 1)
CMP <Xn|SP>, <R><m>{, <extend> {#<amount>}}
is equivalent to
SUBS XZR, <Xn|SP>, <R><m>{, <extend> {#<amount>}}
and is always the preferred disassembly.
Assembler Symbols
<Wn|WSP> Is the 32-bit name of the first source general-purpose register or stack pointer, encoded in the "Rn"
field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
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
CMP (extended register) Page 104
RETIRED


```
