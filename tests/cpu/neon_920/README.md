# neon_920 — NEON 类输入测试运行清单（176 id）

**用途**：本目录 `neon-input-inventory.md` 清单所列 NEON 类输入测试的**运行清单**，
整理成一包供鲲鹏 920（无 SVE）等主机选测、交接与评审——按 id 直接跑。

**形态（必读，2026-11-26 main 回迁版）**：
- **本目录在 main 上是清单不是源码镜像**：源文件全部在 `tests/cpu/` 原位（权威版本永远
  以原位为准）。其中 155 件 main 原本就有；其余 17 件（fmov L2 对照组 4、memory-access
  ma 基件 4、fmov_train droop/L2L3 对 3、写侧快检测器 3、fp64_dup2/3 2、barrier v2 1）
  于 2026-11-26 自 feat/migrate-opendcdiag-tests 分支逐字节回迁原位并接 meson 注册
  （feat/migrate-neon920-pack 分支，前置修复 build 死引用一件）；
- **本目录不参与构建**——无 meson.build、零 meson 引用（同测试 id 重复注册会破坏构建，
  **禁止**把本目录加进任何 meson.build）；
- **清单来源**：分支侧同名目录曾以单一提交 `8581fa3f` 快照复制 172 件源码（cmp 机验
  172/172 字节一致）+ 本 `test_ids.txt`；main 回迁按用户指示去重（main 已有的不再复制），
  仅保留运行清单与指引；
- **机验口径（2026-11-26，本机）**：`test_ids.txt` 176 个 id 对照 main `--quality=-1
  --list-tests` 注册表 **176/176 在册零缺失**；17 个新测试全部单线程+全核 `exit: pass`，
  PROD 计数 728→744（+16 PROD，barrier v2 为 BETA 不入默认集）；整包
  `--test-list-file` 连跑 **177/177（176 id + 框架附加 mce_check）exit: pass**
  （SVE 主机以 `ADAPTIVE_MEM_SDC_ISA=neon ADAPTIVE_MEM_SDC_COUNT=64` 变通，见下方
  已知问题；离线核 122 不在 cpuset 内）。

## 怎么跑这批测试

源文件不能独立编译——测试代码依赖本仓框架（`sandstone.h` 等）。需在本仓（或含同款测试的
sdcshield 仓）完整构建后，按测试 id 运行：

```console
# 1) 完整仓构建（全新克隆先装 vendored 依赖，见顶层 README「从源码构建」）
./scripts/build.sh

# 2) 单跑（id 一览见 test_ids.txt，与 --list-tests 所列一致）
./builddir/sdcshield -e <测试id> -t 5000

# 3) 整包连跑（按文件内顺序执行）
./builddir/sdcshield --test-list-file tests/cpu/neon_920/test_ids.txt
```

**test_ids.txt**：176 个测试 id，来自 172 个源文件——`k7pattern.cpp` 一文件四测试
（k7p_zero/one/rand/hiham）、`spinlock_stress_cmpxchg_even_odd_neon_fp64.cpp` 一文件双测试
（even_n/odd_n）；7 个 fmov_train 文件为「ARM64 实现 + 非 ARM64 占位桩」同 id 双声明，
去重后各计 1。

**920 主机口径**：本包全部为 NEON/标量数据面用例，无 SVE/SVE2/SME 门控（源码机验零
`ARM64_REQUIRES_` 声明）——无 SVE 的鲲鹏 920 主机全量可执行。显式请求（`-e` /
`--test-list-file`）绕过芯片 spec 过滤，个别用例若在特定主机 SKIP，见其 `log_skip` 理由。

**已知问题披露（main 既有缺陷，非本包/本次回迁引入；2026-11-27 本机 gdb 实证）**：
`adaptive_mem_sdc` 在 **含 SVE 的主机**上会 SIGILL——其 SVE 引擎 TU
（`misc/adaptive_mem_sdc_sve.cpp`）以 `-march=armv9-a+sve2` 编译，GCC 把标量
逐元素循环（LOAD_MLA 掩码环，:423）向量化后引入 **SVE2-only `WHILERW`**（库内
3 实例；文件头注「当前零 SVE2 指令」声明不成立，与 CLAUDE.md 记载的 WHILEWR
审计同类），而运行期 HWCAP 梯子
SVE2>SVE>NEON 会把 SVE1 主机路由进该引擎 → `sve_fill_golden`（`adaptive_mem_sdc_sve.cpp:423`）
执行 WHILERW 触发 SIGILL（本机 SVE1-256 实测：默认梯子/`ISA=sve` crash、`ISA=neon` pass）。
本包目标机（无 SVE 920）梯子落 NEON，不受影响。**SVE 主机变通**：
`ADAPTIVE_MEM_SDC_ISA=neon`（同时建议 `ADAPTIVE_MEM_SDC_COUNT=64` 小足迹）或
`-e` 排除该 id；根因修复（+sve2 编译面或 HWCAP 门控）另行立项，不在本包范围。
涉及文件与 main 逐字节一致（`git diff main` 空），缺陷属 main 既有状态。

**与分支快照的差异披露**：分支侧 172 件镜像中，misc/ 目录还包含同名 k 系
（k2pureload/k3res/k4gap/k4bgap/k5far/k5inter_rand/k7pattern）——main 上的同名件是
**L1D 域判别格战役的母本**（misc/ 原位）；与它们同名不同目录不同几何的
`tests/cpu/L2_cache/` L2 三档扩展族（19 族 × 128k/256k/512k，57 件 63 id）已于
**2026-11-27 独立回迁 main**（不在本清单 176 id 内，按 `neon_rot_ldr_at_top_rowmajor_*_l2_*`
id 直接跑；该族不在原 172 件镜像范围，因分支侧快照时尚在途）。
