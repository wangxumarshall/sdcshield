# DC

## 分类
System

## 描述
Data Cache operation:

## 详细信息

```
322 ===
DC
Data Cache operation. For more information, see op0==0b01, cache maintenance, TLB maintenance, and address
translation instructions.
This is an alias of SYS. This means:
• The encodings in this description are named to match the encodings of SYS.
• The description of SYS gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 1 op1 0 1 1 1 CRm op2 Rt
L CRn
DC <dc_op>, <Xt>
is equivalent to
SYS #<op1>, C7, <Cm>, #<op2>, <Xt>
and is the preferred disassembly when SysOp(op1,'0111',CRm,op2) == Sys_DC.
Assembler Symbols
<dc_op> Is a DC instruction name, as listed for the DC system instruction group, encoded in “op1:CRm:op2”:
op1 CRm op2 <dc_op> Architectural Feature
000 0110 001 IVAC -
000 0110 010 ISW -
000 0110 011 IGVAC FEAT_MTE2
000 0110 100 IGSW FEAT_MTE2
000 0110 101 IGDVAC FEAT_MTE2
000 0110 110 IGDSW FEAT_MTE2
000 1010 010 CSW -
000 1010 100 CGSW FEAT_MTE2
000 1010 110 CGDSW FEAT_MTE2
000 1110 010 CISW -
000 1110 100 CIGSW FEAT_MTE2
000 1110 110 CIGDSW FEAT_MTE2
011 0100 001 ZVA -
011 0100 011 GVA FEAT_MTE
011 0100 100 GZVA FEAT_MTE
011 1010 001 CVAC -
011 1010 011 CGVAC FEAT_MTE
011 1010 101 CGDVAC FEAT_MTE
011 1011 001 CVAU -
011 1100 001 CVAP FEAT_DPB
011 1100 011 CGVAP FEAT_MTE
011 1100 101 CGDVAP FEAT_MTE
011 1101 001 CVADP FEAT_DPB2
011 1101 011 CGVADP FEAT_MTE
011 1101 101 CGDVADP FEAT_MTE
011 1110 001 CIVAC -
011 1110 011 CIGVAC FEAT_MTE
011 1110 101 CIGDVAC FEAT_MTE
<op1> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op1" field.
<Cm> Is a name 'Cm', with 'm' in the range 0 to 15, encoded in the "CRm" field.
<op2> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op2" field.
<Xt> Is the 64-bit name of the general-purpose source register, encoded in the "Rt" field.
DC Page 319
RETIRED


```
