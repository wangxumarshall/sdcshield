# ARM64 ISA 编译档位统一（march_arm64_* 单一事实来源）— 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把全仓散落的 16 处 aarch64 专属 `-march=` 字面量统一为根 `meson.build` 的 6 个 `march_arm64_*` 命名变量（单一事实来源，构建前后编译命令逐字节不变），并在 README.md、CLAUDE.md、docs/research/adding-tests.md 写清楚「哪些用例需要 NEON/SVE/SVE2 等什么编译选项、在哪儿统一指定」。

**Architecture:** meson 的 `subdir()` 共享变量作用域——根 `meson.build`（现有 `march_flags` 定义处，L78–109 之后）定义 6 个档位变量，所有 tests/framework 子目录直接引用；`tests/cpu/arm64/sdc_fuzzing/meson.build` 是自动生成的，改生成器 `scripts/generate_sdc_fuzz_cases.py` 后重新生成。不新增 meson option（档位与运行期 HWCAP 门控强耦合，做成可覆盖旋钮反而制造分叉风险）。

**Tech Stack:** meson ≥0.56（全部语法——字符串变量、list 内嵌字符串、`summary(section:)`——自 0.53 起支持，vendored 0.59.4 兼容）。

## Global Constraints（CLAUDE.md 摘要，逐条有效）

- **一单元一补丁**：本计划 = 一个功能单元（构建系统统一 + 强制文档同步），单 commit。
- **提交前 100% 实证**（rule 2）：`./scripts/build.sh` 干净零新警告；前后 `-march` 编译命令多重集 diff 为空；`--list-tests` 输出 sha 不变；`-e zstd19 -t 2000 -n 1` → `exit: pass`；x86 不回归（检视：全部改动在 aarch64 守卫内或对 x86 为惰性字符串数据）。本次改动本机可全量运行，无 gem5-fi 边界。
- **DCO**：`git commit -s`，最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容，无 Co-Authored-By。推送 feature 分支，禁止 main；提交前 `git branch --show-current`。
- **x86-64 不动**：仅 aarch64 路径改动；根定义对 x86 是惰性字符串数据。
- **文档同步**（rule 7）：README.md + CLAUDE.md + docs/research/adding-tests.md 与代码同 commit。
- 回复中文；结论先于原因。

---

## 调研结论（本计划的输入事实，全部实读自当前源码）

**哪些用例需要什么编译选项**（NEON 基线无需任何 `-march`：armv8-a 已含 NEON/ASIMD，基线 `tests_base`/`tests_common` 不设 march，用编译器默认）：

| ISA 档位 | 编译选项 | 受影响库（static_library）与用例 | 当前字面量位置 |
|---|---|---|---|
| ACLE CRC | `-march=armv8.1-a+crc` | `tests_crc`（crc32 / crc32_fixed / crc32_fixed_shuffled，`__crc32b/h/w`） | tests/cpu/meson.build:1152 |
| NEON+AES | `-march=armv8.1-a+crc+crypto` | `tests_arm64`（crypto_test 的 `vaeseq_u8` 等 +crypto）；`tests_arithmetic_arm_{gmp,acl,crt,aor}`；`tests_sdc_fuzzing`（自动生成）；`framework_arm64` | arm64/meson.build:15；arithmetic_arm/meson.build:37；sdc_fuzzing/meson.build:122（生成）；framework/device/cpu/arm64/meson.build:6 |
| SVE1 | `-march=armv8.2-a+sve` | `tests_arm64_sve`（sve512_* 全族）；`tests_sve`（Eigen SVE 分发 TU ×2）；`tests_sve_vla`（sleef_sve + sve_rot×5）；`tests_sve_crt`；`tests_sve_avx53` + `tests_sve_avx53_ipsec`（SVE 版 avx53 套件）；Eigen 分层实例化 `tests_eigen_{gemm,svd}_vl{128,256,512,1024}` ×8 | tests/cpu/meson.build:1271（Eigen tier 循环）、1312（dispatch）、1377/1381（vla）；sve/meson.build:290/362/385；arm64/meson.build:300 |
| SVE FMMLA | `-march=armv8.2-a+sve+f32mm+f64mm` | `tests_arm64_sve_mm`（sve512_fmmla_arm；**刻意不加 +sve2**——F32MM/F64MM 与 SVE2 正交，实测有 FMM 无 SVE2 的芯片） | arm64/meson.build:325 |
| SVE2+BF16 | `-march=armv9-a+sve2+bf16+f32mm+f64mm` | `tests_arm64_sve2`（sve2_cross_precision_arm，svmlalb/svbfdot） | arm64/meson.build:362 |
| SME | `-march=armv9-a+sve+sme+sme-f64f64`（+ 条件 `-mbranch-protection=none`） | `tests_arm64_sme`（sme_za_tileview / sme_fmopa_stress / sme_fmopa_za） | arm64/meson.build:409 |

**Eigen 特例**（用户点名的 eigen 类）：Eigen 的 SVE packet 编译期定宽（`arm_sve_vector_bits(EIGEN_ARM64_SVE_VL)`），VLA 在 C++ 语言层面不可编译——故按 128/256/512/1024 四档各实例化一个库：`march_arm64_sve` + `-msve-vector-bits=<tier>` + `-DEIGEN_ARM64_SVE_VL=<tier>` + `-DEigen=EigenSVE<tier>`（命名空间改名避免与 NEON 基线符号冲突）+ `-DEIGEN_ARM64_USE_SVE`；分发 TU（`tests_set_sve` 仅有的 2 个成员）无 `-msve-vector-bits`，运行时按工人 pinned VL 选档。

**无需库级 march 的特例**：`zpclmul{,_rep}` 用函数级 `__attribute__((target("+crypto")))`（PMULL），无库级 march；`zpclmul` 注释明示。

**编译器探测剔除（不阻断）**：`sve_mm_march`/`sve2_march`/`sme_march` 三档用 `cpp.has_argument()` 探测，不支持时 message 提示并跳过（本机 GCC 12.3 无 SME，预期见 "Compiler lacks SME support"）。

**统一后**：6 个变量在根 `meson.build` 定义，16 处字面量全部改为引用（含生成器 1 处 + 生成物 1 处）。

---

## Task 1: 基线取证与分支创建

**Files:** 无文件修改（证据采集 + git 分支）。

- [ ] **Step 1: 确认当前分支并创建 feature 分支**

```bash
cd /home/sdc/wangxu/sdcshield-wangxu-neon
git branch --show-current        # 记录；若已在 feat/* 则复用
git checkout -b feat/arm64-march-tiers-unify
```

Expected: 新分支创建成功（若同名已存在则 `git checkout` 之）。

- [ ] **Step 2: 把当前树构建到最新（证据基线 = 改前 HEAD）**

```bash
./scripts/build.sh
```

Expected: 构建绿，无新警告。

- [ ] **Step 3: 采集改前 `-march` 编译命令多重集**

```bash
ninja -C builddir -t commands | grep -o -- '-march=[^ ]*' | sort | uniq -c | tee /tmp/march-multiset-before.txt
```

Expected（本机实测 2026-11-03，gcc 12.3.1 openEuler）：**六档全部编入**——实测计数
`armv8.1-a+crc`×3、`armv8.1-a+crc+crypto`×144、`armv8.2-a+sve`×185、
`armv8.2-a+sve+f32mm+f64mm`×1、`armv9-a+sve2+bf16+f32mm+f64mm`×1、
`armv9-a+sve+sme+sme-f64f64`×3。允许计数随源集微变，档位集合应与此一致。
（勘误记录：初版计划在此预期「无 sme、GCC 12.3 不支持」——实证 openEuler gcc
12.3.1 接受该 march 串且定义 `__ARM_FEATURE_SME`（`c++ -march=...sme-f64f64` exit=0，
2026-11-03），tests_arm64_sme 三 TU 已编入。arm64/meson.build 注释「GCC-12 cannot
parse at all」对该工具链过时，Task 3 Step 3 一并修正。）

- [ ] **Step 4: 采集改前 `--list-tests` 指纹**

```bash
./builddir/sdcshield --list-tests | tee /tmp/listtests-before.txt | sha256sum
wc -l /tmp/listtests-before.txt
```

Expected: 记录 sha256 与行数（本机 no-SVE 920 按 CLAUDE.md 应为 439 PROD 档）。

## Task 2: 根 meson.build 定义 6 个档位变量 + 配置摘要

**Files:**
- Modify: `meson.build`（两处：L109 `endif` 后插入变量块；L397–409 `summary({...})` 后插入 aarch64 档位摘要）

**Interfaces:**
- Produces（后续所有 Task 消费的名字，逐字使用）：
  - `march_arm64_crc = '-march=armv8.1-a+crc'`
  - `march_arm64_neon_crypto = '-march=armv8.1-a+crc+crypto'`
  - `march_arm64_sve = '-march=armv8.2-a+sve'`
  - `march_arm64_sve_mm = '-march=armv8.2-a+sve+f32mm+f64mm'`
  - `march_arm64_sve2 = '-march=armv9-a+sve2+bf16+f32mm+f64mm'`
  - `march_arm64_sme = '-march=armv9-a+sve+sme+sme-f64f64'`

- [ ] **Step 1: 在 x86 march 块（L84–109）的 `endif` 之后、`debug_nasm_flags = ''` 之前插入：**

```meson
# ---------------------------------------------------------------------------
# ARM64 ISA march tiers — single source of truth. Every aarch64-specific
# -march in the tree references one of these variables (never a scattered
# literal): subdir() shares this scope, so every tests/ and framework/
# subdirectory sees them. Tier -> test-family table: README "从源码构建" and
# docs/research/adding-tests.md §4. NEON itself needs no -march (the armv8-a
# baseline already has ASIMD); these tiers exist only where intrinsics demand
# a specific extension. Untouched on x86-64 (inert string data there).
march_arm64_crc = '-march=armv8.1-a+crc'                   # tests_crc (ACLE __crc32b/h/w)
march_arm64_neon_crypto = '-march=armv8.1-a+crc+crypto'    # tests_arm64 / tests_arithmetic_arm_* / tests_sdc_fuzzing / framework_arm64
march_arm64_sve = '-march=armv8.2-a+sve'                   # every SVE1 library + the Eigen per-tier instantiations
march_arm64_sve_mm = '-march=armv8.2-a+sve+f32mm+f64mm'    # tests_arm64_sve_mm (FMMLA; deliberately NOT +sve2)
march_arm64_sve2 = '-march=armv9-a+sve2+bf16+f32mm+f64mm'  # tests_arm64_sve2
march_arm64_sme = '-march=armv9-a+sve+sme+sme-f64f64'      # tests_arm64_sme
```

- [ ] **Step 2: 在现有 `summary({...}, section: 'Configuration', ...)` 调用之后追加：**

```meson
if host_machine.cpu_family() == 'aarch64'
    summary(
        {
            'ACLE CRC (march_arm64_crc)': march_arm64_crc,
            'NEON+crypto (march_arm64_neon_crypto)': march_arm64_neon_crypto,
            'SVE1 (march_arm64_sve)': march_arm64_sve,
            'SVE FMMLA (march_arm64_sve_mm)': march_arm64_sve_mm,
            'SVE2 (march_arm64_sve2)': march_arm64_sve2,
            'SME (march_arm64_sme)': march_arm64_sme,
        },
        section: 'ARM64 ISA march tiers',
    )
endif
```

- [ ] **Step 3: reconfigure + 构建，确认摘要出现且构建绿：**

```bash
meson setup --reconfigure builddir 2>&1 | sed -n '/ARM64 ISA march tiers/,/^$/p'
./scripts/build.sh
```

Expected: 摘要 6 行档位全打印；构建绿零新警告（此时 16 处字面量仍是旧值，行为不变）。

## Task 3: 五个手写 meson 文件替换字面量（15 处）

**Files:**
- Modify: `tests/cpu/meson.build`（5 处：L1152、L1271、L1312、L1377、L1381）
- Modify: `tests/cpu/sve/meson.build`（3 处：L290、L362、L385）
- Modify: `tests/cpu/arm64/meson.build`（5 处：L14–16、L300、L325、L362、L409）
- Modify: `tests/cpu/arithmetic_arm/meson.build`（1 处：L36–38）
- Modify: `framework/device/cpu/arm64/meson.build`（1 处：L5–7）

**Interfaces:**
- Consumes: Task 2 的 6 个变量名（逐字）。
- 注意保留各处局部探测变量名 `sve_mm_march`/`sve2_march`/`sme_march`（供 `cpp.has_argument()` 探测与 message 引用），只把字面量赋值改为根变量引用。

- [ ] **Step 1: tests/cpu/meson.build —— 5 处：**

L1152（tests_crc_a cpp_args 内）：
```meson
# 旧：            '-march=armv8.1-a+crc',
            march_arm64_crc,
```
L1271（Eigen tier 循环 eigen_tier_args 首元素）：
```meson
# 旧：            '-march=armv8.2-a+sve',
            march_arm64_sve,
```
L1312：
```meson
    march_sve_dispatch = [march_arm64_sve]
```
L1377 与 L1381（tests_sve_vla 的 c_args 与 cpp_args 两处，同一行文本出现两次，用 replace_all 或逐处锚定）：
```meson
# 旧：                '-march=armv8.2-a+sve',
                march_arm64_sve,
```

- [ ] **Step 2: tests/cpu/sve/meson.build —— 3 处（tests_sve_crt L290、tests_sve_avx53_ipsec L362、tests_sve_avx53 L385，均为同一字面量）：**

```meson
# 旧：                '-march=armv8.2-a+sve',   (16 空格缩进 ×2 处)
# 旧：            '-march=armv8.2-a+sve',       (12 空格缩进 ×1 处)
                march_arm64_sve,
```

- [ ] **Step 3: tests/cpu/arm64/meson.build —— 5 处：**

L14–16（整块收敛为单行）：
```meson
    arm64_cpp_flags = [march_arm64_neon_crypto]
```
L300（tests_arm64_sve cpp_args 内）：
```meson
# 旧：            '-march=armv8.2-a+sve',
            march_arm64_sve,
```
L325 / L362 / L409（探测变量改为根变量引用，探测与 message 逻辑不动）：
```meson
    sve_mm_march = march_arm64_sve_mm
    sve2_march = march_arm64_sve2
    sme_march = march_arm64_sme
```
并修正 L403 过时注释：`and which GCC-12 cannot parse at all` →
`and which not every toolchain parses (openEuler gcc 12.3.1 backport accepts it —
2026-11-03 probe: exit=0, __ARM_FEATURE_SME defined; vanilla GCC < 14 does not)`
——纯注释修正，与实证一致。

- [ ] **Step 4: tests/cpu/arithmetic_arm/meson.build L36–38 收敛为：**

```meson
arm_cpp_flags = [march_arm64_neon_crypto]
```

- [ ] **Step 5: framework/device/cpu/arm64/meson.build L5–7 收敛为：**

```meson
    arm64_cpp_flags = [march_arm64_neon_crypto]
```

（注：framework 与 tests/cpu/arm64 两处同名局部变量 `arm64_cpp_flags` 经 subdir 作用域先后遮蔽，值恒等——历史上已如此，本改动保持恒等，不另立改名单元。）

- [ ] **Step 6: 全量再验证构建：**

```bash
meson setup --reconfigure builddir
./scripts/build.sh
```

Expected: 构建绿，零新警告。

## Task 4: 生成器 + 重新生成 sdc_fuzzing

**Files:**
- Modify: `scripts/generate_sdc_fuzz_cases.py:156`
- Regenerate: `tests/cpu/arm64/sdc_fuzzing/meson.build`（AUTO-GENERATED，禁止手编）

**Interfaces:**
- Consumes: `march_arm64_neon_crypto`（生成物在 `tests/cpu/arm64/sdc_fuzzing/`，其祖先链 root→tests→tests/cpu 经 subdir 作用域可见该变量；生成物本就引用 `march_generic_flags` 等父作用域变量，先例成立）。

- [ ] **Step 1: 改生成器 L156 的发射行：**

```python
# 旧：        "            '-march=armv8.1-a+crc+crypto',",
        "            march_arm64_neon_crypto,",
```

- [ ] **Step 2: 重新生成：**

```bash
python3 scripts/generate_sdc_fuzz_cases.py
```

Expected: `Generated 100 cases ...`，Clusters 三类均衡。

- [ ] **Step 3: 生成器确定性验证（硬闸）：**

```bash
git status --porcelain tests/cpu/arm64/sdc_fuzzing/
git diff tests/cpu/arm64/sdc_fuzzing/meson.build
```

Expected: 仅 `M tests/cpu/arm64/sdc_fuzzing/meson.build` 一个文件被修改；diff **恰好一行**：`- '-march=armv8.1-a+crc+crypto',` → `+ march_arm64_neon_crypto,`。若 100 个 case 文件或 cases_index.csv 出现任何改动 → 停下排查生成器确定性（头注释声明 Deterministic），不得提交。

- [ ] **Step 4: 构建绿：**

```bash
meson setup --reconfigure builddir
./scripts/build.sh
```

## Task 5: 全量验证（rule 2，100% 实证）

**Files:** 无（验证 Task 2–4 的结果）。

- [ ] **Step 1: 改后 `-march` 多重集与改前逐字节相等：**

```bash
ninja -C builddir -t commands | grep -o -- '-march=[^ ]*' | sort | uniq -c > /tmp/march-multiset-after.txt
diff /tmp/march-multiset-before.txt /tmp/march-multiset-after.txt && echo MARCH-IDENTICAL
```

Expected: diff 空输出 + `MARCH-IDENTICAL`（编译命令不变 ⇒ 二进制语义不变的最强证据）。

- [ ] **Step 2: `--list-tests` 指纹不变：**

```bash
./builddir/sdcshield --list-tests > /tmp/listtests-after.txt
diff /tmp/listtests-before.txt /tmp/listtests-after.txt && echo LISTTESTS-IDENTICAL
```

Expected: 逐字节相同。

- [ ] **Step 3: 标准回归 + 档位代表用例实跑：**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 | tail -3     # exit: pass
./builddir/sdcshield -e neon_add -t 2000 -n 1 | tail -3   # NEON+crypto 档代表（neon_test.cpp 的注册 ID 实为 neon_add）
./builddir/sdcshield --quality=-1 -e crc32 -t 2000 -n 1 | tail -3  # crc 档代表
```

Expected: 三者全 pass（引用真实输出）。

- [ ] **Step 4: x86-64 不回归（检视取证）：**

逐处引用守卫证据：全部 16 个替换点位于 `if host_machine.cpu_family() == 'aarch64'` 守卫内（tests/cpu/meson.build L1135/L1248/L1358、sve/meson.build L27、arm64/meson.build L10、arithmetic_arm 守卫 L1395、framework/device/cpu/arm64 L4、生成物守卫）；根定义对 x86 是惰性字符串；摘要块 aarch64 守卫。产出 `grep -n "cpu_family() == 'aarch64'"` 清单佐证。

## Task 6: 文档同步（README.md、CLAUDE.md、adding-tests.md）

**Files:**
- Modify: `README.md`（「从源码构建」节，L182 clang/BiSheng 引用块之后插入新引用块）
- Modify: `CLAUDE.md`（「Build & run」节，`Meson changes need...` 段之前插入档位表）
- Modify: `docs/research/adding-tests.md`（§4 表格与独立库清单更新为变量名 + 补齐缺失档位；顺带修正 tests_set_sve 行的陈旧 `-msve-vector-bits=128` 描述——现况是无 vector-bits 的分发 TU + 四档实例化循环）

- [ ] **Step 1: README.md 在 L183（clang/BiSheng 块之后、`## 离线与多版本构建` 之前）插入：**

````markdown
> **ARM64 ISA 编译档位（NEON/SVE/SVE2/SME，统一指定）**：哪些用例需要什么编译选项、在构建系统里**统一**在哪儿指定——全部 6 档 march 集中定义为根 `meson.build` 的 `march_arm64_*` 变量（单一事实来源；所有子目录经 `subdir()` 共享作用域引用，绝不散落字面量；`meson setup` 末尾的 `ARM64 ISA march tiers` 摘要会打印全部档位）：
>
> | 变量（根 `meson.build`） | 编译选项 | 用到的用例组（static_library） |
> |---|---|---|
> | `march_arm64_crc` | `-march=armv8.1-a+crc` | `tests_crc`（`crc32*`，ACLE `__crc32b/h/w`） |
> | `march_arm64_neon_crypto` | `-march=armv8.1-a+crc+crypto` | `tests_arm64`（NEON+AES `vaeseq`）、`tests_arithmetic_arm_*`（GMP/ACL/compiler-rt/AOR）、`tests_sdc_fuzzing`（自动生成）、`framework_arm64` |
> | `march_arm64_sve` | `-march=armv8.2-a+sve` | `tests_arm64_sve`（sve512_* 全族）、`tests_sve`（Eigen SVE 分发 TU）、`tests_sve_vla`（sleef_sve + sve_rot 探针族）、`tests_sve_crt`、`tests_sve_avx53{,_ipsec}`（SVE 版 avx53 套件） |
> | `march_arm64_sve_mm` | `-march=armv8.2-a+sve+f32mm+f64mm` | `tests_arm64_sve_mm`（FMMLA；**刻意不加 `+sve2`**——F32MM/F64MM 与 SVE2 正交，实测有 FMM 无 SVE2 的芯片可跑） |
> | `march_arm64_sve2` | `-march=armv9-a+sve2+bf16+f32mm+f64mm` | `tests_arm64_sve2`（`sve2_cross_precision_arm`） |
> | `march_arm64_sme` | `-march=armv9-a+sve+sme+sme-f64f64` | `tests_arm64_sme`（ZA tile 族） |
>
> NEON 基线无需任何 `-march`（armv8-a 即含 ASIMD）；`zpclmul{,_rep}` 用函数级 `target("+crypto")`，无库级 march。**Eigen 特例**：Eigen 的 SVE packet 编译期定宽，故按 128/256/512/1024 四档各实例化一个库（`march_arm64_sve` + `-msve-vector-bits=<tier>` + `-DEIGEN_ARM64_SVE_VL=<tier> -DEigen=EigenSVE<tier> -DEIGEN_ARM64_USE_SVE`），运行时分发 TU 按工人 pinned VL 选档。SVE-FMMLA/SVE2/SME 档编译器不支持时探测剔除（message 提示、不阻断；本机 GCC 12.3 即剔除了 SME 档）。新写 SVE 测试选库规则见 [docs/research/adding-tests.md](docs/research/adding-tests.md) §4。
````

- [ ] **Step 2: CLAUDE.md「Build & run」节，在 `Meson changes need` 段之前插入（紧凑版）：**

````markdown
ARM64 ISA march 档位统一于根 `meson.build` 的 `march_arm64_*` 变量（单一
事实来源，子目录经 `subdir()` 作用域引用，绝不散落字面量；`meson setup`
摘要 `ARM64 ISA march tiers` 打印全部档位）：
- `march_arm64_crc` = `-march=armv8.1-a+crc` → `tests_crc`（ACLE CRC）
- `march_arm64_neon_crypto` = `-march=armv8.1-a+crc+crypto` → `tests_arm64`/
  `tests_arithmetic_arm_*`/`tests_sdc_fuzzing`/`framework_arm64`
- `march_arm64_sve` = `-march=armv8.2-a+sve` → 全部 SVE1 库 + Eigen 四档实例化
- `march_arm64_sve_mm` = `-march=armv8.2-a+sve+f32mm+f64mm`（FMMLA，刻意无 `+sve2`）
- `march_arm64_sve2` = `-march=armv9-a+sve2+bf16+f32mm+f64mm`
- `march_arm64_sme` = `-march=armv9-a+sve+sme+sme-f64f64`
NEON 基线无需 march（armv8-a 即含）；Eigen SVE 定宽 packet 按四档实例化
（`-msve-vector-bits=<tier> -DEIGEN_ARM64_SVE_VL`/`-DEigen=EigenSVE<tier>`）+
运行时分发。新用例选库：`docs/research/adding-tests.md` §4；完整表：README。
````

- [ ] **Step 3: docs/research/adding-tests.md §4 更新：**

1. 表格 `tests_set_sve` 行的编译选项列改为：`march_arm64_sve`（无 `-msve-vector-bits`）——Eigen 分发 TU；并注明 payload 四档循环在 tests/cpu/meson.build 的 `foreach spec : [['gemm','128'],...]` 块（修正当前行的陈旧 `-msve-vector-bits=128` 描述）。
2. `tests_set_sve_vla` 行同样改为引用 `march_arm64_sve` 变量名。
3. 「另外几个独立 static_library」清单补全并统一到变量名：`tests_crc`→`march_arm64_crc`；`tests_arm64`→`march_arm64_neon_crypto`；`tests_arm64_sve`→`march_arm64_sve`；**新增** `tests_arm64_sve_mm`→`march_arm64_sve_mm`、`tests_arm64_sve2`→`march_arm64_sve2`、`tests_arm64_sme`→`march_arm64_sme`、`framework_arm64`/`tests_sdc_fuzzing`→`march_arm64_neon_crypto`、`tests_sve_avx53{,_ipsec}`/`tests_sve_crt`→`march_arm64_sve`。
4. 表格下方加一句：档位变量统一定义于根 `meson.build`（单一事实来源），改动档位只改根一处。

- [ ] **Step 4: 文档事实核对（对照本计划「调研结论」表逐行核对三份文档，不得出现任何与源码不符的库名/标志拼写）。**

## Task 7: 提交与推送

- [ ] **Step 1: 确认分支与暂存范围：**

```bash
git branch --show-current        # 必须 feat/arm64-march-tiers-unify
git add meson.build tests/cpu/meson.build tests/cpu/sve/meson.build \
        tests/cpu/arm64/meson.build tests/cpu/arithmetic_arm/meson.build \
        framework/device/cpu/arm64/meson.build scripts/generate_sdc_fuzz_cases.py \
        tests/cpu/arm64/sdc_fuzzing/meson.build README.md CLAUDE.md \
        docs/research/adding-tests.md docs/superpowers/plans/2026-11-03-arm64-march-tiers-unify.md
git status --porcelain           # 逐项核对，不得混入无关未跟踪文件
```

- [ ] **Step 2: DCO 提交（单 commit = 单单元）：**

```bash
git commit -s -m "feat: ARM64 ISA march 档位统一为根 meson.build march_arm64_*（单一事实来源）

16 处散落的 aarch64 专属 -march 字面量（NEON+crypto/SVE1/FMMLA/SVE2/SME/ACLE-CRC
六档，含自动生成的 sdc_fuzzing 一处）全部改为引用根 meson.build 新定义的
march_arm64_{crc,neon_crypto,sve,sve_mm,sve2,sme} 变量；meson setup 新增
ARM64 ISA march tiers 配置摘要。编译命令前后逐字节不变（ninja -t commands
-march 多重集 diff 为空）；--list-tests 逐字节不变；zstd19/neon_add/crc32
回归全 pass；x86-64 全部改动点在 aarch64 守卫内或为惰性数据。README/CLAUDE.md/
adding-tests.md 同步档位→用例组映射表（含 Eigen SVE 四档定宽实例化特例与
FMMLA 刻意不加 +sve2 的依据）。"
git log -1 --format=%B | tail -2   # 最后一行必须是 Signed-off-by: wangxu <wangxumarshall@qq.com>
```

Expected: 最后一行 = `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无内容、无 Co-Authored-By。

- [ ] **Step 3: 推送 feature 分支：**

```bash
git push -u origin feat/arm64-march-tiers-unify
```

---

## Self-Review 结论

- **覆盖**：用户三项要求——(1) 调研（「调研结论」表，16 处全覆盖、含无需档位的 NEON 基线与函数级 target 特例）；(2) 统一指定（Task 2–4，单一事实来源 + 生成器同步）；(3) README/CLAUDE.md 构建章节写清楚（Task 6，另加 adding-tests.md 顺带修正其陈旧行）——均有对应 Task。
- **无占位符**：全部编辑点给出逐字 old/new；验证命令给出期望输出。
- **类型一致**：6 个变量名在 Task 2 定义、Task 3/4/6 逐字引用一致；局部探测变量名保留。
- **边界诚实**：本机可全量实证，无 gem5-fi 需求（编译命令不变即语义不变的证明）。
