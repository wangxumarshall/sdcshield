# gem5 SE-mode config for SVE vector-length-specific binaries.
# Usage: gem5.opt se_sve.py <binary> [args...] --vl <quadwords>
#   --vl: SVE vector length in 128-bit quadwords (1=128, 2=256, 4=512).
#
# Sets ArmISA.sve_vl_se so the emulated CPU's runtime vector length matches
# the -msve-vector-bits the binary was compiled with (size-specific SVE code
# requires a matching runtime VL). Structure follows gem5's canonical
# learning_gem5/part1/simple-arm.py with the ArmISA VL parameter added.
import sys

import m5
from m5.objects import (
    ArmISA,
    ArmSystem,
    SEWorkload,
    Process,
    Root,
    System,
    SrcClockDomain,
    VoltageDomain,
)

# parse --vl out of argv
args = sys.argv[1:]
vl_qw = 1
pos = []
i = 0
while i < len(args):
    if args[i] == "--vl":
        vl_qw = int(args[i + 1])
        i += 2
    else:
        pos.append(args[i])
        i += 1

assert pos, "usage: gem5.opt se_sve.py <binary> [args] --vl <quadwords>"
binary = pos[0]

system = System()
system.clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=VoltageDomain(voltage="1V"))
system.mem_mode = "atomic"
system.mem_ranges = [m5.objects.AddrRange("2GiB")]

from m5.objects import AtomicSimpleCPU

cpu = AtomicSimpleCPU()
# Size-specific SVE code: runtime VL must equal the compile-time
# -msve-vector-bits value (in quadwords).
cpu.isa = ArmISA(sve_vl_se=vl_qw)

system.cpu = cpu

# canonical SE plumbing (simple-arm.py): workload from the binary's ELF,
# process with the command line, thread on the cpu, memory via membus.
from m5.objects import SystemXBar, SimpleMemory

system.membus = SystemXBar()
system.system_port = system.membus.cpu_side_ports
cpu.icache_port = system.membus.cpu_side_ports
cpu.dcache_port = system.membus.cpu_side_ports

# Backing memory on the membus.
system.physmem = SimpleMemory(range=system.mem_ranges[0])
system.physmem.port = system.membus.mem_side_ports

system.workload = SEWorkload.init_compatible(binary)

process = Process()
process.cmd = [binary] + pos[1:]
system.cpu.workload = process
system.cpu.createThreads()

# The CPU needs one interrupt controller per thread.
system.cpu.interrupts = [m5.objects.ArmInterrupts()]
system.cpu.interrupts[0].cpu = system.cpu

root = Root(full_system=False, system=system)
m5.instantiate()

exit_event = m5.simulate()
print(f"Simulated exit code: {exit_event.getCode()} @ tick {m5.curTick()} (cause: {exit_event.getCause()})")
sys.exit(exit_event.getCode())
