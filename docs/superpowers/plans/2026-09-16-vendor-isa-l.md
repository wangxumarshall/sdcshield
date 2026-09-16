# Vendor isa-l into third-party/ Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor Intel isa-l v2.32.1 under `third-party/isa-l/` so the 11 `isal_*` tests (`isal_igzip` + 10 `isal_crc*`) link a repo-built static `libisal.a` with the OpenSSL-style system fallback, instead of depending on the host's `/usr/lib/libisal`.

**Architecture:** Mirror the established vendored-library pattern (openssl in `framework/meson.build`, openblas/sleef in `tests/cpu/meson.build`): upstream tag tarball + `build.sh` producing `install/lib/libisal.a` + `install/include/isa-l/*.h` (gitignored), then rewire the meson gate to prefer the vendored install and fall back to `cpp.find_library('isal')` when absent. isa-l's own autoconf is unnecessary — its `Makefile.unx` + `make.inc` build system compiles cleanly on aarch64 with `make -f Makefile.unx lib` (verified by probe build: NEON pmull CRC kernels, igzip deflate/inflate aarch64 assembly, zero nasm dependency on aarch64).

**Tech Stack:** isa-l v2.32.1 (latest stable tag), BSD-3-Clause; GNU make (Makefile.unx path, no autoconf/nasm needed on aarch64); meson `run_command('test','-f',...)` probe + `declare_dependency` (same as openblas/sleef blocks).

## Global Constraints

- One patch per unit: each task below is exactly one commit; verify (build + functional + regression) before each commit.
- Work on a feature branch (e.g. `feat/vendor-isa-l`), push after each commit, never push to `main`.
- x86-64 non-regression: the meson change must stay behavior-compatible on x86 — the vendored-preference block must be arch-neutral (isa-l is a portable C library with internal dispatch; its Makefile.unx builds on x86 too, but the gate must not *break* x86 hosts where the vendored install is absent — they keep the system-lib path unchanged).
- `install/` is gitignored; meson degrades with a message when absent — never a hard error, never a disabler (empty-list discipline, same as `openblas_dep = []`).
- Upstream tarball kept for provenance: `isa-l-2.32.1.tar.gz`, sha256 `d9f7179ab0e14a3db9b610fac22793854a1435e8423ec9ce07f4cbedc5f92f5e` (verified working download from `https://codeload.github.com/intel/isa-l/tar.gz/refs/tags/v2.32.1`, 842741 bytes).
- The 11 test IDs that must keep passing: `isal_igzip`, `isal_crc32_gzip`, `isal_crc_ieee`, `isal_crc_iscsi`, `isal_crc_t10dif`, `isal_crc64_ecma182_norm`, `isal_crc64_ecma182_refl`, `isal_crc64_iso_norm`, `isal_crc64_iso_refl`, `isal_crc64_jones_norm`, `isal_crc64_jones_refl`.
- All test sources keep `#include <isa-l/...>` unchanged — the vendored install dir layout (`install/include/isa-l/{crc.h,crc64.h,igzip_lib.h}`) matches the system one, so zero test-source edits.
- Facts established by probe (do not re-derive): `make -f Makefile.unx lib` works on this aarch64 host with plain gcc; `make -f Makefile.unx isa-l.h` generates the umbrella header; `make -f Makefile.unx install prefix=<dir>` installs `lib/libisal.a`, `lib/libisal.so*`, `include/isa-l/*.h`, `include/isa-l.h`, `bin/igzip`; prototypes `crc16_t10dif`, `crc32_gzip_refl`, `crc32_iscsi`, `crc64_*`, `isal_deflate_stateless*`, `isal_inflate` are signature-identical between the system headers and v2.32.1 headers.
- x86 hosts: Makefile.unx on x86 needs `nasm` — NOT our problem; build.sh is only *run* by developers who want the vendored lib. The meson fallback keeps x86 hosts working unchanged (they simply don't run build.sh, or they do and have nasm). build.sh itself is aarch64-targeted like the openblas/sleef ones (openblas build.sh hardcodes TARGET=TSV110).

---

### Task 1: Vendor the isa-l tarball + build.sh + gitignore

**Files:**
- Create: `third-party/isa-l/isa-l-2.32.1.tar.gz` (downloaded, 842741 bytes)
- Create: `third-party/isa-l/build.sh` (mode 755)
- Create: `third-party/isa-l/.gitignore`

**Interfaces:**
- Consumes: nothing (self-contained).
- Produces: `third-party/isa-l/build.sh` which, when run, creates `third-party/isa-l/install/lib/libisal.a` + `third-party/isa-l/install/include/isa-l/{crc.h,crc64.h,igzip_lib.h,...}` + `install/include/isa-l.h`. Task 2's meson probe depends on exactly these paths.

- [ ] **Step 1: Create the directory and download the tarball**

```bash
mkdir -p third-party/isa-l
curl -sL -o third-party/isa-l/isa-l-2.32.1.tar.gz \
  https://codeload.github.com/intel/isa-l/tar.gz/refs/tags/v2.32.1
sha256sum third-party/isa-l/isa-l-2.32.1.tar.gz
# MUST print: d9f7179ab0e14a3db9b610fac22793854a1435e8423ec9ce07f4cbedc5f92f5e
```

- [ ] **Step 2: Write build.sh**

Content modeled on `third-party/sleef/build.sh` (idempotent, `set -euo pipefail`, JOBS=$(nproc), probe-verified annotation). Key decisions baked into the comments:
- Why `make -f Makefile.unx` and not autoconf: no autogen/configure dance needed, and the autoconf path drags in nasm checks that are irrelevant on aarch64.
- Why we delete the installed `.so*` files: SDCShield links the static archive only; keeping the shared lib in `install/lib/` would let `-lisal` pick it up accidentally in future rewirings. Also delete `bin/igzip` + man page (CLI tool, not needed).
- Why `make install` and not hand-copying headers: the `install` target also generates the `isa-l.h` umbrella header from `extern_hdrs`, and installing the *whole* header set (8 headers incl. `erasure_code.h`/`raid.h` we don't consume) costs nothing and matches the system package layout exactly.
- The `install` target's order-only prerequisite on `$(so_lib_name)` means `make lib` alone is NOT enough for `make install` — so build `lib slib` (both), install, then `rm` the `.so` products. Alternatively pass them on one make line: `make -f Makefile.unx lib slib isa-l.h` then `make -f Makefile.unx install prefix=...`.

```bash
#!/bin/bash
# Build isa-l 2.32.1 (Intel Intelligent Storage Acceleration Library) for
# SDCShield SDC stress testing (aarch64). The isal_* tests exercise the
# igzip deflate/inflate round-trip (isal_igzip) and the CRC16/32/64 family
# (isal_crc*): NEON pmull CRC kernels + hand-written aarch64 igzip assembly
# — data-dependent branch chains (match search), high-density stores and
# Huffman entropy coding, a third compression scheduling sample beside
# zstd/zlib.
# Why Makefile.unx and not ./autogen.sh + configure: the autoconf path adds
# nothing on aarch64 (its nasm feature probes are x86-only) and drags in an
# autoconf dependency; Makefile.unx + make.inc detect aarch64 via uname and
# compile the aarch64 kernels (crc/*_pmull.S, igzip/aarch64/*.S) with plain
# gcc, no nasm (verified by probe build: zero .asm/nasm invocations).
# install/ keeps ONLY the static archive: the make install target also
# produces libisal.so* (order-only prereq of install) and the igzip CLI —
# both deleted afterwards so nothing but headers + libisal.a remain; the
# tests link -l:libisal.a and a stray .so could otherwise win a future
# -lisal lookup.
# Idempotent: skip if install/lib/libisal.a already present.
# Probe-verified 2026-09-16: builds in ~10s at -j64 on Kunpeng 920;
# nm shows crc32_gzip_refl, crc64_*_pmull, isal_deflate_stateless,
# isal_inflate, and the aarch64 multibinary dispatchers.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libisal.a ] && { echo "install/ already present"; exit 0; }
tar xzf isa-l-2.32.1.tar.gz
cd isa-l-2.32.1
# lib: static archive (bin/isa-l.a); slib + isa-l.h: order-only prereqs of
# the install target (install hardlinks/copies the .so and needs isa-l.h
# generated). Build all three, then install into ../install.
make -f Makefile.unx -j"$JOBS" lib slib isa-l.h
make -f Makefile.unx install prefix="$PWD/../install"
cd ..
# keep only what meson links against: static archive + headers
rm -f install/lib/libisal.so install/lib/libisal.so.[0-9]*
rm -rf install/bin install/share
# canonical name: Makefile.unx installs bin/isa-l.a as lib/libisal.a already;
# no symlink dance needed (unlike openblas).
echo "OK: $(ls -la install/lib/libisal.a)"
```

- [ ] **Step 3: Write .gitignore**

Mirror `third-party/sleef/.gitignore` (`sleef-3.9.0/`, `build/`, `install/` — only the first applies here):

```
isa-l-2.32.1/
install/
```

- [ ] **Step 4: Run build.sh and verify real output**

```bash
chmod +x third-party/isa-l/build.sh
./third-party/isa-l/build.sh
ls -la third-party/isa-l/install/lib/ third-party/isa-l/install/include/isa-l/
nm third-party/isa-l/install/lib/libisal.a | grep -cE "crc32_gzip_refl|crc64_ecma|isal_deflate_stateless|isal_inflate"
# expected: >= 8 matching T symbols across the objects
# second run must be a no-op:
./third-party/isa-l/build.sh   # expected output: "install/ already present"
```

Real expected output (from probe): `install/lib/libisal.a` ~450 KB; `install/include/isa-l/` contains crc.h crc64.h erasure_code.h gf_vect_mul.h igzip_lib.h isal_api.h mem_routines.h raid.h + `install/include/isa-l.h`.

- [ ] **Step 5: Verify headers compile standalone against a test TU**

```bash
printf '#include <isa-l/igzip_lib.h>\n#include <isa-l/crc.h>\n#include <isa-l/crc64.h>\nint main(void){return 0;}\n' > /tmp/isal_hdr_test.c
gcc -I third-party/isa-l/install/include -Wall -Wextra -c /tmp/isal_hdr_test.c -o /tmp/isal_hdr_test.o && echo HEADERS-OK
```

- [ ] **Step 6: Commit (tarball ~825 KB — same order as sleef tarball, fine for this repo's provenance convention)**

```bash
git add third-party/isa-l/isa-l-2.32.1.tar.gz third-party/isa-l/build.sh third-party/isa-l/.gitignore
git commit -m "third-party: vendor isa-l 2.32.1 (build.sh → static libisal.a)

Upstream tag tarball (sha256 d9f7179a...) + idempotent build.sh producing
install/{lib/libisal.a, include/isa-l/*.h} via Makefile.unx on aarch64.
Matches the openssl/openblas/sleef vendoring pattern; meson wiring follows
in the next commit."
```

---

### Task 2: Rewire the meson isal gate: vendored-first, system fallback

**Files:**
- Modify: `tests/cpu/meson.build:676-700` (the `isal_lib` block) and `tests/cpu/meson.build:739-754` (the `tests_base_a` dependencies list — only if `isal_lib` is referenced there under a different name; it already is, as `isal_lib`)

**Interfaces:**
- Consumes: `third-party/isa-l/install/lib/libisal.a` + `install/include/` from Task 1.
- Produces: `isal_lib` (a found dependency object in both the vendored and system branches) consumed by `tests_set_base.add(when: isal_lib, ...)` and the `tests_base_a` `dependencies:` list — names unchanged, so no other build-file edits.

- [ ] **Step 1: Replace the find_library gate with a vendored-probe + fallback**

Replace the current block (lines ~676-700):

```meson
# isa-l (libisal) tests. The host ships /usr/lib/libisal.a (static) and
# /usr/lib/libisal.so but no libisal.pc, so dependency('libisal') fails here;
# use cpp.find_library('isal') instead (mirrors the reference tree). isal is a
# portable C library with arch-optimised dispatch internally, so the isal_*
# tests build on all arches; required:false keeps the build working on hosts
# without libisal installed.
#  - crc/isal_*.cpp (10): CRC functions.
#  - isa-l/igzip.cpp: igzip deflate/inflate round-trip SDC stress.
isal_lib = cpp.find_library('isal', required : false, static : true)
```

with (modeled on the openssl block in `framework/meson.build:212-236` — vendored probe first, system fallback, message on neither; the `run_command('test','-f',...)` probe + `declare_dependency` with `-l:libisal.a` follows the openblas/sleef blocks in this file):

```meson
# isa-l (libisal) tests — 11 total.
#  - crc/isal_*.cpp (10): CRC functions.
#  - isa-l/igzip.cpp: igzip deflate/inflate round-trip SDC stress.
# Prefer the vendored static isa-l under third-party/isa-l/install (built by
# its build.sh from the committed v2.32.1 tarball — same provenance pattern
# as openssl/openblas/sleef); fall back to the system package (hosts ship
# /usr/lib(64)/libisal.{a,so} but no libisal.pc, so dependency('libisal')
# fails — cpp.find_library works, mirroring the reference tree). isa-l is a
# portable C library with arch-optimised dispatch internally, so the isal_*
# tests build on all arches; required:false keeps the build working on hosts
# with neither vendored install nor system libisal.
# include_directories() must use a source-tree-relative path (meson rejects
# absolute paths into the source tree), so the include dir is given relative
# to this tests/cpu/ subdir while the archive probe and link args use the
# joined absolute path (same pattern as the openblas/sleef blocks above and
# the vendored OpenSSL block in framework/meson.build).
isal_install = meson.current_source_dir() / '../../third-party/isa-l/install'
if run_command('test', '-f', isal_install / 'lib' / 'libisal.a', check: false).returncode() == 0
    isal_lib = declare_dependency(
        include_directories: include_directories('../../third-party/isa-l/install/include', is_system: true),
        link_args: ['-L' + isal_install / 'lib', '-l:libisal.a'],
    )
    message('Using vendored isa-l (third-party/isa-l/install)')
else
    isal_lib = cpp.find_library('isal', required : false, static : true)
    if not isal_lib.found()
        message('No vendored or system libisal found — isal_* tests disabled. Run third-party/isa-l/build.sh to enable.')
    endif
endif
```

The subsequent `tests_set_base.add(when : isal_lib, if_true : files(...))` and the `isal_lib` entry in `tests_base_a`'s `dependencies:` list stay exactly as they are (a `declare_dependency` is a valid `when:` gate and dependency entry — same as `openblas_dep`/`sleef_dep`).

- [ ] **Step 2: Reconfigure and confirm the vendored branch is taken**

```bash
meson setup --reconfigure builddir 2>&1 | grep -i isal
# expected line: Message: Using vendored isa-l (third-party/isa-l/install)
```

- [ ] **Step 3: Build clean**

```bash
ninja -C builddir 2>&1 | tail -3
# expected: no new errors/warnings; "Linking target sdcshield" (or already up-to-date + relink)
```

- [ ] **Step 4: Prove the binary links the VENDORED archive, not the system one**

```bash
# (a) confirm no dynamic dependency on libisal:
ldd builddir/sdcshield | grep -c isal || echo "no dynamic libisal — statically linked"
# (b) confirm the linked objects came from the vendored build: compare a
# build-id / symbol fingerprint. The vendored 2.32.1 archive contains
# symbols the system 2.30.x lacks — e.g. crc32_rocksoft / crc64_rocksoft
# (added in 2.31) — but a stronger check is the object-file build path:
strings on the linked binary must NOT be conclusive, so use the archive
# itself. Definitive check: temporarily hide the system lib and relink:
sudo mv /usr/lib/libisal.a /usr/lib/libisal.a.hidden 2>/dev/null || true
sudo mv /usr/lib/libisal.so /usr/lib/libisal.so.hidden 2>/dev/null || true
sudo mv /usr/lib/libisal.so.2 /usr/lib/libisal.so.2.hidden 2>/dev/null || true
rm -f builddir/sdcshield && ninja -C builddir
./builddir/sdcshield --list-tests | grep -c isal    # expected: 11
sudo mv /usr/lib/libisal.a.hidden /usr/lib/libisal.a 2>/dev/null || true
sudo mv /usr/lib/libisal.so.hidden /usr/lib/libisal.so 2>/dev/null || true
sudo mv /usr/lib/libisal.so.2.hidden /usr/lib/libisal.so.2 2>/dev/null || true
```

Real proof captured for the commit message: the hidden-system-lib relink succeeds and `--list-tests` still shows 11 `isal_*` tests — impossible unless the link resolved against `third-party/isa-l/install/lib/libisal.a`.

Note: this step needs sudo; if sudo is unavailable, the alternative proof is `meson setup --reconfigure` message + `nm builddir/sdcshield | grep crc64_rocksoft` succeeding (2.32.1-only symbol — the system 2.30.x does not export it; verify with `nm /usr/lib/libisal.a | grep -c rocksoft` first and record both counts).

- [ ] **Step 5: Functional verification — all 11 isal tests pass**

```bash
./builddir/sdcshield -e isal_igzip,isal_crc32_gzip,isal_crc_ieee,isal_crc_iscsi,isal_crc_t10dif,isal_crc64_ecma182_norm,isal_crc64_ecma182_refl,isal_crc64_iso_norm,isal_crc64_iso_refl,isal_crc64_jones_norm,isal_crc64_jones_refl -t 3000 -n 1
# expected: 11x result: pass, exit: pass (0)
```

Also single-test with default threading for one of them:

```bash
./builddir/sdcshield -e isal_igzip -t 3000
# expected: result: pass across all CPU threads
```

- [ ] **Step 6: Regression check — unaffected tests**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1
# expected: result: pass, zero SIGSEGV
./builddir/sdcshield --list-tests | wc -l   # record count; no tests lost vs pre-change (278 at default quality + the isal tests were already in that count)
```

- [ ] **Step 7: x86-64 non-regression check (by inspection)**

The diff touches only the `isal_lib` resolution block: on any host without `third-party/isa-l/install` (every x86 CI host), the code path is byte-identical to before (`cpp.find_library('isal', required:false, static:true)`); with the vendored install present, x86 would additionally link the vendored lib — behavior-preserving since isa-l is arch-portable. No `#ifdef __x86_64__` source is touched; no x86 meson guard is changed. Record this reasoning in the commit message.

- [ ] **Step 8: Commit**

```bash
git add tests/cpu/meson.build
git commit -m "meson: prefer vendored isa-l over system libisal

OpenSSL-style gate in tests/cpu/meson.build: probe
third-party/isa-l/install/lib/libisal.a first (declare_dependency,
-l:libisal.a — mirrors the openblas/sleef blocks), fall back to
cpp.find_library('isal') when absent, message when neither. isal_* tests
now link the repo-built v2.32.1 static archive; system libisal hidden
during verification relink — 11/11 isal tests still listed and passing.
x86 hosts without the vendored install keep the exact prior path."
```

---

### Task 3: Documentation updates (README + offline-build-dependencies + CLAUDE.md)

**Files:**
- Modify: `README.md` (~line 175: the "isa-l 的 isal_igzip 用系统 libisal" sentence; ~line 49-53: the build-order comment listing the three build.sh; the vendored-library table ~line 166-172: add isa-l row)
- Modify: `docs/offline-build-dependencies.md` (§2.1 required-packages list line ~55 `libisa-l-devel`; §2.2 optional list; ~line 162 the find_library row; ~line 181 troubleshooting item 1)
- Modify: `CLAUDE.md` (Vendored third-party libraries section + the build/run quick-start mentioning which build.sh to run)

**Interfaces:**
- Consumes: final behavior from Task 2 (vendored-first, system fallback).
- Produces: docs stating (truthfully): vendored isa-l 2.32.1 is the default source when `third-party/isa-l/install` exists; system libisal is now only a fallback; `libisa-l-devel` moves from required to optional on openEuler hosts.

- [ ] **Step 1: Update README.md**

Three edits:

(a) The vendored-library table (after the sleef row, before pocketfft or after pocketfft — keep table order matching directory listing order: openssl, openblas, sleef, pocketfft, then isa-l last as the newest addition). Add row:

```markdown
| `third-party/isa-l/` | isa-l 2.32.1（Intel，BSD-3） | 静态 `libisal.a` | `isal_igzip`（igzip deflate/inflate 往返，aarch64 汇编内核）；10 个 `isal_crc*`（NEON pmull CRC16/32/64） |
```

(b) Replace line ~175 `isa-l 的 \`isal_igzip\`（deflate/inflate 往返）用系统 \`libisal\`，不引入新 vendored 依赖。` with:

```markdown
isa-l（`isal_igzip` + 10 个 `isal_crc*`）自 2026-09-16 起 vendor 到 `third-party/isa-l/`（v2.32.1，静态 `libisal.a`）；`install/` 缺失时回退系统 `libisal`。
```

(c) The first-build-order comment (~line 178, `./third-party/openssl/build.sh && ./third-party/openblas/build.sh && ./third-party/sleef/build.sh`): append `&& ./third-party/isa-l/build.sh` and update the preceding sentence "（pocketfft 无需预构建）" to also note isa-l is now part of the optional prebuild set (pocketfft 仍无需预构建).

- [ ] **Step 2: Update docs/offline-build-dependencies.md**

Truthful updates reflecting the new fallback chain:

(a) §2.1 required list: remove/demote the `libisa-l-devel` line — move it to §2.2 optional with a note: "vendored isa-l 默认启用（third-party/isa-l，build.sh 一次构建）；install/ 缺失时回退系统 libisal（此时需要 libisa-l-devel，EPOL 2.30.0 或 everything 2.29.0）".

(b) Line ~16 table row B (编译期库): change `libisal(isa-l)` from the "必须" column to the eigen5-style "仓库自带/可选" treatment — update the row to: `boost、eigen5（仓库自带）、zlib、libzstd、isa-l（仓库自带 vendored，系统包仅作回退）`.

(c) Line ~162 meson-probe table: update the `cpp.find_library('isal')` row to describe the new two-tier gate (vendored probe → system find_library fallback).

(d) Line ~181 troubleshooting item 1 ("isal 找不到"): rewrite symptom/solution — now the fix is `./third-party/isa-l/build.sh`（离线场景：仓库自带 tarball，无需网络）; system RPM only needed if one insists on the fallback path.

- [ ] **Step 3: Update CLAUDE.md**

(a) "Vendored third-party libraries" section list: add after the sleef bullet:

```markdown
- `isa-l/` — Intel isa-l 2.32.1, `build.sh` → static `libisal.a` (Makefile.unx path, no autoconf/nasm on aarch64); powers `isal_igzip` + the 10 `isal_crc*` tests. Meson prefers this install/ and falls back to system libisal.
```

(b) Build & run quick-start comment block: update "run each build.sh once BEFORE first meson setup — openssl, openblas, sleef" to include isa-l; update the `isal_igzip` example line comment `(system libisal, no vendored lib)` → `(vendored third-party/isa-l, static)`.

- [ ] **Step 4: Verify doc claims against reality**

```bash
./builddir/sdcshield --list-tests | grep -c isal    # 11 — matches docs claim
ls third-party/isa-l/install/lib/libisal.a          # exists — "默认启用" claim true
meson setup --reconfigure builddir 2>&1 | grep -i "isa-l"   # "Using vendored isa-l" — fallback claim testable
```

- [ ] **Step 5: Commit**

```bash
git add README.md docs/offline-build-dependencies.md CLAUDE.md
git commit -m "docs: document vendored isa-l (default) with system libisal fallback

README vendored-table row + build order; offline-build-dependencies moves
libisa-l-devel from required to optional (vendored default, RPM only for
the fallback path); CLAUDE.md third-party list + quick-start comments."
```

---

### Task 4: End-to-end verification + push

**Files:** none created (verification only)

**Interfaces:**
- Consumes: Tasks 1-3 committed.
- Produces: the final verification evidence quoted in the summary; branch pushed to remote.

- [ ] **Step 1: Clean-slate rebuild from vendored libs only**

```bash
# prove the vendored path is self-sufficient: system isal hidden, full rebuild
sudo mv /usr/lib/libisal.a /usr/lib/libisal.a.hidden
ninja -C builddir
./builddir/sdcshield -e isal_igzip -t 5000 -n 1     # expected: result: pass
sudo mv /usr/lib/libisal.a.hidden /usr/lib/libisal.a
```

(Skip the sudo-hiding if Task 2 Step 4's hide-relink was already performed and captured — one hide-cycle proof is sufficient; then this step reduces to a plain rebuild + run.)

- [ ] **Step 2: Full isal suite + regression battery**

```bash
./builddir/sdcshield -e isal_igzip,isal_crc32_gzip,isal_crc_ieee,isal_crc_iscsi,isal_crc_t10dif,isal_crc64_ecma182_norm,isal_crc64_ecma182_refl,isal_crc64_iso_norm,isal_crc64_iso_refl,isal_crc64_jones_norm,isal_crc64_jones_refl -t 5000 -n 1
./builddir/sdcshield -e zstd19 -t 3000 -n 1
./builddir/sdcshield -e openssl_sha -t 3000 -n 1
# all expected: exit: pass
```

- [ ] **Step 3: Multi-threaded isal_igzip smoke (the SDC-representative config)**

```bash
./builddir/sdcshield -e isal_igzip -t 5000
# expected: pass on all cores (this is the config the docs recommend)
```

- [ ] **Step 4: Push the branch**

```bash
git push -u origin feat/vendor-isa-l
```

---

## Self-Review

**Spec coverage:** goal = vendor isa-l so isal tests stop using system /usr/lib/libisal → Task 1 (tarball+build.sh), Task 2 (meson gate), Task 3 (docs incl. the README sentence that currently claims "不引入新 vendored 依赖" — now false until updated), Task 4 (e2e + push). The 10 isal_crc tests are covered by the same gate (they were already wired under `isal_lib`). ✓

**Placeholder scan:** no TBDs; all code blocks complete; every command runnable as-written. ✓

**Type consistency:** `isal_lib` name preserved in both branches of the new gate; `when : isal_lib` consumers unchanged; install paths `third-party/isa-l/install/{lib/libisal.a, include/}` used consistently in Task 1 build.sh and Task 2 probe. ✓

**Known risks encoded:** (1) `make install` needs `slib`+`isa-l.h` built (order-only prereqs) — build.sh builds all three targets explicitly. (2) The `.so` cleanup prevents accidental dynamic linking. (3) Sudo may be unavailable for the hide-system-lib proof — the `nm | grep crc64_rocksoft` 2.32.1-only-symbol alternative is specified. (4) System libisal on this host is 2.30.x source-built with *no matching headers in any RPM* — after this change that mismatch becomes irrelevant since both headers and lib come from the same vendored 2.32.1.
