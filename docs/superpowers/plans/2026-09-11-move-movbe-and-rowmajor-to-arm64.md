# 迁移 movbe 系列 + 集成 neon_rot_ldr_at_top_rowmajor 到 arm64 目录

日期：2026-09-11

## 背景

两项用户要求的目录整理（均归入 ARM64 分类 `tests/cpu/arm64/`，该子目录仅在
`host_machine.cpu_family() == 'aarch64'` 时进入构建，x86-64 不受影响）：

1. **movbe 系列 13 个文件**从 `tests/cpu/misc/` 迁移到 `tests/cpu/arm64/`：
   `movbe.cpp`、`movbe_dump.cpp`、`movbe_dump_probe_{a,b,c,d,e,f,g1,g2,h,x,xn}.cpp`。
   这是 Kunpeng 920 core-179 SDC 定位研究的探针系列（见
   `docs/MOVBE_SDC_CORE179_LOCALIZATION_REPORT*.md`）。全部为可移植代码
   （`__builtin_bswap32`，无 x86 intrinsic、无 arch guard），当前在
   `tests_set_base`（x86/ARM 共用）注册。迁移后从 base 集移除、加入 arm64 集。
2. **`neon_rot_ldr_at_top_rowmajor.cpp`**：用户已手动放到 `tests/cpu/arm64/`
   （untracked）。需要注册进 `tests/cpu/arm64/meson.build`。该文件无条件使用
   `uint64x2_t` / arm_neon.h / aarch64 inline asm（`ldr %q0`），只能 aarch64
   编译，与 arm64 目录分类一致。它是 core-179 归因研究的扫描顺序变体
   （偶数轮升序、奇数轮降序），父测试 `neon_rot_ldr_at_top` 在未合并的
   `origin/test/neon-rot-ldr-at-top` 分支上。

## 影响面（已侦察核实）

- `tests/cpu/meson.build:234-261`：misc 块含全部 13 个 movbe 条目（需移除）。
  注：misc 块注释说 "Misc test suite (24 tests)" 需同步改为 11。
- `tests/cpu/arm64/meson.build`：`arm64_tests` 列表加 13 + 1 个条目。
- 文档路径引用（`tests/cpu/misc/movbe*`）3 处需同步：
  - `docs/MOVBE_SDC_CORE179_LOCALIZATION_REPORT.md:222`
  - `docs/MOVBE_SDC_CORE179_LOCALIZATION_REPORT_V3.md:242`
  - `docs/CORE179_SDC_REPORT_CN.md:56`
  （`docs/OFHC_RESEARCH_REPORT_CN.md` 引用的是 `mrn_rmw.cpp`，不在本次范围。）
- README.md：ARM64 SDC 专项/触发配方表格补充 `neon_rot_ldr_at_top_rowmajor`；
  movbe 系列不单独列行（README 表格按检测域归纳，misc 系列本就未逐个列出）。
  **注意**：README.md 有用户自己的未提交修改（第 25-30 行运行示例），提交时
  只 add 本次改动行，绝不 `git add README.md` 全文件——用 `git add -p` 或先
  提交到暂存区再分离。实际操作：直接编辑 README 的 ARM64 表格区（150-190 行），
  提交时用 `git diff` 确认只包含表格行；用户改动区（20-30 行）不触碰。
  由于 git add 是文件级的，为避免把用户未提交改动带进 commit，**先征得用户
  确认或把用户改动 stash**——最安全方案：Task 3 提交前 `git stash push --
  README.md` 保住用户改动？不行，stash 会把我的表格改动也卷走。正确方案：
  我的所有 README 修改只加在 150-190 行区域，提交时用 `git add -p README.md`
  交互式不行（非交互环境）→ 用 `git diff README.md > /tmp/` 分离 hunk 后
  `git apply --cached` 只应用我的 hunk。
- **测试计数变化**：当前 builddir（ssl_link_type=none）PROD 220 个；README 的
  "273/264" 来自 release 全矩阵构建（子模块未检出，无法本地复现该数字来源）。
  movbe 13 个从 base（x86+ARM 共用）移到 arm64-only：**ARM64 二进制测试数
  不变**（仍注册），x86-64 二进制少 13 个（可接受——本仓库目标是 ARM64，
  x86 仅供参考；且 movbe 系列本就是 ARM64 core-179 研究探针，归类 arm64
  语义正确）。README 计数段落不因目录移动改变（ARM64 数量不变），仅当
  rowmajor 集成后 +1（PROD 264→265？无法验证 release 数）——README 计数
  是 release 矩阵口径，本地 220 无法核实 264/273 的构成，**保守处理：README
  计数不动**，只在表格行中加 rowmajor 名称，计数修正需另行核实（诚实原则：
  不能编造 release 口径数字）。

## 决策

- movbe 13 文件 `git mv` 到 `tests/cpu/arm64/`（保留 git 历史跟进）。
- `tests/cpu/meson.build` misc 块移除 13 行 + 注释更新（24→11，说明 movbe
  系列已迁往 arm64 分类）。
- `tests/cpu/arm64/meson.build` `arm64_tests` 加 13+1 条目（带归类注释）。
- 文档 3 处路径同步（`tests/cpu/misc/` → `tests/cpu/arm64/`）。
- README ARM64 表格"触发配方"行加 `neon_rot_ldr_at_top_rowmajor`。
- 不动 x86 任何逻辑：misc 块的移除对 x86-64 构建的影响 = x86 二进制少 13 个
  movbe 测试（这 13 个本来就是 core-179 ARM 研究探针；misc 块中其余 11 个
  mite/movdq2q/movmskpspd/movq2dq/mrn_*/partial_store_forwarding 保持 x86+ARM
  共用不变）。

## One-patch-per-unit 分解

### Task 1: 迁移 movbe 系列 13 个文件到 arm64 分类

文件变更：
- `git mv tests/cpu/misc/movbe*.cpp`（13 个文件）→ `tests/cpu/arm64/`
- `tests/cpu/meson.build`：misc 块删 13 行，注释 "24 tests" → "11 tests"，
  补一行说明 movbe 系列已迁往 arm64（core-179 研究探针归类）
- `tests/cpu/arm64/meson.build`：`arm64_tests` 加 13 个条目（分组注释：core-179
  movbe 字节交换探针系列，可移植 __builtin_bswap32，源自 misc 套件）
- `docs/MOVBE_SDC_CORE179_LOCALIZATION_REPORT.md` / `_V3.md` /
  `docs/CORE179_SDC_REPORT_CN.md`：路径 `tests/cpu/misc/movbe` → `tests/cpu/arm64/movbe`

验证（真实命令 + 引用真实输出）：
1. `ninja -C builddir` — 零新增 error/warning ✅
2. `./builddir/sdcshield --list-tests | grep -c "^movbe"` — 13（全部仍注册）✅ 总数 220 不变
3. `./builddir/sdcshield -e movbe -t 3000 -n 1` — `exit: pass` ✅
   （注：满核运行 movbe 间歇性 fail，**且仅 CPU 179 失败**——这是本机已知的
   core-179 SDC 硬件缺陷，正是 movbe 系列要捕获的目标，非迁移引入。
   已用 worktree 构建迁移前二进制对照验证：机器码逐指令相同、失败 cpu-mask
   位置相同（仅 179）。故功能验证改用 `-n 1`（绑核 0，避开缺陷核）。）
4. `./builddir/sdcshield -e movbe_dump -t 3000 -n 1` — `exit: pass` ✅
   另抽查 probe_a/b/c/d/e 单线程均 pass
5. 回归：`./builddir/sdcshield -e zstd19 -t 3000`（满核）— `exit: pass` ✅
6. x86-64 非回归：diff 检查确认 misc 块移除对 x86 是"少 13 个测试"而非构建破坏
   （misc 剩余 11 个文件仍在 tests_set_base；arm64 块整体在 aarch64 guard 内）✅

- [x] Task 1 完成（commit 93c470b，已推 origin/main）

### Task 2: 集成 neon_rot_ldr_at_top_rowmajor 到 arm64 分类

文件变更：
- `tests/cpu/arm64/neon_rot_ldr_at_top_rowmajor.cpp`（已在位，untracked → git add）
- `tests/cpu/arm64/meson.build`：`arm64_tests` 加 `'neon_rot_ldr_at_top_rowmajor.cpp'`
  （注释：ldr_at_top 的扫描顺序变体——偶数轮升序/奇数轮降序，区分槽位局部
  vs 前进位置 SDC 特征；aarch64-only）
- `README.md`：ARM64 触发配方表格行加该测试名（不动计数段落）

验证：
1. `ninja -C builddir` — 零新增 error/warning ✅（删除目标文件强制全新编译复查，无告警）
2. `./builddir/sdcshield --list-tests | grep neon_rot_ldr_at_top_rowmajor` — 已注册 ✅（总数 220→221）
3. `./builddir/sdcshield -e neon_rot_ldr_at_top_rowmajor -t 5000` — `exit: pass` ✅
   （用户将 CPU 179 hotplug 下线后满核运行：另跑 10s 长测也 pass）
4. 回归：`./builddir/sdcshield -e zstd19 -t 3000` / `-e neon_rot_2src -t 3000` — `exit: pass` ✅
5. x86-64 非回归：条目在 aarch64-only 构建块内 ✅

- [x] Task 2 完成（commit 8e397ec，已推 origin/main）

## 备注

- 两个 Task 各一个 commit，按 patch discipline 顺序执行（Task 1 验证+提交+
  push 后才开始 Task 2）。
- rowmajor 的 `test_run` 中 `switch (i % 4)` 各 case 都赋值 `res`，无 default —
  编译器可能告警 uninitialized；实测为准（Task 2 构建时观察，若告警则修文件
  加 default: __builtin_unreachable() 或初始化 res）。
- rowmajor 文件在 aarch64 下用 `aligned_alloc` 而非框架的 `aligned_alloc_safe`
  （misc 系列用后者）——不动用户文件内容，仅集成（用户写的探针，保持原样；
  框架 API 兼容性已由编译验证）。
- 提交时 README 只 add 本次改动 hunk（用户 25-30 行未提交改动不带入）。
  具体做法：`git diff README.md` 输出中手工分离 hunk，`git apply --cached`
  只应用表格行 hunk。
