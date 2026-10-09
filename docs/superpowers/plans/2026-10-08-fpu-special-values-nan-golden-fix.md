# Plan: fpu_special_values_sve NaN golden 比较修复(NaN class-only)

**日期**: 2026-10-08(战役值守期写成,战役 drain 后执行)
**分支**: 新建 `fix/fpu-sve-nan-golden`(基于 main)
**文件**: `tests/cpu/sve/fma2/fpu_special_values_sve.cpp`(唯一改动点)
**状态**: plan 就绪,等待 SVE 压测战役(114 节点)完全 drain 后执行

## 背景:战役实证的确定性缺陷(非 SDC)

10-08 SVE 压测战役中 6/6 次真实运行(2 v1 + 4 v2,跨节点跨时段)全部失败,
字节级同签名:同一 report_fail_msg(line 228)、同样计数 f32=368/f64=184、
全部 608 线程、相同首个 mismatch 三元组、RNG 无关。

计数自洽证明:46 个 NaN 三元组 x lanes(VL256: f32 8 lanes / f64 4 lanes)
= 368 / 184。**非随机硬件错误,是测试方法论缺陷**。

## 根因(hex 铁证,findings.md 巡检6)

- 硬件 `svmla_f64_x` 含 NaN 输入:结果 NaN payload = **输入 NaN payload 按位 OR**
  (再置 quiet 位)。
- glibc `fma`:结果 NaN payload = **第一操作数 payload**(quieted)。
- IEEE-754 对 NaN payload 传播是**实现自由**,两者都合法。
- 当 a,b 均为 NaN(且 payload 不同)时硬件 OR ≠ glibc 首操作数 → bit-exact
  比较必然 mismatch。仅一个 NaN 输入时两者一致(OR 只有一个贡献者)。
- 框架 `sandstone_data.cpp` 的 SNaN quieting 解决的是 quiet 位统一;本缺陷是
  payload 传播差异,同一层级的合法放宽。

## 方案(3 选 1,选 1)

1. **NaN class-only 比较(选定)**:比较时若 sw 或 hw 任一为 NaN,则只要求
   两者都是 NaN(`std::isnan`);非 NaN 情况维持全位宽 bit-exact。
   - 保留:非 NaN 三元组的 1-bit ULP/符号位 SDC 检测能力(±0 符号位翻转
     仍会被抓住——这正是 SDC 信号)。
   - 放弃的只有 IEEE 本就不规定的 NaN payload 位。
2. 规范化两边 payload 为标准 quiet-NaN 再比:等价于 1,多余操作无增益。
3. 从表中去掉 NaN:损失 inf*0 等 NaN 产生场景的覆盖,不取。

## 实现(最小 diff)

`sweep_f32_sve` / `sweep_f64_sve` 的逐 lane 比较,从:

```c
if (bits_of(hw_lane) != sw_bits) { ... ++mismatches; }
```

改为(NaN 时 class-only):

```c
bool hw_nan = std::isnan(hw_lane), sw_nan = std::isnan(sw_lane);
bool equal = (hw_nan || sw_nan) ? (hw_nan && sw_nan)
                                : (bits_of(hw_lane) == sw_bits);
if (!equal) { ...; ++mismatches; }
```

两个 sweep 函数同型修改;`<cmath>` 已 include。同时更新文件头 `@parblock`
doc 注释:byte-for-byte 描述补一句 NaN class-only 说明(规则 7 文档同步)。

## 验证(CLAUDE.md 规则 2,100% 真实输出)

1. `ninja -C builddir` clean,零新警告。
2. `./builddir/sdcshield -e fpu_special_values_sve -t 2000 -n 1` → `exit: pass`
3. `./builddir/sdcshield -e fpu_special_values_sve -t 5000`(全核)→ pass
4. 回归:`-e zstd19 -t 2000 -n 1` → pass。
5. x86 非回归:改动全部在 `#ifdef __aarch64__` 段内,原 x86 fpu_special_values
   不碰(规则 5,inspection)。

## 硬约束

- **战役 drain 前严禁 `ninja`**:114 节点 PENDING 作业启动时读 builddir 二进制,
  md5 12ef01f8fb93859b41954b774079885e 必须保持到 COMPLETE_nodes=114/114。
- One patch per commit:本修复独立 commit(规则 1),`git commit -s`,DCO 尾行,
  推 feature 分支(规则 3)。
- 同批 backlog:sve_rot VL-overflow fix(尚无 plan,需另行撰写)。