# SVE 全 VL 验证运动 — 用例枚举与 tag 调和结论

- 日期：2026-10-28（Task 1 产物）
- 分支：`feat/sve-all-vl-verification-2026-10-28`（基 `12ccda54`）
- 二进制：`./builddir/sdcshield`（version `sdcshield-12ccda540fd4`，与本清单同源）
- 上游：plan `docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md` Task 1；
  spec `docs/superpowers/specs/2026-10-28-sve-all-vl-verification-campaign-design.md` §1/§3
- 清单工件：同目录 `sve-test-list.txt`（190 行，每行 `<test_name>`，无时长列 → 沿用全局 `-t`；
  格式契约 = `framework/sandstone_tests.cpp` `parse_test_list_line`（:215），`#` 注释、可带 `[时长|default]`）

## 1. 两路枚举（原始数字，全部真实输出）

| 路 | 命令 | 结果 |
|---|---|---|
| name 路 | `--list-tests`（本机默认 PROD 集，spec 过滤后） | **479** 个 |
| name 路 | `grep -i sve` 于 479 | **188** 个（= campaign 口径的"188 个 SVE1 用例"） |
| tag 路 | `grep -rln "ARM64_REQUIRES_SVE\b" tests/cpu/` → 172 个文件 → 提取 `DECLARE_TEST` 名 | **186** 个 |

spec §3 写作时的基线是"171 个已打标签 vs 188 个 sve 命名"；当前实测 tag 路 186
（含不在默认集者，见 §2.1），差异来自 f597dc9a 之后的增量打标签（含 eigen 逐档 TU ae01af96）。

## 2. 调和（`comm`，LC_ALL=C 归一后）

| 集合 | 个数 |
|---|---|
| 交集（named ∧ tagged） | 181 |
| tagged-but-not-named | 5 |
| named-but-not-tagged | 7 |
| 并集（union） | 193 |
| **清单（union − 不在本机默认集的 3 件）** | **190** |

结果已用与 locale 无关的 `grep -xFvx` 交叉复核（两向差集均一致：5 / 7）。

### 2.1 tagged-but-not-named（5 件，逐件定性）

| 测试 | 在默认 479 集？ | 机制（源码证据） | 清单决定 |
|---|---|---|---|
| `eigen_gemm_double_max_arm` | **在**（名字不含 "sve"，name 路盲区） | `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:202-208`：PROD + `.arm64_requires = ARM64_REQUIRES_SVE`（:208）；Eigen 逐档分派 TU（commit ae01af96，128/256/512/1024 变体按框架 pin 的 VL 选择） | **并入** |
| `libomp_ctx_arm` | **在**（同上盲区） | `tests/cpu/arm64/libomp_ctx_arm.cpp:550-562`：PROD（:561）+ tag（:562）；真 SVE 数据面——`#include <arm_sve.h>`（:60）、`arm64_vl.h`（:61），描述明言 "SVE f64 FMLA bursts … all-VL 128..1024-bit via arm64_require_sve_vl" | **并入** |
| `neon_add_sve` | **不在** | `tests/cpu/sve/fma2/neon_add_sve.cpp:141` `quality_level = TEST_QUALITY_BETA` → 被质量门挡在默认 PROD 集之外（`framework/sandstone_tests.cpp:95-96`）；`--quality=-1 --list-tests` 可见，二进制内存在 | **不并入**（见 §3） |
| `arm64_sdc_sve` | **不在** | `tests/cpu/sve/mem/arm64_sdc_sve.cpp:196` `TEST_QUALITY_BETA` → 同上质量门 | **不并入** |
| `crt_builtins_sve` | **不在（二进制内都没有）** | 构建门：`tests/cpu/sve/meson.build:273-297` 要求 `/usr/lib/clang/17/lib/aarch64-openEuler-linux-gnu/libclang_rt.builtins.a`，本机缺失 → 该 TU 未编译（实测 `--quality=-1 --list-tests` 亦无此名）。若入清单会触发 `Unknown test:`（`framework/sandstone_tests.cpp:271-274`），直接违反 Task 1 Step 4 的可执行性判据 | **不并入** |

即：brief 预期的 "gemm_double_max_arm" 实名为 `eigen_gemm_double_max_arm` ✓；同类盲区还多出
`libomp_ctx_arm`（同样"tag 在、默认集在、名字无 sve"，与 gemm 完全同构，并入无争议）；
另有 3 件 tag 在但**不在本机默认集**（2 件 BETA 质量门 + 1 件构建门），按 brief Step 2 第三条
"不在本机默认集的 SVE 测试列出但不入清单"处理——该条括号里预期的"被 spec 过滤的 SVE2/SME 4 件"
实际不在两路并集里（见 §2.3），真正落入该条的是这 3 件。

### 2.2 named-but-not-tagged（7 件，全部 ipsec，逐件核实门控方式）

`ipsec_3des_docsis_aes_cmac_sve_wide`、`ipsec_3des_docsis_aes_xcbc_sve_wide`、
`ipsec_aes128_cbc_aes_cmac_sve`、`ipsec_aes192_cbc_xcbc_96_sve`、`ipsec_aes192_ctr_xcbc_sve`、
`ipsec_aes192_gcm_sve`、`ipsec_aes192_gcm_sve_wide` —— 7 件全部**在本机默认 479 集内 → 并入清单**。

**门控方式的诚实结论：这 7 件没有任何 SVE 门（无 tag、无 HWCAP 探测、无 init 门控），
且这不是违规，而是"无 SVE 数据面"的合法免检**：

- 7 个文件对 `arm_sve.h` / `arm64_vl.h` / `sv*` 内建 / `arm64_requires` 的匹配数为 **0**
  （实测 grep，7 文件全 0 hit）；计算路径全部是 OpenSSL EVP（如
  `tests/cpu/sve/ipsec/ipsec_aes192_gcm_sve.cpp:203-210`：`.test_init = aes_gcm_avx_init` … EVP 调用）。
- 唯一的声明级"门"是 `.minimum_cpu = IPSEC_X86_GATE(...)`，而该宏在 AArch64 展开为 **0**
  （`framework/sandstone_ssl.h:19-23`：x86 保留原门，"on AArch64 … OpenSSL's own runtime
  dispatch selects its aarch64 assembler backends regardless"，故门被丢弃）。
- checker 判定口径一致：`scripts/check-arm64-requires.py:99-100`——无内建/无 SVE 头的 TU
  `continue  # no vector datapath: tag not required`。Step 3 实跑零违规（§4）。
- 对比同族已打标签的 17 件（named 的 24 件 ipsec − 本 delta 的 7 件 = 17，f597dc9a 打的），它们**有**真数据面：如
  `tests/cpu/sve/ipsec/ipsec_aes128_cbc_hmac_sha1_sve.cpp:36` 含 `arm64_vl.h`、`:94` 调
  `svcntw()`、`:352` 挂 tag——打标签的分界线就是"有无 SVE 数据面"，7 件免检件落在无数据面一侧。

矩阵语义注记：这 7 件因无 SVE 数据面，三档 VL（128/256/512）下行为完全相同（OpenSSL 自行分派），
它们的格不构成"VL 敏感性"证据，只构成"同一条件下可跑绿"证据。

**brief 预期 vs 实测的偏差（如实记录）**：brief/spec 预期 named-but-not-tagged 是
"5 个 `sve_rot_ldr_*` 走 `compiler_minimum_device` + init 门控"。实测 **sve_rot_ldr 家族 8 件
全部已打 tag**（`sve_rot_ldr_at_top.cpp:210`、`_rowmajor.cpp:196`、`_k3res.cpp:256`、
`_k5inter_rand.cpp:229`、`_k7pattern.cpp:312/320/328/336`，均 f597dc9a 2026-10-27 11:22 打上，
晚于 spec 头脑风暴所引用的状态）→ 该家族整体落在交集里，不产生 delta。spec §3 的
"171 已打标签/5 个 sve_rot_ldr 无 tag" 基线在执行时已过时；当前真实基线即本文件 §1/§2。

### 2.3 不在本机默认集的 SVE 测试（范围外，列出但不入清单）

- **spec §1 点名的 SVE2/SME 4 件**：`sme_fmopa_stress_arm`、`sme_fmopa_za_arm`、
  `sme_za_tileview_arm`、`sve2_cross_precision_arm`。它们挂的是 SME/SVE2 tag（非本枚举的
  `ARM64_REQUIRES_SVE`），且被 spec 过滤出默认集（`framework/sandstone_tests.cpp:107-114`，
  本机无 SVE2/SME）→ **不出现在两路任何一路**，天然不在 union，无需剔除动作；
  范围外依据 = spec §1（Phase 1 / PR #242 四档实证已覆盖）。
- **实测额外发现的不在默认集者（union 内，已剔除）**：`neon_add_sve`、`arm64_sdc_sve`
  （BETA 质量门）、`crt_builtins_sve`（构建门，见 §2.1）。

## 3. 清单成员资格（190 行的构成与裁决）

```
sve-test-list.txt = union(193) − {neon_add_sve, arm64_sdc_sve, crt_builtins_sve}
                  = 188 named + eigen_gemm_double_max_arm + libomp_ctx_arm
                  = 190 行（wc -l 实测 190；无重复行）
```

**裁决依据**：brief Step 2 的三条规则里，"不在本机默认集 → 列出但不入清单"（第三条）对
`neon_add_sve`/`arm64_sdc_sve`/`crt_builtins_sve` 直接适用；campaign 的行定义口径是
"本机默认 PROD 集中的 SVE 用例"（spec §1"188 个 SVE1 用例"），BETA 质量门是项目自身对
未熟化用例的既定隔离，不因本运动 silently 升格。`crt_builtins_sve` 的剔除另有硬约束：
不在二进制内 → 清单会报 `Unknown test:`，违反 Step 4 判据。

**规则冲突的透明声明**：brief 第一条"tagged-but-not-named → 并入清单"按字面也覆盖 BETA 两件
（它们既是 tagged-but-not-named 又是不在默认集）。本文件按第三条（更具体、且与 campaign
口径一致）裁决为不入清单。若 controller 复审认为 BETA 两件应当纳入（清单变 192 行、
矩阵 576 格），只需在 `sve-test-list.txt` 追加两行——两件均在二进制内，`--test-list-file`
显式请求会绕过质量与 spec 过滤照常执行（`framework/sandstone_tests.cpp:101-104`）。

## 4. checker 零违规门（Step 3，真实输出）

```
$ python3 scripts/check-arm64-requires.py
ok: every vector test declares arm64_requires
exit=0
```

## 5. 可执行性验证（Step 4，全部真实输出）

### 5.1 清单格式门（判据：无 "Syntax error" / "Unknown test"）

```
$ ./builddir/sdcshield --test-list-file docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt -t 1000 -n 1
command-line: 'sdcshield --test-list-file docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt -t 1000 -n 1'
version: sdcshield-12ccda540fd4
tests:
- test: acl_gemm_sve
  result: timed out
  ...
  - { level: error, text: 'E> Child 82233 did not exit, sending signal SIGQUIT' }
exit: invalid        （进程退出码 2）
```

`grep -c "Syntax error\|Unknown test"` 于该输出 = **0**：190 行全部解析通过、名字全部
命中二进制内测试，运行直接进入第一个用例的执行——**清单工件本身合格**。
`exit: pass` 未达成，原因是第一个用例 `acl_gemm_sve` 硬超时（见 5.3，与本清单无关的
既有用例/主机属性，且按 brief 预案的 fallback 以 5.2 的单测抽查补足执行性证据）。

### 5.2 代表性单测抽查（brief Step 4 fallback + 两个 delta 类代表）

brief 指定的 3 个代表 + 本枚举两个 delta 类各 1 个（未打标签类 `ipsec_aes192_gcm_sve`、
并入清单的无 sve 命名类 `libomp_ctx_arm`），全部 `-t 1000 -n 1`：

```
$ ./builddir/sdcshield -e <test> -t 1000 -n 1     # 五个逐一执行
== sve_rot_ldr_at_top:               exit=0 | exit: pass
== fma_tail_sve:                     exit=0 | exit: pass
== ipsec_aes128_cbc_hmac_sha1_sve:   exit=0 | exit: pass
== ipsec_aes192_gcm_sve:             exit=0 | exit: pass
== libomp_ctx_arm:                   exit=0 | exit: pass
```

### 5.3 发现（如实记录，不在本 Task 修复）：`acl_gemm_sve` 在本机短 `-t` 下必然硬超时

**现象**（两次独立复现 + 全核复现，全部真实输出）：

| 运行 | 结果 |
|---|---|
| 清单跑 `-t 1000 -n 1`（test 1 即 `acl_gemm_sve`） | `result: timed out`，runtime 305662 ms，SIGQUIT，`exit: invalid`（退出码 2） |
| 单测复现 `-e acl_gemm_sve -t 1000 -n 1` | `result: timed out`，runtime 301735 ms，SIGQUIT，`exit: invalid`（确定性成立） |
| 全核 `-e acl_gemm_sve -t 2000`（Task 2/3 的跑批参数） | `result: timed out`，两个 fork child runtime ~317-319 s，128 线程全报 `E> Thread is stuck`，`exit: invalid`（退出码 2） |

**机制链**（逐环 file:line 证据）：

1. `tests/cpu/sve/arithmetic/acl_gemm_sve.cpp:76-81`：dim 无显式旋钮时走
   `sdc_auto_square_dim(4, 4, 2.0, 600, thread_count(), 64, 4096, …)`——**内存自适应模型**
   （`framework/sdc_stress_config.h:290`，目标 = 物理内存的 600‰），不是时间校准模型。
   本机 61 GiB RAM：全核 127 线程 → dim=3113（框架自己的日志：
   `W> [压力映射] acl_gemm_sve.dim = 3113 (来源: 内存自适应 600‰, 全机 127 线程) → 预期合计 ~36.7 GiB = 物理内存的 60.0%`）；
   `-n 1` → 模型值 ~33820 被 hi=4096 钳住 → dim=4096。
2. 该用例 run 内核对每 (i, jbase, k) 做标量 B 行搬运（acl_gemm_sve.cpp:119-129），
   单次 TEST_LOOP 迭代 = O(dim³) → 本机 **>300 s/迭代**（实测 utime 224-230 s 时被杀，
   迭代尚未完成）。
3. `TEST_LOOP` 只在迭代**之间**查截止线（`framework/sandstone.h:144-148` →
   `framework/sandstone_run.cpp:620-625`），单迭代超期则无法让出。
4. 硬超时地板：`framework/sandstone.cpp:179-188` `test_timeout() = max(-t×5+30s, 300s)`
   ——`-t` ≤ 54 s 时耐心恒为 300 s。`-t 1000/2000` 均 < 迭代耗时 →
   `framework/sandstone_run.cpp:1111-1122` SIGQUIT → `result: timed out` →
   `framework/sandstone_run.cpp:1650-1653`：TimedOut 在未开 `--ignore-timeout`
   （`framework/sandstone_opts.cpp:135-136`，注意 plan Task 2 命令里的 `-Y` 只是
   `--yaml` 输出格式，**不是** ignore-timeout）时**整个 run 立即 EXIT_INVALID**——
   即全量跑批会在第 1 个用例（字母序恰好是 acl_gemm_sve）中止。

**对 campaign 的影响与处置建议（进 Task 2 分诊，本 Task 不携带代码修复，遵守 brief）**：
实测的 ~302-319 s 是被杀时刻的**下界**（迭代从未完成，真实单迭代耗时未测得）；
`-t` ≤ 54 s 时耐心恒为 300 s 地板 < 该下界，**必然超时**；把 `-t` 抬到更大
（耐心 = -t×5+30）只有当耐心超过真实单迭代耗时才可能让首次迭代完成后经截止线
正常收尾——真实耗时未测，不作为可靠杠杆。确定性杠杆：
(a) 跑批给 `-O acl_gemm_sve.dim=<有界值>`（如 512，旋钮链在 acl_gemm_sve.cpp:76-78）；
(b) `--ignore-timeout` 让超时格降级为记录而非中止全 run；
(c) Task 7 代码修复（内存模型 hi 收紧 / 时间校准 / 迭代内截止检查）。
同类风险预判：凡 `sdc_auto_square_dim(…, 600, …)` 且 run 单迭代 O(dim³) 的用例同构
（Task 2 分诊时按此 grep 排查，PR #294 同构族教训）。

**顺带核实的一个框架行为**（影响后续日志考古）：默认日志路径是目录 `.` →
`open` 得 EISDIR → 落时间戳 yaml 于 cwd 且 `delete_log_on_success=true`
（`framework/logging.cpp:503-521`）——**只有失败的 run 会留下 `sdcshield-<时间戳>.yaml`**，
绿的 run 不留痕；Task 2/3 用 `-o <LOG>` 显式落档即可绕开该行为。

## 6. 工艺发现：`comm` 的 locale 陷阱（后续 Task 8 机械提取必须避坑）

brief 的字面命令会产出**垃圾结果**，机制有二，实测记录如下：

1. `--list-tests` 的输出是 ELF 注册序（eigen 在前、sve_rot 居中），**根本不是 sorted 序**；
2. 本机 shell locale 为 `en_US.UTF-8`，其 collation 对 `_` 的加权与 C collation 不同，
   `sort`（UTF-8）与 `comm` 的序检查互相不认。

实测第一遍字面执行：`comm` 报 `file 2 is not in sorted order` 等警告，输出 178/180/8 的
伪差集。修正法：**sort 与 comm 全链 `LC_ALL=C`**，再用与 locale 无关的 `grep -xFvx` 交叉
复核（5/7 两向一致才采信）。Task 8 从日志机械生成矩阵时同理（`sort`/`uniq`/`comm` 一律
显式 `LC_ALL=C`）。

## 7. 边界与诚实声明

- 枚举的完备性口径是**项目自身的 tag 政策**：`ARM64_REQUIRES_SVE` 只存在于 `tests/cpu/`
  （实测全仓 grep），checker 的扫描域即 SVE 用例的全部栖息地，checker 绿 + 双路并集
  → 在该政策下无第三路遗漏。若存在"有 SVE 数据面但既无 tag 又无 sve 命名且不在 checker
  扫描域"的用例，那是 checker 政策本身的洞，不是本枚举的洞。
- 本机（cortex x3b, 48-d02-00）SVE-256 原生、无 SVE2/SME：479/188 均为**本机默认集**数字，
  换机会变（CLAUDE.md 既有口径）。
- `neon_add_sve` / `arm64_sdc_sve` / `crt_builtins_sve` 三件在三档 VL 矩阵中为**显式空格**
  （非边界格、非绿格），理由见 §2.1/§3。
