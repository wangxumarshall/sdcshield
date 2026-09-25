# 决定性实验记录（第 7、8 案内存真值提取）

执行时间：2026-09-03 23:20–23:40 CST
执行方式：单板 172.168.160.42 上 crash 8.0.4-17.oe2403sp3 + 精确匹配 debuginfo
（/usr/lib/debug/usr/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux）
vmcore：/home/sdc/vmcore/127.0.0.1-2026-08-31-00:47:32/vmcore（14.6GB）与
/home/sdc/vmcore/127.0.0.1-2026-09-03-18:25:12/vmcore（73.7GB）

## 第 7 案（08-31 00:47:32，rcu_sched，i=60）

```
crash> sym runqueues
ffffc1a985e596c0 (D) runqueues
crash> px __per_cpu_offset[60]
$1 = 0xffffbe56fa9b6000          ← 内存真值（非零！）
crash> rd -64 __per_cpu_offset 192
ffffc1a9862555d0:  ffffbe56fa1be000 ffffbe56fa1e0000   ← 完美等差数列
                     （base=ffffbe56fa1be000, step=0x22000, 192 项无一损坏）
crash> p nr_cpu_ids
nr_cpu_ids = 192
crash> vtop ffff80008080f6c0     ← 反事实 x27_true = x1 + true_offset[60]
   PTE: e86057ffe04f03  (VALID|SHARED|AF|NG|PXN|UXN|DIRTY)   ← VALID
crash> rd 0xffff80008080f7e0 8   ← x27_true+0x120（即致命指令本应读的字）
ffff80008080f7e0:  0000000000000400 ...                      ← load_avg=1024，健全
```

对比：
- 真值 entry[60] = `ff ff be 56 fa 9b 60 00`
- 实收 x20     = `a0 00 ff ff be 56 fb 25`
- obs[2:6] = `ffff be56` == true[0:4]（2 字节相位右移），obs[6:8]=`fb25` ≠ true[4:6]=`fa9b`，obs[0:2]=`a000` 污染
- 形态：相位撕裂 + 源污染混合（非纯 ROR16）

## 第 8 案（09-03 18:25:12，rcu_sched，i=12）

```
crash> sym runqueues
ffffc9e8a3cd96c0 (D) runqueues
crash> px __per_cpu_offset[12]
$1 = 0xffffb617dc4d6000          ← 内存真值（非零！）
crash> rd -64 __per_cpu_offset 16
ffffc9e8a40d55d0:  ffffb617dc33e000 ffffb617dc360000   ← 完美等差数列
                     （base=ffffb617dc33e000, step=0x22000）
crash> vtop 0xffff8000801af6c0    ← 反事实 x27_true
   PTE: e80037ffe2ef03  (VALID|SHARED|AF|NG|PXN|UXN|DIRTY)   ← VALID
crash> rd 0xffff8000801af7e0 4
ffff8000801af7e0:  00000000000003ff ...                      ← load_avg=1023，健全
```

对比：
- 真值 entry[12] = `ff ff b6 17 dc 4d 60 00`
- 实收 x20      = `00 ff ff b6 17 dd 39 40`
- obs[1:5] = `00ffff b617` ≈ true>>8 前 5 字节（1 字节相位），obs[5:8]=`dd3940` ≠ true 尾部 `dc4d6000`
- 形态：同样是相位撕裂 + 源污染

## 统一结论

1. 两案内存真值均完好（等差数列逐项验证），寄存器收到坏值 → 排除内存/缓存阵列损坏，锁定装载返回通路。
2. 反事实地址均 VALID 且数据健全 → 若收到真值系统不会异常。
3. 腐化形态为"字节相位错位 + 部分字节来自异源"，指向 fill buffer→RF 写回选路（MUX/对齐网络）间歇失效，而非位翻转（无 stuck-at 特征）。
4. 第 8 案于第 7 案取证当天（2026-09-03 18:25）发生，故障持续活跃，风险现实存在。

## 复现命令

```bash
ssh root@172.168.160.42   # 密码 SDC@2026
printf "sym runqueues\npx __per_cpu_offset[60]\nrd -64 __per_cpu_offset 192\np nr_cpu_ids\nvtop 0xffff80008080f6c0\nrd 0xffff80008080f7e0 8\nquit\n" > /tmp/crash_cmd.txt
crash -i /tmp/crash_cmd.txt \
  /usr/lib/debug/usr/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux \
  /home/sdc/vmcore/127.0.0.1-2026-08-31-00:47:32/vmcore
```

## 附录：第 9 开机（当前，2026-09-03 18:43 CST 启动）实时证据

- 当前 dmesg 已有 2 起 spurious WARNING，均 CPU179（19:26:49 FAR=ffff604003ed3d58、22:34:54 FAR=ffffcfd3a750a057）
- CPU179 仍在线（/sys/devices/system/cpu/cpu179/online = 1），风险持续存在
- 该 FAR=ffffcfd3a750a057 是新形态：指向 vmalloc 尾段（ffffcf…模块区）而非 ffff60… 段，与 08-24 案 ffffc360a8593ab0 同族
- 取证时间：2026-09-03 23:45 CST

## 附录 2：故障窗口反汇编（debuginfo vmlinux，objdump -dl，2026-09-03 验证）

```
static inline struct rq *cpu_rq(int cpu) { return cpu_runqueue(per_cpu_offset(cpu)); }
kernel/sched/fair.c:12050 (update_sg_lb_stats: for_each_cpu_and 循环体)
ffff80008013ae10  ldr  x0, [sp]                    ; x0 = env->cpus (cpumask)
ffff80008013ae20  bl   _find_next_and_bit          ; 返回下一个 CPU 号 i
ffff80008013ae24  mov  x25, x0                     ; x25 = i
ffff80008013ae34  ldp  x0, x1, [sp, #8]            ; x0 = &__per_cpu_offset[0]
                                                ; x1 = &runqueues (percpu 静态模板)
kernel/sched/fair.c:12050
ffff80008013ae3c  ldr  x20, [x0, w25, sxtw #3]     ; x20 = __per_cpu_offset[i]  ← 数据腐化注入点
kernel/sched/fair.c:12054
ffff80008013ae44  add  x27, x1, x20                ; x27 = &per_cpu(runqueues, i)
kernel/sched/fair.c:5024 (cpu_rq)
ffff80008013ae48  ldr  x23, [x27, #288]            ; rq->cfs.avg.load_avg        ← +0x140 致命点
```

- `Code:` 字段五指令字 `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` 与本窗口逐字对应
- 寻址模式：`ldr x20, [x0, w25, sxtw #3]` = 符号扩展缩放变址装载（AGU 移位器参与）
- 8 案中 6 案致命点 = `ldr x23,[x27,#288]`（+0x140 偏移），08-24 案 = `ldr x1,[x3]`（其 x3 来自 `ldr x3,[x3,x2]` 变址装载返回值）
- 全部致命链的根 = 一条变址寻址的 LDR 从完好内存读出后寄存器收到腐化数据
