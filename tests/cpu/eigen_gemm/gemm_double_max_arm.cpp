/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_gemm_double_max_arm
 * @parblock
 * ARM64-only maximum-matrix GEMM SDC stress (request 2026-10-09: eigen +
 * SVE, largest matrix parameters, maximum pressure). Eigen dynamic-square
 * GEMM whose dimension is chosen IN THE CHILD test_init (P0-3 fix,
 * 2026-10-28): an explicit knob/env override wins, else the joint solver
 * sdc_joint_dim_slice returns min(n_mem, n_run) — n_mem sizes the
 * per-NUMA-node memory budget against the measured GEMM RSS coefficient
 * (RSS_RATIO_TABLE SDC_RSS_GEMM_F64, 0.0315 KB/n²/thread differential,
 * which covers the per-thread matrices l_lhs/l_rhs/l_prod and the reroll
 * product x as measured, not modelled), n_run caps the full-core single
 * iteration (same-lineage fill + one multiply, probed per tier in the body
 * TU) at a 200 s table-AR budget via the two-point power-law fit — clamped
 * to [1024, 4096]. The preinit no longer sizes the matrix at all: it only
 * writes a desired_duration UPPER bound from the same memory ceiling —
 * or from the explicit knob/env, which the child applies VERBATIM and
 * therefore must scale the duration too (issue #352) — and
 * sizing in the child also fixes exec_each_test children, whose
 * freshly-exec'd image never ran the preinit and previously sized every
 * run at the un-initialised 1024. The old parent-side formula
 * (sqrt(ram_per_worker / (6.0 * 8)) with a 1/cbrt(workers/8) concurrency
 * cap, which held this 127-thread reference host at N=1630 / ~10 GiB) is
 * superseded: the per-node budget divides the same 650‰ ceiling among the
 * node's workers, so the aggregate-footprint protection the concurrency
 * cap used to approximate (N=4096 x 608 workers once wedged init for
 * 17+ min) now falls out of the memory term directly.
 *
 * The working set overflows L2 and L3 and pressures the TLB — the
 * cache-overflow trigger path validated by the sve512_*_svd family —
 * while the loop keeps the all-core FMA datapath saturated for the whole
 * budget.
 *
 * SDC-excitation ingredients (docs/sdc-excite-reproduce/ matrix):
 *  - cache/TLB overflow: N*N*8 B per matrix (tens of GB node-wide);
 *  - access-pattern switching: every kRerollEvery iterations the loop
 *    flips GEMM compute -> random fill (store storm) -> golden recompute
 *    -> memcmp (streaming read) — di/dt and bus-turnaround transients;
 *  - per-thread operand re-rolls (H11'/P14 hardening) defeat error
 *    hiding in cached operands;
 *  - byte-exact memcmp vs the recomputed golden on every iteration.
 *
 * Compiled only on aarch64 in the tests_sve set
 * (-march=armv8.2-a+sve -msve-vector-bits=128), so Eigen's ARM SVE
 * packet backend drives the GEMM kernels. x86-64 untouched (stock
 * gemm_double_dynamic_square stays at M_DIM 256). Non-aarch64 hosts:
 * the file is not even in the build (meson guard).
 *
 * All-VL Phase 2 (design 2026-10-27 §4.7): Eigen's SVE packet backend is
 * compile-time fixed-width (arm_sve_vector_bits(EIGEN_ARM64_SVE_VL));
 * VLA is un-compilable at the C++ language level (sizeless struct
 * members / NumTraits sizeof — twice-confirmed spike). Instead of locking
 * the whole test to 128-bit, the payload is compiled once per tier
 * (128/256/512/1024) under its own Eigen namespace and this TU — kept
 * vector-length agnostic — dispatches on the tier the framework pinned
 * this worker to. test_run re-reads the ACTUAL VL and re-pins as a second
 * check before calling the matching variant: a refused pin skips, a
 * readback mismatch fails closed. Bare runs at a mismatched VL used to
 * overflow SVE packet tails and heap-corrupt the process
 * (free()/munmap_chunk aborts in validate4b; clean under pinning per
 * validate4c3). The joint-solver probe (solve_once) lives in the same
 * per-tier body TUs and executes on the init thread, which inherits the
 * control thread's framework pin — packet width and executing VL always
 * agree (same tier contract as the SVE SVD pair).
 * @endparblock
 */

#include <sandstone.h>
#include "sdc_stress_config.h"

#include <cmath>
#include <cstdint>

#include <arm_sve.h>
#include <device/cpu/arm64/arm64_vl.h>

/* No Eigen types in this TU: the payload lives in the per-tier body TUs
 * (gemm_double_max_arm_body.cpp), compiled once per vector length under
 * their own Eigen namespaces — design §4.7. */

/* Memory-safe dimension bounds: joint-solver lo/hi AND knob clamp.
 * 全部可通过统一配置层覆盖: -O eigen_gemm_double_max_arm.mdim=N /
 * 配置文件 / 环境变量, 见 sdc_stress_config.h。 */
#define MDIM_MIN 1024
#define MDIM_MAX 4096

namespace {

/* Tier recorded at init (from the framework-pinned control thread, whose
 * VL the init aux thread inherits): the joint-solver probe dispatches on
 * it, so the fixed-width packets it executes always match the executing
 * VL. The GEMM payload's matrices are tier-agnostic memory (plain double
 * arrays), so — unlike the SVE SVD pair, where make_data allocates
 * Eigen objects per tier — a worker/run tier divergence would be a
 * sizing-accuracy concern, not a cross-tier reinterpretation hazard;
 * test_run therefore keeps dispatching on the worker's OWN readback
 * (unchanged semantics, no new failure mode). */
static long gemm_tier_bits;

/* Dimension chosen at init (knob/env > joint solver), read by every
 * worker of this child. Replaces the old extern global the body TUs
 * read: dim is now passed per call (T6). */
static int gemm_run_dim;

}

/* One namespace per compiled tier (see gemm_double_max_arm_body.cpp). */
namespace gemm_vl128  { int run_body(struct test *, int, int); int solve_once(int); }
namespace gemm_vl256  { int run_body(struct test *, int, int); int solve_once(int); }
namespace gemm_vl512  { int run_body(struct test *, int, int); int solve_once(int); }
namespace gemm_vl1024 { int run_body(struct test *, int, int); int solve_once(int); }

/* Joint-solver probe dispatch (T6): route the sizing probe into the
 * per-tier namespace recorded above — same pattern as the SVE SVD pair's
 * solve_once_cdouble_sve. */
static int solve_once_gemm(int n)
{
    switch (gemm_tier_bits) {
    case 128:  return gemm_vl128::solve_once(n);
    case 256:  return gemm_vl256::solve_once(n);
    case 512:  return gemm_vl512::solve_once(n);
    case 1024: return gemm_vl1024::solve_once(n);
    default:   return -1;   /* unreachable: tier validated before probing */
    }
}

/*
 * preinit (runs once in the PARENT, before run_one_test computes
 * test_duration() — so writing test->desired_duration here is effective
 * for the very first run, unlike in test_init; same pattern as
 * ist_skip_preinit setting test->minimum_duration).
 *
 * T6 (spec §4.5 q2 invariant, mirror of the P0-1 SVD fix): the dimension
 * itself is no longer sized here — the child's test_init does that per
 * NUMA node with the joint solver, which also fixes exec_each_test
 * children (their freshly-exec'd image never ran this preinit, so the
 * old parent-sized static arrived un-initialised at 1024). The preinit
 * only writes an UPPER bound:
 *
 *   n_hi = sqrt(budget650_kb / (workers × c_GEMM_F64)), clamped to
 *          [MDIM_MIN, MDIM_MAX] — the same four-source memory budget
 *          and RSS-table coefficient the child's n_mem uses, with
 *          workers = the run's actual whole-machine worker count
 *          (sdc_total_threads(), available in preinit: commit_shmem()
 *          runs before test_preinit). The child's n_mem denominator is
 *          the same worker count scaled to its node
 *          (sdc_joint_dim_slice folds workers/total_cpus into
 *          node_cpus), so n_hi tracks it on every host class — not
 *          just the 127-worker reference the value was historically
 *          hard-coded to.
 *   dur  = AR(compute) × 2 × t(n_hi) × 0.95,  t(N) = T1024·(N/1024)³ s
 *          (quiet single-probe power law, two-point differential fit —
 *          see the measurement record below). The explicit knob tier
 *          models dur = ar × 2 × t(knob) × 0.95 with ar = the same
 *          compute-family table value, EXCEPT workers == 1 takes ar = 1
 *          (the table value is a FULL-CORE figure; a lone worker contends
 *          only with itself — PR#384 review, id 4439402781), and ALSO
 *          writes minimum_duration = ar × 2 × t(knob) (no 0.95 margin:
 *          the "at least one full iteration" floor), because the
 *          framework-level -t override replaces desired_duration
 *          unconditionally and would otherwise land the explicit tier on
 *          the 300 s hard grace (PR#384 review, id 4439402765).
 *
 * The AR × 2 mirrors the joint solver's run_budget semantics (fill + one
 * solve per iteration, all-core slowdown factor), so dur models the
 * FULL-CORE single-iteration time at the memory ceiling: child n <= n_hi
 * keeps dur at or above the child's real iteration, which is what the
 * do-while and the derived timeout test_timeout() = 5×duration + 30 s
 * (300 s floor) expect — EXCEPT for an explicit knob/env, which the
 * child applies VERBATIM (dim = knob, no min against n_hi, issue #352
 * mirror of the SVD fix): the duration model must follow the knob too,
 * or any knob above n_hi times out at the 300 s hard grace and the
 * child is SIGQUIT-killed mid-iteration.
 * (Correction of an earlier comment that claimed
 * a host with fewer threads "only shrinks n_hi — the safe direction":
 * that was backwards. A hard-coded 127 makes the child's n_mem
 * denominator SMALLER than n_hi's on sub-127-worker hosts, so child n
 * can EXCEED n_hi by up to sqrt(127/workers) — dur then UNDER-estimates
 * the real iteration (cubic: ~2.8× at 64 workers) and the derived
 * timeout loses its headroom, the UNSAFE direction. Using the actual
 * worker count restores the invariant on every host; the child's own
 * n_run still bounds one iteration at 200 s as a second belt.)
 *
 * T1024 measurement record (2026-10-28, this host, 256-bit tier — the
 * joint solver's own [时间校准] probes on the quiet single-slice -n 1
 * run: 1024→119.2 ms, 2048→1001.2 ms, verification 4096→7772.5 ms.
 * Cubic differential of the probe pair: a = 882 ms / (2048³−1024³) ⇒
 * T1024 = 0.1260 s; the 4096 point back-solves to 0.1214 s (−3.7%,
 * consistency check). Measured exponent k = 3.07, so the cubic model
 * sits ~9% below the measured power at the n_hi ceiling — well inside
 * the ×2 contract margin above): hosts with a different tier or core
 * count deviate, but only in the safe direction above.
 */
#define GEMM_T1024_S 0.1260    /* quiet fill+multiply at N=1024, seconds */
/* PR#384 检视 R3 (意见 4440012669): the compute-family AR table entry
 * understates ceiling-footprint GEMM 3x (effective AR ~15, measured —
 * see the init CAVEAT below); the explicit-tier anti-kill floor (min)
 * scales its full-core AR by this factor. */
#define GEMM_AR_CAVEAT_X 3.0

static int eigen_gemm_double_max_arm_preinit(struct test *test)
{
    /* 静态表 c + 实际全机 worker 数推 n 上界, 再按 AR×2×t(N) 推 duration
     * 上界: child 精算 n 恒 <= 此上界(spec §4.5)。worker 数用
     * sdc_total_threads()(preinit 时 commit_shmem 已初始化, -n/--cpuset
     * 下是请求的 worker 数)而非参考机 127——子进程 n_mem 分母是同一
     * worker 口径(折算到节点), 硬编码 127 在 worker<127 主机上使
     * child n 可超 n_hi 达 √(127/workers), dur 低估真实迭代(不安全向)。 */
    double budget_kb = (double)sdc_budget_bytes(650) / 1024.0;
    /* GEMM_F64 表值引用表函数而非字面量(09c94fc6 遗留项收口): T9 重标定
     * 0.0290→0.0315 后字面量静默陈旧; 引用后 n_hi 随 1/sqrt(c) 收缩 ~4%,
     * 上界性质不变且更紧(child 同表同预算精算, node0-only 时恰等)。 */
    double c_kb = sdc_rss_table_c(SDC_RSS_GEMM_F64);
    int workers = sdc_total_threads();
    if (workers < 1)
        workers = 1;
    int n_hi = (int)std::sqrt(budget_kb / ((double)workers * c_kb));
    if (n_hi > MDIM_MAX) n_hi = MDIM_MAX;
    if (n_hi < MDIM_MIN) n_hi = MDIM_MIN;

    /* 显式 knob/env 档(issue #352, 与 SVD 修复同构): child 对显式 mdim
     * 逐字采用(dim = knob, 不与 n_hi 取 min), duration 模型必须跟着
     * knob 缩放, 否则 knob > n_hi 的显式档落在 300 s 硬宽限被 SIGQUIT
     * 误杀。读取通道经 sdc_knob_or_env_clamped 单一实现, 与 child 恒
     * 相同(PR#384 检视, 意见 4432271425: 此前 preinit+child 四份逐字
     * 拷贝纯靠手工同步, 任一侧单独改动即静默重开误杀窗口)。 */
    int64_t knob = sdc_knob_or_env_clamped(test, "mdim", MDIM_MIN, MDIM_MAX);

    double iter_s;
    if (knob > 0) {
        /* 【PR#384 检视, 意见 4439402781】显式档 workers==1 时 ar 取 1——
         * 与 svd_cdouble_sve(f23e2fd9)同构: AR 表值是全核口径, 单 worker
         * 自争抢为 1(物理定义); 不折算则 -n 1 显式档裸跑 target 被拉到
         * ~ar 倍 t(knob)(GEMM_T1024_S 本身是 quiet -n 1 口径, 本机单节点
         * compute 表值 5.0 → 4.76 次迭代), 标准 -t…-n 1 回归路径的单迭代
         * 语义破坏。auto 分支维持 f6783077 原模型(ar×2×t(n_hi) 不随
         * worker 数折算——与联合求解 per_solve=run_budget/(ar×2) 同一口
         * 径, 全框架统一的拓扑表习语)。 */
        double ar = workers > 1
                ? sdc_ar_factor(SDC_FAM_COMPUTE_BOUND, sdc_numa_probe()) : 1.0;
        iter_s = ar * 2.0 * GEMM_T1024_S * std::pow((double)knob / 1024.0, 3.0);
        /* 【PR#384 检视, 意见 4439402765】显式档兼写 minimum_duration——
         * 与 svd_cdouble_sve(9a82f53b)同构: -t 在框架层无条件覆盖
         * desired_duration(sandstone_run.cpp test_duration), 无 min 夹钳时
         * `-t 2000 -O …mdim=<大>` 的 timeout 落 300s floor, 而全核单迭代
         * = 有效AR×2×t(knob)——本文件下方 CAVEAT 自认 compute 族表值 5.0
         * 在大足迹下实测低估 3×(有效 AR≈15, 甚至 '~600s' host class),
         * mdim=4096 全核单迭代 ≈242s(计入立方模型 -9% 低估 ≈264s), 余量
         * 不足 20%, 表值档 host class 上确定性 SIGQUIT。min=单次全迭代
         * 模型(无 0.95 裕量), -t 小值档 target 被夹钳到单迭代, timeout
         * (5×min+30)随之随 knob 缩放。auto 分支不写: auto 单迭代由
         * run_budget=200s×AR 表值设计收在 300s floor 内(本 host class
         * 内存侧先绑住, n_run 非绑定侧)。
         * 【PR#384 检视 R3, 意见 4440012669】min 的全核口径再乘 CAVEAT
         * 低估系数(GEMM_AR_CAVEAT_X=3×): 仅表值口径的 min 裕量只有
         * 433s/264s≈1.6×(mdim=4096 全核 -t 小值档), 而 GEMM_T1024_S 系
         * 本机 256-bit quiet 定标——VL=128 档实测每迭代慢 1.78×(2026-11-02,
         * -n 1 -O mdim=2048: 每内层迭代 1.757s→3.133s), 与 CAVEAT 有效
         * AR≈15 叠加后 kill 阈值 5×ar_table=25 被 15×1.78=26.7 打穿
         * (计入 T1024 立方模型 -9% 低估更甚), timeout=5×min+30 在该档
         * 贴线/击穿。min 是反 kill 地板, 须以最坏真实单迭代为口径:
         * ×3 后 VL=128 叠加最坏仍有 5×15/(15×1.78)≈2.8× 裕量(计入
         * -9% ≈2.5×)。workers==1 不放大: CAVEAT 3× 是全核争抢效应,
         * 单 worker 自争抢为 1, -n 1 显式档单迭代语义保持(意见
         * 4439402781); desired 维持表值标称口径(时间预算目标, 非反
         * kill 地板)。SVD 侧不加: 其 mem 族表值 25 实测低估仅 1.56×
         * (39), 5×25/(39×1.78)≈1.8× 裕量仍足。【R4, 意见 4441045425
         * 复算】计入 SVD 幂律拟合误差 ±2%(1.02) 精化为 1.77×, 打穿
         * 需 SVD 档因子 ≥125/(39×1.02)≈3.15×; R4 实测 SVD VL=128
         * 档因子 1.064×(2026-11-04, -n 1 -O mdim=2048 单迭代
         * 56.06s→59.66s, SDCSHIELD_ARM64_SPEC_MAX_VL=128 原生降档;
         * GEMM 同法对照 4.18s→5.87s 证降档改变执行)——代入实测因子
         * 裕量 5×25/(39×1.064×1.02)≈3.1×, R3 借用 GEMM 1.78 系保守
         * 上界(AR 与 tier 反相关不叠加, R3 GEMM 实测)。中间 worker 档
         * (1<w<全核) min 随之更保守——二值闸门裁定见意见 4439402800/
         * 4440012653(保守方向, 插值无标定数据反开 kill 窗)。 */
        test->minimum_duration = (int)(iter_s * 1000.0
                * (workers > 1 ? GEMM_AR_CAVEAT_X : 1.0));   /* ms */
    } else {
        double ar = sdc_ar_factor(SDC_FAM_COMPUTE_BOUND, sdc_numa_probe());
        iter_s = ar * 2.0 * GEMM_T1024_S * std::pow((double)n_hi / 1024.0, 3.0);
    }
    /* 【PR#384 检视 R5, 意见 4463650400】显式档生效侧澄清: 本 desired
     * (0.95×iter_s)恒小于上方 min(1.0×iter_s, workers>1 时 3×iter_s
     * ≈ 3.16×desired), 而框架 test_duration 的 min 夹钳最后执行
     * (sandstone_run.cpp:1879-1880)→ 显式档的 target/timeout 恒由
     * min 决定, 本 desired 在显式档是模型记录值(表值标称口径的时间
     * 预算目标, 见意见 4440012669 裁定), 单独调它不改显式档时长(须调
     * min)。auto 档不写 min, desired 才是生效目标——两档共用本行,
     * "-5% margin" 仅对 auto 档是真实生效裕量。 */
    test->desired_duration = (int)(iter_s * 1000.0 * 0.95);   /* ms */
    return EXIT_SUCCESS;
}

/* All-VL Phase 2 P5: framework-layer capability + 128..1024-bit range
 * gate (framework/device/cpu/arm64/arm64_vl.h). The HWCAP/rdvl probe
 * executes no SVE instruction, and this TU has SVE instructions baked
 * in, so a clean skip must be reachable before any of them; the
 * framework's compiler_minimum_device gate already skips non-SVE hosts
 * before init — this keeps a test-specific reason if that ordering ever
 * changes. The GEMM payload itself is VL-agnostic; test_run dispatches to
 * the per-tier variant that matches the worker's pinned VL (design §4.7).
 * T6: after the gate, the child sizes the dimension (knob/env > joint
 * solver) and records the tier the probe must dispatch to. */
static int eigen_gemm_double_max_arm_init(struct test *test)
{
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;

    /* Framework pinned the control thread before this aux thread was
     * created, so `svl` is the tier the workers run at; the joint
     * solver's probe below executes that tier's fixed-width packets on
     * THIS thread (see gemm_tier_bits above for the tier/run contract). */
    switch (svl * 8) {
    case 128:
    case 256:
    case 512:
    case 1024:
        gemm_tier_bits = svl * 8;
        break;
    default:
        log_skip(CpuNotSupportedSkipCategory,
                 "%s: no Eigen SVE variant compiled for the %ld-bit tier",
                 test->id, svl * 8);
        return EXIT_SKIP;
    }

    /* Dimension (T6, spec §4.5): knob > env > joint-solver auto, all in
     * the CHILD so exec_each_test images size themselves. The auto branch
     * probes solve_once on THIS thread (its VL is the tier just recorded)
     * and returns min(n_mem, n_run): per-NUMA-node memory budget against
     * the measured GEMM RSS coefficient, and a full-core single-iteration
     * budget via the power-law fit and the compute-family AR factor.
     * quick mode collapses to MDIM_MIN. run_budget_ms keeps the contract
     * value 200000: on this host class the memory side binds first
     * (n_mem=3364 < n_run~4900-5300), so the time budget is not the
     * sizing constraint. CAVEAT (fix round 1, 2026-10-28): the
     * compute-family AR table entry (5.0; 4.4 measured at MEDIUM
     * footprint) UNDERSTATES ceiling-footprint GEMM — the T6 verification
     * run's full-core single multiply at N=3364 took ~55-60 s against a
     * ~4.0 s quiet model, an effective AR of ~15 (3x the table). A host
     * where the time side binds would size n_run for a 200 s iteration
     * that really takes ~600 s — past the 300 s hard grace (T5's failure
     * class). Flagged as T7/T9 input (footprint-aware AR entry or
     * per-test compensation); no code change in this round. */
    int dim;
    int64_t knob = sdc_knob_or_env_clamped(test, "mdim", MDIM_MIN, MDIM_MAX);
    if (knob > 0) {
        dim = (int)knob;
    } else {
        struct sdc_joint_args ja = {
            .lo = MDIM_MIN, .hi = MDIM_MAX,
            .fam = SDC_RSS_GEMM_F64,    /* 0.0315 KB/n²/thread, measured */
            .bytes_per_elem = 8, .matrices_per_thread = 4, .rss_ratio = 6.0,
                    /* legacy triple (pre-table model), ignored while fam
                     * selects the RSS table */
            .shared_bytes_per_slice = 0,   /* init holds no matrices */
            .mem_fraction_permille = 650,
            .ar_fam = SDC_FAM_COMPUTE_BOUND,
            .run_budget_ms = 200000,       /* 全核单迭代预算 (fill+一次乘法×AR) */
        };
        try {
            dim = sdc_joint_dim_slice(test, "mdim", &ja, solve_once_gemm);
        } catch (...) {
            /* probe allocation failure (transient OOM &c): fall back to
             * the floor — same doctrine as the SVD pair's calibrated path
             * ("上限宁小勿大"): a sizing probe must never terminate the
             * child. */
            log_warning("[联合求解] %s: mdim 探针异常 → 退守下限 %d",
                        test->id, MDIM_MIN);
            dim = MDIM_MIN;
        }
    }
    gemm_run_dim = dim;

    double gb = (double)dim * dim * 8.0 * 4.0 / (1 << 30);
    /* 【PR#384 检视 R5, 意见 4463650411】exec_each_test child 是全新
     * exec 镜像: preinit 只在 parent 跑, 本进程 test 结构两字段恒
     * 静态初值 0——照常打印会被误读为"无下限保护", 而生效的 -t/min
     * 夹钳经 parent 的 test_duration → shmem->current_test_endtime
     * 传入(R3 意见 4440012682 勘误: fork/exec 迭代语义一致)。判据:
     * preinit 成功后 desired_duration 恒 > 0(两分支 iter_s > 0),
     * == 0 即 exec child。 */
    if (test->desired_duration > 0) {
        log_info("N=%d (~%.2f GB per thread; sized in child init: knob/env, "
                 "else 联合求解 min(n_mem, n_run); preinit desired_duration "
                 "%d ms, minimum_duration %d ms (显式档: CAVEAT 加权单迭代"
                 "下限, -t 夹钳); override with -O eigen_gemm_double_max_arm.mdim=N)",
                 dim, gb, test->desired_duration, test->minimum_duration);
    } else {
        log_info("N=%d (~%.2f GB per thread; sized in child init: knob/env, "
                 "else 联合求解 min(n_mem, n_run); exec_each_test child: "
                 "本进程未跑 preinit, desired/minimum_duration 恒 0——生效的 "
                 "-t/min 夹钳经 parent shmem 传入, 非无下限保护; "
                 "override with -O eigen_gemm_double_max_arm.mdim=N)",
                 dim, gb);
    }
    return EXIT_SUCCESS;
}

/* Design §4.7 dispatch. The framework already pinned this worker to the
 * chip's highest supported tier (arm64_spec_prepare.h); we read the ACTUAL
 * value back — never the requested one — and pick the variant compiled for
 * it. Re-pinning here is a deliberate idempotent second check: the body's
 * Eigen packets are only correct at their own width, so a silent downgrade
 * of the framework's pin must be caught, not papered over. */
static int eigen_gemm_double_max_arm_run(struct test *test, int cpu)
{
    const long want_bytes = arm64_sve_vl_bytes();
    if (want_bytes <= 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "%s: SVE disappeared between pin and run", test->id);
        return EXIT_SKIP;
    }
    const long pinned = arm64_sve_pin_vl(want_bytes);
    if (pinned != want_bytes) {
        log_skip(CpuNotSupportedSkipCategory,
                 "%s: cannot pin the worker to the %ld-bit VL (prctl rc=%ld)",
                 test->id, want_bytes * 8, pinned);
        return EXIT_SKIP;
    }
    if (arm64_sve_vl_bytes() != want_bytes) {
        log_error("%s: pinned %ld bytes but read back %ld",
                  test->id, want_bytes, arm64_sve_vl_bytes());
        report_fail_msg("%s: SVE VL readback mismatch after pin", test->id);
    }

    /* Design §4.9: record which instantiation actually ran. */
    log_info("%s: Eigen SVE gemm variant = %ld-bit", test->id, want_bytes * 8);
    switch (want_bytes * 8) {
    case 128:  return gemm_vl128::run_body(test, cpu, gemm_run_dim);
    case 256:  return gemm_vl256::run_body(test, cpu, gemm_run_dim);
    case 512:  return gemm_vl512::run_body(test, cpu, gemm_run_dim);
    case 1024: return gemm_vl1024::run_body(test, cpu, gemm_run_dim);
    default:
        log_skip(CpuNotSupportedSkipCategory,
                 "%s: no Eigen SVE variant compiled for the %ld-bit tier",
                 test->id, want_bytes * 8);
        return EXIT_SKIP;
    }
}

DECLARE_TEST(eigen_gemm_double_max_arm, "Eigen max-matrix GEMM payload (double, dynamic, memory-safe max square, ARM64+SVE packet backend with per-tier 128/256/512/1024-bit variants selected at the framework-pinned VL, dimension sized in child init by the joint memory/time solver)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_preinit = eigen_gemm_double_max_arm_preinit,
  .test_init = eigen_gemm_double_max_arm_init,
  .test_run = eigen_gemm_double_max_arm_run,
  /* desired_duration is written by test_preinit (parent, before the
   * framework computes test_duration for the first run) as an UPPER-BOUND
   * estimate from the memory ceiling + measured GEMM iteration model —
   * or from the explicit knob, which the child applies verbatim
   * (issue #352) — (T6: the child sizes the dimension itself via the
   * joint solver; this only keeps the parent's timing expectation at or
   * above the child's choice, so the derived timeout (5*duration+30s,
   * 300s floor) scales along). On the explicit tier the preinit also
   * writes minimum_duration = the single full-iteration model (PR#384
   * review, id 4439402765): the framework-level -t override replaces
   * desired_duration unconditionally, so without the min clamp
   * `-t 2000 -O …mdim=<big>` lands on the 300 s floor and is
   * SIGQUIT-killed mid-iteration. */
  .desired_duration = 0,
  .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST
