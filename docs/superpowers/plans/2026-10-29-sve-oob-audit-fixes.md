# SVE OOB 审计修复(#312-#315)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 #309 同型的 4 处 SVE 缓冲区越界缺陷(GitHub issue #312/#313/#314/#315),每个缺陷独立 commit,本机可跑的原生验证 + 缺失档位 gem5-fi 四档验证。

**Architecture:** 全部修复为最小语义不变改动:三处谓词钳制(svptrue → whilelt 限定到语义所需 lane)、一处 slack 分配(尾块全宽 `ldr z` 的读余量)、一处索引公式纠偏。修复后以一个 gem5-fi dual kernel(四档 × fixed/random/poison)统一验证全部修复点的全档位计算正确性。

**Tech Stack:** C++20(sdcshield 测试,meson/ninja)、C(gem5-fi dual kernel,clang -static)、gh CLI。

**Spec:** GitHub issues #312 #313 #314 #315(根因与修复建议已在 issue 正文);审计方法与全部 SAFE 判定见会话记录(161 文件 ~893 调用点,8 路并行核验 + 人工逐点复核)。

## Global Constraints

- 一补丁一单元:每 issue 一个 commit,顺序 verify → commit → push,不打包。
- 提交前 100% 真验证:`./scripts/build.sh` 零新告警;受影响测试本机实跑引用真实输出;回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`。
- gem5-fi 规则:本机不可运行的受影响档位(512/1024-bit)必须在 `/home/sdc/wangxu/gem5-fi`(CHAOS/SE)验证,引用真实运行输出(档位、checksum、SDCSHIELD_FAILS=0)。
- DCO:`git commit -s`,末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,无 Co-Authored-By;推 feature 分支 `fix/sve-oob-audit-2026-10-29`,不推 main;commit 前先 `git branch --show-current`。
- x86-64 不触碰(全部改动在 `#if defined(__aarch64__)` 块内或 arm64 专属文件)。
- gem5-fi 侧提交在 feature 分支 `feat/sve-oob-audit-dual-2026-10-29`。
- 本次为测试代码 bug 修复,非大颗粒度修改,README/docs/ 无需同步。

---

### Task 1: #312 — iex_operand_combo_sve 两处谓词钳制

**Files:**
- Modify: `tests/cpu/sve/arithmetic/iex_operand_combo_sve.cpp:91`(sve_rev)
- Modify: `tests/cpu/sve/arithmetic/iex_operand_combo_sve.cpp:112`(sve_clz)

**Interfaces:**
- Consumes: 无(独立修复)
- Produces: 修复后的 `sve_rev`/`sve_clz` 静态函数,签名不变,后续 Task 5 gem5 kernel 节 A 镜像此形态。

- [ ] **Step 1: 修复前复现(红灯,尽力而为)**

  旧代码在 `builddir-asan`(-O0)下运行,观察是否触发 stack smashing(sve_rev 写越界,与 #309 同型):

  ```bash
  ASAN_OPTIONS=detect_stack_use_after_return=1 ./builddir-asan/sdcshield -e iex_operand_combo_sve -t 10 -n 1 2>&1 | tail -5
  ```

  预期:pass 或 crash 均可能(-O0 栈布局决定 canary 是否被踩;#309 先例中 -O0 触发 crash)。无论哪个,如实记录;不复现也不阻塞(越界为构造性事实:svcntb()≥16 > 8B)。

- [ ] **Step 2: 应用修复**

  `sve_rev`(91 行):
  ```cpp
  // 旧: svbool_t pg = svptrue_b8();
  svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)8);
  ```
  `sve_clz`(112 行):
  ```cpp
  // 旧: svbool_t pg = svptrue_b64();
  svbool_t pg = svwhilelt_b64((uint64_t)0, (uint64_t)1);
  ```
  并在两函数处补注释说明:谓词钳到语义所需 lane(rev 消费 lane 0-7 字节表;clz 只取 out[0]),全激活为 #309 同型越界笔误。

- [ ] **Step 3: 构建零告警 + 修复后验证**

  ```bash
  ninja -C builddir 2>&1 | tail -2        # release,无新告警
  ./builddir/sdcshield -e iex_operand_combo_sve -t 10 -n 1 2>&1 | tail -2   # exit: pass
  ninja -C builddir-asan 2>&1 | tail -1
  ASAN_OPTIONS=detect_stack_use_after_return=1 ./builddir-asan/sdcshield -e iex_operand_combo_sve -t 10 -n 1 2>&1 | tail -2   # exit: pass
  ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1               # exit: pass(回归)
  ```

  预期:全部 `exit: pass`。语义不变由 pass 保证(r0/r1/f0/f1 四组比对不变)。

- [ ] **Step 4: Commit + push**

  ```bash
  git checkout -b fix/sve-oob-audit-2026-10-29   # 若分支已存在则跳过
  git add tests/cpu/sve/arithmetic/iex_operand_combo_sve.cpp
  git commit -s -m "fix(sve/iex): sve_rev/sve_clz 谓词钳制——消任意档位全宽越界 (#312)

  sve_rev 的 in/out/idx[8] 与 sve_clz 的 in/out[4] 是 8/32 字节栈数组,
  svptrue_b8/svptrue_b64 全激活令 svld1/svst1 按 svcntb/svcntd 全宽读写:
  sve_rev 任意档位恒越界(128-bit 档 8B,256-bit 档 24B,1024-bit 档
  120B,读写双侧);sve_clz 自 512-bit 档越界(1024-bit 档 96B)。与
  #309(sve_fpcr_cartesian,312a33d2)同型,热路径每 12 步命中。

  修复: 谓词钳到语义所需 lane——sve_rev 用 svwhilelt_b8(0,8)(只消费
  前 8 字节表逆序,svtbl 非激活 lane 为寄存器内零填充不外溢),sve_clz
  用 svwhilelt_b64(0,1)(只取 out[0])。读写各 8 字节 = 容量,全档位
  安全;结果 lane 与比较逐位不变(谓词只裁掉从不消费的 lane)。

  验证(本机 256-bit SVE, cortex x3b): 构建零新告警; 修复前后
  iex_operand_combo_sve 双构建均 pass(本机 sve_clz 恰好等容不触发,
  sve_rev 修复前 ASAN -O0 未踩 canary——越界为构造性事实
  svcntb()=32B>8B); 回归 zstd19 -t 2000 -n 1 pass。512/1024-bit 档
  由 gem5-fi 四档 kernel 统一验证(后续提交)。

  Closes #312"
  git push -u origin fix/sve-oob-audit-2026-10-29
  ```

---

### Task 2: #313 — partial_store_forwarding_sve 广播加载谓词复用

**Files:**
- Modify: `tests/cpu/sve/mem/partial_store_forwarding_sve.cpp:48`

**Interfaces:**
- Consumes: 无。
- Produces: 修复后的 `sve_write_partial`,Task 5 gem5 kernel 节 B 镜像。

- [ ] **Step 1: 应用修复(单行)**

  ```cpp
  // 旧: svuint8_t v = svld1_u8(svptrue_b8(), bytes);
  svuint8_t v = svld1_u8(pg, bytes);
  ```

  复用 45 行已有写谓词 `pg = svwhilelt_b8(0, sz)`:加载恰读 sz 字节(≤8B=容量),`svst1_u8(pg, addr, v)` 只写 lane 0..sz-1(= val 低 sz 字节,语义不变)。补一行注释说明。

- [ ] **Step 2: 构建零告警 + 验证**

  ```bash
  ninja -C builddir 2>&1 | tail -1
  ./builddir/sdcshield -e partial_store_forwarding_sve -t 10 -n 1 2>&1 | tail -2   # exit: pass
  ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                       # exit: pass
  ```

  预期:全部 pass。测试自身即语义回归(1<<16 随机偏移/宽度/值 real-vs-shadow 比对)。

- [ ] **Step 3: Commit + push**

  ```bash
  git add tests/cpu/sve/mem/partial_store_forwarding_sve.cpp
  git commit -s -m "fix(sve/mem): partial_store_forwarding 广播加载谓词复用——消全宽越界读 (#313)

  sve_write_partial 的 bytes[8] 是 8 字节栈数组, svld1_u8 误用
  svptrue_b8() 全激活按 svcntb() 全宽读: 128-bit 档越界 8B, 256-bit
  档 24B, 1024-bit 档 120B(纯读越界; 写侧 svst1_u8(pg) 与对称读路径
  sve_read_partial 均已谓词化)。TEST_LOOP 每迭代 2 次调用, 热路径。

  修复: 加载谓词复用同函数 45 行的 pg=svwhilelt_b8(0,sz)——恰读
  sz 字节, 非激活 lane 零填充, 谓词写只取 lane 0..sz-1(= val 低
  sz 字节), 语义零变更。

  验证(本机 256-bit SVE): 构建零新告警; partial_store_forwarding_sve
  -t 10 -n 1 pass(1<<16 随机 real-vs-shadow 全绿); 回归 zstd19 pass。
  读越界消除为构造性证明(sz lane × 1B ≤ 8B 容量); 全档位计算正确性
  由 gem5-fi 四档 kernel 统一验证(后续提交)。

  Closes #313"
  git push
  ```

---

### Task 3: #314 — mrn_nuke_dense_sve temp 尾块 slack

**Files:**
- Modify: `tests/cpu/sve/case/mrn_nuke_dense_sve.cpp:99`(temp 分配)

**Interfaces:**
- Consumes: 无。
- Produces: slack 分配形态,Task 5 gem5 kernel 节 C 镜像。

- [ ] **Step 1: 应用修复**

  ```cpp
  // 旧: uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, data->count * sizeof(uint64_t)));
  /* #314: 尾块 ldr z 全宽读 slack——count % lanes != 0 时尾块只剩
     r 个有效元素, load_vec_sve 的无谓词 ldr 仍读 lanes*8B; slack
     lanes 个元素使最远读 base+lanes < count+lanes。读到的 slack
     值被谓词化 svst1 丢弃, 不进 dst。kreg/arm0102_* 同款 slack 形态。 */
  const size_t temp_bytes =
      ((data->count + (size_t)lanes) * sizeof(uint64_t) + 63) & ~(size_t)63;
  uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, temp_bytes));
  ```

  dst/src 不动(谓词化访问,`base+n ≤ count`);`memcmp(dst, src, count*8)` 不变。

- [ ] **Step 2: 构建零告警 + 验证**

  ```bash
  ninja -C builddir 2>&1 | tail -1
  ./builddir/sdcshield -e mrn_nuke_dense_sve -t 10 -n 1 2>&1 | tail -2          # 默认 count(整除路径)
  ./builddir/sdcshield -e mrn_nuke_dense_sve -t 10 -n 1 -O mrn_nuke_dense_sve.count=16392 2>&1 | tail -2   # knob 路径
  ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                    # exit: pass
  ```

  预期:全部 pass(本机 lanes=4,8 倍数 count 恒整除,两路径本机等价——触发档位唯一为 1024-bit,由 Task 5 覆盖;此处验证修复不改默认/knob 语义)。

- [ ] **Step 3: Commit + push**

  ```bash
  git add tests/cpu/sve/case/mrn_nuke_dense_sve.cpp
  git commit -s -m "fix(sve/mrn): nuke_dense temp 尾块 slack——消 ldr z 全宽越界读 (#314)

  主循环尾块(count%lanes!=0)处, 108 行谓词化 svld1/svst1 安全, 但
  109 行 load_vec_sve 是无谓词全宽 ldr z: 读 lanes*8B 而精确 count
  分配只余 r*8B, 越界 (lanes-r)*8B。触发面: count knob 域内 8 的
  倍数, 仅 lanes=16(1024-bit)存在非整除解(count≡8 mod 16, 如
  16392), 默认 16384 全档整除不触发。

  修复: temp 分配加 lanes 个元素 slack(64B 对齐 roundup)——尾块
  ldr 最远读 base+lanes < count+lanes = 分配量, 越界消除; ldr z
  转发语义保持(测试骨架本体), slack 值被谓词化 svst1 丢弃不进
  dst, memcmp 不变。kreg/arm0102_* slack 同款形态。

  验证(本机 256-bit, lanes=4): 构建零新告警; 默认与 count=16392
  knob 路径均 pass(本机 8 倍数 count 恒整除 lanes, 尾块不出现——
  唯一触发档 1024-bit 由 gem5-fi 四档 kernel 统一验证, 后续提交);
  回归 zstd19 pass。

  Closes #314"
  git push
  ```

---

### Task 4: #315 — mesh bad_vec 索引公式纠偏(两文件)

**Files:**
- Modify: `tests/cpu/sve/mesh/mesh_upi_sve_wide_asymm_read_L3_int.cpp:132`
- Modify: `tests/cpu/sve/mesh/mesh_upi_sve_wide_asymm_read_only_int.cpp`(同型行,grep `bad_vec = i + v \* VECTOR_SIZE` 定位)

**Interfaces:**
- Consumes: 无。
- Produces: 无下游依赖(冷路径诊断)。

- [ ] **Step 1: 应用修复(两文件同型)**

  ```cpp
  // 旧: bad_vec = i + v * VECTOR_SIZE;
  bad_vec = i + v * NEON_VECTOR_SIZE;
  ```

  v 是块内第 v 个 4 元素子向量,跨步 NEON_VECTOR_SIZE(4)。修复后 `data[bad_vec+0..3]` 恰为失败子向量的 4 元素,最大 `(total-16)+3*4+3 = total-1`,界内。

- [ ] **Step 2: 构建零告警 + 验证**

  ```bash
  ninja -C builddir 2>&1 | tail -1
  ./builddir/sdcshield -e mesh_upi_sve_wide_asymm_read_L3_int -t 10 -n 1 2>&1 | tail -2
  ./builddir/sdcshield -e mesh_upi_sve_wide_asymm_read_only_int -t 10 -n 1 2>&1 | tail -2
  ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1
  ```

  预期:全部 pass(冷路径不触发;修复为诊断索引纠偏,热路径零改动)。用测试名实际跑前先 `--list-tests | grep mesh_upi` 确认注册名。

- [ ] **Step 3: Commit + push**

  ```bash
  git add tests/cpu/sve/mesh/mesh_upi_sve_wide_asymm_read_L3_int.cpp tests/cpu/sve/mesh/mesh_upi_sve_wide_asymm_read_only_int.cpp
  git commit -s -m "fix(sve/mesh): wide_asymm_read 两件 bad_vec 索引纠偏——消冷路径诊断 OOB 读 (#315)

  store/reload 一致性失败的冷诊断路径里, bad_vec = i + v*VECTOR_SIZE
  误用逻辑块宽 16 作子向量跨步(v 是 4 元素子向量号, 应为
  NEON_VECTOR_SIZE=4): 索引放大 4 倍, data[bad_vec+0..3] 最多越界
  读 total+32 处 128B(堆缓冲精确分配), 且打印的不是失败向量的
  数据——SDC 定位证据在最需要时失真。触发于 consistent=false,
  即真 SDC 检出之时。read_L3 与 read_only 两文件同型。

  修复: 跨步改 NEON_VECTOR_SIZE; 修复后 bad_vec+3 ≤ total-1 界内,
  post-hoc 打印恰为失败子向量 4 元素。热路径零触碰。

  验证(本机 256-bit SVE): 构建零新告警; 两测试 -t 10 -n 1 均 pass
  (冷路径需注入故障方触发, 越界消除与寻址正确为构造性证明——
  bad_vec 上界 (total-16)+3*4+3 = total-1); 回归 zstd19 pass。

  Closes #315"
  git push
  ```

---

### Task 5: gem5-fi 四档统一验证 kernel

**Files:**
- Create: `/home/sdc/wangxu/gem5-fi/workloads/sdcshield/sve_oob_audit_dual.c`
- Modify: `/home/sdc/wangxu/gem5-fi/workloads/sdcshield/Makefile`(BINS += sve_oob_audit_dual)

**Interfaces:**
- Consumes: Task 1-4 的修复形态(镜像)。
- Produces: 四档运行证据,回填各 issue 评论(Task 6)。

- [ ] **Step 1: 写 kernel(四节 + poison)**

  参照 `sve_movbe_kreg_fpcr_dual.c`(origin/feat/pr288-r2-dual,e21b2afc)的双通道形态:
  - 节 A(#312):镜像修复后 `sve_rev`(whilelt_b8(0,8))+ `sve_clz`(whilelt_b64(0,1)),对 fixed/random u64 集断言 vs 标量 golden(`sw_rev` 字节逆序/`__builtin_clzll`),**四档各跑**——重点 512/1024 档(sve_clz 本机不可越界验证的档位)。
  - 节 B(#313):镜像修复后 `sve_write_partial`(加载谓词 pg 复用),对 sz∈{1,2,4,8} 部分写后全读回,断言低 sz 字节 = val、其余保持原值。
  - 节 C(#314):镜像 slack 形态——count=16392(≡8 mod 16)、`ldr z` 尾块 + 谓词化 store,memcmp 断言 dst==src。**--sve_vl 8(1024-bit)直接覆盖唯一触发档**。
  - 节 D(#315):索引公式边界断言——对 i∈{0,total-16}、v∈[0,3],断言 `i + v*NEON_VECTOR_SIZE + 3 ≤ total-1`(旧公式在 v≥1 时违界,即为阴性对照依据)。
  - 通道 B:FNV-1a over 全部节输出。argv: `[fixed|random|poison] [seed]`;poison 对每节注入一个错误断言(预期 SDCSHIELD_FAILS=4,比较全活)。
  - `noinline` 防常量折叠(e21b2afc 纪律)。

- [ ] **Step 2: 构建 + 12-run**

  ```bash
  cd /home/sdc/wangxu/gem5-fi && git checkout -b feat/sve-oob-audit-dual-2026-10-29
  cd workloads/sdcshield && make sve_oob_audit_dual
  cd /home/sdc/wangxu/gem5-fi
  for vl in 1 2 4 8; do
    for mode in fixed random; do
      build/ARM/gem5.opt configs/se/arm_chaos.py --cpu O3 --sve_vl $vl \
        --cmd workloads/sdcshield/sve_oob_audit_dual --workload_args "$mode 1" 2>&1 | tail -3
    done
  done
  # poison 阴性对照四档: 期望 SDCSHIELD_FAILS=4
  ```

  预期:8 个正例 run 全部 `SDCSHIELD_FAILS=0`、gem5 退出码 0;poison 四档 `SDCSHIELD_FAILS=4`。记录每档 VL 与 checksum。

- [ ] **Step 3: gem5-fi 侧 commit**

  ```bash
  git add workloads/sdcshield/sve_oob_audit_dual.c workloads/sdcshield/Makefile
  git commit -s -m "feat(sve-oob-audit-dual): 四修复四档实证——#312/#313/#314/#315 镜像 kernel"
  git push -u origin feat/sve-oob-audit-dual-2026-10-29
  ```

---

### Task 6: PR + issue 收尾

**Files:**
- Create: GitHub PR(fix/sve-oob-audit-2026-10-29 → main)

- [ ] **Step 1: 创建 PR**

  gh pr create,正文含:4 个 commit 清单、本机验证输出摘要、gem5 12-run 结果表(档位×模式×FAILS)、诚实验证边界(读越界无 ASAN 可观察危害的构造性证明声明)。

- [ ] **Step 2: 各 issue 回填 gem5 证据评论**(若 commit 的 Closes 未随 PR merge 自动关闭,先评论后留待 merge 关闭;不手动 close 未 merge 的 issue——与 #309 不同,这些 issue 的修复未经 merge)。

## Self-Review 结论

- 覆盖:4 个 issue 各有 Task(1-4),gem5 验证(Task 5)覆盖 #312 sve_clz 的 512/1024 档、#314 的唯一触发档 1024-bit、#313/#315 全档计算正确性。无遗漏。
- 占位符:无(全部代码/命令/commit message 具体给出)。
- 类型一致:whilelt_b8/b64 的 (uint64_t)0/(uint64_t)N 形式与仓库既有用法一致;kreg slack 形态、dual kernel argv/poison 形态均有先例引用。
