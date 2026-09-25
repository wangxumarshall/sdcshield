# LDTRH

## 分类
Load-Store

## 描述
Load Register Halfword (unprivileged)

## 详细信息

```
473 ===
LDTRH
Load Register Halfword (unprivileged) loads a halfword from memory, zero-extends it, and writes the result to a
register. The address that is used for the load is calculated from a base register and an immediate offset.
Memory accesses made by the instruction behave as if the instruction was executed at EL0 if the Effective value of
PSTATE.UAO is 0 and either:
• The instruction is executed at EL1.
• The instruction is executed at EL2 when the Effective value of HCR_EL2.{E2H, TGE} is {1, 1}.
Otherwise, the memory access operates with the restrictions determined by the Exception level at which the
instruction is executed. For information about memory accesses, see
Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 1 1 0 0 0 0 1 0 imm9 1 0 Rn Rt
size opc
LDTRH <Wt>, [<Xn|SP>{, #<simm>}]
bits(64) offset = SignExtend(imm9, 64);
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<simm> Is the optional signed immediate byte offset, in the range -256 to 255, defaulting to 0 and encoded in
the "imm9" field.
Shared Decode
integer n = UInt
(Rn);
integer t = UInt(Rt);
AccType acctype;
unpriv_at_el1 = PSTATE.EL == EL1 && !(EL2Enabled() && HaveNVExt() && HCR_EL2.<NV,NV1> == '11');
unpriv_at_el2 = PSTATE.EL == EL2 && HaveVirtHostExt() && HCR_EL2.<E2H,TGE> == '11';
user_access_override = HaveUAOExt() && PSTATE.UAO == '1';
if !user_access_override && (unpriv_at_el1 || unpriv_at_el2) then
acctype = AccType_UNPRIV;
else
acctype = AccType_NORMAL;
boolean tag_checked = n != 31;
Operation
bits(64) address;
bits(16) data;
if HaveMTE2Ext() then
SetTagCheckedInstruction(tag_checked);
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
address = address + offset;
data = Mem
[address, 2, acctype];
X[t] = ZeroExtend(data, 32);
LDTRH Page 470
RETIRED


```
