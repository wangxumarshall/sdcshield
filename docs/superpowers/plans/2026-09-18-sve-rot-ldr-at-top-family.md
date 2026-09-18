# Plan: NEON core-179 探针家族移植为 SVE 版(tests/cpu/sve/)

**日期**: 2026-09-18
**分支**: `test/neon-rot-ldr-at-top`(当前)→ 新建 `test/sve-rot-ldr-at-top`
**目标机器**: cn23154(HiSilicon,608 核,SVE2+SME,openEuler 22.03)
**开发机**: Kunpeng 920(无 SVE,已实测 HWCAP bit22=0)— 只能验证编译+自动跳过

## 背景与约束

NEON 家族(tests/cpu/misc/neon_rot_ldr_at_top*.cpp,5 文件 8 测试)是 core-179
SDC 归因矩阵。用户要求将全家族移植为 SVE 指令版,放在新目录 `tests/cpu/sve/`,
保留归因矩阵结构。验收标准:**代码逻辑正确 + openEuler 22.03 编译无错误**。

### 已验证的技术事实(本会话实测)

1. 框架门控链路:`tests_sve` 库以 `-march=armv8.2-a+sve` 编译 → 生成的
   `cpu_features.h` 中 `__ARM_FEATURE_SVE` 已定义 → `device_compiler_features`
   含 `cpu_feature_sve` → `DECLARE_TEST` 宏填入 `compiler_minimum_device` →
   框架在无 SVE 硬件自动 skip(实测:`skip-reason: 'test compiled with sve'`)。
   **无需在测试代码里写任何 getauxval 门控**。
2. GCC 12(openEuler 24.03 SP3,22.03 同为 GCC 12.x)已验证编译通过:
   `svadd/sveor/svand/svorr_u64_x(svptrue_b64(), a, b)`、`svld1/svst1_u64`、
   `svcntb()`、内联汇编 `"ldr %0, [%1]" : "=w"(res)`(z 寄存器操作数)。
3. `-msve-vector-bits=128` 使 `svcntb()` 成为编译期常量、`svptrue` 固化 vl16;
   真 VLA(无该 flag)则生成运行时 `cntb` 指令。
4. x86-64 非回归:所有新代码在 `tests_set_sve`(aarch64-only 库),x86 构建不受影响。

### 设计决策

- **VL 策略:真 VLA(运行时 VL)**。目标机 VL 未知(可能 256b)。元素计数定义为
  "VL 槽数":缓冲区按 `svcntd()` 运行时探测后分配,索引步长 = 1 个 z 寄存器宽度。
  这样 128b/256b/512b 硬件都能跑,且不同 VL 下触发率可对比(对归因有价值)。
  hot loop 保持与 NEON 版逐指令对应:`ldr z` ×3(顶部 scratch + 双源)→
  ALU(svadd/sveor/svand/svorr 按 i%4 轮转)→ `str z` ×2 背靠背。
- **指令深度:仅 SVE1**(用户未要求 SVE2 特有指令;`-march=armv8.2-a+sve` 与
  现有 tests_sve 库一致,不引入新 march 级别;SVE2 字节交换变体留作后续)。
- **目录与命名**:`tests/cpu/sve/neon_rot_ldr_at_top.cpp` 等 5 文件,测试名
  前缀改为 `sve_rot_ldr_at_top*`(文件名保留 neon_rot 前缀会误导,用户指定
  目录为 tests/cpu/sve/,文件名沿用原名以保持与 NEON 版的对应关系,但
  DECLARE_TEST 的 id 用 `sve_rot_ldr_at_top*`)。

  修正:用户说"在这里面全家族 5 个文件",文件名应与 NEON 版一一对应
  (`neon_rot_ldr_at_top.cpp` → `tests/cpu/sve/neon_rot_ldr_at_top.cpp`?),
  但 SVE 目录里叫 neon_ 前缀不合逻辑。**采用 `sve_rot_ldr_at_top*.cpp` 文件名**,
  测试 id 同名,与 misc/ 下的 NEON 版形成清晰的镜像对照。

- **dump 目录**:沿用 `movbe_log/gps_rowmajor`(K5/K7/K3 已用此约定),SVE 版
  dump 文件名加 `sve_` 前缀区分,避免与 NEON 版的 dump 混淆。

### 热循环骨架(SVE 版,与 NEON 版逐指令对应)

```
NEON (misc/)                          SVE (sve/)
--------------------------------------------------------------------
load_vec: ldr %q0,[%1] (inline asm)   load_vec: ldr %0,[%1] z-reg (inline asm)
vaddq_u64(a,b)                        svadd_u64_x(svptrue_b64(),a,b)
veorq_u64 / vandq_u64 / vorrq_u64     sveor/svand/svorr_u64_x(...)
vst1q_u64(addr,val) store_vec         str z-reg (inline asm)  [str 改为内联汇编
                                      保持与 NEON store_vec 相同的"非内建调用"形态]
```

NEON 版 store_vec 用 `vst1q_u64` 内建;SVE 版对应的内建是 `svst1_u64`。
为最大程度保持指令形态对应(不用编译器自由度),SVE 版 load 用内联汇编
`ldr`,store 用内联汇编 `str`(已验证 GCC 12 支持 `"w"` 约束的 z 寄存器)。
ALU 用内建(SVE 内联汇编写 ALU 需要手写谓词寄存器,内建更可靠且生成的就是
单条 add/eor/and/orr z 指令)。

### VL 适配的数据布局

- `count` = 缓冲区 z 寄存器槽数(如 1024),每槽 `vl_bytes = svcntb()` 字节。
- `srcA/srcB/scratch/expected/temp/dst` 各分配 `count * vl_bytes`(运行时)。
- golden 在 init 里用与 run 完全相同的 SVE 内建计算(同一套指令,保证
  "golden 就是同一条 ALU 指令在健康核上的输出")。
- memcmp 长度 `count * vl_bytes`。
- K3RES 的 4x 足迹:`count = 4096` 槽(运行时字节 footprint = 4096*VL,
  在 128b VL 上 = 64KiB/缓冲,与 NEON 版一致;256b VL 上 = 128KiB/缓冲,
  超出 L1D 更多——在计划文档中注明这一差异,由用户在目标机上解读时知悉)。
- GPS tag 的 byte_off 字段(K5/K7/K3 用):`byte_off = slot_idx * vl_bytes + lane*8`,
  128b VL 时与 NEON 版一致;256b VL 时最大 byte_off = 4095*32+24 < 65535,
  仍 fit 16b 字段。**tag 写入改为按字节构造后 memcpy 进缓冲,避免依赖
  svcreate 之类不存在的内建**——直接在 uint64_t 数组上构造,再整体拷入
  SVE 缓冲(或者直接就在 uint64_t* 上操作:缓冲类型改为 `uint64_t*`,
  视图更简单)。**最终选择:缓冲区统一用 `uint8_t*`/`uint64_t*` 承载,
  load/store 通过内联汇编按 z 寄存器宽度访问,ALU 用 svuint64_t 类型。**

### 每文件一补丁(one patch per unit)

按 CLAUDE.md 纪律,8 个测试 = 5 文件,每个文件一个 commit(+1 个 meson
基础设施 commit),共 6 个 commit,逐个验证-提交-推送。

---

## Task 0:meson 基础设施(sve/ 目录接入 tests_set_sve)

- [ ] 创建 `tests/cpu/sve/` 目录
- [ ] `tests/cpu/meson.build`:在 `tests_set_sve.add(...)` 处新增
      aarch64-only 块,加入 5 个文件(随各任务逐步加入;本 task 先建块并
      加入第一个文件占位结构,后续 task 各加一行)
      - 实际操作:每个文件任务里往 `tests_set_sve` 的 files() 列表追加一行,
        Task 0 只创建目录 + 空 meson 块(带注释),不引入文件
- [ ] 验证:`ninja -C builddir` 无错误;`--list-tests` 无变化(无新测试)
- [ ] commit: `build: scaffold tests/cpu/sve for SVE rot_ldr_at_top family`

**注意**:现有 `tests_set_sve` 块依赖 `eigen3_dep`(when 条件)。新 SVE 探针
不依赖 Eigen。方案:在现有块后面加一个**独立的** `tests_set_sve.add(files(...))`
(无 when 条件,aarch64-only 由外层 `if host_machine.cpu_family() == 'aarch64'`
块保证——检查现有代码:`tests_set_sve.add` 在 412 行,不在 aarch64 条件内!
但 `tests_config_sve` 的应用在 aarch64 条件内,且 `tests_sve_a` 库只在
aarch64 构建。需确认:非 aarch64 上 sourceset 空 → 无库,安全。
但 meson 的 `tests_set_sve.add(files('sve/xxx.cpp'))` 在 x86 上会把文件登记
进 set,而 set 只在 aarch64 分支被 apply → 文件不编译。**但** meson 会
对 sourceset 里的文件做存在性检查,无妨。为绝对安全(x86-64 非回归),
把 add 包在 `if host_machine.cpu_family() == 'aarch64'` 内,与 arm-0102
块同模式。)

**修正方案(Task 0 定稿)**:
```meson
# ARM64-only SVE SDC probes: SVE/SVE2 ports of the core-179 NEON
# attribution family (tests/cpu/misc/neon_rot_ldr_at_top*.cpp).
# Compiled in the tests_sve static library (-march=armv8.2-a+sve,
# true VLA — no -msve-vector-bits), so the framework auto-skips them
# on non-SVE hardware via compiler_minimum_device. x86-64 untouched.
if host_machine.cpu_family() == 'aarch64'
    tests_set_sve.add(
        files(
            'sve/sve_rot_ldr_at_top.cpp',   # added in later tasks
        )
    )
endif
```
问题:现有 `tests_sve_a` 库的 cpp_args 里有 `-DEIGEN_ARM64_USE_SVE` 和
`-DEigen=EigenSVE` — 对不含 Eigen 的新文件无害(宏未被引用),但更干净
的做法是评估是否新建独立库。**决定:复用 tests_sve 库**——Eigen 宏对非
Eigen 文件无作用,且库已有正确的 march_sve flags 和依赖链;避免 meson
结构膨胀。(编译验证会确认无 Eigen 宏副作用。)

另一个问题:现有 tests_sve 库在 eigen3_dep 缺失时 sources 非空但仍会
构建——检查 `tests_config_sve = tests_set_sve.apply(tests_config)`,
`tests_config` 不含 eigen 条件,eigen 的 when 条件在 add 时已消费。
所以新文件无条件进入库,库无条件构建(aarch64 上),正确。

**再修正**:经过重读 meson:`tests_set_sve.add(when: eigen3_dep, if_true:
files(...))` — eigen 条件只作用于 SVD 文件。我们无条件 add 新文件即可,
它们不依赖 eigen_dep。但注意库的 `dependencies:` 里有 `boost_dep` 和
`tests_config_sve.dependencies()` — 检查这些在无 eigen 时是否可用。
本开发机 eigen5 在 third-party,始终可用。目标机 openEuler 22.03 需要用户
自行准备(Eigen 5.0.0+,CLAUDE.md 已记录)。**这不影响本任务**:我们的
SVE 文件不 include Eigen,即使 eigen 依赖变化,文件本身照常编译。

## Task 1:sve_rot_ldr_at_top(父测试)

- [ ] 创建 `tests/cpu/sve/sve_rot_ldr_at_top.cpp`
  - 结构镜像 `tests/cpu/misc/neon_rot_ldr_at_top.cpp`
  - `#include <arm_sve.h>`(在 `#if defined(__aarch64__)` 内)
  - VL 运行时适配:`vl_slots = 1024` 固定槽数;init 时 `vl_bytes = svcntb()`
    后分配所有缓冲(`vl_slots * vl_bytes`);golden 用同一套 SVE ALU 计算
  - 数据类型:缓冲 `uint8_t*`(64 对齐,aligned_alloc_safe);load/store
    内联汇编按 z 宽度;ALU 用 `svuint64_t`
  - DECLARE_TEST id:`sve_rot_ldr_at_top`;description 注明是 NEON 版的
    SVE VLA 移植
  - `.quality_level = TEST_QUALITY_PROD`;无 `.groups`(与 NEON 版一致)
- [ ] meson 加入 `'sve/sve_rot_ldr_at_top.cpp'`
- [ ] 验证:
  - `ninja -C builddir` 零新告警(新文件必须零告警)
  - `--list-tests | grep sve_rot` 出现 `sve_rot_ldr_at_top`
  - `-e sve_rot_ldr_at_top -t 3000 -v`:本机(无 SVE)必须
    `result: skip, skip-reason: 'test compiled with sve'`(框架自动门控)
  - 回归:`-e zstd19 -t 3000` pass
- [ ] commit: `test: add sve_rot_ldr_at_top — SVE VLA port of the NEON position probe`

## Task 2:sve_rot_ldr_at_top_rowmajor(顺序变体)

- [ ] 创建 `tests/cpu/sve/sve_rot_ldr_at_top_rowmajor.cpp`(镜像 misc/
  neon_rot_ldr_at_top_rowmajor.cpp:升降交替扫描;真 VLA)
- [ ] meson 追加一行
- [ ] 验证同 Task 1(编译零告警、list、本机 skip、zstd19 回归)
- [ ] commit: `test: add sve_rot_ldr_at_top_rowmajor order variant (SVE)`

## Task 3:sve_rot_ldr_at_top_rowmajor_k5inter_rand(K5 布局控制)

- [ ] 镜像 misc/neon_rot_ldr_at_top_rowmajor_k5inter_rand.cpp:
  - 交错布局 `src[2i]/src[2i+1]`(SVE 槽单位)
  - memset_random 填充
  - dump 到 `movbe_log/gps_rowmajor/`,文件名 `svek5ir_fail_...bin`
  - golden/比对与 dump 结构同 NEON 版
- [ ] meson 追加;验证同上
- [ ] commit: `test: add sve_rot_ldr_at_top_rowmajor_k5inter_rand K5 layout control (SVE)`

## Task 4:sve_rot_ldr_at_top_rowmajor_k7pattern(K7 模式电池 ×4)

- [ ] 镜像 misc/neon_rot_ldr_at_top_rowmajor_k7pattern.cpp:
  - `pattern_word` 逐字节构造(P_ZERO/P_ONE/P_RAND_GPS/P_HIHAM)不变 —
    纯标量 C,零移植成本
  - 槽填充:`i*vl_bytes + lane*8` 的 byte_off;256b VL 时 byte_off 最大
    1023*32+24=32760 < 65535,tag 字段安全
  - 四个 DECLARE_TEST:k7p_zero/one/rand/hiham(前缀 sve_)
  - dump 文件名 `svek7p_<cell>_fail_...bin`
- [ ] meson 追加;验证同上(四个 cell 都要 list 出来 + 本机全 skip)
- [ ] commit: `test: add sve_rot_ldr_at_top_rowmajor_k7pattern battery (SVE)`

## Task 5:sve_rot_ldr_at_top_rowmajor_k3res(K3 驻留,4x 足迹)

- [ ] 镜像 misc/neon_rot_ldr_at_top_rowmajor_k3res.cpp:
  - `K3RES_SLOTS = 4096` 槽;footprint 随 VL 缩放(128b: 64KiB/缓冲,
    与 NEON 版同;256b: 128KiB/缓冲,超出 L1D 更多 — 文档注释注明)
  - GPS tag byte_off 最大 4095*32+24=131064 **> 65535 溢出 16b 字段!**
    → SVE 版 tag 的 byte_off 字段改为"槽内偏移+槽索引分治"或直接截断。
    **决定**:byte_off 语义改为"VL 槽内的字节偏移"(0..vl_bytes-8,恒 <32),
    槽索引不进 tag(K3 的归因维度是足迹驻留性,不是位置分类;dump 里有
    完整缓冲,offline 可恢复位置)。文件头注释明确记录此差异。
  - dump 文件名 `svek3res_fail_...bin`
- [ ] meson 追加;验证同上
- [ ] commit: `test: add sve_rot_ldr_at_top_rowmajor_k3res K3 residency cell (SVE)`

## Task 6:文档更新

- [ ] `README.md` 测试目录说明(若有 tests 目录清单)
- [ ] `docs/writing_tests.md` 若有 NEON 家族提及则补 SVE 版
- [ ] commit: `docs: note the SVE port of the rot_ldr_at_top family`

## 全局验证清单(每 task 必须全部通过才 commit)

1. `ninja -C builddir` — 零新告警/错误
2. `./builddir/sdcshield --list-tests | grep sve_rot` — 新测试注册
3. `./builddir/sdcshield -e <new> -t 3000 -v` — 本机 skip with
   `'test compiled with sve'`(证明门控正确,不会在无 SVE 机器上 SIGILL)
4. `./builddir/sdcshield -e zstd19 -t 3000` — exit: pass(回归)
5. x86-64 非回归(检查):diff 只含 `tests/cpu/sve/` 新文件 + meson 的
   aarch64-only 块

## 目标机(openEuler 22.03 / cn23154)交接说明(写入最终交付信息,不是本地 task)

- 构建命令与 CLAUDE.md 相同(PKG_CONFIG_PATH 指向 eigen5);SVE 文件不依赖
  Eigen,但 tests_sve 库整体需要 eigen3_dep 存在(库依赖里有)——如目标机
  Eigen 不可用,SVD-SVE 会被 when 条件剔除,但我们的文件无条件 add,库仍
  构建。**需在交付时提醒用户**:若目标机构建时 eigen3_dep 判定失败导致
  tests_sve 库依赖缺失,可能需要检查;本地已验证全链路。
- 运行:`./sdcshield -e sve_rot_ldr_at_top -t 30m`(真 SVE 硬件上跑)
- 本地无法验证 SVE 运行时行为(Kunpeng 920 无 SVE);计划已通过
  "golden 用与 run 相同的 SVE 指令计算 + 内联汇编形态一致"保证逻辑正确性,
  首次目标机运行即为最终验证。

## 风险与缓解

| 风险 | 缓解 |
|---|---|
| GCC 12 对 VLA SVE 内联汇编的怪癖 | 已单独验证 ldr/str z-reg `"w"` 约束编译通过 |
| openEuler 22.03 的 GCC(12.x)与本地 24.03(12.3.1)差异 | 均为 GCC 12.x,SVE 支持同代;交付时注明 |
| 无 SVE 硬件本地验证 | 框架 skip 门控已实测;golden=同指令计算保证逻辑自洽 |
| K3 tag 16b 溢出(256b VL) | Task 5 设计决策:byte_off 改槽内偏移 |
| tests_sve 库 Eigen 宏副作用 | 宏未被新文件引用,编译验证确认 |
