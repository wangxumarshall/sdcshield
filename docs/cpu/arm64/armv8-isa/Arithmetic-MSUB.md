# MSUB

## 分类
Arithmetic

## 描述
Multiply-Subtract

## 详细信息

```
538 ===
MSUB
Multiply-Subtract multiplies two register values, subtracts the product from a third register value, and writes the
result to the destination register.
This instruction is used by the alias MNEG.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 1 0 0 0 Rm 1 Ra Rn Rd
o0
32-bit (sf == 0)
MSUB <Wd>, <Wn>, <Wm>, <Wa>
64-bit (sf == 1)
MSUB <Xd>, <Xn>, <Xm>, <Xa>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer a = UInt(Ra);
integer destsize = if sf == '1' then 64 else 32;
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
<Wa> Is the 32-bit name of the third general-purpose source register holding the minuend, encoded in the
"Ra" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
<Xa> Is the 64-bit name of the third general-purpose source register holding the minuend, encoded in the
"Ra" field.
Alias Conditions
Alias Is preferred when
MNEG
Ra == '11111'
Operation
bits(destsize) operand1 = X[n];
bits(destsize) operand2 = X[m];
bits(destsize) operand3 = X[a];
integer result;
result = UInt(operand3) - (UInt(operand1) * UInt(operand2));
X[d] = result<destsize-1:0>;
MSUB Page 535
RETIRED


```
