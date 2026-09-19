# ipsec 访存路径覆盖：DATA_SIZE knob 化（46 用例 1024B → 1KB..64MB）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ipsec 46 用例的 `DATA_SIZE=1024`（编译期常量，工作集 ~4KB，纯 L1 驻留）改为
**test knob**（`-O <testid>.datasize=N`，1024..64MB，默认 1024 行为零变化），使 AES/3DES/SHA
数据路径覆盖 L1/L2/L3/DRAM 全谱，并顺带解锁 AES 尾部块路径（1024%16=0 → 整块，改尺寸后
非 16 倍数走尾部处理）。

**依据**（docs/research/third-party-sdc-param-critique.md §二.1 + 本会话实测）：
- 46 文件逐一验证：`DATA_SIZE=1024u` 无一例外；工作集 4KB → L1（64KB）驻留，
  L2/L3/DRAM 访存路径、TLB、预取器零覆盖。
- 1024 % 16 = 0：**AES 尾块路径从未执行**。
- 实测（本机 vendored OpenSSL 3.5.0）：AES-CBC 吞吐恒定 ~1.1 GB/s，
  16MB 单次 14.4ms——TEST_LOOP(256) 粒度可承受，**desired_duration 无需随 knob 调整**
  （1s 预算下 16MB 档每轮 body ~43ms×256≈11s 会超预算少许，fracture 轮自然适配，
  无误报风险：desired_duration=0 走 1s 默认，时长校验 ±25% 带内以 fracture 轮补足）。
- 46 文件结构高度统一（本会话逐一验证）：`plaintext[DATA_SIZE]` + `golden_ciphertext[DATA_SIZE]`
  静态数组 ×46、3 个 wrapper 函数（encrypt/decrypt/mac，签名含 len）×43（GCM 3 个变体
  签名多一个 tag 参数）、run 体 5 处 DATA_SIZE 引用。

**Architecture:** 每文件机械变换 8 处（保持文件独立性，不引入共享头——46 份拷贝的既有
结构不动，只把尺寸从编译期搬到运行期）：

1. `#define DATA_SIZE (1024u)` → `#define DATA_SIZE_DEFAULT (1024u)` + init 读 knob
   `datasize`（1024..64MB，须为 16 的倍数保持无填充语义——CBC/CTR 都要求，
   越界/非 16 倍数 fail-loudly 报错并提示合法域）
2. struct 的 `plaintext[DATA_SIZE]`/`golden_ciphertext[DATA_SIZE]` → 指针
   `uint8_t *plaintext; uint8_t *golden_ciphertext;` + struct 增 `size_t datasize;`
3. init：knob 校验 → `d->datasize = N` → `malloc` 两个数组 → memset_random(明文)
4. golden 计算 3 处 `DATA_SIZE` → `d->datasize`
5. run：malloc 3 缓冲用 `d->datasize`；调用/memcmp 5 处同步
6. cleanup：free 两个数组

**Tech Stack:** `get_testspecific_knob_value_int`（framework/test_knobs.h:31，C++ 下直接可用）。

**Spec:** 参数审查报告 §四.2（ipsec 模板化+datasize knob 是第 2 优先建议）；
TEST_LOOP 语义（sandstone.h:140——N 是时间检查粒度非总量）；AES 吞吐实测（本计划上方）。

## Global Constraints

- **默认行为零变化**：不传 knob 时 DATA_SIZE=1024，所有输出与历史逐字节一致
  （LTS 15 镜像矩阵不破坏——复现性论点：默认保守 + knob 扫参是压力策略）。
- **one-patch-per-unit**：46 文件同属一个单元（同一机械变换，拆开无独立价值），
  但为可审查性分 2 个 commit：Task 1 = 3 个代表文件（cbc/ctr/gcm 各一）落地+验证模板；
  Task 2 = 其余 43 文件批量套用同一变换+全量回归。计划文件随 Task 1。
- **16 的倍数约束**：`datasize % 16 != 0` → fail-loudly（CBC/CTR/GCM 无填充语义要求；
  尾块路径由 16 倍数之间的块数差异自然覆盖不了——尾块需要非 16 倍数，但那会改变
  密文长度语义，破坏 golden 结构。**诚实记录**：本 knob 不解锁尾块路径，
  只解锁访存谱系；尾块解锁需要独立设计（padding=PKCS7 或显式 partial-block 测试），
  不在本计划范围）。
- **x86-64 零改动**：变换在 `#if SANDSTONE_SSL_BUILD` 内，两 arch 同构适用。
- **验证 100% 真实**：改造后默认档全量 ipsec 46 用例 pass；4KB/16MB 档抽 3 用例 pass；
  越界/非 16 倍数报错信息实测。

---

### Task 1: 模板落地（3 个代表文件：cbc+mac / ctr+mac / gcm）

**Files:**
- Modify: `tests/cpu/ipsec/aes128_cbc/ipsec_aes128_cbc_hmac_sha1_sse.cpp`（cbc+mac 代表）
- Modify: `tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha256_sse.cpp`（ctr 代表）
- Modify: `tests/cpu/ipsec/aes192_gcm/ipsec_aes192_gcm_sse.cpp`（gcm 代表）
- Create: 本计划文件

**Interfaces:**
- Produces（Task 2 依赖的确切变换模板）：
  - knob 读取：`int64_t knob = get_testspecific_knob_value_int(test, "datasize", DATA_SIZE_DEFAULT);`
  - 校验：`knob < 1024 || knob > 64*1024*1024 || (knob % 16) != 0` → `report_fail_msg("datasize knob invalid: %ld (valid 1024..67108864, multiple of 16, default 1024)", (long)knob);`
  - struct：数组→指针 + `size_t datasize;`
  - init/cleanup/run 的 8 处替换（见 Architecture）

- [ ] **Step 1: 逐文件手工改造 3 个代表**（cbc→ctr→gcm，gcm 注意 tag 参数签名）
- [ ] **Step 2: 构建** `ninja -C builddir` 零新警告
- [ ] **Step 3: 默认档回归**：3 用例 `-t 3000 -n 1` pass + `-v` 确认 knob 默认值日志
- [ ] **Step 4: knob 档验证**：3 用例 `datasize=65536`（64KB，L1→L2 边界）与 `datasize=16777216`（16MB，DRAM）pass
- [ ] **Step 5: 越界报错实测**：`datasize=1000`（非16倍数）与 `datasize=1023`（<1024）→ fail-loudly 消息含合法域
- [ ] **Step 6: commit**（3 文件 + 计划文件）

### Task 2: 批量套用（其余 43 文件）

**Files:**
- Modify: 其余 43 个 `tests/cpu/ipsec/**/*.cpp`（同一变换，脚本辅助 + 逐一 diff 核对）

- [ ] **Step 1: 脚本化变换**（python 正则替换 8 处模式 + 每文件 diff 人工核对——46 文件结构已验证统一，但 xcbc/3des 函数名不同，替换按模式不按名字）
- [ ] **Step 2: 构建** 零警告
- [ ] **Step 3: 全量 ipsec 回归**：`-e ipsec_* 全 46`（默认档）pass
- [ ] **Step 4: 抽样 knob 档**：3 个未手改的代表（3des/avx512/xcbc）`datasize=4194304` pass
- [ ] **Step 5: 全量套件回归**：zstd19 + eigen_svd_cdouble pass
- [ ] **Step 6: README knob 表更新**（ipsec 行加入 §Test knob 表）
- [ ] **Step 7: commit + push**

### 验证命令汇总

```console
ninja -C builddir
# 默认档（零变化证明）
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse,ipsec_aes192_ctr_hmac_sha256_sse,ipsec_aes192_gcm_sse -t 3000 -n 1
# knob 档
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse -O ipsec_aes128_cbc_hmac_sha1_sse.datasize=16777216 -t 5000 -n 1
# 越界
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse -O ipsec_aes128_cbc_hmac_sha1_sse.datasize=1000 -t 2000 -n 1   # expect: fail msg with valid domain
# 全量
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse,... -t 3000 -n 1
```
