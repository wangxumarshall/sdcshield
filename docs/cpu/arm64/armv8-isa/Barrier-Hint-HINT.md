# HINT

## 分类
Barrier-Hint

## 描述
Hint instruction

## 详细信息

```
346 ===
HINT
Hint instruction is for the instruction set space that is reserved for architectural hint instructions.
Some encodings described here are not allocated in this revision of the architecture, and behave as NOPs. These
encodings might be allocated to other hint functionality in future revisions of the architecture and therefore must not
be used by software.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 CRm op2 1 1 1 1 1
HINT #<imm>
SystemHintOp
op;
case CRm:op2 of
when '0000 000' op = SystemHintOp_NOP;
when '0000 001' op = SystemHintOp_YIELD;
when '0000 010' op = SystemHintOp_WFE;
when '0000 011' op = SystemHintOp_WFI;
when '0000 100' op = SystemHintOp_SEV;
when '0000 101' op = SystemHintOp_SEVL;
when '0000 110'
if !HaveDGHExt() then EndOfInstruction();    // Instruction executes as NOP
op = SystemHintOp_DGH;
when '0000 111' SEE "XPACLRI";
when '0001 xxx'
case op2 of
when '000' SEE "PACIA1716";
when '010' SEE "PACIB1716";
when '100' SEE "AUTIA1716";
when '110' SEE "AUTIB1716";
otherwise EndOfInstruction
();
when '0010 000'
if !HaveRASExt() then EndOfInstruction();    // Instruction executes as NOP
op = SystemHintOp_ESB;
when '0010 001'
if !HaveStatisticalProfiling() then EndOfInstruction();    // Instruction executes as NOP
op = SystemHintOp_PSB;
when '0010 010'
if !HaveSelfHostedTrace() then EndOfInstruction();    // Instruction executes as NOP
op = SystemHintOp_TSB;
when '0010 100'
op = SystemHintOp_CSDB;
when '0011 xxx'
case op2 of
when '000' SEE "PACIAZ";
when '001' SEE "PACIASP";
when '010' SEE "PACIBZ";
when '011' SEE "PACIBSP";
when '100' SEE "AUTIAZ";
when '101' SEE "AUTIASP";
when '110' SEE "AUTIBZ";
when '111' SEE "AUTIBSP";
when '0100 xx0'
op = SystemHintOp_BTI
;
// Check branch target compatibility between BTI instruction and PSTATE.BTYPE
SetBTypeCompatible
(BTypeCompatible_BTI(op2<2:1>));
otherwise EndOfInstruction();
Assembler Symbols
<imm> Is a 7-bit unsigned immediate, in the range 0 to 127 encoded in the "CRm:op2" field.
The encodings that are allocated to architectural hint functionality are described in the "Hints" table in
the "Index by Encoding".
HINT Page 343
RETIRED


```
