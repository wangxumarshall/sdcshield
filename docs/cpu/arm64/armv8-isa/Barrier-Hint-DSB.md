# DSB

## 分类
Barrier-Hint

## 描述
Data Synchronization Barrier

## 详细信息

```
331 ===
DSB
Data Synchronization Barrier is a memory barrier that ensures the completion of memory accesses, see Data
Synchronization Barrier.
A DSB instruction with the nXS qualifier is complete when the subset of these memory accesses with the XS attribute
set to 0 are complete. It does not require that memory accesses with the XS attribute set to 1 are complete.
This instruction is used by the aliases PSSBB, and SSBB.
It has encodings from 2 classes: Memory barrier and Memory nXS barrier
Memory barrier
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 1 CRm 1 0 0 1 1 1 1 1
opc
DSB <option>|#<imm>
boolean nXS = FALSE;
DSBAlias alias;
case CRm of
when '0000' alias = DSBAlias_SSBB;
when '0100' alias = DSBAlias_PSSBB;
otherwise alias = DSBAlias_DSB;
MBReqDomain domain;
case CRm<3:2> of
when '00' domain = MBReqDomain_OuterShareable;
when '01' domain = MBReqDomain_Nonshareable;
when '10' domain = MBReqDomain_InnerShareable;
when '11' domain = MBReqDomain_FullSystem;
MBReqTypes types;
case CRm<1:0> of
when '00' types = MBReqTypes_All; domain = MBReqDomain_FullSystem;
when '01' types = MBReqTypes_Reads;
when '10' types = MBReqTypes_Writes;
when '11' types = MBReqTypes_All;
Memory nXS barrier
(FEAT_XS)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 1 imm2 1 0 0 0 1 1 1 1 1 1
DSB <option>nXS|#<imm>
if !HaveFeatXS() then UNDEFINED;
MBReqTypes types = MBReqTypes_All;
boolean nXS = TRUE;
DSBAlias alias = DSBAlias_DSB;
MBReqDomain domain;
case imm2 of
when '00' domain = MBReqDomain_OuterShareable;
when '01' domain = MBReqDomain_Nonshareable;
when '10' domain = MBReqDomain_InnerShareable;
when '11' domain = MBReqDomain_FullSystem;
Assembler Symbols
<option> For the memory barrier variant: specifies the limitation on the barrier operation. Values are:
DSB Page 328
RETIRED


```
