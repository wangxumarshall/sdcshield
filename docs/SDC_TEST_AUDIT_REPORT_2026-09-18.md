# SDCShield 用例审计报告：无法有效压测对应 SDC 单元故障的测试

**日期**: 2026-09-18
**范围**: tests/common/ + tests/cpu/ 全部 301 个测试源文件（356 个 DECLARE_TEST，308 个唯一测试 ID），x86-64 参考实现与 ARM64 移植一并审计
**方法**: 逐族阅读验证逻辑（golden 如何产生、如何比较）+ 在鲲鹏 920 实机运行验证可疑用例
**结论**: 确认 11 类缺陷（D1–D11）。其中 2 类为**假通过（false-pass，测试静默通过但检不出目标单元的 SDC）**，涉及 22 个默认 PROD 质量等级的用例。

## 审计判据

一个用例若满足以下任一条，即判定为"无法压测对应 SDC 单元故障"：

1. **假通过**：比较逻辑数学上不可能检出目标单元的位错误（容差过宽 / 恒真）
2. **空转**：声称压测某单元，实际执行的指令不经过该单元（或编译器已消除）
3. **同源 golden**：golden 与被测结果由同一硬件/软件路径计算，确定性缺陷两边同错
4. **质量门控空洞**：某单元唯一的有效用例被 quality_level 门控在默认运行之外
5. **违反占位诚实规则**：不做事却返回 EXIT_SUCCESS 而非 EXIT_SKIP

## 缺陷总表（按严重度排序）

| ID | 用例/家族 | 声称压测单元 | 实际行为 | 严重度 | 类型 |
|----|----------|-------------|----------|--------|------|
| D6 | vmx_*（9 个） | 虚拟化 vmexit 路径 | 无 VM/无 guest，栈变量 memcpy 恒真比较，无 TEST_LOOP，全部 `exit: pass` | **致命** | 假通过+空转 |
| D11 | crc 家族（13 个） | CRC 指令/CRC 单元 | crc1 与 crc2 用**同一指令同一数据算两遍**再比较；确定性 CRC 单元缺陷两边同错 | **高** | 同源 golden |
| D1 | fma | FMA 单元 | 1e-6 相对容差；`vfmaq_f32` 与 `fmaf` 均为 IEEE 单舍入 FMA，必须位相等 → 1 位尾数错误（≈1.2e-7）**通过** | **高** | 假通过 |
| D2 | fma_patterns_avx512_{pd,ps} | FMA 模式压测 | pd 版 1e-12 绝对下限、ps 版 1e-5f 绝对下限 —— 任何单比特尾数错误均小于绝对下限 | **高** | 假通过 |
| D3 | acl_gemm | ACL NEON GEMM | **已修复 (2026-09-19)**：原 init/run 双处 `return EXIT_SKIP` + 无 log_skip；真根因是 init 漏赋 `test->data`（run 解引用垃圾指针恒 SIGSEGV，被误归因为 fork-safety）。现 vendored ACL v23.02 + import_memory + long-double golden + 1e-4 绝对容差，NEGEMM 真跑 `exit: pass` | 中 → **已关闭** | 已修复 |
| D4 | 73 个文件的 reload_buf 模式 | 存储→加载路径 | `memcpy→memcpy→memcmp` 恒真"一致性检查"；编译器可完全消除。多数家族中仅为冗余装饰，但 vmx/crc 家族中它是**唯一**的"验证" | 中 | 空转检查 |
| D7 | neon_add / arm64_sdc / arm_crypto | NEON 加法 / CRC 数据通路 / AESE | 三个基础 ARM64 用例标为 BETA，默认 `--quality=2` 不运行 → 默认生产运行中该三单元**零覆盖** | 中 | 质量门控空洞 |
| D9 →并入 D11 | crc32 三兄弟 | CRC32C 指令 | 同 D11 | — | — |
| D5 | mesh_upi_* 家族（26 个） | UPI/互联（x86 命名移植） | 求和校验可被补偿性错误抵消；单插槽 ARM 上实际压的是 NEON 载入+L3/DRAM 而非"互联"（命名误导）；但每向量另有 4-lane 逐元素比较兜底 | 低-中 | 弱校验+命名误导 |
| D10 | mite | 解码/分支预测 | 512 元素求和校验，补偿性错误可抵消 | 低 | 弱校验 |
| D8 | ist/ist_array | ARM IST 自检 | 诚实占位 skip（有 log_skip + EXIT_SKIP），**非缺陷**，记录在案 | 无 | 合规占位 |

## 关键证据（file:line）

### D6 — vmx 家族假通过（实机验证）

全部 9 个用例在本机（无虚拟化参与）运行 `exit: pass`：

```
$ ./sdcshield -e vmx_vmexit_cpuid -t 2000 -n 1   → exit: pass
$ ./sdcshield -e vmx_io_exit -t 1000 -n 1        → exit: pass
（其余 7 个同，全部 pass）
```

- `vmx_vmexit_cpuid.cpp:21`：`bool data_ok = true;`（注释"只要不产生异常就算成功，不需要验证具体值"）—— 恒真。
- `vmx_vmexit_cpuid.cpp`：整个 run 函数**无 TEST_LOOP/test_time_condition**，单次 MRS 读 MIDR_EL1 即返回；声称 "Have a guest trigger vmexits"，实际运行在宿主 EL0/EL1，**不存在任何 VM/guest/vmexit**。
- `vmx_io_exit.cpp:8-28`（ARM 分支）：注释自述"跳过实际测试，仅返回成功"，却返回 **EXIT_SUCCESS** 而非 EXIT_SKIP —— 直接违反 CLAUDE.md 占位诚实规则（"A no-op test that returns success is a bug"）。
- `vmx_vmexit_vmcall.cpp:17-35`："模拟 hypercall" = 向栈变量写常数 + `__sync_synchronize` + memcpy 读回。
- `vmx_vmexit_to_dr7` / `to_cr8` / `from_cr8` / `pause` / `invd` / `vmxmsr`：同模式（栈变量往返、"模拟"、from_cr8 第 28 行 `data_ok = true`）。
- 9 个文件 grep `TEST_LOOP|test_time_condition` 命中数 = **0** → 零持续压测。

### D11 — CRC 家族同源 golden

`tests/cpu/crc/crc32.cpp:27-48`：

```c
uint32_t crc1 = 0xFFFFFFFF;
for (i...) crc1 = __crc32b(crc1, local_data[i]);   // 硬件指令算第一遍
crc1 = ~crc1;
__sync_synchronize();
uint32_t crc2 = 0xFFFFFFFF;
for (i...) crc2 = __crc32b(crc2, local_data[i]);   // 同一指令同一数据再算一遍
crc2 = ~crc2;
bool data_ok = (crc1 == crc2);                     // ← 唯一校验
```

若 CRC 单元多项式位固化错误（deterministic defect），两次结果**同样地错** → 恒通过。此为重演一致性检查，不是正确性检查。**无任何软件查表 golden。**

同构模式蔓延 13 个用例：`crc32`、`crc32_fixed`（crc32_fixed.cpp:33,41）、`crc32_fixed_shuffled`、`isal_crc_ieee/iscsi/t10dif/crc64_{ecma182,iso,jones}_{norm,refl}`（如 isal_crc_ieee.cpp:30-38，`crc32_ieee()` 调两遍）。

对照（合规范例）：`isal_igzip` 同时做字节精确压缩输出比对（igzip.cpp:189）**和** inflate 解压回读 memcmp（igzip.cpp:207）—— 压缩族是扎实的，唯独 CRC 族有此结构性空洞。

### D1/D2 — FMA 容差假通过

`tests/cpu/fma/fma.cpp:20-30`：

```c
float tol = 1e-6f * fmaxf(fabsf(x[i]), fabsf(y[i]));
if (diff > tol && diff > 1e-7f) return false;   // diff ≤ 1e-7 直接放过
```

ARM64 上 `vfmaq_f32`（被测）与 `fmaf`（golden）都是 IEEE-754 单舍入 FMA，**规范要求位相等**。1 位尾数翻转的相对误差 ≈ 1.19e-7 < 1e-6 → **静默通过**。仓库自己的 meson 注释（tests/cpu/arm64/meson.build:60-63）承认："fma.cpp compares with a 1e-6 tolerance, so a 1-bit FSU SDC passes"。补偿用例 `fsu_byteexact_arm` 存在且字节精确，但 `fma` 自身仍以 PASS 上报。

`fma_patterns_avx512_pd.cpp:76-80`：`tol = 1e-15*max` 但 `diff > 1e-12` 才算失败 —— **1e-12 绝对下限**放过一切单比特尾数错误（double 1 ulp ≈ 2.2e-16 量级）。ps 版（:77-78）1e-5f 绝对下限更宽。

同目录 `fma_tail_avx2`/`fmatail_avx2` 却用字节精确 memcmp（fma_tail_avx2.cpp:64）—— 同一目录内标准不一致。

### D3 — acl_gemm 永久死码

`tests/cpu/arithmetic_arm/acl_gemm.cpp`：init 第 59 行、run 第 109 行均无条件 `return EXIT_SKIP;`，**前面没有 log_skip 调用**（理由只写在注释里）。原因已查明并记录（ACL NEGEMM 在 fork-per-iteration 模型下 SIGSEGV），但：(a) 违反"log_skip 后再 EXIT_SKIP"的占位规则；(b) 未装 ACL 的主机上此测试根本不参与链接（meson 双重门控）→ ACL GEMM 单元永远零压测。死码中潜伏的比较还有 1e-3f 绝对容差问题（:124）。

### D7 — BETA 质量门控造成的默认覆盖空洞

```
$ sdcshield --list-tests | grep -c neon_add     → 0
$ sdcshield --list-tests --quality=0 | grep neon_add → neon_add（293 vs 默认 289）
```

`neon_test.cpp:91`、`sdc_test.cpp:225`、`crypto_test.cpp:232` 均为 `TEST_QUALITY_BETA`。默认 `--quality=2` 的生产运行中：无普通 NEON 整数加法用例、无 AESE 硬件加密用例（加密仅剩 openssl/ipsec 软件路径）。`eigen_svd_jacobi*` 4 个用例为 SKIP 级，同样默认不跑。

### D4 — reload_buf 恒真"一致性检查"（73 文件）

`memcpy(store_buf, result, N); memcpy(reload_buf, store_buf, N); memcmp(reload_buf, result, N)` —— 纯内存软件往返，不经过任何被测硬件存储/加载路径，编译器可完全消除。在 spinlock/lock/atomic/adcx 等家族中主校验（计数器一致性/memcmp）是真的，此模式只是冗余；但在 **vmx（D6）与 crc（D11）家族中它是唯一的"验证"**，这正是那两族致命的原因。对照正确做法：`tests/cpu/arm64/neon_rot_2src.cpp` 用内联汇编强制 str q → ldr q 配对（meson 注释明确指出"编译器会消除 intrinsic 重载"）。

## 确认无缺陷的家族（抽样核verify过验证逻辑）

| 家族 | 验证方式 | 结论 |
|------|----------|------|
| eigen_svd / eigen_gemm | 首次运行 golden + 字节精确 memcmp_or_fail（sandstone_eigen_common.h:42-51） | 合格（多线程 ULP 抖动已文档化，非本审计范畴） |
| openblas_{d,s,z,c}gemm, lu | 输入防篡改 memcmp + 输出字节精确 golden（dgemm.cpp:194-205）；golden 由同库计算但输入固定、库调用确定性（USE_THREAD=0） | 合格 |
| sleef_neon / sleef_sve | 字节精确 memcmp_or_fail（sleef_neon.cpp:499+） | 合格 |
| zlib / zstd / isal_igzip | 解压回读 memcmp（zstd/test.c:78-79, igzip.cpp:189,207） | 合格 |
| openssl_sha / ipsec（46 个） | 字节精确摘要 memcmp（openssl_sha.cpp:146-150） | 合格 |
| memcpy* / vmovnt* / mfence / mem_disambiguation | dst-vs-src memcmp | 合格 |
| spinlock_* / lock* / atomic_* | 计数器一致性 / 撕裂检查（spinlock_stress.cpp:119, atomic_simd_128.cpp:76） | 合格（原子性语义恰当） |
| arm64/*（39 个 SDC 用例） | 全部具备 report_fail/memcmp_or_fail（逐文件核verify） | 合格 |
| movbe / dump_probe_*（12 个） | 字节交换往返比较（研究探针，属意设计） | 合格 |
| gather_* | 字节精确 memcmp（gather_f32.cpp:48） | 合格 |
| mite 之外 misc 族 | memcmp 校验 | 合格（mite 见 D10） |
| smi_count / mce_check | SKIP 级 / EDAC 真实计数 | 合规 |

## 修复建议（按投入产出排序）

1. **D6 vmx 家族（9 个）**：ARM64 上要么诚实 skip（log_skip + EXIT_SKIP，理由"ARM64 无 VMX，需 KVM/VM 实现"），要么实现真实虚拟化压测（KVM 运行 aarch64 guest 触发异常注入）。当前状态是最严重的虚假覆盖。
2. **D11 CRC 家族（13 个）**：加一条**独立软件 golden**（CRC-32C 查表实现或已验证常数向量），hw 结果对 golden 比较；去掉第二遍同指令重算。
3. **D1/D2 fma 家族（3 个）**：把 `approx_equal` 换成字节精确 memcmp（同目录 fma_tail_avx2 已示范），或至少删除绝对误差下限。
4. **D4 reload_buf 模式**：要么删除（避免虚假安全感），要么改用内联汇编强制真实 str→ldr（neon_rot_2src.cpp 示范）。
5. **D3 acl_gemm**：在两处 EXIT_SKIP 前补 log_skip（一行修复），长期修 fork-safety。
6. **D7**：评估 neon_add/arm64_sdc/arm_crypto 提升到 PROD 的可行性（它们是各自单元唯一用例）。
7. **D5/D10**：低优先级；可在求和之外加逐元素校验或改 CRC 校验和。

## 审计方法学说明

- 静态：356 个 DECLARE_TEST 全量清点 → 逐族（5 组）阅读 test_run 验证逻辑 → 模式化 grep（`reload_buf`、`crc1 == crc2`、容差、无 TEST_LOOP）
- 动态：对每个可疑家族抽样实机运行（vmx×9、fma、crc32、mite、movdq2q、partial_store_forwarding）确认 pass/skip 真实行为
- 判定"确定性缺陷可逃逸"的标准：golden 与被测路径是否同源、比较宽度是否低于 1 ulp、是否存在恒真分支
