# SMSUBL

## 分类
Other

## 描述
Signed Multiply-Subtract Long

## 详细信息

```
649 ===
SMSUBL
Signed Multiply-Subtract Long multiplies two 32-bit register values, subtracts the product from a 64-bit register value,
and writes the result to the 64-bit destination register.
This instruction is used by the alias SMNEGL.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 1 0 1 1 0 0 1 Rm 1 Ra Rn Rd
U o0
SMSUBL <Xd>, <Wn>, <Wm>, <Xa>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer a = UInt(Ra);
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
<Xa> Is the 64-bit name of the third general-purpose source register holding the minuend, encoded in the
"Ra" field.
Alias Conditions
Alias Is preferred when
SMNEGL Ra == '11111'
Operation
bits(32) operand1 = X[n];
bits(32) operand2 = X[m];
bits(64) operand3 = X[a];
integer result;
result = Int(operand3, FALSE) - (Int(operand1, FALSE) * Int(operand2, FALSE));
X[d] = result<63:0>;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SMSUBL Page 646
RETIRED


```
