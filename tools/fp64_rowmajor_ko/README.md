# fp64_rowmajor_ko — ldp/stp 内核模块变体

框架测试 `tests/cpu/misc/neon_rot_ldr_at_top_rowmajor` 的 **fp64 变体**，
以可加载内核模块（ko）形式存在，检验对象是**访存通路**：把 NEON 单寄存器
访存（`ldr q` / `str q`）替换成双寄存器对（`ldp` / `stp`），并核验两套
访存序列在逐位（byte-level）上完全一致。

> 注意：仓库中**不存在** `neon_rot_ldr_at_top_rowmajor_fp64`（`*_fp64*`）族
> 父文件。本变体沿用的是**整数**父测试的骨架（slot add/eor/and/orr），其
> fp64 算术映射（`fadd/fsub/fmul/fmaxnm`）是**新写**，不是对某个既有测试的
> 等价改写。

## 文件

| 文件 | 作用 |
| --- | --- |
| `fp64_rowmajor_ldp_stp.c` | 内核模块本体：`ldp`/`stp` 访存 + fp64 旋转 slot 合成 + 标量独立 golden |
| `userspace_ref.c` | 用户态 oracle：保留 `ldr q`/`str q` 原访存路径，供跨实现逐位比对 |
| `verify.sh` | 端到端验证脚本（编译 → 反汇编门禁 → insmod → 一致性 → rmmod） |
| `Makefile` | 外部模块 kbuild 构建（`-std=gnu11`，见下） |

## 关键设计

- **标量独立 golden（issue #373）**：`expected` 由标量 `fadd/fsub/fmul/fmaxnm`
  （d 寄存器）+ 标量 `ldr/str` 重算，独立于被测的 `ldp`/`stp` 访存通路与
  128 位向量数据通路（高 lane）——持续型向量通路故障不再让 golden 与 dst
  同步出错。**边界须如实界定**：ARMv8 上标量 `fadd d` 与向量 `fadd v.2d`
  发往同一条 FP 流水线，golden 并不独立于 FP 算术 ALU；持续型 FP 加/乘/
  取最大算术单元故障会让 golden 与 dst 同步出错——**该缺陷类在本家族内
  无任何独立黄金覆盖**：整数 slot 父测试只执行整数向量 ALU
  （vaddq_u64/veorq/vandq/vorrq）与标量整数重算，不执行任何 FP64 指令，
  对该单元故障零检出；本模块的独立覆盖面是 ldp/stp 访存通路与向量 lane
  通路，不含共享 FP 算术核心（家族级盲区，与 neon_rot_ldp 用例的如实披露
  同口径）。
- **slot 3 用 FMAXNM 而非 FMAX**：`vmaxq_f64`（FMAX）的 NaN 传播语义在标量 C
  无对应表达式，会破坏标量 golden 的逐位一致性；`vmaxnmq_f64`（FMAXNM）与
  C `fmax`/`__builtin_fmax` 逐位一致。
- **定时持续压测**：`duration_ms`（每核时间预算，默认 2000）+ `sweeps`（显式
  次数上限，0=关），先到先止；全 0 回退到默认 2s。绝不固定短跑。
- **per-CPU 全核覆盖**：每个在线 CPU 各跑一遍主循环（`queue_work_on` →
  per-cpu kworker），聚合所有核的 miscompare 数。
- **insmod 退出码门禁**：检出 miscompare（`failing>0`）→ `-EIO`，insmod 失败，
  不以 dmesg 单行代替失败信号。

## 构建 / 运行

```console
cd tools/fp64_rowmajor_ko
make                      # 构建 fp64_rowmajor_ldp_stp.ko（需内核头文件）
sudo insmod fp64_rowmajor_ldp_stp.ko seed=0x12345678DEADBEEF [sweeps=N] [duration_ms=M]
sudo dmesg | grep fp64_rowmajor_ldp_stp:   # PASS/FAIL 每核一行 + 六缓冲 HASH
sudo rmmod fp64_rowmajor_ldp_stp
```

模块参数：`seed`（xorshift64* 种子，默认 `0x12345678DEADBEEF`）、
`sweeps`（每核显式 sweep 上限，0=关）、`duration_ms`（每核时间预算 ms，
默认 2000，0=关）。

## 验证

```console
sudo bash verify.sh    # 需 root；阶段 [1]..[5]，全 PASS 且 FAIL=0 为通过
```

5.10 内核 kbuild 默认 `-std=gnu89`，本模块为 C11 风格，故 `Makefile` 显式加
`-std=gnu11`（并关掉面向 in-tree 回移的 `-Wdeclaration-after-statement`）。
