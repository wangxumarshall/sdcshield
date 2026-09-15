# LDRB (register)

## 分类
Load-Store

## 描述
Load Register Byte (register)

## 详细信息

```
425 ===
LDRB (register)
Load Register Byte (register) calculates an address from a base register value and an offset register value, loads a
byte from memory, zero-extends it, and writes it to a register. For information about memory accesses, seeLoad/Store
addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 0 1 1 1 0 0 0 0 1 1 Rm option S 1 0 Rn Rt
size opc
Extended register (option != 011)
LDRB <Wt>, [<Xn|SP>, (<Wm>|<Xm>), <extend> {<amount>}]
Shifted register (option == 011)
LDRB <Wt>, [<Xn|SP>, <Xm>{, LSL <amount>}]
if option<1> == '0' then UNDEFINED;    // sub-word index
ExtendType
extend_type = DecodeRegExtend(option);
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<Wm> When option<0> is set to 0, is the 32-bit name of the general-purpose index register, encoded in the
"Rm" field.
<Xm> When option<0> is set to 1, is the 64-bit name of the general-purpose index register, encoded in the
"Rm" field.
<extend> Is the index extend specifier, encoded in “option”:
option <extend>
010 UXTW
110 SXTW
111 SXTX
<amount> Is the index shift amount, it must be #0, encoded in "S" as 0 if omitted, or as 1 if present.
Shared Decode
integer n = UInt
(Rn);
integer t = UInt(Rt);
integer m = UInt(Rm);
LDRB (register) Page 422
RETIRED


```
