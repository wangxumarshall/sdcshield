# se_sve_min.py — minimal SE config for SVE-only workload runs (issue #484
# harness validation). Minimized from gem5-fi-fuzz configs/se/arm_chaos.py:
# that config's import list includes CHAOSRAS, which the checked-in
# build/ARM/gem5.opt predates; CHAOS attachment is unnecessary here (the
# fault model is injected in-harness, detection-neutral). Board / cache /
# memory / processor / VL plumbing copied verbatim; clk 2GHz = arm_chaos
# default; no CHAOS objects attached.
import argparse
import shlex

from gem5.components.boards.simple_board import SimpleBoard
from gem5.components.cachehierarchies.classic.private_l1_private_l2_cache_hierarchy import (
    PrivateL1PrivateL2CacheHierarchy,
)
from gem5.components.memory import SingleChannelDDR3_1600
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.components.processors.cpu_types import CPUTypes
from gem5.isas import ISA
from gem5.resources.resource import FileResource
from gem5.simulate.simulator import Simulator

p = argparse.ArgumentParser()
p.add_argument("--cmd", required=True)
p.add_argument("--workload_args", default="")
p.add_argument("--sve_vl", type=int, default=4, choices=[1, 2, 4, 8],
               help="ArmISA.sve_vl_se, 128-bit quanta: 1/2/4/8 = 128/256/512/1024-bit")
args = p.parse_args()

cache_hierarchy = PrivateL1PrivateL2CacheHierarchy(
    l1d_size="64KiB", l1i_size="64KiB", l2_size="512KiB",
)
memory = SingleChannelDDR3_1600("1GiB")
processor = SimpleProcessor(cpu_type=CPUTypes.O3, num_cores=1, isa=ISA.ARM)
cpu0 = processor.get_cores()[0].core

board = SimpleBoard(
    clk_freq="2GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

for _isa in cpu0.isa:
    _isa.sve_vl_se = args.sve_vl

board.set_se_binary_workload(
    binary=FileResource(args.cmd, override=True),
    arguments=shlex.split(args.workload_args) if args.workload_args else [],
)

Simulator(board=board).run()
