# SYS

## 分类
System

## 描述
System instruction

## 详细信息

```
782 ===
SYS
System instruction. For more information, see Op0 equals 0b01, cache maintenance, TLB maintenance, and address
translation instructions for the encodings of System instructions.
This instruction is used by the aliases AT, CFP, CPP, DC, DVP, IC, and TLBI.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 1 op1 CRn CRm op2 Rt
L
SYS #<op1>, <Cn>, <Cm>, #<op2>{, <Xt>}
integer t = UInt(Rt);
integer sys_op1 = UInt(op1);
integer sys_op2 = UInt(op2);
integer sys_crn = UInt(CRn);
integer sys_crm = UInt(CRm);
Assembler Symbols
<op1> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op1" field.
<Cn> Is a name 'Cn', with 'n' in the range 0 to 15, encoded in the "CRn" field.
<Cm> Is a name 'Cm', with 'm' in the range 0 to 15, encoded in the "CRm" field.
<op2> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op2" field.
<Xt> Is the 64-bit name of the optional general-purpose source register, defaulting to '11111', encoded in the
"Rt" field.
Alias Conditions
Alias Is preferred when
AT
CRn == '0111' && CRm == '100x' && SysOp(op1,'0111',CRm,op2) == Sys_AT
CFP op1 == '011' && CRn == '0111' && CRm == '0011' && op2 == '100'
CPP op1 == '011' && CRn == '0111' && CRm == '0011' && op2 == '111'
DC CRn == '0111' && SysOp(op1,'0111',CRm,op2) == Sys_DC
DVP op1 == '011' && CRn == '0111' && CRm == '0011' && op2 == '101'
IC CRn == '0111' && SysOp(op1,'0111',CRm,op2) == Sys_IC
TLBI CRn == '1000' && SysOp(op1,'1000',CRm,op2) == Sys_TLBI
Operation
AArch64.SysInstr(1, sys_op1, sys_crn, sys_crm, sys_op2, t);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SYS Page 779
RETIRED


```
