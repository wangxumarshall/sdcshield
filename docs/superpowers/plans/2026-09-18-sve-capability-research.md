# SVE 指令测试用例可行性研究（2026-09-18）

> 研究者：xu + Claude。全部结论基于本机实测命令，无臆测。

## 1. 机器真实状态（实测，纠正旧文档）

| 项目 | 实测值 | 命令 |
|---|---|---|
| CPU | HiSilicon **cortex x3b**（`--dump-cpu-info` 报告），MIDR `0x00000000480fd020` | `cat /sys/devices/system/cpu/cpu0/regs/identification/midr_el1` |
| 核数 | **127 在线**（2 socket × 63 core，无 SMT），4 NUMA 节点（0-31/32-63/64-95/96-126） | `lscpu` |
| **SVE** | ✅ **有！** flags 含 `sve`；`/proc/sys/abi/sve_default_vector_length` = **32 字节 = 256 bit** | `lscpu` / `cat /proc/sys/abi/sve_default_vector_length` |
| SVE 相关 flags | `sve` `svei8mm` `svef32mm` `svef64mm` `svebf16`（i8mm/bf16/f32mm/f64mm 的 SVE 形态） | `/proc/cpuinfo` |
| 其他关键 flags | `i8mm` `bf16` `fcma` `fphp` `asimdhp` `asimdfhm` `asimdrdm` `sha3` `sha512` `sm3` `sm4` `aes` `pmull` `crc32` `atomics` `dit` `flagm/2` `rng` 等 | 同上 |
| **没有的** | `sve2` `sveaes` `svesha3` `svesm4` `svebitperm` `svepmull` `sme*` `paca/pacg` `afp` `rpres` `wfxt` | grep 无匹配（实测确认） |

**重要**：仓库旧文档（README/CLAUDE.md/`docs/research/usage.md`）写"鲲鹏 920 无 SVE、sleef_sve 会干净 skip"——**这台机器已不是那台鲲鹏 920**。实测 `sleef_sve`、`sve512_f64_chain_arm`、`sve512_f32_chain_arm` 全部真实 `result: pass`（2026-09-18）。

**用户贴的 flags 分析与实测不符的部分**：608 核/32 NUMA（实测 127 核/4 NUMA）、SVE2/SME 全家桶（实测一个都没有）。那份分析对应的可能是另一台机器或虚拟化环境。本机 flags 完整列表（实测）：
```
fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb dcpodp flagm2 frint svei8mm svef32mm svef64mm svebf16 i8mm bf16 dgh rng ecv
```
（比用户贴的表少了：sve2 系列、sme 系列、paca/pacg、afp、rpres、wfxt、uscat 有、ecv 有——以这条实测为准。）

## 2. SVE 矩阵/AI 指令逐条实测（带 golden 验证）

VL = **256 bit**（`svcntb()`=32）。每条都真实执行 + golden 比对：

| 指令 | intrinsic | 硬件 | golden | 语义（实测确认） |
|---|---|---|---|---|
| FMMLA f32 | `svmmla_f32` | ✅ 执行 | ✅ 正确 | 每 128-bit 段 2x2 矩阵乘：`d_rc += Σ_k n[4i+2r+k]*m[4i+2c+k]`（n={10,11,12,13}, m={20..23} → d={431,473,513,563}，标准行×列） |
| FMMLA f64 | `svmmla_f64` | ✅ | ✅ | 同上 f64 版（VL=256 时 4 lane 全参与） |
| BFDOT | `svbfdot_f32` | ✅ | ✅ | `d[i] += bf16[2i]*bf16[2i] + bf16[2i+1]*bf16[2i+1]` |
| SDOT s8 | `svdot_s32` | ✅ | ✅ | 每 128-bit 段 4 字节 dot：`d[i] += Σ s8[4i..4i+3]²` |
| USMMLA | `svusmmla_s32` | ✅ | ✅ | **d0 = Σ_{k=0..7} u[k]*s[k]**——整个 128-bit 段的 u8·s8 全点积分进 2x2 中的每个 s32（u=10..25,s=20..35 → d={2580,3444,4084,5460}，与 `Σ u[0..7]*s[0..7]` 等模型逐一吻合） |
| FCMLA #0/#90/#180/#270 | `svcmla_f32_m` / raw asm | ✅ | ✅（语义澄清后） | 见下节 |

## 3. FCMLA 语义研究（重要收获，差点误判成硬件 SDC）

### 过程
1. 初测 `svcmla_f32_m #0` 输出 `(5,6)`，我预期的"完整复乘"是 `(-7,16)` → 疑似硬件缺陷
2. 排除法一路走到底：intrinsic → C 内联汇编 → **纯汇编 + 直接 syscall 输出**（无 libc/无编译器）→ 全部同样结果
3. NEON `vcmla`（同名指令 128-bit 版）**行为与 SVE 完全一致** → 不是 SVE 单元问题
4. objdump 核对指令编码 `fcmla z0.s, p0/m, z1.s, z2.s, #0` = `0x64820020`，标准编码
5. 真值表实测（4 组基向量 + 4 个旋转角）得出硬件行为模型：
   ```
   #0:   d = (ar*br,  ar*bi)     ← 只含 n 实部项
   #90:  d = (-ai*bi, ai*br)     ← 只含 n 虚部项
   #180: d = -(#0);  #270: -(#90)
   ```
6. **决定性仲裁**：让 GCC 自动向量化 `float complex` 乘法循环——
   GCC 生成 **`#0 + #90` 指令对**（NEON 和 SVE 路径都是），程序结果正确 `(-7,16)` PASS

### 结论（修正我最初的错误理解）
**ARM ARM 的 FCMLA 真实语义 = 每条指令只算复乘的一半项，`#0` 和 `#90` 成对使用才完成一次完整复数乘**：
```
完整复乘 (ar+ai·i)(br+bi·i) = (ar·br − ai·bi) + (ar·bi + ai·br)·i
  = FCMLA #0  贡献 (ar·br,  ar·bi)
  + FCMLA #90 贡献 (−ai·bi,  ai·br)
```
**本机硬件行为与 ARM ARM 一致，无缺陷**。GCC 向量化器是权威仲裁者（其实现源自 ARM 官方手册）。

### 教训（对 SDC 检测工作极其重要）
> 单元测试新指令时，**golden 模型必须先经过"编译器向量化仲裁"或手册逐字核对**，否则会把"自己记错语义"误判成"硬件 SDC"。这正是 SDCShield 测试开发的核心风险——先证伪自己，再怀疑硬件。

## 4. 回答：能不能做 SVE 指令的测试用例？

**能，且有充分条件**：
- ✅ 硬件 SVE 真实可用（VL=256bit），现有 11 个 SVE 测试（`sve512_*`、`sleef_sve`、`eigen_svd_cdouble_sve`）全部实测 pass
- ✅ 本机独有且未被现有测试覆盖的指令域：
  - **FMMLA f32/f64**（`svef32mm`/`svef64mm`）—— 矩阵外积 FMA，SVD/GEMM 微内核核心，SEVI 文献指出 FMA 是第一大 SDC 源
  - **BFDOT/FMMLA bf16**（`svebf16`/`bf16`）—— AI 推理通路
  - **SDOT/UDOT/USMMLA**（`svei8mm`/`i8mm`）—— INT8 矩阵乘
  - **FCMLA**（`fcma`）—— 复数 FMA（FFT 核心；语义已吃透，见上）
- ✅ 仓库有成熟的 SVE 测试模板：`tests/cpu/arm64/sve512_*.cpp`（HWCAP 探测 + 干净 skip + `-march=armv8.2-a+sve` 独立编译库 `tests_arm64_sve`）
- ⚠️ 约束：本机 **无 SVE2/SME**，SVE2 专属指令（sveaes/svesha3/svesm4/svebitperm）写了也无法在本机验证，只能写"探测+skip"型
- ⚠️ 新测试要加的 march 修饰符：`+f32mm+f64mm+bf16+i8mm`（GCC 12.3 实测支持）

## 5. 候选测试设计（待用户确认方向后写计划）

| 候选测试 | 指令 | golden 来源 | 风险 |
|---|---|---|---|
| `sve_fmmla_f64_chain_arm` | FMMLA f64 串行链 | 标量 fma() 同序重算 | 低（语义已实测确认） |
| `sve_fmmla_f32_chain_arm` | FMMLA f32 2x2 段循环 | 标量 golden | 低 |
| `sve_bfdot_ai_arm` | BFDOT/BFMLAL bf16 | fp32 精确 golden（bf16 输入恰可精确表示） | 低 |
| `sve_i8mm_dot_arm` | SDOT/UDOT/USMMLA | 整数 golden（精确） | 低 |
| `sve_fcmla_fft_arm` | FCMLA #0+#90 对 + FFT 蝶形 | std::complex 标量 golden | 中（复数 NaN 语义注意） |

## 6. 本次实测命令存档（关键证据）

```bash
# SVE 硬件确认
./builddir/sdcshield -e sleef_sve -t 3000 -n 1          # result: pass ×N
./builddir/sdcshield -e sve512_f64_chain_arm -t 3000 -n 1  # result: pass ×N
cat /proc/sys/abi/sve_default_vector_length             # 32 (=256bit)
# 矩阵指令探针（/tmp/sve_probe6.c 等，golden 内嵌）
gcc -O2 -march=armv8.2-a+sve+f32mm+f64mm+bf16+i8mm ...  # FMMLA/BFDOT/SDOT/USMMLA 全 OK
# FCMLA 仲裁
gcc -O3 -fcx-limited-range -march=armv8.2-a+sve /tmp/vec_cmul.c   # PASS(-7,16)
```
