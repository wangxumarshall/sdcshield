# AT

## 分类
System

## 描述
Address Translate:

## 详细信息

```
45 ===
AT
Address Translate. For more information, see op0==0b01, cache maintenance, TLB maintenance, and address
translation instructions.
This is an alias of SYS. This means:
• The encodings in this description are named to match the encodings of SYS.
• The description of SYS gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 1 op1 0 1 1 1 1 0 0 x op2 Rt
L CRn CRm
AT <at_op>, <Xt>
is equivalent to
SYS #<op1>, C7, <Cm>, #<op2>, <Xt>
and is the preferred disassembly when SysOp(op1,'0111',CRm,op2) == Sys_AT.
Assembler Symbols
<at_op> Is an AT instruction name, as listed for the AT system instruction group, encoded in
“op1:CRm<0>:op2”:
op1 CRm<0> op2 <at_op> Architectural Feature
000 0 000 S1E1R -
000 0 001 S1E1W -
000 0 010 S1E0R -
000 0 011 S1E0W -
000 1 000 S1E1RP FEAT_PAN2
000 1 001 S1E1WP FEAT_PAN2
100 0 000 S1E2R -
100 0 001 S1E2W -
100 0 100 S12E1R -
100 0 101 S12E1W -
100 0 110 S12E0R -
100 0 111 S12E0W -
110 0 000 S1E3R -
110 0 001 S1E3W -
<op1> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op1" field.
<Cm> Is a name 'Cm', with 'm' in the range 0 to 15, encoded in the "CRm" field.
<op2> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op2" field.
<Xt> Is the 64-bit name of the general-purpose source register, encoded in the "Rt" field.
Operation
The description of SYS
gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
AT Page 42
RETIRED


```
