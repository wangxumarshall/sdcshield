# MSR (immediate)

## 分类
System

## 描述
Move immediate value to Special Register

## 详细信息

```
534 ===
MSR (immediate)
Move immediate value to Special Register moves an immediate value to selected bits of the PSTATE. For more
information, see Process state, PSTATE.
The bits that can be written by this instruction are:
• PSTATE.D, PSTATE.A, PSTATE.I, PSTATE.F , and PSTATE.SP .
• If
FEAT_SSBS is implemented, PSTATE.SSBS.
• If FEAT_PAN is implemented, PSTATE.PAN.
• If FEAT_UAO is implemented, PSTATE.UAO.
• If FEAT_DIT is implemented, PSTATE.DIT.
• If FEAT_MTE is implemented, PSTATE.TCO.
• If FEAT_NMI is implemented, PSTATE.ALLINT.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 op1 0 1 0 0 CRm op2 1 1 1 1 1
MSR (immediate) Page 531
RETIRED


```
