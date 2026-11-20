# neon_rot_ldr_at_top_rowmajor_fp64_dump — Cold-Path FP64 Dump Variant Plan

**Goal:** Give the FP64 rotation trigger a got/expected bit-position record. The
integer rowmajor family reports failures via binary dumps
(`neon_rot_ldr_at_top_rowmajor_dump`); the FP64 consumer of the same family had
no dump variant, so a failure would only say "data miscompare" without words to
analyze. This variant keeps the FP64 hot loop byte-stable and adds a cold-path
full-context dump for offline bit-histogram (sign/exponent/mantissa) and
field-graft analysis.

**Spec / provenance:** Campaign-originated (2026-11-06/11-07 node72 CPU31
investigation; the file was run in the 11-07 fp64 soak — 3 h ≈ 1783 sweeps on
logical 2-31, 0 fail/0 crash/0 dump — and in the 11-07 18:18 NEON 30-min
family run, both recorded in `docs/research/progress-log.md` 2026-11-07
entries). Committed here after the fact (rebased onto 2026-11-09 main) with
fresh native verification per CLAUDE.md rule 2.

**Parent honesty note:** the header references the campaign-era parent
`neon_rot_ldr_at_top_rowmajor_fp64` (a 2026-11-06 working file, never carried
in-tree). This variant is self-contained: `test_init` regenerates its own golden
with the same `fp64_rot` slot order (0=fadd/1=fsub/2=fmul/3=fmax), so the
"hot loop byte-identity with the parent" paragraph is a design record, not a
build-time dependency.

**Design invariants (the movbe_dump/dualcmp family pattern):**
- Hot loop stays dump-free; all dump/report work lives in an out-of-line
  `__attribute__((noinline))` helper so its frame/register pressure never
  enters the hot codegen.
- `report_fail_msg` text keeps the substring "miscompare" so campaign
  `grep` counting keeps working.
- Dump files land in `movbe_log/fp64_rowmajor/` (created in init, dualcmp
  convention); if the directory cannot be created the binary dump is skipped
  silently and the failure message still carries the first differing
  elements with per-lane XOR.
- Cold path derives `cpu` from `sched_getcpu()` instead of threading the
  `test_run` cpu argument through, so the hot loop's register allocation is
  not perturbed.

**Registration:** one line in the `tests/cpu/meson.build` aarch64 list next to
the other rowmajor variants (after `_dump.cpp`). No `arm64_requires` tag —
NEON-only, no SVE/SME gating; identical to sibling family members. x86-64
untouched: the file body is guarded by `#if defined(__aarch64__)` and the meson
list sits inside the aarch64 guard.

**Verification (rule 2):**
- clean build, zero new warnings;
- `-e neon_rot_ldr_at_top_rowmajor_fp64_dump` short run → `exit: pass`;
- confirmed present in `--list-tests` PROD set;
- regression `-e zstd19 -t 2000 -n 1` → `exit: pass`;
- x86-64 non-regression by inspection (guard analysis above).
