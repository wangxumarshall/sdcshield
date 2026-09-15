# ARMv8-A Conditional-Select Instructions

Total: 7 instructions

---

## CINC

**Description:** Conditional Increment:

**Details:**
```
CINC: Conditional Increment: an alias of CSINC.
CINV: Conditional Invert: an alias of CSINV .
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CINV

**Description:** Conditional Invert:

**Details:**
```
CINV: Conditional Invert: an alias of CSINV .
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CNEG

**Description:** Conditional Negate:

**Details:**
```
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CSEL

**Description:** Conditional Select

**Details:**
```
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CSINC

**Description:** Conditional Select Increment

**Details:**
```
CSINC.
CINV: Conditional Invert: an alias of CSINV .
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CSINV

**Description:** Conditional Select Invert

**Details:**
```
CSINV .
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CSNEG

**Description:** Conditional Select Negation

**Details:**
```
CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

