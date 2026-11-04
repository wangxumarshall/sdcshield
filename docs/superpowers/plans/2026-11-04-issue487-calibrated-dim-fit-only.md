# sdc_calibrated_dim fit-only 入参(联合求解跳过验证档)— 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #487(audit Minor/性能):`sdc_joint_dim_slice`(联合求解)调用
`sdc_calibrated_dim` 只消费 `fit`(幂律系数)却弃用返回值,而该函数无条件执行
「外推后验证探针 + 收缩轮」——该档成本只为被弃用的返回值服务。按审计处方增加
**fit-only 入参**(缺省 `false`)跳过验证档,联合求解调用点切换为 `fit_only=true`。

**Architecture:** 单头文件单函数最小改动:`sdc_calibrated_dim` 加 defaulted
`bool fit_only`(第 10 参;既有 7/8 实参调用方签名与行为均不变);验证探针 +
收缩轮 + 两行验证日志收进 `if (!fit_only)` 块,`else` 分支打 fit-only 日志
(试解两点 + k + 未验证外推值)。**fit 的两点差分只用验证前的 (p1, mid) 探针**
(`fit` 块只读 p1/mid/t1/t2/k,验证探针不写这些量),跳过验证档不改变拟合值
→ 联合求解 `n_run` 逐值不变(实证见下方复现记录)。

**Tech Stack:** C++20 header-only(`framework/sdc_stress_config.h`),meson/ninja。

**Spec:** issue #487 审计意见原文处方——「需要的是 fit-only 入参跳过验证档」;
行号锚点(:960 / :958-962 / :587-598)为审计时版本,当前 main 对应
`sdc_stress_config.h:1130`(联合调用点)与 `:751-777`(验证块),实质一致。

## Global Constraints

- **复现记录(2026-11-04 本机:aarch64 无 SVE 鲲鹏 920,128 CPU / 4 NUMA 节点
  (2 节点有内存→AR 表 comp=4.0 / mem=10.0)/ 30 GiB,build 418e4e09)**:
  - `eigen_gemm_double_dynamic_square -t 2000 -n 1 -o -`:
    `[时间校准] ... 单次求解预算 25000ms (试解 1024→309.5ms, 2048→2448.7ms,
    k=3.00) → 3513 (验证 11201.1ms)`;`[联合求解] ... n_mem=20772 n_run=4445
    AR=4.0 c=32.3 → 4096`;wall 102.59 s;exit: pass。
    **验证探针 11201.1ms 单线程纯浪费**(返回值 3513 被弃用;n_run=4445 由
    fit 算出,与验证档无关)。
  - `eigen_svd_double -t 2000 -n 1 -o -`:
    `[时间校准] ... 单次求解预算 7000ms (试解 256→105.0ms, 512→563.7ms,
    k=3.00) → 823 (验证 1772.4ms)`;`[联合求解] ... n_run=1214 → 1214`;
    wall 13.00 s;exit: pass。**验证探针 1772.4ms 纯浪费**。
- **一个补丁一个单元**:本计划文档为 `docs(plan)` commit;代码修复为独立
  `fix` commit;绝不捆绑。
- **x86-64 非回归(纯检查论证,如实声明)**:`fit_only` 缺省 `false`,x86 直调方
  `tests/cpu/eigen_sparse/eigen_sparse.cpp:194` 与
  `tests/cpu/eigen_svd/sandstone_eigen_common.h:261`(均 7 实参,消费返回值)
  编译期即走原路径,`if (!fit_only)` 恒真,验证档保证逐字不变;联合求解的
  x86 `#else` 分支本就不调 `sdc_calibrated_dim`(忽略 solve_once)。
  `docs/architecture.md` 所述「constructive one-solve ≤ max_single_ms guarantee
  (verification probe + shrink loop) holds on x86-64 unchanged」修后仍成立。
- **docs 同步判定**:非大颗粒度修改——README 不涉验证档 internals;
  `docs/architecture.md` 对联合路径的描述("n_run: probe power-law fit
  (fixed overhead subtracted, 100 ms probe threshold) × AR")修后仍准确;
  `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` 为历史设计
  工件不改。头文件内注释即文档,随补丁同步。

---

### Task 1: fit_only 参数 + 联合求解调用点切换 + 验证 + commit

**Files:**
- Modify: `framework/sdc_stress_config.h:683-689`(函数 doc 注释,fit 出参段后加
  fit_only 契约)
- Modify: `framework/sdc_stress_config.h:695-699`(签名加第 10 参)
- Modify: `framework/sdc_stress_config.h:751-777`(验证块收进 `if (!fit_only)`,
  else 打 fit-only 日志)
- Modify: `framework/sdc_stress_config.h:1117-1141`(联合求解调用点传
  `/*fit_only=*/true` + 注释更新)

**Interfaces:**
- Consumes: 既有 `struct sdc_fit { double a_ms; double p; };`、`probe` lambda、
  联合求解 `per_solve`(=`run_budget_ms/(ar×2)`)。
- Produces: `sdc_calibrated_dim(lo, hi, max_single_ms, test_id, knob, k_floor,
  solve_once, fit = nullptr, fit_only = false)`——第 9/10 参均缺省,既有 7/8
  实参调用方不变;fit_only=true 时返回**未验证外推档**(联合调用方弃用)。

- [ ] **Step 1: 改签名 + doc 注释**——`framework/sdc_stress_config.h` 函数
  doc 注释「拟合出参 fit」段之后、「调用契约」之前插入:

```c
 * fit_only(issue #487, 缺省 false): 跳过外推后的验证档与收缩轮, 返回
 *   未验证的外推档——供只消费 fit 的调用方(联合求解)使用: 其返回值
 *   被弃用, 验证/收缩的全部成本只为被弃用的返回值(2026-11-04 实测
 *   128 CPU 主机 GEMM 族每 init 验证档单解 11.2s 纯浪费)。fit 的两点
 *   差分只用验证前的 (p1, mid) 探针, 跳过验证档不改变拟合值。
```

  签名(`struct sdc_fit *fit = nullptr` 后加一行):

```c
template <typename F>
static inline int sdc_calibrated_dim(int lo, int hi, long max_single_ms,
                                     const char *test_id, const char *knob,
                                     double k_floor, F solve_once,
                                     struct sdc_fit *fit = nullptr,
                                     bool fit_only = false)
```

- [ ] **Step 2: 验证块收进 `if (!fit_only)` + else 日志**——原
  `:751-777` 的块注释尾加 fit_only 说明,语句整体进 `if (!fit_only) { ... }`,
  新增 else 分支(代码见下;`xdim/td/td0/shrinks` 变为块内局部,无未用变量):

```c
    /* 外推后验证探针(2026-10-24 检视 #11): 幂律外推与 k_floor 都是
     * 启发式, 失准时单解可远超预算(检视实证: 大内存主机旧口径外推
     * 落点单解 8.3s >> 2s)。在选定 dim 再实测一次, 超预算按实测幂律
     * 收缩并复验——构造性保证 one solve ≤ max_single_ms; 收缩到 lo
     * 仍超限则如实记录(机器极慢/lo 过大, 后果与显式 knob 同级)。
     * newdim 强制单调递减吸收计时噪声, 轮数上限 4 防抖动。
     * fit_only(issue #487)整档跳过: 该保证守护的是返回值, 而联合
     * 求解弃用返回值只取 fit——跳过后返回未验证外推档。 */
    if (!fit_only) {
        int xdim = dim;
        double td = probe(dim);
        double td0 = td;
        int shrinks = 0;
        while (td > (double)max_single_ms && dim > lo && shrinks < 4) {
            int newdim = (int)((double)dim * std::pow(budget / td, 1.0 / k));
            if (newdim >= dim) newdim = dim - 1;
            if (newdim < lo) newdim = lo;
            dim = newdim;
            ++shrinks;
            td = probe(dim);
        }
        if (td0 > (double)max_single_ms)
            log_warning("[时间校准] %s.%s: 初选 %d 实测 %.1fms 超预算 → 按实测幂律收缩 %d 轮",
                        test_id, knob, xdim, td0, shrinks);
        log_warning("[时间校准] %s.%s: 单次求解预算 %ldms (试解 %d→%.1fms, %d→%.1fms, "
                    "k=%.2f) → %d (验证 %.1fms%s)",
                    test_id, knob, max_single_ms, p1, t1, mid, t2, k, dim, td,
                    td > (double)max_single_ms
                        ? (dim <= lo ? ", lo 档仍超预算" : ", 收缩轮数上限后仍超预算")
                        : "");
    } else {
        log_warning("[时间校准] %s.%s: 试解 %d→%.1fms, %d→%.1fms, k=%.2f → %d "
                    "(fit-only: 跳过验证档, 未验证外推值)",
                    test_id, knob, p1, t1, mid, t2, k, dim);
    }
```

- [ ] **Step 3: 联合求解调用点切换**——`framework/sdc_stress_config.h:1125-1132`
  注释更新 + 调用传 `/*fit_only=*/true`:

```c
        /* 探针上限 = knob hi(不用 n_mem 截断): 差分基线 (p1, mid) 拉得
         * 越开, 固定开销扣除对计时噪声越稳; 探针成本由 100ms 阈值
         * 机制约束(终点档起每档 ≤ ~8×t1), 与 n_mem 无关。返回值弃用
         * (其内向外推是未扣 b 的保守口径), 只取 fit —— fit_only=true
         * 跳过验证档/收缩轮(issue #487: 验证的全部成本只为被弃用的
         * 返回值, 实测 GEMM 族每 init 白烧 ~per_solve/2 单线程时间)。 */
        struct sdc_fit fit;
        sdc_calibrated_dim(a->lo, a->hi, per_solve,
                           test ? test->id : "(global)", knob, 3.0,
                           solve_once, &fit, /*fit_only=*/true);
```

- [ ] **Step 4: 构建** `./scripts/build.sh` → 零新警告(引用真实输出)。

- [ ] **Step 5: 受影响行为(GEMM,对照复现基线)**——
  `./builddir/sdcshield -e eigen_gemm_double_dynamic_square -t 2000 -n 1 -o -`:
  (a) `[时间校准]` 行出现「fit-only: 跳过验证档」且无「验证 Nms」;
  (b) `[联合求解]` n_run ≈ 4445(同一 fit,机器噪声内)、n 仍 4096;
  (c) wall 较基线 102.59 s 降 ~11 s;(d) exit: pass。

- [ ] **Step 6: 受影响行为(SVD 族 + sparse)**——
  `./builddir/sdcshield -e eigen_svd_double -t 2000 -n 1 -o -`(对照基线
  验证 1772.4ms / n_run=1214)与 `-e eigen_sparse -t 2000 -n 1 -o -`:
  fit-only 日志 + n_run 不变 + exit: pass。

- [ ] **Step 7: 回归** `./builddir/sdcshield -e zstd19 -t 2000 -n 1` →
  `exit: pass`(规则 2(c) 标准回归,不受影响的哨兵)。

- [ ] **Step 8: x86-64 非回归(检查论证)**——确认仅上述三处改动、
  `fit_only` 缺省 false、两个 x86 直调方 7 实参原样(引用 grep 输出),
  如实声明「本机无 x86-64 运行环境,x86 非回归以编译期默认参数 + 调用点
  检查论证」。

- [ ] **Step 9: commit -s + push**——`git branch --show-current` 确认在
  fix/issue-487;commit 信息:
  `fix(framework): sdc_calibrated_dim 增加 fit_only 入参——联合求解跳过白烧的验证档 (#487)`
  (DCO Signed-off-by 最后一行);push 到 fix/issue-487(绝不推 main)。
