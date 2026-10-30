# gem5 SE-mode config for SVE vector-length-specific binaries.
# Usage: gem5.opt se_sve.py <binary> [args...] --vl <quadwords>
#   --vl: SVE vector length in 128-bit quadwords (1=128, 2=256, 4=512).
#   REQUIRED: the VL tier is the identity of the verification this config
#   exists for — a missing --vl is an error, never a silent 128-bit default.
#
# Sets ArmISA.sve_vl_se so the emulated CPU's runtime vector length matches
# the -msve-vector-bits the binary was compiled with (size-specific SVE code
# requires a matching runtime VL). Structure follows gem5's canonical
# learning_gem5/part1/simple-arm.py with the ArmISA VL parameter added.
#
# The config provisions several CPUs (all carrying the same VL). Only the
# first thread context is activated at startup (Process::initState activates
# contextIds[0]); the rest stay Halted and serve as the free thread contexts
# that clone(2)/pthread_create consumes — the upstream single-process-SMP
# pattern of configs/deprecated/example/se.py ("workload = multiprocesses[0]"
# for every CPU). Without them, gem5 SE clone returns EAGAIN once the main
# thread occupies the only context (System::Threads::findFree finds nothing).
import sys

import m5
from m5.objects import (
    ArmISA,
    ArmSystem,
    SEWorkload,
    Process,
    Root,
    System,
    SystemXBar,
    SrcClockDomain,
    VoltageDomain,
)

# parse --vl out of argv
args = sys.argv[1:]
vl_qw = 0
vl_given = False
pos = []
i = 0
while i < len(args):
    if args[i] == "--vl":
        vl_qw = int(args[i + 1])
        vl_given = True
        i += 2
    else:
        pos.append(args[i])
        i += 1

assert pos, "usage: gem5.opt se_sve.py <binary> [args] --vl <quadwords>"
# --vl is REQUIRED (PR #307 #5b): this config is a CLAUDE.md rule-2
# verification vehicle and the tier IS the evidence's identity. The old
# silent 128-bit default let a dropped flag produce a mislabeled-tier run
# that still printed "Simulated exit code: 0".
assert vl_given, (
    "--vl <quadwords> is REQUIRED (1=128, 2=256, 4=512 bit): the VL tier is "
    "the identity of the verification; refusing to silently default to 128-bit"
)
binary = pos[0]

# free thread contexts for the workload's clone(2) calls: sdcshield forks a
# child per test and each child spawns per-CPU worker threads, so provision
# headroom (main + child + workers) without paying for idle Halted contexts.
cpu_count = 4

system = System()
system.clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=VoltageDomain(voltage="1V"))
system.mem_mode = "atomic"
system.mem_ranges = [m5.objects.AddrRange("2GiB")]

from m5.objects import AtomicSimpleCPU

system.cpu = [
    AtomicSimpleCPU() for _ in range(cpu_count)
]
for cpu in system.cpu:
    # Size-specific SVE code: runtime VL must equal the compile-time
    # -msve-vector-bits value (in quadwords), on EVERY cpu a thread may land.
    cpu.isa = ArmISA(sve_vl_se=vl_qw)

system.membus = SystemXBar()
system.system_port = system.membus.cpu_side_ports

from m5.objects import ArmInterrupts, SimpleMemory

for cpu in system.cpu:
    cpu.icache_port = system.membus.cpu_side_ports
    cpu.dcache_port = system.membus.cpu_side_ports
    # The CPU needs one interrupt controller per thread.
    cpu.interrupts = [ArmInterrupts()]
    cpu.interrupts[0].cpu = cpu

# Backing memory on the membus.
system.physmem = SimpleMemory(range=system.mem_ranges[0])
system.physmem.port = system.membus.mem_side_ports

system.workload = SEWorkload.init_compatible(binary)

process = Process()
process.cmd = [binary] + pos[1:]
for cpu in system.cpu:
    # same Process on every CPU: only contextIds[0] starts ticking, the
    # others are the free pool clone(2) draws from (se.py SMP pattern)
    cpu.workload = process
for cpu in system.cpu:
    cpu.createThreads()

root = Root(full_system=False, system=system)
m5.instantiate()

exit_event = m5.simulate()
print(f"Simulated exit code: {exit_event.getCode()} @ tick {m5.curTick()} (cause: {exit_event.getCause()})")
sys.exit(exit_event.getCode())
