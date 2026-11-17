#!/usr/bin/env bash
# verify.sh — fp64_rowmajor_ldp_stp 内核模块完整验证流程
#
# 阶段: [1] 用户态参考编译/运行 (原始 ldr q/str q 路径)
#       [2] 内核模块编译 (kbuild, 无报错生成 .ko)
#       [3] insmod 加载 (无内核报错/oops)
#       [4] 计算结果一致性 (内核 vs 用户态参考, 六缓冲 FNV 哈希)
#       [5] rmmod 安全卸载
#       [6] 定时持续压测路径 (sweeps=0 duration_ms, 生产默认; 反 no-op 门禁)
#
# 需 root 运行: sudo bash verify.sh

set -u

SEED=0x12345678DEADBEEF
MOD=fp64_rowmajor_ldp_stp
DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DIR/verify.log"

PASS=0; FAIL=0
log()   { printf '%s\n' "$*" | tee -a "$LOG"; }
stage() { if [ "$1" -eq 0 ]; then log "[PASS] $2"; PASS=$((PASS+1));
          else log "[FAIL] $2"; FAIL=$((FAIL+1)); fi; }

: > "$LOG"
log "================================================================"
log " fp64 row-major ldp/stp 内核模块 — 完整验证流程"
log " kernel=$(uname -r)  arch=$(uname -m)  gcc=$(gcc -dumpversion)"
log " seed=$SEED  time=$(date '+%F %T')  user=$(id -un)"
log "================================================================"

# ---------- [1] 用户态参考 (原始 ldr q / str q 路径) ----------
log ""
log "---- [1/5] 用户态参考实现 (原始 ldr q / str q 路径) ----"
gcc -O2 -Wall -Wextra -Werror -o "$DIR/userspace_ref" "$DIR/userspace_ref.c" >>"$LOG" 2>&1
stage $? "userspace_ref 编译 (gcc -O2 -Wall -Wextra -Werror, 零告警零报错)"
"$DIR/userspace_ref" "$SEED" > "$DIR/ref.out" 2> "$DIR/ref.err"
stage $? "userspace_ref 运行 (exit 0 = 256 sweep 自检通过)"
log "用户态参考输出:"
sed 's/^/    /' "$DIR/ref.out" | tee -a "$LOG"
REF_HASH="$(grep '^HASH ' "$DIR/ref.out")"
[ -n "$REF_HASH" ]; stage $? "获取用户态 HASH 行 (六个缓冲)"

# ---------- [2] 内核模块编译 ----------
log ""
log "---- [2/5] 内核模块编译 (kbuild 外部模块) ----"
make -C "$DIR" clean >>"$LOG" 2>&1
make -C "$DIR" -j"$(nproc)" >>"$LOG" 2>&1
stage $? "内核模块编译 (make, 无报错)"
[ -f "$DIR/$MOD.ko" ]; stage $? "生成 $MOD.ko"
log "模块元数据 (modinfo):"
modinfo "$DIR/$MOD.ko" 2>/dev/null | sed 's/^/    /' | tee -a "$LOG"

# 反汇编验证 (单次 objdump):
#  (a) 数据通路替换: 全局 ldr q / str q 应为 0, ldp/stp 存在;
#  (b) FPSIMD 收容: 整个 TU 已摘掉 -mgeneral-regs-only, 除 fp64_golden_init /
#      fp64_run_sweeps 两个 kernel_neon 保护函数外, 任何函数不得出现 FP/NEON
#      指令 (否则会破坏用户态 FPSIMD 状态)。
OBJDUMP="$(objdump -d "$DIR/$MOD.o" 2>/dev/null)"
QLDR=$(printf '%s\n' "$OBJDUMP" | grep -cE 'ldr[[:space:]]+q[0-9]+')
QSTR=$(printf '%s\n' "$OBJDUMP" | grep -cE 'str[[:space:]]+q[0-9]+')
LDPN=$(printf '%s\n' "$OBJDUMP" | grep -cE 'ldp[[:space:]]+x[0-9]+')
STPN=$(printf '%s\n' "$OBJDUMP" | grep -cE 'stp[[:space:]]+x[0-9]+')
log "反汇编统计: ldr q=$QLDR  str q=$QSTR  ldp x=$LDPN  stp x=$STPN"
[ "$QLDR" -eq 0 ] && [ "$QSTR" -eq 0 ] && [ "$LDPN" -gt 0 ] && [ "$STPN" -gt 0 ]
stage $? "指令替换完整 (数据通路无 ldr q/str q, 已用 ldp/stp)"

# FPSIMD 收容 — 按寄存器类检测而非枚举助记符: 任何触碰 V/D/S/Q/H/B 数据
# 寄存器的指令 (不论 ldp/stp q/d、fmlal/sdot/... 等形态) 都会被捕获; 保护区
# 外出现即破坏用户态 FPSIMD 状态。mrs/msr fpcr/fpsr 只碰控制寄存器、不碰数据
# 寄存器, 正确地不计入。--no-show-raw-insn 去掉十六进制编码列, 避免编码/地址
# 里的 a-f 数字与寄存器名混淆。
OBJDUMP_NORAW="$(objdump -d --no-show-raw-insn "$DIR/$MOD.o" 2>/dev/null)"
BAD_FUNCS="$(printf '%s\n' "$OBJDUMP_NORAW" | awk '
    /^[0-9a-f]+ <[A-Za-z_][A-Za-z0-9_.]*>:/ {
        fn = $2; sub(/^</, "", fn); sub(/>:$/, "", fn); next
    }
    fn != "" && NF >= 2 {
        line = ""
        for (i = 2; i <= NF; i++) line = line (i == 2 ? "" : " ") $i
        sub(/\/\/.*$/, "", line)                              # 去 "// imm" 注释
        gsub(/[0-9a-f]+[[:space:]]*<[^>]*>/, "", line)        # 去分支目标 hex+<sym>
        if (line ~ /[[:space:]][vdqshb][0-9]+/ &&
            fn != "fp64_golden_init" && fn != "fp64_run_sweeps")
            print fn
    }
' | sort -u)"
if [ -z "$BAD_FUNCS" ]; then
    stage 0 "FPSIMD 收容 (FP/NEON 寄存器仅出现在 fp64_golden_init/fp64_run_sweeps 内)"
else
    stage 1 "FPSIMD 收容 — 保护区外出现 FP/NEON 指令的函数: $BAD_FUNCS"
fi

# ---------- [3] insmod 加载 ----------
log ""
log "---- [3/5] insmod 加载 ----"
DMESG_N0=$(dmesg | wc -l)
insmod "$DIR/$MOD.ko" seed="$SEED" sweeps=256 duration_ms=0 >>"$LOG" 2>&1
stage $? "insmod 加载成功 (无报错; 参数 sweeps=256 duration_ms=0 确定性快跑)"
lsmod | grep -q "^$MOD "; stage $? "lsmod 确认模块在内核中"
KLOG="$(dmesg | tail -n +$((DMESG_N0 + 1)))"
log "模块 dmesg 输出:"
printf '%s\n' "$KLOG" | grep "$MOD:" | sed 's/^/    /' | tee -a "$LOG"
printf '%s\n' "$KLOG" | grep -qE 'Oops|BUG:|Call trace|kernel fault'
[ $? -ne 0 ]; stage $? "无内核 Oops/BUG/Call trace"

# ---------- [4] 计算结果一致性 ----------
# 逐位检出 (一级) 由两侧各自的 memcmp(dst, scalar_expected) 保证: 内核侧每核
# PASS、用户侧 exit 0 均意味着 byte-exact 自检通过。这里的 FNV-1a 六缓冲哈希
# 是二级桥 — 仅用于证明「内核 ldp/stp 通路 vs 用户态 ldr/str 通路」跨实现
# 逐位一致 (每缓冲独立 64-bit, 碰撞概率 2^-64)。
log ""
log "---- [4/5] 计算结果一致性 (内核 ldp/stp vs 用户态 ldr/str) ----"
KERN_HASH="$(printf '%s\n' "$KLOG" | grep -o 'HASH srcA=.*' | head -1)"
[ -n "$KERN_HASH" ]; stage $? "获取内核 HASH 行 (六个缓冲)"
if [ "$REF_HASH" = "$KERN_HASH" ]; then
    stage 0 "六缓冲 FNV-1a 哈希一致 (跨实现逐位一致性的二级桥)"
else
    stage 1 "哈希不一致 — 结果比对失败"
    log "  REF : $REF_HASH"
    log "  KERN: $KERN_HASH"
fi
# 内核侧 fp64_run_sweeps 经 for_each_online_cpu 对全网在线 CPU 各打印一行
# PASS, 不受 insmod 进程 affinity/cpuset 限制; 故用 nproc --all (全网在线
# CPU 数) 对齐, 而非 nproc (受 sched_getaffinity/cpuset 限制, 会 false-FAIL)。
NCPU="$(nproc --all)"
NPASS="$(printf '%s\n' "$KLOG" | grep -c "$MOD: PASS")"
NFAIL="$(printf '%s\n' "$KLOG" | grep -c "$MOD: FAIL")"
log "模块自检覆盖: $NPASS PASS / $NFAIL FAIL, 全网在线 CPU=$NCPU"
[ "$NFAIL" -eq 0 ] && [ "$NPASS" -eq "$NCPU" ]
stage $? "模块内自检 (逐位 memcmp): 每个在线 CPU 各 PASS 一次, 无 FAIL"

# ---------- [5] rmmod 卸载 ----------
log ""
log "---- [5/5] rmmod 卸载 ----"
rmmod "$MOD" >>"$LOG" 2>&1
stage $? "rmmod 卸载成功 (无报错)"
lsmod | grep -q "^$MOD "; [ $? -ne 0 ]; stage $? "模块已从内核移除"
printf '%s\n' "$(dmesg | tail -n +$((DMESG_N0 + 1)))" | grep -E 'Oops|BUG:|Call trace' >/dev/null
[ $? -ne 0 ]; stage $? "全程 (加载-运行-卸载) 无内核异常"
dmesg | tail -n +$((DMESG_N0 + 1)) | grep "$MOD: unloaded" >/dev/null
stage $? "卸载日志确认 (fp64_rowmajor_ldp_stp: unloaded)"

# ---------- [6] 定时持续压测路径 (生产默认; 反 no-op 门禁) ----------
# [3/4] 用 sweeps=256 duration_ms=0 确定性快跑; 生产默认是无参数
# (sweeps=0 duration_ms=2000, time_after(jiffies, deadline) 定时终止)。该定时
# 路径若出现「0 次 sweep 假通过」在 [3/4] 抓不到, 故单独加载一次并断言每个
# 在线 CPU 至少跑出 1 个 sweep。
log ""
log "---- [6/6] 定时持续压测路径 (sweeps=0 duration_ms=2000, 生产默认) ----"
DMESG_N1=$(dmesg | wc -l)
insmod "$DIR/$MOD.ko" seed="$SEED" sweeps=0 duration_ms=2000 >>"$LOG" 2>&1
stage $? "insmod 加载成功 (定时路径: sweeps=0 duration_ms=2000)"
KLOG2="$(dmesg | tail -n +$((DMESG_N1 + 1)))"
log "模块 dmesg 输出 (定时路径):"
printf '%s\n' "$KLOG2" | grep "$MOD:" | sed 's/^/    /' | tee -a "$LOG"
printf '%s\n' "$KLOG2" | grep -qE 'Oops|BUG:|Call trace|kernel fault'
[ $? -ne 0 ]; stage $? "无内核 Oops/BUG/Call trace (定时路径)"
NPASS2="$(printf '%s\n' "$KLOG2" | grep -c "$MOD: PASS")"
NFAIL2="$(printf '%s\n' "$KLOG2" | grep -c "$MOD: FAIL")"
MINSWEEP="$(printf '%s\n' "$KLOG2" | grep "$MOD: PASS" | grep -o 'sweeps=[0-9]*' | cut -d= -f2 | sort -n | head -1)"
log "定时路径覆盖: $NPASS2 PASS / $NFAIL2 FAIL, 全网在线 CPU=$NCPU, 最小 sweep 数=${MINSWEEP:-0}"
[ "$NFAIL2" -eq 0 ] && [ "$NPASS2" -eq "$NCPU" ]
stage $? "定时路径: 每个在线 CPU 各 PASS 一次, 无 FAIL"
[ -n "$MINSWEEP" ] && [ "$MINSWEEP" -gt 0 ]
stage $? "定时路径确实执行了 sweep (反 no-op: 最小 sweep 数 > 0)"
rmmod "$MOD" >>"$LOG" 2>&1
stage $? "rmmod 卸载成功 (定时路径)"

log ""
log "================================================================"
log " 验证汇总: PASS=$PASS  FAIL=$FAIL   (详细日志: $LOG)"
log "================================================================"
[ "$FAIL" -eq 0 ]
