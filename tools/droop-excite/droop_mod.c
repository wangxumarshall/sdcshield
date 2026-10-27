/*
 * droop_mod.c — 周期级负载调制器（瞬态电压骤降 / di/dt 激发实验）
 *
 * Spec: docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md
 *
 * 原理：在选定核上以指令计数门控交替"最大电流载荷"与"低电流停顿"。
 * 相位以迭代数表达（--on-iters/--off-iters）；--freq-hz/--duty 由启动时独立
 * 校准换算（高频段绝对精度有限——每相位调用/循环开销，摘要如实报告实测频率，
 * 战役驱动用实测值闭环修正 iters）。
 *
 * 载荷技术复用 tests/cpu/arm64/power_virus_dit.cpp：
 *   - dep  = 依赖 NEON FMA 链背靠背 + 高汉明距离操作数交替（最大翻转活动）
 *   - ilp2 = 4 条独立链交织（饱和双 NEON 管的吞吐路径）
 *   - 断电流相 = yield 停顿（stall）或 GPR 依赖乘加链（light，部分深度）
 *   - bit-exact 软件参考校验（fmaf 单舍入与 NEON FMA 一致）
 *
 * 构建（aarch64）：
 *   gcc -O2 -Wall -Wextra -Werror -std=c17 -pthread -lm -o droop_mod droop_mod.c
 * 仅 aarch64（非 aarch64 编译期 #error）；不进 meson，x86 构建面不受影响。
 *
 * 退出码：0 正常；1 用法/内部错误；2 载荷校验失败（疑似 SDC 证据，
 *         触发战役停机取证流程）。
 */
#define _GNU_SOURCE 1

#if !defined(__aarch64__)
#error "droop_mod.c is aarch64-only (NEON gate loops); not part of the meson build"
#endif

#include <arm_neon.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------------- 计时（EL0 恒可读，无系统调用） ---------------- */
static inline uint64_t cntvct(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

static inline uint32_t cntfrq_hz(void)
{
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return (uint32_t)f;   /* CNTFRQ_EL0 是 64 位寄存器：先按全宽读入再截断，
                             避免 32 位 asm 操作数宽度不匹配（clang
                             -Wasm-operand-widths 诊断；CNTFRQ 架构上 < 2^32） */
}

static inline double ticks_to_secs(uint64_t ticks)
{
    return (double)ticks / (double)cntfrq_hz();
}

/* ---------------- 配置 ---------------- */
enum payload_kind { PAYLOAD_DEP = 0, PAYLOAD_ILP2 = 1 };
enum off_kind { OFF_STALL = 0, OFF_LIGHT = 1 };

/* 校验仅在小相位启用：低频档单相位可达 1e9+ 次迭代，软件参考会拖垮门控
 * 节奏（低频段 oracle 是并行的 eigen 用例，不缺这一层）。 */
#define CHECKSUM_MAX_ITERS 1000000ULL
/* 启动校准样本量（各模式 ~5-30ms，CNTVCT 精度足够） */
#define CAL_N 10000000ULL

struct config {
    uint64_t on_iters;
    uint64_t off_iters;
    double freq_hz;              /* >0：由 freq/duty 计算 iters */
    double duty;                 /* (0,1]；1.0 → off_iters=0 */
    enum payload_kind payload;
    enum off_kind off_mode;
    uint64_t resync;             /* 每 N 门控周期重同步（0=关） */
    double duration;             /* 秒 */
    cpu_set_t cpus;
    int nthreads;                /* 0 = cores 计数 */
    int self_test;
    int calibrate_only;
    const char *json_summary_path;
    /* 运行期派生（不来自命令行） */
    double core_mhz;
    double ips_on, ips_off;
    double cyc_per_iter_on, cyc_per_iter_off;
};

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_signal = 0;   /* 仅真实 SIGTERM/SIGINT 置位；g_stop
                                               还会被 deadline 逃逸/部分创建失败/
                                               亲和性失败置位，不等于"被信号停止" */

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
    g_signal = 1;
}

/* ---------------- 载荷常量（power_virus_dit 同款高汉明距离操作数对） ---------------- */
#define BURST_A0 1.0f     /* 0x3f800000 */
#define BURST_A1 (-2.0f)  /* 0xc0000000 */
#define BURST_B0 4.0f     /* 0x40800000 */
#define BURST_B1 0.125f   /* 0x3e000000 */

/* 非静态全局：外部可见，编译器不能消掉消费它的载荷循环 */
float32x4_t g_sink_vec;
uint64_t g_sink_u64;

/* dep：依赖 FMA 链 + 操作数交替。iters 向上取整为偶（一对 = 2 FMA）。
 * 每次调用从 0 起链（一个 ON 相位一条链，软件参考只依赖 iters）。 */
static float32x4_t payload_dep(uint64_t iters)
{
    float32x4_t a0 = vdupq_n_f32(BURST_A0);
    float32x4_t a1 = vdupq_n_f32(BURST_A1);
    float32x4_t b0 = vdupq_n_f32(BURST_B0);
    float32x4_t b1 = vdupq_n_f32(BURST_B1);
    float32x4_t acc = vdupq_n_f32(0.0f);
    uint64_t pairs = (iters + 1) / 2;
    for (uint64_t i = 0; i < pairs; ++i) {
        acc = vfmaq_f32(acc, a0, b0);
        acc = vfmaq_f32(acc, a1, b1);
    }
    return acc;
}

/* ilp2：4 条独立链交织（吞吐受限路径）。每次调用从 0 起链。
 * lane0/2 用 (A0,B0)，lane1/3 用 (A1,B1)——交替翻转。 */
static void payload_ilp2(uint64_t iters, float32x4_t out[4])
{
    float32x4_t a0 = vdupq_n_f32(BURST_A0);
    float32x4_t a1 = vdupq_n_f32(BURST_A1);
    float32x4_t b0 = vdupq_n_f32(BURST_B0);
    float32x4_t b1 = vdupq_n_f32(BURST_B1);
    float32x4_t x0 = vdupq_n_f32(0.0f), x1 = vdupq_n_f32(0.0f);
    float32x4_t x2 = vdupq_n_f32(0.0f), x3 = vdupq_n_f32(0.0f);
    for (uint64_t i = 0; i < iters; ++i) {
        x0 = vfmaq_f32(x0, a0, b0);
        x1 = vfmaq_f32(x1, a1, b1);
        x2 = vfmaq_f32(x2, a0, b0);
        x3 = vfmaq_f32(x3, a1, b1);
    }
    out[0] = x0; out[1] = x1; out[2] = x2; out[3] = x3;
}

/* 软件 bit-exact 参考：fmaf 单舍入与 NEON FMA 一致（power_virus_dit 论证） */
static float ref_dep(uint64_t iters)
{
    uint64_t pairs = (iters + 1) / 2;
    float acc = 0.0f;
    for (uint64_t i = 0; i < pairs; ++i) {
        acc = fmaf(BURST_A0, BURST_B0, acc);
        acc = fmaf(BURST_A1, BURST_B1, acc);
    }
    return acc;
}

static float ref_chain(uint64_t steps, float a, float b)
{
    float acc = 0.0f;
    for (uint64_t i = 0; i < steps; ++i)
        acc = fmaf(a, b, acc);
    return acc;
}

/* ---------------- 断电流相 ---------------- */
static void off_stall(uint64_t iters)
{
    for (uint64_t i = 0; i < iters; ++i)
        __asm__ volatile("yield" ::: "memory");
}

static uint64_t off_light(uint64_t iters, uint64_t seed)
{
    /* GPR 依赖乘加链：低但非零的电流（部分深度调制） */
    uint64_t x = seed | 1u;
    for (uint64_t i = 0; i < iters; ++i)
        x = x * 3u + 1u;
    return x;
}

/* ---------------- 校准包装（统一签名；同一函数既校准又门控） ---------------- */
static void cal_dep(uint64_t n)   { g_sink_vec = payload_dep(n); }
static void cal_ilp2(uint64_t n)  { float32x4_t a[4]; payload_ilp2(n, a); g_sink_vec = a[0]; }
static void cal_stall(uint64_t n) { off_stall(n); }
static void cal_light(uint64_t n) { g_sink_u64 ^= off_light(n, g_sink_u64); }

static double calibrate_fn(void (*fn)(uint64_t), double core_mhz, double *cyc_per_iter)
{
    uint64_t t0 = cntvct();
    fn(CAL_N);
    uint64_t t1 = cntvct();
    double secs = ticks_to_secs(t1 - t0);
    double ips = (double)CAL_N / secs;
    *cyc_per_iter = core_mhz * 1e6 / ips;
    return ips;
}

static double read_core_mhz_from_sysfs(void)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq", "r");
    if (!f)
        return 2600.0;
    double khz = 0.0;
    int n = fscanf(f, "%lf", &khz);
    fclose(f);
    return (n == 1 && khz > 0.0) ? khz / 1000.0 : 2600.0;
}

static void startup_calibration(struct config *cfg)
{
    void (*on_fn)(uint64_t) = cfg->payload == PAYLOAD_DEP ? cal_dep : cal_ilp2;
    void (*off_fn)(uint64_t) = cfg->off_mode == OFF_STALL ? cal_stall : cal_light;
    cfg->ips_on = calibrate_fn(on_fn, cfg->core_mhz, &cfg->cyc_per_iter_on);
    cfg->ips_off = calibrate_fn(off_fn, cfg->core_mhz, &cfg->cyc_per_iter_off);
}

/* ---------------- 每线程上下文 ---------------- */
struct thread_ctx {
    int cpu;
    uint64_t periods, checksum_fails;
    int failed_saved;
    uint32_t fail_bits[4];
    uint64_t light_state;
    uint64_t t_start, t_end;
};

/* ---------------- 门控相位 ---------------- */
static struct config *g_cfg;
static float32x4_t g_ref_dep;
static float32x4_t g_ref_ilp2[4];
static int g_checksum_on;

static inline uint32_t bits_of_lane(float32x4_t v, int lane)
{
    /* GCC aarch64 的 vgetq_lane_f32 要求 lane 为常量立即数，运行期下标
     * 需经内存提取（语义等价：取该 lane 浮点的位模式）。 */
    float f[4];
    vst1q_f32(f, v);
    uint32_t u;
    memcpy(&u, &f[lane], sizeof(u));
    return u;
}

static void note_fail(struct thread_ctx *ctx, float32x4_t acc)
{
    ctx->checksum_fails++;
    if (!ctx->failed_saved) {
        ctx->failed_saved = 1;
        for (int l = 0; l < 4; ++l)
            ctx->fail_bits[l] = bits_of_lane(acc, l);
    }
}

static void on_phase(struct thread_ctx *ctx)
{
    if (g_cfg->payload == PAYLOAD_DEP) {
        float32x4_t acc = payload_dep(g_cfg->on_iters);
        if (g_checksum_on) {
            uint32x4_t cmp = vceqq_f32(acc, g_ref_dep);
            uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                          & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
            if (mask == 0)
                note_fail(ctx, acc);
        } else {
            g_sink_vec = acc;
        }
    } else {
        float32x4_t acc[4];
        payload_ilp2(g_cfg->on_iters, acc);
        if (g_checksum_on) {
            for (int j = 0; j < 4; ++j) {
                uint32x4_t cmp = vceqq_f32(acc[j], g_ref_ilp2[j]);
                uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                              & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
                if (mask == 0) {
                    note_fail(ctx, acc[j]);
                    break;
                }
            }
        } else {
            g_sink_vec = acc[0];
        }
    }
}

static void off_phase(struct thread_ctx *ctx)
{
    if (g_cfg->off_iters == 0)
        return;                          /* duty=1.0：连续满载 */
    if (g_cfg->off_mode == OFF_STALL)
        off_stall(g_cfg->off_iters);
    else
        ctx->light_state ^= off_light(g_cfg->off_iters, ctx->light_state);
}

/* ---------------- 门控主循环 ----------------
 * 检查窗口 g_check_every 个周期做一次 stop/时限检查与（可选）重同步；
 * 门控周期内部零系统调用、零计时器读取（高频段的周期完整性优先）。
 * 单线程（nthreads==1）跳过所有 barrier。 */
static uint64_t g_check_every = 1;
static uint64_t g_resync_snapped = 0;
static uint64_t g_deadline_ticks;

static void sbarrier_wait(void);   /* 前向声明：定义在下方多线程同步节 */

static uint64_t gate_loop(struct thread_ctx *ctx)
{
    uint64_t periods = 0, since_resync = 0;
    while (!g_stop) {
        for (uint64_t p = 0; p < g_check_every; ++p) {
            on_phase(ctx);
            off_phase(ctx);
        }
        periods += g_check_every;
        if (cntvct() >= g_deadline_ticks)
            break;
        if (g_resync_snapped) {
            since_resync += g_check_every;
            if (since_resync >= g_resync_snapped) {
                since_resync = 0;
                sbarrier_wait();
            }
        }
    }
    ctx->periods = periods;
    return periods;
}

/* ---------------- 多线程同步 ---------------- */
struct sbarrier {
    uint32_t n;                  /* 参与线程数（>1 才启用） */
    _Atomic uint32_t count;
    _Atomic uint32_t gen;
};
static struct sbarrier g_sbar;
static int g_rt_ok = 1;

/* 票据自旋 barrier（无 futex）。deadline 逃逸时置 g_stop 并返回，避免
 * 兄弟线程悬挂（count 残留一个增量，但 g_stop 已置位，全员随即退出——
 * 有界实验工具的取舍，注释留档）。 */
static void sbarrier_wait(void)
{
    if (g_sbar.n <= 1)
        return;
    uint32_t g = atomic_load_explicit(&g_sbar.gen, memory_order_acquire);
    if (atomic_fetch_add_explicit(&g_sbar.count, 1, memory_order_acq_rel)
            == g_sbar.n - 1) {
        atomic_store_explicit(&g_sbar.count, 0, memory_order_relaxed);
        atomic_store_explicit(&g_sbar.gen, g + 1, memory_order_release);
        return;
    }
    while (atomic_load_explicit(&g_sbar.gen, memory_order_acquire) == g) {
        if (g_stop || cntvct() >= g_deadline_ticks) {
            g_stop = 1;
            return;
        }
        __asm__ volatile("yield");
    }
}

static void try_rt_priority(void)
{
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = 1;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0)
        g_rt_ok = 0;    /* EPERM 等 → 降级普通调度，摘要如实报告 */
}

static void *thread_main(void *arg)
{
    struct thread_ctx *ctx = arg;
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET((unsigned)ctx->cpu, &one);
    if (sched_setaffinity(0, sizeof(one), &one) != 0) {
        fprintf(stderr, "droop_mod: setaffinity cpu%d: %s\n", ctx->cpu,
                strerror(errno));
        g_stop = 1;
        return NULL;
    }
    try_rt_priority();
    sbarrier_wait();   /* 起始对齐（一次性；自旋内含 g_stop/deadline 逃逸，
                          部分线程创建失败时 main 置 g_stop 即可全员释放，无悬挂） */
    ctx->t_start = cntvct();
    gate_loop(ctx);
    ctx->t_end = cntvct();
    return NULL;
}

/* ---------------- 自检 ---------------- */
static int self_test(void)
{
    int fails = 0;
    /* 1) dep 校验通过 */
    {
        uint64_t n = 2000;
        float32x4_t acc = payload_dep(n);
        float32x4_t ref = vdupq_n_f32(ref_dep(n));
        uint32x4_t cmp = vceqq_f32(acc, ref);
        uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                      & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
        if (mask == 0) {
            fprintf(stderr, "self-test FAIL: dep checksum mismatch\n");
            fails++;
        }
    }
    /* 2) 注错必须被抓（防"恒真"校验——power_virus_dit 的 veorq no-op 教训） */
    {
        uint64_t n = 2000;
        float32x4_t wrong = vaddq_f32(payload_dep(n), vdupq_n_f32(1.0f));
        float32x4_t ref = vdupq_n_f32(ref_dep(n));
        uint32x4_t cmp = vceqq_f32(wrong, ref);
        uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                      & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
        if (mask != 0) {
            fprintf(stderr, "self-test FAIL: corrupted acc NOT detected\n");
            fails++;
        }
    }
    /* 3) ilp2 校验通过（lane0/2=(A0,B0)，lane1/3=(A1,B1)） */
    {
        uint64_t n = 2000;
        float32x4_t acc[4];
        payload_ilp2(n, acc);
        float r0 = ref_chain(n, BURST_A0, BURST_B0);
        float r1 = ref_chain(n, BURST_A1, BURST_B1);
        float32x4_t refs[4] = { vdupq_n_f32(r0), vdupq_n_f32(r1),
                                vdupq_n_f32(r0), vdupq_n_f32(r1) };
        for (int j = 0; j < 4; ++j) {
            uint32x4_t cmp = vceqq_f32(acc[j], refs[j]);
            uint32_t mask = vgetq_lane_u32(cmp, 0) & vgetq_lane_u32(cmp, 1)
                          & vgetq_lane_u32(cmp, 2) & vgetq_lane_u32(cmp, 3);
            if (mask == 0) {
                fprintf(stderr, "self-test FAIL: ilp2 lane %d mismatch\n", j);
                fails++;
            }
        }
    }
    /* 4) 校准值有限且正（真实 sysfs/2600MHz） */
    {
        struct config cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.payload = PAYLOAD_DEP;
        cfg.off_mode = OFF_STALL;
        cfg.core_mhz = read_core_mhz_from_sysfs();
        startup_calibration(&cfg);
        if (!(cfg.ips_on > 0.0 && cfg.ips_off > 0.0)
                || !(cfg.cyc_per_iter_on > 0.0 && cfg.cyc_per_iter_off > 0.0)) {
            fprintf(stderr, "self-test FAIL: calibration not positive\n");
            fails++;
        } else {
            printf("self-test calibrate: dep %.3f cyc/iter, stall %.3f cyc/iter"
                   " @ %.0f MHz\n", cfg.cyc_per_iter_on, cfg.cyc_per_iter_off,
                   cfg.core_mhz);
        }
    }
    printf("self-test: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

/* ---------------- CLI ---------------- */
static void usage(FILE *out, const char *prog)
{
    fprintf(out,
        "用法: %s [选项]\n"
        "  (--freq-hz F --duty D | --on-iters N --off-iters M)  门控参数（二选一）\n"
        "  --cores LIST        逗号/区间核列表（默认全部。0103: socketA=0-63 socketB=64-127）\n"
        "  --threads N         线程数（默认 = cores 数）\n"
        "  --duration S        运行秒数（默认 60）\n"
        "  --payload dep|ilp2  载荷（默认 dep）\n"
        "  --off-mode stall|light  断电流模式（默认 stall）\n"
        "  --resync N          每 N 门控周期自旋重同步（0=关，默认 1024）\n"
        "  --core-mhz M        核时钟 MHz（默认读 policy0/scaling_cur_freq）\n"
        "  --json-summary P    摘要 JSON 文件路径（也总是打到 stdout）\n"
        "  --calibrate         只校准并打印 JSON，不门控\n"
        "  --self-test         功能自检\n"
        "退出码: 0 正常 | 1 用法/内部错误 | 2 载荷校验失败（疑似 SDC）\n",
        prog);
}

static const char *need_val(int argc, char **argv, int *i)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "missing value for %s\n", argv[*i]);
        usage(stderr, argv[0]);
        exit(1);
    }
    ++*i;
    return argv[*i];
}

static int parse_cpu_list(const char *s, cpu_set_t *set)
{
    CPU_ZERO(set);
    char *buf = strdup(s);
    if (!buf)
        return -1;
    int rc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        unsigned long a, b;
        if (sscanf(tok, "%lu-%lu", &a, &b) == 2) {
            for (unsigned long c = a; c <= b; ++c) {
                if (c >= (unsigned long)CPU_SETSIZE) { rc = -1; break; }
                CPU_SET((unsigned)c, set);
            }
        } else if (sscanf(tok, "%lu", &a) == 1) {
            if (a >= (unsigned long)CPU_SETSIZE)
                rc = -1;
            else
                CPU_SET((unsigned)a, set);
        } else {
            rc = -1;
        }
        if (rc)
            break;
    }
    free(buf);
    return rc;
}

int main(int argc, char **argv)
{
    struct config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.payload = PAYLOAD_DEP;
    cfg.off_mode = OFF_STALL;
    cfg.resync = 1024;
    cfg.duration = 60.0;
    cfg.duty = 0.5;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    for (long c = 0; c < ncpu && c < CPU_SETSIZE; ++c)
        CPU_SET((unsigned)c, &cfg.cpus);

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--self-test"))
            cfg.self_test = 1;
        else if (!strcmp(a, "--calibrate"))
            cfg.calibrate_only = 1;
        else if (!strcmp(a, "--cores")) {
            if (parse_cpu_list(need_val(argc, argv, &i), &cfg.cpus) != 0) {
                fprintf(stderr, "bad --cores list\n");
                return 1;
            }
        }
        else if (!strcmp(a, "--threads")) cfg.nthreads = atoi(need_val(argc, argv, &i));
        else if (!strcmp(a, "--on-iters")) cfg.on_iters = strtoull(need_val(argc, argv, &i), NULL, 10);
        else if (!strcmp(a, "--off-iters")) cfg.off_iters = strtoull(need_val(argc, argv, &i), NULL, 10);
        else if (!strcmp(a, "--freq-hz")) cfg.freq_hz = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--duty")) cfg.duty = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--payload")) {
            const char *v = need_val(argc, argv, &i);
            if (!strcmp(v, "dep")) cfg.payload = PAYLOAD_DEP;
            else if (!strcmp(v, "ilp2")) cfg.payload = PAYLOAD_ILP2;
            else { fprintf(stderr, "--payload dep|ilp2\n"); return 1; }
        }
        else if (!strcmp(a, "--off-mode")) {
            const char *v = need_val(argc, argv, &i);
            if (!strcmp(v, "stall")) cfg.off_mode = OFF_STALL;
            else if (!strcmp(v, "light")) cfg.off_mode = OFF_LIGHT;
            else { fprintf(stderr, "--off-mode stall|light\n"); return 1; }
        }
        else if (!strcmp(a, "--resync")) cfg.resync = strtoull(need_val(argc, argv, &i), NULL, 10);
        else if (!strcmp(a, "--duration")) cfg.duration = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--core-mhz")) cfg.core_mhz = atof(need_val(argc, argv, &i));
        else if (!strcmp(a, "--json-summary")) cfg.json_summary_path = need_val(argc, argv, &i);
        else {
            fprintf(stderr, "unknown option: %s\n", a);
            usage(stderr, argv[0]);
            return 1;
        }
    }
    if (cfg.duty <= 0.0 || cfg.duty > 1.0) {
        fprintf(stderr, "--duty 必须 (0,1]\n");
        return 1;
    }

    if (cfg.self_test)
        return self_test();

    /* 亲和性先行：校准与门控都发生在目标核上 */
    if (sched_setaffinity(0, sizeof(cfg.cpus), &cfg.cpus) != 0) {
        fprintf(stderr, "droop_mod: sched_setaffinity: %s\n", strerror(errno));
        return 1;
    }
    if (cfg.core_mhz <= 0.0)
        cfg.core_mhz = read_core_mhz_from_sysfs();

    g_cfg = &cfg;
    startup_calibration(&cfg);

    if (cfg.calibrate_only) {
        printf("{\"mode\":\"calibrate\",\"core_mhz\":%.1f,"
               "\"payload\":\"%s\",\"cyc_per_iter_on\":%.4f,"
               "\"off_mode\":\"%s\",\"cyc_per_iter_off\":%.4f}\n",
               cfg.core_mhz, cfg.payload == PAYLOAD_DEP ? "dep" : "ilp2",
               cfg.cyc_per_iter_on,
               cfg.off_mode == OFF_STALL ? "stall" : "light",
               cfg.cyc_per_iter_off);
        return 0;
    }

    if (cfg.freq_hz > 0.0) {
        double period = 1.0 / cfg.freq_hz;
        double on_s = period * cfg.duty;
        double off_s = period * (1.0 - cfg.duty);
        cfg.on_iters = (uint64_t)(on_s * cfg.ips_on);
        cfg.off_iters = (uint64_t)(off_s * cfg.ips_off);
    }
    if (cfg.on_iters == 0 && cfg.off_iters == 0) {
        fprintf(stderr, "需要 --freq-hz/--duty 或 --on-iters/--off-iters\n");
        usage(stderr, argv[0]);
        return 1;
    }

    /* 校验参考缓存（iters 此时定型） */
    g_checksum_on = (cfg.on_iters > 0 && cfg.on_iters <= CHECKSUM_MAX_ITERS);
    if (g_checksum_on) {
        g_ref_dep = vdupq_n_f32(ref_dep(cfg.on_iters));
        float r0 = ref_chain(cfg.on_iters, BURST_A0, BURST_B0);
        float r1 = ref_chain(cfg.on_iters, BURST_A1, BURST_B1);
        g_ref_ilp2[0] = vdupq_n_f32(r0);
        g_ref_ilp2[1] = vdupq_n_f32(r1);
        g_ref_ilp2[2] = vdupq_n_f32(r0);
        g_ref_ilp2[3] = vdupq_n_f32(r1);
    }

    /* 检查窗口：min(10ms 周期数, 4096)；resync snapped 到窗口整数倍 */
    {
        double period_s = (double)cfg.on_iters / cfg.ips_on
                        + (double)cfg.off_iters / cfg.ips_off;
        double f_est = period_s > 0.0 ? 1.0 / period_s : 1e9;
        uint64_t per_10ms = (uint64_t)(f_est * 0.01);
        g_check_every = per_10ms < 1 ? 1 : (per_10ms > 4096 ? 4096 : per_10ms);
    }
    if (cfg.resync) {
        uint64_t r = ((cfg.resync + g_check_every - 1) / g_check_every)
                     * g_check_every;
        g_resync_snapped = r < 1 ? g_check_every : r;
    }

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    int cores[CPU_SETSIZE], ncores = 0;
    for (int c = 0; c < CPU_SETSIZE; ++c)
        if (CPU_ISSET(c, &cfg.cpus))
            cores[ncores++] = c;
    if (ncores < 1) {
        fprintf(stderr, "no usable cores\n");
        return 1;
    }
    int nthreads = (cfg.nthreads > 0 && cfg.nthreads <= ncores) ? cfg.nthreads
                                                                : ncores;

    static struct thread_ctx ctxs[CPU_SETSIZE];
    pthread_t tids[CPU_SETSIZE];
    memset(ctxs, 0, sizeof(ctxs));
    g_sbar.n = (uint32_t)nthreads;
    atomic_store(&g_sbar.count, 0);
    atomic_store(&g_sbar.gen, 0);

    g_deadline_ticks = cntvct()
                     + (uint64_t)(cfg.duration * (double)cntfrq_hz());

    int created = 0;
    for (int t = 0; t < nthreads; ++t) {
        ctxs[t].cpu = cores[t];
        if (pthread_create(&tids[t], NULL, thread_main, &ctxs[t]) != 0) {
            fprintf(stderr, "pthread_create failed at %d: %s\n", t,
                    strerror(errno));
            g_stop = 1;
            break;
        }
        created++;
    }
    if (created < nthreads)
        g_stop = 1;    /* 部分创建失败：已建线程从起始对齐自旋中逃逸后即退 */
    for (int t = 0; t < created; ++t)
        pthread_join(tids[t], NULL);

    /* 汇总 */
    uint64_t total_periods = 0, total_fails = 0;
    double f_min = 1e30, f_max = 0.0, f_sum = 0.0;
    for (int t = 0; t < created; ++t) {
        total_periods += ctxs[t].periods;
        total_fails += ctxs[t].checksum_fails;
        double secs = ticks_to_secs(ctxs[t].t_end - ctxs[t].t_start);
        double f = secs > 0.0 ? (double)ctxs[t].periods / secs : 0.0;
        f_sum += f;
        if (f < f_min) f_min = f;
        if (f > f_max) f_max = f;
    }
    double f_mean = created > 0 ? f_sum / created : 0.0;
    if (f_min > f_max) f_min = f_max;   /* created==0/空跑保护 */

    char summary[2048];
    int m = snprintf(summary, sizeof(summary),
        "{\"mode\":\"gate\",\"payload\":\"%s\",\"off_mode\":\"%s\",\"threads\":%d,"
        "\"on_iters\":%llu,\"off_iters\":%llu,"
        "\"freq_target_hz\":%.9g,\"duty\":%.4f,"
        "\"freq_measured_hz_mean\":%.9g,\"freq_measured_hz_min\":%.9g,"
        "\"freq_measured_hz_max\":%.9g,"
        "\"duration_s\":%.3f,\"gate_periods_total\":%llu,"
        "\"checksum\":\"%s\",\"checksum_failures\":%llu,"
        "\"rt_priority\":\"%s\",\"stopped_by_signal\":\"%s\","
        "\"cyc_per_iter_on\":%.4f,\"cyc_per_iter_off\":%.4f}\n",
        cfg.payload == PAYLOAD_DEP ? "dep" : "ilp2",
        cfg.off_mode == OFF_STALL ? "stall" : "light",
        created,
        (unsigned long long)cfg.on_iters,
        (unsigned long long)cfg.off_iters,
        cfg.freq_hz, cfg.duty, f_mean, f_min, f_max,
        cfg.duration, (unsigned long long)total_periods,
        g_checksum_on ? "on" : "off",
        (unsigned long long)total_fails,
        g_rt_ok ? "fifo" : "degraded",
        g_signal ? "true" : "false",
        cfg.cyc_per_iter_on, cfg.cyc_per_iter_off);
    (void)m;
    fputs(summary, stdout);
    if (cfg.json_summary_path) {
        FILE *f = fopen(cfg.json_summary_path, "w");
        if (f) {
            fputs(summary, f);
            fclose(f);
        }
    }
    return total_fails ? 2 : (created < nthreads ? 1 : 0);   /* 部分线程创建失败
                                                                 = 内部错误 → 1 */
}
