# 宿主机 GCC10 兼容注入（`<barrier>`）— 计划与 as-built 记录

日期：2026-11-02
范围：`meson.build`、`framework/compat/cpp23_polyfill.h`、`README.md`
状态：已实现并本机验证（完整链接依赖同日已存在的 PR #434）

## 问题

openEuler 22.03 宿主机（GCC 10.3.1）上，新版 main 无法完整构建：

```
../framework/device/cpu/topology_cpu.h:16:10: fatal error: barrier: No such file or directory
```

GCC10 的 libstdc++ 不提供 `<barrier>`（GCC 11 才加入）。仓库已有两套兜底：
`framework/compat/cpp23_polyfill.h`（提供 `std::barrier` 等符号）与
`framework/compat/barrier`（`-isystem` 让 `#include <barrier>` 命中 shim）。
但两者只在**容器构建**里通过 `CXXFLAGS` 注入
（`scripts/offline-build/container-build.sh:316-324`），宿主机直编按
README 的 `meson setup builddir --buildtype=release` 不会注入，于是
`#include <barrier>` 在源码头文件里直接失败，后置的 polyfill 也无从生效。

## 设计

1. `meson.build` 顶部（compiler 探测之后、任何 target 之前）用
   `cpp.has_header('barrier', args: ['-std=gnu++20'])` 探测本机 libstdc++。
   探测失败时以 `add_project_arguments` 注入与容器完全相同的标志：
   `-DSDCSHIELD_COMPAT_LIBSTDCXX -include <polyfill> -include unistd.h
   -isystem <compat>`（`language: 'cpp'`）。
2. `framework/compat/cpp23_polyfill.h` 的外层守卫增加
   `SDCSHIELD_COMPAT_LIBSTDCXX` 别名；既有 `OPENEULER_22_03/20_03`
   路径不变，容器构建与宿主机注入可叠加（`#pragma once`，幂等）。
3. 24.03+（libstdc++ 有 `<barrier>`）探测为真 → 不注入任何参数，构建
   与之前逐字节一致；x86-64 同理（有头就不注入）。
4. 旧编译器不支持 `gnu++23` 是 meson 选项层的限制，无法在 project()
   之后改写；README 明确宿主机 22.03/20.03 需
   `-Dcpp_std=gnu++20`（与容器脚本一致）。容器路径保持不变。

## 验证（本机 aarch64 / GCC 10.3.1 / 96 核 / 295 GiB）

- 裸 `meson setup builddir --buildtype=release -Dcpp_std=gnu++20`（不带
  任何 CXXFLAGS）：打印
  `Message: libstdc++ has no <barrier>: injecting framework/compat C++23 polyfill`，
  `compile_commands.json` 中 TUs 含
  `-DSDCSHIELD_COMPAT_LIBSTDCXX -include .../cpp23_polyfill.h -include unistd.h -isystem .../framework/compat`。
- 构建：`barrier: No such file or directory` 消失；565/616 TU 通过后，
  余下 4 个 TU 失败：
  `tests/cpu/sve/case/movbe_dump_probe_{f,g1,g2,h}_sve.cpp`
  `internal compiler error: Segmentation fault (RTL pass: expand)`。
  这 4 个是**同日已存在的独立存量问题**（每日 15 镜像 CI 20.03/22.03
  job 红根因），已由开放且 mergeable 的 PR #434
  （`fix/movbe-sve-cleanup-gcc10-ice`，cleanup 函数加 `optimize("O1")`）修复。
- 本地应用 PR #434 补丁后：**56/56 链接成功、0 error**，二进制可用：
  - `-e zstd19 -t 2000 -n 1` → `exit: pass`
  - `-e neon_rot_ldr_rand -t 2000 -n 8` → `exit: pass`
  - `-e neon_rot_ldr_rand_thread -t 2000 -n 8` → `exit: pass`
  - `--list-tests` 387 条（本机 aarch64 无 SVE，含 spec 过滤后的集合）。

## 边界

- 24.03 / GCC 12+ / x86-64 未在本机实测（无对应工具链）；探测为真时
  不加参数、代码路径完全不变，属纯增量。
- 完整宿主机构建目前需要 PR #434 一并落地；本 PR 不复制其修复。
