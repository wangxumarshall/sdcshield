# Plan: GHA 每次 CI 构建后打包 built/ 自包含产物（bin+libs+脚本+元数据）

日期：2026-09-20
分支：`feat/gha-package-built`（从 main 切出）
关联：`.github/workflows/multi-os-verify.yml`、`scripts/offline-build/package-built-artifacts.sh`

## 目标

每次 Multi-OS Verify CI 构建（15 镜像矩阵，cron 每日 + workflow_dispatch）后，
每个 SP 在 GitHub 上产出一版**自包含 built/ 包**，结构与本地 podman 链路的
`third-party/rpms/.../built/` 完全一致：

```
sdcshield            (stripped 二进制)
libs/                (非系统自带的运行时 .so)
run-sdcshield.sh     (设 LD_LIBRARY_PATH 后 exec)
MANIFEST.tsv         (文件 sha256 清单)
VERSION              (git sha / series / cpp_std / binary-sha256)
BUILD-HASH           (与 build-all.sh compute_build_hash 公式一致)
```

交付形态：每 SP 一个 tar.gz，作为 workflow artifact 上传（`if: always()`，
verify 失败也上传，便于诊断——与现有日志上传哲学一致）。公开仓 artifact
免费且默认保留 90 天，天然满足"每次 CI 构建一版"。

## 背景事实（全部实测，2026-09-20）

1. **GHA 与本地镜像同源**：`ghcr.io/wangxumarshall/sdcshield-offline:24.03-LTS-SP3`
   与本地 `localhost/openeuler-offline:24.03-LTS-SP3` 同 image ID（`c7ad55731a87`），
   20.03-LTS-SP4 同为 `56da1abfc830`。构建环境零差异。
2. **15 镜像工具探测全通过**（libatomic=Y ×15、20.03 全系 toolset 库=Y、
   git=Y ×15、strip=Y ×15）→ 打包所需工具镜像内全有，无需装包。
3. **20.03-LTS 无 GNU tar**（有 bsdtar）→ 脚本需 `command -v tar || bsdtar` 兜底
   （与 container-build.sh TAR_BIN 同模式）。
4. **测试覆盖与本地链路等价**：本地 built/（24.03-SP3，09-12 构建）221 测试，
   sleef=0 acl=0 isal=10；sleef/acl 缺席是**镜像层限制**（24.03 cmake 断链缺
   libuv.so.1/jsoncpp.so.25 且 RPM 树无解；22.03/20.03 无 cmake），本地与 GHA 同样缺席。
   测试数随 commit 演进（host 当前 241、GHA canary 09-17 实测 289），
   plan 不断言固定值，只做 `>100` 下限断言（与现有 self-check 一致）。
5. **libs 收集策略**（对齐 package-built-artifacts.sh 的判定规则，但 ldd 直跑）：
   - 20.03：二进制 RPATH 指向 `/opt/openEuler/gcc-toolset-10/root/usr/lib64`，
     ldd 列出的 `/opt/openEuler/*` 库全拷（libstdc++/libgcc_s/libatomic，
     cp -L 连 real file 一起）；
   - 22.03/24.03：恒拷 `libatomic.so.1`（最小目标机缺，镜像 `/usr/lib64/` 有，
     与本地从 LTS RPM 提取同源）；
   - 其余（libc/libm/libz/libzstd/libgmp/libstdc++(22.03/24.03)）视为系统自带。
   - 24.03 本地 built/libs 的 libarm_compute*.so（22MB）是老方案遗留死重
     （ACL 已改 vendored 静态 `-l:libarm_compute-core.a`，ldd 不依赖），
     GHA 侧不拷——产物更小更干净，无功能损失。
6. **isal 链接方式一致**：GHA 容器内 vendored build.sh → 静态 libisal.a；
   本地链路用宿主 install/ 静态 .a。均静态链接，无运行时差异。
7. `gh` CLI 本机未登录 → 端到端 CI 验证需要用户在 GitHub 页面手动 dispatch
   一次（workflow_dispatch 对任意分支可用），或提供 PAT 后 `gh workflow run`。
8. repo 公开（HTTP 200）→ artifact 无配额顾虑。

## 任务分解（one patch per unit）

### Task 1: 新增 `scripts/gha/package-built.sh`

在容器内运行的打包核心（GHA verify job 原生容器模式，无需 podman 包装）。

CLI：`package-built.sh <builddir> <outdir> <series> <sp>`
（builddir 含 sdcshield 二进制；outdir 为产物落盘目录）

逻辑：
1. strip 二进制（`strip --strip-debug --strip-unneeded`，失败容错 `|| true`）；
2. ldd 收集 libs（规则见背景事实 5；`cp -L` 解引用 symlink，soname 与 real
   file 两个名字都落盘——与本地 tar -h 双拷同效果）；
3. 生成 `run-sdcshield.sh`（模板与 package-built-artifacts.sh L162-205 逐字一致，
   含 `full` 两段式 eigen 运行模式）；
4. 写 `MANIFEST.tsv`（file/sha256/size/source）+ `VERSION`（git sha、series/sp、
   cpp_std/macro、binary-sha256、image tag）；
5. 写 `BUILD-HASH`：公式与 `build-all.sh compute_build_hash` **完全一致**
   （`git ls-tree -r HEAD -- framework tests meson.build meson_options.txt` 的
   sha256 + container-build.sh sha256 + cpp_std + macro + image-manifest.tsv 的
   input-hash + series-sp_label 拼串再 sha256）——保证 GHA 产物与本地 built/
   的 BUILD-HASH 同 commit 时相等，可作为两条链路等价的机器可查锚点；
6. `TAR_BIN=$(command -v tar || command -v bsdtar)` 打 tar.gz，
   名 `sdcshield-<OS_TAG>-<git-sha8>.tar.gz`。

**验证（本地 podman，同镜像 canary）**：
- [x] `bash -n scripts/gha/package-built.sh` 语法通过；
- [x] 24.03-LTS-SP3 canary：当前 main 代码在容器内重建（325 测试，271MB→
      strip 后 8.8MB），跑脚本 → tar.gz 3.8MB，结构 = sdcshield +
      libs/(libatomic.so.1 + .so.1.2.0) + 脚本 + 元数据；新二进制 ldd 无
      libarm_compute（vendored 静态链接生效，验证了不拷 ACL .so 的判断）；
- [x] pristine 实跑：list-tests 325（>100 ✓）+ `-e zstd19 -t 2000 -n 1`
      → `exit: pass`；run-sdcshield.sh 透传 → `exit: pass`；
- [x] 20.03-LTS-SP4 canary（旧二进制，验证 libs 收集 + bsdtar 兜底）：
      libs/ = libstdc++.so.6+.so.6.0.28 / libgcc_s.so.1+libgcc_s-10.so.1 /
      libatomic.so.1+.so.1.2.0（toolset 三库双名字全收集）；bsdtar 兜底
      生效（20.03-LTS 无 GNU tar）；pristine list-tests 287 + zstd19
      `exit: pass`；
- [x] BUILD-HASH 双向验证：① 用本地 built/ 的源 commit 08c37be3 重算公式 =
      本地 built/BUILD-HASH（92263cea... 相等）；② 24.03 canary 包的
      BUILD-HASH（876c5826...）= 宿主重算同 commit edb8633 公式（相等）。

### Task 2: workflow 集成（`multi-os-verify.yml`）

verify job 的 Build step 之后、Verify sweep 之前插入两步：

```yaml
- name: Package built/ (self-contained tarball)
  shell: bash
  run: |
    OS_TAG="openEuler-${SERIES}$(case $SP in LTS) echo LTS;; SP*) echo LTS_$SP;; esac)"
    bash scripts/gha/package-built.sh builddir "dist/$OS_TAG" "$SERIES" "$SP"

- name: Package smoke-verify (pristine, libs/ only)
  shell: bash
  run: |
    # 解 tarball 到干净目录，仅 LD_LIBRARY_PATH=libs 运行——等价
    # verify-built-pristine.sh smoke 的容器内形态
    ... --list-tests 断言 >100；-e zstd19 -t 2000 -n 1 断言 exit: pass
```

Upload 部分新增独立 step（原 verify logs 上传不动）：

```yaml
- name: Upload built/ tarball
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: built-${{ matrix.series }}-${{ matrix.sp }}
    path: dist/*/sdcshield-*.tar.gz
    retention-days: 90        # 公开仓默认即 90，显式写出
    if-no-files-found: error  # 打包失败应显式红，不静默
```

（smoke 失败 fail 整个 job——产物闸门是真实门禁；tarball 上传 `if: always()`
保失败现场。`pr.yaml` 不动——PR 角落三哨兵保持快。）

**验证**：
- [x] `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/multi-os-verify.yml'))"`
      语法通过；
- [x] smoke-verify 命令串在本地 24.03-SP3 容器按 GHA 目录布局 replay 实跑
      （pristine list-tests 325 + zstd19 exit: pass）；tar/bsdtar 兜底在
      20.03-LTS / 22.03-LTS / 24.03-SP3 三镜像探测确认；
- [ ] 端到端：push `feat/gha-package-built`（已推）→ workflow_dispatch
      （smoke 档）→ 16 job 全绿 → Actions 页面确认 15 个 `built-*`
      artifact 存在 → 本地下载 1 个 pristine 实跑 zstd19 通过。
      （待用户 dispatch；注意 main 的重复测试 ID bug 已在本分支修复
      —— bea8d2b，dispatch 必须选本分支或等修复合入 main）

### Task 3: 文档同步

- [x] `scripts/gha/README.md`：新增 package-built.sh 条目（表格 + 用法 + 产物
      结构 + libs 收集规则 + BUILD-HASH 等价锚点 + 与本地
      package-built-artifacts.sh 的分工）；
- [x] `docs/multi-version-build-deploy.md` §6.2.1：新增"下载 CI 预构建产物"
      小节——网页路径 + `gh run download` 命令 + 保留期 90 天 + tarball 使用
      方法 + 产物闸门 + 与本地 built/ 的关系（BUILD-HASH 等价、binary 不保证
      bit 级一致、长期沉淀仍走 package-release.sh）；
- [x] `README.md`：快速开始区补"从 CI 下载预构建二进制"入口段（指向
      docs §6.2.1）；
- [x] 验证：文档命令用 canary 产物真实跑通（tar -xzf → cd →
      ./run-sdcshield.sh --list-tests = 325 → -e zstd19 -t 2000 -n 1 =
      exit: pass）；`gh` 命令标注"需 gh auth login"。

### Task 4（2026-09-20 追加）: Release 永久保存开关

需求：artifact 90 天滚动过期，不是版本库；需要人工选择性把某次构建的 15 个
tarball 钉进 GitHub Release（永久保存、公开仓免费、单资产上限 2GB）。

设计：
- `workflow_dispatch` 新增输入 `publish_release`（boolean，默认 false）；
  cron 触发时该输入不存在 → 恒 false，行为不变。
- 新增 `release` job：`needs: verify` + `if: inputs.publish_release == true`，
  download-artifact(pattern: built-*, merge-multiple) → `gh release create`
  上传 15 个 tar.gz，tag 形如 `build-YYYYMMDD-<sha8>`。
- `permissions` 从 `contents: read` 提升为 `contents: write`（Release 创建/
  上传需要；只影响本 workflow 的 GITHUB_TOKEN，不影响其他 workflow）。
- 发布频率语义：里程碑式手动钉版（重要合入后）；nightly 由 90 天滚动
  artifact 天然承担，不当 nightly 用。

验证：
- [x] YAML safe_load 通过（jobs: verify/report/release；inputs 含
      publish_release）；
- [x] release job 命令串本地干跑：模拟 download-artifact merge-multiple 的
      dist/ 平铺布局 + TAG 生成（`build-20260920-edb8633d` 形如预期）+
      `dist/*.tar.gz` glob 展开验证；
- [ ] 端到端：dispatch（勾选 publish_release + smoke 档）→ verify 16 job
      全绿 + release job 绿 → Releases 页面出现 build-* tag 与 15 资产 →
      `gh release download`（或网页）取回一个 tarball，在对应
      localhost/openeuler-offline 容器 pristine 实跑 zstd19 通过。
- [x] 文档：docs/multi-version-build-deploy.md §6.2.1 补 Release 小节
      （永久 vs 90 天的分工、发布操作、下载方式）；README 快速开始段补一句。


## 边界与诚实声明（写入文档）

- GHA tarball 的测试覆盖 = 本地 podman 链路同级（sleef/acl 因镜像 cmake
  断链/缺失而缺席——两链路共同边界，非 GHA 独有缺陷）；
- package smoke 在同镜像里跑，检不出"镜像有而最小目标机没有"的库
  （如 libgmp）——与本地 verify-built-pristine.sh 同盲区，两链路对等；
- artifact 按 run 隔离、90 天滚动，不是永久版本库；要长期版本走
  package-release.sh 的 Release 路线（后续可选，不在本 plan 范围）；
- 本地 built/ 与 GHA tarball 的 binary-sha256 不保证 bit 级一致
  （构建路径细节差异），但 BUILD-HASH（输入配置指纹）同 commit 时相等。

## 执行方式

每 Task 一个 commit（Task1 脚本 / Task2 workflow / Task3 文档），每 commit 前
跑完该 Task 的全部验证 checkbox，验证通过后 push 到 `feat/gha-package-built`
（不推 main）。合入 main 走 PR。
