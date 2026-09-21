# SVE 转换批次 1：kreg 软件仿真 → SVE 谓词真硬件（7 个测试）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 7 个 x86 kreg 软件仿真测试转换为 SVE 谓词真硬件实现（64 批次计划的第一批），全部进 `tests/cpu/sve/kreg/`，接入 `tests_sve_avx53` 库，编译通过并走 122 两阶段验证。

**核心价值**：原测试是"x86 AVX-512 k 寄存器在 ARM 无真等价物"的历史妥协（软件仿真自己跟自己比）；SVE 谓词寄存器是唯一真等价物——转换后测试的是**真谓词指令硬件行为**。

## 负载保持原则（与 avx53 相同）

- 输入生成模式保留（`std::mt19937` + 随机分布，每迭代新随机）
- golden 结构保留（sw 参考与 hw 计算比对 + store/reload 一致性）
- 日志格式保留（fprintf stderr 同款）
- **新增**：sw 参考从"与 hw 相同的表达式"升级为"独立的标量参考实现"（真 golden——这是本批转换的灵魂：原版 sw==hw 是同义反复，转换版 sw 是独立标量语义，谓词指令必须真正算对才能 pass）

## SVE 指令映射（全部已独立程序验证 500-1000 次随机全对）

| 原测试 | 原软件仿真 | SVE 真指令/操作 |
|---|---|---|
| `kreg2` | 整数移位仿真 KSHIFTL/KSHIFTR | `svlsl_n_u16_x` / `svlsr_n_u16_x`（向量移位） |
| `kreg3` | KORTEST/KTEST 标志位 if | `svorr/svand/svand(svnot)` + `svcmpne` + **`svptest_any`（真谓词测试指令）** |
| `kreg5` | KUNPCK 位拼接 + KNOT | **`svzip1_u16`（真交织指令）** + `sveor` 取反 |
| `kreg6` | 16 位掩码→16×u32 展开循环 | **`svsel_u32`（真谓词选择指令）**：`svcmpne(s vand mask, 0)` 生成谓词，svsel 展开 |
| `kreg8` | VPMOVM2 64 位掩码→64 元素展开（B/W/D 三档） | 同 kreg6 svsel 技术，`svcntb/w/d` 分批 |
| `kreg9` | KMOVB/W/D/Q 传输仿真 | 谓词↔向量通道：`svld1/svst1` + `svcntp` 计数验证 |
| `arm0102_kreg_mask` | NEON vceq/vcgt/vtst 旋转掩码扩展 | `svcmpeq/svcmpgt/svcmpts(tst=svand+cmpne)` + `svsel` + str/ldr 放大器（`ldr z` asm 保真） |

## 关键实现细节

1. **VL 无关**：所有数组按 `svcnt*()` 分批 + `svwhilelt` 尾部谓词（探针阶段两次踩坑都是 b16/b64 谓词 lanes ≠ 数组尺寸——16/4 而非 8/8——正式实现必须用 svcnt 动态分批）
2. **命名**：`kreg2` → `kreg2_sve`（原 ID 无 NEON/AVX 字样，直接加 `_sve` 后缀）；`arm0102_kreg_mask` → `arm0102_kreg_mask_sve`
3. **arm0102 的 asm 锁定对**：`load_vec` 的 `ldr q0` 换 `ldr z0` asm（保持 asm 保证的加载语义），store 同理 `str z0`；掩码扩展负载旋转（eq/gt/eq-u/tst per i%4）保留
4. **目录**：`tests/cpu/sve/kreg/`（7 个 .cpp）；注册进 `tests_sve_avx53` 库（同 avx53 模式）
5. **HWCAP_SVE 探测**：init 里 getauxval 探测 + CpuNotSupported skip（sleef_sve 先例）

## 验证命令（每步真实执行）

```bash
export PATH=$PATH:/home/sdc/.local/bin
PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release && ninja -C builddir
./builddir/sdcshield --list-test-ids | grep "_sve" | grep kreg   # 7 个新 ID
# 单测
for t in kreg2_sve ...; do ./builddir/sdcshield -e $t -t 3000 -n 2; done
# 两阶段
./builddir/sdcshield -e "<7个>" -t 60000 --cpuset='!122'   # 阶段1: 全 pass
./builddir/sdcshield -e "<7个>" -t 60000                    # 阶段2: 观察是否触发 122
# 回归
./builddir/sdcshield -e zstd19 -t 3000 -n 1                  # pass
./builddir/sdcshield -e kreg2 -t 3000 -n 2                   # 原版不回归
```

## One-patch-per-unit

| Task | 内容 |
|---|---|
| 1 | 7 个 .cpp 写作 + meson 注册 + 构建 + 单测 |
| 2 | 两阶段验证 + 回归 + commit "tests(sve): kreg batch — software simulations to real SVE predicate instructions" + push |

---

### Task 1: 实现与构建

**Files:** Create `tests/cpu/sve/kreg/{kreg2_sve,kreg3_sve,kreg5_sve,kreg6_sve,kreg8_sve,kreg9_sve,arm0102_kreg_mask_sve}.cpp`；Modify `tests/cpu/sve/meson.build`

- [x] Step 1: 写 kreg2_sve（svlsl/svlsr + 独立标量 golden）
- [x] Step 2: 写 kreg3_sve（svptest_any 真指令 + OR/AND/ANDNOT 向量）
- [x] Step 3: 写 kreg5_sve（svzip1 + sveor）
- [x] Step 4: 写 kreg6_sve（svsel 展开，svcntw 分批）
- [x] Step 5: 写 kreg8_sve（三宽度 svsel 展开）
- [x] Step 6: 写 kreg9_sve（谓词传输 + svcntp）
- [x] Step 7: 写 arm0102_kreg_mask_sve（4 比较旋转 + svsel + asm str/ldr z）
- [x] Step 8: meson 注册 + 构建 + `--list-test-ids` 确认 7 个
- [x] Step 9: 单测每个 `-t 3000 -n 2` 全 pass（737/742/737/364/674/2276/35 pass）

### Task 2: 两阶段验证 + 提交

- [x] \1（7954 pass/0 fail 长窗口 + 733/0 短窗口复核）
- [x] \1（7359 pass/0 fail, 122 参与未触发——数据点 #6：谓词指令负载）
- [x] Step 3: 回归 zstd19 + 原版 kreg2/3 抽验（zstd19 pass; kreg2/3/arm0102 原版 pass）
- [x] Step 4: commit + push
