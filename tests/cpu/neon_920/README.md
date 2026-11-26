# neon_920 — NEON 类输入测试收集包（172 件）

**用途**：`docs/research/neon-input-inventory.md` 清单所列 NEON 类输入测试的**复制快照**，
整理成一包供鲲鹏 920（无 SVE）等主机选测、交接与评审——拿到即可按 id 跑。

**性质（必读）**：
- **复制不移动**：原文件仍在 `tests/cpu/` 原位，权威版本永远以原位为准；本目录不参与构建
  ——无 meson.build、零 meson 引用（同测试 id 重复注册会破坏构建，**禁止**把本目录加进任何
  meson.build）；
- **基线单一**：172 件全部复制自单一提交 `8581fa3f`（`git show` 导出，cmp 机验 172/172
  字节一致，原位零改动）；
- **子目录**：与 `tests/cpu/` 下原文件夹一一对应——fmov 14 / fp64 29 / L3_cache_920 25 /
  memory-access 5 / misc 62 / neon_memcpy_cache 26 / vector 11，共 172；
- **L2_cache 整族未收录**（2026-11-26 用户指示：该文件夹仍在途未完成；清单 md 已相应移除
  该节，合计 223→172，文件夹定稿后如需收录另行补齐）。

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
