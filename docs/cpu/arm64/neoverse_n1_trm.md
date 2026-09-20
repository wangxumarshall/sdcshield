# Arm® Neoverse™ N1 Core Technical Reference Manual

> Source: arm_neoverse_n1_trm_100616_0401_02_en.pdf (Arm Neoverse N1 Core Technical Reference Manual)

> **Scope:** Neoverse N1 *implementation* manual. Generic Armv8-A architecture (instruction semantics, register architecture, exception model, memory model) is defined in the Arm Architecture Reference Manual (DDI 0487, A-profile). This document focuses on N1-specific deltas: reset values, IMPLEMENTATION DEFINED choices, cache/TLB sizes, and build options. Where a section restates architecture, defer to DDI 0487.


# 2. Introduction to the Neoverse™ N1 core

## 2.1 About the core

This chapter provides an overview of the Neoverse™ N1 core and its features.

The Neoverse™ N1 core is a high-performance and low-power Arm product that implements the
Arm®v8‑A architecture.

The Neoverse™ N1 core supports:

- The Arm®v8.2-A extension.

- The RAS extension.

- The Statistical Proﬁling extension.

- The Load acquire (LDAPR) instructions introduced in the Arm®v8.3-A extension

- The Dot Product support instructions introduced in the Arm®v8.4-A extension.

- The traps for EL1 and EL0 cache controls, PSTATE SSBS (Speculative Store Bypass Safe) bit
that supports software mitigation for Spectre Variant 4, and the speculation barriers (CSDB,
SSBB, PSSBB) instructions introduced in the Arm®v8.5‑A extension.

The Neoverse™ N1 core has a Level 1 (L1) memory system and a private, integrated Level 2 (L2)
cache. It also includes a superscalar, variable-length, out-of-order pipeline.

The Neoverse™ N1 core is implemented inside the DynamIQ™ Shared Unit (DSU) cluster. For more
information, see the  Arm® DynamIQ™ Shared Unit Technical Reference Manual.

The following ﬁgure shows an example of a conﬁguration with four Neoverse™ N1 cores.

## 2.2 Features

Figure 2-1: Example Neoverse™ N1 quad-core conﬁguration

Neoverse™ N1 can be implemented as a single Neoverse™ N1 core with the DSU conﬁgured for
direct connect, without the L3 cache, snoop ﬁlter, and Snoop Control Unit (SCU) logic present.

For more information on the DSU direct connect conﬁguration, see the  Arm® DynamIQ™ Shared
Unit Technical Reference Manual.

The Neoverse™ N1 core includes the following features:

Core features

- 48-bit Physical Address (PA).

- A Memory Management Unit (MMU).

- Optional Cryptographic Extension.

- Superscalar, variable-length, out-of-order pipeline.

- Support for Arm TrustZone® technology.

- Support for Page-Based Hardware Attributes (PBHA).

- Reliability, Availability, and Serviceability (RAS) Extension.

- Generic Interrupt Controller (GICv4.1) CPU interface to connect to an external distributor.

- Generic Timers interface supporting 64-bit count input from an external system counter.

- An integrated execution unit that implements the Advanced SIMD and ﬂoating-point
architecture support.

|Cluster<br>Core 0<br>Core 1<br>DSU<br>Core 2<br>Core 3|External memory interface<br>Interrupt interface<br>Power management and<br>clock control<br>DFT<br>CoreSight infrastructure|
|---|---|

## 2.3 Implementation options

- AArch32 Execution state at Exception level EL0 only. AArch64 Execution state at all Exception
levels (EL0 to EL3).

Cache features

- Separate L1 data and instruction caches.

- Private, uniﬁed data and instruction L2 cache.

- L1 and L2 memory protection in the form of Error Correcting Code (ECC) or parity on RAM
instances which aﬀect functionality.

- Conﬁgurable instruction cache hardware coherency.

Debug features

- Armv8.2 debug logic.

- Activity Monitor Unit (AMU).

- Performance Monitoring Unit (PMU).

- Statistical Proﬁling Extension (SPE).

- Optional CoreSight Embedded Logic Analyzer (ELA).

- Embedded Trace Macrocell (ETM) that supports instruction trace only.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

All Neoverse™ N1 cores in the cluster must have the same build-time conﬁguration options.

The following table lists the implementation options for a core.

Table 2-1: Core implementation options

|Feature|Range of options|Notes|
|---|---|---|
|L2 cache size|•<br>256KB<br>•<br>512KB<br>•<br>1024KB|-|
|L2 transaction queue size|•<br>24 entries<br>•<br>36 entries<br>•<br>48 entries|There are two identical L2 banks in the<br>Neoverse™ N1 core that can be conﬁgured<br>with 12, 18, or 24 transaction queue entries<br>per L2 bank.|
|Cryptographic Extension|Can be included or not included.|-|

## 2.4 Supported standards and specifications

2.4 Supported standards and speciﬁcations

The Neoverse™ N1 core implements the Arm®v8‑A architecture and some architecture extensions.
It also supports interconnect, interrupt, timer, debug, and trace architectures.

Table 2-2: Compliance with standards and speciﬁcations

|Feature|Range of options|Notes|
|---|---|---|
|Core bus width|128-bit, 256-bit|This speciﬁes the bus width between the<br>core and the DSU CPU bridge. The legal<br>core bus width and master bus width<br>combinations are:<br>•<br>If the core bus width is 128 bits, the<br>master bus interface can be any of the<br>following options.<br>◦<br>Single 128-bit wide ACE interface.<br>◦<br>Dual 128-bit wide ACE interface.<br>◦<br>Single 128-bit wide CHI interface.<br>◦<br>Single 256-bit wide CHI interface.<br>•<br>If the core bus width is 256 bits, the<br>master bus interface is a single 256-bit<br>wide CHI interface.<br>•<br>If the Neoverse™ N1 core is integrated<br>with DSU conﬁgured without the SCU<br>and L3, both the core bus width and<br>master bus width must be set to 256-<br>bits.|
|Coherent Instruction Cache|Optional support|Support for instruction cache hardware<br>coherency.|
|CoreSight_Embedded Logic Analyzer_ (ELA)|Optional support|Support for integrating CoreSight ELA-500.<br>CoreSight ELA-500 is a separately licensable<br>product.|
|_Page-Based Hardware Attributes_ (PBHA)|Can be included or not included.|Support for PBHA. For more information,<br>see6.7 Page-based hardware attributes on<br>page 53.|
|ELA RAM Address size|See the _ Arm® CoreSight™ ELA-500_<br>_Embedded Logic Analyzer Technical Reference_<br>_Manual_ for the full supported range.|-|

|Architecture specification or standard|Version|Notes|
|---|---|---|
|Arm architecture|Arm®v8‑A|•<br>AArch32 Execution state at Exception level EL0 only. AArch64<br>Execution state at all Exception levels (EL0-EL3).<br>•<br>A64, A32, and T32 instruction sets.|

# 3. Technical overview

## 3.1 Components

This chapter describes the structure of the Neoverse™ N1 core.

In a standalone conﬁguration, there can be up to four Neoverse™ N1 cores and a DynamIQ Shared
Unit (DSU) that connects the cores to an external memory system.

For more information about the DSU, see the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

The main components of the Neoverse™ N1 core are:

- Instruction fetch

- Instruction decode

- Register rename

- Instruction issue

- Execution pipelines

- L1 data memory system

- L2 memory system

The following ﬁgure is an overview of the Neoverse™ N1 core.

Figure 3-1: Neoverse™ N1 core overview

Cluster

DSU

Core 3*

Core 1*

Core 0

Register

Rename

Execution

pipelines
Instruction Issue/

Commit

Instruction

Decode

Load/Store

MMU

Instruction Fetch

L2 memory system

ETM
GIC CPU interface

DSU Asynchronous bridges

DSU SCU and L3*

*Optional

### 3.1.1 Instruction fetch

### 3.1.2 Instruction decode

### 3.1.3 Register rename

There are multiple asynchronous bridges between the Neoverse™ N1 core and
the DSU. Only the coherent interface between the Neoverse™ N1 core and the
DSU can be conﬁgured to run synchronously, however it does not aﬀect the other
interfaces such as debug, trace, and Generic Interrupt Controller (GIC) which are
always asynchronous. For more information on how to set the coherent interface
to run either synchronously or asynchronously, see Conﬁguration Guidelines in the
Arm® DynamIQ™ Shared Unit Conﬁguration and Sign-oﬀ Guide.

Related information
Memory Management Unit on page 46
L1 memory system on page 55
L2 memory system on page 78
Generic Interrupt Controller CPU interface on page 88
Debug on page 312
Performance Monitoring Unit on page 317
Embedded Trace Macrocell on page 329

3.1.1 Instruction fetch

The instruction fetch unit fetches instructions from the L1 instruction cache and delivers the
instruction stream to the instruction decode unit.

The instruction fetch unit includes:

- A 64KB, 4-way, set associative L1 instruction cache with 64-byte cache lines and parity
protection.

- A fully associative L1 instruction TLB with native support for 4KB, 16KB, 64KB, 2MB, and
32MB page sizes.

- A dynamic branch predictor.

- Conﬁgurable support for instruction cache hardware coherency.

3.1.2 Instruction decode

The instruction decode unit supports the A32, T32, and A64 instruction sets. It also supports
Advanced SIMD and ﬂoating-point instructions in each instruction set.

The register rename unit performs register renaming to facilitate out-of-order execution and
dispatches decoded instructions to various issue queues.

### 3.1.4 Instruction issue

### 3.1.5 Execution pipeline

### 3.1.6 L1 data memory system

### 3.1.7 L2 memory system

## 3.2 Interfaces

3.1.4 Instruction issue

The instruction issue unit controls when the decoded instructions are dispatched to the execution
pipelines. It includes issue queues for storing instruction pending dispatch to execution pipelines.

3.1.5 Execution pipeline

The execution pipeline includes:

- Integer execute unit that performs arithmetic and logical data processing operations.

- Vector execute unit that performs Advanced SIMD and ﬂoating-point operations. Optionally, it
can execute the cryptographic instructions.

3.1.6 L1 data memory system

The L1 data memory system executes load and store instructions and encompasses the L1 data
side memory system. It also services memory coherency requests.

The load/store unit includes:

- A 64KB, 4-way, set associative L1 data cache with 64-byte cache lines and ECC protection per
32 bits.

- A fully associative L1 data TLB with native support for 4KB, 16KB, 64KB, 2MB, and 512MB
page sizes.

3.1.7 L2 memory system

The L2 memory system services L1 instruction and data cache misses in the Neoverse™ N1 core.

The L2 memory system includes:

- An 8-way set associative L2 cache with data ECC protection per 64 bits. The L2 cache is
conﬁgurable with sizes of 256KB, 512KB, or 1024KB.

- An interface with the DynamIQ Shared Unit (DSU) conﬁgurable at implementation time for
synchronous or asynchronous operation.

The Neoverse™ N1 core has several interfaces to connect it to a SoC. The DynamIQ Shared Unit
(DSU) manages all interfaces.

For information on the interfaces, see the  Arm® DynamIQ™ Shared Unit Technical Reference Manual.

## 3.3 About system control

## 3.4 About the Generic Timer

3.3 About system control

The System registers control and provide status information for the functions that the core
implements.

The main functions of the System registers are:

- Overall system control and conﬁguration

- Memory Management Unit (MMU) conﬁguration and management

- Cache conﬁguration and management

- System performance monitoring

- Generic Interrupt Controller (GIC) conﬁguration and management

The System registers are accessible in the AArch64 EL0-EL3 and AArch32 EL0 Execution state.
Some of the System registers are accessible through the external debug interface.

The Generic Timer can schedule events and trigger interrupts that are based on an incrementing
counter value. It generates timer events as active-LOW interrupt outputs and event streams.

The Neoverse™ N1 core provides a set of timer registers. The timers are:

- An EL1 Non-secure physical timer

- An EL2 Hypervisor physical timer

- An EL3 Secure physical timer

- A virtual timer

- A Hypervisor virtual timer

The Neoverse™ N1 core does not include the system counter. The system counter resides in the
SoC, and its value is distributed to the core over a 64-bit bus.

For more information on the Generic Timer, see the  Arm® DynamIQ™ Shared Unit Technical
Reference Manual and the Arm® Architecture Reference Manual for A-proﬁle architecture.

# 4. Clocks, resets, and input synchronization

## 4.1 About clocks, resets, and input synchronization

## 4.2 Asynchronous interface

This chapter describes the clocks, resets, and input synchronization of the Neoverse™ N1 core.

4.1 About clocks, resets, and input synchronization

The Neoverse™ N1 core supports hierarchical clock gating.

The Neoverse™ N1 core contains several interfaces that connect to other components in the
system. These interfaces can be in the same clock domain or in other clock domains.

For information about clocks, resets, and input synchronization, see the Arm® DynamIQ™ Shared
Unit Technical Reference Manual.

Your implementation can include an optional asynchronous interface between the core and the
DynamIQ Shared Unit (DSU) top level.

See the  Arm® DynamIQ™ Shared Unit Technical Reference Manual for more information.

# 5. Power management

## 5.1 About power management

## 5.2 Voltage domains

This chapter describes the power domains and the power modes in the Neoverse™ N1 core.

5.1 About power management

The Neoverse™ N1 core provides mechanisms to control both dynamic and static power
dissipation.

Dynamic power management includes the following features:

- Architectural clock gating.

- Per-core Dynamic Voltage and Frequency Scaling (DVFS).

Static power management includes the following features:

- Dynamic retention.

- Powerdown.

Related information
Power domain states for power modes on page 43
Power control on page 39
Power domains on page 36
Core powerup and powerdown sequences on page 44

The Neoverse™ N1 core supports a VCPU voltage domain and a VSYS voltage domain.

The following ﬁgure shows the VCPU and VSYS voltage domains in each Neoverse™ N1 core and in
the DSU. The example shows a conﬁguration with four Neoverse™ N1 cores.

## 5.3 Power domains

Figure 5-1: Neoverse™ N1 voltage domains

Core 0

Core 1
Core 2
Core 3

DSU

VCPU0
VCPU1

VCPU3
VCPU2

VSYS

Asynchronous bridge logic exists between the voltage domains. The Neoverse™ N1 core processing
logic and core clock domain of the asynchronous bridge are in the VCPU voltage domain. The DSU
clock domain of the asynchronous bridge is in the VSYS voltage domain.

You can tie VCPU and VSYS to the same supply if the core is not required to
support Dynamic Voltage and Frequency Scaling (DVFS). If it is in its own power
domain with proper isolation, the core can still be powered down independently.

The Neoverse™ N1 core contains a core power domain (PDCPU) and a core top-level SYS power
domain (PDSYS) where all the Neoverse™ N1 core I/O signals go through.

PDCPU power domain
The PDCPU power domain contains all core processing logic excluding the cluster clock domain
side of the bridge.

PDSYS power domain
The PDSYS power domain contains the cluster clock domain side of the bridge.

There are additional system power domains in the DSU. See the  Arm® DynamIQ™
Shared Unit Technical Reference Manual for information.

The following ﬁgure shows an example of how the voltage and power domains are organized.

Figure 5-2: Neoverse™ N1 core power domain diagram at core processing logic level

Core processing logic

L1

L2

Asynchronous bridge

CPU domain

Asynchronous bridge

SYS domain

VCPU voltage domain

PDCPU

PDSYS

VSYS voltage domain

The following table describes the power domains that the Neoverse™ N1 core supports.

Table 5-1: Power domain description

Clamping cells between power domains are inferred through power intent ﬁles rather than
instantiated in the RTL.

The following ﬁgure shows the power domains in the DSU cluster, where everything in the same
color is part of the same power domain. The number of Neoverse™ N1 cores can vary, and the
number of domains increases based on the number of Neoverse™ N1 cores present. This example
only shows four Neoverse™ N1 cores and the power domains that are associated with them. Other
power domains are required for a DSU cluster and are not shown in this example.

|Power<br>domain|Description|
|---|---|
|PDCPU<n>|The domain includes the Advanced SIMD and ﬂoating-point block, the L1 and L2 TLBs, L1 and L2 cache RAMs, and Debug<br>registers that are associated with the Neoverse™ N1 core.<br><n> is the core number in the range 0-3. The number represents core 0, core 1, core 2, and core 3. If a core is not present,<br>then the corresponding power domain is not present.|
|PDSYS|The domain is the interface between Neoverse™ N1 and the DSU. It contains the cluster clock domain logic of the CPU<br>bridge. The CPU Bridge contains all asynchronous bridges for crossing clock domains, and is split with one half of each bridge<br>in the core clock domain and the other half in the relevant cluster domain. All core I/O signals go through the CPU bridge<br>and the SYS power domain.|

## 5.4 Architectural clock gating modes

### 5.4.1 Core Wait for Interrupt

Figure 5-3: Neoverse™ N1 power domains at core processing logic level

Cluster

PDCPU[0] domain

PDCPU[1] domain

PDCPU[2] domain

PDCPU[3] domain

Core 0

Core 1

Core 2

Core 3

L1
Adv-SIMD/FP
Adv-SIMD/FP
Adv-SIMD/FP
Adv-SIMD/FP

L2

L2

L2

L2

PDSYS

When the Neoverse™ N1 core is in standby mode, it is architecturally clock gated at the top of the
clock tree.

Wait for Interrupt (WFI) and Wait for Event (WFE) are features of Arm®v8‑A architecture that put
the core in a low-power standby mode by architecturally disabling the clock at the top of the clock
tree. The core is fully powered and retains all the state in standby mode.

There is a small dynamic power overhead from the logic that is required to wake up the core from
WFI low-power state. Other than this, the power that is drawn is reduced to static leakage current
only.

WFI uses a locking mechanism, based on events, to put the core in a low-power state by disabling
most of the clocks in the core, while keeping the core powered up.

When the core executes the WFI instruction, the core waits for all instructions in the core, including
explicit memory accesses, to retire before it enters a low-power state. The WFI instruction also
ensures that store instructions have updated the cache or have been issued to the L3 memory
system.

While the core is in WFI low-power state, the clocks in the core are temporarily enabled without
causing the core to exit WFI low-power state when any of the following events are detected:

- An L3 snoop request that must be serviced by the core data caches.

- A cache or TLB maintenance operation that must be serviced by the core L1 instruction cache,
data cache, TLB, or L2 cache.

- An APB access to the debug or trace registers residing in the core power domain.

|Adv-SIMD/FP|L1|
|---|---|

|Adv-SIMD/FP|L1|
|---|---|

|Adv-SIMD/FP|L1|
|---|---|

|Adv-SIMD/FP|L1|
|---|---|

### 5.4.2 Core Wait for Event

- A GIC CPU access through the AXI4 stream channel.

Exit from WFI low-power state occurs when one of the following occurs:

- The core detects one of the WFI wake-up events.

- The core detects a reset.

For more information, see the Arm® Architecture Reference Manual for A-proﬁle architecture.

Wait For Event (WFE) uses a locking mechanism, based on events, to put the core in a low-power
state by disabling most of the clocks in the core, while keeping the core powered up.

When the core executes the WFE instruction, the core waits for all instructions in the core, including
explicit memory accesses, to retire before it enters a low-power state. The WFE instruction also
ensures that store instructions have updated the cache or have been issued to the L3 memory
system.

If the event register is set, execution of WFE does not cause entry into standby state, but clears
the event register.

While the core is in WFE low-power state, the clocks in the core are temporarily enabled without
causing the core to exit WFE low-power state when any of the following events are detected:

- An L3 snoop request that must be serviced by the core data caches.

- A cache or TLB maintenance operation that must be serviced by the core L1 instruction cache,
data cache, TLB, or L2 cache.

- An APB access to the debug or trace registers residing in the core power domain.

- A GIC CPU access through the AXI4 stream channel.

Exit from WFE low-power state occurs when one of the following occurs:

- The core detects one of the WFE wake-up events.

- The EVENTI input signal is asserted.

- The core detects a reset.

For more information, see the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 5.5 Power control

## 5.6 Core power modes

5.5 Power control

All power mode transitions are performed at the request of the power controller, using a P-Channel
interface to communicate with the Neoverse™ N1 core.

There is one P-Channel per core, plus one P-Channel for the cluster. The Neoverse™ N1 core
provides the current requirements on the PACTIVE signals, so that the power controller can make
decisions and request any change with PREQ and PSTATE. The Neoverse™ N1 core then performs
any actions necessary to reach the requested power mode, such as gating clocks, ﬂushing caches,
or disabling coherency, before accepting the request.

If the request is not valid, either because of an incorrect transition or because the status has
changed so that state is no longer appropriate, then the request is denied. The power mode of
each core can be independent of other cores in the cluster, however the cluster power mode is
linked to the mode of the cores.

The following ﬁgure shows the supported modes for each core domain P-Channel, and the legal
transitions between them.

Figure 5-4: Neoverse™ N1 core power domain mode transitions

Core
dynamic
retention

On

Emulated off
Debug
recovery

Off

From any
power mode

The blue modes indicate the modes that the channel can be initialized into.

### 5.6.1 On mode

### 5.6.2 Off mode

### 5.6.3 Emulated off mode

5.6.1 On mode

In this mode, the core is on and fully operational.

The core can be initialized into the On mode. If the core does not use P-Channel, you can tie the
core in the On mode by tying PREQ LOW.

When a transition to the On mode completes, all caches are accessible and coherent. Other than
the normal architectural steps to enable caches, no additional software conﬁguration is required.

When the core domain P-Channel is initialized into the On mode, either as a shortcut for entering
that mode or as a tie-oﬀ for an unused P-Channel, it is an assumed transition from the Oﬀ mode.
This includes an invalidation of any cache RAM within the core domain.

5.6.2 Oﬀ mode

The Neoverse™ N1 core supports a full Shutdown mode where power can be removed completely
and no state is retained.

The shutdown can be for either the whole cluster or just for an individual core, which allows other
cores in the cluster to continue operating.

In this mode, all core processing logic and RAMs are oﬀ. The domain is inoperable and all core
state is lost. The L1 and L2 caches are disabled, ﬂushed and the core is removed from coherency
automatically on transition to Oﬀ mode.

A Cold reset can reset the core in this mode.

The core P-Channel can be initialized into this mode.

An attempted debug access when the core domain is oﬀ returns an error response on the internal
debug interface indicating the core is not available.

5.6.3 Emulated oﬀ mode

In this mode, all Core domain logic and RAMs are kept on. However, core Warm reset can be
asserted externally to emulate a powerdown scenario while keeping core debug state and allowing
debug access.

All Debug registers must retain their mode and be accessible from the external debug interface. All
other functional interfaces behave as if the core was in Oﬀ mode.

### 5.6.4 Core dynamic retention mode

### 5.6.5 Debug recovery mode

5.6.4 Core dynamic retention mode

In this mode, all core processing logic and RAMs are in retention and the core domain is inoperable.
The core can be entered into this power mode when it is in Wait For Interrupt (WFI) or Wait For
Event (WFE) mode.

The Core dynamic retention mode can be enabled and disabled separately for WFI and WFE by
software running on the core. Separate timeout values can be programmed for entry into this mode
from WFI and WFE mode:

- Use the CPUPWRCTLR.WFI_RET_CTRL register bits to program timeout values for entry into
Core dynamic retention mode from WFI mode.

- Use the CPUPWRCTLR.WFE_RET_CTRL register bits to program timeout values for entry into
Core dynamic retention mode from WFE mode.

The clock to the core is automatically gated outside of the domain when the core is in Core
dynamic retention mode and is running synchronously to the cluster. However, if the core is
running asynchronously to the cluster, the system integrator must gate the clock externally
during Core dynamic retention mode. For more information, see the  Arm® DynamIQ™ Shared Unit
Conﬁguration and Sign-oﬀ Guide.

The outputs of the domain must be isolated to prevent buﬀers without power from propagating

UNKNOWN values to any operational parts of the system.

When the core is in Core dynamic retention mode there is support for snoop, GIC, and debug
access, so the core appears as if it were in WFI or WFE mode. When an incoming access occurs,
it stalls, and the On PACTIVE bit is set HIGH. The incoming access proceeds when the domain is
returned to the On mode using P-Channel.

When the incoming access completes, and if the core has not exited WFI or WFE mode, then the
On PACTIVE bit is set LOW after the programmed retention timeout. The power controller can
then request to reenter the Core dynamic retention mode.

The Debug recovery mode assists with debugging external watchdog-triggered reset events.

It allows contents of the core L1 instruction, L1 data and L2 caches that were present before the
reset to be observable after the reset. The contents of the caches are retained and are not altered
on the transition back to the On mode.

By default, the core invalidates its caches when Cold reset (nCPUPORESET) is deasserted. If the P-
Channel is initialized to the Debug recovery mode, and the core is cycled through power-on reset
along with the system power-on reset, then the cache invalidation is disabled. Initializing the P-
Channel to the Debug recovery mode ensures that the cache contents are preserved when the
core is transitioned to the On mode.

## 5.7 Encoding for power modes

## 5.8 Power domain states for power modes

Debug recovery mode also supports preserving the RAS state, in addition to the cache contents. In
this case, a transition to the Debug recovery mode is made from any of the current states. When in
Debug recovery mode, the core is cycled through a Warm reset with the system Warm reset. The
RAS and cache state are preserved when the core is transitioned to the On mode.

This mode is strictly for debug purposes. It must not be used for functional purposes, because the
correct operation of the L1 cache is not guaranteed when entering this mode.

This mode can occur at any time with no guarantee of the state of the core. A
P-Channel request of this type is accepted immediately, therefore its eﬀects on
the core, cluster, or the wider system are UNPREDICTABLE, and a wider system
reset might be required. For example, if there were outstanding memory system
transactions at the time of the reset, then these transactions might complete after
the reset when the core is not expecting them and cause a system deadlock.

5.7 Encoding for power modes

The following table shows the encodings for the supported modes for each core domain P-
Channel.

Table 5-2: Core power modes COREPSTATE encoding

The power domains can be controlled independently to give diﬀerent combinations when powered
up and powered down.

However, only some powered up and powered down domain combinations are valid and
supported.

The PDCPU power domain supports the power states that are described in the following table.

1 It is tied oﬀ to 0 and should be inferred when all other PACTIVE bits are LOW. For more information, see the
AMBA® Low Power Interface Speciﬁcation.

|Power mode|Short name|PACTIVE bit<br>number|PSTATE<br>value|Power mode description|
|---|---|---|---|---|
|Core debug<br>recovery mode|DEBUG_RECOV|-|`0b001010`|Logic is oﬀ (or in reset), RAM state is retained and not invalidated<br>when transitioning to On mode.|
|On mode|ON|8|`0b001000`|All powerup.|
|Core dynamic<br>retention mode|FULL_RET|5|`0b000101`|Logic and RAM state are inoperable but retained.|
|Emulated oﬀ mode|OFF_EMU|1|`0b000001`|On with Warm reset asserted, debug state is retained and accessible.|
|Oﬀ mode|OFF|0 (implicit)1|`0b000000`|All powerdown.|

## 5.9 Core powerup and powerdown sequences

Table 5-3: PDCPU power state description

States that are not shown in the following tables are unsupported and must not
occur.

The following table describes the power modes, and the corresponding power domain states for
individual cores. The power mode of each core is independent of all other cores in the cluster.

Table 5-4: Supported core power domain states

Deviating from the legal power modes can lead to UNPREDICTABLE results. You must comply with
the dynamic power management and powerup and powerdown sequences described in the
following sections.

There are speciﬁc steps that you must perform when taking the Neoverse™ N1 cores in the cluster
in and out of coherence.

Core powerdown
To take a core out of coherence ready for core powerdown, complete the following steps:

1.
Save all architectural states.

2.
Conﬁgure the GIC distributor to disable or reroute interrupts away from this core.

3.
Set the CPUPWRCTLR.CORE_PWRDN_EN bit to 1 to indicate to the power controller that a
powerdown is requested.

4.
Execute an ISB instruction.

5.
Execute a WFI instruction.

|Power state|Description|
|---|---|
|Oﬀ|Core oﬀ. Power to the block is gated.|
|Ret|Core retention. Logic and RAM retention power only.|
|On|Core on. Block is active.|

|Power mode|Power domain state|Description|
|---|---|---|
|Debug recovery|On|Core on|
|On|On|Core on|
|Core dynamic retention|Ret|Core in retention|
|Emulated oﬀ|On|Core on|
|Oﬀ|Oﬀ|Core oﬀ|

## 5.10 Debug over powerdown

All L1 and L2 cache disabling, L1 and L2 cache ﬂushing, and communication with the L3 memory
system is performed in hardware after the WFI is executed, under the direction of the power
controller.

Executing any WFI instruction when the CPUPWRCTLR.CORE_PWRDN_EN bit
is set automatically masks out all interrupts and wake-up events in the core. If
executed when the CPUPWRCTLR.CORE_PWRDN_EN bit is set, the WFI never
wakes up and the core needs to be reset to restart.

For information about cluster powerdown, see the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

Core powerup
To bring a core into coherence after reset, no software steps are required.

Related information
CPUPWRCTLR_EL1, Power Control Register, EL1 on page 156

The Neoverse™ N1 core supports debug over powerdown, which allows a debugger to retain its
connection with the core even when powered down. This enables debug to continue through
powerdown scenarios, rather than having to re-establish a connection each time the core is
powered up.

The debug over powerdown logic is part of the DebugBlock, which is external to the cluster and
can be implemented in a separate power domain. If the DebugBlock is in the same power domain
as the core, then debug over powerdown is not supported.

For more information on the DebugBlock, see the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

# 6. Memory Management Unit

## 6.1 About the MMU

### 6.1.1 Main functions

This chapter describes the Memory Management Unit (MMU) of the Neoverse™ N1 core.

The Memory Management Unit (MMU) is responsible for translating addresses of code and data
Virtual Addresses (VAs) to Physical Addresses (PAs) in the real system. The MMU also controls
memory access permissions, memory ordering, and cache policies for each region of memory.

The three main functions of the MMU are to:

- Control the table walk hardware that accesses translation tables in main memory.

- Translate Virtual Addresses (VAs) to Physical Addresses (PAs).

- Provide ﬁne-grained memory system control through a set of virtual-to-physical address
mappings and memory attributes that are held in translation tables.

Each stage of address translation uses a set of address translations and associated memory
properties that are held in memory mapped tables that are called translation tables. Translation
table entries can be cached into a Translation Lookaside Buﬀer (TLB).

The following table describes the components that are included in the MMU.

Table 6-1: TLBs and TLB caches in the MMU

The TLB entries contain either one or both of a global indicator and an Address Space Identiﬁer
(ASID) to permit context switches without requiring the TLB to be invalidated.

The TLB entries contain a Virtual Machine Identiﬁer (VMID) to permit virtual machine switches by
the hypervisor without requiring the TLB to be invalidated.

|Component|Description|
|---|---|
|Instruction L1 TLB|48 entries, fully associative.|
|Data L1 TLB|48 entries, fully associative.|
|L2 TLB|1280 entries, 5-way set associative.|
|Translation table<br>prefetcher|Detects an access to contiguous translation tables and prefetches the next one. This prefetcher can be disabled in<br>the ECTLR register.|

### 6.1.2 AArch64 behavior

The Neoverse™ N1 core is an Armv8 compliant core that supports execution in AArch64 state.

The following table shows the AArch64 behavior.

Table 6-2: AArch64 behavior

AArch64

Address translation
system

The Armv8 address translation system resembles an extension to the Long descriptor format address translation
system to support the expanded virtual and physical address space.

Translation granule
4KB, 16KB, or 64KB for Armv8 AArch64 Virtual Memory System Architecture (VMSAv8-64)

Using a larger granule size can reduce the maximum required number of levels of address lookup.

8 bits or 16 bits depending on the value of TCR_ELx.AS

Address Space
Identiﬁer (ASID) size

8 bits or 16 bits depending on the value of VTCR_EL2.VS

Virtual Machine
Identiﬁer (VMID) size

Physical Address (PA)
size

Maximum 52 bits

Any conﬁguration of TCR_ELx.IPS over 52 bits is considered as 52 bits. You can enable or disable each stage of
the address translation independently.

Table 6-3: AArch64 behavior

AArch64

Address translation
system

The Armv8 address translation system resembles an extension to the Long descriptor format address translation
system to support the expanded virtual and physical address space.

Translation granule
4KB, 16KB, or 64KB for Armv8 AArch64 Virtual Memory System Architecture (VMSAv8-64)

Using a larger granule size can reduce the maximum required number of levels of address lookup.

8 bits or 16 bits depending on the value of TCR_ELx.AS

Address Space
Identiﬁer (ASID) size

8 bits or 16 bits depending on the value of VTCR_EL2.VS

Virtual Machine
Identiﬁer (VMID) size

Physical Address (PA)
size

Maximum 48 bits

Any conﬁguration of TCR_ELx.IPS over 48 bits is considered as 48 bits. You can enable or disable each stage of
the address translation independently.

The Neoverse™ N1 core also supports the Virtualization Host Extension (VHE), including ASID space
for EL2. When VHE is implemented and enabled, EL2 has the same behavior as EL1.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information on
concatenated translation tables and for address translation formats.

## 6.2 TLB organization

### 6.2.1 Instruction L1 TLB

### 6.2.2 Data L1 TLB

### 6.2.3 L2 TLB

The TLB is a cache of recently executed page translations within the Memory Management Unit
(MMU). The Neoverse™ N1 core implements a two-level TLB structure. The TLB stores all page
sizes and is responsible for breaking these down in to smaller pages when required for the data or
instruction L1 TLB.

6.2.1 Instruction L1 TLB

The instruction L1 TLB is implemented as a 48-entry fully associative structure. This TLB caches
entries at the 4KB, 16KB, 64KB, 2MB, and 32MB granularity of Virtual Address (VA) to Physical
Address (PA) mapping only.

A hit in the instruction L1 TLB provides a single CLK cycle access to the translation, and returns
the PA to the instruction cache for comparison. It also checks the access permissions to signal an
Instruction Abort.

6.2.2 Data L1 TLB

The data L1 TLB is a 48-entry fully associative TLB that is used by load and store operations. The
cache entries have 4KB, 16KB, 64KB, 2MB, and 512MB granularity of Virtual Address (VA) to
Physical Address (PA) mappings only.

A hit in the data L1 TLB provides a single CLK cycle access to the translation, and returns the PA to
the data cache for comparison. It also checks the access permissions to signal a Data Abort.

The L2 TLB structure is shared by instruction and data. It handles misses from the instruction and
data L1 TLBs.

The following table describes the L2 TLB characteristics.

Table 6-4: L2 TLB characteristics

Access to the L2 TLB usually takes three cycles. If a diﬀerent page or block size mapping is used,
then this access can take longer.

|Characteristic|Note|
|---|---|
|5-way, set associative, 1280-entry<br>cache|Stores:<br>•<br>_Virtual Address_ (VA) to_Physical Address_ (PA) mappings for 4KB, 16KB, 64KB, 2MB, 32MB,<br>512MB, and 1GB block sizes.<br>•<br>_Intermediate physical address_ (IPA) to PA mappings for 2MB and 1GB (in a 4KB translation<br>granule), 32MB (in a 16K translation granule), and 512MB (in a 64K granule) block sizes.<br>Only Non-secure EL1 and EL0 stage 2 translations are cached.<br>•<br>Intermediate PAs obtained during a translation table walk.|

## 6.3 TLB match process

The L2 TLB supports four translation table walks in parallel (four TLB misses), and can service two
TLB lookups while the translation table walks are in progress. If there are six successive misses, the
L2 TLB stalls.

Caches in the core are invalidated automatically at reset deassertion unless the core
power mode is initialized to Debug recovery mode. See the  Arm® DynamIQ™ Shared
Unit Technical Reference Manual for more information.

The Armv8-A architecture provides support for multiple maps from the Virtual Address (VA) space
that are translated diﬀerently.

TLB entries store the context information that is required to facilitate a match and avoid the need
for a TLB ﬂush on a context or virtual machine switch.

Each TLB entry contains a:

- VA

- Physical Address (PA)

- Set of memory properties that includes type and access permissions

Each entry is either a global entry, or it is associated with a particular Address Space Identiﬁer (ASID).
In addition, each TLB entry contains a ﬁeld to store the Virtual Machine Identiﬁer (VMID) in the
entry applicable to accesses from Non-secure EL0 and EL1 Exception levels.

Each entry is associated with a particular translation regime:

- EL3 in Secure state in AArch64 state only.

- EL2, EL1, or EL0 in Non-secure state.

- EL1 or EL0 in Secure state.

A TLB match entry occurs when the following conditions are met:

- A VA, moderated by the page size such as the VA bits[48:N], where N is log2 of the block size
for that translation that is stored in the TLB entry, matches the requested address.

- Entry translation regime matches the current translation regime.

- The ASID matches the current ASID held in the CONTEXTIDR, TTBR0, or TTBR1 register, or
the entry is marked global.

- The VMID matches the current VMID held in the VTTBR_EL2 register.

- The ASID and VMID matches are IGNORED when ASID and VMID are not relevant.
ASID is relevant when the translation regime is:

- EL2 in Non-secure state with HCR_EL2.E2H and HCR_EL2.TGE set to 1

## 6.4 Translation table walks

### 6.4.1 AArch64 behavior

- EL1 or EL0 in Secure state

- EL1 or EL0 in Non-secure state

VMID is relevant for EL1 or EL0 in Non-secure state.

When an access is requested at an address, the Memory Management Unit (MMU) searches for the
requested Virtual Address (VA) in the Translation Lookaside Buﬀers (TLBs). If it is not present, then it
is a miss and the translation proceeds by looking up the translation table during a translation table
walk.

When the Neoverse™ N1 core generates a memory access, the following process occurs:

1.
The MMU performs a lookup for the requested VA, current Address Space Identiﬁer (ASID),
current  Virtual Machine Identiﬁer (VMID), and current translation regime in the relevant
instruction or data L1 TLB.

2.
If there is a miss in the relevant L1 TLB, the MMU performs a lookup for the requested VA,
current ASID, current VMID, and translation regime in the L2 TLB.

3.
If there is a miss in the L2 TLB, the MMU performs a hardware translation table walk.

If an L2 TLB miss, the hardware does a translation table walk as long as the MMU is enabled, and
the translation using the base register has not been disabled.

If the translation table walk is disabled for a particular base register, the core returns a translation
fault. If the TLB ﬁnds a matching entry, it uses the information in the entry as follows.

The access permission bits determine whether the access is permitted. If the matching entry does
not pass the permission checks, the MMU signals a Permission fault. See the Arm® Architecture
Reference Manual for A-proﬁle architecture for details of Permission faults.

When executing in AArch64 state at a particular Exception level, you can conﬁgure the hardware
translation table walk to use either the 4KB, 16KB, or 64KB translation granule.

Program the Translation Granule bit, TG0, in the appropriate translation control register:

- TCR_EL1

- TCR_EL2

- TCR_EL3

- VTCR_EL2

## 6.5 MMU memory accesses

### 6.5.1 Configuring MMU accesses

### 6.5.2 Descriptor hardware update

For TCR_EL1, you can program the Translation Granule bits TG0 and TG1 to conﬁgure the
translation granule respectively for TTBR0_EL1 and TTBR1_EL1, or TCR_EL2 when Virtualization
Host Extension (VHE) is enabled.

During a translation table walk, the Memory Management Unit (MMU) generates memory accesses.
The Neoverse™ N1 core has speciﬁc behaviors for MMU memory accesses.

6.5.1 Conﬁguring MMU accesses

By programming the IRGN and ORGN bits, you can conﬁgure the MMU to perform translation
table walks in cacheable or non-cacheable regions:

AArch64
Appropriate TCR_ELx register.

If the encoding of both the ORGN and IRGN bits is Write-Back, the data cache lookup is
performed and data is read from the data cache. External memory is accessed, if the ORGN and
IRGN bit contain diﬀerent attributes, or if the encoding of the ORGN and IRGN bits is Write-
Through or Non-cacheable.

The core supports hardware update in AArch64 state using hardware management of the Access
ﬂag and hardware management of dirty state.

These features are enabled in registers TCR_ELx and VTCR_EL2.

Hardware management of the Access ﬂag is enabled by the following conﬁguration ﬁelds:

- TCR_ELx.HA for stage 1 translations.

- VTCR_EL2.HA for stage 2 translations.

Hardware management of dirty state is enabled by the following conﬁguration ﬁelds:

- TCR_ELx.HD for stage 1 translations.

- VTCR_EL2.HD for stage 2 translations.

Hardware management of dirty state can only be enabled if hardware management
of the Access ﬂag is enabled.

To support the hardware management of dirty state, the DBM ﬁeld is added to the translation table
descriptors as part of Armv8.1 architecture.

## 6.6 Specific behaviors on aborts and memory attributes

### 6.6.1 External aborts

The core supports hardware update only in outer Write-Back and inner Write-Back memory
regions.

If software requests a hardware update in a memory region that is not inner Write-Back or not
outer Write-Back, then the core returns an abort with the following encoding:

- ESR.ELx.DFSC = 0b110001 for Data Aborts in AArch64.

- ESR.ELx.IFSC = 0b110001 for Instruction Aborts in AArch64.

6.6 Speciﬁc behaviors on aborts and memory attributes

This section describes speciﬁc behaviors that are caused by aborts and also describes memory
attributes.

MMU responses
The MMU generates a response to the requester, when one of the following translations is
completed:

- A L1 TLB hit.

- A L2 TLB hit.

- A translation table walk.

The response from the MMU contains the following information:

- The PA corresponding to the translation.

- A set of permissions.

- Secure or Non-secure.

- All the information that is required to report aborts. See the Arm® Architecture Reference Manual
for A-proﬁle architecture for more details.

External aborts are deﬁned as those that occur in the memory system rather than those that the
Memory Management Unit (MMU) detects. Normally, external memory aborts are rare. External
aborts are caused by errors that are ﬂagged to the external interface.

When an external abort to the external interface occurs on an access for a translation table walk
access, the MMU returns a synchronous external abort. For a load multiple or a store multiple
operation, the address that is captured in the fault address register is that of the address that
generated the synchronous external abort.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

### 6.6.2 Mis-programming contiguous hints

### 6.6.3 Memory attributes

## 6.7 Page-based hardware attributes

6.6.2 Mis-programming contiguous hints

In the case of a mis-programming contiguous hint, when there is a descriptor that contains a set
CH bit, all contiguous virtual addresses that are contained in the block must be included in the
input Virtual Address (VA) space that is deﬁned for stage 1 by TxSZ for TTBx, or for stage 2 by {SL0,
T0SZ}.

The Neoverse™ N1 core treats such a block as not causing a translation fault.

Conﬂict aborts

The Neoverse™ N1 core does not generate Conﬂict aborts.

6.6.3 Memory attributes

The memory region attributes speciﬁed in the TLB entry, or in the descriptor in case of translation
table walk, determine if the access is:

- Normal Memory or Device type.

- One of the four diﬀerent device memory types that are deﬁned for Armv8:

Device-
nGnRnE

Device non-Gathering, non-Reordering, No Early Write
Acknowledgement.
Device-
nGnRE

Device non-Gathering, non-Reordering, Early Write
Acknowledgement.
Device-
nGRE

Device non-Gathering, Reordering, Early Write Acknowledgement.

Device-
GRE

Device Gathering, Reordering, Early Write Acknowledgment.

In the Neoverse™ N1 core, a page is cacheable only if the Inner and Outer memory attributes are
Write-Back. In all other cases, all pages are downgraded to Non-cacheable Normal memory.

When the Memory Management Unit (MMU) is disabled at stage 1 and stage 2, and SCTLR.I is set
to 1, instruction prefetches are cached in the instruction cache but not in the uniﬁed cache. In all
other cases, normal behavior on memory attribute applies.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information on
translation table formats.

Page-Based Hardware Attributes (PBHA) is an optional, IMPLEMENTATION DEFINED feature.

It allows software to set up to two bits in the translation tables, which are then propagated
though the memory system with transactions, and can be used in the system to control system
components. The meaning of the bits is speciﬁc to the system design.

For information on how to set and enable the PBHA bits in the translation tables, see the Arm®
Architecture Reference Manual for A-proﬁle architecture. When disabled, the PBHA value that is
propagated on the bus is 0.

For memory accesses caused by a translation table walk, the AHTCR, ATTBCR, and AVTCR
registers control the PBHA values.

PBHA combination between stage 1 and stage 2 on memory accesses
PBHA should always be considered as an attribute of the physical address.

When stage 1 and stage 2 are enabled:

- If both stage 1 PBHA and stage 2 PBHA are enabled, the ﬁnal PBHA is stage 2 PBHA.

- If stage 1 PBHA is enabled and stage 2 PBHA is disabled, the ﬁnal PBHA is stage 1 PBHA.

- If stage 1 PBHA is disabled and stage 2 PBHA is enabled, the ﬁnal PBHA is stage 2 PBHA.

- If both stage 1 PBHA and stage 2 PBHA are disabled, the ﬁnal PBHA is deﬁned to 0.

Enable of PBHA has a granularity of 1 bit, so this property is applied independently on each PBHA
bit.

Mismatched aliases
If the same physical address is accessed through more than one virtual address mapping, and the
PBHA bits are diﬀerent in the mappings, then the results are UNPREDICTABLE. The PBHA value sent
on the bus could be for either mapping.

# 7. L1 memory system

## 7.1 About the L1 memory system

### 7.1.1 L1 instruction side memory system

### 7.1.2 L1 data side memory system

This chapter describes the L1 instruction cache and data cache that make up the L1 memory
system.

The Neoverse™ N1 L1 memory system is designed to enhance core performance and save power.

The L1 memory system consists of separate instruction and data caches. Both have a ﬁxed size of
64KB.

7.1.1 L1 instruction side memory system

The L1 instruction memory system has the following key features:

- Virtually Indexed, Physically Tagged (VIPT) 4-way set-associative L1 instruction cache, which
behaves as a Physically Indexed, Physically Tagged (PIPT) cache.

- Fixed cache line length of 64 bytes.

- Pseudo-LRU cache replacement policy.

- 256-bit read interface from the L2 memory system.

- Optional instruction cache hardware coherency.

The L1 data memory system has the following features:

- Virtually Indexed, Physically Tagged (VIPT), which behaves as a Physically Indexed, Physically Tagged
(PIPT) 4-way set-associative L1 data cache.

- Fixed cache line length of 64 bytes.

- Pseudo-LRU cache replacement policy.

- 256-bit write interface from the L2 memory system.

- 256-bit read interface from the L2 memory system.

- Two 128-bit read paths from the data L1 memory system to the datapath.

- 256-bit write path from the datapath to the L1 memory system.

## 7.2 Cache behavior

### 7.2.1 Instruction cache disabled behavior

### 7.2.2 Instruction cache speculative memory accesses

The IMPLEMENTATION SPECIFIC features of the instruction and data caches include:

- At reset the instruction and data caches are disabled and both caches are automatically
invalidated.

Caches in the core are invalidated automatically at reset deassertion unless the core
power mode is initialized to Debug recovery mode. See the  Arm® DynamIQ™ Shared
Unit Technical Reference Manual for more information.

- You can enable or disable each cache independently.

- On a cache miss, data for the cache lineﬁll is requested in critical word-ﬁrst order.

7.2.1 Instruction cache disabled behavior

If the instruction cache is disabled, fetches cannot access any of the instruction cache arrays. An
exception is the instruction cache maintenance operations. If the instruction cache is disabled, the
instruction cache maintenance operations can still execute normally.

If the instruction cache is disabled, all instruction fetches to cacheable memory are treated as if
they were Non-cacheable. This treatment means that instruction fetches might not be coherent
with caches in other cores, and software must take account of this.

Conﬁguring instruction cache hardware coherency does not aﬀect this requirement.
Software will still need to guarantee that any stores that should be visible to
non-cacheable instruction fetches are explicitly made visible using data cache
maintenance operations.

Instruction fetches are speculative. Execution is not guaranteed, because there can be several
unresolved branches in the pipeline.

A branch instruction or exception in the code stream can cause a pipeline ﬂush, discarding the
currently fetched instructions. On instruction fetch accesses, pages with Device memory type
attributes are treated as Non-Cacheable Normal Memory.

Device memory pages must be marked with the translation table descriptor attribute bit Execute
Never (XN). The device and code address spaces must be separated in the physical memory map.
This separation prevents speculative fetches to read-sensitive devices when address translation is
disabled.

### 7.2.3 Data cache disabled behavior

### 7.2.4 Data cache maintenance considerations

### 7.2.5 Data cache coherency

If the instruction cache is enabled, and if the instruction fetches miss in the L1 instruction cache,
they can still look up in the L1 data caches. However, a new line is not allocated in the data cache
unless the data cache is enabled.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

7.2.3 Data cache disabled behavior

If the data cache is disabled, load and store instructions do not access any of the L1 data, L2 cache,
and, if present, the DynamIQ Shared Unit (DSU) L3 cache arrays.

When the data cache is disabled, instructions and operations are aﬀected as follows:

- An instruction fetch does not allocate a new line in the L2 or L3 caches.

- All load and store instructions to cacheable memory are treated as if they were Non-cacheable
and are incoherent with the caches in both this core and other cores in the cluster. Software
must take this into account.

- Data cache maintenance operations are an exception and will execute normally.

The L2 and L1 data caches cannot be disabled independently.

7.2.4 Data cache maintenance considerations

DCIMVAC operations in AArch32 state are treated as DCCIMVAC. DC IVAC operations in
AArch64 state are treated as DC CIVAC except for permission checking and watchpoint matching.

DCISW operations in AArch32 state and DC ISW operations in AArch64 state, perform both a
clean and invalidate of the target set/way. The values of HCR.SWIO and HCR_EL2.SWIO have no
eﬀect.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

To maintain data coherency between multiple cores, the Neoverse™ N1 core uses the Modiﬁed
Exclusive Shared Invalid (MESI) protocol.

### 7.2.6 Instruction cache coherency

### 7.2.7 Write streaming mode

7.2.6 Instruction cache coherency

If conﬁgured with instruction cache hardware coherency, the instruction cache maintains
coherency with instruction, data, and uniﬁed caches between multiple cores using the MESI
protocol.

Software instruction cache maintenance is not necessary when the core is conﬁgured for
instruction cache hardware coherency.

A cache line is allocated to the L1 on either a read miss or a write miss.

However, there are some situations where allocating on writes is not required. For example, when
executing the C standard library memset() function to clear a large block of memory to a known
value. Writes of large blocks of data can pollute the cache with unnecessary data. It can also waste
power and performance if a lineﬁll must be performed only to discard the lineﬁll data because the
entire line was subsequently written by the memset().

To counter this, the L1 memory system includes logic to detect when the core has stores pending
to a full cache line when it is waiting for a lineﬁll to complete, or when it detects a DCZVA (full cache
line write to zero). If this situation is detected, then it switches into write streaming mode.

When in write streaming mode, loads behave as normal, and can still cause lineﬁlls, and writes still
lookup in the cache, but if they miss then they write out to L2 (or possibly L3, system cache, or
DRAM) rather than starting a lineﬁll.

The L1 memory system continues in write streaming mode until it can no longer create a full
cacheline of store (for example because of a lack of resource in the L1 memory system) or has
detected a high proportion of store hitting in the cache.

The L1 memory system is monitoring transaction traﬃc through L1 and, depending
on diﬀerent thresholds, can set a stream to go out to L2, L3, and system cache and
DRAM.

The following register controls the diﬀerent thresholds:

AArch64 state

CPUECTLR_EL1 conﬁgure the L2, L3, and system cache write streaming mode threshold. See
13.32 CPUECTLR_EL1, CPU Extended Control Register, EL1  on page 141.

## 7.3 L1 instruction memory system

### 7.3.1 Program flow prediction

The L1 instruction side memory system provides an instruction stream to the decoder.

To increase overall performance and to reduce power consumption, it uses:

- Dynamic branch prediction.

- Instruction caching.

7.3.1 Program ﬂow prediction

The Neoverse™ N1 core contains program ﬂow prediction hardware, also known as branch
prediction.

Branch prediction increases overall performance and reduces power consumption. With program
ﬂow prediction disabled, all taken branches incur a penalty that is associated with ﬂushing the
pipeline.

To avoid this penalty, the branch prediction hardware predicts if a conditional or unconditional
branch is to be taken. For conditional branches, the hardware predicts if the branch is to be taken.
It also predicts the address that the branch goes to, known as the branch target address. For
unconditional branches, only the target is predicted.

The hardware contains the following functionality:

- A Branch Target Buﬀer (BTB) holding the branch target address of previously taken branches.

- Dynamic branch predictor history.

- The return stack, a stack of nested subroutine return addresses.

- A static branch predictor.

- An indirect branch predictor.

Predicted and non-predicted instructions
Unless otherwise speciﬁed, the following list applies to A64, A32, and T32 instructions.  As a rule
the ﬂow prediction hardware predicts all branch instructions regardless of the addressing mode,
and includes:

- Conditional branches.

- Unconditional branches.

- Indirect branches that are associated with procedure call and return instructions.

- Branches that switch between A32 and T32 states.

The following branch instructions are not predicted:

- Exception return instructions.

### 7.3.2 Instruction cache hardware coherency

T32 state conditional branches
A T32 unconditional branch instruction can be made conditional by inclusion in an If-Then (IT)
block. It is then treated as a conditional branch.

Return stack
The return stack stores the address and instruction set state.

This address is equal to the link register value stored in R14 in AArch32 state or X30 in AArch64
state.

The following instructions cause a return stack push if predicted:

- BL r14

- BLX (immediate) in AArch32 state

- BLX (register) in AArch32 state

- BLR in AArch64 state

- MOV pc,r14

In AArch32 state, the following instructions cause a return stack pop if predicted:

- BX

- LDR pc, [r13], #imm

- LDM r13, {…pc}

- LDM r13, {…pc}

In AArch64 state, the RET instruction causes a return stack pop.

As exception return instructions can change core privilege mode and Security state, they are not
predicted. These include:

- ERET

When the optional instruction cache hardware coherency option is conﬁgured via the
COHERENT_ICACHE parameter, the following behaviors in the core are aﬀected:

- L1 instruction cache and L2 cache become strictly inclusive. Any cache line present in the L1
instruction cache is also present in the L2 cache.

- Instruction cache invalidate instructions are treated as no-ops and do not cause instruction
cache invalidatations or DVMMsg broadcasts to other cores.

- L2 cache monitors all store and cache invalidation coherency traﬃc and ensures that the L1
instruction cache invalidates any entry that is written to, or invalidated from, the L2 cache.

## 7.4 L1 data memory system

### 7.4.1 Memory system implementation

- CTR_EL0[29] reads as 1. Using this register, software can discover that the core implements
instruction cache hardware coherency and can optimize functions to not issue instruction cache
instructions.

The following restrictions and recommendations apply to conﬁguring instruction cache hardware
coherency in the core:

- The coherency domain containing a core conﬁgured with instruction cache hardware coherency
must not contain any coherent masters that require software instruction cache maintenance.

- Arm recommends that systems using instruction cache hardware coherency should be
conﬁgured with an L2 cache size of 1MB. An L2 cache size of 512KB is also acceptable, but will
see approximately a 1-2% reduction in performance due to the overhead of a strictly inclusive
L1 instruction cache and L2 cache.

- Arm recommends systems consisting of a large number of Neoverse™ N1 cores should
conﬁgure the cores with instruction cache coherency to eliminate possible performance issues
related to instruction cache instruction broadcasts as DVMMsg transactions to all masters in
the system.

The L1 data cache is organized as a Virtually Indexed, Physically Tagged (VIPT) cache featuring four
ways.

Data cache invalidate on reset

The Armv8-A architecture does not support an operation to invalidate the entire data
cache. If software requires this function, it must be constructed by iterating over the cache
geometry and executing a series of individual invalidate by set/way instructions.

This section describes the implementation of the L1 memory system.

Limited Order Regions
The core oﬀers support for four limited ordering region descriptors, as introduced by the Armv8.1
Limited Ordering Regions.

Atomic instructions

The Neoverse™ N1 core supports the atomic instructions that are added in Armv8.1 architecture.

Atomic instructions to cacheable memory can be performed as either near atomics or far atomics,
depending on where the cache line containing the data resides.

When an instruction hits in the L1 data cache in a unique state, then it is performed as a near
atomic in the L1 memory system. If the atomic operation misses in the L1 cache, or the line is
shared with another core, then the atomic is sent as a far atomic on the core CHI interface.

If the operation misses everywhere within the cluster, and the interconnect supports far atomics,
then the atomic is passed on to the interconnect to perform the operation.

When the operation hits anywhere inside the cluster, or when an interconnect does not support
atomics, the L3 memory system performs the atomic operation. If the line it is not already there,
it allocates the line into the L3 cache. This depends on whether the DynamIQ Shared Unit (DSU) is
conﬁgured with an L3 cache.

Therefore, if software prefers that the atomic is performed as a near atomic, precede the atomic
instruction with a PLDW or PRFM PSTL1KEEP instruction.

Alternatively, the CPUECTLR can be programmed such that diﬀerent types of atomic instructions
attempt to execute as a near atomic. One cache ﬁll is made on an atomic. If the cache line is lost
before the atomic operation can be made, it is sent as a far atomic.

The Neoverse™ N1 core supports atomics to device or non-cacheable memory, however this relies
on the interconnect also supporting atomics. If such an atomic instruction is executed when the
interconnect does not support them, it results in an abort.

For more information on the CPUECTLR register, see 13.32 CPUECTLR_EL1, CPU Extended
Control Register, EL1  on page 141.

LDAPR instructions
The core supports Load acquire instructions adhering to the RCpc consistency semantic introduced
in the Armv8.3 extensions for A proﬁle. This is reﬂected in register ID_AA64ISAR1_EL1 where
bits[23:20] are set to 0b0001 to indicate that the core supports LDAPRB, LDAPRH, and LDAPR
instructions implemented in AArch64.

Transient memory region
The core has a speciﬁc behavior for memory regions that are marked as write-back cacheable and
transient, as deﬁned in the Armv8.0 architecture.

For any load or store that is targeted at a memory region that is marked as transient, the following
occurs:

- If the memory access misses in the L1 data cache, the returned cache line is allocated in the L1
data cache but is marked as transient.

- When the line is evicted from the L1 data cache, the transient hint is passed to the L2 cache so
that the replacement policy will not attempt to retain the line. When the line is subsequently
evicted from the L2 cache, it will bypass the next level cache entirely.

Non-temporal loads
Non-temporal loads indicate to the caches that the data is likely to be used for only short periods.
For example, when streaming single-use read data that is then discarded. In addition to non-
temporal loads, there are also prefetch-memory (PRFM) hint instructions with the STRM qualiﬁer.

Non-temporal loads to memory which are designated as Write-Back are treated the same as loads
to Transient memory.

### 7.4.2 Internal exclusive monitor

## 7.5 Data prefetching

7.4.2 Internal exclusive monitor

The Neoverse™ N1 core L1 memory system has an internal exclusive monitor.

This monitor is a 2-state, open and exclusive, state machine that manages Load-Exclusive or Store-
Exclusive accesses and Clear-Exclusive (CLREX) instructions. You can use these instructions to
construct semaphores, ensuring synchronization between diﬀerent processes running on the
core, and also between diﬀerent cores that are using the same coherent memory locations for
the semaphore. A Load-Exclusive instruction tags a small block of memory for exclusive access.
CTR.ERG deﬁnes the size of the tagged block as 16 words, one cache line.

A load/store exclusive instruction is any one of the following:

- In the A64 instruction set, any instruction that has a mnemonic starting with
LDX, LDAX, STX, or STLX.

- In the A32 and T32 instruction sets, any instruction that has a mnemonic
starting with LDREX, STREX, LDAEX, or STLEX.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information about
these instructions.

This section describes the data prefetching behavior for the Neoverse™ N1 core.

Preload instructions

The Neoverse™ N1 core supports the AArch64 Prefetch Memory (PRFM) instructions and the
AArch32 Prefetch Data (PLD) and Preload Data With Intent To Write (PLDW) instructions. These
instructions signal to the memory system that memory accesses from a speciﬁed address are likely
to occur soon. The memory system acts by taking actions that aim to reduce the latency of the
memory access when they occur. PRFM instructions perform a lookup in the cache, and if they miss
and are to a cacheable address, a lineﬁll starts. However, the PRFM instruction retires when its lineﬁll
is started, rather than waiting for the lineﬁll to complete. This enables other instructions to execute
while the lineﬁll continues in the background.

The Preload Instruction (PLI) memory system hint performs preloading in the L2 cache for cacheable
accesses if they miss in both the L1 instruction cache and L2 cache. Instruction preloading is
performed in the background.

For more information about prefetch memory and preloading caches, see the Arm® Architecture
Reference Manual for A-proﬁle architecture.

## 7.6 Direct access to internal memory

Data prefetching and monitoring
The load-store unit includes a hardware prefetcher that is responsible for generating prefetches
targeting both the L1 and the L2 cache. The load side prefetcher uses the virtual address to
prefetch to both the L1 and L2 Cache. The store side prefetcher uses the physical address, and
only prefetches to the L2 Cache.

The CPUECTLR register allows you to have some control over the prefetcher. See 13.32
CPUECTLR_EL1, CPU Extended Control Register, EL1  on page 141 for more information on the
control of the prefetcher.

Use the prefetch memory system instructions for data prefetching where short sequences or
irregular pattern fetches are required.

Data cache zero
The Armv8-A architecture introduces a Data Cache Zero by Virtual Address (DC ZVA) instruction.

In the Neoverse™ N1 core, this enables a block of 64 bytes in memory, which is aligned to 64 bytes
in size, to be set to zero.

For more information, see the Arm® Architecture Reference Manual for A-proﬁle architecture.

The Neoverse™ N1 core provides a mechanism to read the internal memory that is used by the
L1 caches, L2 cache, and TLB structures through IMPLEMENTATION DEFINED System registers. This
functionality can be useful when debugging software or hardware issues.

When the core executes in AArch64 state, there are six read-only registers that are used to
access the contents of the internal memory. The internal memory is selected by programming
the IMPLEMENTATION DEFINED RAMINDEX register (using SYS #6, c15, c0, #0 instruction). These
operations are available only in EL3. In all other modes, executing these instructions results in
an Undeﬁned Instruction exception. The data is read from read-only registers as shown in the
following table. After accessing the cache, a Data Synchronization Barrier (DSB) is required prior to
reading the data register.

Table 7-1: AArch64 registers used to access internal memory

|Register name|Function|Access|Operation|Rd Data|
|---|---|---|---|---|
|IDATA0_EL3|Instruction Register 0|Read-only|`S3_6_c15_c0_0`|Data|
|IDATA1_EL3|Instruction Register 1|Read-only|`S3_6_c15_c0_1`|Data|
|IDATA2_EL3|Instruction Register 2|Read-only|`S3_6_c15_c0_2`|Data|
|DDATA0_EL3|Data Register 0|Read-only|`S3_6_c15_c1_0`|Data|
|DDATA1_EL3|Data Register 1|Read-only|`S3_6_c15_c1_1`|Data|
|DDATA2_EL3|Data Register 2|Read-only|`S3_6_c15_c1_2`|Data|

### 7.6.1 Encoding for L1 instruction cache tag, L1 instruction cache data, L1 BTB, L1 GHB, L1 TLB instruction, and BPIQ

7.6.1 Encoding for L1 instruction cache tag, L1 instruction cache data, L1
BTB, L1 GHB, L1 TLB instruction, and BPIQ

The following tables show the encoding required to select a given cache line.

Table 7-2: L1 instruction cache tag location encoding

Table 7-3: L1 instruction cache data location encoding

Table 7-4: L1 BTB data location encoding

Table 7-5: L1 GHB data location encoding

Table 7-6: L1 instruction TLB data location encoding

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x00`|
|[23:20]|Reserved|
|[19:18]|Way|
|[17:14]|Reserved|
|[13:6]|Index [13:6]|
|[5:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x01`|
|[23:20]|Reserved|
|[19:18]|Way|
|[17:14]|Reserved|
|[13:3]|Index [13:3]|
|[2:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x02`|
|[23:20]|Reserved|
|[19:18]|Way|
|[17:15]|Reserved|
|[14:5]|Index [14:5]|
|[4:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x03`|
|[23:14]|Reserved|
|[13:4]|Index [13:4]|
|[3:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x04`|

Table 7-7: BPIQ data location encoding

The following table shows the data that is returned from accessing the L1 instruction tag RAM.

Table 7-8: L1 instruction cache tag format

The following table shows the data that is returned from accessing the L1 instruction data RAM.

Table 7-9: L1 instruction cache data format

The following table shows the data that is returned from accessing the L1 BTB RAM.

Table 7-10: L1 BTB cache format

|Bit fields of Rd|Description|
|---|---|
|[23:8]|Reserved|
|[7:0]|TLB Entry (<=47)|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x05`|
|[23:10]|Reserved|
|[9:4]|Index [5:0]|
|[3:0]|Reserved|

|Register|Bit field|Description|
|---|---|---|
|Instruction Register 0|[39]|Non-secure identiﬁer for the physical<br>address|
|Instruction Register 0|[38:3]|Physical address [47:12]|
|Instruction Register 0|[2:1]|Instruction state [1:0]<br>**00**<br>Invalid<br>**01**<br>T32<br>**10**<br>A32<br>**11**<br>A64|
|Instruction Register 0|[0]|Parity|
|Instruction Register 1|[63:0]|0|
|Instruction Register 2|[63:0]|0|

|Register|Bit field|Description|
|---|---|---|
|Instruction Register 0|[63:0]|Data [63:0]|
|Instruction Register 1|[63:9]|0|
|Instruction Register 1|[8]|Parity|
|Instruction Register 1|[7:0]|Data [71:64]|
|Instruction Register 2|[63:0]|0|

|Register|Bit field|Description|
|---|---|---|
|Instruction Register 0|[63:0]|Data [63:0]|
|Instruction Register 1|[63:18]|0|
|Instruction Register 1|[17:0]|Data [81:64]|

The following table shows the data that is returned from accessing the L1 GHB RAM.

Table 7-11: L1 GHB cache format

The following table shows the data that is returned from accessing the L1 instruction TLB RAM.

Table 7-12: L1 instruction TLB cache format

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Instruction Register 0

[63:59]
[63:59]
Virtual address [16:12]

-
[58:57]
PBHA[1:0]

[58:56]
[56]
TLB attribute

[55:53]
[55:53]
Memory attributes:

000
Device nGnRnE
001
Device nGnRE
010
Device nGRE
011
Device GRE
100
Non-cacheable
101
Write-Back No-Allocate
110
Write-Back Transient
111
Write-Back Read-Allocate and Write-Allocate

[52:50]
[52:50]
Page size:

000
4KB
001
16KB
010
64KB
011
256KB
100
2MB
101
32MB
11x
Reserved

[49:46]
[49:46]
TLB attribute

[45]
[45]
Outer-shared

[44]
[44]
Inner-shared

[43:39]
[43:39]
TLB attribute

[38:23]
[38:23]
ASID

[22:7]
[22:7]
VMID

|Register|Bit field|Description|
|---|---|---|
|Instruction Register 2|[63:0]|0|

|Register|Bit field|Description|
|---|---|---|
|Instruction Register 0|[63:0]|Data [63:0]|
|Instruction Register 1|[63:32]|0|
|Instruction Register 1|[31:0]|Data [95:64]|
|Instruction Register 2|[63:0]|0|

### 7.6.2 Encoding for L1 data cache tag, L1 data cache data, and L1 TLB data

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

[6:5]
[6:5]
Translation regime:

00
Secure EL1/EL0
01
Secure EL3
10
Non-secure EL1/EL0
11
Non-secure EL2

[4:1]
[4:1]
TLB attribute

[0]
[0]
Valid

[63:32]
[63:32]
Physical address[43:12]
Instruction Register 1

[31:0]
[31:0]
Virtual address [48:17]

[4]
[4]
Non-secure
Instruction Register 2

[3:0]
[3:0]
Physical address [47:44]

The following table shows the data that is returned from accessing the BPIQ RAM.

Table 7-13: BPIQ cache format

The core data cache consists of a 4-way set-associative structure.

The encoding, which is set in Rd in the appropriate MCR instruction, used to locate the required
cache data entry for tag, data, and TLB memory is shown in the following tables. It is similar for
both the tag RAM, data RAM, and TLB access. Data RAM access includes an additional ﬁeld to
locate the appropriate doubleword in the cache line.

Tag RAM encoding includes an additional ﬁeld to select which one of the two cache channels must
be used to perform any access.

Table 7-14: L1 data cache tag location encoding

|Register|Bit field|Description|
|---|---|---|
|Instruction Register 0|[63:0]|Data [63:0]|
|Instruction Register 1|[63:32]|0|
|Instruction Register 1|[31:0]|Data [95:64]|
|Instruction Register 2|[63:0]|0|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x08`|
|[23:20]|Reserved|
|[19:18]|Way|
|[17]|Copy<br>**0**<br>Tag RAM associated with Pipe 0<br>**1**<br>Tag RAM associated with Pipe 1|

Table 7-15: L1 data cache data location encoding

Table 7-16: L1 data TLB location encoding

Data cache reads return 64 bits of data in Data Register 0, Data Register 1, and Data Register 2. If
cache protection is supported, Data Register 2 is used to report ECC information using the format
shown in the following tables.

The following table shows the data that is returned from accessing the L1 data cache tag RAM.

Table 7-17: L1 data cache tag format

|Bit fields of Rd|Description|
|---|---|
|[16:14]|Reserved|
|[13:6]|Index [13:6]|
|[5:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x09`|
|[23:20]|Reserved|
|[19:18]|Way|
|[17:16]|BankSel|
|[15:14]|Unused|
|[13:6]|Index [13:6]|
|[5:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x0A`|
|[23:6]|Reserved|
|[5:0]|TLB Entry (0->47)|

|Register|Bit field|Description|
|---|---|---|
|Data Register 0|[63:49]|0|
|Data Register 0|[48:42]|ECC|
|Data Register 0|[41]|Non-secure identiﬁer for the physical<br>address|
|Data Register 0|[40:5]|Physical address [47:12]|
|Data Register 0|[4:3]|Reserved|
|Data Register 0|[2]|Transient/WBNA|
|Data Register 0|[1:0]|MESI<br>**00**<br>Invalid<br>**01**<br>Shared<br>**10**<br>Exclusive<br>**11**<br>Modiﬁed with respect to the L2<br>cache|
|Data Register 1|[63:0]|0|
|Data Register 2|[63:0]|0|

The following table shows the data that is returned from accessing the L1 data cache data RAM.

Table 7-18: L1 data cache data format

The following table shows the data that is returned from accessing the L1 data TLB RAM.

Table 7-19: L1 data TLB cache format

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 0

[63:62]
[63:62]
Virtual address [13:12]

[58]
[58]
Outer-shared

[57]
[57]
Inner-shared

[52:50]
[52:50]
Memory attributes:

000
Device nGnRnE
001
Device nGnRE
010
Device nGRE
011
Device GRE
100
Non-cacheable
101
Write-Back No-Allocate
110
Write-Back Transient
111
Write-Back Read-Allocate and Write-Allocate

[38:36]
[38:36]
Page size:

000
4KB
001
16KB
010
64KB
011
256KB
100
2MB
101
Reserved
110
512MB
111
Reserved

[35]
[35]
Non-secure

[34:33]
[34:33]
Translation regime:

00
Secure EL1/EL0
01
Secure EL3
10
Non-secure EL1/EL0
11
Non-secure EL2

[32:17]
[32:17]
ASID

[16:1]
[16:1]
VMID

[0]
[0]
Valid

|Register|Bit field|Description|
|---|---|---|
|Data Register 0|[63:0]|Word1_data[31:0], Word0_data[31:0]|
|Data Register 1|[63:0]|Word3_data[31:0], Word2_data[31:0]|
|Data Register 2|[63:0]|Word3_ecc[6:0], Word3_poison,<br>Word2_ecc[6:0], Word2_poison,<br>Word1_ecc[6:0], Word1_poison,<br>Word0_ecc[6:0], Word0_poison|

### 7.6.3 Encoding for the L2 unified cache

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

[63:35]
[63:35]
Physical address [40:12]
Data Register 1

[34:0]
[34:0]
Virtual address[48:14]

-
[8:7]
PBHA[1:0]
Data Register 2

[6:0]
[6:0]
Physical address [47:41]

7.6.3 Encoding for the L2 uniﬁed cache

The following tables show the encoding required to select a given cache line.

Table 7-20: L2 tag location encoding

Table 7-21: L2 data location encoding

Table 7-22: L2 victim location encoding

The following table shows the data that is returned from accessing the L2 tag RAM when L2 is
conﬁgured with a 256KB cache size.

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x10`|
|[23:21]|Reserved|
|[20:18]|Way (0->7)|
|[17]|Reserved|
|[16:6]|Index[16:6]|
|[5:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x11`|
|[23:21]|Reserved|
|[20:18]|Way (0->7)|
|[17]|Reserved|
|[16:4]|Index[16:4]|
|[3:0]|Reserved|

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x12`|
|[23:17]|Reserved|
|[16:6]|Index[16:6]|
|[5:0]|Reserved|

Table 7-23: L2 tag format with a 256KB L2 cache size without COHERENT_ICACHE

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 0

[63:52]
[63:54]
0

[51:45]
[53:47]
ECC [6:0]

-
[46:45]
PBHA[1:0]

[44:12]
[44:12]
Physical address [47:15]

[11]
[11]
Non-secure identiﬁer for the
physical address

[10:9]
[10:9]
Virtual index [13:12]

[8:6]
[8:6]
Reserved

[5]
[5]
Shareable

[4]
[4]
Outer allocation hint

[3]
[3]
L1 data cache valid

[2:0]
[2:0]
L2 State

101
Modiﬁed
001
Exclusive
x11
Shared
xx0
Invalid

Data Register 1
[63:0]
[63:0]
0

Data Register 2
[63:0]
[63:0]
0

Table 7-24: L2 tag format with a 256KB L2 cache size with COHERENT_ICACHE

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 0

[63:57]
[63:59]
0

[56:50]
[58:52]
ECC [6:0]

[49:46]
[51:48]
L1 instruction cache valid

-
[47:46]
PBHA[1:0]

[45:13]
[45:13]
Physical address [47:15]

[12]
[12]
Non-secure identiﬁer for the
physical address

[11:10]
[11:10]
Virtual index [13:12]

[9:7]
[9:7]
Reserved

[6]
[6]
Shareable

[5]
[5]
Outer allocation hint

[4]
[4]
L1 data cache shared

[3]
[3]
L1 data cache valid

[2:0]
[2:0]
L2 State

101
Modiﬁed
001
Exclusive
x11
Shared
xx0
Invalid

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 1
[63:0]
[63:0]
0

Data Register 2
[63:0]
[63:0]
0

The following table shows the data that is returned from accessing the L2 tag RAM when L2 is
conﬁgured with a 512KB cache size.

Table 7-25: L2 tag format with a 512KB L2 cache size without COHERENT_ICACHE

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 0

[63:51]
[63:53]
0

[50:44]
[52:46]
ECC [6:0]

-
[45:44]
PBHA[1:0]

[43:12]
[43:12]
Physical address [47:16]

[11]
[11]
Non-secure identiﬁer for the
physical address

[10:9]
[10:9]
Virtual index [13:12]

[8:6]
[8:6]
Reserved

[5]
[5]
Shareable

[4]
[4]
Outer allocation hint

[3]
[3]
L1 data cache valid

[2:0]
[2:0]
L2 State

101
Modiﬁed
001
Exclusive
x11
Shared
xx0
Invalid

Data Register 1
[63:0]
[63:0]
0

Data Register 2
[63:0]
[63:0]
0

Table 7-26: L2 tag format with a 512KB L2 cache size with COHERENT_ICACHE

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 0

[63:56]
[63:58]
0

[55:49]
[57:51]
ECC [6:0]

[48:45]
[50:47]
L1 instruction cache valid

-
[46:45]
PBHA[1:0]

[44:13]
[44:13]
Physical address [47:16]

[12]
[12]
Non-secure identiﬁer for the
physical address

[11:10]
[11:10]
Virtual index [13:12]

[9:7]
[9:7]
Reserved

[6]
[6]
Shareable

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

[5]
[5]
Outer allocation hint

[4]
[4]
L1 data cache shared

[3]
[3]
L1 data cache valid

[2:0]
[2:0]
L2 State

101
Modiﬁed
001
Exclusive
x11
Shared
xx0
Invalid

Data Register 1
[63:0]
[63:0]
0

Data Register 2
[63:0]
[63:0]
0

The following table shows the data that is returned from accessing the L2 tag RAM when L2 is
conﬁgured with a 1MB cache size.

Table 7-27: L2 tag format with a 1MB L2 cache size without COHERENT_ICACHE

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Data Register 0

[63:50]
[63:52]
0

[49:43]
[51:45]
ECC [6:0]

-
[44:43]
PBHA[1:0]

[42:12]
[42:12]
Physical address [47:17]

[11]
[11]
Non-secure identiﬁer for the
physical address

[10:9]
[10:9]
Virtual index [13:12]

[8:6]
[8:6]
Reserved

[5]
[5]
Shareable

[4]
[4]
Outer allocation hint

[3]
[3]
L1 data cache valid

[2:0]
[2:0]
L2 State

101
Modiﬁed
001
Exclusive
x11
Shared
xx0
Invalid

Data Register 1
[63:0]
[63:0]
0

Data Register 2
[63:0]
[63:0]
0

Table 7-28: L2 tag format with a 1MB L2 cache size with COHERENT_ICACHE

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

[63:55]
[63:57]
0
Data Register 0

[54:48]
[56:50]
ECC [6:0]

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

[47:44]
[49:46]
L1 instruction cache valid

-
[45:44]
PBHA[1:0]

[43:13]
[43:13]
Physical address [47:17]

[12]
[12]
Non-secure identiﬁer for the
physical address

[11:10]
[11:10]
Virtual index [13:12]

[9:7]
[9:7]
Reserved

[6]
[6]
Shareable

[5]
[5]
Outer allocation hint

[4]
[4]
L1 data cache shared

[3]
[3]
L1 data cache valid

[2:0]
[2:0]
L2 State

101
Modiﬁed
001
Exclusive
x11
Shared
xx0
Invalid

Data Register 1
[63:0]
[63:0]
0

Data Register 2
[63:0]
[63:0]
0

The following table shows the data that is returned from accessing the L2 data RAM.

Table 7-29: L2 data format

The following table shows the data that is returned from accessing the L2 victim RAM.

Table 7-30: L2 victim format

|Register|Bit field|Description|
|---|---|---|
|Data Register 0|[63:0]|Data [63:0]|
|Data Register 1|[63:0]|Data [127:64]|
|Data Register 2|[63:16]|0|
|Data Register 2|[15:8]|ECC for Data [127:64]|
|Data Register 2|[7:0]|ECC for Data [63:0]|

|Register|Bit field|Description|
|---|---|---|
|Data Register 0|[63:7]|0|
|Data Register 0|[6:0]|PLRU [6:0]|
|Data Register 1|[63:0]|0|
|Data Register 2|[63:0]|0|

### 7.6.4 Encoding for the L2 TLB

The following section describes the encoding for L2 TLB direct accesses.

The following table shows the encoding required to select a given TLB entry.

Table 7-31: L2 TLB encoding

The following table shows the data that is returned from accessing the L2 TLB.

Table 7-32: L2 TLB format

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

Instruction Register 0

[63]
[63]
Reserved

[62:60]
[62:60]
Memory attributes:

000
Device nGnRnE
001
Device nGnRE
010
Device nGRE
011
Device GRE
100
Non-cacheable
101
Write-Back No-
Allocate
110
Write-Back Transient
111
Write-Back Read-
Allocate and Write-
Allocate

[59:56]
[59:56]
Reserved

[55:20]
[55:20]
Physical address [47:12]

|Bit fields of Rd|Description|
|---|---|
|[31:24]|RAMID =`0x18`|
|[23:21]|Reserved|
|[20:18]|Way<br>**000**<br>way0<br>**001**<br>way1<br>**010**<br>way2<br>**011**<br>way3<br>**100**<br>way4|
|[17:8]|Reserved|
|[7:0]|Index|

Bit ﬁeld
Register

Description

CORE_PBHA=FALSE
CORE_PBHA=TRUE

[19:17]
[19:17]
Page size:

000
4KB
001
16KB
010
64KB
011
256KB
100
2MB
101
32MB
110
512MB
111
1GB

[16:7]
[16:7]
Reserved

[6]
[6]
Indicates that the entry is
coalesced and holds translations
for four contiguous pages

[5:2]
[5:2]
This bit ﬁeld contains the valid
bits for four contiguous pages. If
the entry is non-coalesced, then
0b0001 indicates a valid entry.

[1:0]
[1:0]
Reserved

Instruction Register 1

[63:62]
-
VMID [1:0]

[61:46]
[63:48]
ASID [15:0]

-
[47:46]
PBHA[1:0]

[45]
[45]
Walk cache entry

[44]
[44]
Prefetched translation

[43:15]
[43:15]
Virtual address [48:20]

[14]
[14]
Non-secure

[13:3]
[13:3]
Reserved

[2]
[2]
Non-global

[1]
[1]
Outer-shared

[0]
[0]
Inner-shared

Instruction Register 2

[63:16]
[63:18]
Reserved

[15:14]
[17:16]
Translation regime:

00
Secure EL1
01
EL3
10
Non-secure EL1
11
EL2

[13:0]
-
VMID [15:2]

-
[15:0]
VMID [15:0]

# 8. L2 memory system

## 8.1 About the L2 memory system

## 8.2 About the L2 cache

This chapter describes the L2 memory system.

8.1 About the L2 memory system

The L2 memory subsystem consists of:

- An 8-way set associative L2 cache with a conﬁgurable size of 256KB, 512KB, or 1024KB.
Cache lines have a ﬁxed length of 64 bytes.

- ECC protection for all RAM structures except victim array.

- Strictly inclusive with L1 data cache.

- When conﬁgured with instruction cache hardware coherency, strictly inclusive with L1
instruction cache.

- When conﬁgured without instruction cache hardware coherency, weakly inclusive with L1
instruction cache.

- Conﬁgurable CHI interface to the DynamIQ Shared Unit (DSU) or CHI compliant system with
support for 128-bit and 256-bit data widths.

- Dynamic biased replacement policy.

- Modiﬁed Exclusive Shared Invalid (MESI) coherency.

- Conﬁgurable support for instruction cache hardware coherency.

The integrated L2 cache is the Point of Uniﬁcation for the Neoverse™ N1 core. It handles both
instruction and data requests from the instruction side and data side of each core respectively.

When fetched from the system, instructions are allocated to the L2 cache and can be invalidated
during maintenance operations.

Caches in the core are invalidated automatically at reset deassertion unless the core
power mode is initialized to Debug recovery mode. See the  Arm® DynamIQ™ Shared
Unit Technical Reference Manual for more information.

## 8.3 Support for memory types

The Neoverse™ N1 core simpliﬁes the coherency logic by downgrading some memory types.

- Memory that is marked as both Inner Write-Back Cacheable and Outer Write-Back Cacheable
is cached in the L1 data cache and the L2 cache.

- Memory that is marked Inner Write-Through is downgraded to Non-cacheable.

- Memory that is marked Outer Write-Through or Outer Non-cacheable is downgraded to Non-
cacheable, even if the inner attributes are Write-Back cacheable.

The following table shows the transaction capabilities of the Neoverse™ N1 core. It lists the
maximum possible values for read, write, DVM issuing, and snoop capabilities of the private L2
cache.

Table 8-1: Neoverse™ N1 transaction capabilities

|Attribute|Value|Description|
|---|---|---|
|Write issuing capability|22/34/46|Maximum number of outstanding write<br>transactions. Dependent on the conﬁgured<br>TQ size. (24/36/48)|
|Read issuing capability|22/34/46|Maximum number of outstanding read<br>transactions. Dependent on the conﬁgured<br>TQ size. (24/36/48)|
|Snoop acceptance capability|17/23/29|Maximum number of outstanding snoops<br>and stashes accepted. Dependent on the TQ<br>size. (24/36/48)|
|DVM issuing capability|22/34/46|Maximum number of outstanding DVMOp<br>transactions. Dependent on the conﬁgured<br>TQ size. (24/36/48)|

# 9. Reliability, Availability, and Serviceability (RAS)

## 9.1 Cache ECC and parity

9. Reliability, Availability, and Serviceability
(RAS)

This chapter describes the RAS features implemented in the Neoverse™ N1 core.

The Neoverse™ N1 core implements the Reliability, Availability, and Serviceability (RAS) extension to
the Arm®v8‑A architecture which provides mechanisms for standardized reporting of the errors
that are generated by cache protection mechanisms.

When conﬁgured with core cache protection, the Neoverse™ N1 core can detect and correct a 1-
bit error in any RAM and detect 2-bit errors in some RAMs.

The Neoverse™ N1 core always includes core cache protection. The Neoverse™ N1 core can detect
and correct a 1-bit error in any RAM and detect 2-bit errors in some RAMs.

For information about SCU-L3 cache protection, see the  Arm® DynamIQ™ Shared
Unit Technical Reference Manual.

The RAS extension improves the system by reducing unplanned outages:

- Transient errors can be detected and corrected before they cause application or system failure.

- Failing components can be identiﬁed and replaced.

- Failure can be predicted ahead of time to allow replacement during planned maintenance.

Errors that are present but not detected are known as latent or undetected errors. A transaction
carrying a latent error is corrupted. In a system with no error detection, all errors are latent errors
and are silently propagated by components until either:

- They are masked and do not aﬀect the outcome of the system. These are benign or false errors.

- They aﬀect the service interface of the system and cause failure. These are silent data
corruptions.

The severity of a failure can range from minor to catastrophic. In many systems, data or service loss
is regarded as more of a minor failure than data corruption, as long as backup data is available.

The RAS extension focuses on errors that are produced from hardware faults, which fall into two
main categories:

- Transient faults.

- Persistent faults.

## 9.2 Cache protection behavior

The RAS extension describes data corruption faults, which mostly occur in memories and on data
links. RAS concepts can also be used for the management of other types of physical faults that are
found in systems, such as lock-step errors, thermal trip, and mechanical failure. The RAS extension
provides a common programmers model and mechanisms for fault handling and error recovery.

The conﬁguration of the RAS extension that is implemented in the Neoverse™ N1 core includes
cache protection.

In this case, the Neoverse™ N1 core protects against errors that result in a RAM bitcell holding the
incorrect value.

The RAMs in the Neoverse™ N1 core have the following capability:

SED

Single Error Detect. One bit of parity is applicable to the entire word. The word size is speciﬁc
for each RAM and depends on the protection granule.

Interleaved parity

One bit of parity is applicable to the even bits of the word, and one bit of parity is applicable
to the odd bits of the word.

SECDED

Single Error Correct, Double Error Detect.

Table 9-1: Cache protection behavior on page 82 indicates which protection type is applied to
each RAM.

The core can progress and remain functionally correct when there is a single bit error in any RAM.

If there are multiple single bit errors in diﬀerent RAMs, or within diﬀerent protection granules
within the same RAM, then the core also remains functionally correct.

If there is a double bit error in a single RAM within the same protection granule, then the behavior
depends on the RAM:

- For RAMs with SECDED capability, the core detects and either reports or defers the error. If
the error is in a cache line containing dirty data, then that data might be lost.

- For RAMs with only SED, the core does not detect a double bit error. This might cause data
corruption.

If there are three or more bit errors within the same protection granule, then depending on the
RAM and the position of the errors within the RAM, the core might or might not detect the errors.

The cache protection feature of the core has a minimal performance impact when no errors are
present.

Table 9-1: Cache protection behavior

To ensure that progress is guaranteed even in case of hard error, the core returns corrected data to
the core, and no cache access is required after data correction.

|RAM|Protection type|Protection granule|Correction behavior|
|---|---|---|---|
|L1 instruction cache tag|1 parity bit|39 bits|The line that contains the<br>error is invalidated from the L1<br>instruction cache and fetched<br>again from the subsequent<br>memory system.|
|L1 instruction cache data|SED|72 bits|The line that contains the<br>error is invalidated from the L1<br>instruction cache and fetched<br>again from the subsequent<br>memory system.|
|L1 BTB|None|-|-|
|L1 GHB|None|-|-|
|L1 BPIQ|None|-|-|
|L1 data cache tag|SECDED|42 bits + 7 bits for ECC attached<br>to the word.|The cache line that contains the<br>error gets evicted, corrected in<br>line, and reﬁlled to the core.|
|L1 data cache data|SECDED|32 bits of data + 1 poison bit +<br>7 bits for ECC attached to the<br>word|The cache line that contains the<br>error gets evicted, corrected in<br>line, and reﬁlled to the core.|
|L1 Prefetch History Table (PHT)|None|-|-|
|MMU translation cache|2 interleaved parity bits|71 bits|Entry invalidated, new pagewalk<br>started to refetch it.|
|MMU replacement policy|None|-|Entry invalidated, new pagewalk<br>started to refetch it.<br>**Note:**<br>The L1 TLBs are implemented<br>with ﬂops, so there is no cache<br>protection for L1 TLBs.|
|MMU biased replacement|None|-|Entry invalidated, new pagewalk<br>started to refetch it.|
|L2 cache tag|SECDED|Varies with L2 cache size and<br>COHERENT_ICACHE parameter<br>with 7 ECC bits for 50 to 57 tag<br>bits|Tag is corrected inline.|
|L2 cache data|SECDED|8 ECC bits for 64 data bits|Data is corrected inline and the<br>L2 cache is updated with the<br>corrected data.|
|L2 victim|None|-|-|
|L2 TQ data|SECDED|8 ECC bit for 64 data bits|Data is corrected inline.|

## 9.3 Uncorrected errors and data poisoning

## 9.4 RAS error types

## 9.5 Error Synchronization Barrier

9.3 Uncorrected errors and data poisoning

When an error is detected, the correction mechanism is triggered. However, if the error is a 2-bit
error in a RAM protected by ECC, then the error is not correctable.

The behavior on an uncorrected error depends on the type of RAM.

Uncorrected error detected in a data RAM
When an uncorrected error is detected in a data RAM, the chunk of data with the error is marked
as poisoned. This poison information is then transferred with the data and stored in the cache if the
data is allocated into another cache. The poisoned information is stored per 64 bits of data, except
in the L1 data cache where it is stored per 32 bits of data.

Uncorrected error detected in a tag RAM
When an uncorrected error is detected in a tag RAM, either the address or coherency state of
the line is not known, and the corresponding data cannot be poisoned. In this case, the line is
invalidated and an error recovery interrupt is generated to notify software that data has potentially
been lost.

9.4 RAS error types

This section describes the RAS error types that are introduced by the RAS extension and supported
in the Neoverse™ N1 core.

When a component accesses memory, an error might be detected in that memory and then be
corrected, deferred, or detected but silently propagated. The following table lists the types of RAS
errors that are supported in the Neoverse™ N1 core.

Table 9-2: RAS error types supported in the Neoverse™ N1 core

The Error Synchronization Barrier (ESB) instruction synchronizes unrecoverable system errors.

In the Neoverse™ N1 core, the ESB instruction allows eﬃcient isolation of errors:

|RAS error<br>type|Definition|
|---|---|
|Corrected|A _Corrected Error_ (CE) is reported for a single-bit ECC error on any protected RAM.|
|Deferred|A _Deferred Error_ (DE) is reported for a double-bit ECC error that aﬀects the data RAM on either the L1 data cache or the L2<br>cache.|
|Uncorrected|An_Uncorrected Error_ (UE) is reported for a double-bit ECC error that aﬀects the tag RAM of either the L1 data cache or<br>the L2 cache. An Uncorrected Error is also reported for external aborts that are received in response to a store, data cache<br>maintenance, instruction cache maintenance, TLBI maintenance, or cache copyback of dirty data.|

- The ESB instruction does not wait for completion of accesses that cannot generate an
asynchronous external abort. For example, if all external aborts are handled synchronously or it
is known that no such accesses are outstanding.

- The ESB instruction does not order accesses and does not guarantee a pipeline ﬂush.

All system errors must be synchronized by an ESB instruction, which guarantees the following:

- All system errors that are generated before the ESB instruction have pended a System Error
Interrupts (SEI) exception.

- If a physical SEI is pended by or was pending before the ESB instruction executes, then:

- It is taken before completion of the ESB instruction, if the physical SEI exception is
unmasked at the current Exception level.

- The pending SEI is cleared, the SEI status is recorded in DISR_EL1, and DISR_EL1.A is set
to 1 if the physical SEI exception is masked at the current Exception level. It indicates that
the SEI exception was generated before the ESB instruction by instructions that occur in
program order.

- If a virtual SEI is pended by or was pending before the ESB instruction executes, then:

- It is taken before completion of the ESB instruction, if the virtual SEI exception is unmasked.

- The pending virtual SEI is cleared and the SEI status is recorded in VDISR_EL2 using the
information that is provided by software in VSESR_EL2, if the virtual SEI exception is
masked.

After the ESB instruction, one of the following scenarios occurs:

- SEIs pended by errors are taken and their status is recorded in ESR_ELn.

- SEIs pended by errors are deferred and their status is recorded in DISR_EL1 or VDISR_EL2.

This includes unrecoverable SEIs that are generated by instructions, translation table walks, and
instruction fetches on the same core.

DISR_EL1 can only be accessed at EL1 and above. If EL2 is implemented and
HCR_EL2.AMO is set to 1, then reads and writes of DISR_EL1 at Non-secure EL1
access VDISR_EL2.

See the following registers:

- 13.41 DISR_EL1, Deferred Interrupt Status Register, EL1 on page 163.

- 13.57 HCR_EL2, Hypervisor Conﬁguration Register, EL2 on page 177.

- 13.107 VDISR_EL2, Virtual Deferred Interrupt Status Register, EL2 on page
249.

## 9.6 Error recording

## 9.7 Error injection

9.6 Error recording

The component that detects an error is called a node. The Neoverse™ N1 core is a node that
interacts with the DynamIQ™ Shared Unit node. There is one record per node for the errors
detected.

For more information on error recording that is generated by cache protection, see the Arm®
Architecture Reference Manual Supplement, Reliability, Availability, and Serviceability (RAS), for A-proﬁle
architecture. The following points apply speciﬁcally to the Neoverse™ N1 core:

- In the Neoverse™ N1 core, any error that is detected is reported and recorded in the error
record registers:

- 13.43 ERRSELR_EL1, Error Record Select Register, EL1 on page 166

- 13.44 ERXADDR_EL1, Selected Error Record Address Register, EL1 on page 167

- 13.45 ERXCTLR_EL1, Selected Error Record Control Register, EL1 on page 167

- 13.46 ERXFR_EL1, Selected Error Record Feature Register, EL1 on page 167

- 13.47 ERXMISC0_EL1, Selected Error Record Miscellaneous Register 0, EL1 on page 167

- 13.48 ERXMISC1_EL1, Selected Error Record Miscellaneous Register 1, EL1 on page 168

- 13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault Generation Count Down Register,
EL1 on page 168

- 13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault Generation Control Register, EL1 on
page 169

- 13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation Feature Register, EL1 on page
171

- 13.52 ERXSTATUS_EL1, Selected Error Record Primary Status Register, EL1 on page 172

- There are two error records provided, which can be selected with the ERRSELR_EL1 register:

- Record 0 is private to the core, and is updated on any error in the core RAMs including L1
caches, TLB, and L2 cache.

- Record 1 records any error in the L3 and snoop ﬁlter RAMs and is shared between all cores
in the cluster.

- The fault handling interrupt is generated on the nFAULTIRQ[0] pin for L3 and snoop ﬁlter
errors, or on the nFAULTIRQ[n+1] pin for core n L1 and L2 errors.

The Neoverse™ N1 core supports fault injection for the purpose of testing fault handling software.

The core is programmable to inject an error for any of the possible error types (corrected error,
deferred error, uncontainable error, and recoverable error) on a future memory access. When that
access is performed, the core responds as if an error was detected on that access by asserting error
interrupts, logging information in the error records, and taking aborts as appropriate for the type of
error. Injecting an error will not aﬀect the data in the RAM or the checking process itself. When a

real error is detected on an access for which an injected error is programmed, the injected error will
not prevent the core from handling the real error. The RAS register might log the injected error or
the real error in this case.

To get the error injection to work:

- Program the Error Record Select Register (ERRSELR_EL1) to select Error record 0.

- Program the Error Record Control Register (ERR0CTLR) to enable error detection/recovery and
fault detection.

- Program the Error Pseudo Fault Generation Control Register (ERR0PFGCTL) to allow error
injection.

Cacheable code must also be executed, which will cause cacheable transactions that
can be injected with errors.

The following table describes all the possible types of error that the core can encounter and
therefore inject.

Table 9-3: Errors injected in the Neoverse™ N1 core

The following table describes the registers that handle error injection in the Neoverse™ N1 core.

Table 9-4: Error injection registers

This mechanism simulates the corruption of any RAM but the data is not actually
corrupted.

See also:

- 14.7 ERR0PFGCDN, Error Pseudo Fault Generation Count Down Register on page 262.

- 14.8 ERR0PFGCTL, Error Pseudo Fault Generation Control Register on page 263.

|Error type|Description|
|---|---|
|Corrected errors|A corrected error is generated for a single-bit ECC error on L1 data caches and L2 caches, both on data and tag<br>RAMs.|
|Deferred errors|A deferred error is generated for a double-bit ECC error on L1 data caches and L2 caches, but only on data RAM.|
|Uncontainable<br>errors|An uncontainable error is generated for a double-bit ECC error on L1 data caches and L2 caches, but only on tag<br>RAM.|

|Register name|Description|
|---|---|
|ERR0PFGF|The ERR Pseudo Fault Generation Feature register deﬁnes which errors can be injected.|
|ERR0PFGCTL|The ERR Pseudo Fault Generation Control register controls the errors that are injected.|
|ERR0PFGCDN|The ERR Pseudo Fault Generation Count Down register controls the fault injection timing.|

- 14.9 ERR0PFGF, Error Pseudo Fault Generation Feature Register on page 265.

# 10. Generic Interrupt Controller CPU interface

## 10.1 About the Generic Interrupt Controller CPU interface

10. Generic Interrupt Controller CPU
interface

This chapter describes the Neoverse™ N1 core implementation of the Arm Generic Interrupt
Controller (GIC) CPU interface.

The Neoverse™ N1 core implements the GIC CPU interface as described in the Arm® Generic
Interrupt Controller Architecture Speciﬁcation.

This interfaces with an external GICv4 distributor component within the cluster system and is a
resource for supporting and managing interrupts. The GIC CPU interface hosts registers to mask,
identify, and control states of interrupts forwarded to that core. Each core in the cluster system has
a GIC CPU interface component and connects to a common external distributor component.

This chapter describes only features that are speciﬁc to the Neoverse™ N1 core
implementation. Additional information speciﬁc to the cluster can be found in  Arm®
DynamIQ™ Shared Unit Technical Reference Manual.

The GICv4 architecture supports:

- Two Security states.

- Interrupt virtualization.

- Software-generated Interrupts (SGIs).

- Message-Based Interrupts.

- System register access for the CPU interface.

- Interrupt masking and prioritization.

- Cluster environments, including systems that contain more than eight cores.

- Wake-up events in power management environments.

- Control whether deactivation of virtual SGIs can increment ICH_HCR_EL2.EOI count.

The GIC includes interrupt grouping functionality that supports:

- Conﬁguring each interrupt to belong to an interrupt group.

- Signaling Group 1 interrupts to the target core using either the IRQ or the FIQ exception
request. Group 1 interrupts can be Secure or Non-secure.

- Signaling Group 0 interrupts to the target core using the FIQ exception request only.

- A uniﬁed scheme for handling the priority of Group 0 and Group 1 interrupts.

## 10.2 Bypassing the CPU interface

This chapter describes only features that are speciﬁc to the Neoverse™ N1 core implementation.

Related information
GIC registers on page 270

The GIC CPU interface is always implemented within the Neoverse™ N1 core.

However, you can disable it if you assert the GICCDISABLE signal HIGH at reset. If you disable the
GIC CPU interface, the input pins nVIRQ and nVFIQ can be driven by an external GIC in the SoC.
GIC System register access generates UNDEFINED instruction exceptions when the GICCDISABLE
signal is HIGH.

If the GIC is enabled, the input pins nVIRQ and nVFIQ must be tied oﬀ to HIGH. This is because
the internal GIC CPU interface generates the virtual interrupt signals to the cores. The nIRQ and
nFIQ signals are controlled by software, therefore there is no requirement to tie them HIGH.

# 11. Advanced SIMD and floating-point support

## 11.1 About the Advanced SIMD and floating-point support

## 11.2 Accessing the feature identification registers

11. Advanced SIMD and ﬂoating-point
support

This chapter describes the Advanced SIMD and ﬂoating-point features and registers in the
Neoverse™ N1 core. The unit in charge of handling the Advanced SIMD and ﬂoating-point features
is also referred to as the data engine in this manual.

11.1 About the Advanced SIMD and ﬂoating-point
support

The Neoverse™ N1 core supports the Advanced SIMD and scalar ﬂoating-point instructions in the
A64 instruction set and the Advanced SIMD and ﬂoating-point instructions in the A32 and T32
instruction sets.

The Neoverse™ N1 ﬂoating-point implementation:

- Does not generate ﬂoating-point exceptions.

- Implements all scalar operations in hardware with support for all combinations of:

- Rounding modes.

- Flush-to-zero.

- Default Not a Number (NaN) modes.

The Arm®v8‑A architecture does not deﬁne a separate version number for its Advanced SIMD and
ﬂoating-point support in the AArch64 Execution state because the instructions are always implicitly
present.

11.2 Accessing the feature identiﬁcation registers

Software can identify the Advanced SIMD and ﬂoating-point features using the feature
identiﬁcation registers in the AArch64 Execution state only.

The Neoverse™ N1 core only supports AArch32 in EL0, therefore none of the feature identiﬁcation
registers are accessible in the AArch32 Execution state.

You can access the feature identiﬁcation registers in the AArch64 Execution state using the MRS
instruction, for example:

MRS <Xt>, ID_AA64PFR0_EL1 ; Read ID_AA64PFR0_EL1 into Xt
      MRS <Xt>, MVFR0_EL1       ; Read MVFR0_EL1 into Xt
      MRS <Xt>, MVFR1_EL1       ; Read MVFR1_EL1 into Xt
      MRS <Xt>, MVFR2_EL1       ; Read MVFR2_EL1 into Xt

Table 11-1: AArch64 Advanced SIMD and scalar ﬂoating-point feature identiﬁcation registers

|Register name|Description|
|---|---|
|ID_AA64PFR0_EL1|See13.67 ID_AA64PFR0_EL1, AArch64 Processor Feature Register 0, EL1 on page 190.|
|MVFR0_EL1|See16.4 MVFR0_EL1, Media, and VFP Feature Register 0, EL1 on page 302.|
|MVFR1_EL1|See16.5 MVFR1_EL1, Media, and VFP Feature Register 1, EL1 on page 303.|
|MVFR2_EL1|See16.6 MVFR2_EL1, Media, and VFP Feature Register 2, EL1 on page 305.|

# 12. AArch32 System registers

## 12.1 AArch32 architectural system register summary

This chapter describes the System registers in the AArch32 state.

This chapter identiﬁes the AArch32 architectural system registers implemented in the Neoverse™
N1 core.

The following table identiﬁes the architecturally deﬁned registers that are implemented in the
Neoverse™ N1 core. For a description of these registers see the Arm® Architecture Reference Manual
for A-proﬁle architecture.

For the registers listed in the following table, coproc==0b1111.

Table 12-1: Architecturally deﬁned registers

|Name|CRn|Opc1|CRm|Opc2|Width|description|
|---|---|---|---|---|---|---|
|CNTFRQ|c14|0|c0|0|32|Timer Clock Ticks per Second|
|CNTP_CTL|c14|0|c2|1|32|Counter-timer Physical Timer Control register|
|CNTP_CVAL|-|2|c14|-|64|Counter-timer Physical Timer CompareValue register|
|CNTP_TVAL|c14|0|c2|0|32|Counter-timer Physical Timer TimerValue register|
|CNTPCT|-|0|c14|-|64|Counter-timer Physical Count register|
|CNTV_CTL|c14|0|c3|1|32|Counter-timer Virtual Timer Control register|
|CNTV_CVAL|-|3|c14|-|64|Counter-timer Virtual Timer CompareValue register|
|CNTV_TVAL|c14|0|c3|0|32|Counter-timer Virtual Timer TimerValue register|
|CNTVCT|-|1|c14|-|64|Counter-timer Virtual Count register|
|CP15ISB|c7|0|c5|4|32|Instruction Synchronization Barrier System instruction|
|CP15DSB|c7|0|c10|4|32|Data Synchronization Barrier System instruction|
|CP15DMB|c7|0|c10|5|32|Data Memory Barrier System instruction|
|DLR|c4|3|c5|1|32|Debug Link Register|
|DSPSR|c4|3|c5|0|32|Debug Saved Program Status Register|
|TPIDRURO|c13|0|c0|3|32|User Read Only Thread ID Register|
|TPIDRURW|c13|0|c0|2|32|User Read/Write Thread ID Register|

# 13. AArch64 System registers

## 13.1 AArch64 registers

## 13.2 AArch64 architectural system register summary

This chapter describes the System registers in the AArch64 state.

13.1 AArch64 registers

This chapter provides information about the AArch64 System registers with IMPLEMENTATION

DEFINED bit ﬁelds and IMPLEMENTATION DEFINED registers associated with the core.

The chapter provides IMPLEMENTATION SPECIFIC information, for a complete description of the
registers, see the Arm® Architecture Reference Manual for A-proﬁle architecture.

The chapter is presented as follows:

AArch64 architectural System register summary

This section identiﬁes the AArch64 architectural System registers implemented in the
Neoverse™ N1 core that have IMPLEMENTATION DEFINED bit ﬁelds. The register descriptions
for these registers only contain information about the IMPLEMENTATION DEFINED bits.

AArch64 IMPLEMENTATION DEFINED register summary

This section identiﬁes the AArch64 architectural registers that are implemented in the
Neoverse™ N1 core that are IMPLEMENTATION DEFINED.

AArch64 registers by functional group

This section groups the IMPLEMENTATION DEFINED registers and architectural System registers
with IMPLEMENTATION DEFINED bit ﬁelds, as identiﬁed previously, by function. It also provides
reset details for key register types.

Register descriptions

The remainder of the chapter provides register descriptions of the IMPLEMENTATION DEFINED
registers and architectural system registers with IMPLEMENTATION DEFINED bit ﬁelds, as
identiﬁed previously. These are listed in alphabetic order.

This section describes the AArch64 architectural system registers implemented in the Neoverse™
N1 core.

The section contains two tables:

Registers with IMPLEMENTATION DEFINED bit ﬁelds

This table identiﬁes the architecturally deﬁned registers in Neoverse™ N1 that have

IMPLEMENTATION DEFINED bit ﬁelds. The register descriptions for these registers only contain
information about the IMPLEMENTATION DEFINED bits.

See Table 13-1: Registers with implementation deﬁned bit ﬁelds on page 94.

Other architecturally deﬁned registers

This table identiﬁes the other architecturally deﬁned registers that are implemented in the
Neoverse™ N1 core. These registers are described in the Arm® Architecture Reference Manual
for A-proﬁle architecture.

See Table 13-2: Other architecturally deﬁned registers on page 97.

Table 13-1: Registers with IMPLEMENTATION DEFINED bit ﬁelds

|Name|Op0|CRn|Op1|CRm|Op2|Width|Description|
|---|---|---|---|---|---|---|---|
|ACTLR_EL1|3|c1|0|c0|1|64|13.5 ACTLR_EL1, Auxiliary Control Register, EL1 on page 105|
|ACTLR_EL2|3|c1|4|c0|1|64|13.6 ACTLR_EL2, Auxiliary Control Register, EL2 on page 105|
|ACTLR_EL3|3|c1|6|c0|1|64|13.7 ACTLR_EL3, Auxiliary Control Register, EL3 on page 108|
|AIDR_EL1|3|c0|1|c0|7|64|13.14 AIDR_EL1, Auxiliary ID Register, EL1 on page 116|
|AFSR0_EL1|3|c5|0|c1|0|64|13.8 AFSR0_EL1, Auxiliary Fault Status Register 0, EL1 on page 111|
|AFSR0_EL2|3|c5|4|c1|0|64|13.9 AFSR0_EL2, Auxiliary Fault Status Register 0, EL2 on page 111|
|AFSR0_EL3|3|c5|6|c1|0|64|13.10 AFSR0_EL3, Auxiliary Fault Status Register 0, EL3 on page 112|
|AFSR1_EL1|3|c5|0|c1|1|64|13.11 AFSR1_EL1, Auxiliary Fault Status Register 1, EL1 on page 113|
|AFSR1_EL2|3|c5|4|c1|1|64|13.12 AFSR1_EL2, Auxiliary Fault Status Register 1, EL2 on page 114|
|AFSR1_EL3|3|c5|6|c1|1|64|13.13 AFSR1_EL3, Auxiliary Fault Status Register 1, EL3 on page 115|
|AMAIR_EL1|3|c10|0|c3|0|64|13.15 AMAIR_EL1, Auxiliary Memory Attribute Indirection Register, EL1 on<br>page 116|
|AMAIR_EL2|3|c10|4|c3|0|64|13.16 AMAIR_EL2, Auxiliary Memory Attribute Indirection Register, EL2 on<br>page 117|
|AMAIR_EL3|3|c10|6|c3|0|64|13.17 AMAIR_EL3, Auxiliary Memory Attribute Indirection Register, EL3 on<br>page 118|
|CCSIDR_EL1|3|c0|1|c0|0|64|13.23 CCSIDR_EL1, Cache Size ID Register, EL1 on page 128|
|CLIDR_EL1|3|c0|1|c0|1|64|13.24 CLIDR_EL1, Cache Level ID Register, EL1 on page 130|
|CPACR_EL1|3|c1|0|c0|2|64|13.25 CPACR_EL1, Architectural Feature Access Control Register, EL1 on<br>page 132|
|CPTR_EL2|3|c1|4|c1|2|64|13.26 CPTR_EL2, Architectural Feature Trap Register, EL2 on page 133|
|CPTR_EL3|3|c1|6|c1|2|64|13.27 CPTR_EL3, Architectural Feature Trap Register, EL3 on page 133|
|CSSELR_EL1|3|c0|2|c0|0|64|13.38 CSSELR_EL1, Cache Size Selection Register, EL1 on page 159|
|CTR_EL0|3|c0|3|c0|1|64|13.39 CTR_EL0, Cache Type Register, EL0 on page 160|
|DISR_EL1|3|c12|0|c1|1|64|13.41 DISR_EL1, Deferred Interrupt Status Register, EL1 on page 163|
|ERRIDR_EL1|3|c5|0|c3|0|64|13.42 ERRIDR_EL1, Error ID Register, EL1 on page 165|
|ERRSELR_EL1|3|c5|0|c3|1|64|13.43 ERRSELR_EL1, Error Record Select Register, EL1 on page 166|
|ERXADDR_EL1|3|c5|0|c4|3|64|13.44 ERXADDR_EL1, Selected Error Record Address Register, EL1 on page<br>167|
|ERXCTLR_EL1|3|c5|0|c4|1|64|13.45 ERXCTLR_EL1, Selected Error Record Control Register, EL1 on page<br>167|
|ERXFR_EL1|3|c5|0|c4|0|64|13.46 ERXFR_EL1, Selected Error Record Feature Register, EL1 on page<br>167|
|ERXMISC0_EL1|3|c5|0|c5|0|64|13.47 ERXMISC0_EL1, Selected Error Record Miscellaneous Register 0, EL1<br>on page 167|

|Name|Op0|CRn|Op1|CRm|Op2|Width|Description|
|---|---|---|---|---|---|---|---|
|ERXMISC1_EL1|3|c5|0|c5|1|64|13.48 ERXMISC1_EL1, Selected Error Record Miscellaneous Register 1, EL1<br>on page 168|
|ERXSTATUS_EL1|3|c5|0|c4|2|64|13.52 ERXSTATUS_EL1, Selected Error Record Primary Status Register, EL1<br>on page 172|
|ESR_EL1|3|c5|0|c2|0|64|13.53 ESR_EL1, Exception Syndrome Register, EL1 on page 172|
|ESR_EL2|3|c5|4|c2|0|64|13.54 ESR_EL2, Exception Syndrome Register, EL2 on page 174|
|ESR_EL3|3|c5|6|c2|0|64|13.55 ESR_EL3, Exception Syndrome Register, EL3 on page 175|
|HACR_EL2|3|c1|4|c1|7|64|13.56 HACR_EL2, Hyp Auxiliary Conﬁguration Register, EL2 on page 176|
|HCR_EL2|3|c1|4|c1|0|64|13.57 HCR_EL2, Hypervisor Conﬁguration Register, EL2 on page 177|
|ID_AFR0_EL1|3|c0|0|c1|3|64|13.69 ID_AFR0_EL1, AArch32 Auxiliary Feature Register 0, EL1 on page<br>193|
|ID_DFR0_EL1|3|c0|0|c1|2|64|13.70 ID_DFR0_EL1, AArch32 Debug Feature Register 0, EL1 on page<br>194|
|ID_ISAR0_EL1|3|c0|0|c2|0|64|13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on<br>page 196|
|ID_ISAR1_EL1|3|c0|0|c2|1|64|13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on<br>page 198|
|ID_ISAR2_EL1|3|c0|0|c2|2|64|13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on<br>page 200|
|ID_ISAR3_EL1|3|c0|0|c2|3|64|13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on<br>page 202|
|ID_ISAR4_EL1|3|c0|0|c2|4|64|13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on<br>page 204|
|ID_ISAR5_EL1|3|c0|0|c2|5|64|13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on<br>page 207|
|ID_ISAR6_EL1|3|c0|0|c2|7|64|13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on<br>page 209|
|ID_MMFR0_EL1|3|c0|0|c1|4|64|13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature Register 0, EL1 on<br>page 210|
|ID_MMFR1_EL1|3|c0|0|c1|5|64|13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature Register 1, EL1 on<br>page 212|
|ID_MMFR2_EL1|3|c0|0|c1|6|64|13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature Register 2, EL1 on<br>page 214|
|ID_MMFR3_EL1|3|c0|0|c1|7|64|13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature Register 3, EL1 on<br>page 216|
|ID_MMFR4_EL1|3|c0|0|c2|6|64|13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature Register 4, EL1 on<br>page 218|
|ID_PFR0_EL1|3|c0|0|c1|0|64|13.83 ID_PFR0_EL1, AArch32 Processor Feature Register 0, EL1 on page<br>220|
|ID_PFR1_EL1|3|c0|0|c1|1|64|13.84 ID_PFR1_EL1, AArch32 Processor Feature Register 1, EL1 on page<br>222|
|ID_AA64DFR0_EL1|3|c0|0|c5|0|64|13.60 ID_AA64DFR0_EL1, AArch64 Debug Feature Register 0, EL1 on page<br>180|
|ID_AA64ISAR0_EL1|3|c0|0|c6|0|64|13.62 ID_AA64ISAR0_EL1, AArch64 Instruction Set Attribute Register 0,<br>EL1 on page 182|

|Name|Op0|CRn|Op1|CRm|Op2|Width|Description|
|---|---|---|---|---|---|---|---|
|ID_AA64ISAR1_EL1|3|c0|0|c6|1|64|13.63 ID_AA64ISAR1_EL1, AArch64 Instruction Set Attribute Register 1,<br>EL1 on page 184|
|ID_AA64MMFR0_EL1|3|c0|0|c7|0|64|13.64 ID_AA64MMFR0_EL1, AArch64 Memory Model Feature Register 0,<br>EL1 on page 185|
|ID_AA64MMFR1_EL1|3|c0|0|c7|1|64|13.65 ID_AA64MMFR1_EL1, AArch64 Memory Model Feature Register 1,<br>EL1 on page 187|
|ID_AA64MMFR2_EL1|3|c0|0|c7|2|64|13.66 ID_AA64MMFR2_EL1, AArch64 Memory Model Feature Register 2,<br>EL1 on page 189|
|ID_AA64PFR0_EL1|3|c0|0|c4|0|64|13.67 ID_AA64PFR0_EL1, AArch64 Processor Feature Register 0, EL1 on<br>page 190|
|LORC_EL1|3|c10|0|c4|3|64|13.86 LORC_EL1, LORegion Control Register, EL1 on page 225|
|LORID_EL1|3|c10|0|c4|7|64|13.87 LORID_EL1, LORegion ID Register, EL1 on page 225|
|LORN_EL1|3|c10|0|c4|2|64|13.88 LORN_EL1, LORegion Number Register, EL1 on page 226|
|MDCR_EL3|3|c1|6|c3|1|64|13.89 MDCR_EL3, Monitor Debug Conﬁguration Register, EL3 on page<br>227|
|MIDR_EL1|3|c0|0|c0|0|64|13.90 MIDR_EL1, Main ID Register, EL1 on page 230|
|MPIDR_EL1|3|c0|0|c0|5|64|13.91 MPIDR_EL1, Multiprocessor Aﬃnity Register, EL1 on page 231|
|PAR_EL1|3|c7|0|c4|0|64|13.92 PAR_EL1, Physical Address Register, EL1 on page 233|
|RVBAR_EL3|3|c12|6|c0|1|64|13.95 RVBAR_EL3, Reset Vector Base Address Register, EL3 on page 235|
|REVIDR_EL1|3|c0|0|c0|6|64|13.93 REVIDR_EL1, Revision ID Register, EL1 on page 234|
|SCTLR_EL1|3|c1|0|c0|0|64|13.96 SCTLR_EL1, System Control Register, EL1 on page 236|
|SCTLR_EL2|3|c1|4|c0|0|64|13.97 SCTLR_EL2, System Control Register, EL2 on page 238|
|SCTLR_EL12|3|c1|5|c0|0|64|13.96 SCTLR_EL1, System Control Register, EL1 on page 236|
|SCTLR_EL3|3|c1|6|c0|0|64|13.98 SCTLR_EL3, System Control Register, EL3 on page 239|
|TCR_EL1|3|c2|0|c0|2|64|13.99 TCR_EL1, Translation Control Register, EL1 on page 241|
|TCR_EL2|3|c2|4|c0|2|64|13.100 TCR_EL2, Translation Control Register, EL2 on page 242|
|TCR_EL3|3|c2|6|c0|2|64|13.101 TCR_EL3, Translation Control Register, EL3 on page 243|
|TTBR0_EL1|3|c2|0|c0|0|64|13.102 TTBR0_EL1, Translation Table Base Register 0, EL1 on page 244|
|TTBR0_EL2|3|c2|4|c0|0|64|13.103 TTBR0_EL2, Translation Table Base Register 0, EL2 on page 246|
|TTBR0_EL3|3|c2|6|c0|0|64|13.104 TTBR0_EL3, Translation Table Base Register 0, EL3 on page 247|
|TTBR1_EL1|3|c2|0|c0|1|64|13.105 TTBR1_EL1, Translation Table Base Register 1, EL1 on page 248|
|TTBR1_EL2|3|c2|4|c0|1|64|13.106 TTBR1_EL2, Translation Table Base Register 1, EL2 on page 249|
|VDISR_EL2|3|c12|4|c1|1|64|13.107 VDISR_EL2, Virtual Deferred Interrupt Status Register, EL2 on page<br>249|
|VSESR_EL2|3|c5|4|c2|3|64|13.108 VSESR_EL2, Virtual SError Exception Syndrome Register on page<br>250|
|VTCR_EL2|3|c2|4|c1|2|64|13.109 VTCR_EL2, Virtualization Translation Control Register, EL2 on page<br>251|
|VTTBR_EL2|3|c2|4|c1|0|64|13.110 VTTBR_EL2, Virtualization Translation Table Base Register, EL2 on<br>page 252|

Table 13-2: Other architecturally deﬁned registers

|Name|Op0|CRn|Op1|CRm|Op2|Width|Description|
|---|---|---|---|---|---|---|---|
|AFSR0_EL12|3|c5|5|1|0|64|Auxiliary Fault Status Register 0|
|AFSR1_EL12|3|c5|5|1|1|64|Auxiliary Fault Status Register 1|
|AMAIR_EL12|3|c10|5|c3|0|64|Auxiliary Memory Attribute Indirection Register|
|CNTFRQ_EL0|3|c14|3|0|0|64|Counter-timer Frequency register|
|CNTHCTL_EL2|3|c14|4|c1|0|64|Counter-timer Hypervisor Control register|
|CNTHP_CTL_EL2|3|c14|4|c2|1|64|Counter-timer Hypervisor Physical Timer Control register|
|CNTHP_CVAL_EL2|3|c14|4|c2|2|64|Counter-timer Hyp Physical CompareValue register|
|CNTHP_TVAL_EL2|3|c14|4|c2|0|64|Counter-timer Hyp Physical Timer TimerValue register|
|CNTHV_CTL_EL2|3|c14|4|c3|1|64|Counter-timer Virtual Timer Control register|
|CNTHV_CVAL_EL2|3|c14|4|c3|2|64|Counter-timer Virtual Timer CompareValue register|
|CNTHV_TVAL_EL2|3|c14|4|c3|0|64|Counter-timer Virtual Timer TimerValue register|
|CNTKCTL_EL1|3|c14|0|c1|0|64|Counter-timer Kernel Control register|
|CNTKCTL_EL12|3|c14|5|c1|0|64|Counter-timer Kernel Control register|
|CNTP_CTL_EL0|3|c14|3|c2|1|64|Counter-timer Physical Timer Control register|
|CNTP_CTL_EL02|3|c14|5|c2|1|64|Counter-timer Physical Timer Control register|
|CNTP_CVAL_EL0|3|c14|3|c2|2|64|Counter-timer Physical Timer CompareValue register|
|CNTP_CVAL_EL02|3|c14|5|c2|2|64|Counter-timer Physical Timer CompareValue register|
|CNTP_TVAL_EL0|3|c14|3|c2|0|64|Counter-timer Physical Timer TimerValue register|
|CNTP_TVAL_EL02|3|c14|5|c2|0|64|Counter-timer Physical Timer TimerValue register|
|CNTPCT_EL0|3|c14|3|c0|1|64|Counter-timer Physical Count register|
|CNTPS_CTL_EL1|3|c14|7|c2|1|64|Counter-timer Physical Secure Timer Control register|
|CNTPS_CVAL_EL1|3|c14|7|c2|2|64|Counter-timer Physical Secure Timer CompareValue register|
|CNTPS_TVAL_EL1|3|c14|7|c2|0|64|Counter-timer Physical Secure Timer TimerValue register|
|CNTV_CTL_EL0|3|c14|3|c3|1|64|Counter-timer Virtual Timer Control register|
|CNTV_CTL_EL02|3|c14|5|c3|1|64|Counter-timer Virtual Timer Control register|
|CNTV_CVAL_EL0|3|c14|3|c3|2|64|Counter-timer Virtual Timer CompareValue register|
|CNTV_CVAL_EL02|3|c14|5|c3|2|64|Counter-timer Virtual Timer CompareValue register|
|CNTV_TVAL_EL0|3|c14|3|c3|0|64|Counter-timer Virtual Timer TimerValue register|
|CNTV_TVAL_EL02|3|c14|5|c3|0|64|Counter-timer Virtual Timer TimerValue register|
|CNTVCT_EL0|3|c14|3|c0|2|64|Counter-timer Virtual Count register|
|CNTVOFF_EL2|3|c14|4|c0|3|64|Counter-timer Virtual Oﬀset register|
|CONTEXTIDR_EL1|3|c13|0|c0|1|64|Context ID Register (EL1)|
|CONTEXTIDR_EL12|3|c13|5|c0|1|64|Context ID Register (EL12)|
|CONTEXTIDR_EL2|3|c13|4|c0|1|64|Context ID Register (EL2)|
|CPACR_EL12|3|c1|5|c0|2|64|Architectural Feature Access Control Register|
|CPTR_EL3|3|c1|6|c1|2|64|Architectural Feature Trap Register (EL3)|
|ESR_EL12|3|c5|5|c2|0|64|Exception Syndrome Register (EL12)|
|FAR_EL1|3|c6|0|c0|0|64|Fault Address Register (EL1)|
|FAR_EL12|3|c6|5|c0|0|64|Fault Address Register (EL12)|

|Name|Op0|CRn|Op1|CRm|Op2|Width|Description|
|---|---|---|---|---|---|---|---|
|FAR_EL2|3|c6|4|c0|0|64|Fault Address Register (EL2)|
|FAR_EL3|3|c6|6|c0|0|64|Fault Address Register (EL3)|
|HPFAR_EL2|3|c6|4|c0|4|64|Hypervisor IPA Fault Address Register|
|HSTR_EL2|3|c1|4|c1|3|64|Hypervisor System Trap Register|
|ID_AA64AFR0_EL1|3|c0|0|c5|4|64|AArch64 Auxiliary Feature Register 0|
|ID_AA64AFR1_EL1|3|c0|0|c5|5|64|AArch64 Auxiliary Feature Register 1|
|ID_AA64DFR1_EL1|3|c0|0|c5|1|64|AArch64 Debug Feature Register 1|
|ID_AA64PFR1_EL1|3|c0|0|c4|1|64|AArch64 Core Feature Register 1|
|ISR_EL1|3|c12|0|c1|0|64|Interrupt Status Register|
|LOREA_EL1|3|c10|0|c4|1|64|LORegion End Address Register|
|LORSA_EL1|3|c10|0|c4|0|64|LORegion Start Address Register|
|MAIR_EL1|3|c10|0|c2|0|64|Memory Attribute Indirection Register (EL1)|
|MAIR_EL12|3|c10|5|c2|0|64|Memory Attribute Indirection Register (EL12)|
|MAIR_EL2|3|c10|4|c2|0|64|Memory Attribute Indirection Register (EL2)|
|MAIR_EL3|3|c10|6|c2|0|64|Memory Attribute Indirection Register (EL3)|
|MDCR_EL2|3|c1|4|c1|1|64|Monitor Debug Conﬁguration Register|
|MVFR0_EL1|3|c0|0|c3|0|64|AArch32 Media and VFP Feature Register 0|
|MVFR1_EL1|3|c0|0|c3|1|64|AArch32 Media and VFP Feature Register 1|
|MVFR2_EL1|3|c0|0|c3|2|64|AArch32 Media and VFP Feature Register 2|
|RMR_EL3|3|c12|6|c0|2|64|Reset Management Register|
|SCR_EL3|3|c1|6|c1|0|64|Secure Conﬁguration Register|
|TCR_EL12|3|c2|5|c0|2|64|Translation Control Register (EL12)|
|TPIDR_EL0|3|c13|3|c0|2|64|EL0 Read/Write Software Thread ID Register|
|TPIDR_EL1|3|c13|0|c0|4|64|EL1 Software Thread ID Register|
|TPIDR_EL2|3|c13|4|c0|2|64|EL2 Software Thread ID Register|
|TPIDR_EL3|3|c13|6|c0|2|64|EL3 Software Thread ID Register|
|TPIDRRO_EL0|3|c13|3|c0|3|64|EL0 Read-Only Software Thread ID Register|
|TTBR0_EL12|3|c2|5|c0|0|64|Translation Table Base Register 0 (EL12)|
|TTBR1_EL12|3|c2|5|c0|1|64|Translation Table Base Register 1 (EL12)|
|VBAR_EL1|3|c12|0|c0|0|64|Vector Base Address Register (EL1)|
|VBAR_EL12|3|c12|5|c0|0|64|Vector Base Address Register (EL12)|
|VBAR_EL2|3|c12|4|c0|0|64|Vector Base Address Register (EL2)|
|VBAR_EL3|3|c12|6|c0|0|64|Vector Base Address Register (EL3)|
|VMPIDR_EL2|3|c0|4|c0|5|64|Virtualization Multiprocessor ID Register|
|VPIDR_EL2|3|c0|4|c0|0|64|Virtualization Core ID Register|

## 13.3 AArch64 IMPLEMENTATION DEFINED register summary

13.3 AArch64 IMPLEMENTATION DEFINED register
summary

This section describes the AArch64 registers in the Neoverse™ N1 core that are IMPLEMENTATION
DEFINED.

The following tables lists the AArch 64 IMPLEMENTATION DEFINED registers, sorted by opcode.

Table 13-3: AArch64 IMPLEMENTATION DEFINED registers

|Name|Copro|CRn|Op1|CRm|Op2|Width|Description|
|---|---|---|---|---|---|---|---|
|ATCR_EL1|3|c15|0|c7|0|64|13.18 ATCR_EL1, Auxiliary Translation Control Register, EL1 on page<br>1191|
|ATCR_EL2|3|c15|4|c7|0|64|13.19 ATCR_EL2, Auxiliary Translation Control Register, EL2 on page 121|
|ATCR_EL3|3|c15|6|c7|0|64|13.21 ATCR_EL3, Auxiliary Translation Control Register, EL3 on page 125|
|ATCR_EL12|3|c15|5|c7|0|64|13.20 ATCR_EL12, Alias to Auxiliary Translation Control Register EL1 on<br>page 124|
|AVTCR_EL2|3|c15|4|c7|1|64|13.22 AVTCR_EL2, Auxiliary Virtualized Translation Control Register, EL2 on<br>page 126|
|CPUACTLR_EL1|3|c15|0|c1|0|64|13.28 CPUACTLR_EL1, CPU Auxiliary Control Register, EL1 on page 135|
|CPUACTLR2_EL1|3|c15|0|c1|1|64|13.29 CPUACTLR2_EL1, CPU Auxiliary Control Register 2, EL1 on page<br>136|
|CPUACTLR3_EL1|3|c15|0|c1|2|64|13.30 CPUACTLR3_EL1, CPU Auxiliary Control Register 3, EL1 on page<br>138|
|CPUCFR_EL1|3|c15|0|c0|0|64|13.31 CPUCFR_EL1, CPU Conﬁguration Register, EL1 on page 139|
|CPUECTLR_EL1|3|c15|0|c1|4|64|13.32 CPUECTLR_EL1, CPU Extended Control Register, EL1 on page 141|
|CPUPCR_EL3|3|15|6|c8|1|64|13.33 CPUPCR_EL3, CPU Private Control Register, EL3 on page 151|
|CPUPMR_EL3|3|c15|6|c8|3|64|13.34 CPUPMR_EL3, CPU Private Mask Register, EL3 on page 152|
|CPUPOR_EL3|3|c15|6|c8|2|64|13.35 CPUPOR_EL3, CPU Private Operation Register, EL3 on page 153|
|CPUPSELR_EL3|3|c15|6|c8|0|64|13.36 CPUPSELR_EL3, CPU Private Selection Register, EL3 on page 155|
|CPUPWRCTLR_EL1|3|c15|0|c2|7|64|13.37 CPUPWRCTLR_EL1, Power Control Register, EL1 on page 156|
|ERXPFGCDN_EL1|3|c15|0|c2|2|64|13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault Generation Count<br>Down Register, EL1 on page 168|
|ERXPFGCTL_EL1|3|c15|0|c2|1|64|13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault Generation Control<br>Register, EL1 on page 169|
|ERXPFGF_EL1|3|c15|0|c2|0|64|13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation Feature Register,<br>EL1 on page 171|

## 13.4 AArch64 registers by functional group

This section identiﬁes the AArch64 registers by their functional groups and applies to the registers
in the core that are IMPLEMENTATION DEFINED or have micro-architectural bit ﬁelds. Reset values are
provided for these registers.

Identiﬁcation registers

|Name|Type|Reset|Description|
|---|---|---|---|
|AIDR_EL1|RO|`0x00000000`|13.14 AIDR_EL1, Auxiliary ID Register, EL1 on<br>page 116|
|CCSIDR__EL1|RO|-|13.23 CCSIDR_EL1, Cache Size ID Register, EL1<br>on page 128|
|CLIDR_EL1|RO|•<br>`0xC3000123` if L3 cache present.<br>•<br>`0x82000023` if no L3 cache.|13.24 CLIDR_EL1, Cache Level ID Register, EL1<br>on page 130|
|CSSELR_EL1|RW|UNK|13.38 CSSELR_EL1, Cache Size Selection<br>Register, EL1 on page 159|
|CTR_EL0|RO|`0x8444C004`|13.39 CTR_EL0, Cache Type Register, EL0 on<br>page 160|
|DCZID_EL0|RO|`0x00000004`|13.40 DCZID_EL0, Data Cache Zero ID<br>Register, EL0 on page 162|
|ERRIDR_EL1|RO|•<br>`0x00000002` if DSU SCU present.<br>•<br>`0x00000001` if no DSU SCU cache.|13.42 ERRIDR_EL1, Error ID Register, EL1 on<br>page 165|
|ID_AA64AFR0_EL1|RO|`0x00000000`|13.58 ID_AA64AFR0_EL1, AArch64 Auxiliary<br>Feature Register 0 on page 180|
|ID_AA64AFR1_EL1|RO|`0x00000000`|13.59 ID_AA64AFR1_EL1, AArch64 Auxiliary<br>Feature Register 1 on page 180|
|ID_AA64DFR0_EL1|RO|`0x0000000110305408`|13.60 ID_AA64DFR0_EL1, AArch64 Debug<br>Feature Register 0, EL1 on page 180|
|ID_AA64DFR1_EL1|RO|`0x00000000`|13.61 ID_AA64DFR1_EL1, AArch64 Debug<br>Feature Register 1, EL1 on page 182|
|ID_AA64ISAR0_EL1|RO|•<br>`0x0000100010211120` if the Cryptographic<br>Extension is implemented.<br>•<br>`0x0000100010210000` if the Cryptographic<br>Extension is not implemented.|13.62 ID_AA64ISAR0_EL1, AArch64 Instruction<br>Set Attribute Register 0, EL1 on page 182|
|ID_AA64ISAR1_EL1|RO|`0x0000000000100001`|13.63 ID_AA64ISAR1_EL1, AArch64 Instruction<br>Set Attribute Register 1, EL1 on page 184|
|ID_AA64MMFR0_EL1|RO|`0x0000000000101125`|13.64 ID_AA64MMFR0_EL1, AArch64 Memory<br>Model Feature Register 0, EL1 on page 185|
|ID_AA64MMFR1_EL1|RO|`0x0000000010212122`|13.65 ID_AA64MMFR1_EL1, AArch64 Memory<br>Model Feature Register 1, EL1 on page 187|
|ID_AA64MMFR2_EL1|RO|`0x0100000000001011`|13.66 ID_AA64MMFR2_EL1, AArch64 Memory<br>Model Feature Register 2, EL1 on page 189|
|ID_AA64PFR0_EL1|RO|•<br>`0x1100000010111112` if the GICv4 interface is<br>disabled.<br>•<br>`0x1100000013111112` if the GICv4 interface is<br>enabled.|13.67 ID_AA64PFR0_EL1, AArch64 Processor<br>Feature Register 0, EL1 on page 190|

|Name|Type|Reset|Description|
|---|---|---|---|
|ID_AA64PFR1_EL1|RO|`0x0000000000000020`|13.68 ID_AA64PFR1_EL1, AArch64 Processor<br>Feature Register 1, EL1 on page 192|
|ID_AFR0_EL1|RO|`0x00000000`|13.69 ID_AFR0_EL1, AArch32 Auxiliary Feature<br>Register 0, EL1 on page 193|
|ID_DFR0_EL1|RO|`0x04010088`|13.70 ID_DFR0_EL1, AArch32 Debug Feature<br>Register 0, EL1 on page 194|
|ID_ISAR0_EL1|RO|`0x02101110`|13.71 ID_ISAR0_EL1, AArch32 Instruction Set<br>Attribute Register 0, EL1 on page 196|
|ID_ISAR1_EL1|RO|`0x13112111`|13.72 ID_ISAR1_EL1, AArch32 Instruction Set<br>Attribute Register 1, EL1 on page 198|
|ID_ISAR2_EL1|RO|`0x21232042`|13.73 ID_ISAR2_EL1, AArch32 Instruction Set<br>Attribute Register 2, EL1 on page 200|
|ID_ISAR3_EL1|RO|`0x01112131`|13.74 ID_ISAR3_EL1, AArch32 Instruction Set<br>Attribute Register 3, EL1 on page 202|
|ID_ISAR4_EL1|RO|`0x00010142`|13.75 ID_ISAR4_EL1, AArch32 Instruction Set<br>Attribute Register 4, EL1 on page 204|
|ID_ISAR5_EL1|RO|`0x01011121`<br>ID_ISAR5 has the value`0x01010001` if the<br>Cryptographic Extension is not implemented and enabled.|13.76 ID_ISAR5_EL1, AArch32 Instruction Set<br>Attribute Register 5, EL1 on page 207|
|ID_ISAR6_EL1|RO|`0x00000010`|13.77 ID_ISAR6_EL1, AArch32 Instruction Set<br>Attribute Register 6, EL1 on page 209|
|ID_MMFR0_EL1|RO|`0x10201105`|13.78 ID_MMFR0_EL1, AArch32 Memory<br>Model Feature Register 0, EL1 on page 210|
|ID_MMFR1_EL1|RO|`0x40000000`|13.79 ID_MMFR1_EL1, AArch32 Memory<br>Model Feature Register 1, EL1 on page 212|
|ID_MMFR2_EL1|RO|`0x01260000`|13.80 ID_MMFR2_EL1, AArch32 Memory<br>Model Feature Register 2, EL1 on page 214|
|ID_MMFR3_EL1|RO|`0x02122211`|13.81 ID_MMFR3_EL1, AArch32 Memory<br>Model Feature Register 3, EL1 on page 216|
|ID_MMFR4_EL1|RO|`0x00021110`|13.82 ID_MMFR4_EL1, AArch32 Memory<br>Model Feature Register 4, EL1 on page 218|
|ID_PFR0_EL1|RO|`0x10010131`|13.83 ID_PFR0_EL1, AArch32 Processor<br>Feature Register 0, EL1 on page 220|
|ID_PFR1_EL1|RO|`0x10010000`<br>Bits [31:28] are`0x1` if the GIC CPU interface is<br>implemented and enabled, and`0x0` otherwise.|13.84 ID_PFR1_EL1, AArch32 Processor<br>Feature Register 1, EL1 on page 222|
|ID_PFR2_EL1|RO|`0x00000011`|13.85 ID_PFR2_EL1, AArch32 Processor<br>Feature Register 2, EL1 on page 224|
|LORID_EL1|RO|`0x0000000000040004`|13.87 LORID_EL1, LORegion ID Register, EL1<br>on page 225|
|MIDR_EL1|RO|`0x414FD0C1`|13.90 MIDR_EL1, Main ID Register, EL1 on<br>page 230|
|MPIDR_EL1|RO|The reset value depends on CLUSTERIDAFF2[7:0] and<br>CLUSTERIDAFF3[7:0]. See register description for details.|13.91 MPIDR_EL1, Multiprocessor Aﬃnity<br>Register, EL1 on page 231|
|REVIDR_EL1|RO|`0x00000000`|13.93 REVIDR_EL1, Revision ID Register, EL1<br>on page 234|

Other system control registers

Reliability, Availability, Serviceability (RAS) registers

Virtual Memory control registers

|Name|Type|Reset|Description|
|---|---|---|---|
|VMPIDR_EL2|RW|The reset value is the value of MPIDR_EL1.|Virtualization Multiprocessor ID Register EL2|
|VPIDR_EL2|RW|The reset value is the value of MIDR_EL1.|Virtualization Core ID Register EL2|

|Name|Type|Description|
|---|---|---|
|ACTLR_EL1|RW|13.5 ACTLR_EL1, Auxiliary Control Register, EL1 on page 105|
|ACTLR_EL2|RW|13.6 ACTLR_EL2, Auxiliary Control Register, EL2 on page 105|
|ACTLR_EL3|RW|13.7 ACTLR_EL3, Auxiliary Control Register, EL3 on page 108|
|CPACR_EL1|RW|13.25 CPACR_EL1, Architectural Feature Access Control Register, EL1 on page 132|
|SCTLR_EL1|RW|13.96 SCTLR_EL1, System Control Register, EL1 on page 236|
|SCTLR_EL2|RW|13.97 SCTLR_EL2, System Control Register, EL2 on page 238|
|SCTLR_EL3|RW|13.98 SCTLR_EL3, System Control Register, EL3 on page 239|
|SCTLR_EL12|RW|13.96 SCTLR_EL1, System Control Register, EL1 on page 236|

|Name|Type|Description|
|---|---|---|
|DISR_EL1|RW|13.41 DISR_EL1, Deferred Interrupt Status Register, EL1 on page 163|
|ERRIDR_EL1|RW|13.42 ERRIDR_EL1, Error ID Register, EL1 on page 165|
|ERRSELR_EL1|RW|13.43 ERRSELR_EL1, Error Record Select Register, EL1 on page 166|
|ERXADDR_EL1|RW|13.44 ERXADDR_EL1, Selected Error Record Address Register, EL1 on page 167|
|ERXCTLR_EL1|RW|13.45 ERXCTLR_EL1, Selected Error Record Control Register, EL1 on page 167|
|ERXFR_EL1|RO|13.46 ERXFR_EL1, Selected Error Record Feature Register, EL1 on page 167|
|ERXMISC0_EL1|RW|13.47 ERXMISC0_EL1, Selected Error Record Miscellaneous Register 0, EL1 on page 167|
|ERXMISC1_EL1|RW|13.48 ERXMISC1_EL1, Selected Error Record Miscellaneous Register 1, EL1 on page 168|
|ERXSTATUS_EL1|RW|13.52 ERXSTATUS_EL1, Selected Error Record Primary Status Register, EL1 on page 172|
|ERXPFGCDN_EL1|RW|13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault Generation Count Down Register, EL1 on page 168|
|ERXPFGCTL_EL1|RW|13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault Generation Control Register, EL1 on page 169|
|ERXPFGF_EL1|RO|13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation Feature Register, EL1 on page 171|
|HCR_EL2|RW|13.57 HCR_EL2, Hypervisor Conﬁguration Register, EL2 on page 177|
|VDISR_EL2|RW|13.107 VDISR_EL2, Virtual Deferred Interrupt Status Register, EL2 on page 249|
|VSESR_EL2|RW|13.108 VSESR_EL2, Virtual SError Exception Syndrome Register on page 250|

|Name|Type|Description|
|---|---|---|
|AMAIR_EL1|RW|13.15 AMAIR_EL1, Auxiliary Memory Attribute Indirection Register, EL1 on page 116|
|AMAIR_EL2|RW|13.16 AMAIR_EL2, Auxiliary Memory Attribute Indirection Register, EL2 on page 117|
|AMAIR_EL3|RW|13.17 AMAIR_EL3, Auxiliary Memory Attribute Indirection Register, EL3 on page 118|
|ATCR_EL1|RW|13.18 ATCR_EL1, Auxiliary Translation Control Register, EL1 on page 119|
|ATCR_EL2|RW|13.19 ATCR_EL2, Auxiliary Translation Control Register, EL2 on page 121|
|ATCR_EL12|RW|13.20 ATCR_EL12, Alias to Auxiliary Translation Control Register EL1 on page 124|

Virtualization registers

Exception and fault handling registers

|Name|Type|Description|
|---|---|---|
|ATCR_EL3|RW|13.21 ATCR_EL3, Auxiliary Translation Control Register, EL3 on page 125|
|AVTCR_EL2|RW|13.22 AVTCR_EL2, Auxiliary Virtualized Translation Control Register, EL2 on page 126|
|LORC_EL1|RW|13.86 LORC_EL1, LORegion Control Register, EL1 on page 225|
|LOREA_EL1|RW|LORegion End Address Register EL1|
|LORID_EL1|RO|13.87 LORID_EL1, LORegion ID Register, EL1 on page 225|
|LORN_EL1|RW|13.88 LORN_EL1, LORegion Number Register, EL1 on page 226|
|LORSA_EL1|RW|LORegion Start Address Register EL1|
|TCR_EL1|RW|13.99 TCR_EL1, Translation Control Register, EL1 on page 241|
|TCR_EL2|RW|13.100 TCR_EL2, Translation Control Register, EL2 on page 242|
|TCR_EL3|RW|13.101 TCR_EL3, Translation Control Register, EL3 on page 243|
|TTBR0_EL1|RW|13.102 TTBR0_EL1, Translation Table Base Register 0, EL1 on page 244|
|TTBR0_EL2|RW|13.103 TTBR0_EL2, Translation Table Base Register 0, EL2 on page 246|
|TTBR0_EL3|RW|13.104 TTBR0_EL3, Translation Table Base Register 0, EL3 on page 247|
|TTBR1_EL1|RW|13.105 TTBR1_EL1, Translation Table Base Register 1, EL1 on page 248|
|TTBR1_EL2|RW|13.106 TTBR1_EL2, Translation Table Base Register 1, EL2 on page 249|
|VTTBR_EL2|RW|13.110 VTTBR_EL2, Virtualization Translation Table Base Register, EL2 on page 252|

|Name|Type|Description|
|---|---|---|
|ACTLR_EL2|RW|13.6 ACTLR_EL2, Auxiliary Control Register, EL2 on page 105|
|AFSR0_EL2|RW|13.9 AFSR0_EL2, Auxiliary Fault Status Register 0, EL2 on page 111|
|AFSR1_EL2|RW|13.12 AFSR1_EL2, Auxiliary Fault Status Register 1, EL2 on page 114|
|AMAIR_EL2|RW|13.16 AMAIR_EL2, Auxiliary Memory Attribute Indirection Register, EL2 on page 117|
|CPTR_EL2|RW|13.26 CPTR_EL2, Architectural Feature Trap Register, EL2 on page 133|
|ESR_EL2|RW|13.54 ESR_EL2, Exception Syndrome Register, EL2 on page 174|
|HACR_EL2|RW|13.56 HACR_EL2, Hyp Auxiliary Conﬁguration Register, EL2 on page 176|
|HCR_EL2|RW|13.57 HCR_EL2, Hypervisor Conﬁguration Register, EL2 on page 177|
|HPFAR_EL2|RW|Hypervisor IPA Fault Address Register EL2|
|TCR_EL2|RW|13.100 TCR_EL2, Translation Control Register, EL2 on page 242|
|VMPIDR_EL2|RW|Virtualization Multiprocessor ID Register EL2|
|VPIDR_EL2|RW|Virtualization Core ID Register EL2|
|VSESR_EL2|RW|13.108 VSESR_EL2, Virtual SError Exception Syndrome Register on page 250|
|VTCR_EL2|RW|13.109 VTCR_EL2, Virtualization Translation Control Register, EL2 on page 251|
|VTTBR_EL2|RW|13.110 VTTBR_EL2, Virtualization Translation Table Base Register, EL2 on page 252|

|Name|Type|Description|
|---|---|---|
|AFSR0_EL1<br>|RW|13.8 AFSR0_EL1, Auxiliary Fault Status Register 0, EL1 on page 111|
|AFSR0_EL2<br>|RW|13.9 AFSR0_EL2, Auxiliary Fault Status Register 0, EL2 on page 111|
|AFSR0_EL3<br>|RW|13.10 AFSR0_EL3, Auxiliary Fault Status Register 0, EL3 on page 112|

IMPLEMENTATION DEFINED registers

Security

Reset management registers

|Name|Type|Description|
|---|---|---|
|AFSR1_EL1|RW|13.11 AFSR1_EL1, Auxiliary Fault Status Register 1, EL1 on page 113|
|AFSR1_EL2|RW|13.12 AFSR1_EL2, Auxiliary Fault Status Register 1, EL2 on page 114|
|AFSR1_EL3|RW|13.13 AFSR1_EL3, Auxiliary Fault Status Register 1, EL3 on page 115|
|DISR_EL1|RW|13.41 DISR_EL1, Deferred Interrupt Status Register, EL1 on page 163|
|ESR_EL1|RW|13.53 ESR_EL1, Exception Syndrome Register, EL1 on page 172|
|ESR_EL2|RW|13.54 ESR_EL2, Exception Syndrome Register, EL2 on page 174|
|ESR_EL3|RW|13.55 ESR_EL3, Exception Syndrome Register, EL3 on page 175|
|HPFAR_EL2|RW|Hypervisor IPA Fault Address Register EL2|
|VDISR_EL2|RW|13.107 VDISR_EL2, Virtual Deferred Interrupt Status Register, EL2 on page 249|
|VSESR_EL2|RW|13.108 VSESR_EL2, Virtual SError Exception Syndrome Register on page 250|

|Name|Type|Description|
|---|---|---|
|ATCR_EL1|RW|13.18 ATCR_EL1, Auxiliary Translation Control Register, EL1 on page 119|
|ATCR_EL2|RW|13.19 ATCR_EL2, Auxiliary Translation Control Register, EL2 on page 121|
|ATCR_EL3|RW|13.21 ATCR_EL3, Auxiliary Translation Control Register, EL3 on page 125|
|ATCR_EL12|RW|13.20 ATCR_EL12, Alias to Auxiliary Translation Control Register EL1 on page 124|
|AVTCR_EL2|RW|13.22 AVTCR_EL2, Auxiliary Virtualized Translation Control Register, EL2 on page 126|
|CPUACTLR_EL1|RW|13.28 CPUACTLR_EL1, CPU Auxiliary Control Register, EL1 on page 135|
|CPUACTLR2_EL1|RW|13.29 CPUACTLR2_EL1, CPU Auxiliary Control Register 2, EL1 on page 136|
|CPUACTLR3_EL1|RW|13.30 CPUACTLR3_EL1, CPU Auxiliary Control Register 3, EL1 on page 138|
|CPUCFR_EL1|RO|13.31 CPUCFR_EL1, CPU Conﬁguration Register, EL1 on page 139|
|CPUECTLR_EL1|RW|13.32 CPUECTLR_EL1, CPU Extended Control Register, EL1 on page 141|
|CPUPWRCTLR_EL1|RW|13.37 CPUPWRCTLR_EL1, Power Control Register, EL1 on page 156|
|ERXPFGCDN_EL1|RW|13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault Generation Count Down Register, EL1 on page 168|
|ERXPFGCTL_EL1|RW|13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault Generation Control Register, EL1 on page 169|
|ERXPFGF_EL1|RW|13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation Feature Register, EL1 on page 171|

|Name|Type|Description|
|---|---|---|
|ACTLR_EL3|RW|13.7 ACTLR_EL3, Auxiliary Control Register, EL3 on page 108|
|AFSR0_EL3|RW|13.10 AFSR0_EL3, Auxiliary Fault Status Register 0, EL3 on page 112|
|AFSR1_EL3|RW|13.13 AFSR1_EL3, Auxiliary Fault Status Register 1, EL3 on page 115|
|AMAIR_EL3|RW|13.17 AMAIR_EL3, Auxiliary Memory Attribute Indirection Register, EL3 on page 118|
|CPTR_EL3|RW|13.27 CPTR_EL3, Architectural Feature Trap Register, EL3 on page 133|
|MDCR_EL3|RW|13.89 MDCR_EL3, Monitor Debug Conﬁguration Register, EL3 on page 227|

|Name|Type|Description|
|---|---|---|
|RMR_EL3|RW|13.94 RMR_EL3, Reset Management Register on page 234|

## 13.5 ACTLR_EL1, Auxiliary Control Register, EL1

Address registers

ACTLR_EL1 provides IMPLEMENTATION DEFINED conﬁguration and control options for execution at
EL1 and EL0. This register is not used in the N1 core.

Bit ﬁeld descriptions
ACTLR_EL1 is a 64-bit register, and is part of:

- The Other system control registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-1: ACTLR_EL1 bit assignments

63
0

RES0

RES0, [63:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

|Name|Type|Description|
|---|---|---|
|RVBAR_EL3|RW|13.95 RVBAR_EL3, Reset Vector Base Address Register, EL3 on page 235|

|Name|Type|Description|
|---|---|---|
|PAR_EL1|RW|13.92 PAR_EL1, Physical Address Register, EL1 on page 233|

## 13.6 ACTLR_EL2, Auxiliary Control Register, EL2

The ACTLR_EL2 provides IMPLEMENTATION DEFINED conﬁguration and control options for EL2.

Bit ﬁeld descriptions
ACTLR_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group.

- The Other system control registers functional group.

- The IMPLEMENTATION DEFINED functional group.

This register resets to value 0x0000000000000000.

Figure 13-2: ACTLR_EL2 bit assignments

63
7
6
5
1
0
4
2

8
10
11
12
13

3

CLUSTERPMUEN

SMEN

PWREN

ERXPFGEN

AMEN

ECTLREN
ACTLREN

RES0

RES0, [63:13]

RES0
Reserved.

CLUSTERPMUEN, [12]

Performance Management Registers enable. The possible values are:

0
CLUSTERPM* registers are not write-accessible from a lower
Exception level. This is the reset value.
1
CLUSTERPM* registers are write-accessible from EL1 Non-secure if
they are write-accessible from EL2.

SMEN, [11]

Scheme Management Registers enable. The possible values are:

0
Registers CLUSTERACPSID, CLUSTERSTASHSID, CLUSTERPARTCR,
CLUSTERBUSQOS, and CLUSTERTHREADSIDOVR are not write-
accessible from EL1 Non-secure. This is the reset value.
1
Registers CLUSTERACPSID, CLUSTERSTASHSID, CLUSTERPARTCR,
CLUSTERBUSQOS, and CLUSTERTHREADSIDOVR are write-
accessible from EL1 Non-secure if they are write-accessible from
EL2.

RES0, [9:8]

RES0
Reserved.

PWREN, [7]

Power Control Registers enable. The possible values are:

0
Registers CPUPWRCTLR, CLUSTERPWRCTLR, CLUSTERPWRDN,
CLUSTERPWRSTAT, CLUSTERL3HIT and CLUSTERL3MISS are not
write-accessible from EL1 Non-secure. This is the reset value.
1
Registers CPUPWRCTLR, CLUSTERPWRCTLR, CLUSTERPWRDN,
CLUSTERPWRSTAT, CLUSTERL3HIT and CLUSTERL3MISS are
write-accessible from EL1 Non-secure if they are write-accessible
from EL2.

RES0, [6]

RES0
Reserved.

ERXPFGEN, [5]

Error Record Registers enable. The possible values are:

0
ERXPFG* are not write-accessible from EL1 Non-secure. This is the
reset value.
1
ERXPFG* are write-accessible from EL1 Non-secure if they are write-
accessible from EL2.

AMEN, [4]

Activity Monitor enable. The possible values are:

0
Non-secure accesses from EL1 and EL0 to activity monitor registers
are trapped to EL2. This is the reset value.
1
Non-secure accesses from EL1 and EL0 to activity monitor registers
are not trapped to EL2.

RES0, [3:2]

RES0
Reserved.

## 13.7 ACTLR_EL3, Auxiliary Control Register, EL3

ECTLREN, [1]

Extended Control Registers enable. The possible values are:

0
CPUECTLR and CLUSTERECTLR are not write-accessible from EL1
Non-secure. This is the reset value.
1
CPUECTLR and CLUSTERECTLR are write-accessible from EL1 Non-
secure if they are write-accessible from EL2.

ACTLREN, [0]

Auxiliary Control Registers enable. The possible values are:

0
CPUACTLR, CPUACTLR2, and CLUSTERACTLR are not write-
accessible from EL1 Non-secure. This is the reset value.
1
CPUACTLR, CPUACTLR2, and CLUSTERACTLR are write-accessible
from EL1 Non-secure if they are write-accessible from EL2.

Conﬁgurations

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The ACTLR_EL3 provides IMPLEMENTATION DEFINED conﬁguration and control options for EL3.

Bit ﬁeld descriptions
ACTLR_EL3 is a 64-bit register, and is part of:

- The Other system control registers functional group.

- The Security registers functional group.

- The IMPLEMENTATION DEFINED functional group.

This register resets to value 0x0000000000000000.

Figure 13-3: ACTLR_EL3 bit assignments

63
7
6
5
1
0
4
2

8
10
11
12
13

3

CLUSTERPMUEN

SMEN

PWREN

ERXPFGEN

AMEN

ECTLREN
ACTLREN

RES0

RES0, [63:13]

RES0
Reserved.

CLUSTERPMUEN, [12]

Performance Management Registers enable. The possible values are:

0
CLUSTERPM* registers are not write-accessible from a lower
Exception level. This is the reset value.
1
CLUSTERPM* registers are write-accessible from EL2 and EL1
Secure.

SMEN, [11]

Scheme Management Registers enable. The possible values are:

0
Registers CLUSTERACPSID, CLUSTERSTASHSID, CLUSTERPARTCR,
CLUSTERBUSQOS, and CLUSTERTHREADSIDOVR are not write-
accessible from EL2 and EL1 Secure. This is the reset value.
1
Registers CLUSTERACPSID, CLUSTERSTASHSID, CLUSTERPARTCR,
CLUSTERBUSQOS, and CLUSTERTHREADSIDOVR are write-
accessible from EL2 and EL1 Secure.

TSIDEN, [10]

Thread Scheme ID Register enable. The possible values are:

0
Register CLUSTERTHREADSID is not write-accessible from EL2 and
EL1 Secure. This is the reset value.
1
Register CLUSTERTHREADSID is write-accessible from EL2 and EL1
Secure.

RES0, [9:8]

RES0
Reserved.

PWREN, [7]

Power Control Registers enable. The possible values are:

0
Registers CPUPWRCTLR, CLUSTERPWRCTLR, CLUSTERPWRDN,
CLUSTERPWRSTAT, CLUSTERL3HIT and CLUSTERL3MISS are not
write-accessible from EL2 and EL1 Secure. This is the reset value.
1
Registers CPUPWRCTLR, CLUSTERPWRCTLR, CLUSTERPWRDN,
CLUSTERPWRSTAT, CLUSTERL3HIT and CLUSTERL3MISS are
write-accessible from EL2 and EL1 Secure.

RES0, [6]

RES0
Reserved.

ERXPFGEN, [5]

Error Record Registers enable. The possible values are:

0
ERXPFG* are not write-accessible from EL2 and EL1 Secure. This is
the reset value.
1
ERXPFG* are write-accessible from EL2 and EL1 Secure.

AMEN, [4]

Activity Monitor enable. The possible values are:

0
Accesses from EL2, EL1, and EL0 to activity monitor registers are
trapped to EL3. This is the reset value.
1
Accesses from EL2, EL1, and EL0 to activity monitor registers are not
trapped to EL3.

RES0, [3:2]

RES0
Reserved.

ECTLREN, [1]

Extended Control Registers enable. The possible values are:

0
CPUECTLR and CLUSTERECTLR are not write-accessible from EL2
and EL1 Secure. This is the reset value.
1
CPUECTLR and CLUSTERECTLR are write-accessible from EL2 and
EL1 Secure.

ACTLREN, [0]

Auxiliary Control Registers enable. The possible values are:

## 13.8 AFSR0_EL1, Auxiliary Fault Status Register 0, EL1

0
CPUACTLR, CPUACTLR2, and CLUSTERACTLR are not write-
accessible from EL2 and EL1 Secure. This is the reset value.
1
CPUACTLR, CPUACTLR2, and CLUSTERACTLR are write-accessible
from EL2 and EL1 Secure.

Conﬁgurations

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

AFSR0_EL1 provides additional IMPLEMENTATION DEFINED fault status information for exceptions
that are taken to EL1. In the Neoverse™ N1 core, no additional information is provided for these
exceptions. Therefore this register is not used.

Bit ﬁeld descriptions
AFSR0_EL1 is a 64-bit register, and is part of:

- The Exception and fault handling registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-4: AFSR0_EL1 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.9 AFSR0_EL2, Auxiliary Fault Status Register 0, EL2

## 13.10 AFSR0_EL3, Auxiliary Fault Status Register 0, EL3

13.9 AFSR0_EL2, Auxiliary Fault Status Register 0, EL2

AFSR0_EL2 provides extra IMPLEMENTATION DEFINED fault status information for exceptions that are
taken to EL2. In the N1 core, no additional information is provided for these exceptions. Therefore
this register is not used.

Bit ﬁeld descriptions
AFSR0_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group.

- The Exception and fault handling registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-5: AFSR0_EL2 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

AFSR0_EL3 provides additional IMPLEMENTATION DEFINED fault status information for exceptions
that are taken to EL3. In the Neoverse™ N1 core, no additional information is provided for these
exceptions. Therefore this register is not used.

Bit ﬁeld descriptions
AFSR0_EL3 is a 64-bit register, and is part of:

## 13.11 AFSR1_EL1, Auxiliary Fault Status Register 1, EL1

- The Exception and fault handling registers functional group.

- The Security registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-6: AFSR0_EL3 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

AFSR1_EL1 provides additional IMPLEMENTATION DEFINED fault status information for exceptions
that are taken to EL1. This register is not used in Neoverse™ N1.

Bit ﬁeld descriptions
AFSR1_EL1 is a 64-bit register, and is part of:

- The Exception and fault handling registers functional group.

- The IMPLEMENTATION DEFINED functional group.

## 13.12 AFSR1_EL2, Auxiliary Fault Status Register 1, EL2

Figure 13-7: AFSR1_EL1 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

AFSR1_EL2 provides additional IMPLEMENTATION DEFINED fault status information for exceptions
that are taken to EL2. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
AFSR1_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group.

- The Exception and fault handling registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-8: AFSR1_EL2 bit assignments

63
32

31

0

Reserved

RES0

## 13.13 AFSR1_EL3, Auxiliary Fault Status Register 1, EL3

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

AFSR1_EL3 provides additional IMPLEMENTATION DEFINED fault status information for exceptions
that are taken to EL3. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
AFSR1_EL3 is a 64-bit register, and is part of:

- The Exception and fault handling registers functional group.

- The Security registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-9: AFSR1_EL3 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

## 13.14 AIDR_EL1, Auxiliary ID Register, EL1

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

AIDR_EL1 provides IMPLEMENTATION DEFINED identiﬁcation information. This register is not used in
the Neoverse™ N1 core.

Bit ﬁeld descriptions
AIDR_EL1 is a 64-bit register, and is part of:

- The Identiﬁcation registers functional group.

- The IMPLEMENTATION DEFINED functional group.

This register is read-only.

Figure 13-10: >AIDR_EL1 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.15 AMAIR_EL1, Auxiliary Memory Attribute Indirection Register, EL1

## 13.16 AMAIR_EL2, Auxiliary Memory Attribute Indirection Register, EL2

13.15 AMAIR_EL1, Auxiliary Memory Attribute Indirection
Register, EL1

AMAIR_EL1 provides IMPLEMENTATION DEFINED memory attributes for the memory regions that are
speciﬁed by MAIR_EL1. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
AMAIR_EL1 is a 64-bit register, and is part of:

- The Virtual memory control registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-11: AMAIR_EL1 bit assignments

0
63

RES0

RES0, [63:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.16 AMAIR_EL2, Auxiliary Memory Attribute Indirection
Register, EL2

AMAIR_EL2 provides IMPLEMENTATION DEFINED memory attributes for the memory regions that are
speciﬁed by MAIR_EL2. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
AMAIR_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group.

- The Virtual memory control registers functional group.

- The IMPLEMENTATION DEFINED functional group.

## 13.17 AMAIR_EL3, Auxiliary Memory Attribute Indirection Register, EL3

Figure 13-12: AMAIR_EL1 bit assignments

0
63

RES0

RES0, [63:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.17 AMAIR_EL3, Auxiliary Memory Attribute Indirection
Register, EL3

AMAIR_EL3 provides IMPLEMENTATION DEFINED memory attributes for the memory regions that are
speciﬁed by MAIR_EL3. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
AMAIR_EL3 is a 64-bit register, and is part of:

- The Virtual memory control registers functional group.

- The Security registers functional group.

- The IMPLEMENTATION DEFINED functional group.

Figure 13-13: AMAIR_EL3 bit assignments

0
63

RES0

## 13.18 ATCR_EL1, Auxiliary Translation Control Register, EL1

RES0, [63:0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.18 ATCR_EL1, Auxiliary Translation Control Register,
EL1

The ATCR_EL1 determines the values of Page-Based Hardware Attributes (PBHA) on translation
table walks memory access in EL1 translation regime.

This register has no eﬀects unless PBHA is conﬁgured by the core.

Bit ﬁeld descriptions
ATCR_EL1 is a 64-bit register.

Figure 13-14: ATCR_EL1 bit assignments

63
14
0
1
2
3
4
5
6
7
8
9
10
11
12
13

HWVAL160
HWVAL159
HWVAL060
HWVAL059

HWEN059
HWEN060
HWEN159
HWEN160

RES0

RES0, [63:14]

RES0.

HWVAL160, [13]

Indicates the value of PBHA[1] on translation table walks memory
access targeting the base address deﬁned by TTBR1_EL1 if
HWEN160 is set.

HWVAL159, [12]

Indicates the value of PBHA[0] on translation table walks memory
access targeting the base address deﬁned by TTBR1_EL1 if
HWEN159 is set.

RES0, [11:10]

RES0.

HWVAL060, [9]

Indicates the value of PBHA[1] translation table walks memory
access targeting the base address deﬁned by TTBR0_EL1 if
HWEN060 is set.

HWVAL059, [8]

Indicates the value of PBHA[1] translation table walks memory
access targeting the base address deﬁned by TTBR0_EL1 if
HWEN059 is set.

RES0, [7:6]

RES0.

HWEN160, [5]

Enables PBHA[1] translation table walks memory access targeting the
base address deﬁned by TTBR1_EL1. If this bit is clear, PBHA[1] on
translation table walks is 0.

HWEN159, [4]

Enables PBHA[0] translation table walks memory access targeting the
base address deﬁned by TTBR1_EL1. If this bit is clear, PBHA[0] on
translation table walks is 0.

RES0, [3:2]

RES0.

HWEN060, [1]

Enables PBHA[1] translation table walks memory access targeting the
base address deﬁned by TTBR0_EL1. If this bit is clear, PBHA[1] on
translation table walks is 0.

HWEN059, [0]

Enables PBHA[0] translation table walks memory access targeting the
base address deﬁned by TTBR0_EL1. If this bit is clear, PBHA[0] on
translation table walks is 0.

Conﬁgurations

AArch64 register ATCR_EL1 is mapped to AArch32 register ATTBCR (NS).
At EL2 with HCR_EL2.E2H set, accesses to ATCR_EL1 are remapped to access ATCR_EL2.

Usage constraints

Accessing the ATCR_EL1

To access the ATCR_EL1:

MRS Xt , S< 3   0  c15   c7  0> ; Read ATCR_EL1 into Xt
MSR S < 3   0  c15   c7  0 > , Xt   ; Write Xt to ATCR_EL1

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

ATCR_EL1 is accessible as follows:

Control
Accessibility

E2H
TGE
NS
EL0
EL1
EL2
EL3

ATCR_EL1
x
x
0
-
RW
n/a
RW

ATCR_EL1
0
0
1
-
RW
RW
RW

ATCR_EL1
0
1
1
-
n/a
RW
RW

ATCR_EL1
1
0
1
-
RW
ATCR_EL2
RW

ATCR_EL1
1
1
1
-
n/a
ATCR_EL2
RW

ATCR_EL1 is also accessible using ATCR_EL12 when HCR.EL2.E2H is set. See
13.20 ATCR_EL12, Alias to Auxiliary Translation Control Register EL1 on page
124.

Traps and enables

Rules of traps and enables for this register are the same as TCR_EL1. See the Arm®
Architecture Reference Manual for A-proﬁle architecture.

|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|
|3|0|c15|c7|0|

## 13.19 ATCR_EL2, Auxiliary Translation Control Register, EL2

13.19 ATCR_EL2, Auxiliary Translation Control Register,
EL2

The ATCR_EL2 determines the values of Page-Based Hardware Attributes (PBHA) on translation
table walks memory access in EL2 translation regime.

This register is only used when PBHA is conﬁgured by the core.

Bit ﬁeld descriptions
ATCR_EL2 is a 64-bit register.

Figure 13-15: ATCR_EL2 bit assignments

63
14
0
1
2
3
4
5
6
7
8
9
10
11
12
13

HWVAL160
HWVAL159
HWVAL060
HWVAL059

HWEN059
HWEN060
HWEN159
HWEN160

RES0

RES0, [63:14]

RES0.

HWVAL160, [13]

Indicates the value of PBHA[1] on translation table walks memory
access targeting the base address deﬁned by TTBR1_EL2 if
HWEN160 is set.

HWVAL159, [12]

Indicates the value of PBHA[0] on translation table walks memory
access targeting the base address deﬁned by TTBR1_EL2 if
HWEN159 is set.

RES0, [11:10]

RES0.

HWVAL060, [9]

Indicates the value of PBHA[1] translation table walks memory
access targeting the base address deﬁned by TTBR0_EL2 if
HWEN060 is set.

HWVAL059, [8]

Indicates the value of PBHA[1] translation table walks memory
access targeting the base address deﬁned by TTBR0_EL2 if
HWEN059 is set.

RES0, [7:6]

RES0.

HWEN160, [5]

Enables PBHA[1] translation table walks memory access targeting the
base address deﬁned by TTBR1_EL2. If this bit is clear, PBHA[1] on
translation table walks is 0.

HWEN159, [4]

Enables PBHA[0] translation table walks memory access targeting the
base address deﬁned by TTBR1_EL2. If this bit is clear, PBHA[0] on
translation table walks is 0.

RES0, [3:2]

RES0.

HWEN060, [1]

Enables PBHA[1] translation table walks memory access targeting the
base address deﬁned by TTBR0_EL2. If this bit is clear, PBHA[1] on
translation table walks is 0.

HWEN059, [0]

Enables PBHA[0] translation table walks memory access targeting the
base address deﬁned by TTBR0_EL2. If this bit is clear, PBHA[0] on
translation table walks is 0.

Conﬁgurations

AArch64 ATCR_EL2 register is architecturally mapped to AArch32 register AHTCR.

Usage constraints

Accessing the ATCR_EL2

To access the ATCR_EL2:

MRS Xt, S< 3   4  c15   c7  0> ; Read ATCR_EL2 into Xt
MSR S < 3   4 c15   c7 0 > , Xt   ; Write Xt to ATCR_EL2

## 13.20 ATCR_EL12, Alias to Auxiliary Translation Control Register EL1

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

ATCR_EL2 is accessible as follows:

13.20 ATCR_EL12, Alias to Auxiliary Translation Control
Register EL1

The ATCR_EL12 alias allows access to ATCR_EL1 at EL2 or EL3 when HCR_EL2.E2H is set to 1.

This register is only used when Page-Based Hardware Attributes (PBHA) is conﬁgured by the core.

Usage constraints

Accessing the ATCR_EL12

To access the ATCR_EL1 using the ATCR_EL12 alias:

MRS Xt , S< 3   5  c15   c7  0> ; Read ATCR_EL12/ATCR_EL1 into Xt
MSR S < 3   5  c15   c7  0 > , Xt   ; Write Xt to ATCR_EL12/ATCR_EL1

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

ATCR_EL12 is accessible as follows:

Control
Accessibility

E2H
TGE
NS
EL0
EL1
EL2
EL3

ATCR_EL12
x
x
0
-
-
n/a
-

ATCR_EL12
0
0
1
-
-
-
-

ATCR_EL12
0
1
1
-
n/a
-
-

ATCR_EL12
1
0
1
-
-
ATCR_EL1
ATCR_EL1

ATCR_EL12
1
1
1
-
n/a
ATCR_EL1
ATCR_EL1

|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|
|3|4|c15|c7|0|

|EL0 (NS)|EL1 (NS)|EL1 (S)|EL2|EL3 (SCR.NS=1)|EL3 (SCR.NS=0)|
|---|---|---|---|---|---|
|-|-|-|RW|RW|RW|

|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|
|3|5|15|7|0|

## 13.21 ATCR_EL3, Auxiliary Translation Control Register, EL3

Traps and enables

All traps that are associated with the ATCR_EL1 register that apply at EL2 or EL3 also apply
to the ATCR_EL12 alias.

This alias is only accessible when HCR_EL2.E2H == 1.

When HCR_EL2.E2H == 0, access to this alias is UNDEFINED.

13.21 ATCR_EL3, Auxiliary Translation Control Register,
EL3

The ATCR_EL3 determines the values of Page-Based Hardware Attributes (PBHA) on translation
table walks memory access in EL3 translation regime.

This register is only used when PBHA is conﬁgured by the core.

Bit ﬁeld descriptions
ATCR_EL3 is a 64-bit register.

Figure 13-16: ATCR_EL3 bit assignments

63
0
1
2
7
8
9
10

HWVAL60

HWEN59

HWVAL59

HWEN60

res0

RES0, [63:10]

RES0.

HWVAL60, [9]

Indicates the value of PBHA[1] translation table walks memory
access if HWEN60 is set.

HWVAL59, [8]

Indicates the value of PBHA[1] translation table walks memory
access if HWEN59 is set.

RES0, [7:2]

RES0.

## 13.22 AVTCR_EL2, Auxiliary Virtualized Translation Control Register, EL2

HWEN60, [1]

Enables PBHA[1] translation table walks memory access. If this bit is
clear, PBHA[1] on translation table walks is 0.

HWEN59, [0]

Enables PBHA[0] translation table walks memory access. If this bit is
clear, PBHA[0] on translation table walks is 0.

Conﬁgurations

AArch64 register ATCR_EL3 is architecturally mapped to AArch32 register ATCR (S).

Usage constraints

Accessing the ATCR_EL3

To access the ATCR_EL3:

MRS Xt , < 3  6 c15  c7 0>  ; Read ATCR_EL3 into Xt
MSR S < 3   6 c15   c7 0 > , Xt   ; Write Xt to ATCR_EL3

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

ATCR_EL3 is accessible as follows:

13.22 AVTCR_EL2, Auxiliary Virtualized Translation
Control Register, EL2

The AVTCR_EL2 determines the values of Page-Based Hardware Attributes (PBHA) on stage 2
translation table walks memory access in EL1 Non-secure translation regime if stage 2 is enable.

This register is only used when PBHA is conﬁgured by the core.

Bit ﬁeld descriptions
AVTCR_EL2 is a 64-bit register.

|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|
|3|6|c15|c7|0|

|EL0|EL1 (NS)|EL1 (S)|EL2|EL3 (SCR.NS=1)|EL3 (SCR.NS=0)|
|---|---|---|---|---|---|
|-|-|-|-|RW|RW|

Figure 13-17: AVTCR_EL2 bit assignments

63
0
1
2
7
8
9
10

HWVAL60
HWVAL59

HWEN59
HWEN60

RES0

RES0, [63:10]

RES0.

HWVAL60, [9]

Indicates the value of PBHA[1] translation table walks memory
access if HWEN60 is set.

HWVAL59, [8]

Indicates the value of PBHA[1] translation table walks memory
access if HWEN59 is set.

RES0, [7:2]

RES0.

HWEN60, [1]

Enables PBHA[1] translation table walks memory access. If this bit is
clear, PBHA[1] on translation table walks is 0.

HWEN59, [0]

Enables PBHA[0] translation table walks memory access. If this bit is
clear, PBHA[0] on translation table walks is 0.

Conﬁgurations

AArch64 register AVTCR_EL2 is architecturally mapped to AArch32 register AVTCR.

Usage constraints

Accessing the AVTCR_EL2

To access the AVTCR_EL2:

MRS  Xt , S< 3  4  c15  c7  1> ; Read AVTCR_EL2 into Xt
MSR S < 3   4  c15   c7  1 > , Xt   ; Write Xt to AVTCR_EL2

## 13.23 CCSIDR_EL1, Cache Size ID Register, EL1

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

AVTCR_EL2 is accessible as follows:

The CCSIDR_EL1 provides information about the architecture of the currently selected cache.

Bit ﬁeld descriptions
CCSIDR_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-18: CCSIDR_EL1 bit assignments

63
32

31
28 27
12
3
0

30 29
13
2

NumSets
Associativity
Reserved

LineSize
WT

WA

WB

RA

Reserved, [63:32]

Reserved.

WT, [31]

Indicates whether the selected cache level supports Write-Through:

0
Cache Write-Through is not supported at any level.

For more information about encoding, see CCSIDR_EL1 encodings
on page 130.

|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|
|3|4|c15|c7|1|

|EL0|EL1 (NS)|EL1 (S)|EL2|EL3 (SCR.NS=1)|EL3 (SCR.NS=0)|
|---|---|---|---|---|---|
|-|-|-|RW|RW|RW|

WB, [30]

Indicates whether the selected cache level supports Write-Back. Permitted values are:

0
Write-Back is not supported.
1
Write-Back is supported.

For more information about encoding, see CCSIDR_EL1 encodings
on page 130.

RA, [29]

Indicates whether the selected cache level supports read-allocation. Permitted values are:

0
Read-allocation is not supported.
1
Read-allocation is supported.

For more information about encoding, see CCSIDR_EL1 encodings
on page 130.

WA, [28]

Indicates whether the selected cache level supports write-allocation. Permitted values are:

0
Write-allocation is not supported.
1
Write-allocation is supported.

For more information about encoding, see CCSIDR_EL1 encodings
on page 130.

NumSets, [27:13]

(Number of sets in cache) - 1. Therefore, a value of 0 indicates one set in the cache. The
number of sets does not have to be a power of 2.

For more information about encoding, see CCSIDR_EL1 encodings on page 130.

Associativity, [12:3]

(Associativity of cache) - 1. Therefore, a value of 0 indicates an associativity of 1. The
associativity does not have to be a power of 2.

For more information about encoding, see CCSIDR_EL1 encodings on page 130.

LineSize, [2:0]

(Log2(Number of bytes in cache line)) - 4. For example:

For a line length of 16 bytes: Log2(16) = 4, LineSize entry = 0. This is the minimum line
length.

For a line length of 32 bytes: Log2(32) = 5, LineSize entry = 1.

For more information about encoding, see CCSIDR_EL1 encodings on page 130.

## 13.24 CLIDR_EL1, Cache Level ID Register, EL1

Conﬁgurations

There are no conﬁguration notes.
Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

CCSIDR_EL1 encodings
The following table shows the individual bit ﬁeld and complete register encodings for the
CCSIDR_EL1.

Table 13-24: CCSIDR encodings

CSSELR
Register bit ﬁeld encoding

Cache
Size
Complete register encoding

Level
InD

WT WB RA WA NumSets Associativity LineSize

0b000
0b0
L1 Data cache
64KB
701FE01A
0
1
1
1
0x00FF
0x003
2

0b000
0b1
L1 Instruction cache 64KB
201FE01A
0
0
1
0
0x00FF
0x003
2

0b001
0b0
L2 cache

256KB
703FE03A
0
1
1
1
0x01FF
0x007
2

512KB
707FE03A
0
1
1
1
0x03FF
0x007
2

1024KB 70FFE03A
0
1
1
1
0x07FF
0x007
2

0b001
0b1
Reserved
-
-
-
-
-
-
-
-
-

0b010
0b0
Reserved
-
-
-
-
-
-
-
-
-

0b010
0b1
Reserved
-
-
-
-
-
-
-
-
-

0b0101 - 0b1111 Reserved
-
-
-
-
-
-
-
-
-

The CLIDR_EL1 identiﬁes the type of cache, or caches, which are implemented at each level, up to
a maximum of seven levels.

It also identiﬁes the Level of Coherency (LoC) and Level of Uniﬁcation (LoU) for the cache hierarchy.

Bit ﬁeld descriptions
CLIDR_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-19: CLIDR_EL1 bit assignments

30 29
27 26
24 23
21 20
9
8
6
5
3
2
0

63
32

33

LoUU
LoC
ICB

Ctype3
Ctype2
Ctype1

LoUIS

RES0

RES0, [63:33]

RES0
Reserved.

ICB, [32:30]

Inner cache boundary. This ﬁeld indicates the boundary between the inner and the outer
domain:

0b010
L2 cache is the highest inner level.
0b011
L3 cache is the highest inner level.

LoUU, [29:27]

Indicates the Level of Uniﬁcation Uniprocessor for the cache hierarchy:

0b000
No levels of cache need to be cleaned or invalidated when cleaning
or invalidating to the Point of Uniﬁcation. This is the value if no
caches are conﬁgured.

LoC, [26:24]

Indicates the Level of Coherency for the cache hierarchy:

0b010
L3 cache is not implemented.
0b011
L3 cache is implemented.

LoUIS, [23:21]

Indicates the Level of Uniﬁcation Inner Shareable (LoUIS) for the cache hierarchy.

0b000
No cache level needs cleaning to Point of Uniﬁcation.

RES0, [20:9]

No cache at levels L7 down to L4.

RES0
Reserved.

Ctype3, [8:6]

Indicates the type of cache if the core implements L3 cache. If present, uniﬁed instruction
and data caches at level 3:

0b100
Both per-core L2 and cluster L3 caches are present.
0b000
All other options.

If Ctype2 has a value of 0b000, then the value of Ctype3 must be IGNORED.

Ctype2, [5:3]

Indicates the type of uniﬁed instruction and data caches at level 2:

0b100
Either per-core L2 or cluster L2 cache is present.
0b000
All other options.

## 13.25 CPACR_EL1, Architectural Feature Access Control Register, EL1

Ctype1, [2:0]

Indicates the type of cache which is implemented at L1:

0b011
Separate instruction and data caches at L1.

Conﬁgurations

There are no conﬁguration notes.
Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.25 CPACR_EL1, Architectural Feature Access Control
Register, EL1

The CPACR_EL1 controls access to trace functionality and access to registers associated with
Advanced SIMD and ﬂoating-point execution.

Bit ﬁeld descriptions
CPACR_EL1 is a 64-bit register, and is part of the Other system control registers functional group.

Figure 13-20: CPACR_EL1 bit assignments

63
32

31
0

28

19
20
21
22

Reserved

TTA

FPEN

res0

Reserved, [63:32]

Reserved.

RES0, [31:29]

RES0
Reserved.

TTA, [28]

Traps EL0 and EL1 System register accesses to all implemented trace registers to EL1, from
both Execution states. This bit is RES0. The core does not provide System register access to
ETM control.

## 13.26 CPTR_EL2, Architectural Feature Trap Register, EL2

Conﬁgurations

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The CPTR_EL2 controls trapping to EL2 for accesses to CPACR, trace functionality and registers
associated with Advanced SIMD and ﬂoating-point execution. It also controls EL2 access to this
functionality.

Bit ﬁeld descriptions
CPTR_EL2 is a 64-bit register, and is part of the Virtualization registers functional group.

Figure 13-21: CPTR_EL2 bit assignments

63
32

31
0

13 12
14
30

20 19
21
10 9
11

Reserved

TFP
TCPAC

TTA

RES1

RES0

Reserved, [63:32]

Reserved.

TTA, [20]

Trap Trace Access.

This bit is not implemented. RES0.

Conﬁgurations

RW ﬁelds in this register reset to UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.27 CPTR_EL3, Architectural Feature Trap Register, EL3

The CPTR_EL3 controls trapping to EL3 of access to CPACR_EL1, CPTR_EL2, trace functionality
and registers associated with Advanced SIMD and ﬂoating-point execution.

It also controls EL3 access to trace functionality and registers associated with Advanced SIMD and
ﬂoating-point execution.

Bit ﬁeld descriptions
CPTR_EL3 is a 64-bit register, and is part of the Security registers functional group.

Figure 13-22: CPTR_EL3 bit assignments

63
32

31
0

10 9
11
30

19
20
21

Reserved

TCPAC

TTA

TFP

RES0

Reserved, [63:32]

Reserved.

TTA, [20]

Trap Trace Access.

Not implemented. RES0.

TFP, [10]

Traps all accesses to SVE, Advanced SIMD and ﬂoating-point functionality to EL3. This
applies to all Exception levels, both Security states, and both Execution states. The possible
values are:

0
Does not cause any instruction to be trapped. This is the reset value.
1
Any attempt at any Exception level to execute an instruction that
uses the registers that are associated with SVE, Advanced SIMD
and ﬂoating-point is trapped to EL3, subject to the exception
prioritization rules.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.28 CPUACTLR_EL1, CPU Auxiliary Control Register, EL1

13.28 CPUACTLR_EL1, CPU Auxiliary Control Register,
EL1

The CPUACTLR_EL1 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUACTLR_EL1 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers functional
group.

Figure 13-23: CPUACTLR_EL1 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUACTLR_EL1 is common to the Secure and Non-secure states.

Usage constraints

Accessing the CPUACTLR_EL1

The CPU Auxiliary Control Register can be written only when the system is idle. Arm
recommends that you write to this register after a Cold reset, before the MMU is enabled.

Setting many of these bits can cause signiﬁcantly lower performance on your code.
Therefore, Arm strongly recommends that you do not modify this register unless directed by
Arm.

This register is accessible as follows:

This register can be read with the MRS instruction using the following syntax:

MRS <Xt>,<systemreg>

This register can be written with the MSR instruction using the following syntax:

MSR <systemreg>, <Xt>

## 13.29 CPUACTLR2_EL1, CPU Auxiliary Control Register 2, EL1

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C1_0
x
x
0
-
RW
n/a
RW

S3_0_C15_C1_0
x
0
1
-
RW
RW
RW

S3_0_C15_C1_0
x
1
1
-
n/a
RW
RW

This register is write-accessible in EL1 on either of these conditions:

- ACTLR_EL3.CPUACTLR_EN ==1 && ACTLR_EL2.CPUACTLR_EN==1.

- ACTLR_EL3.CPUACTLR_EN==1 && SCR.NS==0.

This register is write-accessible in EL2 if ACTLR_EL3.CPUACTLR_EN==1.

If Write-Access is not possible, then trap to the lowest Exception level that denied the access (EL2
or EL3).

'n/a' Not accessible. The core cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Synchronous exception
prioritization in the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.29 CPUACTLR2_EL1, CPU Auxiliary Control Register 2,
EL1

The CPUACTLR2_EL1 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUACTLR2_EL1 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers
functional group.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_0_C15_C1_0|11|000|1111|0001|000|

Figure 13-24: CPUACTLR2_EL1 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUACTLR2_EL1 is common to the Secure and Non-secure states.

Usage constraints

Accessing the CPUACTLR2_EL1

The CPUACTLR2_EL1 can be written only when the system is idle. Arm recommends that
you write to this register after a powerup reset, before the MMU is enabled.

Setting many of these bits can cause signiﬁcantly lower performance on your code.
Therefore, Arm strongly recommends that you do not modify this register unless directed by
Arm.

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This register can be written using MSR with the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<syntax>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C1_1
x
x
0
-
RW
n/a
RW

S3_0_C15_C1_1
x
0
1
-
RW
RW
RW

S3_0_C15_C1_1
x
1
1
-
n/a
RW
RW

|<systemreg>|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|---|
|S3_0_C15_C1_1|11|000|1111|0001|001|

## 13.30 CPUACTLR3_EL1, CPU Auxiliary Control Register 3, EL1

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Exception priority order
in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions that are
taken to AArch64 state, and see Synchronous exception prioritization for exceptions that are
taken to AArch64 state.

Write-Access to this register from EL1 or EL2 depends on the value of bit[0] of ACTLR_EL2 and
ACTLR_EL3.

13.30 CPUACTLR3_EL1, CPU Auxiliary Control Register 3,
EL1

The CPUACTLR3_EL1 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUACTLR3_EL1 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers
functional group.

Figure 13-25: CPUACTLR3_EL1 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUACTLR3_EL1 is common to the Secure and Non-secure states.

Usage constraints

Accessing the CPUACTLR3_EL1

The CPUACTLR3_EL1 can be written only when the system is idle. Arm recommends that
you write to this register after a powerup reset, before the MMU is enabled.

Setting many of these bits can cause signiﬁcantly lower performance on your code.
Therefore, Arm strongly recommends that you do not modify this register unless directed by
Arm.

## 13.31 CPUCFR_EL1, CPU Configuration Register, EL1

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This register can be written using MSR with the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<syntax>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C1_2
x
x
0
-
RW
n/a
RW

S3_0_C15_C1_2
x
0
1
-
RW
RW
RW

S3_0_C15_C1_2
x
1
1
-
n/a
RW
RW

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Exception priority order
in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions that are
taken to AArch64 state, and see Synchronous exception prioritization for exceptions that are
taken to AArch64 state.

Write-Access to this register from EL1 or EL2 depends on the value of bit[0] of ACTLR_EL2 and
ACTLR_EL3.

13.31 CPUCFR_EL1, CPU Conﬁguration Register, EL1

The CPUCFR_EL1 provides conﬁguration information for the core.

Bit ﬁeld descriptions
CPUCFR_EL1 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers functional
group.

This register is read-only.

|<systemreg>|Op0|Op1|CRn|CRm|Op2|
|---|---|---|---|---|---|
|S3_0_C15_C1_2|11|000|1111|0001|010|

Figure 13-26: CPUCFR_EL1 bit assignments

31
0

63
32

1
3
2

ECC

Reserved

SCU

RES0

Reserved, [63:32]

Reserved.

RES0, [31:3]

Reserved, RES0.

SCU, [2]

Indicates whether the DSU SCU is present or not. The value is:

0
The DSU SCU is present.

1
The DSU SCU is not present. The DSU is conﬁgured without the SCU and L3, allowing
Neoverse™ N1 with a minimally conﬁgured DSU for Direct connect with the CMN-600
interconnect.

ECC, [1:0]

Indicates whether ECC is present or not. The possible values are:

00
ECC is not present.

01
ECC is present.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.32 CPUECTLR_EL1, CPU Extended Control Register, EL1

Usage constraints

Accessing the CPUCFR_EL1

This register can be read with the MRS instruction using the following syntax:

MRS <Xt>,<systemreg>

To access the CPUCFR_EL1:

MRS <Xt>, CPUCFR_EL1 ; Read CPUCFR_EL1 into Xt

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C0_0
x
x
0
-
RO
n/a
RO

S3_0_C15_C0_0
x
0
1
-
RO
RO
RO

S3_0_C15_C0_0
x
1
1
-
n/a
RO
RO

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

13.32 CPUECTLR_EL1, CPU Extended Control Register,
EL1

The CPUECTLR_EL1 provides additional IMPLEMENTATION DEFINED conﬁguration and control
options for the core.

Bit ﬁeld descriptions
CPUECTLR_EL1 is a 64-bit register, and is part of the 64-bit registers functional group.

This register resets to value 0x0000000961563010.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_0_C15_C0_0|11|000|1111|0000|000|

Figure 13-27: CPUECTLR_EL1 bit assignments

63
32
33

34
35
62 61 60 59
57
54
56
53 52
55
58
51
49
50
47 46
48
45 44

36
38 37
39
40
41
42
43

MXP_EN

ATOMIC_ACQ_NEAR

MXP_TP
MXP_ATHR

CA_EVICT_DIS
CA_UCLEAN_EVICT_EN

MM_VMID_THR

MM_ASP_EN

MM_CH_DIS

PFT_IF
PFT_LS
PFT_MM

MM_TLBPF_DIS

L2_FLUSH
HPA_MODE

HPA_CAP
HPA_L1_DIS

HPA_DIS

27 26
28
30 29
25
14 13 12

1
17
19 18
20
21
22
23
24

15
16

5
6
7
8
9
11

3
4

2

0
31

ATOMIC_ST_NEAR
ATOMIC_REL_NEAR

EXTLLC

RPF_PHIT_EN

ATOMIC_LD_NEAR

RPF_LO_CONF
RPF_DIS

TLD_PRED_DIS

DTLB_CABT_EN

PF_STS_DIS

WS_THR_L2
WS_THR_L3
WS_THR_L4
WS_THR_DRAM

PF_STI_DIS

PF_SS_L2_DIST

PF_DIS

WS_THR_DCZVA

RES0

RES0, [63:62]

RES0
Reserved.

MXP_EN, [61]

Max-power throttle enable. The possible values are:

0
Disables max-power throttling mechanism. This is the reset value.
1
Enables max-power throttling mechanism.

Both the MXP_EN bit and the MPMMEN input pin at the DSU cluster level must be
asserted to enable the max-power throttling mechanism.

RES0, [60:59]

RES0
Reserved.

MXP_TP, [58:57]

Percentage of throttling in the Load-Store and Vector Execute units during the period when
throttling has been triggered and is active. The possible values are:

00
Throttle by 60%. This is the reset value.
01
Throttle by 50%.
10
Throttle by 40%.
11
Throttle by 30%.

MXP_ATHR, [56:55]

Peak activity threshold at which max-power throttling is triggered. The possible values are:

00
Max-power throttling that is triggered at 70% of peak activity. This is
the reset value.
01
Max-power throttling that is triggered at 60% of peak activity.
10
Max-power throttling that is triggered at 50% of peak activity.
11
Max-power throttling that is triggered at 40% of peak activity.

MM_VMID_THR, [54]

VMID ﬁlter threshold. The possible values are:

0
Flush VMID ﬁlter after 16 unique VMID allocations to the MMU
Translation Cache. This is the reset value.
1
Flush VMID ﬁlter after 32 unique VMID allocations to the MMU
Translation Cache.

MM_ASP_EN, [53]

Disables allocation of splintered pages in L2 TLB. The possible values are:

0
Enables allocation of splintered pages in the L2 TLB. This is the reset
value.
1
Disables allocation of splintered pages in the L2 TLB.

MM_CH_DIS, [52]

Disables use of contiguous hint. The possible values are:

0
Enables use of contiguous hint. This is the reset value.
1
Disables use of contiguous hint.

MM_TLBPF_DIS, [51]

Disables L2 TLB prefetcher. The possible values are:

0
Enables L2 TLB prefetcher. This is the reset value.
1
Disables L2 TLB prefetcher.

HPA_MODE, [50:49]

Hardware Page Aggregation (HPA) mode. The possible values are:

00
Moderately conservative hardware page aggregation. This is the reset
value.
01
Aggressive hardware page aggregation.
10
Moderately aggressive hardware page aggregation.
11
Conservative hardware page aggregation.

HPA_CAP, [48]

Limited or full hardware page aggregation selection. The possible values are:

0
Limited hardware page aggregation. This is the reset value.

1
Full hardware page aggregation.

HPA_L1_DIS, [47]

Disables HPA in L1 TLBs (but continues to use HPA in L2 TLB). The possible values are:

0
Enables hardware page aggregation in L1 TLBs. This is the reset
value.
1
Disables hardware page aggregation in L1 TLBs.

HPA_DIS, [46]

Disables hardware page aggregation. The possible values are:

0
Enables hardware page aggregation. This is the reset value.
1
Disables hardware page aggregation.

RES0, [45:44]

RES0
Reserved.

L2_FLUSH, [43]

Allocation behavior of copybacks that are caused by L2 cache hardware ﬂush and DC CISW
instructions targeting the L2 cache. If it is known that data is likely to be used soon by
another core, setting this bit can improve system performance. The possible values are:

0
L2 cache ﬂushes and invalidates by set/way do not allocate in
the L3 cache. Cache lines in the UniqueDirty state cause Write-
Back transactions with the allocation hint cleared, while cache lines
in UniqueClean or SharedClean states cause address-only Evict
transactions. This is the reset value.
1
L2 cache ﬂushes by set/way allocate in the L3 cache. Cache lines
in the UniqueDirty or UniqueClean state cause WriteBackFull or
WriteEvictFull transactions, respectively, both with the allocation hint

set. Cache lines in the SharedClean state cause address-only Evict
transactions.

RES0, [42]

RES0
Reserved.

PFT_MM, [41:40]

DRAM prefetch using PrefetchTgt transactions for table walk requests. The possible values
are:

00
Disable prefetchtgt generation for requests from the Memory
Management Unit (MMU). This is the reset value.
01
Conservatively generate prefetchtgt for cacheable requests from the
MMU, always generate for non-cacheable.
10
Aggressively generate prefetchtgt for cacheable requests from the
MMU, always generate for non-cacheable.
11
Always generate prefetchtgt for cacheable requests from the MMU,
always generate for non-cacheable.

PFT_LS, [39:38]

DRAM prefetch using PrefetchTgt transactions for load and store requests. The possible
values are:

00
Disable prefetchtgt generation for requests from the Load-Store unit
(LS). This is the reset value.
01
Conservatively generate prefetchtgt for cacheable requests from the
LS, always generate for non-cacheable.
10
Aggressively generate prefetchtgt for cacheable requests from the
LS, always generate for non-cacheable.
11
Always generate prefetchtgt for cacheable requests from the LS,
always generate for non-cacheable.

PFT_IF, [37:36]

DRAM prefetch using PrefetchTgt transactions for instruction fetch requests. The possible
values are:

00
Disable prefetchtgt generation for requests from the Instruction
Fetch unit (IF). This is the reset value.
01
Conservatively generate prefetchtgt for cacheable requests from the
IF, always generate for non-cacheable.
10
Aggressively generate prefetchtgt for cacheable requests from the IF,
always generate for non-cacheable.
11
Always generate prefetchtgt for cacheable requests from the IF,
always generate for non-cacheable.

CA_UCLEAN_EVICT_EN, [35]

Enables sending WriteEvict transactions on the CPU CHI interface for UniqueClean evictions.
WriteEvict transactions update downstream caches. Enable WriteEvict transactions only if
there is an extra level of cache below the CPU's level 2 cache. The possible values are:

0
Disables sending data with UniqueClean evictions.
1
Enables sending data with UniqueClean evictions. This is the reset
value.

CA_EVICT_DIS, [34]

Disables sending of Evict transactions on the CPU CHI interface for clean cache lines that
are evicted from the core. Evict transactions are required only if the system contains a snoop
ﬁlter that requires notiﬁcation when the core evicts the cache line. The possible values are:

0
Enables sending Evict transactions. This is the reset value.
1
Disables sending Evict transactions.

RES0, [33]

RES0
Reserved.

ATOMIC_ACQ_NEAR, [32]

An atomic instruction to WB memory with acquire semantics that does not hit in the cache in
Exclusive state, can make up to one ﬁll request. The possible values are:

0
Acquire-atomic is near if cache line is already Exclusive, otherwise
make far atomic request.
1
Acquire-atomic will make up to 1 ﬁll request to perform near. This is
the reset value.

ATOMIC_ST_NEAR, [31]

A store atomic instruction to WB memory that does not hit in the cache in Exclusive state,
can make up to one ﬁll request. The possible values are:

0
Store-atomic is near if cache line is already Exclusive, otherwise make
far atomic request. This is the reset value.
1
Store-atomic will make up to 1 ﬁll request to perform near.

ATOMIC_REL_NEAR, [30]

An atomic instruction to WB memory with release semantics that does not hit in the cache in
Exclusive state, can make up to one ﬁll request. The possible values are:

0
Release-atomic is near if cache line is already Exclusive, otherwise
make far atomic request.
1
Release-atomic will make up to 1 ﬁll request to perform near. This is
the reset value.

ATOMIC_LD_NEAR, [29]

A load atomic (including SWP and CAS) instruction to WB memory that does not hit in the
cache in Exclusive state, can make up to one ﬁll request. The possible values are:

0
Load-atomic is near if cache line is already Exclusive, otherwise make
far atomic request.
1
Load-atomic will make up to 1 ﬁll request to perform near. This is the
reset value.

TLD_PRED_DIS, [28]

Disables Transient Load Prediction. The possible values are:

0
Enables transient load prediction. This is the reset value.
1
Disables transient load prediction.

RES0, [27]

RES0
Reserved.

DTLB_CABT_EN, [26]

Enables TLB Conﬂict Data Abort Exception. The possible values are:

0
Disables TLB conﬂict data abort exception. This is the reset value.
1
Enables TLB conﬂict data abort exception.

WS_THR_L2, [25:24]

Threshold for direct stream to L2 cache on store. The possible values are:

00
256B.
01
4KB. This is the reset value.
10
8KB.
11
Disables direct stream to L2 cache on store.

WS_THR_L3, [23:22]

Threshold for direct stream to L3 cache on store. The possible values are:

00
768B.
01
16KB. This is the reset value.
10
32KB.
11
Disables direct stream to L3 cache on store.

WS_THR_L4, [21:20]

Threshold for direct stream to L4 cache on store. The possible values are:

00
16KB.
01
64KB. This is the reset value.
10
128KB.
11
Disables direct stream to L4 cache on store.

WS_THR_DRAM, [19:18]

Threshold for direct stream to DRAM on store. The possible values are:

00
64KB.
01
1MB, for memory designated as outer-allocate. This is the reset
value.
10
1MB, allocating irrespective of outer-allocation designation.
11
Disables direct stream to DRAM on store.

WS_THR_DCZVA, [17]

Have DCZVA use a lower WS_THR_L2 conﬁguration. The possible values are:

0
DCZVA behaves like normal store wrt WS_THR_L2.
1
DCZVA will use one lower stream threshold from WS_THR_L2. This
is the reset value.

RES0, [16]

RES0
Reserved.

PF_DIS, [15]

Disables data-side hardware prefetching. The possible values are:

0
Enables hardware prefetching. This is the reset value.
1
Disables hardware prefetching.

RES0, [14]

RES0
Reserved.

PF_SS_L2_DIST, [13:12]

Single cache line stride prefetching L2 distance. The possible values are:

00
22
01
28
10
34
11
40. This is the reset value.

RES0, [11:10]

RES0
Reserved.

RES0, [9]

RES0
Reserved.

PF_STI_DIS, [8]

Disables store prefetches at issue (not overridden by CPUECTLR_EL1[15]). The possible
values are:

0
Enables store prefetching. This is the reset value.
1
Disables store prefetching.

PF_STS_DIS, [7]

Disables store-stride prefetches. The possible values are:

0
Enables store prefetching. This is the reset value.
1
Disables store prefetching.

RES0, [6]

RES0
Reserved.

RPF_DIS, [5]

Disables region prefetcher. The possible values are:

0
Enables region prefetching. This is the reset value.
1
Disables region prefetching.

RPF_LO_CONF, [4]

Region prefetcher training behavior. The possible values are:

0
Limited training for region prefetcher on single accesses.
1
Always train the region prefetcher on single accesses, which results
in fewer prefetch requests. This is the reset value.

RPF_PHIT_EN, [3]

Enable region prefetcher propagation on hit. The possible values are:

0
Disables region prefetcher propagation on hit. This is the reset value.
1
Enables region prefetcher propagation on hit.

RES0, [2:1]

RES0
Reserved.

EXTLLC, [0]

Internal or external Last-level cache (LLC) in the system. The possible values are:

0
Indicates that an internal Last-level cache is present in the system,
and that the DataSource ﬁeld on the master CHI interface indicates
when data is returned from the LLC. This is used to control how the
LL_CACHE* PMU events count. This is the reset value.

1
Indicates that an external Last-level cache is present in the system,
and that the DataSource ﬁeld on the master CHI interface indicates
when data is returned from the LLC. This is used to control how the
LL_CACHE* PMU events count.

Conﬁgurations

This register has no conﬁguration options.

Usage constraints

Accessing the CPUECTLR_EL1

The CPU Extended Control Register can be written only when the system is idle. Arm
recommends that you write to this register after a powerup reset, before the MMU is
enabled.

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This register can be written using MSR with the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

CPUECTLR_EL1
x
x
0
-
RW
n/a
RW

CPUECTLR_EL1
x
0
1
-
RW
RW
RW

CPUECTLR_EL1
x
1
1
-
n/a
RW
RW

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Synchronous exception
prioritization in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions
taken to AArch64 state.

Access to this register depends on bit[1] of ACTLR_EL2 and ACTLR_EL3.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|CPUECTLR_EL1|11|000|1111|0001|100|

## 13.33 CPUPCR_EL3, CPU Private Control Register, EL3

The CPUPCR_EL3 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUPCR_EL3 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers functional
group.

Figure 13-28: CPUPCR_EL3 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUPCR_EL3 is only accessible in Secure state.

Usage constraints

Accessing the CPUPCR_EL3

The CPUPCR_EL3 can be written only when the system is idle. Arm recommends that you
write to this register after a powerup reset, before the MMU is enabled.

Writing to this register might cause UNPREDICTABLE behaviors. Therefore, Arm strongly
recommends that you do not modify this register unless directed by Arm.

This register is accessible as follows:

This register can be read with the MRS instruction using the following syntax:

MRS <Xt>,<systemreg>

This register can be written with the MSR instruction using the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

## 13.34 CPUPMR_EL3, CPU Private Mask Register, EL3

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_6_C15_8_1
x
x
0
-
-
n/a
RW

S3_6_C15_8_1
x
0
1
-
-
-
RW

S3_6_C15_8_1
x
1
1
-
n/a
-
RW

'n/a' Not accessible. The core cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Synchronous exception
prioritization in the Arm® Architecture Reference Manual for A-proﬁle architecture.

The CPUPMR_EL3 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUPMR_EL3 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers functional
group.

Figure 13-29: CPUPMR_EL3 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUPMR_EL3 is only accessible in Secure state.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_6_C15_8_1|11|110|1111|1000|001|

Usage constraints

Accessing the CPUPMR_EL3

The CPUPMR_EL3 can be written only when the system is idle. Arm recommends that you
write to this register after a powerup reset, before the MMU is enabled.

Writing to this register might cause UNPREDICTABLE behaviors. Therefore, Arm strongly
recommends that you do not modify this register unless directed by Arm.

This register is accessible as follows:

This register can be read with the MRS instruction using the following syntax:

MRS <Xt>,<systemreg>

This register can be written with the MSR instruction using the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_6_C15_8_3
x
x
0
-
-
n/a
RW

S3_6_C15_8_3
x
0
1
-
-
-
RW

S3_6_C15_8_3
x
1
1
-
n/a
-
RW

'n/a' Not accessible. The core cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Synchronous exception
prioritization in the Arm® Architecture Reference Manual for A-proﬁle architecture.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_6_C15_8_3|11|110|1111|1000|011|

## 13.35 CPUPOR_EL3, CPU Private Operation Register, EL3

The CPUPOR_EL3 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUPOR_EL3 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers functional
group.

Figure 13-30: CPUPOR_EL3 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUPOR_EL3 is only accessible in Secure state.

Usage constraints

Accessing the CPUPOR_EL3

The CPUPOR_EL3 can be written only when the system is idle. Arm recommends that you
write to this register after a powerup reset, before the MMU is enabled.

Writing to this register might cause UNPREDICTABLE behaviors. Therefore, Arm strongly
recommends that you do not modify this register unless directed by Arm.

This register is accessible as follows:

This register can be read with the MRS instruction using the following syntax:

MRS <Xt>,<systemreg>

This register can be written with the MSR instruction using the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

## 13.36 CPUPSELR_EL3, CPU Private Selection Register, EL3

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_6_C15_8_2
x
x
0
-
-
n/a
RW

S3_6_C15_8_2
x
0
1
-
-
-
RW

S3_6_C15_8_2
x
1
1
-
n/a
-
RW

'n/a' Not accessible. The core cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Synchronous exception
prioritization in the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.36 CPUPSELR_EL3, CPU Private Selection Register,
EL3

The CPUPSELR_EL3 provides IMPLEMENTATION DEFINED conﬁguration and control options for the
core.

Bit ﬁeld descriptions
CPUPSELR_EL3 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers functional
group.

Figure 13-31: CPUPSELR_EL3 bit assignments

63

0

Reserved

Reserved, [63:0]

Reserved for Arm® internal use.

Conﬁgurations

CPUPSELR_EL3 is only accessible in Secure state.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_6_C15_8_2|11|110|1111|1000|010|

Usage constraints

Accessing the CPUPSELR_EL3

The CPUPSELR_EL3 can be written only when the system is idle. Arm recommends that you
write to this register after a powerup reset, before the MMU is enabled.

Writing to this register might cause UNPREDICTABLE behaviors. Therefore, Arm strongly
recommends that you do not modify this register unless directed by Arm.

This register is accessible as follows:

This register can be read with the MRS instruction using the following syntax:

MRS <Xt>,<systemreg>

This register can be written with the MSR instruction using the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_6_C15_8_0
x
x
0
-
-
n/a
RW

S3_6_C15_8_0
x
0
1
-
-
-
RW

S3_6_C15_8_0
x
1
1
-
n/a
-
RW

'n/a' Not accessible. The core cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Synchronous exception
prioritization in the Arm® Architecture Reference Manual for A-proﬁle architecture.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_6_C15_8_0|11|110|1111|1000|000|

## 13.37 CPUPWRCTLR_EL1, Power Control Register, EL1

The CPUPWRCTLR_EL1 provides information about power control support for the core.

Bit ﬁeld descriptions
CPUPWRCTLR_EL1 is a 64-bit register, and is part of the IMPLEMENTATION DEFINED registers
functional group.

This register resets to value 0x00000000.

Figure 13-32: CPUPWRCTLR_EL1 bit assignments

63
32

31
7
6
10 9
0

4

3
1

Reserved

WFI_RET_CTRL
WFE_RET_CTRL

CORE_PWRDN_EN

RES0

Reserved, [63:32]

Reserved.

RES0, [31:10]

RES0
Reserved.

WFE_RET_CTRL, [9:7]

CPU WFE retention control:

000
Disable the retention circuit. This is the default value, see Table
13-43: CPUPWRCTLR Retention Control Field on page 158 for
more retention control options.

WFI_RET_CTRL, [6:4]

CPU WFI retention control:

000
Disable the retention circuit. This is the default value, see Table
13-43: CPUPWRCTLR Retention Control Field on page 158 for
more retention control options.

RES0, [3:1]

RES0
Reserved.

CORE_PWRDN_EN, [0]

Indicates to the power controller using PACTIVE if the core wants to power down when it
enters WFI state.

0
No powerdown requested. This is the reset value.
1
A power down is requested.

Table 13-43: CPUPWRCTLR Retention Control Field

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the CPUPWRCTLR_EL1

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This register can be written using MSR with the following syntax:

MSR <systemreg>, <Xt>

This syntax is encoded with the following settings in the instruction encoding:

2 The number of system counter ticks required before the core signals retention readiness on PACTIVE to the power
controller. The core does not accept a retention entry request until this time.

|Encoding|Number of counter ticks 2|Minimum retention entry delay<br>(System counter at 50MHz-10MHz)|
|---|---|---|
|`000`|Disable the retention circuit|Default Condition.|
|`001`|2|40ns-200ns|
|`010`|8|160ns-800ns|
|`011`|32|640ns – 3,200ns|
|`100`|64|1,280ns-6,400ns|
|`101`|128|2,560ns-12,800ns|
|`110`|256|5,120ns-25,600ns|
|`111`|512|10,240ns-51,200ns|

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_0_C15_C2_7|11|000|1111|0010|111|

## 13.38 CSSELR_EL1, Cache Size Selection Register, EL1

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<systemreg>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C2_7
x
x
0
-
RW
n/a
RW

S3_0_C15_C2_7
x
0
1
-
RW
RW
RW

S3_0_C15_C2_7
x
1
1
-
n/a
RW
RW

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Exception priority order
in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions that are
taken to AArch32 state, and see Synchronous exception prioritization for exceptions that are
taken to AArch64 state.

Write-Access to this register from EL1 or EL2 depends on the value of bit[7] of ACTLR_EL2
and ACTLR_EL3.

CSSELR_EL1 selects the current Cache Size ID Register (CCSIDR_EL1), by specifying:

- The required cache level.

- The cache type, either instruction or data cache.

For details of the CCSIDR_EL1, see 13.23 CCSIDR_EL1, Cache Size ID Register, EL1 on page
128.

Bit ﬁeld descriptions
CSSELR_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

Figure 13-33: CSSELR_EL1 bit assignments

63
32

31
4
3
1
0

Level

Reserved

InD

RES0

## 13.39 CTR_EL0, Cache Type Register, EL0

Reserved, [63:32]

Reserved.

RES0, [31:4]

RES0
Reserved.

Level, [3:1]

Cache level of required cache:

000
L1.
001
L2.
010
L3, if present.

The combination of Level=001 and InD=1 is reserved.

The combinations of Level and InD for 0100 to 1111 are reserved.

InD, [0]

Instruction not Data bit:

0
Data or uniﬁed cache.
1
Instruction cache.

The combination of Level=001 and InD=1 is reserved.

The combinations of Level and InD for 0100 to 1111 are reserved.

Conﬁgurations

If a cache level is missing but CSSELR_EL1 selects this level, then a CCSIDR_EL1 read returns
an UNKNOWN value.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The CTR_EL0 provides information about the architecture of the caches.

Bit ﬁeld descriptions
CTR_EL0 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-34: CTR_EL0 bit assignments

29

63
32

31 30
28 27
24 23
20 19
16 15 14 13
4
3
0

CWG
ERG
DminLine
L1Ip

IminLine

Reserved

IDC
DIC

res1

res0

Reserved, [63:32]

Reserved.

RES1, [31]

RES1
Reserved.

RES0, [30:28]

RES0
Reserved.

DIC, [29]

Instruction cache invalidation requirements for instruction to data coherence:

0
Instruction cache invalidation to the point of uniﬁcation is required
for instruction to data coherence.
1
Instruction cache cleaning to the point of uniﬁcation is not required
for instruction to data coherence.

When conﬁgured with instruction cache hardware coherency, DIC is 1. When conﬁgured
without instruction cache hardware coherency, DIC is 0.

IDC, [28]

Data cache clean requirements for instruction to data coherence:

0
Data cache clean to the point of uniﬁcation is required for
instruction to data coherence, unless CLIDR_EL1.LoC == 0b000 or
(CLIDR_EL1.LoUIS == 0b000 && CLIDR_EL1.LoUU == 0b000).
1
Data cache clean to the point of uniﬁcation is not required for
instruction to data coherence.

IDC reﬂects the inverse value of the BROADCASTCACHEMAINTPOU pin.

CWG, [27:24]

Cache write-back granule. Log2 of the number of words of the maximum size of memory
that can be overwritten as a result of the eviction of a cache entry that has had a memory
location in it modiﬁed:

0100
Cache write-back granule size is 16 words.

ERG, [23:20]

Exclusives Reservation Granule. Log2 of the number of words of the maximum size of the
reservation granule that has been implemented for the Load-Exclusive and Store-Exclusive
instructions:

0100
Exclusive reservation granule size is 16 words.

DminLine, [19:16]

Log2 of the number of words in the smallest cache line of all the data and uniﬁed caches that
the core controls:

0100
Smallest data cache line size is 16 words.

L1Ip, [15:14]

Instruction cache policy. Indicates the indexing and tagging policy for the L1 instruction
cache:

11
Physically Indexed Physically Tagged (PIPT).

RES0, [13:4]

RES0
Reserved.

IminLine, [3:0]

Log2 of the number of words in the smallest cache line of all the instruction caches that the
core controls.

0100
Smallest instruction cache line size is 16 words.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.40 DCZID_EL0, Data Cache Zero ID Register, EL0

## 13.41 DISR_EL1, Deferred Interrupt Status Register, EL1

13.40 DCZID_EL0, Data Cache Zero ID Register, EL0

The DCZID_EL0 indicates the block size that is written with byte values of zero by the DC ZVA
(Data Cache Zero by Address) System instruction.

Bit ﬁeld descriptions
DCZID_EL0 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-35: DCZID_EL0 bit assignments

63
5
4
3

0

BlockSize

DZP

RES0

RES0, [63:5]

RES0
Reserved.

BlockSize, [3:0]

Log2 of the block size in words:

0100
The block size is 16 words.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The DISR_EL1 records the SError interrupts consumed by an ESB instruction.

Bit ﬁeld descriptions
DISR_EL1 is a 64-bit register, and is part of the registers Reliability, Availability, Serviceability (RAS)
functional group.

Figure 13-36: DISR_EL1 bit assignments, DISR_EL1.IDS is 0

63
32
30
25 24
0

31

5
6
8
9
10
12
13
23

AET
DFSC

A

IDS

EA

RES0

RES0, [63:32]

RES0
Reserved.

A, [31]

Set to 1 when ESB defers an asynchronous SError interrupt. If the implementation does not
include any synchronizable sources of SError interrupt, this bit is RES0.

RES0, [30:25]

RES0
Reserved.

IDS, [24]

Indicates the type of format the deferred SError interrupt uses. The value of this bit is:

0
Deferred error uses architecturally-deﬁned format.

RES0, [23:13]

RES0
Reserved.

AET, [12:10]

Asynchronous Error Type. Describes the state of the core after taking an asynchronous Data
Abort exception. The possible values are:

000
Uncontainable error (UC).
001
Unrecoverable error (UEU).

The recovery software must also examine any implemented fault records to
determine the location and extent of the error.

EA, [9]

RES0
Reserved.

## 13.42 ERRIDR_EL1, Error ID Register, EL1

RES0, [8:6]

RES0
Reserved.

DFSC, [5:0]

Data Fault Status Code. The possible values of this ﬁeld are:

010001
Asynchronous SError interrupt.

In AArch32, the 010001 code previously meant an
Asynchronous External Abort on memory access.
With the RAS extension, it extends to include any
asynchronous SError interrupt. The Parity Error codes
are not used in the RAS extension.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The ERRIDR_EL1 deﬁnes the number of error record registers.

Bit ﬁeld descriptions
ERRIDR_EL1 is a 64-bit register, and is part of the registers Reliability, Availability, Serviceability (RAS)
functional group.

This register is read-only.

Figure 13-37: ERRIDR_EL1 bit assignments

63
0
15
16

NUM

res0

RES0, [63:16]

RES0
Reserved.

## 13.43 ERRSELR_EL1, Error Record Select Register, EL1

NUM, [15:0]

Number of records that can be accessed through the Error Record System registers.

0x0001
One record present. (If no DSU SCU is present)
0x0002
Two records present. (If DSU SCU is present)

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The ERRSELR_EL1 selects which error record should be accessed through the Error Record System
registers. This register is not reset on a Warm reset.

Bit ﬁeld descriptions
ERRSELR_EL1 is a 64-bit register, and is part of the Reliability, Availability, Serviceability (RAS)
registers functional group.

Figure 13-38: ERRSELR_EL1 bit assignments

63
0
1

SEL

RES0

RES0, [63:1]

RES0
Reserved.

SEL, [0]

Selects which error record should be accessed.

0
Select error record 0 containing errors from level 1 and level 2 RAMs
that are located on the Neoverse™ N1 core.
1
Select error record 1 containing errors from level 3 RAMs that are
located on the DSU. (If DSU SCU is present)

Conﬁgurations

There are no conﬁguration notes.

## 13.44 ERXADDR_EL1, Selected Error Record Address Register, EL1

## 13.45 ERXCTLR_EL1, Selected Error Record Control Register, EL1

## 13.46 ERXFR_EL1, Selected Error Record Feature Register, EL1

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.44 ERXADDR_EL1, Selected Error Record Address
Register, EL1

Register ERXADDR_EL1 accesses the ERR<n>ADDR address register for the error record that is
selected by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXADDR_EL1 accesses the ERR0ADDR register of the core error
record. See 14.2 ERR0ADDR, Error Record Address Register on page 254.

If ERRSELR_EL1.SEL==1, then ERXADDR_EL1 accesses the ERR1ADDR register of the DSU
error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

13.45 ERXCTLR_EL1, Selected Error Record Control
Register, EL1

Register ERXCTLR_EL1 accesses the ERR<n>CTLR control register for the error record that is
selected by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXCTLR_EL1 accesses the ERR0CTLR register of the core error
record. See 14.3 ERR0CTLR, Error Record Control Register on page 255.

If ERRSELR_EL1.SEL==1, then ERXCLTR_EL1 accesses the ERR1CTLR register of the DSU error
record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

13.46 ERXFR_EL1, Selected Error Record Feature Register,
EL1

Register ERXFR_EL1 accesses the ERR<n>FR feature register for the error record that is selected
by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXFR_EL1 accesses the ERR0FR register of the core error record.
See 14.4 ERR0FR, Error Record Feature Register on page 257.

If ERRSELR_EL1.SEL==1, then ERXFR_EL1 accesses the ERR1FR register of the DSU error record
(if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference Manual.

## 13.47 ERXMISC0_EL1, Selected Error Record Miscellaneous Register 0, EL1

## 13.48 ERXMISC1_EL1, Selected Error Record Miscellaneous Register 1, EL1

## 13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault Generation Count Down Register, EL1

13.47 ERXMISC0_EL1, Selected Error Record
Miscellaneous Register 0, EL1

Register ERXMISC0_EL1 accesses the ERR<n>MISC0 register for the error record that is selected
by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXMISC0_EL1 accesses the ERR0MISC0 register of the core error
record. See 14.5 ERR0MISC0, Error Record Miscellaneous Register 0 on page 259.

If ERRSELR_EL1.SEL==1, then ERXMISC0_EL1 accesses the ERR1MISC0 register of the DSU
error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

13.48 ERXMISC1_EL1, Selected Error Record
Miscellaneous Register 1, EL1

Register ERXMISC1_EL1 accesses the ERR<n>MISC1 miscellaneous register 1 for the error record
that is selected by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXMISC1_EL1 accesses the ERR0MISC1 register of the core error
record. See 14.6 ERR0MISC1, Error Record Miscellaneous Register 1 on page 262.

If ERRSELR_EL1.SEL==1, then ERXMISC1_EL1 accesses the ERR1MISC1 register of the DynamIQ
Shared Unit (DSU) error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit
Technical Reference Manual.

13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault
Generation Count Down Register, EL1

Register ERXPFGCDN_EL1 accesses the ERR<n>PFGCND register for the error record that is
selected by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXPFGCDN_EL1 accesses the ERR0PFGCDN register of the core
error record. See 14.7 ERR0PFGCDN, Error Pseudo Fault Generation Count Down Register on
page 262.

If ERRSELR_EL1.SEL==1, then ERXPFGCDN_EL1 accesses the ERR1PFGCDNR register of the
DSU error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical
Reference Manual.

Conﬁgurations

There are no conﬁguration notes.

Accessing the ERXPFGCDN_EL1

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This register can be written using MSR with the following syntax:

MSR <Xt>,<systemreg>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<syntax>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C2_2
x
x
0
-
RW
n/a
RW

S3_0_C15_C2_2
x
0
1
-
RW
RW
RW

S3_0_C15_C2_2
x
1
1
-
n/a
RW
RW

'n/a' Not accessible. Executing the PE at this Exception level is not permitted.

Traps and enables

For a description of the prioritization of any generated exceptions, see Exception priority order
in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions taken to
AArch32 state, and see Synchronous exception prioritization for exceptions taken to AArch64
state. Subject to these prioritization rules, the following traps and enables are applicable
when accessing this register.
ERXPFGCDN_EL1 is accessible at EL3 and can be accessible at EL1 and EL2 depending on
the value of bit[5] in ACTLR_EL2 and ACTLR_EL3. See 13.6 ACTLR_EL2, Auxiliary Control
Register, EL2 on page 105 and 13.7 ACTLR_EL3, Auxiliary Control Register, EL3 on page
108.
ERXPFGCDN_EL1 is UNDEFINED at EL0.

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_0_C15_C2_2|11|000|1111|0010|010|

## 13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault Generation Control Register, EL1

13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault
Generation Control Register, EL1

Register ERXPFGCTL_EL1 accesses the ERR<n>PFGCTL register for the error record that is
selected by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXPFGCTL_EL1 accesses the ERR0PFGCTL register of the core
error record. See 14.8 ERR0PFGCTL, Error Pseudo Fault Generation Control Register on page
263.

If ERRSELR_EL1.SEL==1, then ERXPFGCTL_EL1 accesses the ERR1PFGCTL register of the DSU
error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

Conﬁgurations

There are no conﬁguration notes.

Accessing the ERXPFGCTL_EL1

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This register can be written using MSR with the following syntax:

MSR <Xt>,<systemreg>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

Control
Accessibility
<syntax>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C2_1
x
x
0
-
RW
n/a
RW

S3_0_C15_C2_1
x
0
1
-
RW
RW
RW

S3_0_C15_C2_1
x
1
1
-
n/a
RW
RW

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Exception priority order
in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions taken to

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_0_C15_C2_1|11|000|1111|0010|001|

## 13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation Feature Register, EL1

AArch32 state, and see Synchronous exception prioritization for exceptions taken to AArch64
state. Subject to these prioritization rules, the following traps and enables are applicable
when accessing this register.
ERXPFGCTL_EL1 is accessible at EL3 and can be accessible at EL1 and EL2 depending on
the value of bit[5] in ACTLR_EL2 and ACTLR_EL3. See 13.6 ACTLR_EL2, Auxiliary Control
Register, EL2 on page 105 and 13.7 ACTLR_EL3, Auxiliary Control Register, EL3 on page
108.
ERXPFGCTL_EL1 is UNDEFINED at EL0.

If ERXPFGCTL_EL1 is accessible at EL1 and HCR_EL2.TERR == 1, then direct reads and
writes of ERXPFGCTL_EL1 at Non-secure EL1 generate a Trap exception to EL2.

If ERXPFGCTL_EL1 is accessible at EL1 or EL2 and SCR_EL3.TERR == 1, then direct reads
and writes of ERXPFGCTL_EL1 at EL1 or EL2 generate a Trap exception to EL3.

13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation
Feature Register, EL1

Register ERXPFGF_EL1 accesses the ERR<n>PFGF register for the error record that is selected by
ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXPFGF_EL1 accesses the ERR0PFGF register of the core error
record. See 14.9 ERR0PFGF, Error Pseudo Fault Generation Feature Register on page 265.

If ERRSELR_EL1.SEL==1, then ERXPFGF_EL1 accesses the ERR1PFGFR register of the DSU
error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

Conﬁgurations

This core has no conﬁguration notes.

Accessing the ERXPFGF_EL1

This register can be read using MRS with the following syntax:

MRS <Xt>,<systemreg>

This syntax is encoded with the following settings in the instruction encoding:

Accessibility

This register is accessible in software as follows:

|<systemreg>|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|---|
|S3_0_C15_C2_0|11|000|1111|0010|000|

## 13.52 ERXSTATUS_EL1, Selected Error Record Primary Status Register, EL1

Control
Accessibility
<syntax>

E2H
TGE
NS
EL0
EL1
EL2
EL3

S3_0_C15_C2_0
x
x
0
-
RO
n/a
RO

S3_0_C15_C2_0
x
0
1
-
RO
RO
RO

S3_0_C15_C2_0
x
1
1
-
n/a
RO
RO

'n/a' Not accessible. The PE cannot be executing at this Exception level, so this access is not
possible.

Traps and enables

For a description of the prioritization of any generated exceptions, see Exception priority order
in the Arm® Architecture Reference Manual for A-proﬁle architecture for exceptions taken to
AArch32 state, and see Synchronous exception prioritization for exceptions taken to AArch64
state. Subject to these prioritization rules, the following traps and enables are applicable
when accessing this register.
ERXPFGR_EL1 is accessible at EL3 and can be accessible at EL1 and EL2 depending on
the value of bit[5] in ACTLR_EL2 and ACTLR_EL3. See 13.6 ACTLR_EL2, Auxiliary Control
Register, EL2 on page 105 and 13.7 ACTLR_EL3, Auxiliary Control Register, EL3 on page
108.
ERXPFGR_EL1 is UNDEFINED at EL0.

If ERXPFGR_EL1 is accessible at EL1 and HCR_EL2.TERR == 1, then direct reads and writes
of ERXPFGR_EL1 at Non-secure EL1 generate a Trap exception to EL2.

If ERXPFGR_EL1 is accessible at EL1 or EL2 and SCR_EL3.TERR == 1, then direct reads and
writes of ERXPFGR_EL1 at EL1 or EL2 generate a Trap exception to EL3.

13.52 ERXSTATUS_EL1, Selected Error Record Primary
Status Register, EL1

Register ERXSTATUS_EL1 accesses the ERR<n>STATUS primary status register for the error record
that is selected by ERRSELR_EL1.SEL.

If ERRSELR_EL1.SEL==0, then ERXSTATUS_EL1 accesses the ERR0STATUS register of the core
error record. See 14.10 ERR0STATUS, Error Record Primary Status Register on page 266.

If ERRSELR_EL1.SEL==1, then ERXSTATUS_EL1 accesses the ERR1STATUS register of the DSU
error record (if the DSU SCU is present). See the  Arm® DynamIQ™ Shared Unit Technical Reference
Manual.

## 13.53 ESR_EL1, Exception Syndrome Register, EL1

The ESR_EL1 holds syndrome information for an exception taken to EL1.

Bit ﬁeld descriptions
ESR_EL1 is a 64-bit register, and is part of the Exception and fault handling registers functional
group.

Figure 13-39: ESR_EL1 bit assignments

63
32

31
0

25
24
26

Reserved

EC

ISS

IL

Reserved, [63:32]

Reserved.

EC, [31:26]

Exception Class. Indicates the reason for the exception that this register holds information
about.

IL, [25]

Instruction Length for synchronous exceptions. The possible values are:

0
16-bit.
1
32-bit.

This ﬁeld is 1 for the SError interrupt, instruction aborts, misaligned PC, Stack pointer
misalignment, Data Aborts for which the ISV bit is 0, exceptions caused by an illegal
instruction set state, and exceptions using the 0x00 Exception Class.

ISS, [24:0]

Syndrome information.

When reporting a virtual SEI, bits[24:0] take the value of VSESRL_EL2[24:0].

When reporting a physical SEI, the following occurs:

- IDS==0 (architectural syndrome).

- AET always reports an uncontainable error (UC) with value 0b000 or an unrecoverable error
(UEU) with value 0b001.

## 13.54 ESR_EL2, Exception Syndrome Register, EL2

- EA is RES0.

When reporting a synchronous data abort, EA is RES0.

See 13.108 VSESR_EL2, Virtual SError Exception Syndrome Register  on page 250.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the Arm®
Architecture Reference Manual for A-proﬁle architecture.

The ESR_EL2 holds syndrome information for an exception taken to EL2.

Bit ﬁeld descriptions
ESR_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group.

- The Exception and fault handling registers functional group.

Figure 13-40: ESR_EL2 bit assignments

63
32

31
0

25
24
26

Reserved

EC

ISS

IL

Reserved, [63:32]

Reserved.

EC, [31:26]

Exception Class. Indicates the reason for the exception that this register holds information
about. See the Arm® Architecture Reference Manual for A-proﬁle architecture for more
information.

IL, [25]

Instruction Length for synchronous exceptions. The possible values are:

0
16-bit.

## 13.55 ESR_EL3, Exception Syndrome Register, EL3

1
32-bit.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

ISS, [24:0]

Syndrome information. See the Arm® Architecture Reference Manual for A-proﬁle architecture
for more information.

When reporting a virtual SEI, bits[24:0] take the value of VSESRL_EL2[24:0].

When reporting a physical SEI, the following occurs:

- IDS==0 (architectural syndrome).

- AET always reports an uncontainable error (UC) with value 0b000 or an unrecoverable
error (UEU) with value 0b001.

- EA is RES0.

When reporting a synchronous Data Abort, EA is RES0.

See 13.108 VSESR_EL2, Virtual SError Exception Syndrome Register  on page 250.

Conﬁgurations

RW ﬁelds in this register reset to architecturally UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The ESR_EL3 holds syndrome information for an exception taken to EL3.

Bit ﬁeld descriptions
ESR_EL3 is a 64-bit register, and is part of the Exception and fault handling registers functional
group.

Figure 13-41: ESR_EL3 bit assignments

63
32

31
0

25
24
26

Reserved

EC

ISS

IL

Reserved, [63:32]

Reserved.

EC, [31:26]

Exception Class. Indicates the reason for the exception that this register holds information
about.

IL, [25]

Instruction Length for synchronous exceptions. The possible values are:

0
16-bit.
1
32-bit.

This ﬁeld is 1 for the SError interrupt, instruction aborts, misaligned PC, Stack pointer
misalignment, data aborts for which the ISV bit is 0, exceptions caused by an illegal
instruction set state, and exceptions using the 0x0 Exception Class.

ISS, [24:0]

Syndrome information.

When reporting a virtual SEI, bits[24:0] take the value of VSESRL_EL2[24:0].

When reporting a physical SEI, the following occurs:

- IDS==0 (architectural syndrome).

- AET always reports an uncontainable error (UC) with value 0b000 or an unrecoverable error
(UEU) with value 0b001.

- EA is RES0.

When reporting a synchronous data abort, EA is RES0.

See 13.108 VSESR_EL2, Virtual SError Exception Syndrome Register  on page 250.

Conﬁgurations

RW ﬁelds in this register reset to architecturally UNKNOWN values.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the Arm®
Architecture Reference Manual for A-proﬁle architecture.

## 13.56 HACR_EL2, Hyp Auxiliary Configuration Register, EL2

## 13.57 HCR_EL2, Hypervisor Configuration Register, EL2

13.56 HACR_EL2, Hyp Auxiliary Conﬁguration Register,
EL2

HACR_EL2 controls trapping to EL2 of IMPLEMENTATION DEFINED aspects of Non-secure EL1 or EL0
operation. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
HACR_EL2 is a 64-bit register, and is part of Virtualization registers functional group.

Figure 13-42: HACR_EL2 bit assignments

63
32

31

0

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

Reserved, RES0.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.57 HCR_EL2, Hypervisor Conﬁguration Register, EL2

The HCR_EL2 provides conﬁguration control for virtualization, including whether various Non-
secure operations are trapped to EL2.

Bit ﬁeld descriptions
HCR_EL2 is a 64-bit register, and is part of the Virtualization registers functional group.

Figure 13-43: HCR_EL2 bit assignments

63
35
36
37
38
48

49
50
51
52
53
39

30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13
10
9
8
7
6
5
4
3
32
33
34

31
0
1
2
11
12

TOCU

MIOCNCE

VM

SWIO

TICAB

TEA

PTW

TID4

TERR

FMO

IMO

TLOR

AMO

E2H

ID

VF

VI

CD

VSE

RW

FB

TRVM

BSU

HCD

DC

TDZ

TWI

TGE

TWE

TVM

TID0

TTLB

TID1

TID2

TPU

TID3

TPC

TSW

TSC

res0

TACR
TIDCP

res1

RES0, [63:53]

RES0
Reserved.

TOCU, [52]

Trap IC IVAU, IC IALLU, and DC CVAU. The possible values are:

0
Execution of the IC IVAU, IC IALLU, and DC CVAU instructions at
EL1 is not trapped as a result of this control bit.

Execution of the IC IVAU and DC CVAU instructions at EL0 is not
trapped as a result of this control bit.
1
Execution of the IC IVAU, IC IALLU, and DC CVAU instructions at
EL1 is trapped to EL2.

Execution of the IC IVAU and DC CVAU instructions at EL0 is
trapped to EL2 if SCTLR_EL1.UCI==1.

RES0, [51]

RES0
Reserved.

TICAB, [50]

Trap IC IALLUIS. The possible values are:

0
Execution of the IC IALLUIS instructions at EL1 is not trapped as a
result of this control bit.
1
Execution of the IC IALLUIS instructions at EL1 is trapped to EL2.

TID4, [49]

Trap CLIDR_EL1, CSSELR_EL1, CCSIDR_EL1, and CCSIDR2_EL1. The possible values are:

0
Read of CLIDR_EL1, CSSELR_EL1, CCSIDR_EL1, CCSIDR2_EL1, and
write of CSSELR_EL1 at EL1 is not trapped as a result of this control
bit.
1
Read of CLIDR_EL1, CSSELR_EL1, CCSIDR_EL1, CCSIDR2_EL1, and
write of CSSELR_EL1 at EL1 is trapped to EL2.

RES0, [48:39]

RES0
Reserved.

MIOCNCE, [38]

Mismatched Inner/Outer Cacheable Non-Coherency Enable, for the Non-secure EL1 and
EL0 translation regime.

RW, [31]

RES1
Reserved.

HCD, [29]

RES0
Reserved.

TGE, [27]

Traps general exceptions. If this bit is set, and SCR_EL3.NS is set, then:

- All exceptions that would be routed to EL1 are routed to EL2.

- The SCTLR_EL1.M bit is treated as 0 regardless of its actual state, other than for reading
the bit.

- The HCR_EL2.FMO, IMO, and AMO bits are treated as 1 regardless of their actual state,
other than for reading the bits.

- All virtual interrupts are disabled.

- Any IMPLEMENTATION DEFINED mechanisms for signaling virtual interrupts are disabled.

- An exception return to EL1 is treated as an illegal exception return.

HCR_EL2.TGE must not be cached in a TLB.

When the value of SCR_EL3.NS is 0, the core behaves as if this ﬁeld is 0 for all purposes
other than a direct read or Write-Access of HCR_EL2.

TID3, [18]

Traps ID group 3 registers. The possible values are:

0
ID group 3 register accesses are not trapped.
1
Reads to ID group 3 registers that are executed from Non-secure EL1
are trapped to EL2.

## 13.58 ID_AA64AFR0_EL1, AArch64 Auxiliary Feature Register 0

## 13.59 ID_AA64AFR1_EL1, AArch64 Auxiliary Feature Register 1

## 13.60 ID_AA64DFR0_EL1, AArch64 Debug Feature Register 0, EL1

See the Arm® Architecture Reference Manual for A-proﬁle architecture for the registers covered
by this setting.

Conﬁgurations

If EL2 is not implemented, this register is RES0 from EL3

RW ﬁelds in this register reset to architecturally UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.58 ID_AA64AFR0_EL1, AArch64 Auxiliary Feature
Register 0

The core does not use this register, ID_AA64AFR0_EL1 is RES0.

13.59 ID_AA64AFR1_EL1, AArch64 Auxiliary Feature
Register 1

The core does not use this register, ID_AA64AFR0_EL1 is RES0.

13.60 ID_AA64DFR0_EL1, AArch64 Debug Feature
Register 0, EL1

Provides top-level information about the debug system in AArch64.

Bit ﬁeld descriptions
ID_AA64DFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-44: ID_AA64DFR0_EL1 bit assignments

4
3
8
7
12 11
16 15
20 19
24 23
28 27
0
63

35
36

32 31

PMSVer

Debugver
Tracever
PMUver
BRPs
WRPs
CTX_CMPs

res0

RES0, [63:36]

RES0

Reserved.

PMSVer, [35:32]

Statistical Proﬁling Extension version.

0x1
Version 1 of the Statistical Proﬁling extension is present.

CTX_CMPs, [31:28]

Number of breakpoints that are context-aware, minus 1. These are the highest numbered
breakpoints:

0x1
Two breakpoints are context-aware.

RES0, [27:24]

RES0

Reserved.

WRPs, [23:20]

The number of watchpoints minus 1:

0x3
Four watchpoints.

RES0, [19:16]

RES0

Reserved.

BRPs, [15:12]

The number of breakpoints minus 1:

0x5
Six breakpoints.

PMUVer, [11:8]

Performance Monitors Extension version.

0x4
Performance monitor System registers implemented, PMUv3.

TraceVer, [7:4]

Trace extension:

0x0
Trace System registers not implemented.

## 13.61 ID_AA64DFR1_EL1, AArch64 Debug Feature Register 1, EL1

## 13.62 ID_AA64ISAR0_EL1, AArch64 Instruction Set Attribute Register 0, EL1

DebugVer, [3:0]

Debug architecture version:

0x8
Arm®v8‑A debug architecture implemented.

Conﬁgurations

ID_AA64DFR0_EL1 is architecturally mapped to external register EDDFR.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.61 ID_AA64DFR1_EL1, AArch64 Debug Feature
Register 1, EL1

This register is reserved for future expansion of top-level information about the debug system in
AArch64 state.

13.62 ID_AA64ISAR0_EL1, AArch64 Instruction Set
Attribute Register 0, EL1

The ID_AA64ISAR0_EL1 provides information about the instructions that are implemented in
AArch64 state, including the instructions that are provided by the Cryptographic Extension.

Bit ﬁeld descriptions
ID_AA64ISAR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

The optional Cryptographic Extension is not included in the base product of the core. Arm requires
licensees to have contractual rights to obtain the Cryptographic Extension.

Figure 13-45: ID_AA64ISAR0_EL1 bit assignments

63
0

43
44
47
48

32

27
24
28
31

20 19
23

16 15
12 11
8
7
4
3

DP

RDM

Atomic

CRC32

AES
SHA1
SHA2

RES0

RES0, [63:48]

RES0
Reserved.

DP, [47:44]

Indicates whether Dot Product support instructions are implemented.

0x1
UDOT, SDOT instructions are implemented.

RES0, [43:32]

RES0
Reserved.

RDM, [31:28]

Indicates whether SQRDMLAH and SQRDMLSH instructions in AArch64 are implemented.

0x1
SQRDMLAH and SQRDMLSH instructions are implemented.

RES0, [27:24]

RES0
Reserved.

Atomic, [23:20]

Indicates whether Atomic instructions in AArch64 are implemented. The value is:

0x2
LDADD, LDCLR, LDEOR, LDSET, LDSMAX, LDSMIN, LDUMAX, LDUMIN, CAS, CASP,
and SWP instructions are implemented.

CRC32, [19:16]

Indicates whether CRC32 instructions are implemented. The value is:

0x1
CRC32 instructions are implemented.

SHA2, [15:12]

Indicates whether SHA2 instructions are implemented. The possible values are:

0x0
No SHA2 instructions are implemented. This is the value if the core
implementation does not include the Cryptographic Extension.
0x1
SHA256H, SHA256H2, SHA256U0, and SHA256U1 implemented. This is
the value if the core implementation includes the Cryptographic
Extension.

SHA1, [11:8]

Indicates whether SHA1 instructions are implemented. The possible values are:

0x0
No SHA1 instructions implemented. This is the value if the core
implementation does not include the Cryptographic Extension.
0x1
SHA1C, SHA1P, SHA1M, SHA1SU0, and SHA1SU1 implemented. This is
the value if the core implementation includes the Cryptographic
Extension.

## 13.63 ID_AA64ISAR1_EL1, AArch64 Instruction Set Attribute Register 1, EL1

AES, [7:4]

Indicates whether AES instructions are implemented. The possible values are:

0x0
No AES instructions implemented. This is the value if the core
implementation does not include the Cryptographic Extension.
0x2
AESE, AESD, AESMC, and AESIMC implemented, plus PMULL and PMULL2
instructions operating on 64-bit data. This is the value if the core
implementation includes the Cryptographic Extension.

RES0, [3:0]

RES0
Reserved.

Conﬁgurations

ID_AA64ISAR0_EL1 is architecturally mapped to external register ID_AA64ISAR0.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.63 ID_AA64ISAR1_EL1, AArch64 Instruction Set
Attribute Register 1, EL1

The ID_AA64ISAR1_EL1 provides information about the instructions that are implemented in
AArch64 state.

Bit ﬁeld descriptions
ID_AA64ISAR1_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-46: ID_AA64ISAR1_EL1 bit assignments

63
0

23
24

4
3
20 19

LRCPC

DC CVAP

res0

RES0, [63:24]

RES0

Reserved.

## 13.64 ID_AA64MMFR0_EL1, AArch64 Memory Model Feature Register 0, EL1

LRCPC, [23:20]

Indicates whether load-acquire (LDA) instructions are implemented for a Release Consistent
core consistent RCPC model.

0x1
The LDAPRB, LDAPRH, and LDAPR instructions are implemented in
AArch64.

RES0, [19:4]

RES0

Reserved.

DC CVAP, [3:0]

Indicates whether data cache, Clean to the Point of Persistence (DC CVAP) instructions are
implemented.

0x1
DC CVAP is supported in AArch64.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.64 ID_AA64MMFR0_EL1, AArch64 Memory Model
Feature Register 0, EL1

The ID_AA64MMFR0_EL1 provides information about the implemented memory model and
memory management support in the AArch64 Execution state.

Bit ﬁeld descriptions
ID_AA64MMFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional
group.

This register is read-only.

Figure 13-47: ID_AA64MMFR0_EL1 bit assignments

27
28
31
32

63
0
4
3
8
7
12 11
16 15

24 23

20 19

TGran64
TGran4

BigEndEL0

SNSMem
TGran16
PARange

BigEnd
ASIDBits

res0

RES0, [63:32]

RES0

Reserved.

TGran4, [31:28]

Support for 4KB memory translation granule size:

0x0
4KB granule supported.

TGran64, [27:24]

Support for 64KB memory translation granule size:

0x0
64KB granule supported.

TGran16, [23:20]

Support for 16KB memory translation granule size:

0x1
Indicates that the 16KB granule is supported.

BigEndEL0, [19:16]

Mixed-endian support only at EL0.

0x0
No mixed-endian support at EL0. The SCTLR_EL1.E0E bit has a ﬁxed value.

SNSMem, [15:12]

Secure versus Non-secure Memory distinction:

0x1
Supports a distinction between Secure and Non-secure Memory.

BigEnd, [11:8]

Mixed-endian conﬁguration support:

0x1
Mixed-endian support. The SCTLR_ELx.EE and SCTLR_EL1.E0E bits can be conﬁgured.

ASIDBits, [7:4]

Number of ASID bits:

0x2
16 bits.

PARange, [3:0]

Physical address range supported:

## 13.65 ID_AA64MMFR1_EL1, AArch64 Memory Model Feature Register 1, EL1

0x5
48 bits, 256TB.
The supported Physical Address Range is 48-bits. Other cores in the DSU might
support a diﬀerent Physical Address Range.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.65 ID_AA64MMFR1_EL1, AArch64 Memory Model
Feature Register 1, EL1

The ID_AA64MMFR1_EL1 provides information about the implemented memory model and
memory management support in the AArch64 Execution state.

Bit ﬁeld descriptions
ID_AA64MMFR1_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional
group.

This register is read-only.

Figure 13-48: ID_AA64MMFR1_EL1 bit assignments

63
0
4
3
8
7
12 11
16 15

32 31

24 23
28 27

20 19

XNX

SpecSEI

HPDS
PAN
HAFDBS

LO

VH
VMIDBits

RES0

RES0, [63:32]

RES0

Reserved.

XNX, [31:28]

Indicates whether provision of EL0 vs EL1 execute-never control at stage 2 is supported.

0x1
EL0/EL1 execute control distinction at stage 2 bit is supported. All other values are
reserved.

SpecSEI, [27:24]

Describes whether the PE can generate SError interrupt exceptions from Speculative reads of
memory, including Speculative instruction fetches.

0x0
The PE never generates an SError interrupt due to an External abort on a Speculative
read.

PAN, [23:20]

Privileged Access Never. Indicates support for the PAN bit in PSTATE, SPSR_EL1, SPSR_EL2,
SPSR_EL3, and DSPSR_EL0.

0x2
PAN supported and AT S1E1RP and AT S1E1WP instructions supported.

LO, [19:16]

Indicates support for LORegions.

0x1
LORegions are supported.

HPDS, [15:12]

Presence of Hierarchical Disables. Enables an operating system or hypervisor to hand over
up to 4 bits of the last level translation table descriptor (bits[62:59] of the translation table
entry) for use by hardware for IMPLEMENTATION DEFINED usage. The value is:

0x2
Hierarchical Permission Disables and Hardware allocation of bits[62:59] supported.

VH, [11:8]

Indicates whether Virtualization Host Extensions are supported.

0x1
Virtualization Host Extensions supported.

VMIDBits, [7:4]

Indicates the number of VMID bits supported.

0x2
16 bits are supported.

HAFDBS, [3:0]

Indicates the support for hardware updates to Access ﬂag and dirty state in translation tables.

0x2
Hardware update of both the Access ﬂag and dirty state is supported in hardware.

Conﬁgurations

There are no conﬁguration notes.

## 13.66 ID_AA64MMFR2_EL1, AArch64 Memory Model Feature Register 2, EL1

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.66 ID_AA64MMFR2_EL1, AArch64 Memory Model
Feature Register 2, EL1

The ID_AA64MMFR2_EL1 provides information about the implemented memory model and
memory management support in the AArch64 Execution state.

Bit ﬁeld descriptions
ID_AA64MMFR2_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional
group.

This register is read-only.

Figure 13-49: ID_AA64MMFR2_EL1 bit assignments

63
0
4
3
8
7
12 11
16 15

59
60
56 55

19
20

EVT

VARange

LSM
UAO
IESB
CnP

RES0

RES0, [63:60]

RES0

Reserved.

EVT, [59:56]

Enhanced Virtualization Traps. The value is:

0x1
HCR_EL2.TICAB, HCR_EL2.TOCU, and HCR_EL2.TID4 traps are supported.
HCR_EL2.TTLBIS and HCR_EL2.TTLBOS traps are not supported.

RES0, [55:20]

RES0

Reserved.

VARange, [19:16]

Indicates support for a larger virtual address. The value is:

0x0
VMSAv8-64 supports 48-bit virtual addresses.

## 13.67 ID_AA64PFR0_EL1, AArch64 Processor Feature Register 0, EL1

IESB, [15:12]

Indicates whether an implicit Error Synchronization Barrier has been inserted. The value is:

0x1
SCTLR_ELx.IESB implicit ErrorSynchronizationBarrier control implemented.

LSM, [11:8]

Indicates whether LDM and STM ordering control bits are supported. The value is:

0x0
LSMAOE and nTLSMD bit not supported.

UAO, [7:4]

Indicates the presence of the User Access Override (UAO). The value is:

0x1
UAO is supported.

CnP, [3:0]

Common not Private. Indicates whether a TLB entry is pointed at a translation table base
register that is a member of a common set. The value is:

0x1
CnP bit is supported.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.67 ID_AA64PFR0_EL1, AArch64 Processor Feature
Register 0, EL1

The ID_AA64PFR0_EL1 provides additional information about implemented core features in
AArch64.

The optional Advanced SIMD and ﬂoating-point support is not included in the base product of the
core. Arm requires licensees to have contractual rights to obtain the Advanced SIMD and ﬂoating-
point support.

Bit ﬁeld descriptions
ID_AA64PFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-50: ID_AA64PFR0_EL1 bit assignments

RES0

CSV3, [63:60]

0x1
Data that are loaded under speculation with a permission or domain fault cannot be
used to form an address or generate condition codes to be used by instructions newer
than the load in the speculative sequence. This is the reset value.

All other values reserved.

CSV2, [59:56]

0x1
Branch targets trained in one context cannot aﬀect speculative execution in a diﬀerent
hardware described context. This is the reset value.

All other values reserved.

RES0, [55:32]

RES0

Reserved.

RAS, [31:28]

RAS extension version. The possible values are:

0x1
Version 1 of the RAS extension is present.

GIC, [27:24]

GIC CPU interface:

0x0
GIC CPU interface is disabled, GICCDISABLE is HIGH, or not implemented.

0x1
GIC CPU interface is implemented and enabled, GICCDISABLE is LOW. GICv4 is
supported.

AdvSIMD, [23:20]

Advanced SIMD. The possible values are:

|63 60|59 56|55 32|31 28|27 24|23 20|19 16|15 12|11 8|7 4|3 0|
|---|---|---|---|---|---|---|---|---|---|---|
|CSV3|CSV2||RAS|GIC|AdvSIMD|FP|EL3<br>handling|EL2<br>handling|EL1<br>handling|EL0<br>handling|

## 13.68 ID_AA64PFR1_EL1, AArch64 Processor Feature Register 1, EL1

0x1
Advanced SIMD, including half-precision support, is implemented.

FP, [19:16]

Floating-point. The possible values are:

0x1
Floating-point, including half-precision support, is implemented.

EL3 handling, [15:12]

EL3 exception handling:

0x1
Instructions can be executed at EL3 in AArch64 state only.

EL2 handling, [11:8]

EL2 exception handling:

0x1
Instructions can be executed at EL3 in AArch64 state only.

EL1 handling, [7:4]

EL1 exception handling. The possible values are:

0x1
Instructions can be executed at EL3 in AArch64 state only.

EL0 handling, [3:0]

EL0 exception handling. The possible values are:

0x2
Instructions can be executed at EL0 in AArch64 or AArch32 state.

Conﬁgurations

ID_AA64PFR0_EL1 is architecturally mapped to External register EDPFR.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.68 ID_AA64PFR1_EL1, AArch64 Processor Feature
Register 1, EL1

The ID_AA64PFR1_EL1 provides additional information about implemented core features in
AArch64.

Bit ﬁeld descriptions
ID_AA64PFR1_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

## 13.69 ID_AFR0_EL1, AArch32 Auxiliary Feature Register 0, EL1

This register is read-only.

Figure 13-51: ID_AA64PFR1_EL1 bit assignments

63

0
4
3
8
7

SSBS

RES0

RES0, [63:8]

RES0

Reserved.

SSBS, [7:4]

PSTATE.SSBS. The possible values are:

0x2
AArch64 provides the PSTATE.SSBS mechanism to mark regions that are Speculative
Store Bypassing Safe (SSBS), and the MSR/MRS instructions to directly read and write
the PSTATE.SSBS ﬁeld.

RES0, [3:0]

RES0

Reserved.

Conﬁgurations

ID_AA64PFR1_EL1 is architecturally mapped to External register EDPFR.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.69 ID_AFR0_EL1, AArch32 Auxiliary Feature Register
0, EL1

The ID_AFR0_EL1 provides information about the IMPLEMENTATION DEFINED features of the PE in
AArch32. This register is not used in the Neoverse™ N1 core.

Bit ﬁeld descriptions
ID_AFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

## 13.70 ID_DFR0_EL1, AArch32 Debug Feature Register 0, EL1

Figure 13-52: ID_AFR0_EL1 bit assignments

0
31

63
32

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:0]

Reserved, RES0.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.70 ID_DFR0_EL1, AArch32 Debug Feature Register 0,
EL1

The ID_DFR0_EL1 provides top-level information about the debug system in AArch32.

Bit ﬁeld descriptions
ID_DFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-53: ID_DFR0_EL1 bit assignments

63
32

31
12 11
8
7
0
4
3
24 23
20 19
16 15
28 27

Reserved

PerfMon
MProfDbg
MMapTrc
CopTrc
CopSDbg
CopDbg

RES0

Reserved, [63:32]

Reserved.

RES0, [31:28]

RES0
Reserved.

PerfMon, [27:24]

Indicates support for performance monitor model:

4
Support for Performance Monitoring Unit version 3 (PMUv3) System
registers, with a 16-bit evtCount ﬁeld.

MProfDbg, [23:20]

Indicates support for memory-mapped debug model for M proﬁle cores:

0
This product does not support M proﬁle Debug architecture.

MMapTrc, [19:16]

Indicates support for memory-mapped trace model:

1
Support for Arm trace architecture, with memory-mapped access.

In the Trace registers, the ETMIDR gives more information about the implementation.

CopTrc, [15:12]

Indicates support for coprocessor-based trace model:

0
This product does not support Arm trace architecture.

RES0, [11:8]

RES0
Reserved.

## 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1

CopSDbg, [7:4]

Indicates support for coprocessor-based Secure debug model:

8
This product supports the Armv8.2 Debug architecture.

CopDbg, [3:0]

Indicates support for coprocessor-based debug model:

8
This product supports the Armv8.2 Debug architecture.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute
Register 0, EL1

The ID_ISAR0_EL1 provides information about the instruction sets implemented by the core in
AArch32.

Bit ﬁeld descriptions
ID_ISAR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-54: ID_ISAR0_EL1 bit assignments

63
32

31
28 27
24 23
20 19
16 15
12 11
8
7
4
3
0

Divide
Debug
Coproc
CmpBranch
Bitfield
BitCount
Swap

Reserved

RES0

Reserved, [63:32]

Reserved.

RES0, [31:28]

RES0
Reserved.

Divide, [27:24]

Indicates the implemented Divide instructions:

0x2

- SDIV and UDIV in the T32 instruction set.

- SDIV and UDIV in the A32 instruction set.

Debug, [23:20]

Indicates the implemented Debug instructions:

0x1
BKPT.

Coproc, [19:16]

Indicates the implemented coprocessor instructions:

0x0
None implemented, except for instructions separately attributed by
the architecture to provide access to AArch32 System registers and
System instructions.

CmpBranch, [15:12]

Indicates the implemented combined Compare and Branch instructions in the T32 instruction
set:

0x1
CBNZ and CBZ.

Bitﬁeld, [11:8]

Indicates the implemented bit ﬁeld instructions:

0x1
BFC, BFI, SBFX, and UBFX.

BitCount, [7:4]

Indicates the implemented Bit Counting instructions:

0x1
CLZ.

Swap, [3:0]

Indicates the implemented Swap instructions in the A32 instruction set:

0x0
None implemented.

Conﬁgurations

In an AArch64-only implementation, this register is UNKNOWN.

## 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1

Must be interpreted with ID_ISAR1_EL1, ID_ISAR2_EL1, ID_ISAR3_EL1, ID_ISAR4_EL1,
ID_ISAR5_EL1, and ID_ISAR6_EL1. See:

- 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on page 198.

- 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on page 200.

- 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on page 202.

- 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on page 204.

- 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on page 207.

- 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on page 209.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute
Register 1, EL1

The ID_ISAR1_EL1 provides information about the instruction sets implemented by the core in
AArch32.

Bit ﬁeld descriptions
ID_ISAR1_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-55: ID_ISAR1_EL1 bit assignments

63
32

31
28 27
24 23
20 19
16 15
12 11
8
7
4
3
0

Reserved, [63:32]

Reserved.

Jazelle, [31:28]

Indicates the implemented Jazelle state instructions:

0x1
Adds the BXJ instruction, and the J bit in the PSR.

|Reserved|Jazelle|Interwork|Immediate|IfThen|Extend|Except AR<br>_|Except|Endian|
|---|---|---|---|---|---|---|---|---|

Interwork, [27:24]

Indicates the implemented interworking instructions:

0x3

- The BX instruction, and the T bit in the PSR.

- The BLX instruction. The PC loads have BX-like behavior.

- Data-processing instructions in the A32 instruction set with the PC as the
destination and the S bit clear, have BX-like behavior.

Immediate, [23:20]

Indicates the implemented data-processing instructions with long immediates:

0x1

- The MOVT instruction.

- The MOV instruction encodings with zero-extended 16-bit immediates.

- The T32 ADD and SUB instruction encodings with zero-extended 12-bit immediates,
and other ADD, ADR, and SUB encodings cross-referenced by the pseudocode for
those encodings.

IfThen, [19:16]

Indicates the implemented If-Then instructions in the T32 instruction set:

0x1
The IT instructions, and the IT bits in the PSRs.

Extend, [15:12]

Indicates the implemented Extend instructions:

0x2

- The SXTB, SXTH, UXTB, and UXTH instructions.

- The SXTB16, SXTAB, SXTAB16, SXTAH, UXTB16, UXTAB, UXTAB16, and UXTAH instructions.

Except_AR, [11:8]

Indicates the implemented A proﬁle exception-handling instructions:

0x1
The SRS and RFE instructions, and the A proﬁle forms of the CPS
instruction.

Except, [7:4]

Indicates the implemented exception-handling instructions in the A32 instruction set:

0x1
The LDM (exception return), LDM (user registers), and STM (user
registers) instruction versions.

Endian, [3:0]

Indicates the implemented Endian instructions:

## 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1

0x1
The SETEND instruction, and the E bit in the PSRs.

Conﬁgurations

In an AArch64-only implementation, this register is UNKNOWN.

Must be interpreted with ID_ISAR0_EL1, ID_ISAR2_EL1, ID_ISAR3_EL1, ID_ISAR4_EL1,
ID_ISAR5_EL1, and ID_ISAR6_EL1. See:

- 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on page 196.

- 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on page 200.

- 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on page 202.

- 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on page 204.

- 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on page 207.

- 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on page 209.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute
Register 2, EL1

The ID_ISAR2_EL1 provides information about the instruction sets implemented by the core in
AArch32.

Bit ﬁeld descriptions
ID_ISAR2_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-56: ID_ISAR2_EL1 bit assignments

63
32

31
28 27
24 23
20 19
16 15
12 11
8
7
4
3
0

Reversal
PSR_AR
MultU
MultS
Mult
MemHint
LoadStore
Reserved

MultiAccessInt

Reserved, [63:32]

Reserved.

Reversal, [31:28]

Indicates the implemented Reversal instructions:

0x2
The REV, REV16, REVSH, and RBIT instructions.

PSR_AR, [27:24]

Indicates the implemented A and R proﬁle instructions to manipulate the PSR:

0x1
The MRS and MSR instructions, and the exception return forms of data-
processing instructions.

The exception return forms of the data-processing instructions are:

- In the A32 instruction set, data-processing instructions with the PC as the destination
and the S bit set.

- In the T32 instruction set, the SUBSPC, LR, #N instruction.

MultU, [23:20]

Indicates the implemented advanced unsigned Multiply instructions:

0x2
The UMULL, UMLAL, and UMAAL instructions.

MultS, [19:16]

Indicates the implemented advanced signed Multiply instructions.

0x3

- The SMULL and SMLAL instructions.

- The SMLABB, SMLABT, SMLALBB, SMLALBT, SMLALTB, SMLALTT, SMLATB, SMLATT, SMLAWB,
SMLAWT, SMULBB, SMULBT, SMULTB, SMULTT, SMULWB, SMULWT instructions, and the Q bit
in the PSRs.

- The SMLAD, SMLADX, SMLALD, SMLALDX, SMLSD, SMLSDX, SMLSLD, SMLSLDX, SMMLA, SMMLAR,
SMMLS, SMMLSR, SMMUL, SMMULR, SMUAD, SMUADX, SMUSD, and SMUSDX instructions.

Mult, [15:12]

Indicates the implemented additional Multiply instructions:

0x2
The MUL, MLA, and MLS instructions.

MultiAccessInt, [11:8]

Indicates the support for interruptible multi-access instructions:

0x0
No support. This means that the LDM and STM instructions are not
interruptible.

MemHint, [7:4]

Indicates the implemented memory hint instructions:

0x4
The PLD, PLI, and PLDWinstructions.

## 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1

LoadStore, [3:0]

Indicates the implemented additional load/store instructions:

0x2
The LDRD and STRD instructions.

The Load Acquire (LDAB, LDAH, LDA, LDAEXB, LDAEXH, LDAEX, and LDAEXD)
and Store Release (STLB, STLH, STL, STLEXB, STLEXH, STLEX, and
STLEXD) instructions.

Conﬁgurations

In an AArch64-only implementation, this register is UNKNOWN.

Must be interpreted with ID_ISAR0_EL1, ID_ISAR1_EL1, ID_ISAR3_EL1, ID_ISAR4_EL1,
ID_ISAR5_EL1, and ID_ISAR6_EL1. See:

- 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on page 196.

- 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on page 198.

- 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on page 202.

- 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on page 204.

- 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on page 207.

- 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on page 209.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute
Register 3, EL1

The ID_ISAR3_EL1 provides information about the instruction sets implemented by the core in
AArch32.

Bit ﬁeld descriptions
ID_ISAR3_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-57: ID_ISAR3_EL1 bit assignments

63
32

31
28 27
24 23
20 19
16 15
12 11
8
7
4
3
0

Reserved, [63:32]

Reserved.

T32EE, [31:28]

Indicates the implemented T32EE instructions:

0x0
None implemented.

TrueNOP, [27:24]

Indicates support for True NOP instructions:

0x1
True NOP instructions in both the A32 and T32 instruction sets, and
additional NOP-compatible hints.

T32Copy, [23:20]

Indicates the support for T32 non ﬂag-setting MOV instructions:

0x1
Support for T32 instruction set encoding T1 of the MOV (register)
instruction, copying from a low register to a low register.

TabBranch, [19:16]

Indicates the implemented Table Branch instructions in the T32 instruction set.

0x1
The TBB and TBH instructions.

SynchPrim, [15:12]

Indicates the implemented synchronization primitive instructions:

0x2

- The LDREX and STREX instructions.

- The CLREX, LDREXB, STREXB, and STREXH instructions.

- The LDREXD and STREXD instructions.

SVC, [11:8]

Indicates the implemented SVC instructions:

|Reserved|T32EE|TrueNOP|T32Copy|TabBranch|SynchPrim|SVC|SIMD|Saturate|
|---|---|---|---|---|---|---|---|---|

0x1
The SVC instruction.

SIMD, [7:4]

Indicates the implemented Single Instruction Multiple Data (SIMD) instructions.

0x3

- The SSAT and USAT instructions, and the Q bit in the PSRs.

- The PKHBT, PKHTB, QADD16, QADD8, QASX, QSUB16, QSUB8, QSAX, SADD16, SADD8, SASX,
SEL, SHADD16, SHADD8, SHASX, SHSUB16, SHSUB8, SHSAX, SSAT16, SSUB16, SSUB8, SSAX,
SXTAB16, SXTB16, UADD16, UADD8, UASX, UHADD16, UHADD8, UHASX, UHSUB16, UHSUB8,
UHSAX, UQADD16, UQADD8, UQASX, UQSUB16, UQSUB8, UQSAX, USAD8, USADA8, USAT16,
USUB16, USUB8, USAX, UXTAB16, UXTB16 instructions, and the GE[3:0] bits in the PSRs.
The SIMD ﬁeld relates only to implemented instructions that perform SIMD operations
on the general-purpose registers. In an implementation that supports Advanced SIMD
and ﬂoating-point instructions, MVFR0, MVFR1, and MVFR2 give information about
the implemented Advanced SIMD instructions.

Saturate, [3:0]

Indicates the implemented Saturate instructions:

0x1
The QADD, QDADD, QDSUB, QSUB Q bit in the PSRs.

Conﬁgurations

In an AArch64-only implementation, this register is UNKNOWN.

Must be interpreted with ID_ISAR0_EL1, ID_ISAR1_EL1, ID_ISAR2_EL1, ID_ISAR4_EL1,
ID_ISAR5_EL1, and ID_ISAR6_EL1. See:

- 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on page 196.

- 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on page 198.

- 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on page 200.

- 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on page 204.

- 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on page 207.

- 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on page 209.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1

13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute
Register 4, EL1

The ID_ISAR4_EL1 provides information about the instruction sets implemented by the core in
AArch32.

Bit ﬁeld descriptions
ID_ISAR4_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-58: ID_ISAR4_EL1 bit assignments

63
32

31
24 23
20 19
16 15
12 11
8
7
4 3
0

28 27

PSR_M
Barrier
SMC
WriteBack
WithShifts
Unpriv
Reserved

SWP_frac

SynchPrim_frac

Reserved, [63:32]

Reserved.

SWP_frac, [31:28]

Indicates support for the memory system locking the bus for SWP or SWPB instructions:

0x0
SWP and SWPB instructions not implemented.

PSR_M, [27:24]

Indicates the implemented M proﬁle instructions to modify the PSRs:

0x0
None implemented.

SynchPrim_frac, [23:20]

This ﬁeld is used with the ID_ISAR3.SynchPrim ﬁeld to indicate the implemented
synchronization primitive instructions:

0x0

- The LDREX and STREX instructions.

- The CLREX, LDREXB, LDREXH, STREXB, and STREXH instructions.

- The LDREXD and STREXD instructions.

Barrier, [19:16]

Indicates the supported Barrier instructions in the A32 and T32 instruction sets:

0x1
The DMB, DSB, and ISB barrier instructions.

SMC, [15:12]

Indicates the implemented SMC instructions:

0x0
None implemented.

WriteBack, [11:8]

Indicates the support for Write-Back addressing modes:

0x1
Core supports all the Write-Back addressing modes as deﬁned in
Arm®v8‑A.

WithShifts, [7:4]

Indicates the support for instructions with shifts.

0x4

- Support for shifts of loads and stores over the range LSL 0-3.

- Support for other constant shift options, both on load/store and other instructions.

- Support for register-controlled shift options.

Unpriv, [3:0]

Indicates the implemented unprivileged instructions.

0x2

- The LDRBT, LDRT, STRBT, and STRT instructions.

- The LDRHT, LDRSBT, LDRSHT, and STRHT instructions.

Conﬁgurations

In an AArch64-only implementation, this register is UNKNOWN.

Must be interpreted with ID_ISAR0_EL1, ID_ISAR1_EL1, ID_ISAR2_EL1, ID_ISAR3_EL1,
ID_ISAR5_EL1, and ID_ISAR6_EL1. See:

- 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on page 196.

- 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on page 198.

- 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on page 200.

- 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on page 202.

- 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on page 207.

- 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on page 209.

## 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute
Register 5, EL1

The ID_ISAR5_EL1 provides information about the instruction sets that the core implements.

Bit ﬁeld descriptions
ID_ISAR5_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-59: ID_ISAR5_EL1 bit assignments

63
32

31
12 11
8
7
0

23
24
27
28

4
3
16 15
20 19

Reserved

RDM

CRC32

SHA1
AES
SEVL
SHA2

RES0

Reserved, [63:32]

Reserved.

RES0, [31:28]

RES0
Reserved.

RDM, [27:24]

VQRDMLAH and VQRDMLSH instructions in AArch32. The value is:

0x1
VQRDMLAH and VQRDMLSH instructions are implemented.

RES0, [23:20]

RES0
Reserved.

CRC32, [19:16]

Indicates whether CRC32 instructions are implemented in AArch32 state. The value is:

0x1
CRC32B, CRC32H, CRC32W, CRC32CB, CRC32CH, and CRC32CW instructions
are implemented.

SHA2, [15:12]

Indicates whether SHA2 instructions are implemented in AArch32 state. The possible values
are:

0x0
No SHA2 instructions implemented. This is the value when the
Cryptographic Extensions are not implemented or are disabled.
0x1
SHA256H, SHA256H2, SHA256SU0, and SHA256SU1 instructions are
implemented. This is the value when the Cryptographic Extensions
are implemented and enabled.

SHA1, [11:8]

Indicates whether SHA1 instructions are implemented in AArch32 state. The possible values
are:

0x0
No SHA1 instructions implemented. This is the value when the
Cryptographic Extensions are not implemented or are disabled.
0x1
SHA1C, SHA1P, SHA1M, SHA1H, SHA1SU0, and SHA1SU1 instructions are
implemented. This is the value when the Cryptographic Extensions
are implemented and enabled.

AES, [7:4]

Indicates whether AES instructions are implemented in AArch32 state. The possible values
are:

0x0
No AES instructions implemented. This is the value when the
Cryptographic Extensions are not implemented or are disabled.
0x2
- AESE, AESD, AESMC, and AESIMC implemented.

- PMULL and PMULL2 instructions operating on 64-bit data.

This is the value when the Cryptographic Extensions are
implemented and enabled.

SEVL, [3:0]

Indicates whether the SEVL instruction is implemented:

0x1
SEVL implemented to send event local.

Conﬁgurations

ID_ISAR5 must be interpreted with ID_ISAR0_EL1, ID_ISAR1_EL1, ID_ISAR2_EL1,
ID_ISAR3_EL1, ID_ISAR4_EL1, and ID_ISAR6_EL1. See:

- 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on page 196.

- 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on page 198.

- 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on page 200.

## 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1

- 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on page 202.

- 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on page 204.

- 13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute Register 6, EL1 on page 209.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.77 ID_ISAR6_EL1, AArch32 Instruction Set Attribute
Register 6, EL1

The ID_ISAR6_EL1 provides information about the instruction sets that the core implements.

Bit ﬁeld descriptions
ID_ISAR6_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-60: ID_ISAR6_EL1 bit assignments

63
32

31
8
7
0

4
3

Reserved

DP

RES0

Reserved, [63:32]

Reserved.

RES0, [31:8]

RES0
Reserved.

DP, [7:4]

UDOT and SDOT instructions. The value is:

0b0001
UDOT and SDOT instructions are implemented.

## 13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature Register 0, EL1

RES0, [3:0]

RES0
Reserved.

Conﬁgurations

There is one copy of this register that is used in both Secure and Non-secure states.

ID_ISAR6_EL1 must be interpreted with ID_ISAR0_EL1, ID_ISAR1_EL1, ID_ISAR2_EL1,
ID_ISAR3_EL1, ID_ISAR4_EL1, and ID_ISAR5_EL1. See:

- 13.71 ID_ISAR0_EL1, AArch32 Instruction Set Attribute Register 0, EL1 on page 196.

- 13.72 ID_ISAR1_EL1, AArch32 Instruction Set Attribute Register 1, EL1 on page 198.

- 13.73 ID_ISAR2_EL1, AArch32 Instruction Set Attribute Register 2, EL1 on page 200.

- 13.74 ID_ISAR3_EL1, AArch32 Instruction Set Attribute Register 3, EL1 on page 202.

- 13.75 ID_ISAR4_EL1, AArch32 Instruction Set Attribute Register 4, EL1 on page 204.

- 13.76 ID_ISAR5_EL1, AArch32 Instruction Set Attribute Register 5, EL1 on page 207.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature
Register 0, EL1

The ID_MMFR0_EL1 provides information about the memory model and memory management
support in AArch32.

Bit ﬁeld descriptions
ID_MMFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-61: ID_MMFR0_EL1 bit assignments

63
32

31
12 11
8
7
0

4
3
28 27
24 23
20 19
16 15

Reserved, [63:32]

Reserved.

|Reserved|InnerShr|FCSE|AuxReg|TCM|ShareLvl|OuterShr|PMSA|VMSA|
|---|---|---|---|---|---|---|---|---|

InnerShr, [31:28]

Indicates the innermost Shareability domain implemented:

0x1
Implemented with hardware coherency support.

FCSE, [27:24]

Indicates support for Fast Context Switch Extension (FCSE):

0x0
Not supported.

AuxReg, [23:20]

Indicates support for Auxiliary registers:

0x2
Support for Auxiliary Fault Status Registers (AIFSR and ADFSR) and
Auxiliary Control Register.

TCM, [19:16]

Indicates support for TCMs and associated DMAs:

0x0
Not supported.

ShareLvl, [15:12]

Indicates the number of Shareability levels implemented:

0x1
Two levels of Shareability implemented.

OuterShr, [11:8]

Indicates the outermost Shareability domain implemented:

0x1
Implemented with hardware coherency support.

PMSA, [7:4]

Indicates support for a Protected Memory System Architecture (PMSA):

0x0
Not supported.

VMSA, [3:0]

Indicates support for a Virtual Memory System Architecture (VMSA).

0x5
Support for:

- VMSAv7, with support for remapping and the Access ﬂag.

- The PXN bit in the Short-descriptor translation table format
descriptors.

- The Long-descriptor translation table format.

## 13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature Register 1, EL1

Conﬁgurations

Must be interpreted with ID_MMFR1_EL1, ID_MMFR2_EL1, ID_MMFR3_EL1, and
ID_MMFR4_EL1. See:

- 13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature Register 1, EL1 on page 212.

- 13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature Register 2, EL1 on page 214.

- 13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature Register 3, EL1 on page 216.

- 13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature Register 4, EL1 on page 218.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature
Register 1, EL1

The ID_MMFR1_EL1 provides information about the memory model and memory management
support in AArch32.

Bit ﬁeld descriptions
ID_MMFR1_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-62: ID_MMFR1_EL1 bit assignments

63
32
0

31
28 27
24 23
20 19
16 15
12 11
8
7
4
3

Reserved, [63:32]

Reserved.

BPred, [31:28]

Indicates branch predictor management requirements:

0x4
For execution correctness, branch predictor requires no ﬂushing at
any time.

|Reserved|BPred|L1TstCln|L1Uni|L1Hvd|L1UniSW|L1HvdSW|L1UniVA|L1HvdVA|
|---|---|---|---|---|---|---|---|---|

L1TstCln, [27:24]

Indicates the supported L1 data cache test and clean operations, for Harvard or uniﬁed cache
implementation:

0x0
None supported.

L1Uni, [23:20]

Indicates the supported entire L1 cache maintenance operations, for a uniﬁed cache
implementation:

0x0
None supported.

L1Hvd, [19:16]

Indicates the supported entire L1 cache maintenance operations, for a Harvard cache
implementation:

0x0
None supported.

L1UniSW, [15:12]

Indicates the supported L1 cache line maintenance operations by set/way, for a uniﬁed cache
implementation:

0x0
None supported.

L1HvdSW, [11:8]

Indicates the supported L1 cache line maintenance operations by set/way, for a Harvard
cache implementation:

0x0
None supported.

L1UniVA, [7:4]

Indicates the supported L1 cache line maintenance operations by MVA, for a uniﬁed cache
implementation:

0x0
None supported.

L1HvdVA, [3:0]

Indicates the supported L1 cache line maintenance operations by MVA, for a Harvard cache
implementation:

0x0
None supported.

Conﬁgurations

Must be interpreted with ID_MMFR0_EL1, ID_MMFR2_EL1, ID_MMFR3_EL1, and
ID_MMFR4_EL1. See:

- 13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature Register 0, EL1 on page 210.

- 13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature Register 2, EL1 on page 214.

## 13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature Register 2, EL1

- 13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature Register 3, EL1 on page 216.

- 13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature Register 4, EL1 on page 218.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature
Register 2, EL1

The ID_MMFR2_EL1 provides information about the implemented memory model and memory
management support in AArch32.

Bit ﬁeld descriptions
ID_MMFR2_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-63: ID_MMFR2_EL1 bit assignments

63
32

31
12 11
8
7
0

4
3
28 27
24 23
20 19
16 15

Reserved, [63:32]

Reserved.

HWAccFlg, [31:28]

Hardware access ﬂag. Indicates support for a hardware access ﬂag, as part of the VMSAv7
implementation:

0x0
Not supported.

WFIStall, [27:24]

Wait For Interrupt Stall. Indicates the support for Wait For Interrupt (WFI) stalling:

0x1
Support for WFI stalling.

MemBarr, [23:20]

Memory Barrier. Indicates the supported CP15 memory barrier operations.

|Reserved|HWAccFlg|WFIStall|MemBarr|UniTLB|HvdTLB|LL1HvdRng|L1HvdBG|L1HvdFG|
|---|---|---|---|---|---|---|---|---|

0x2
Supported CP15 memory barrier operations are:

- Data Synchronization Barrier (DSB).

- Instruction Synchronization Barrier (ISB).

- Data Memory Barrier (DMB).

UniTLB, [19:16]

Uniﬁed TLB. Indicates the supported TLB maintenance operations, for a uniﬁed TLB
implementation.

0x6
Supported uniﬁed TLB maintenance operations are:

- Invalidate all entries in the TLB.

- Invalidate TLB entry by MVA.

- Invalidate TLB entries by ASID match.

- Invalidate instruction TLB and data TLB entries by MVA All ASID.
This is a shared uniﬁed TLB operation.

- Invalidate Hyp mode uniﬁed TLB entry by MVA.

- Invalidate entire Non-secure EL1 and EL0 uniﬁed TLB.

- Invalidate entire Hyp mode uniﬁed TLB.

- TLBIMVALIS, TLBIMVAALIS, TLBIMVALHIS, TLBIMVAL, TLBIMVAAL, and
TLBIMVALH.

- TLBIIPAS2IS, TLBIIPAS2LIS, TLBIIPAS2, and TLBIIPAS2L.

HvdTLB, [15:12]

Harvard TLB. Indicates the supported TLB maintenance operations, for a Harvard TLB
implementation:

0x0
Not supported.

LL1HvdRng, [11:8]

L1 Harvard cache Range. Indicates the supported L1 cache maintenance range operations,
for a Harvard cache implementation:

0x0
Not supported.

L1HvdBG, [7:4]

L1 Harvard cache Background fetch. Indicates the supported L1 cache background prefetch
operations, for a Harvard cache implementation:

0x0
Not supported.

L1HvdFG, [3:0]

L1 Harvard cache Foreground fetch. Indicates the supported L1 cache foreground prefetch
operations, for a Harvard cache implementation:

## 13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature Register 3, EL1

0x0
Not supported.

Conﬁgurations

Must be interpreted with ID_MMFR0_EL1, ID_MMFR1_EL1, ID_MMFR3_EL1, and
ID_MMFR4_EL1. See:

- 13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature Register 0, EL1 on page 210.

- 13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature Register 1, EL1 on page 212.

- 13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature Register 3, EL1 on page 216.

- 13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature Register 4, EL1 on page 218.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature
Register 3, EL1

The ID_MMFR3_EL1 provides information about the memory model and memory management
support in AArch32.

Bit ﬁeld descriptions
ID_MMFR3_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-64: ID_MMFR3_EL1 bit assignments

63
32

31
12 11
8
7
0
4
3
28 27
24 23
20 19
16 15

Reserved, [63:32]

Reserved.

Supersec, [31:28]

Supersections. Indicates support for supersections:

0x0
Supersections supported.

|Reserved|Supersec|CMemSz|CohWalk|PAN|MaintBcst|BPMaint|CMaintSW|CMaintVA|
|---|---|---|---|---|---|---|---|---|

CMemSz, [27:24]

Cached memory size. Indicates the size of physical memory that is supported by the core
caches:

0x2
1TByte or more, corresponding to a 40-bit, or larger physical address
range.

CohWalk, [23:20]

Coherent walk. Indicates whether translation table updates require a clean to the point of
uniﬁcation:

0x1
Updates to the translation tables do not require a clean to the point
of uniﬁcation to ensure visibility by subsequent translation table
walks.

PAN, [19:16]

Privileged Access Never.

0x2
PAN supported and new ATS1CPRP and ATS1CPWP instructions supported.

MaintBcst, [15:12]

Maintenance broadcast. Indicates whether cache, TLB, and branch predictor operations are
broadcast:

0x2
Cache, TLB, and branch predictor operations aﬀect structures
according to Shareability and deﬁned behavior of instructions.

BPMaint, [11:8]

Branch predictor maintenance. Indicates the supported branch predictor maintenance
operations.

0x2
Supported branch predictor maintenance operations are:

- Invalidate all branch predictors.

- Invalidate branch predictors by MVA.

CMaintSW, [7:4]

Cache maintenance by set/way. Indicates the supported cache maintenance operations by
set/way.

0x1
Supported hierarchical cache maintenance operations by set/way
are:

- Invalidate data cache by set/way.

- Clean data cache by set/way.

- Clean and invalidate data cache by set/way.

## 13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature Register 4, EL1

CMaintVA, [3:0]

Cache maintenance by Virtual Address (VA). Indicates the supported cache maintenance
operations by VA.

0x1
Supported hierarchical cache maintenance operations by VA are:

- Invalidate data cache by VA.

- Clean data cache by VA.

- Clean and invalidate data cache by VA.

- Invalidate instruction cache by VA.

- Invalidate all instruction cache entries.

Conﬁgurations

Must be interpreted with ID_MMFR0_EL1, ID_MMFR1_EL1, ID_MMFR2_EL1, and
ID_MMFR4_EL1. See:

- 13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature Register 0, EL1 on page 210.

- 13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature Register 1, EL1 on page 212.

- 13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature Register 2, EL1 on page 214.

- 13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature Register 4, EL1 on page 218.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.82 ID_MMFR4_EL1, AArch32 Memory Model Feature
Register 4, EL1

The ID_MMFR4_EL1 provides information about the memory model and memory management
support in AArch32.

Bit ﬁeld descriptions
ID_MMFR4_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-65: ID_MMFR4_EL1 bit assignments

63
32

31
8
7
0
4
3

11
12
15
16
19
20
23
24

Reserved, [63:32]

Reserved.

RAZ, [31:24]

Read-As-Zero.

LSM, [23:20]

Load/Store Multiple. Indicates whether adjacent loads or stores can be combined. The value
is:

0x0
LSMAOE and nTLSMD bit not supported.

HPDS, [19:16]

Presence of Hierarchical Disables. Enables an operating system or hypervisor to hand over
up to 4 bits of the last level translation table descriptor (bits[62:59] of the translation table
entry) for use by hardware for IMPLEMENTATION DEFINED usage. The value is:

0x2
Hierarchical Permission Disables and Hardware allocation of
bits[62:59] supported.

CNP, [15:12]

Common Not Private. Indicates support for selective sharing of TLB entries across multiple
PEs. The value is:

0x1
CnP bit supported.

XNX, [11:8]

Execute Never. Indicates whether the stage 2 translation tables allows the stage 2 control of
whether memory is executable at EL1 independent of whether memory is executable at EL0.
The value is:

0x1
EL0/EL1 execute control distinction at stage 2 bit supported.

AC2, [7:4]

Indicates the extension of the ACTLR and HACTLR registers using ACTLR2 and HACTLR2.
The value is:

|Reserved|RAZ|LSM|HPDS|CNP|XNX|AC2|SpecSEI|
|---|---|---|---|---|---|---|---|

## 13.83 ID_PFR0_EL1, AArch32 Processor Feature Register 0, EL1

0x1
ACTLR2 and HACTLR2 are implemented.

SpecSEI, [3:0]

Describes whether the core can generate SError interrupt exceptions from Speculative reads
of memory, including Speculative instruction fetches. The value is:

0x0
The core never generates an SError interrupt due to an External
abort on a Speculative read.

Conﬁgurations

There is one copy of this register that is used in both Secure and Non-secure states.

Must be interpreted with ID_MMFR0_EL1, ID_MMFR1_EL1, ID_MMFR2_EL1, and
ID_MMFR3_EL1. See:

- 13.78 ID_MMFR0_EL1, AArch32 Memory Model Feature Register 0, EL1 on page 210.

- 13.79 ID_MMFR1_EL1, AArch32 Memory Model Feature Register 1, EL1 on page 212.

- 13.80 ID_MMFR2_EL1, AArch32 Memory Model Feature Register 2, EL1 on page 214.

- 13.81 ID_MMFR3_EL1, AArch32 Memory Model Feature Register 3, EL1 on page 216.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.83 ID_PFR0_EL1, AArch32 Processor Feature Register
0, EL1

The ID_PFR0_EL1 provides top-level information about the instruction sets supported by the core
in AArch32.

Bit ﬁeld descriptions
ID_PFR0_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-66: ID_PFR0_EL1 bit assignments

63
32

31
12 11
8
7
0

28

27

19
20

16 15
4
3

CSV2
Reserved

RAS

State0
State3

State2
State1

RES0

Reserved, [63:32]

Reserved.

RAS, [31:28]

RAS extension version. The value is:

0x1
Version 1 of the RAS extension is present.

RES0, [27:20]

RES0
Reserved.

CSV2, [19:16]

0x0
This device does not disclose whether branch targets trained in one
context can aﬀect speculative execution in a diﬀerent context.
0x1
Branch targets trained in one context cannot aﬀect speculative
execution in a diﬀerent hardware described context. This is the reset
value.

State3, [15:12]

Indicates support for Thumb Execution Environment (T32EE) instruction set. This value is:

0x0
Core does not support the T32EE instruction set.

State2, [11:8]

Indicates support for Jazelle. This value is:

0x1
Core supports trivial implementation of Jazelle.

State1, [7:4]

Indicates support for T32 instruction set. This value is:

## 13.84 ID_PFR1_EL1, AArch32 Processor Feature Register 1, EL1

0x3
Core supports T32 encoding after the introduction of Thumb-2
technology, and for all 16-bit and 32-bit T32 basic instructions.

State0, [3:0]

Indicates support for A32 instruction set. This value is:

0x1
A32 instruction set implemented.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

13.84 ID_PFR1_EL1, AArch32 Processor Feature Register
1, EL1

The ID_PFR1_EL1 provides information about the programmers model and architecture extensions
that are supported by the core.

Bit ﬁeld descriptions
ID_PFR1_EL1 is a 64-bit register, and must be interpreted with ID_PFR0. It is part of the
Identiﬁcation registers functional group.

This register is read-only.

Figure 13-67: ID_PFR1_EL1 bit assignments

63
32

31
12 11
8
7
0

20 19
27
28

24 23

4
3
16 15

Virt_frac
Sec_frac
Reserved

GIC CPU

GenTimer
MProgMod
Security
ProgMod

Virtualization

Reserved, [63:32]

Reserved.

GIC CPU, [31:28]

GIC CPU support:

0
GIC CPU interface is disabled, GICCDISABLE is HIGH, or not implemented.

1
GIC CPU interface is implemented and enabled, GICCDISABLE is LOW.

Virt_frac, [27:24]

0
No features from the Armv7 Virtualization Extensions are implemented.

Sec_frac, [23:20]

0
No features from the Armv7 Virtualization Extensions are implemented.

GenTimer, [19:16]

Generic Timer support:

1
Generic Timer supported.

Virtualization, [15:12]

Virtualization support:

0
Virtualization not implemented.

MProgMod, [11:8]

M proﬁle programmers model support:

0
Not supported.

Security, [7:4]

Security support:

0
Security not implemented.

ProgMod, [3:0]

Indicates support for the standard programmers model for Armv4 and later.

Model must support User, FIQ, IRQ, Supervisor, Abort, Undeﬁned, and System modes:

0
Not supported.

Conﬁgurations

There are no conﬁguration notes.

## 13.85 ID_PFR2_EL1, AArch32 Processor Feature Register 2, EL1

13.85 ID_PFR2_EL1, AArch32 Processor Feature Register
2, EL1

The ID_PFR2_EL1 provides information about the programmers model and architecture extensions
that are supported by the core.

Bit ﬁeld descriptions
ID_PFR2_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-68: ID_PFR2_EL1 bit assignments

63
32

31
0
4
3

7

SSBS
Reserved

CSV3

RES0

Reserved, [63:32]

Reserved.

RES0, [31:8]

RES0

Reserved.

SSBS, [7:4]

1
AArch32 provides the PSTATE.SSBS mechanism to mark regions that are Speculative
Store Bypassing Safe (SSBS).

CSV3, [3:0]

1
Data that are loaded under speculation with a permission or domain fault cannot be
used to form an address or generate condition codes to be used by instructions newer
than the load in the speculative sequence. This is the reset value.

## 13.86 LORC_EL1, LORegion Control Register, EL1

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the Arm®
Architecture Reference Manual for A-proﬁle architecture.

The LORC_EL1 register enables and disables LORegions, and selects the current LORegion
descriptor.

Bit ﬁeld descriptions
LORC_EL1 is a 64-bit register and is part of the Virtual memory control registers functional group.

Figure 13-69: LORC_EL1 bit assignments

63
0

3
4

1
2

DS

EN

RES0

RES0, [63:4]

Reserved, RES0.

DS, [3:2]

Descriptor Select. Number that selects the current LORegion descriptor that is accessed by
the LORSA_EL1, LOREA_EL1, and LORN_EL1 registers.

RES0, [1]

Reserved, RES0.

EN, [0]

Enable. The possible values are:

0
Disabled. This is the reset value.
1
Enabled.

Conﬁgurations

RW ﬁelds in this register reset to architecturally UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.87 LORID_EL1, LORegion ID Register, EL1

The LORID_EL1 ID register indicates the supported number of LORegions and LORegion
descriptors.

Bit ﬁeld descriptions
LORID_EL1 is a 64-bit register.

Figure 13-70: LORID_EL1 bit assignments

63
0
8
7
15
16
23
24

LR
LD

RES0

RES0, [63:24]

Reserved, RES0.

LD, [23:16]

Number of LORegion descriptors supported by the implementation, expressed as binary 8-bit
number. The value is:

0x04
Four LORegion descriptors are supported.

RES0, [15:8]

Reserved, RES0.

LR, [7:0]

Number of LORegions supported by the implementation, expressed as a binary 8-bit number.
The value is:

0x04
Four LORegions are supported.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.88 LORN_EL1, LORegion Number Register, EL1

## 13.89 MDCR_EL3, Monitor Debug Configuration Register, EL3

13.88 LORN_EL1, LORegion Number Register, EL1

The LORN_EL1 register holds the number of the LORegion described in the current LORegion
descriptor that is selected by LORC_EL1.DS.

Bit ﬁeld descriptions
LORN_EL1 is a 64-bit register and is part of the Virtual memory control registers functional group.

Figure 13-71: LORN_EL1 bit assignments

63
0

1
2

Num

RES0

RES0, [63:2]

Reserved, RES0.

Num, [1:0]

Indicates the LORegion number.

Conﬁgurations

RW ﬁelds in this register reset to architecturally UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.89 MDCR_EL3, Monitor Debug Conﬁguration Register,
EL3

The MDCR_EL3 provides conﬁguration options for Security to self-hosted debug.

Bit ﬁeld descriptions
MDCR_EL3 is a 64-bit register, and is part of:

- The Debug registers functional group.

- The Security registers functional group.

Figure 13-72: MDCR_EL3 bit assignments

63
32

31
0

20 19
21
22
16 15
17
18
14 13
11 10 9
6
7
8
5

12

Reserved

EDAD
EPMAD

TDA
TDOSA

TPM

NSPB

SPD32
SDD
SPME

res0

Reserved, [63:32]

Reserved.

RES0, [31:22]

RES0
Reserved.

EPMAD, [21]

External debugger access to Performance Monitors registers disabled. This disables access to
these registers by an external debugger. The possible values are:

0
Access to Performance Monitors registers from external debugger is
permitted.
1
Access to Performance Monitors registers from external debugger is
disabled, unless overridden by authentication interface.

EDAD, [20]

External debugger access to breakpoint and watchpoint registers disabled. This disables
access to these registers by an external debugger. The possible values are:

0
Access to breakpoint and watchpoint registers from external
debugger is permitted.
1
Access to breakpoint and watchpoint registers from external
debugger is disabled, unless overridden by authentication interface.

SPME, [17]

Secure performance monitors enable. This enables event counting exceptions from Secure
state. The possible values are:

0
Event counting prohibited in Secure state.
1
Event counting allowed in Secure state.

SPD32, [15:14]

RES0

Reserved.

NSPB, [13:12]

Non-secure Proﬁling Buﬀer. Controls the owning translation regime and accesses to
Statistical Proﬁling and Proﬁling Buﬀer control registers. The possible values are:

00
Proﬁling Buﬀer uses Secure Virtual Addresses. Statistical Proﬁling
enabled in Secure state and disabled in Non-secure state. Accesses
to Statistical Proﬁling and Proﬁling Buﬀer controls at EL2 and EL1 in
both Security states generate Trap exceptions to EL3.
01
Proﬁling Buﬀer uses Secure Virtual Addresses. Statistical Proﬁling
enabled in Secure state and disabled in Non-secure state. Accesses
to Statistical Proﬁling and Proﬁling Buﬀer controls in Non-secure
state generate Trap exceptions to EL3.
10
Proﬁling Buﬀer uses Non-secure Virtual Addresses. Statistical
Proﬁling enabled in Non-secure state and disabled in Secure state.
Accesses to Statistical Proﬁling and Proﬁling Buﬀer controls at EL2
and EL1 in both Security states generate Trap exceptions to EL3.
11
Proﬁling Buﬀer uses Non-secure Virtual Addresses. Statistical
Proﬁling enabled in Non-secure state and disabled in Secure state.
Accesses to Statistical Proﬁling and Proﬁling Buﬀer controls at Secure
EL1 generate Trap exceptions to EL3.

RES0, [11]

RES0
Reserved.

TDOSA, [10]

Trap accesses to the OS Debug system registers, OSLAR_EL1, OSLSR_EL1, OSDLR_EL1, and
DBGPRCR_EL1 OS.

0
Accesses are not trapped.
1
Accesses to the OS Debug system registers are trapped to EL3.

The reset value is UNKNOWN.

TDA, [9]

Trap accesses to the remaining sets of Debug registers to EL3.

0
Accesses are not trapped.
1
Accesses to the remaining Debug system registers are trapped to
EL3.

The reset value is UNKNOWN.

## 13.90 MIDR_EL1, Main ID Register, EL1

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The MIDR_EL1 provides identiﬁcation information for the core, including an implementer code for
the device and a device ID number.

Bit ﬁeld descriptions
MIDR_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register is read-only.

Figure 13-73: MIDR_EL1 bit assignments

63
32

31
23
20 19
16 15
4
3
0

24

Reserved, [63:32]

Reserved.

Implementer, [31:24]

Indicates the implementer code. This value is:

0x41
ASCII character 'A' - implementer is Arm® Limited.

Variant, [23:20]

Indicates the variant number of the core. This is the major revision number x in the rx part of
the rxpy description of the product revision status. This value is:

0x4
r4p1.

Architecture, [19:16]

Indicates the architecture code. This value is:

0xF
Deﬁned by CPUID scheme.

|Reserved|Implementer|Variant|Architecture|PartNum|Revision|
|---|---|---|---|---|---|

## 13.91 MPIDR_EL1, Multiprocessor Affinity Register, EL1

PartNum, [15:4]

Indicates the primary part number. This value is:

0xD0C
Neoverse™ N1 core.

Revision, [3:0]

Indicates the minor revision number of the core. This is the minor revision number y in the py
part of the rxpy description of the product revision status. This value is:

0x1
r4p1.

Conﬁgurations

The MIDR_EL1 is architecturally mapped to external MIDR_EL1 register.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.91 MPIDR_EL1, Multiprocessor Aﬃnity Register, EL1

The MPIDR_EL1 provides an additional core identiﬁcation mechanism for scheduling purposes in a
cluster.

Bit ﬁeld descriptions
MPIDR_EL1 is a 64-bit register, and is part of the Other system control registers functional group.

This register is read-only.

Figure 13-74: MPIDR_EL1 bit assignments

0
63
16 15
8
7

40 39
25

32 31

29
30

23
24

11 10

Aff3

U

Aff2

Aff1
Aff0

Aff1

MT

RES1

RES0

RES0, [63:40]

RES0
Reserved.

Aﬀ3, [39:32]

Aﬃnity level 3. Highest level aﬃnity ﬁeld.

CLUSTERID

Indicates the value read in the CLUSTERIDAFF3 conﬁguration signal.

RES1, [31]

RES1
Reserved.

U, [30]

Indicates a single core system, as distinct from core 0 in a cluster. This value is:

0
Core is part of a multiprocessor system. This is the value for
implementations with more than one core, and for implementations
with an ACE manager interface or a CHI requester interface.

RES0, [29:25]

RES0
Reserved.

MT, [24]

Indicates whether the lowest level of aﬃnity consists of logical cores that are implemented
using a multithreading type approach. This value is:

1
Performance of PEs at the lowest aﬃnity level is interdependent.

Aﬃnity0 represents threads. Neoverse™ N1 is not multithreaded, but
may be in a system with other cores that are multithreaded.

Aﬀ2, [23:16]

Aﬃnity L2. Second highest level aﬃnity ﬁeld.

CLUSTERID

Indicates the value read in the CLUSTERIDAFF2 conﬁguration signal.

Aﬀ1, [15:11]

Part of Aﬃnity L1. Third highest level aﬃnity ﬁeld.

RAZ
Read-As-Zero.

Aﬀ1, [10:8]

Part of Aﬃnity L1. Third highest level aﬃnity ﬁeld.

CPUID
Identiﬁcation number for each CPU in the cluster:

0x0
MP1: CPUID: 0.
0x7
MP8: CPUID: 7.

## 13.92 PAR_EL1, Physical Address Register, EL1

Aﬀ0, [7:0]

Aﬃnity level 0. The level identiﬁes individual threads within a multithreaded core. The
Neoverse™ N1 core is single-threaded, so this ﬁeld has the value 0x00.

Conﬁgurations

MPIDR_EL1[31:0] is mapped to external register EDDEVAFF0.

MPIDR_EL1[63:32] is mapped to external register EDDEVAFF1.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The PAR_EL1 returns the output address from an address translation instruction that executed
successfully, or fault information if the instruction did not execute successfully.

Bit ﬁeld descriptions, PAR_EL1.F is 0
The following ﬁgure shows the PAR bit assignments when PAR.F is 0.

Figure 13-75: PAR bit assignments, PAR_EL1.F is 0

63
0
6
7
1
12

47
48
55
56

8
9
10
11

ATTR

PA

F
SH

LPAE

IMP DEF

NS

res0

IMP DEF, [10]

IMPLEMENTATION DEFINED. Bit[10] is RES0.

F, [0]

Indicates whether the instruction performed a successful address translation.

0
Address translation completed successfully.
1
Address translation aborted.

Conﬁgurations

There are no conﬁguration notes.

## 13.93 REVIDR_EL1, Revision ID Register, EL1

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

Bit ﬁeld descriptions, PAR_EL1.F is 1

See the Arm® Architecture Reference Manual for A-proﬁle architecture.

The REVIDR_EL1 provides revision information, additional to MIDR_EL1, that identiﬁes minor ﬁxes
(errata) which might be present in a speciﬁc implementation of the Neoverse™ N1 core.

Bit ﬁeld descriptions
REVIDR_EL1 is a 64-bit register, and is part of the Identiﬁcation registers functional group.

This register resets to value 0x0000000000000000.

This register is read-only.

Figure 13-76: REVIDR_EL1 bit assignments

63
32

31
0

Reserved, [63:32]

Reserved.

IMPLEMENTATION DEFINED, [31:0]

IMPLEMENTATION DEFINED.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

|Reserved|IMPLEMENTATION DEFINED|
|---|---|

## 13.94 RMR_EL3, Reset Management Register

## 13.95 RVBAR_EL3, Reset Vector Base Address Register, EL3

13.94 RMR_EL3, Reset Management Register

The RMR_EL3 controls the Execution state that the core boots into and allows request of a Warm
reset.

Bit ﬁeld descriptions
RMR_EL3 is a 64-bit register, and is part of the Reset management registers functional group.

Figure 13-77: RMR_EL3 bit assignments

63
0
1
2

RR

res1

res0

RES0, [63:2]

RES0
Reserved.

RR, [1]

Reset Request. The possible values are:

0
This is the reset value on both a Warm and a Cold reset.
1
Requests a Warm reset.

The bit is strictly a request.

RES1, [0]

RES1
Reserved.

Conﬁgurations

There are no conﬁguration notes.

Details that are not provided in this description are architecturally deﬁned. See the Arm®
Architecture Reference Manual for A-proﬁle architecture.

13.95 RVBAR_EL3, Reset Vector Base Address Register,
EL3

RVBAR_EL3 contains the IMPLEMENTATION DEFINED address that execution starts from after reset.

## 13.96 SCTLR_EL1, System Control Register, EL1

Bit ﬁeld descriptions
RVBAR_EL3 is a 64-bit register, and is part of the Reset management registers functional group.

This register is read-only.

Figure 13-78: RVBAR_EL3 bit assignments

63

0

Reset Vector Base Address

RVBA, [63:0]

Reset Vector Base Address. The address that execution starts from after reset. Bits[1:0] of
this register are 0b00, as this address must be aligned, and bits [63:48] are 0x0000 because
the address must be within the physical address size that is supported by the core.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The SCTLR_EL1 provides top-level control of the system, including its memory system, at EL1 and
EL0.

Bit ﬁeld descriptions
SCTLR_EL1 is a 64-bit register, and is part of the Other system control registers functional group.

This register resets to 0x0000000030D50838.

Figure 13-79: SCTLR_EL1 bit assignments

63
43
45

44

25
26
24 23
20
18
19
17 16 15
13
14
12 11 10
8
9
7
6
5
3
4
2
1
27
28
29
30
21
22

0

M
A
C
I

DSSBS

UCI

DZE

SA

nTWI
UCT
E0E

SA0
EE

CP15BEN

SPAN

IESB

WXN
nTWE

ITD
SED
UMA

RES1

RES0

RES0, [63:45]

RES0
Reserved

DSSBS, [44]

DSSBS is used to set the new PSTATE bit, SSBS (Speculative Store Bypassing Safe).

0
PSTATE.SSBS is set to 0 on an exception taken to this Exception
level. This is the reset value.
1
PSTATE.SSBS is set to 1 on an exception taken to this Exception
level.

RES0, [43:30]

RES0
Reserved

RES1, [29:28]

RES1
Reserved

RES0, [27]

RES0
Reserved

EE, [25]

Exception endianness. The value of this bit controls the endianness for explicit data accesses
at EL1. This value also indicates the endianness of the translation table data for translation
table lookups. The possible values of this bit are:

0
Little-endian.
1
Big-endian.

## 13.97 SCTLR_EL2, System Control Register, EL2

ITD, [7]

This ﬁeld is RAZ/WI.

RES0, [6]

RES0
Reserved

CP15BEN, [5]

CP15 barrier enable. The possible values are:

0
CP15 barrier operations disabled. Their encodings are UNDEFINED.
1
CP15 barrier operations enabled.

M, [0]

MMU enable. The possible values are:

0
EL1 and EL0 stage 1 MMU disabled.
1
EL1 and EL0 stage 1 MMU enabled.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The SCTLR_EL2 provides top-level control of the system, including its memory system at EL2.

Bit ﬁeld descriptions
SCTLR_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group

- The Other system control registers functional group

Figure 13-80: SCTLR_EL2 bit assignments

45 44
63

43
0

30 29 28 27
24 23 22 21
17 16 15 14 13
10
6
5

25
26
20 19 18
12 11
2
1
4
3

I
C A M

DSSBS
IESB

EE

SA

WXN

RES1

RES0

## 13.98 SCTLR_EL3, System Control Register, EL3

This register resets to 0x30C50838.

Reserved, [63:45]

Reserved.

DSSBS, [44]

DSSBS is used to set the new PSTATE bit, SSBS (Speculative Store Bypassing Safe).

SCTLR_EL2.DSSBS is held in bit[44] regardless of the value of HCR_EL2.E2H or
HCR_EL2.TGE.

0
PSTATE.SSBS is set to 0 on an exception taken to this Exception
level. This is the reset value.
1
PSTATE.SSBS is set to 1 on an exception taken to this Exception
level.

Conﬁgurations

If EL2 is not implemented, this register is RES0 from EL3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The SCTLR_EL3 provides top-level control of the system, including its memory system at EL3.

Bit ﬁeld descriptions
SCTLR_EL3 is a 64-bit register, and is part of the Other system control registers functional group.

This register resets to 0x30C50838.

Figure 13-81: SCTLR_EL3 bit assignments

63
43
45
0

44

30 29 28 27
24 23 22 21
17 16 15 14 13
10
6
5

25
26
20 19 18
12 11
2
1
4
3

I
C A M

EE

IESB

WXN

SA

DSSBS

RES1

RES0

RES0, [63:45]

RES0
Reserved

DSSBS, [44]

DSSBS is used to set the new PSTATE bit, SSBS (Speculative Store Bypassing Safe).

0
PSTATE.SSBS is set to 0 on an exception taken to this Exception
level. This is the reset value.
1
PSTATE.SSBS is set to 1 on an exception taken to this Exception
level.

RES0, [43:30]

RES0
Reserved

RES1, [29:28]

RES1
Reserved

RES0, [27:26]

RES0
Reserved

EE, [25]

Exception endianness. This bit controls the endianness for:

- Explicit data accesses at EL3.

- Stage 1 translation table walks at EL3.

The possible values are:

0
Little-endian
1
Big-endian

The reset value is determined by the CFGEND conﬁguration signal.

I, [12]

Global instruction cache enable. The possible values are:

0
Instruction caches disabled. This is the reset value.
1
Instruction caches enabled.

C, [2]

Global enable for data and uniﬁed caches. The possible values are:

0
Disables data and uniﬁed caches. This is the reset value.
1
Enables data and uniﬁed caches.

## 13.99 TCR_EL1, Translation Control Register, EL1

M, [0]

Global enable for the EL3 MMU. The possible values are:

0
Disables EL3 MMU. This is the reset value.
1
Enables EL3 MMU.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The TCR_EL1 determines which Translation base registers deﬁne the base address register for a
translation table walk required for stage 1 translation of a memory access from EL0 or EL1 and
holds Cacheability and Shareability information.

Bit ﬁeld descriptions
TCR_EL1 is a 64-bit register, and is part of the Virtual memory control registers functional group.

Figure 13-82: TCR_EL1 bit assignments

63
51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35

34
32

IPS

HWU162

AS

HWU161

TBI0

HWU160

TBI1

HA

HD

HPD0

HPD1

HWU059

HWU060

HWU061

HWU062

HWU159

31 30

29 28 27 26 25 24 23

22

21
16

15 14

13 12 11 10 9
8
7
6

5
0

TG1

SH1

A1

T1SZ

TG0

SH0

T0SZ

ORGN1

EPD

IRGN1

IRGN0

EPD
ORGN0

RES0

## 13.100 TCR_EL2, Translation Control Register, EL2

Bits[50:39], architecturally deﬁned, are implemented in the core.

HD, [40]

Hardware management of dirty state in stage 1 translations from EL0 and EL1. The possible
values are:

0
Stage 1 hardware management of dirty state disabled.
1
Stage 1 hardware management of dirty state enabled, only if the HA
bit is also set to 1.

HA, [39]

Hardware Access ﬂag update in stage 1 translations from EL0 and EL1. The possible values
are:

0
Stage 1 Access ﬂag update disabled.
1
Stage 1 Access ﬂag update enabled.

Conﬁgurations

RW ﬁelds in this register reset to UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The TCR_EL2 controls translation table walks required for stage 1 translation of a memory access
from EL2 and holds Cacheability and Shareability information.

Bit ﬁeld descriptions
TCR_EL2 is a 64-bit register.

TCR_EL2 is part of:

- The Virtual memory control registers functional group.

- The Hypervisor and virtualization registers functional group.

## 13.101 TCR_EL3, Translation Control Register, EL3

Figure 13-83: TCR_EL2 bit assignments

32
63

31 30
24 23 22 21 20 19 18
16 15 14 13 12 11 10 9
8
7
6
5
0

25
28
29

HWU62-59

SH0
TG0
PS

T0SZ

IRGN0
ORGN0

TBI
HA
HD
HPD

RES0

RES1

Bits[28:21], architecturally deﬁned, are implemented in the core.

HD, [22]

Dirty bit update. The possible values are:

0
Dirty bit update is disabled.
1
Dirty bit update is enabled.

HA, [21]

Stage 1 Access ﬂag update. The possible values are:

0
Stage 1 Access ﬂag update is disabled.
1
Stage 1 Access ﬂag update is enabled.

Conﬁgurations

When the Virtualization Host Extension is activated, TCR_EL2 has the same bit assignments
as TCR_EL1.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The TCR_EL3 controls translation table walks required for stage 1 translation of memory accesses
from EL3 and holds Cacheability and Shareability information for the accesses.

Bit ﬁeld descriptions
TCR_EL3 is a 64-bit register and is part of the Virtual memory control registers functional group.

Figure 13-84: TCR_EL3 bit assignments

0
63
32

31 30
24 23 22 21 20 19 18
16 15 14 13 12 11 10 9
8 7
6
5

29 28
25

27 26

SH0
TG0
PS0

T0SZ

IRGN0
ORGN0

TBI

HA
HD
HPD

HWU59
HWU60
HWU61
HWU62

RES0

RES1

Bits[28:21], architecturally deﬁned, are implemented in the core.

Reserved, [63:32]

Reserved.

HD, [22]

Dirty bit update. The possible values are:

0
Dirty bit update is disabled.
1
Dirty bit update is enabled.

HA, [21]

Stage 1 Access ﬂag update. The possible values are:

0
Stage 1 Access ﬂag update is disabled.
1
Stage 1 Access ﬂag update is enabled.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.102 TTBR0_EL1, Translation Table Base Register 0, EL1

The TTBR0_EL1 holds the base address of translation table 0, and information about the memory it
occupies. This is one of the translation tables for the stage 1 translation of memory accesses from
modes other than Hyp mode.

Bit ﬁeld descriptions
TTBR0_EL1 is 64-bit register.

Figure 13-85: TTBR0_EL1 bit assignments

47
48
0
63
1

BADDR[47:x]
ASID

CnP

ASID, [63:48]

An ASID for the translation table base address. The TCR_EL1.A1 ﬁeld selects either
TTBR0_EL1.ASID or TTBR1_EL1.ASID.

BADDR[47:x], [47:1]

Translation table base address, bits[47:x]. Bits [x-1:1] are RES0.

x is based on the value of TCR_EL1.T0SZ, the stage of translation, and the memory
translation granule size.

For instructions on how to calculate it, see the Arm® Architecture Reference Manual for A-
proﬁle architecture.

The value of x determines the required alignment of the translation table, that must be
aligned to 2x bytes.

If bits [x-1:1] are not all zero, this is a misaligned translation table base address. Its eﬀects
are CONSTRAINED UNPREDICTABLE, where bits [x-1:1] are treated as if all the bits are zero. The
value read back from those bits is the value that is written.

CnP, [0]

Common not Private. The possible values are:

0
CnP is not supported.

1
CnP is supported.

Conﬁgurations

There are no conﬁguration notes.

## 13.103 TTBR0_EL2, Translation Table Base Register 0, EL2

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The TTBR0_EL2 holds the base address of the translation table for the stage 1 translation of
memory accesses from EL2.

Bit ﬁeld descriptions

TTBR0_EL2 is a 64-bit register, and is part of the Virtual memory control registers functional group.

Figure 13-86: TTBR0_EL2 bit assignments

47
48
0
63
1

BADDR[47:x]

CnP

RES0

RES0, [63:48]

RES0

Reserved.

BADDR, [47:1]

Translation table base address, bits[47:x]. Bits [x-1:1] are RES0.

x is based on the value of TCR_EL2.T0SZ, the stage of translation, and the memory
translation granule size.

For instructions on how to calculate it, see the Arm® Architecture Reference Manual Arm®v8,
for Arm®v8-A architecture proﬁle.

The value of x determines the required alignment of the translation table, that must be
aligned to 2x bytes.

If bits [x-1:1] are not all zero, this is a misaligned translation table base address. Its eﬀects
are CONSTRAINED UNPREDICTABLE, where bits [x-1:1] are treated as if all the bits are zero. The
value read back from those bits is the value that is written.

CnP, [0]

Common not Private. The possible values are:

0
CnP is not supported.

## 13.104 TTBR0_EL3, Translation Table Base Register 0, EL3

1
CnP is supported.

Conﬁgurations

When the Virtualization Host Extension is activated, TTBR0_EL2 has the same bit
assignments as TTBR0_EL1.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The TTBR0_EL3 holds the base address of the translation table for the stage 1 translation of
memory accesses from EL3.

Bit ﬁeld descriptions
TTBR0_EL3 is a 64-bit register.

Figure 13-87: TTBR0_EL3 bit assignments

47
48
0
63
1

BADDR[47:x]

CnP

RES0

[63:48]

Reserved, RES0.

BADDR[47:x], [47:1]

Translation table base address, bits[47:x]. Bits [x-1:1] are RES0.

x is based on the value of TCR_EL1.T0SZ, the stage of translation, and the memory
translation granule size.

For instructions on how to calculate it, see the Arm® Architecture Reference Manual for A-
proﬁle architecture.

The value of x determines the required alignment of the translation table, that must be
aligned to 2x bytes.

If bits [x-1:1] are not all zero, this is a misaligned translation table base address. Its eﬀects
are CONSTRAINED UNPREDICTABLE, where bits [x-1:1] are treated as if all the bits are zero. The
value read back from those bits is the value that is written.

## 13.105 TTBR1_EL1, Translation Table Base Register 1, EL1

CnP, [0]

Common not Private. The possible values are:

0
CnP is not supported.
1
CnP is supported.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

The TTBR1_EL1 holds the base address of translation table 1, and information about the memory it
occupies. This is one of the translation tables for the stage 1 translation of memory accesses at EL0
and EL1.

Bit ﬁeld descriptions
TTBR1_EL1 is a 64-bit register.

Figure 13-88: TTBR1_EL1 bit assignments

47
48
0
63
1

BADDR[47:x]
ASID

CnP

ASID, [63:48]

An ASID for the translation table base address. The TCR_EL1.A1 ﬁeld selects either
TTBR0_EL1.ASID or TTBR1_EL1.ASID.

BADDR[47:x], [47:1]

Translation table base address, bits[47:x]. Bits [x-1:0] are RES0.

x is based on the value of TCR_EL1.T0SZ, the stage of translation, and the memory
translation granule size.

For instructions on how to calculate it, see the Arm® Architecture Reference Manual for A-
proﬁle architecture.

The value of x determines the required alignment of the translation table, that must be
aligned to 2x bytes.

## 13.106 TTBR1_EL2, Translation Table Base Register 1, EL2

## 13.107 VDISR_EL2, Virtual Deferred Interrupt Status Register, EL2

### 13.107.1 VDISR_EL2 at EL1 using AArch64

If bits [x-1:1] are not all zero, this is a misaligned Translation Table Base Address. Its eﬀects
are CONSTRAINED UNPREDICTABLE, where bits [x-1:1] are treated as if all the bits are zero. The
value read back from those bits is the value that is written.

CnP, [0]

Common not Private. The possible values are:

0
CnP is not supported.
1
CnP is supported.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.106 TTBR1_EL2, Translation Table Base Register 1, EL2

TTBR1_EL2 has the same format and contents as TTBR1_EL1.

See 13.105 TTBR1_EL1, Translation Table Base Register 1, EL1 on page 248.

13.107 VDISR_EL2, Virtual Deferred Interrupt Status
Register, EL2

The VDISR_EL2 records that a virtual SError interrupt has been consumed by an ESB instruction
executed at Non-secure EL1.

Bit ﬁeld descriptions
VDISR_EL2 is a 64-bit register, and is part of the Reliability, Availability, Serviceability (RAS) registers
functional group.

Conﬁgurations

See 13.107.1 VDISR_EL2 at EL1 using AArch64 on page 249.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

VDISR_EL2 has a speciﬁc format when written at EL1.

The following ﬁgure shows the VDISR_EL2 bit assignments when written at EL1 using AArch64:

## 13.108 VSESR_EL2, Virtual SError Exception Syndrome Register

Figure 13-89: VDISR_EL2 at EL1 using AArch64

63

30
24
25
23
32

31
0

A
ISS

IDS

RES0

RES0, [63:32]

RES0
Reserved.

A, [31]

Set to 1 when ESB defers an asynchronous SError interrupt.

RES0, [30:25]

RES0
Reserved.

IDS, [24]

Contains the value from VSESR_EL2.IDS.

ISS, [23:0]

Contains the value from VSESR_EL2, bits[23:0].

13.108 VSESR_EL2, Virtual SError Exception Syndrome
Register

The VSESR_EL2 provides the syndrome value that is reported to software on taking a virtual SError
interrupt exception.

Bit ﬁeld descriptions
VSESR_EL2 is a 64-bit register, and is part of:

- The Exception and fault handling registers functional group.

- The Virtualization registers functional group.

If the virtual SError interrupt is taken to EL1, VSESR_EL2 provides the syndrome value that is
reported in ESR_EL1.

## 13.109 VTCR_EL2, Virtualization Translation Control Register, EL2

VSESR_EL2 bit assignments

Figure 13-90: VSESR_EL2 bit assignments

63
25 24

23
0

ISS

IDS

RES0

RES0, [63:25]

RES0
Reserved.

IDS, [24]

Indicates whether the deferred SError interrupt was of an IMPLEMENTATION DEFINED type. See
ESR_EL1.IDS for a description of the functionality.

On taking a virtual SError interrupt to EL1 using AArch64 because HCR_EL2.VSE == 1,
ESR_EL1[24] is set to VSESR_EL2.IDS.

ISS, [23:0]

Syndrome information. See ESR_EL1.ISS for a description of the functionality.

On taking a virtual SError interrupt to EL1 using AArch32 due to HCR_EL2.VSE == 1,
ESR_EL1 [23:0] is set to VSESR_EL2.ISS.

Conﬁgurations

There are no conﬁguration notes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

13.109 VTCR_EL2, Virtualization Translation Control
Register, EL2

The VTCR_EL2 controls the translation table walks required for the stage 2 translation of memory
accesses from Non-secure EL0 and EL1.

It also holds Cacheability and Shareability information for the accesses.

Bit ﬁeld descriptions
VTCR_EL2 is a 64-bit register, and is part of:

- The Virtualization registers functional group.

- The Virtual memory control registers functional group.

Figure 13-91: VTCR_EL2 bit assignments

63
32
29 28 27 26 25 24 23 22 21 20

31
0
5
6
7
8
9
10
11
12
13
14
15
16

30

18
19

PS
SH0
SL0
T0SZ

TG0

Reserved

HWU62

HWU61

HD

ORGN0

HWU60

HA

IRGN0

RES1

HWU59

VS

RES0

Bits[28:25] and bits[22:21], architecturally deﬁned, are implemented in the core.

Reserved, [63:32]

Reserved.

TG0, [15:14]

TTBR0_EL2 granule size. The possible values are:

00
4KB.
01
64KB.
10
16KB.
11
Reserved.

All other values are not supported.

Conﬁgurations

RW ﬁelds in this register reset to architecturally UNKNOWN values.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

## 13.110 VTTBR_EL2, Virtualization Translation Table Base Register, EL2

13.110 VTTBR_EL2, Virtualization Translation Table Base
Register, EL2

VTTBR_EL2 holds the base address of the translation table for the stage 2 translation of memory
accesses from Non-secure EL0 and EL1.

Bit ﬁeld descriptions
VTTBR_EL2 is a 64-bit register.

Figure 13-92: VTTBR_EL2 bit assignments

4
47
48

1
3

63
0

BADDR

VMID

CnP

RES0

CnP, [0]

Common not Private. The possible values are:

0
CnP is not supported.
1
CnP is supported.

Conﬁgurations

There are no conﬁguration notes.
Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See
the Arm® Architecture Reference Manual for A-proﬁle architecture.

# 14. Error System registers

## 14.1 Error System register summary

This chapter describes the error registers accessed by the AArch64 error registers.

This section identiﬁes the ERR0* core error record registers accessed by the AArch64 ERX* error
registers.

For those registers that are not described in this chapter, see the Arm® Architecture Reference
Manual for A-proﬁle architecture.

The following table describes the architectural error record registers.

Table 14-1: Architectural error System register summary

The following table describes the error record registers that are IMPLEMENTATION DEFINED.

Table 14-2: IMPLEMENTATION DEFINED error System register summary

|Register<br>mnemonic|Size|Register name|Access aliases from AArch64|
|---|---|---|---|
|ERR0ADDR|64|14.2 ERR0ADDR, Error Record Address Register on<br>page 254|13.44 ERXADDR_EL1, Selected Error Record Address Register,<br>EL1 on page 167|
|ERR0CTLR|64|14.3 ERR0CTLR, Error Record Control Register on<br>page 255|13.45 ERXCTLR_EL1, Selected Error Record Control Register,<br>EL1 on page 167|
|ERR0FR|64|14.4 ERR0FR, Error Record Feature Register on<br>page 257|13.46 ERXFR_EL1, Selected Error Record Feature Register, EL1<br>on page 167|
|ERR0MISC0|64|14.5 ERR0MISC0, Error Record Miscellaneous<br>Register 0 on page 259|13.47 ERXMISC0_EL1, Selected Error Record Miscellaneous<br>Register 0, EL1 on page 167|
|ERR0MISC1|64|14.6 ERR0MISC1, Error Record Miscellaneous<br>Register 1 on page 262|13.48 ERXMISC1_EL1, Selected Error Record Miscellaneous<br>Register 1, EL1 on page 168|
|ERR0STATUS|32|14.10 ERR0STATUS, Error Record Primary Status<br>Register on page 266|13.52 ERXSTATUS_EL1, Selected Error Record Primary Status<br>Register, EL1 on page 172|

|Register<br>mnemonic|Size|Register name|Access aliases from AArch64|
|---|---|---|---|
|ERR0PFGCDN|32|14.7 ERR0PFGCDN, Error Pseudo Fault Generation<br>Count Down Register on page 262|13.49 ERXPFGCDN_EL1, Selected Error Pseudo Fault<br>Generation Count Down Register, EL1 on page 168|
|ERR0PFGCTL|32|14.8 ERR0PFGCTL, Error Pseudo Fault Generation<br>Control Register on page 263|13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault<br>Generation Control Register, EL1 on page 169|
|ERR0PFGF|32|14.9 ERR0PFGF, Error Pseudo Fault Generation<br>Feature Register on page 265|13.51 ERXPFGF_EL1, Selected Pseudo Fault Generation<br>Feature Register, EL1 on page 171|

## 14.2 ERR0ADDR, Error Record Address Register

## 14.3 ERR0CTLR, Error Record Control Register

14.2 ERR0ADDR, Error Record Address Register

The ERR0ADDR stores the address that is associated to an error that is recorded.

Bit ﬁeld descriptions
ERR0ADDR is a 64-bit register, and is part of the Reliability, Availability, Serviceability (RAS) registers
functional group.

Figure 14-1: ERR0ADDR bit assignments

NS, [63]

Non-secure attribute. The possible values are:

0
The physical address is Secure.
1
The physical address is Non-secure.

RES0, [62:48]

RES0
Reserved.

PADDR, [47:0]

Physical address.

Conﬁgurations

ERR0ADDR resets to UNKNOWN.

When ERRSELR.SEL==0, this register is accessible from 13.44 ERXADDR_EL1, Selected
Error Record Address Register, EL1 on page 167.

The ERR0CTLR contains enable bits for the node that writes to this record:

- Enabling error detection and correction.

- Enabling an error recovery interrupt.

- Enabling a fault handling interrupt.

- Enabling error recovery reporting as a read or write error response.

Bit ﬁeld descriptions
ERR0CTLR is a 64-bit register and is part of the Reliability, Availability, Serviceability (RAS) registers
functional group.

ERR0CTLR resets to ED is 0x0. CFI [8], FI [3], and UI [2] are UNKNOWN. The rest of the register is
RES0.

Figure 14-2: ERR0CTLR bit assignments

63
9
8
7
4
3
2
1
0

CFI

FI

UI

ED

RES0

RES0, [63:9]

RES0
Reserved.

CFI, [8]

Fault handling interrupt for corrected errors enable.

The fault handling interrupt is generated when one of the standard CE counters on
ERR0MISC0 overﬂows and the overﬂow bit is set. The possible values are:

0
Fault handling interrupt not generated for corrected errors.
1
Fault handling interrupt generated for corrected errors.

The interrupt is generated even if the error status is overwritten because the error record
already records a higher priority error.

This applies to both reads and writes.

RES0, [7:4]

RES0
Reserved.

FI, [3]

Fault handling interrupt enable.

The fault handling interrupt is generated for all detected Deferred errors and Uncorrected
errors. The possible values are:

0
Fault handling interrupt disabled.
1
Fault handling interrupt enabled.

## 14.4 ERR0FR, Error Record Feature Register

UI, [2]

Uncorrected error recovery interrupt enable. When enabled, the error recovery interrupt is
generated for all detected Uncorrected errors that are not deferred. The possible values are:

0
Error recovery interrupt disabled.
1
Error recovery interrupt enabled.

Applies to both reads and writes.

RES0, [1]

RES0
Reserved.

ED, [0]

Error Detection and correction enable. The possible values are:

0
Error detection and correction disabled.
1
Error detection and correction enabled.

Conﬁgurations

This register is accessible from the following registers when ERRSELR.SEL==0:
13.45 ERXCTLR_EL1, Selected Error Record Control Register, EL1 on page 167.

The ERR0FR deﬁnes which of the common architecturally deﬁned features are implemented and,
of the implemented features, which are software programmable.

Bit ﬁeld descriptions
ERR0FR is a 64-bit register, and is part of the Reliability, Availability, Serviceability (RAS) registers
functional group.

The register is read-only.

Figure 14-3: ERR0FR bit assignments

RES0, [63:18]

RES0

Reserved.

DUI, [17:16]

Error recovery interrupt for deferred errors. The value is:

00
The core does not support this feature.

RP, [15]

Repeat counter. The value is:

1
A ﬁrst repeat counter and a second other counter are implemented.
The repeat counter is the same size as the primary error counter.

CEC, [14:12]

Corrected Error Counter. The value is:

010
The node implements an 8-bit standard CE counter in
ERR0MISC0[39:32].

CFI, [11:10]

Fault handling interrupt for corrected errors. The value is:

10
The node implements a control for enabling fault handling interrupts
on corrected errors.

UE, [9:8]

In-band uncorrected error reporting. The value is:

01
The node implements in-band uncorrected error reporting, that is
External aborts.

FI, [7:6]

Fault handling interrupt. The value is:

10
The node implements a fault handling interrupt and implements
controls for enabling and disabling.

UI, [5:4]

Error recovery interrupt for uncorrected errors. The value is:

10
The node implements an error recovery interrupt and implements
controls for enabling and disabling.

RES0, [3:2]

RES0

Reserved.

ED, [1:0]

Error detection and correction. The value is:

## 14.5 ERR0MISC0, Error Record Miscellaneous Register 0

10
The node implements controls for enabling or disabling error
detection and correction.

Conﬁgurations

ERR0FR resets to 0x000000000000A9A2
ERR0FR is accessible from the following registers when ERRSELR.SEL==0:
13.46 ERXFR_EL1, Selected Error Record Feature Register, EL1 on page 167.

The ERR0MISC0 is an error syndrome register. It contains corrected error counters, information
to identify where the error was detected, and other state information not present in the
corresponding status and address error record registers.

Bit ﬁeld descriptions
ERR0MISC0 is a 64-bit register, and is part of the Reliability, Availability, Serviceability (RAS) registers
functional group.

Figure 14-4: ERR0MISC0 bit assignments

63
0
31

3
4
5
6
19 18
27
32
47
48
28

39
40
46

38

26

25

23
24

22

CECO

CECR

WAY
UNIT
INDEX

OFR
OFO

SUBARRAY

ARRAY

BANK

SUBBANK

RES0

RES0, [63:48]

Reserved, RES0.

OFO, [47]

Sticky overﬂow bit, other. The possible values of this bit are:

0
Other counter has not overﬂowed.
1
Other counter has overﬂowed.

The fault handling interrupt is generated when the corrected fault handling interrupt is
enabled and either overﬂow bit is set to 1.

CECO, [46:40]

Corrected error count, other. Incremented for each Corrected error that does not match the
recorded syndrome.

This ﬁeld resets to an IMPLEMENTATION DEFINED which might be UNKNOWN on a Cold reset.
If the reset value is UNKNOWN, then the value of this ﬁeld remains UNKNOWN until software
initializes it.

OFR, [39]

Sticky overﬂow bit, repeat. The possible values of this bit are:

0
Repeat counter has not overﬂowed.
1
Repeat counter has overﬂowed.

The fault handling interrupt is generated when the corrected fault handling interrupt is
enabled and either overﬂow bit is set to 1.

CECR, [38:32]

Corrected error count, repeat. Incremented for the ﬁrst recorded error, which also records
other syndromes, and then again for each Corrected error that matches the recorded
syndrome.

This ﬁeld resets to an IMPLEMENTATION DEFINED which might be UNKNOWN on a Cold reset.
If the reset value is UNKNOWN, then the value of this ﬁeld remains UNKNOWN until software
initializes it.

WAY, [31:28]

The encoding depends on the unit from which the error being recorded was detected. The
possible values are:

L1
Data
Cache

Indicates which Tag RAM way or data RAM way detected the error.
Upper 2 bits are unused.

L2
TLB

Indicates which RAM has an error. The possible values are 0 (RAM 1)
to 9 (RAM 10).
L1
Instruction
Cache

Indicates which way has the error. Upper 2 bits are unused.

RES0, [27:26]

Reserved, RES0.

SUBBANK, [25]

The encoding depends on the unit from which the error being recorded was detected. The
possible values are:

L1
Instruction
Cache

Indicates which subbank has the error, valid for instruction data
cache. For Tag errors, this ﬁeld is zero.

BANK, [24:23]

The encoding depends on the unit from which the error being recorded was detected. The
possible values are:

L2
cache

Indicates which L2 bank detected the error. Upper 1 bit is unused.

L1
Instruction
Cache

Indicates which bank has the error, valid for instruction data cache.
For Tag errors, this ﬁeld is zero.

SUBARRAY, [22:19]

The encoding depends on the unit from which the error being recorded was detected. The
possible values are:

L2
Cache

Indicates which L2 Tag way or data doubleword detected the error.
Upper 1 bit is unused.
L1
Data
Cache

Indicates for L1 Data RAM which word had the error detected. For
L1 Tag RAMs which bank had the error (0b0000: bank0, 0b0001:
bank1)

INDEX, [18:6]

The encoding depends on the unit from which the error being recorded was detected. The
possible values are:

L2
Cache

Indicates which index detected the error. Upper bits of the index are
unused depending on the cache size.
L1
Data
Cache

Indicates which index detected the error. Upper bits of the index are
unused depending on the cache size.

L2
TLB

Index of TLB RAM. Upper 4 bits are unused.

L1
Instruction
Cache

Indicates which index has the error. Upper bits of the index are
unused depending on the cache size.

ARRAY, [5:4]

The encoding depends on the unit from which the error being recorded was detected. The
possible values are:

L2
Cache

Indicates which array has the error. The possible values are:

0b00
L2 Tag RAM.
0b01
L2 Data RAM.
0b10
TQ Data RAM.
0b11
CHI Completer Error.
L1
Data
Cache

Indicates which array detected the error. The possible values are:

0b00
LS0 copy of Tag RAM.
0b01
LS1 copy of Tag RAM.
0b10
LS Data RAM.

## 14.6 ERR0MISC1, Error Record Miscellaneous Register 1

## 14.7 ERR0PFGCDN, Error Pseudo Fault Generation Count Down Register

L1
Instruction
Cache

Indicates which array that detected the error, Data Array has higher
priority. The possible values are:

0b0
Tag.
0b1
Data.

UNIT, [3:0]

Indicates the unit which detected the error. The possible values are:

0b1000
L2 Cache.
0b0100
L1 Data Cache.
0b0010
L2 TLB.
0b0001
L1 Instruction Cache.

Conﬁgurations

ERR0MISC0 resets to [63:32] is 0x00000000, [31:0] is UNKNOWN.
This register is accessible from the following registers when ERRSELR_EL1.SEL==0:

- 13.47 ERXMISC0_EL1, Selected Error Record Miscellaneous Register 0, EL1 on page
167.

14.6 ERR0MISC1, Error Record Miscellaneous Register 1

This register is unused in the Neoverse™ N1 core and marked as RES0.

Conﬁgurations
When ERRSELR.SEL==0, ERR0MISC1 is accessible from 13.48 ERXMISC1_EL1, Selected Error
Record Miscellaneous Register 1, EL1 on page 168.

14.7 ERR0PFGCDN, Error Pseudo Fault Generation Count
Down Register

ERR0PFGCDN is the Neoverse™ N1 node register that generates one of the errors that are
enabled in the corresponding ERR0PFGCTL register.

Bit ﬁeld descriptions
ERR0PFGCDN is a 32-bit register and is RW.

## 14.8 ERR0PFGCTL, Error Pseudo Fault Generation Control Register

Figure 14-5: ERR0PFGCDN bit assignments

31
0

CDN

CDN, [31:0]

Count Down value. The reset value of the Error Generation Counter is used for the
countdown.

Conﬁgurations

There are no conﬁguration options.

ERR0PFGCDN resets to UNKNOWN.

When ERRSELR.SEL==0, ERR0PFGCDN is accessible from 13.49 ERXPFGCDN_EL1,
Selected Error Pseudo Fault Generation Count Down Register, EL1 on page 168.

14.8 ERR0PFGCTL, Error Pseudo Fault Generation Control
Register

The ERR0PFGCTL is the Neoverse™ N1 node register that enables controlled fault generation.

Bit ﬁeld descriptions
ERR0PFGCTL is a 32-bit read/write register.

Figure 14-6: ERR0PFGCTL bit assignments

31 30
6
5
4
2
0

29

7

1

DE
R
CE

CDNEN

UC

RES0

CDNEN, [31]

Count down enable. This bit controls transfers from the value that is held in the
ERR0PFGCDN into the Error Generation Counter and enables this counter to start counting
down. The possible values are:

0
The Error Generation Counter is disabled.
1
The value that is held in the ERR0PFGCDN register is transferred
into the Error Generation Counter. The Error Generation Counter
counts down.

R, [30]

Restartable bit. When it reaches 0, the Error Generation Counter restarts from the
ERR0PFGCDN value or stops. The possible values are:

0
When it reaches 0, the counter stops.
1
When it reaches 0, the counter reloads the value that is stored in
ERR0PFGCDN and starts counting down again.

RES0, [29:7]

Reserved, RES0.

CE, [6]

Corrected error generation enable. The possible values are:

0
No corrected error is generated.
1
A corrected error might be generated when the Error Generation
Counter is triggered.

DE, [5]

Deferred Error generation enable. The possible values are:

0
No deferred error is generated.
1
A deferred error might be generated when the Error Generation
Counter is triggered.

This bit is RES0 if the node does not support this control.

This bit resets to an architecturally UNKNOWN value on a Cold reset. This bit is preserved on
an Error Recovery reset.

RES0, [4:2]

Reserved, RES0.

UC, [1]

Uncontainable error generation enable. The possible values are:

0
No uncontainable error is generated.
1
An uncontainable error might be generated when the Error
Generation Counter is triggered.

## 14.9 ERR0PFGF, Error Pseudo Fault Generation Feature Register

RES0, [0]

Reserved, RES0.

Conﬁgurations

There are no conﬁguration notes.

ERR0PFGCTL resets to 0x00000000.

ERR0PFGCTL is accessible from the following registers when ERRSELR.SEL==0:

- 13.50 ERXPFGCTL_EL1, Selected Error Pseudo Fault Generation Control Register, EL1
on page 169.

14.9 ERR0PFGF, Error Pseudo Fault Generation Feature
Register

The ERR0PFGF is the Neoverse™ N1 node register that deﬁnes which fault generation features are
implemented.

Bit ﬁeld descriptions
ERR0PFGF is a 32-bit register and is RO.

Figure 14-7: ERR0PFGF bit assignments

31
0

1
30 29
2
3
4
5
6
7

R
PFG

CE
DE
UEO
UER
UEU

UC

RES0

PFG, [31]

Pseudo Fault Generation. The value is:

1
The node implements a fault injection mechanism.

R, [30]

Restartable bit. When it reaches zero, the Error Generation Counter restarts from the
ERR0PFGCDN value or stops. The value is:

1
This feature is controllable.

RES0, [29:7]

RES0
Reserved.

CE, [6]

Corrected Error generation. The value is:

1
This feature is controllable.

DE, [5]

Deferred Error generation. The value is:

1
This feature is controllable.

UEO, [4]

Latent or Restartable Error generation. The value is:

0
The node does not support this feature.

UER, [3]

Signaled or Recoverable Error generation. The value is:

0
The node does not support this feature.

UEU, [2]

Unrecoverable Error generation. The value is:

0
The node does not support this feature.

UC, [1]

Uncontainable Error generation. The value is:

1
This feature is controllable.

[0]

RES0
Reserved.

Conﬁgurations

There are no conﬁguration notes.

ERR0PFGF resets to 0xC0000062.

When ERRSELR.SEL==0, ERR0PFGF is accessible from 13.51 ERXPFGF_EL1, Selected
Pseudo Fault Generation Feature Register, EL1 on page 171.

## 14.10 ERR0STATUS, Error Record Primary Status Register

The ERR0STATUS contains information about the error record.

The register indicates:

- Whether any error has been detected.

- Whether any detected error was not corrected and returned to a requester.

- Whether any detected error was not corrected and deferred.

- Whether a second error of the same type was detected before software handled the ﬁrst error.

- Whether any error has been reported.

- Whether the other error record registers contain valid information.

Bit ﬁeld descriptions
ERR0STATUS is a 32-bit register.

Figure 14-8: ERR0STATUS bit assignments

31 30
4
29

28 27 26 25
24 23 22 21
20 19
5

0

CE
SERR

AV

UET

V

PN

UE

DE

ER

OF

MV

res0

AV, [31]

Address Valid. The possible values are:

0
ERR0ADDR is not valid.
1
ERR0ADDR contains an address that is associated with the highest
priority error recorded by this record.

V, [30]

Status Register valid. The possible values are:

0
ERR0STATUS is not valid.
1
ERR0STATUS is valid. At least one error has been recorded.

UE, [29]

Uncorrected error. The possible values are:

0
No error that could not be corrected or deferred has been detected.

1
At least one error that could not be corrected or deferred has been
detected. If error recovery interrupts are enabled, then the interrupt
signal is asserted until this bit is cleared.

ER, [28]

Error reported. The possible values are:

0
No External abort has been reported.
1
The node has reported an External abort to the requester that is in
access or making a transaction.

OF, [27]

Overﬂow. The possible values are:

0
- If UE == 1, then no error status for an Uncorrected error has
been discarded.

- If UE == 0 and DE == 1, then no error status for a Deferred error
has been discarded.

- If UE == 0, DE == 0, and CE !== 0b00, then:
The corrected error counter has not overﬂowed.
1
More than one error has occurred and so details of the other error
have been discarded.

MV, [26]

Miscellaneous Registers Valid. The possible values are:

0
ERR0MISC0 and ERR0MISC1 are not valid.
1
This bit indicates that ERR0MISC0 contains additional information
about any error that is recorded by this record.

CE, [25:24]

Corrected error. The possible values are:

0b00
No corrected error recorded.
0b10
At least one corrected error recorded.

DE, [23]

Deferred error. The possible values are:

0
No errors were deferred.
1
At least one error was not corrected and deferred by poisoning.

PN, [22]

Poison. The value is:

0
The Neoverse™ N1 core cannot distinguish a poisoned value from a
corrupted value.

UET, [21:20]

Uncorrected Error Type. The value is:

0b00
Uncontainable.

RES0, [19:5]

RES0.

Reserved.

SERR, [4:0]

Primary error code. The possible values are:

0x0
No error.
0x1
Errors due to fault injection.
0x2
ECC error from internal data buﬀer.
0x6
ECC error on cache data RAM.
0x7
ECC error on cache tag or dirty RAM.
0x8
Parity error on TLB data RAM.
0x12
Error response for a cache copyback.
0x15
Deferred error from a completer not supported at the consumer. For
example, poisoned data received from a completer by a requester
that cannot defer the error further.

Conﬁgurations

There are no conﬁguration notes.

ERR0STATUS resets to 0x00000000.

ERR0STATUS is accessible from the following registers when ERRSELR_EL1.SEL==0:

- 13.52 ERXSTATUS_EL1, Selected Error Record Primary Status Register, EL1 on page 172.

# 15. GIC registers

## 15.1 CPU interface registers

## 15.2 AArch64 physical GIC CPU interface System register summary

This chapter describes the GIC registers.

15.1 CPU interface registers

Each CPU interface block provides the interface for the Neoverse™ N1 core that interfaces with a
GIC distributor within the system.

The Neoverse™ N1 core only supports System register access to the GIC CPU interface registers.
The following table lists the three types of GIC CPU interface System registers supported in the
Neoverse™ N1 core.

Table 15-1: GIC CPU interface System register types supported in the Neoverse™ N1 core.

Access to virtual GIC CPU interface System registers is only possible at Non-secure EL1.

Access to ICC registers or the equivalent ICV registers is determined by HCR_EL2. See 13.57
HCR_EL2, Hypervisor Conﬁguration Register, EL2 on page 177.

For more information on the CPU interface, see the  Arm® Generic Interrupt Controller Architecture
Speciﬁcation, GIC architecture version 3 and version 4.

15.2 AArch64 physical GIC CPU interface System register
summary

The following table lists the AArch64 physical GIC CPU interface System registers that have
IMPLEMENTATION DEFINED bits.

See the  Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and
version 4 for more information and a complete list of AArch64 physical GIC CPU interface System
registers.

Table 15-2: AArch64 physical GIC CPU interface System register summary

|Register prefix|Register type|
|---|---|
|ICC|Physical GIC CPU interface System registers.|
|ICV|Virtual GIC CPU interface System registers.|
|ICH|Virtual interface control System registers.|

|Name|Op0|Op1|CRn|CRm|Op2|Type|Description|
|---|---|---|---|---|---|---|---|
|ICC_AP0R0_EL1|3|0|12|8|4|RW|15.3 ICC_AP0R0_EL1, Interrupt Controller Active Priorities Group 0 Register 0, EL1<br>on page 271|

## 15.3 ICC_AP0R0_EL1, Interrupt Controller Active Priorities Group 0 Register 0, EL1

15.3 ICC_AP0R0_EL1, Interrupt Controller Active
Priorities Group 0 Register 0, EL1

The ICC_AP0R0_EL1 provides information about Group 0 active priorities.

Bit descriptions
This register is a 32-bit register and is part of:

- The GIC System registers functional group.

- The GIC control registers functional group.

The core implements 5 bits of priority with 32 priority levels, corresponding to the 32 bits [31:0] of
the register. The possible values for each bit are:

0x00000000
No interrupt active. This is the reset value.
0x00000001
Interrupt active for priority 0x0.
0x00000002
Interrupt active for priority 0x8.

...
0x80000000
Interrupt active for priority 0xF8.

Details that are not provided in this description are architecturally deﬁned. See the  Arm® Generic
Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

|Name|Op0|Op1|CRn|CRm|Op2|Type|Description|
|---|---|---|---|---|---|---|---|
|ICC_AP1R0_EL1|3|0|12|9|0|RW|15.4 ICC_AP1R0_EL1, Interrupt Controller Active Priorities Group 1 Register 0 EL1<br>on page 271|
|ICC_BPR0_EL1|3|0|12|8|3|RW|15.5 ICC_BPR0_EL1, Interrupt Controller Binary Point Register 0, EL1 on page<br>272|
|ICC_BPR1_EL1|3|0|12|12|3|RW|15.6 ICC_BPR1_EL1, Interrupt Controller Binary Point Register 1, EL1 on page<br>273|
|ICC_CTLR_EL1|3|0|12|12|4|RW|15.7 ICC_CTLR_EL1, Interrupt Controller Control Register, EL1 on page 274|
|ICC_CTLR_EL3|3|6|12|12|4|RW|15.8 ICC_CTLR_EL3, Interrupt Controller Control Register, EL3 on page 276|
|ICC_SRE_EL1|3|0|12|12|5|RW|15.9 ICC_SRE_EL1, Interrupt Controller System Register Enable Register, EL1 on<br>page 278|
|ICC_SRE_EL2|3|4|12|9|5|RW|15.10 ICC_SRE_EL2, Interrupt Controller System Register Enable register, EL2 on<br>page 280|
|ICC_SRE_EL3|3|6|12|12|5|RW|15.11 ICC_SRE_EL3, Interrupt Controller System Register Enable register, EL3 on<br>page 281|

## 15.4 ICC_AP1R0_EL1, Interrupt Controller Active Priorities Group 1 Register 0 EL1

## 15.5 ICC_BPR0_EL1, Interrupt Controller Binary Point Register 0, EL1

15.4 ICC_AP1R0_EL1, Interrupt Controller Active
Priorities Group 1 Register 0 EL1

The ICC_AP1R0_EL1 provides information about Group 1 active priorities.

Bit descriptions
This register is a 32-bit register and is part of:

- The GIC System registers functional group.

- The GIC control registers functional group.

The core implements 5 bits of priority with 32 priority levels, corresponding to the 32 bits [31:0] of
the register. The possible values for each bit are:

0x00000000
No interrupt active. This is the reset value.
0x00000001
Interrupt active for priority 0x0.
0x00000002
Interrupt active for priority 0x8.

...
0x80000000
Interrupt active for priority 0xF8.

Details that are not provided in this description are architecturally deﬁned. See the  Arm® Generic
Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.5 ICC_BPR0_EL1, Interrupt Controller Binary Point
Register 0, EL1

ICC_BPR0_EL1 deﬁnes the point at which the priority value ﬁelds split into two parts, the group
priority ﬁeld and the subpriority ﬁeld. The group priority ﬁeld determines Group 0 interrupt
preemption.

Bit ﬁeld descriptions
ICC_BPR0_EL1 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The GIC control registers functional group.

## 15.6 ICC_BPR1_EL1, Interrupt Controller Binary Point Register 1, EL1

Figure 15-1: ICC_BPR0_EL1 bit assignments

31
0
2
3

BinaryPoint

RES0

RES0, [31:3]

RES0
Reserved.

BinaryPoint, [2:0]

The value of this ﬁeld controls how the 8-bit interrupt priority ﬁeld is split into a group
priority ﬁeld, that determines interrupt preemption, and a subpriority ﬁeld. The minimum
value that is implemented is:

0x2

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.6 ICC_BPR1_EL1, Interrupt Controller Binary Point
Register 1, EL1

ICC_BPR1_EL1 deﬁnes the point at which the priority value ﬁelds split into two parts, the group
priority ﬁeld and the subpriority ﬁeld. The group priority ﬁeld determines Group 1 interrupt
preemption.

Bit ﬁeld descriptions
ICC_BPR1_EL1 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The GIC control registers functional group.

## 15.7 ICC_CTLR_EL1, Interrupt Controller Control Register, EL1

Figure 15-2: ICC_BPR1_EL1 bit assignments

31
0
2
3

BinaryPoint

RES0

RES0, [31:3]

RES0
Reserved.

BinaryPoint, [2:0]

The value of this ﬁeld controls how the 8-bit interrupt priority ﬁeld is split into a group
priority ﬁeld, that determines interrupt preemption, and a subpriority ﬁeld.

The minimum value that is implemented of ICC_BPR1_EL1 Secure register is 0x2.

The minimum value that is implemented of ICC_BPR1_EL1 Non-secure register is 0x3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.7 ICC_CTLR_EL1, Interrupt Controller Control Register,
EL1

ICC_CTLR_EL1 controls aspects of the behavior of the GIC CPU interface and provides information
about the features implemented.

Bit ﬁeld descriptions
ICC_CTLR_EL1 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The GIC control registers functional group.

Figure 15-3: ICC_CTLR_EL1 bit assignments

31
0
1
2
5
6
7
8
10
11
13
14
15
16

PRIbits
IDbits

CBPR
EOImode
PMHE

SEIS
A3V

RES0

RES0, [31:16]

RES0
Reserved.

A3V, [15]

Aﬃnity 3 Valid. The value is:

1
The CPU interface logic supports nonzero values of Aﬃnity 3 in SGI
generation System registers.

SEIS, [14]

SEI Support. The value is:

0
The CPU interface logic does not support local generation of SEIs.

IDbits, [13:11]

Identiﬁer bits. The value is:

0
The number of physical interrupt identiﬁer bits supported is 16 bits.

This ﬁeld is an alias of ICC_CTLR_EL3.IDbits.

PRIbits, [10:8]

Priority bits. The value is:

0x4
The core supports 32 levels of physical priority with 5 priority bits.

RES0, [7]

RES0
Reserved.

PMHE, [6]

Priority Mask Hint Enable. This bit is read-only and is an alias of ICC_CTLR_EL3.PMHE. The
possible values are:

## 15.8 ICC_CTLR_EL3, Interrupt Controller Control Register, EL3

0
Disables use of ICC_PMR as a hint for interrupt distribution.
1
Enables use of ICC_PMR as a hint for interrupt distribution.

RES0, [5:2]

RES0
Reserved.

EOImode, [1]

End of interrupt mode for the current Security state. The possible values are:

0
ICC_EOIR0 and ICC_EOIR1 provide both priority drop and interrupt
deactivation functionality. Accesses to ICC_DIR are UNPREDICTABLE.
1
ICC_EOIR0 and ICC_EOIR1 provide priority drop functionality only.
ICC_DIR provides interrupt deactivation functionality.

CBPR, [0]

Common Binary Point Register. Control whether the same register is used for interrupt
preemption of both Group 0 and Group 1 interrupt. The possible values are:

0
ICC_BPR0 determines the preemption group for Group 0 interrupts.

ICC_BPR1 determines the preemption group for Group 1 interrupts.
1
ICC_BPR0 determines the preemption group for Group 0 and Group
1 interrupts.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.8 ICC_CTLR_EL3, Interrupt Controller Control Register,
EL3

ICC_CTLR_EL3 controls aspects of the behavior of the GIC CPU interface and provides information
about the features implemented.

Bit ﬁeld descriptions
ICC_CTLR_EL3 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Security registers functional group.

- The GIC control registers functional group.

Figure 15-4: ICC_CTLR_EL3 bit assignments

31
0
1
2
5
6
7
8
10
11
13
14
15
16

3
4
17
18

PRIbits
IDbits

CBPR_EL1S
CBPR_EL1NS

EOImode_EL3
EOImode_EL1S
EOImode_EL1NS
RM

RES0

PMHE

SEIS
A3V

nDS

RES0, [31:18]

RES0
Reserved.

nDS, [17]

Disable Security not supported. Read-only and writes are IGNORED. The value is:

1
The CPU interface logic does not support disabling of security, and
requires that security is not disabled.

RES0, [16]

RES0
Reserved.

A3V, [15]

Aﬃnity 3 Valid. This bit is RAO/WI.

SEIS, [14]

SEI Support. The value is:

0
The CPU interface logic does not support generation of SEIs.

IDbits, [13:11]

Identiﬁer bits. The value is:

0x0
The number of physical interrupt identiﬁer bits supported is 16 bits.

This ﬁeld is an alias of ICC_CTLR_EL3.IDbits.

PRIbits, [10:8]

Priority bits. The value is:

0x4
The core supports 32 levels of physical priority with 5 priority bits.

RES0, [7]

Reserved, RES0.

PMHE, [6]

Priority Mask Hint Enable. The possible values are:

0
Disables use of ICC_PMR as a hint for interrupt distribution.
1
Enables use of ICC_PMR as a hint for interrupt distribution.

RM, [5]

Routing Modiﬁer. This bit is RAZ/WI.

EOImode_EL1NS, [4]

EOI mode for interrupts handled at Non-secure EL1 and EL2.

Controls whether a write to an End of Interrupt register also deactivates the interrupt.

EOImode_EL1S, [3]

EOI mode for interrupts handled at Secure EL1.

Controls whether a write to an End of Interrupt register also deactivates the interrupt.

EOImode_EL3, [2]

EOI mode for interrupts handled at EL3.

Controls whether a write to an End of Interrupt register also deactivates the interrupt.

CBPR_EL1NS, [1]

Common Binary Point Register, EL1 Non-secure.

Control whether the same register is used for interrupt preemption of both Group 0 and
Group 1 Non-secure interrupts at EL1 and EL2.

CBPR_EL1S, [0]

Common Binary Point Register, EL1 Secure.

Control whether the same register is used for interrupt preemption of both Group 0 and
Group 1 Secure interrupt at EL1.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

## 15.9 ICC_SRE_EL1, Interrupt Controller System Register Enable Register, EL1

15.9 ICC_SRE_EL1, Interrupt Controller System Register
Enable Register, EL1

ICC_SRE_EL1 controls whether the System register interface or the memory-mapped interface to
the GIC CPU interface is used for EL0 and EL1.

Bit ﬁeld descriptions
ICC_SRE_EL1 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The GIC control registers functional group.

Figure 15-5: ICC_SRE_EL1 bit assignments

31
0
1
2
3

SRE
DFB
DIB

RES0

RES0, [31:3]

RES0
Reserved.

DIB, [2]

Disable IRQ bypass. The possible values are:

0x0
IRQ bypass enabled.
0x1
IRQ bypass disabled.

This bit is an alias of ICC_SRE_EL3.DIB

DFB, [1]

Disable FIQ bypass. The possible values are:

0x0
FIQ bypass enabled.
0x1
FIQ bypass disabled.

This bit is an alias of ICC_SRE_EL3.DFB

SRE, [0]

System Register Enable. The value is:

## 15.10 ICC_SRE_EL2, Interrupt Controller System Register Enable register, EL2

0x1
The System register interface for the current Security state is
enabled.

This bit is RAO/WI. The core only supports a System register interface to the GIC CPU
interface.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.10 ICC_SRE_EL2, Interrupt Controller System Register
Enable register, EL2

ICC_SRE_EL2 controls whether the System register interface or the memory-mapped interface to
the GIC CPU interface is used for EL2.

Bit ﬁeld descriptions
ICC_SRE_EL2 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Virtualization registers functional group.

- The GIC control registers functional group.

Figure 15-6: ICC_SRE_EL2 bit assignments

31
0
1
2
3

4

SRE
DFB
DIB

Enable

RES0

RES0, [31:4]

RES0
Reserved.

Enable, [3]

Enables lower Exception level access to ICC_SRE_EL1. The value is:

0x1
Non-secure EL1 accesses to ICC_SRE_EL1 do not trap to EL2.

This bit is RAO/WI.

## 15.11 ICC_SRE_EL3, Interrupt Controller System Register Enable register, EL3

DIB, [2]

Disable IRQ bypass. The possible values are:

0x0
IRQ bypass enabled.
0x1
IRQ bypass disabled.

This bit is an alias of ICC_SRE_EL3.DIB

DFB, [1]

Disable FIQ bypass. The possible values are:

0x0
FIQ bypass enabled.
0x1
FIQ bypass disabled.

This bit is an alias of ICC_SRE_EL3.DFB

SRE, [0]

System Register Enable. The value is:

0x1
The System register interface for the current Security state is
enabled.

This bit is RAO/WI. The core only supports a System register interface to the GIC CPU
interface.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.11 ICC_SRE_EL3, Interrupt Controller System Register
Enable register, EL3

ICC_SRE_EL3 controls whether the System register interface or the memory-mapped interface to
the GIC CPU interface is used for EL3.

Bit ﬁeld descriptions
ICC_SRE_EL3 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Security registers functional group.

- The GIC control registers functional group.

Figure 15-7: ICC_SRE_EL3 bit assignments

31
0
1
2
3

4

SRE
DFB
DIB

Enable

RES0

RES0, [31:4]

RES0
Reserved.

Enable, [3]

Enables lower Exception level access to ICC_SRE_EL1 and ICC_SRE_EL2. The value is:

1
Secure EL1 accesses to Secure ICC_SRE_EL1 do not trap to EL3.

EL2 accesses to Non-secure ICC_SRE_EL1 and ICC_SRE_EL2 do not
trap to EL3.

Non-secure EL1 accesses to ICC_SRE_EL1 do not trap to EL3.

This bit is RAO/WI.

DIB, [2]

Disable IRQ bypass. The possible values are:

0
IRQ bypass enabled.
1
IRQ bypass disabled.

DFB, [1]

Disable FIQ bypass. The possible values are:

0
FIQ bypass enabled.
1
FIQ bypass disabled.

SRE, [0]

System Register Enable. The value is:

1
The System register interface for the current Security state is
enabled.

## 15.12 AArch64 virtual GIC CPU interface register summary

## 15.13 ICV_AP0R0_EL1, Interrupt Controller Virtual Active Priorities Group 0 Register 0, EL1

This bit is RAO/WI. The core only supports a System register interface to the GIC CPU
interface.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.12 AArch64 virtual GIC CPU interface register
summary

The following table describes the AArch64 virtual GIC CPU interface System registers that have

IMPLEMENTATION DEFINED bits.

See the  Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and
version 4 for more information and a complete list of AArch64 virtual GIC CPU interface System
registers.

Table 15-3: AArch64 virtual GIC CPU interface register summary

15.13 ICV_AP0R0_EL1, Interrupt Controller Virtual Active
Priorities Group 0 Register 0, EL1

The ICV_AP0R0_EL1 register provides information about virtual Group 0 active priorities.

Bit descriptions
This register is a 32-bit register and is part of the virtual GIC System registers functional group.

The core implements 5 bits of priority with 32 priority levels, corresponding to the 32 bits [31:0] of
the register. The possible values for each bit are:

0x00000000
No interrupt active. This is the reset value.
0x00000001
Interrupt active for priority 0x0.
0x00000002
Interrupt active for priority 0x8.

|Name|Op0|Op1|CRn|CRm|Op2|Type|Description|
|---|---|---|---|---|---|---|---|
|ICV_AP0R0_EL1|3|0|12|8|4|RW|15.13 ICV_AP0R0_EL1, Interrupt Controller Virtual Active Priorities Group 0<br>Register 0, EL1 on page 283|
|ICV_AP1R0_EL1|3|0|12|9|0|RW|15.14 ICV_AP1R0_EL1, Interrupt Controller Virtual Active Priorities Group 1<br>Register 0, EL1 on page 284|
|ICV_BPR0_EL1|3|0|12|8|3|RW|15.15 ICV_BPR0_EL1, Interrupt Controller Virtual Binary Point Register 0, EL1 on<br>page 284|
|ICV_BPR1_EL1|3|0|12|12|3|RW|15.16 ICV_BPR1_EL1, Interrupt Controller Virtual Binary Point Register 1, EL1 on<br>page 285|
|ICV_CTLR_EL1|3|0|12|12|4|RW|15.17 ICV_CTLR_EL1, Interrupt Controller Virtual Control Register, EL1 on page<br>286|

## 15.14 ICV_AP1R0_EL1, Interrupt Controller Virtual Active Priorities Group 1 Register 0, EL1

## 15.15 ICV_BPR0_EL1, Interrupt Controller Virtual Binary Point Register 0, EL1

...
0x80000000
Interrupt active for priority 0xF8.

Details that are not provided in this description are architecturally deﬁned. See the  Arm® Generic
Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.14 ICV_AP1R0_EL1, Interrupt Controller Virtual Active
Priorities Group 1 Register 0, EL1

The ICV_AP1R0_EL1 register provides information about virtual Group 1 active priorities.

Bit descriptions
This register is a 32-bit register and is part of the virtual GIC System registers functional group.

The core implements 5 bits of priority with 32 priority levels, corresponding to the 32 bits [31:0] of
the register. The possible values for each bit are:

0x00000000
No interrupt active. This is the reset value.
0x00000001
Interrupt active for priority 0x0.
0x00000002
Interrupt active for priority 0x8.

...
0x80000000
Interrupt active for priority 0xF8.

Details that are not provided in this description are architecturally deﬁned. See the  Arm® Generic
Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.15 ICV_BPR0_EL1, Interrupt Controller Virtual Binary
Point Register 0, EL1

ICV_BPR0_EL1 deﬁnes the point at which the priority value ﬁelds split into two parts, the group
priority ﬁeld and the subpriority ﬁeld. The group priority ﬁeld determines virtual Group 0 interrupt
preemption.

Bit ﬁeld descriptions
ICC_BPR0_EL1 is a 32-bit register and is part of the virtual GIC System registers functional group.

## 15.16 ICV_BPR1_EL1, Interrupt Controller Virtual Binary Point Register 1, EL1

Figure 15-8: ICV_BPR0_EL1 bit assignments

31
0
2
3

BinaryPoint

RES0

RES0, [31:3]

Reserved, RES0.

BinaryPoint, [2:0]

The value of this ﬁeld controls how the 8-bit interrupt priority ﬁeld is split into a group
priority ﬁeld, that determines interrupt preemption, and a subpriority ﬁeld. The minimum
value that is implemented is:

0x2

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.16 ICV_BPR1_EL1, Interrupt Controller Virtual Binary
Point Register 1, EL1

ICV_BPR1_EL1 deﬁnes the point at which the priority value ﬁelds split into two parts, the group
priority ﬁeld and the subpriority ﬁeld. The group priority ﬁeld determines virtual Group 1 interrupt
preemption.

Bit ﬁeld descriptions
ICV_BPR1_EL1 is a 32-bit register and is part of the virtual GIC System registers functional group.

## 15.17 ICV_CTLR_EL1, Interrupt Controller Virtual Control Register, EL1

Figure 15-9: ICV_BPR1_EL1 bit assignments

31
0
2
3

BinaryPoint

RES0

RES0, [31:3]

RES0
Reserved.

BinaryPoint, [2:0]

The value of this ﬁeld controls how the 8-bit interrupt priority ﬁeld is split into a group
priority ﬁeld, that determines interrupt preemption, and a subpriority ﬁeld.

The minimum value that is implemented of ICV_BPR1_EL1 Secure register is 0x2.

The minimum value that is implemented of ICV_BPR1_EL1 Non-secure register is 0x3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.17 ICV_CTLR_EL1, Interrupt Controller Virtual Control
Register, EL1

ICV_CTLR_EL1 controls aspects of the behavior of the GIC virtual CPU interface and provides
information about the features implemented.

Bit ﬁeld descriptions
ICV_CTLR_EL1 is a 32-bit register and is part of the virtual GIC System registers functional group.

Figure 15-10: ICV_CTLR_EL1 bit assignments

31
0
16

15 14 13
10
8
7
2
1
11

IDbits

PRIbits

A3V
SEIS

VCBPR
VEOImode

RES0

RES0, [31:16]

RES0
Reserved.

A3V, [15]

Aﬃnity 3 Valid. The value is:

0x1
The virtual CPU interface logic supports nonzero values of Aﬃnity 3
in SGI generation System registers.

SEIS, [14]

SEI Support. The value is:

0x0
The virtual CPU interface logic does not support local generation of
SEIs.

IDbits, [13:11]

Identiﬁer bits. The value is:

0x0
The number of physical interrupt identiﬁer bits supported is 16 bits.

PRIbits, [10:8]

Priority bits. The value is:

0x4
Support 32 levels of physical priority (5 priority bits).

RES0, [7:2]

RES0
Reserved.

VEOImode, [1]

Virtual EOI mode. The possible values are:

## 15.18 AArch64 virtual interface control System register summary

0x0
ICV_EOIR0_EL1 and ICV_EOIR1_EL1 provide both priority drop and
interrupt deactivation functionality. Accesses to ICV_DIR_EL1 are
UNPREDICTABLE.
0x1
ICV_EOIR0_EL1 and ICV_EOIR1_EL1 provide priority drop
functionality only. ICV_DIR provides interrupt deactivation
functionality.

VCBPR, [0]

Common Binary Point Register. Controls whether the same register is used for interrupt
preemption of both virtual Group 0 and virtual Group 1 interrupts. The possible values are:

0
ICV_BPR0_EL1 determines the preemption group for virtual Group 0
interrupts only.

ICV_BPR1_EL1 determines the preemption group for virtual Group 1
interrupts.
1
ICV_BPR0_EL1 determines the preemption group for both virtual
Group 0 and virtual Group 1 interrupts.

Reads of ICV_BPR1_EL1 return ICV_BPR0_EL1 plus one, saturated
to 111. Writes to ICV_BPR1_EL1 are IGNORED.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.18 AArch64 virtual interface control System register
summary

The following table lists the AArch64 virtual interface control System registers that have

IMPLEMENTATION DEFINED bits.

See the  Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and
version 4 for more information and a complete list of AArch64 virtual interface control System
registers.

Table 15-4: AArch64 virtual interface control System register summary

|Name|Op0|Op1|CRn|CRm|Op2|Type|Description|
|---|---|---|---|---|---|---|---|
|ICH_AP0R0_EL2|3|0|12|8|4|RW|15.19 ICH_AP0R0_EL2, Interrupt Controller Hyp Active Priorities Group 0 Register<br>0, EL2 on page 289|
|ICH_AP1R0_EL2|3|0|19|9|0|RW|15.20 ICH_AP1R0_EL2, Interrupt Controller Hyp Active Priorities Group 1 Register<br>0, EL2 on page 289|
|ICH_HCR_EL2|3|4|12|11|0|RW|15.21 ICH_HCR_EL2, Interrupt Controller Hyp Control Register, EL2 on page<br>290|
|ICH_VTR_EL2|3|4|12|11|1|RO|15.22 ICH_VMCR_EL2, Interrupt Controller Virtual Machine Control Register, EL2<br>on page 292|

## 15.19 ICH_AP0R0_EL2, Interrupt Controller Hyp Active Priorities Group 0 Register 0, EL2

## 15.20 ICH_AP1R0_EL2, Interrupt Controller Hyp Active Priorities Group 1 Register 0, EL2

15.19 ICH_AP0R0_EL2, Interrupt Controller Hyp Active
Priorities Group 0 Register 0, EL2

The ICH_AP0R0_EL2 provides information about Group 0 active priorities for EL2.

Bit ﬁeld descriptions
This register is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Virtualization registers functional group.

- The GIC host interface control registers functional group.

The core implements 5 bits of priority with 32 priority levels, corresponding to the 32 bits [31:0] of
the register. The possible values for each bit are:

0x00000000
No interrupt active. This is the reset value.
0x00000001
Interrupt active for priority 0x0.
0x00000002
Interrupt active for priority 0x8.

...
0x80000000
Interrupt active for priority 0xF8.

Details that are not provided in this description are architecturally deﬁned. See the  Arm® Generic
Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.20 ICH_AP1R0_EL2, Interrupt Controller Hyp Active
Priorities Group 1 Register 0, EL2

The ICH_AP1R0_EL2 provides information about Group 1 active priorities for EL2.

Bit ﬁeld descriptions
This register is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Virtualization registers functional group.

- The GIC host interface control registers functional group.

The core implements 5 bits of priority with 32 priority levels, corresponding to the 32 bits [31:0] of
the register. The possible values for each bit are:

|Name|Op0|Op1|CRn|CRm|Op2|Type|Description|
|---|---|---|---|---|---|---|---|
|ICH_VMCR_EL2|3|4|12|11|7|RW|15.23 ICH_VTR_EL2, Interrupt Controller VGIC Type Register, EL2 on page 295|

## 15.21 ICH_HCR_EL2, Interrupt Controller Hyp Control Register, EL2

0x00000000
No interrupt active. This is the reset value.
0x00000001
Interrupt active for priority 0x0.
0x00000002
Interrupt active for priority 0x8.

...
0x80000000
Interrupt active for priority 0xF8.

Details that are not provided in this description are architecturally deﬁned. See the  Arm® Generic
Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.21 ICH_HCR_EL2, Interrupt Controller Hyp Control
Register, EL2

ICH_HCR_EL2 controls the environment for VMs.

Bit ﬁeld descriptions
ICH_HCR_EL2 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Virtualization registers functional group.

- The GIC host interface control registers functional group.

Figure 15-11: ICH_HCR_EL2 bit assignments

31
0
1
2
5
6
7
8
10
11
13
14
15
26

12
27

3
4

EOIcount

TDIR

En

TSEI
TALL1
TALL0

UIE

LRENPIE

NPIE
VGrp0EIE
VGrp0DIE
res0

TC

VGrp1EIE
VGrp1DIE

vSGIEOICount

EOIcount, [31:27]

Number of outstanding deactivates.

RES0, [26:15]

RES0
Reserved.

TDIR, [14]

Trap Non-secure EL1 writes to ICC_DIR_EL1 and ICV_DIR_EL1. The possible values are:

0x0
Non-secure EL1 writes of ICC_DIR_EL1 and ICV_DIR_EL1 are not
trapped to EL2, unless trapped by other mechanisms.

0x1
Non-secure EL1 writes of ICC_DIR_EL1 and ICV_DIR_EL1 are
trapped to EL2.

TSEI, [13]

Trap all locally generated SEIs. The value is:

0
Locally generated SEIs do not cause a trap to EL2.

TALL1, [12]

Trap all Non-secure EL1 accesses to ICC_* and ICV_* System registers for Group 1 interrupts
to EL2. The possible values are:

0x0
Non-secure EL1 accesses to ICC_* and ICV_* registers for Group 1
interrupts proceed as normal.
0x1
Non-secure EL1 accesses to ICC_* and ICV_* registers for Group 1
interrupts trap to EL2.

TALL0, [11]

Trap all Non-secure EL1 accesses to ICC_* and ICV_* System registers for Group 0 interrupts
to EL2. The possible values are:

0x0
Non-secure EL1 accesses to ICC_* and ICV_* registers for Group 0
interrupts proceed as normal.
0x1
Non-secure EL1 accesses to ICC_* and ICV_* registers for Group 0
interrupts trap to EL2.

TC, [10]

Trap all Non-secure EL1 accesses to System registers that are common to Group 0 and Group
1 to EL2. The possible values are:

0x0
Non-secure EL1 accesses to common registers proceed as normal.
0x1
Non-secure EL1 accesses to common registers trap to EL2.

RES0, [9]

RES0
Reserved.

vSGIEOICount, [8]

0x0
Deactivation of virtual SGIs can increment ICH_HCR_EL2.EOIcount.
0x1
Deactivation of virtual SGIs does not increment
ICH_HCR_EL2.EOIcount.

VGrp1DIE, [7]

VM Group 1 Disabled Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt signaled when ICH_VMCR_EL2.VENG1 is 0.

VGrp1EIE, [6]

VM Group 1 Enabled Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt signaled when ICH_VMCR_EL2.VENG1 is 1.

VGrp0DIE, [5]

VM Group 0 Disabled Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt signaled when ICH_VMCR_EL2.VENG0 is 0.

VGrp0EIE, [4]

VM Group 0 Enabled Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt signaled when ICH_VMCR_EL2.VENG0 is 1.

NPIE, [3]

No Pending Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt signaled while the List registers contain no
interrupts in the pending state.

LRENPIE, [2]

List Register Entry Not Present Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt is asserted while the EOIcount ﬁeld is not 0.

UIE, [1]

Underﬂow Interrupt Enable. The possible values are:

0
Maintenance interrupt disabled.
1
Maintenance interrupt is asserted if none, or only one, of the List
register entries is marked as a valid interrupt.

En, [0]

Enable. The possible values are:

0
Virtual CPU interface operation disabled.
1
Virtual CPU interface operation enabled.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

## 15.22 ICH_VMCR_EL2, Interrupt Controller Virtual Machine Control Register, EL2

15.22 ICH_VMCR_EL2, Interrupt Controller Virtual
Machine Control Register, EL2

ICH_VMCR_EL2 enables the hypervisor to save and restore the virtual machine view of the GIC
state.

Bit ﬁeld descriptions
ICH_VMCR_EL2 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Virtualization registers functional group.

- The GIC host interface control registers functional group.

Figure 15-12: ICH_VMCR_EL2 bit assignments

31
0
1
2
5
8
10
18
23

24

21

20

17
9

3
4

VPMR

VBPR0

VBPR1

VENG0

VENG1

VFIQEn
VCBPR

VEOIM

RES0

VPMR, [31:24]

Virtual Priority Mask.

This ﬁeld is an alias of ICV_PMR_EL1.Priority.

VBPR0, [23:21]

Virtual Binary Point Register, Group 0. The minimum value is:

0x2
This ﬁeld is an alias of ICV_BPR0_EL1.BinaryPoint.

VBPR1, [20:18]

Virtual Binary Point Register, Group 1. The minimum value is:

0x3
This ﬁeld is an alias of ICV_BPR1_EL1.BinaryPoint.

RES0, [17:10]

RES0
Reserved.

VEOIM, [9]

Virtual EOI mode. The possible values are:

0x0
ICV_EOIR0_EL1 and ICV_EOIR1_EL1 provide both priority drop and
interrupt deactivation functionality. Accesses to ICV_DIR_EL1 are
UNPREDICTABLE.
0x1
ICV_EOIR0_EL1 and ICV_EOIR1_EL1 provide priority drop
functionality only. ICV_DIR_EL1 provides interrupt deactivation
functionality.

This bit is an alias of ICV_CTLR_EL1.EOImode.

RES0, [8:5]

RES0
Reserved.

VCBPR, [4]

Virtual Common Binary Point Register. The possible values are:

0x0
ICV_BPR0_EL1 determines the preemption group for virtual Group 0
interrupts only.

ICV_BPR1_EL1 determines the preemption group for virtual Group 1
interrupts.
0x1
ICV_BPR0_EL1 determines the preemption group for both virtual
Group 0 and virtual Group 1 interrupts.

Reads of ICV_BPR1_EL1 return ICV_BPR0_EL1 plus one, saturated
to 111. Writes to ICV_BPR1_EL1 are IGNORED.

VFIQEn, [3]

Virtual FIQ enable. The value is:

0x1
Group 0 virtual interrupts are presented as virtual FIQs.

RES0, [2]

RES0
Reserved.

VENG1, [1]

Virtual Group 1 interrupt enable. The possible values are:

0x0
Virtual Group 1 interrupts are disabled.
0x1
Virtual Group 1 interrupts are enabled.

VENG0, [0]

Virtual Group 0 interrupt enable. The possible values are:

0x0
Virtual Group 0 interrupts are disabled.

## 15.23 ICH_VTR_EL2, Interrupt Controller VGIC Type Register, EL2

0x1
Virtual Group 0 interrupts are enabled.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

15.23 ICH_VTR_EL2, Interrupt Controller VGIC Type
Register, EL2

ICH_VTR_EL2 reports supported GIC virtualization features.

Bit ﬁeld descriptions
ICH_VTR_EL2 is a 32-bit register and is part of:

- The GIC System registers functional group.

- The Virtualization registers functional group.

- The GIC host interface control registers functional group.

Figure 15-13: ICH_VTR_EL2 bit assignments

29

28
26 25

22
19

31
0
5
18
23

4
21

20

PRIbits

PREbits

IDbits

ListRegs

TDS
nV4
A3V
SEIS

RES0

PRIbits, [31:29]

Priority bits. The number of virtual priority bits implemented, minus one.

0x4
Priority implemented is 5-bit.

PREbits, [28:26]

The number of virtual preemption bits implemented, minus one. The value is:

0x4
Virtual preemption implemented is 5-bit.

IDbits, [25:23]

The number of virtual interrupt identiﬁer bits supported. The value is:

0x0
Virtual interrupt identiﬁer bits that are implemented is 16-bit.

SEIS, [22]

SEI Support. The value is:

0x0
The virtual CPU interface logic does not support generation of SEIs.

A3V, [21]

Aﬃnity 3 Valid. The value is:

0x1
The virtual CPU interface logic supports nonzero values of Aﬃnity 3
in SGI generation System registers.

nV4, [20]

Direct injection of virtual interrupts not supported. The value is:

0x0
The CPU interface logic supports direct injection of virtual interrupts.

TDS, [19]

Separate trapping of Non-secure EL1 writes to ICV_DIR_EL1 supported. The value is:

0x1
Implementation supports ICH_HCR_EL2.TDIR.

RES0, [18:5]

RES0
Reserved.

ListRegs, [4:0]

0x3
The number of implemented List registers, minus one.

The core implements four list registers. Accesses to ICH_LR_EL2[x]
(x>3) in AArch64 are UNDEFINED.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Generic Interrupt Controller Architecture Speciﬁcation, GIC architecture version 3 and version 4.

# 16. Advanced SIMD and floating-point registers

## 16.1 AArch64 register summary

## 16.2 FPCR, Floating-point Control Register

16. Advanced SIMD and ﬂoating-point
registers

This chapter describes the Advanced SIMD and ﬂoating-point registers.

16.1 AArch64 register summary

The core has several Advanced SIMD and ﬂoating-point System registers in the AArch64 Execution
state. Each register has a speciﬁc purpose, speciﬁc usage constraints, conﬁgurations, and attributes.

The following table gives a summary of the Neoverse™ N1 core Advanced SIMD and ﬂoating-point
System registers in the AArch64 Execution state.

Table 16-1: AArch64 Advanced SIMD and ﬂoating-point System registers

The FPCR controls ﬂoating-point behavior.

Bit ﬁeld descriptions
FPCR is a 32-bit register.

|Name|Type|Reset|Description|
|---|---|---|---|
|FPCR|RW|`0x00000000`|See16.2 FPCR, Floating-point Control Register on page 297.|
|FPSR|RW|**UNKNOWN**|See16.3 FPSR, Floating-point Status Register on page 300.|
|MVFR0_EL1|RO|`0x10110222`|See16.4 MVFR0_EL1, Media, and VFP Feature Register 0, EL1 on page<br>302.|
|MVFR1_EL1|RO|`0x13211111`|See16.5 MVFR1_EL1, Media, and VFP Feature Register 1, EL1 on page<br>303.|
|MVFR2_EL1|RO|`0x00000043`|See16.6 MVFR2_EL1, Media, and VFP Feature Register 2, EL1 on page<br>305.|

Figure 16-1: FPCR bit assignments

31
0
27 26 25 24 23 22 21

20 19 18

AHP

FZ16

DN

FZ
RMode

RES0

RES0, [31:27]

RES0

Reserved.

AHP, [26]

Alternative half-precision control bit. The possible values are:

0
IEEE half-precision format selected. This is the reset value.

1
Alternative half-precision format selected.

DN, [25]

Default NaN mode control bit. The possible values are:

0
NaN operands propagate through to the output of a ﬂoating-point operation. This is
the reset value.

1
Any operation involving one or more NaNs returns the Default NaN.

FZ, [24]

Flush-to-zero mode control bit. The possible values are:

0
Flush-to-zero mode disabled. Behavior of the ﬂoating-point system is fully compliant
with the IEEE 754 standard. This is the reset value.

1
Flush-to-zero mode enabled.

RMode, [23:22]

Rounding Mode control ﬁeld. The encoding of this ﬁeld is:

0b00
Round to Nearest (RN) mode. This is the reset value.
0b01
Round towards Plus Inﬁnity (RP) mode.
0b10
Round towards Minus Inﬁnity (RM) mode.
0b11
Round towards Zero (RZ) mode.

RES0, [21:20]

RES0

Reserved.

FZ16, [19]

Flush-to-zero mode control bit on half-precision data-processing instructions. The possible
values are:

0
Flush-to-zero mode disabled. Behavior of the ﬂoating-point system is fully compliant
with the IEEE 754 standard. This is the default value.

1
Flush-to-zero mode enabled.

RES0, [18:0]

RES0

Reserved.

Conﬁgurations

The named ﬁelds in this register map to the equivalent ﬁelds in the AArch32 FPSCR. See
16.8 FPSCR, Floating-Point Status and Control Register on page 307.

Usage constraints

Accessing the FPCR

To access the FPCR:

Flush-to-zero mode disabled. Behavior of the floating-point
MRS <Xt>, FPCR ; Read FPCR into Xt
MSR FPCR, <Xt> ; Write Xt to FPCR

Register access is encoded as follows:

Table 16-2: FPCR access encoding

Accessibility

This register is accessible as follows:

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|0100|0100|000|

## 16.3 FPSR, Floating-point Status Register

The FPSR provides ﬂoating-point system status information.

Bit ﬁeld descriptions
FPSR is a 32-bit register.

Figure 16-2: FPSR bit assignments

31
0
8
7

30 29 28

27
6
5
4
3
2
1

26

QC
IDC

V

C

IXC

Z

UFC

N

OFC

DZC

IOC

RES0

N, [31]

Negative condition ﬂag for AArch32 ﬂoating-point comparison operations. AArch64 ﬂoating-
point comparisons set the PSTATE.N ﬂag instead.

Z, [30]

Zero condition ﬂag for AArch32 ﬂoating-point comparison operations. AArch64 ﬂoating-
point comparisons set the PSTATE.Z ﬂag instead.

C, [29]

Carry condition ﬂag for AArch32 ﬂoating-point comparison operations. AArch64 ﬂoating-
point comparisons set the PSTATE.C ﬂag instead

V, [28]

Overﬂow condition ﬂag for AArch32 ﬂoating-point comparison operations. AArch64 ﬂoating-
point comparisons set the PSTATE.V ﬂag instead.

QC, [27]

Cumulative saturation bit. This bit is set to 1 to indicate that an Advanced SIMD integer
operation has saturated since a 0 was last written to this bit.

RES0, [26:8]

Reserved, RES0.

|EL0|EL1<br>(NS)|EL1<br>(S)|EL2|EL3<br>(SCR.NS = 1)|EL3<br>(SCR.NS = 0)|
|---|---|---|---|---|---|
|RW|RW|RW|RW|RW|RW|

IDC, [7]

Input Denormal cumulative exception bit. This bit is set to 1 to indicate that the Input
Denormal exception has occurred since 0 was last written to this bit.

RES0, [6:5]

Reserved, RES0.

IXC, [4]

Inexact cumulative exception bit. This bit is set to 1 to indicate that the Inexact exception has
occurred since 0 was last written to this bit.

UFC, [3]

Underﬂow cumulative exception bit. This bit is set to 1 to indicate that the Underﬂow
exception has occurred since 0 was last written to this bit.

OFC, [2]

Overﬂow cumulative exception bit. This bit is set to 1 to indicate that the Overﬂow exception
has occurred since 0 was last written to this bit.

DZC, [1]

Division by Zero cumulative exception bit. This bit is set to 1 to indicate that the Division by
Zero exception has occurred since 0 was last written to this bit.

IOC, [0]

Invalid Operation cumulative exception bit. This bit is set to 1 to indicate that the Invalid
Operation exception has occurred since 0 was last written to this bit.

Conﬁgurations

The named ﬁelds in this register map to the equivalent ﬁelds in the AArch32 FPSCR. See
16.8 FPSCR, Floating-Point Status and Control Register on page 307.

Usage constraints

Accessing the FPSR

To access the FPSR:

MRS <Xt>, FPSR; Read FPSR into Xt
MSR FPSR, <Xt>; Write Xt to FPSR

Register access is encoded as follows:

Table 16-4: FPSR access encoding

Accessibility

This register is accessible as follows:

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|0100|0100|001|

## 16.4 MVFR0_EL1, Media, and VFP Feature Register 0, EL1

The MVFR0_EL1 describes the features that are provided by the AArch64 Advanced SIMD and
ﬂoating-point implementation.

Bit ﬁeld descriptions
MVFR0_EL1 is a 32-bit register.

Figure 16-3: MVFR0_EL1 bit assignments

FPRound, [31:28]

Indicates the rounding modes supported by the ﬂoating-point hardware:

0x1
All rounding modes supported.

FPShVec, [27:24]

Indicates the hardware support for ﬂoating-point short vectors:

0x0
Not supported.

FPSqrt, [23:20]

Indicates the hardware support for ﬂoating-point square root operations:

0x1
Supported.

FPDivide, [19:16]

Indicates the hardware support for ﬂoating-point divide operations:

0x1
Supported.

FPTrap, [15:12]

Indicates whether the ﬂoating-point hardware implementation supports exception trapping:

0x0
Not supported.

|EL0|EL1<br>(NS)|EL1<br>(S)|EL2|EL3<br>(SCR.NS = 1)|EL3<br>(SCR.NS = 0)|
|---|---|---|---|---|---|
|RW|RW|RW|RW|RW|RW|

|31 28|27 24|23 20|19 16|15 12|11 8|7 4|3 0|
|---|---|---|---|---|---|---|---|
|FPRound|FPShVec|FPSqrt|FPDivide|FPTrap|FPDP|FPSP|SIMDReg|

FPDP, [11:8]

Indicates the hardware support for ﬂoating-point double-precision operations:

0x2
Supported, VFPv3 or greater.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

FPSP, [7:4]

Indicates the hardware support for ﬂoating-point single-precision operations:

0x2
Supported, VFPv3 or greater.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

SIMDReg, [3:0]

Indicates support for the Advanced SIMD register bank:

0x2
Supported, 32 x 64-bit registers supported.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the MVFR0_EL1

To access the MVFR0_EL1:

MRS <Xt>, MVFR0_EL1 ; Read MVFR0_EL1 into Xt

Register access is encoded as follows:

Table 16-6: MVFR0_EL1 access encoding

Accessibility

This register is accessible as follows:

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|000|0000|0011|000|

|EL0|EL1(NS)|EL1(S)|EL2|EL3 (SCR.NS = 1)|EL3(SCR.NS = 0)|
|---|---|---|---|---|---|
|-|RO|RO|RO|RO|RO|

## 16.5 MVFR1_EL1, Media, and VFP Feature Register 1, EL1

The MVFR1_EL1 describes the features that are provided by the AArch64 Advanced SIMD and
ﬂoating-point implementation.

Bit ﬁeld descriptions
MVFR1_EL1 is a 32-bit register.

Figure 16-4: MVFR1_EL1 bit assignments

SIMDFMAC, [31:28]

Indicates whether the Advanced SIMD and ﬂoating-point unit supports fused multiply
accumulate operations:

1
Implemented.

FPHP, [27:24]

Indicates whether the Advanced SIMD and ﬂoating-point unit supports half-precision
ﬂoating-point conversion instructions:

3
Floating-point half-precision conversion and data processing instructions implemented.

SIMDHP, [23:20]

Indicates whether the Advanced SIMD and ﬂoating-point unit supports half-precision
ﬂoating-point conversion operations:

2
Advanced SIMD half-precision conversion and data processing instructions
implemented.

SIMDSP, [19:16]

Indicates whether the Advanced SIMD and ﬂoating-point unit supports single-precision
ﬂoating-point operations:

1
Implemented.

SIMDInt, [15:12]

Indicates whether the Advanced SIMD and ﬂoating-point unit supports integer operations:

1
Implemented.

|31 28|27 24|23 20|19 16|15 12|11 8|7 4|3 0|
|---|---|---|---|---|---|---|---|
|SIMDFMAC|FPHP|SIMDHP|SIMDSP|SIMDInt|SIMDLS|FPDNaN|FPFtZ|

SIMDLS, [11:8]

Indicates whether the Advanced SIMD and ﬂoating-point unit supports load/store
instructions:

1
Implemented.

FPDNaN, [7:4]

Indicates whether the ﬂoating-point hardware implementation supports only the Default
NaN mode:

1
Hardware supports propagation of NaN values.

FPFtZ, [3:0]

Indicates whether the ﬂoating-point hardware implementation supports only the Flush-to-
zero mode of operation:

1
Hardware supports full denormalized number arithmetic.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the MVFR1_EL1

To access the MVFR1_EL1:

MRS <Xt>, MVFR1_EL1 ; Read MVFR1_EL1 into Xt

Register access is encoded as follows:

Table 16-8: MVFR1_EL1 access encoding

Accessibility

This register is accessible as follows:

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|000|0000|0011|001|

|EL0|EL1(NS)|EL1(S)|EL2|EL3 (SCR.NS = 1)|EL3(SCR.NS = 0)|
|---|---|---|---|---|---|
|-|RO|RO|RO|RO|RO|

## 16.6 MVFR2_EL1, Media, and VFP Feature Register 2, EL1

The MVFR2_EL1 describes the features that are provided by the AArch64 Advanced SIMD and
ﬂoating-point implementation.

Bit ﬁeld descriptions
MVFR2_EL1 is a 32-bit register.

Figure 16-5: MVFR2_EL1 bit assignments

31
8
7
4
3
0

FPMisc
SIMDMisc

RES0

RES0, [31:8]

RES0

Reserved.

FPMisc, [7:4]

Indicates support for miscellaneous ﬂoating-point features.

0x4
Supports:

- Floating-point selection.

- Floating-point Conversion to Integer with Directed rounding
modes.

- Floating-point Round to Integral Floating-point.

- Floating-point MaxNum and MinNum.

SIMDMisc, [3:0]

Indicates support for miscellaneous Advanced SIMD features.

0x3
Supports:

- Floating-point Conversion to Integer with Directed rounding
modes.

- Floating-point Round to Integral Floating-point.

- Floating-point MaxNum and MinNum.

Conﬁgurations

There are no conﬁguration notes.

## 16.7 AArch32 register summary

## 16.8 FPSCR, Floating-Point Status and Control Register

Usage constraints

Accessing the MVFR2_EL1

To access the MVFR2_EL1:

MRS <Xt>, MVFR2_EL1 ; Read MVFR2_EL1 into Xt

Register access is encoded as follows:

Table 16-10: MVFR2_EL1 access encoding

Accessibility

This register is accessible as follows:

16.7 AArch32 register summary

The core has one Advanced SIMD and ﬂoating-point System registers in the AArch32 Execution
state.

The following table gives a summary of the Neoverse™ N1 core Advanced SIMD and ﬂoating-point
System registers in the AArch32 Execution state.

Table 16-12: AArch32 Advanced SIMD and ﬂoating-point System registers

See the Arm® Architecture Reference Manual for A-proﬁle architecture for information on permitted
accesses to the Advanced SIMD and ﬂoating-point System registers.

The FPSCR provides ﬂoating-point system status information and control.

Bit ﬁeld descriptions
FPSCR is a 32-bit register.

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|000|0000|0011|010|

|EL0|EL1(NS)|EL1(S)|EL2|EL3 (SCR.NS = 1)|EL3(SCR.NS = 0)|
|---|---|---|---|---|---|
|-|RO|RO|RO|RO|RO|

|Name|Type|Reset|Description|
|---|---|---|---|
|FPSCR|RW|**UNKNOWN**|See16.8 FPSCR, Floating-Point Status and Control Register on page<br>307.|

Figure 16-6: FPSCR bit assignments

31
0
8
7
16 15
27 26 25 24 23 22 21 20 19 18

30 29 28
6
5
4
3
2
1

N

Z C V

Len

QC
IOC
DZC
OFC
UFC
IXC

IDC
FZ16

AHP

DN

FZ
RMode

Stride

RES0

N, [31]

Floating-point Negative condition code ﬂag.

Set to 1 if a ﬂoating-point comparison operation produces a less than result.

Z, [30]

Floating-point Zero condition code ﬂag.

Set to 1 if a ﬂoating-point comparison operation produces an equal result.

C, [29]

Floating-point Carry condition code ﬂag.

Set to 1 if a ﬂoating-point comparison operation produces an equal, greater than, or
unordered result.

V, [28]

Floating-point Overﬂow condition code ﬂag.

Set to 1 if a ﬂoating-point comparison operation produces an unordered result.

QC, [27]

Cumulative saturation bit.

This bit is set to 1 to indicate that an Advanced SIMD integer operation has saturated after 0
was last written to this bit.

AHP, [26]

Alternative Half-Precision control bit:

0
IEEE half-precision format selected. This is the reset value.

1
Alternative half-precision format selected.

DN, [25]

Default NaN mode control bit:

0
NaN operands propagate through to the output of a ﬂoating-point operation. This is
the reset value.

1
Any operation involving one or more NaNs returns the Default NaN.

The value of this bit only controls ﬂoating-point arithmetic. AArch32 Advanced SIMD
arithmetic always uses the Default NaN setting, regardless of the value of the DN bit.

FZ, [24]

Flush-to-zero mode control bit:

0
Flush-to-zero mode disabled. Behavior of the ﬂoating-point system is fully compliant
with the IEEE 754 standard. This is the reset value.

1
Flush-to-zero mode enabled.

The value of this bit only controls ﬂoating-point arithmetic. AArch32 Advanced SIMD
arithmetic always uses the Flush-to-zero setting, regardless of the value of the FZ bit.

RMode, [23:22]

Rounding Mode control ﬁeld:

0b00
Round to Nearest (RN) mode. This is the reset value.

0b01
Round towards Plus Inﬁnity (RP) mode.

0b10
Round towards Minus Inﬁnity (RM) mode.

0b11
Round towards Zero (RZ) mode.

The speciﬁed rounding mode is used by almost all ﬂoating-point instructions. AArch32
Advanced SIMD arithmetic always uses the Round to Nearest setting, regardless of the value
of the RMode bits.

Stride, [21:20]

RES0

Reserved.

FZ16, [19]

Flush-to-zero mode control bit on half-precision data-processing instructions:

0
Flush-to-zero mode disabled. Behavior of the ﬂoating-point system is fully compliant
with the IEEE 754 standard.

1
Flush-to-zero mode enabled.

Len, [18:16]

RES0

Reserved.

RES0, [15:8]

RES0

Reserved.

IDC, [7]

Input Denormal cumulative exception bit. This bit is set to 1 to indicate that the Input
Denormal exception has occurred since 0 was last written to this bit.

RES0, [6:5]

RES0

Reserved.

IXC, [4]

Inexact cumulative exception bit. This bit is set to 1 to indicate that the Inexact exception has
occurred since 0 was last written to this bit.

UFC, [3]

Underﬂow cumulative exception bit. This bit is set to 1 to indicate that the Underﬂow
exception has occurred since 0 was last written to this bit.

OFC, [2]

Overﬂow cumulative exception bit. This bit is set to 1 to indicate that the Overﬂow exception
has occurred since 0 was last written to this bit.

DZC, [1]

Division by Zero cumulative exception bit. This bit is set to 1 to indicate that the Division by
Zero exception has occurred since 0 was last written to this bit.

IOC, [0]

Invalid Operation cumulative exception bit. This bit is set to 1 to indicate that the Invalid
Operation exception has occurred since 0 was last written to this bit.

Conﬁgurations

There is one copy of this register that is used in both Secure and Non-secure states.

The named ﬁelds in this register map to the equivalent ﬁelds in the AArch64 FPCR and FPSR.
See 16.2 FPCR, Floating-point Control Register on page 297 and 16.3 FPSR, Floating-point
Status Register on page 300

.

Usage constraints

Accessing the FPSCR

To access the FPSCR:

VMRS <Rt>, FPSCR ; Read FPSCR into Rt
VMSR FPSCR, <Rt> ; Write Rt to FPSCR

Register access is encoded as follows:

Table 16-13: FPSCR access encoding

spec_reg

0001

The Neoverse™ N1 core implementation does not support the deprecated VFP
short vector feature. Attempts to execute the associated VFP data-processing
instructions result in an UNDEFINED Instruction exception.

Accessibility

This register is accessible as follows:

Access to this register depends on the values of CPACR_EL1.FPEN, CPTR_EL2.FPEN,
CPTR_EL2.TFP, CPTR_EL3.TFP, and HCR_EL2.{E2H, TGE}. For details of which values of these
ﬁelds allow access at which Exception levels, see the Arm® Architecture Reference Manual for A-
proﬁle architecture.

|EL0<br>(NS)|EL0<br>(S)|EL1<br>(NS)|EL1<br>(S)|EL2|EL3<br>(SCR.NS = 1)|EL3<br>(SCR.NS = 0)|
|---|---|---|---|---|---|---|
|Conﬁg<br>|RW|-|-|-|-|-|

# 17. Debug

## 17.1 About debug methods

This chapter describes the Neoverse™ N1 core Debug registers and shows examples of how to use
them.

The core is part of a debug system and supports both self-hosted and external debug.

The following ﬁgure shows a typical external debug system.

Figure 17-1: External debug system

Debug target
Protocol
converter
Debug host

Core

Debug

unit

Debug host

A computer, for example a personal computer, that is running a software debugger such
as the DS-5 Debugger. With the debug host, you can issue high-level commands, such as
setting a breakpoint at a certain location or examining the contents of a memory address.

Protocol converter

The debug host sends messages to the debug target using an interface such as Ethernet.
However, the debug target typically implements a diﬀerent interface protocol. A device such
as DSTREAM is required to convert between the two protocols.

Debug target

The lowest level of the system implements system support for the protocol converter to
access the debug unit using the Advanced Peripheral Bus (APB) completer interface. An
example of a debug target is a development system with a test chip or a silicon part with a
core.

Debug unit

Helps debugging software that is running on the core:

- Hardware systems that are based on the core.

- Operating systems.

- Application software.

With the debug unit, you can:

- Stop program execution.

## 17.2 Debug register interfaces

### 17.2.1 Core interfaces

- Examine and alter process and coprocessor state.

- Examine and alter memory and the state of the input or output peripherals.

- Restart the core.

For self-hosted debug, the debug target runs additional debug monitor software that runs on the
Neoverse™ N1 core itself. This way, it does not require expensive interface hardware to connect a
second host computer.

The Debug architecture deﬁnes a set of Debug registers.

The Debug register interfaces provide access to these registers from:

- Software running on the core.

- An external debugger.

The Neoverse™ N1 core implements the Armv8 Debug architecture and debug events as described
in the Arm® Architecture Reference Manual for A-proﬁle architecture. It also implements improvements
to Debug introduced in Armv8.1 and Armv8.2.

System register access allows the core to directly access certain debug registers.

The external debug interface enables both external and self-hosted debug agents to access Debug
registers. Access to the Debug registers is partitioned as follows:

Debug registers

This function is System register based and memory-mapped. You can access the Debug
register map using the APB completer port.

Performance monitor

This function is System register based and memory-mapped. You can access the performance
monitor registers using the APB completer port.

Activity monitor

This function is System register based and memory-mapped. You can access the activity
monitor registers using the APB completer port.

Trace registers

This function is memory-mapped.

ELA registers

This function is memory-mapped.

### 17.2.2 Breakpoints and watchpoints

### 17.2.3 Effects of resets on Debug registers

### 17.2.4 External access permissions to Debug registers

Related information
External debug interface on page 316

17.2.2 Breakpoints and watchpoints

The core supports six breakpoints, four watchpoints, and a standard Debug Communications Channel
(DCC).

A breakpoint consists of a breakpoint control register and a breakpoint value register. These two
registers are referred to as a Breakpoint Register Pair (BRP).

Four of the breakpoints (BRP 0-3) match only to virtual address and the other two (BRP 4 and
5) match against either virtual address or context ID, or VMID. All the watchpoints can be linked
to two breakpoints (BRP 4 and 5) to enable a memory request to be trapped in a given process
context.

17.2.3 Eﬀects of resets on Debug registers

The core has the following reset signals that aﬀect the Debug registers:

nCPUPORESET

This signal initializes the core processing logic, including the debug, ETM trace unit,
breakpoint, watchpoint logic, and performance monitors logic. This maps to a Cold reset that
covers reset of the core processing logic and the integrated debug functionality.

nCORERESET

This signal resets some of the debug and performance monitor logic. This maps to a Warm
reset that covers reset of the core processing logic.

External access permission to the Debug registers is subject to the conditions at the time of the
access.

The following table describes the core response to accesses through the external debug interface.

Table 17-1: External access conditions to registers

|Name|Condition|Description|
|---|---|---|
|Oﬀ|EDPRSR.PU is 0|Core power domain is completely oﬀ, or in a low-power state where the Core power<br>domain registers cannot be accessed.<br>If debug power is oﬀ, then all external debug and memory-mapped register accesses return<br>an error.|
|DLK|`DoubleLockStatus() == TRUE`<br>(EDPRSR.DLK is 1)|OS Double Lock is locked.|
|OSLK|OSLSR_EL1.OSLK is 1|OS Lock is locked.|

## 17.3 Debug events

### 17.3.1 Watchpoint debug events

The following table shows an example of external register access condition codes for access to
a performance monitor register. To determine the access permission for the register, scan the
columns from left to right. Stop at the ﬁrst column a condition is true, the entry gives the access
permission of the register and scanning stops.

Table 17-2: External register condition code example

A debug event can be a software debug event or a Halting debug event.

A core responds to a debug event in one of the following ways:

- Ignores the debug event.

- Takes a debug exception.

- Enters debug state.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information about the
debug events.

Related information
External debug interface on page 316
About clocks, resets, and input synchronization on page 34

In the Neoverse™ N1 core, watchpoint debug events are always synchronous.

Memory hint instructions and cache clean operations, except DC ZVA and DC IVAC, do not generate
watchpoint debug events. Store exclusive instructions generate a watchpoint debug event even
when the check for the control of exclusive monitor fails. Atomic CAS instructions generate a
watchpoint debug event even when the compare operation fails.

|Name|Condition|Description|
|---|---|---|
|EDAD|`AllowExternalDebugAccess()`<br>`==FALSE`|External debug access is disabled. When an error is returned because of an EDAD<br>condition code, and this is the highest priority error condition, EDPRSR.SDAD is set to 1.<br>Otherwise SDAD is unchanged.|
|Default|-|None of the conditions apply, normal access.|

|Off|DLK|OSLK|EDAD|Default|
|---|---|---|---|---|
|-|-|-|-|RO|

### 17.3.2 Debug OS Lock

## 17.4 External debug interface

17.3.2 Debug OS Lock

Debug OS Lock is set by the powerup reset, nCPUPORESET.

For normal behavior of debug events and Debug register accesses, Debug OS Lock must be
cleared. For more information, see the Arm® Architecture Reference Manual for A-proﬁle architecture.

For information about external debug interface, including debug memory map and debug signals,
see the  Arm® DynamIQ™ Shared Unit Technical Reference Manual.

# 18. Performance Monitoring Unit

## 18.1 About the PMU

## 18.2 PMU functional description

### 18.2.1 External register access permissions

This chapter describes the Performance Monitoring Unit (PMU) and the registers that it uses.

18.1 About the PMU

The Neoverse™ N1 core includes performance monitors that enable you to gather various
statistics on the operation of the core and its memory system during runtime. These provide useful
information about the behavior of the core that you can use when debugging or proﬁling code.

The PMU provides six event counters. Each counter can count any of the events available in
the core. The absolute counts that are recorded might vary because of pipeline eﬀects. This has
negligible eﬀect except in cases where the counters are enabled for a very short time.

Related information
PMU events on page 318

This section describes the functionality of the PMU.

The PMU includes the following interfaces and counters:

Event interface

Events from all other units from across the design are provided to the PMU.

System register and APB interface

You can program the PMU registers using the System registers or the external APB interface.

Counters

The PMU has 32-bit counters that increment when they are enabled, based on events, and a
64-bit cycle counter.

PMU register interfaces

The Neoverse™ N1 core supports access to the performance monitor registers from the
internal System register interface and a memory-mapped interface.

Whether or not access is permitted to a register depends on:

- If the core is powered up.

- The state of the OS Lock and OS Double Lock.

- The state of External Performance Monitors access disable.

## 18.3 PMU events

- The state of the debug authentication inputs to the core.

The behavior is speciﬁc to each register and is not described in this document. For a detailed
description of these features and their eﬀects on the registers, see the Arm® Architecture Reference
Manual Arm®v8, for Arm®v8-A architecture proﬁle.

The register descriptions that are provided in this manual describe whether each register is read/
write or read-only.

The following table shows the events that are generated and the numbers that the PMU uses to
reference the events. The table also shows the bit position of each event on the event bus. Event
reference numbers that are not listed are reserved.

Table 18-1: PMU Events

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x0`|[00]|SW_INCR|Software increment. Instruction architecturally executed (condition code check pass).|
|`0x1`|[01]|L1I_CACHE_REFILL|L1 instruction cache reﬁll. This event counts any instruction fetch which misses in the<br>cache.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions.<br>•<br>Non-cacheable accesses.|
|`0x2`|[02]|L1I_TLB_REFILL|L1 instruction TLB reﬁll. This event counts any reﬁll of the instruction L1 TLB from<br>the L2 TLB. This includes reﬁlls that result in a translation fault.<br>The following instructions are not counted:<br>•<br>TLB maintenance instructions.<br>This event counts regardless of whether the MMU is enabled.|
|`0x3`|[167]|L1D_CACHE_REFILL|L1 data cache reﬁll. This event counts any load or store operation or page table walk<br>access which causes data to be read from outside the L1, including accesses which<br>do not allocate into L1.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions and prefetches.<br>•<br>Stores of an entire cache line, even if they make a coherency request outside the<br>L1.<br>•<br>Partial cache line writes which do not allocate into the L1 cache.<br>•<br>Non-cacheable accesses.<br>This event counts the sum of L1D_CACHE_REFILL_RD and<br>L1D_CACHE_REFILL_WR.|

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x4`|[05:03]|L1D_CACHE|L1 data cache access. This event counts any load or store operation or page table<br>walk access which looks up in the L1 data cache. In particular, any access which could<br>count the L1D_CACHE_REFILL event causes this event to count.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions and prefetches.<br>•<br>Non-cacheable accesses.<br>This event counts the sum of L1D_CACHE_RD and L1D_CACHE_WR.|
|`0x5`|[07:06]|L1D_TLB_REFILL|L1 data TLB reﬁll. This event counts any reﬁll of the data L1 TLB from the L2 TLB.<br>This includes reﬁlls that result in a translation fault. The following instructions are not<br>counted:<br>•<br>TLB maintenance instructions.<br>This event counts regardless of whether the MMU is enabled.|
|`0x8`|[11:08]|INST_RETIRED|Instruction architecturally executed. This event counts all retired instructions,<br>including those that fail their condition check.|
|`0x9`|[12]|EXC_TAKEN|Exception taken.|
|`0x0A`|[13]|EXC_RETURN|Instruction architecturally executed, condition code check pass, exception return.|
|`0x0B`|[156]|CID_WRITE_RETIRED|Instruction architecturally executed, condition code check pass, write to<br>CONTEXTIDR. This event only counts writes to CONTEXTIDR in AArch32 state, and<br>via the CONTEXTIDR_EL1 mnemonic in AArch64 state.<br>The following instructions are not counted:<br>•<br>Writes to CONTEXTIDR_EL12 and CONTEXTIDR_EL2.|
|`0x10`|[14]|BR_MIS_PRED|Mispredicted or not predicted branch speculatively executed. This event counts<br>any predictable branch instruction which is mispredicted either due to dynamic<br>misprediction or because the MMU is oﬀ and the branches are statically predicted<br>not taken.|
|`0x11`|[15]|CPU_CYCLES|Cycle|
|`0x12`|[16]|BR_PRED|Predictable branch speculatively executed. This event counts all predictable branches.|
|`0x13`|[19:17]|MEM_ACCESS|Data memory access. This event counts memory accesses due to load or store<br>instructions.<br>The following instructions are not counted:<br>•<br>Instruction fetches.<br>•<br>Cache maintenance instructions.<br>•<br>Translation table walks or prefetches.<br>This event counts the sum of MEM_ACCESS_RD and MEM_ACCESS_WR.|
|`0x14`|[20]|L1I_CACHE|L1 instruction cache access or Level 0 Macro-op cache access. This event counts any<br>instruction fetch which accesses the L1 instruction cache or L0 Macro-op cache.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions.<br>•<br>Non-cacheable accesses.|

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x15`|[21]|L1D_CACHE_WB|L1 data cache Write-Back. This event counts any write-back of data from the L1 data<br>cache to L2 or L3. This counts both victim line evictions and snoops, including cache<br>maintenance operations.<br>The following instructions are not counted:<br>•<br>Invalidations which do not result in data being transferred out of the L1.<br>•<br>Full-line writes which write to L2 without writing L1, such as write streaming<br>mode.|
|`0x16`|[24:22]|L2D_CACHE|L2 uniﬁed cache access. This event counts any transaction from L1 which looks up<br>in the L2 cache, and any write-back from the L1 to the L2. Snoops from outside the<br>core and cache maintenance operations are not counted.|
|`0x17`|[27:25]|L2D_CACHE_REFILL|L2 uniﬁed cache reﬁll. This event counts any Cacheable transaction from L1 which<br>causes data to be read from outside the core. L2 reﬁlls caused by stashes and<br>prefetches that target this level of cache, should not be counted.|
|`0x18`|[30:28]|L2D_CACHE_WB|L2 uniﬁed cache write-back. This event counts any write-back of data from the<br>L2 cache to outside the core. This includes snoops to the L2 which return data,<br>regardless of whether they cause an invalidation. Invalidations from the L2 which do<br>not write data outside of the core and snoops which return data from the L1 are not<br>counted.|
|`0x19`|[32:31]|BUS_ACCESS|Bus access. This event counts for every beat of data that are transferred over the<br>data channels between the core and the SCU. If both read and write data beats are<br>transferred on a given cycle, this event is counted twice on that cycle. This event<br>counts the sum of BUS_ACCESS_RD, BUS_ACCESS_WR, and snoops.|
|`0x1A`|[33]|MEMORY_ERROR|Local memory error. This event counts any correctable or uncorrectable memory<br>error (ECC or parity) in the protected core RAMs.|
|`0x1B`|[36:34]|INST_SPEC|Operation speculatively executed|
|`0x1C`|[37]|TTBR_WRITE_RETIRED|Instruction architecturally executed, condition code check pass, write to TTBR.This<br>event only counts writes to TTBR0/TTBR1 in AArch32 state and TTBR0_EL1/<br>TTBR1_EL1 in AArch64 state.<br>The following instructions are not counted:<br>•<br>Accesses to TTBR0_EL12/TTBR1_EL12 or TTBR0_EL2/TTBR1_EL2.|
|`0x1D`|[38]|BUS_CYCLES|Bus cycles. This event duplicates CPU_CYCLES.|
|`0x1E`|[39]|CHAIN|For odd-numbered counters, increments the count by one for each overﬂow of<br>the preceding even-numbered counter. For even-numbered counters, there is no<br>increment.|
|`0x20`|[41:40]|L2D_CACHE_ALLOCATE|L2 uniﬁed cache allocation without reﬁll. This event counts any full cache line write<br>into the L2 cache which does not cause a lineﬁll, including write-backs from L1 to L2<br>and full-line writes which do not allocate into L1.|
|`0x21`|[42]|BR_RETIRED|Instruction architecturally executed, branch. This event counts all branches, taken or<br>not. This excludes exception entries, debug entries and CCFAIL branches.|
|`0x22`|[43]|BR_MIS_PRED_RETIRED|Instruction architecturally executed, mispredicted branch. This event counts any<br>branch counted by BR_RETIRED which is not correctly predicted and causes a<br>pipeline ﬂush.|
|`0x23`|[44]|STALL_FRONTEND|No operation issued because of the frontend. The counter counts on any cycle when<br>there are no fetched instructions available to dispatch.|
|`0x24`|[45]|STALL_BACKEND|No operation issued because of the backend. The counter counts on any cycle<br>fetched instructions are not dispatched due to resource constraints.|

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x25`|[48:46]|L1D_TLB|L1 data TLB access. This event counts any load or store operation which accesses<br>the data L1 TLB. If both a load and a store are executed on a cycle, this event counts<br>twice. This event counts regardless of whether the MMU is enabled.|
|`0x26`|[168]|L1I_TLB|L1 instruction TLB access. This event counts any instruction fetch which accesses the<br>instruction L1 TLB.This event counts regardless of whether the MMU is enabled.|
|`0x29`|[157]|L3D_CACHE_ALLOCATE|Attributable L3 data or uniﬁed cache allocation without reﬁll. This event counts any<br>full cache line write into the L3 cache which does not cause a lineﬁll, including write-<br>backs from L2 to L3 and full-line writes which do not allocate into L2.|
|`0x2A`|[159:158]|L3D_CACHE_REFILL|Attributable Level 3 uniﬁed cache reﬁll.<br>This event counts for any cacheable read transaction returning data from the SCU<br>for which the data source was outside the cluster. Transactions such as ReadUnique<br>are counted here as 'read' transactions, even though they can be generated by store<br>instructions.<br>Prefetches and stashes that target the L3 cache are not counted.|
|`0x2B`|[160]|L3D_CACHE_RD|Attributable L3 uniﬁed cache access.<br>This event counts for any cacheable read transaction returning data from the SCU, or<br>for any cacheable write to the SCU.|
|`0x2D`|[49]|L2D_TLB_REFILL|Attributable L2 data or uniﬁed TLB reﬁll. This event counts on any reﬁll of the L2<br>TLB, caused by either an instruction or data access. This event does not count if the<br>MMU is disabled.|
|`0x2F`|[51:50]|L2D_TLB|Attributable L2 data or uniﬁed TLB access. This event counts on any access to the L2<br>TLB (caused by a reﬁll of any of the L1 TLBs). This event does not count if the MMU<br>is disabled.|
|`0x31`|[161]|REMOTE_ACCESS|Access to another socket in a multi-socket system.|
|`0x34`|[52]|DTLB_WALK|Access to data TLB that caused a page table walk. This event counts on any data<br>access which causes L2D_TLB_REFILL to count.|
|`0x35`|[53]|ITLB_WALK|Access to instruction TLB that caused a page table walk. This event counts on any<br>instruction access which causes L2D_TLB_REFILL to count.|
|`0x36`|[163:162]|LL_CACHE_RD|Last level cache access, read.<br>•<br>If CPUECTLR.EXTLLC is set: This event counts any cacheable read transaction<br>which returns a data source of 'interconnect cache'.<br>•<br>If CPUECTLR.EXTLLC is not set: This event is a duplicate of the L*D_CACHE_RD<br>event corresponding to the last level of cache implemented – L3D_CACHE_RD<br>if both per-core L2 and cluster L3 are implemented, L2D_CACHE_RD if only one<br>is implemented, or L1D_CACHE_RD if neither is implemented.|
|`0x37`|[165:164]|LL_CACHE_MISS_RD|Last level cache miss, read.<br>•<br>If CPUECTLR.EXTLLC is set: This event counts any cacheable read transaction<br>which returns a data source of 'DRAM', 'remote' or 'inter-cluster peer'.<br>•<br>If CPUECTLR.EXTLLC is not set: This event is a duplicate of the<br>L*D_CACHE_REFILL_RD event corresponding to the last level of cache<br>implemented – L3D_CACHE_REFILL_RD if both per-core L2 and cluster L3<br>are implemented, L2D_CACHE_REFILL_RD if only one is implemented, or<br>L1D_CACHE_REFILL_RD if neither is implemented.|

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x40`|[55:54 ]|L1D_CACHE_RD|L1 data cache access, read. This event counts any load operation or page table walk<br>access which looks up in the L1 data cache. In particular, any access which could<br>count the L1D_CACHE_REFILL_RD event causes this event to count.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions and prefetches.<br>•<br>Non-cacheable accesses.|
|`0x41`|[57:56]|L1D_CACHE_WR|L1 data cache access, write. This event counts any store operation which<br>looks up in the L1 data cache. In particular, any access which could count the<br>L1D_CACHE_REFILL_WR event causes this event to count.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions and prefetches.<br>•<br>Non-cacheable accesses.|
|`0x42`|[58]|L1D_CACHE_REFILL_RD|L1 data cache reﬁll, read. This event counts any load operation or page table walk<br>access which causes data to be read from outside the L1, including accesses which<br>do not allocate into L1.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions and prefetches.<br>•<br>Non-cacheable accesses.|
|`0x43`|[59]|L1D_CACHE_REFILL_WR|L1 data cache reﬁll, write. This event counts any store operation which causes data to<br>be read from outside the L1, including accesses which do not allocate into L1.<br>The following instructions are not counted:<br>•<br>Cache maintenance instructions and prefetches.<br>•<br>Stores of an entire cache line, even if they make a coherency request outside the<br>L1.<br>•<br>Partial cache line writes which do not allocate into the L1 cache.<br>•<br>Non-cacheable accesses.|
|`0x44`|[60]|L1D_CACHE_REFILL_INNER|L1 data cache reﬁll, inner. This event counts any L1 data cache lineﬁll (as counted by<br>L1D_CACHE_REFILL) which hits in the L2 cache, L3 cache, or another core in the<br>cluster.|
|`0x45`|[61]|L1D_CACHE_REFILL_OUTER|L1 data cache reﬁll, outer. This event counts any L1 data cache lineﬁll (as counted by<br>L1D_CACHE_REFILL) which does not hit in the L2 cache, L3 cache, or another core<br>in the cluster, and instead obtains data from outside the cluster.|
|`0x46`|[62]|L1D_CACHE_WB_VICTIM|L1 data cache write-back, victim|
|`0x47`|[63]|L1D_CACHE_WB_CLEAN|L1 data cache write-back cleaning and coherency|
|`0x48`|[64]|L1D_CACHE_INVAL|L1 data cache invalidate.|
|`0x4C`|[65]|L1D_TLB_REFILL_RD|L1 data TLB reﬁll, read.|
|`0x4D`|[66]|L1D_TLB_REFILL_WR|L1 data TLB reﬁll, write.|
|`0x4E`|[68:67]|L1D_TLB_RD|L1 data TLB access, read.|
|`0x4F`|[70:69]|L1D_TLB_WR|L1 data TLB access, write.|

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x50`|[72:71]|L2D_CACHE_RD|L2 uniﬁed cache access, read. This event counts any read transaction from L1 which<br>looks up in the L2 cache.<br>Snoops from outside the core are not counted.|
|`0x51`|[74:73]|L2D_CACHE_WR|L2 uniﬁed cache access, write. This event counts any write transaction from L1 which<br>looks up in the L2 cache or any write-back from L1 which allocates into the L2 cache.<br>Snoops from outside the core are not counted.|
|`0x52`|[76:75]|L2D_CACHE_REFILL_RD|L2 uniﬁed cache reﬁll, read. This event counts any cacheable read transaction from<br>L1 which causes data to be read from outside the core. L2 reﬁlls caused by stashes<br>into L2 should not be counted. Transactions such as ReadUnique are counted here as<br>'read' transactions, even though they can be generated by store instructions.|
|`0x53`|[78:77]|L2D_CACHE_REFILL_WR|L2 uniﬁed cache reﬁll, write. This event counts any write transaction from L1 which<br>causes data to be read from outside the core. L2 reﬁlls caused by stashes into L2<br>should not be counted. Transactions such as ReadUnique are not counted as write<br>transactions.|
|`0x56`|[80:79]|L2D_CACHE_WB_VICTIM|L2 uniﬁed cache write-back, victim.|
|`0x57`|[82:81]|L2D_CACHE_WB_CLEAN|L2 uniﬁed cache write-back, cleaning, and coherency.|
|`0x58`|[84:83]|L2D_CACHE_INVAL|L2 uniﬁed cache invalidate.|
|`0x5C`|[85]|L2D_TLB_REFILL_RD|L2 data or uniﬁed TLB reﬁll, read.|
|`0x5D`|[86]|L2D_TLB_REFILL_WR|L2 data or uniﬁed TLB reﬁll, write.|
|`0x5E`|[88:87]|L2D_TLB_RD|L2 data or uniﬁed TLB access, read.|
|`0x5F`|[89]|L2D_TLB_WR|L2 data or uniﬁed TLB access, write.|
|`0x60`|[90]|BUS_ACCESS_RD|Bus access read. This event counts for every beat of data transferred over the read<br>data channel between the core and the SCU.|
|`0x61`|[91]|BUS_ACCESS_WR|Bus access write. This event counts for every beat of data transferred over the write<br>data channel between the core and the SCU.|
|`0x66`|[93:92]|MEM_ACCESS_RD|Data memory access, read. This event counts memory accesses due to load<br>instructions. The following instructions are not counted:<br>• Instruction fetches.<br>• Cache maintenance instructions.<br>• Translation table walks.<br>• Prefetches.|
|`0x67`|[95:94]|MEM_ACCESS_WR|Data memory access, write. This event counts memory accesses due to store<br>instructions.<br>The following instructions are not counted:<br>• Instruction fetches.<br>• Cache maintenance instructions.<br>• Translation table walks.<br>• Prefetches.|

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x68`|[97:96]|UNALIGNED_LD_SPEC|Unaligned access, read|
|`0x69`|[99:98]|UNALIGNED_ST_SPEC|Unaligned access, write|
|`0x6A`|[102:100]|UNALIGNED_LDST_SPEC|Unaligned access|
|`0x6C`|[103]|LDREX_SPEC|Exclusive operation speculatively executed, LDREX or LDX.|
|`0x6D`|[104]|STREX_PASS_SPEC|Exclusive operation speculatively executed, STREX or STX pass.|
|`0x6E`|[105]|STREX_FAIL_SPEC|Exclusive operation speculatively executed, STREX or STX fail.|
|`0x6F`|[106]|STREX_SPEC|Exclusive operation speculatively executed, STREX or STX.|
|`0x70`|[109:107]|LD_SPEC|Operation speculatively executed, load.|
|`0x71`|[112:110]|ST_SPEC|Operation speculatively executed, store.|
|`0x73`|[117:115]|DP_SPEC|Operation speculatively executed, integer data-processing.|
|`0x74`|[120:118]|ASE_SPEC|Operation speculatively executed, Advanced SIMD instruction.|
|`0x75`|[123:121]|VFP_SPEC|Operation speculatively executed, ﬂoating-point instruction.|
|`0x76`|[125:124]|PC_WRITE_SPEC|Operation speculatively executed, software change of the PC.|
|`0x77`|[128:126]|CRYPTO_SPEC|Operation speculatively executed, Cryptographic instruction.|
|`0x78`|[129]|BR_IMMED_SPEC|Branch speculatively executed, immediate branch.|
|`0x79`|[130]|BR_RETURN_SPEC|Branch speculatively executed, procedure return.|
|`0x7A`|[131]|BR_INDIRECT_SPEC|Branch speculatively executed, indirect branch.|
|`0x7C`|[132]|ISB_SPEC|Barrier speculatively executed, ISB.|
|`0x7D`|[134:133]|DSB_SPEC|Barrier speculatively executed, DSB.|
|`0x7E`|[136:135]|DMB_SPEC|Barrier speculatively executed, DMB.|
|`0x81`|[137]|EXC_UNDEF|Counts the number of undeﬁned exceptions taken locally.|
|`0x82`|[138]|EXC_SVC|Exception taken locally, Supervisor Call.|
|`0x83`|[139]|EXC_PABORT|Exception taken locally, Instruction Abort.|
|`0x84`|[140]|EXC_DABORT|Exception taken locally, Data Abort and SError.|
|`0x86`|[141]|EXC_IRQ|Exception taken locally, IRQ.|
|`0x87`|[142]|EXC_FIQ|Exception taken locally, FIQ.|
|`0x88`|[143]|EXC_SMC|Exception taken locally, Secure Monitor Call.|
|`0x8A`|[144]|EXC_HVC|Exception taken locally, Hypervisor Call.|
|`0x8B`|[145]|EXC_TRAP_PABORT|Exception taken, Instruction Abort not taken locally.|
|`0x8C`|[146]|EXC_TRAP_DABORT|Exception taken, Data Abort or SError not taken locally.|
|`0x8D`|[147]|EXC_TRAP_OTHER|Exception taken, Other traps not taken locally.|
|`0x8E`|[148]|EXC_TRAP_IRQ|Exception taken, IRQ not taken locally.|
|`0x8F`|[149]|EXC_TRAP_FIQ|Exception taken, FIQ not taken locally.|
|`0x90`|[152:150]|RC_LD_SPEC|Release consistency operation speculatively executed, load-acquire.|
|`0x91`|[155:153]|RC_ST_SPEC|Release consistency operation speculatively executed, store-release.|
|`0xA0`|[166]|L3D_CACHE_RD|L3 data cache read.|
|`0x4000`|N/A|SAMPLE_POP|Sample Population|
|`0x4001`|N/A|SAMPLE_FEED|Sample Taken|
|`0x4002`|N/A|SAMPLE_FILTRATE|Sample Taken and not removed by ﬁltering|

## 18.4 PMU interrupts

## 18.5 Exporting PMU events

18.4 PMU interrupts

The Neoverse™ N1 core asserts the nPMUIRQ signal when the PMU generates an interrupt.

You can route this signal to an external interrupt controller for prioritization and masking. This is the
only mechanism that signals this interrupt to the core.

This interrupt is also driven as a trigger input to the CTI. See the  Arm® DynamIQ™ Shared Unit
Technical Reference Manual for more information.

Some of the PMU events are exported to the ETM trace unit to be monitored.

The PMUEVENT bus is not exported to external components. This is because the
event bus cannot safely cross an asynchronous boundary when events can be
generated on every cycle.

|Event<br>number|PMU<br>event bus<br>(to trace)|Event mnemonic|Event description|
|---|---|---|---|
|`0x4003`|N/A|SAMPLE_COLLISION|Sample collided with previous sample|

# 19. Activity Monitor Unit

## 19.1 About the AMU

## 19.2 Accessing the activity monitors

### 19.2.1 Access enable bit

This chapter describes the Activity Monitor Unit (AMU).

19.1 About the AMU

The Neoverse™ N1 core includes activity monitoring. It has features in common with performance
monitoring, but is intended for system management use whereas performance monitoring is aimed
at user and debug applications.

The activity monitors provide useful information for system power management and persistent
monitoring. The activity monitors are read-only in operation and their conﬁguration is limited to the
highest Exception level implemented.

The Neoverse™ N1 core implements ﬁve counters, 0-4, and activity monitoring is only implemented
in AArch64.

The Neoverse™ N1 core activity monitoring is not architecturally described in the
Arm® Architecture Reference Manual for A-proﬁle architecture. Details regarding the
AMU implementation for the Neoverse™ N1 core are found wholly within this
chapter.

The activity monitors can be accessed by:

- The System register interface for both AArch64 and AArch32 states.

- Read-only memory-mapped access using the debug APB interface.

The access enable bit for traps on accesses to activity monitor registers is required at EL2 and EL3.

In the Neoverse™ N1 core, the AMEN[4] bit in registers ACTLR_EL2 and ACTLR_EL3 controls the
activity monitor registers enable.

In the Neoverse™ N1 core, the AMEN[4] bit is RES0 in ACTLR and HACTLR. Activity
monitors are not implemented in AArch32.

### 19.2.2 System register access

### 19.2.3 External memory-mapped access

## 19.3 AMU counters

## 19.4 AMU events

19.2.2 System register access

The core implements activity monitoring in AArch64 and the activity monitors can be accessed
using the MRS and MSR instructions.

19.2.3 External memory-mapped access

Activity monitors can also be memory-mapped accessed from the APB debug interface.

In this case, the AMU registers provide debug information and are read-only.

19.3 AMU counters

The Neoverse™ N1 core implements ﬁve counters, 0-4. The activity monitor counters,
AMEVCNTR<0-4>_EL0, have the following characteristics:

- All events are counted in 64-bit wrapping counters that overﬂow when they wrap. There is no
support for overﬂow status indication or interrupts.

- Any change in clock frequency, including when a WFI and WFE instruction stops the clock, can
aﬀect any counter.

- Events 0, 1, 2, 3, and 4 are ﬁxed, and the AMEVTYPER<n> evtCount bits are read-only.

The following table describes the counters that are implemented in the Neoverse™ N1 core and the
mapping to events. All events are ﬁxed.

Table 19-1: Mapping of counters to ﬁxed events

|Activity<br>monitor<br>counter<br><n>|Event|Event<br>number|Description|
|---|---|---|---|
|0|Cycles<br>at core<br>frequency|`0x11`|Cycles count.|
|1|Cycles at<br>constant<br>frequency|`0xEF`|This counter is used to replicate the generic system counter that is incremented on a constant basis,<br>and not incremented depending on the PE frequency core.|
|2|Instructions<br>retired|`0x08`|Instruction architecturally executed. This counter increments for every instruction that is executed<br>architecturally, including instructions that fail their condition code check.|
|3|First miss|`0xF0`|The ﬁrst miss event tracks whether any external load miss is outstanding and starts counting only<br>from a ﬁrst-miss until data returns for that miss. The counter does not count for any remaining part of<br>overlapping accesses, only counting again when the ﬁrst-miss condition is re-detected.|

To program AMU counter 4, you need to program the AMEVTYPER4_EL0 register.
For more information, see 29.7 AMEVTYPERn_EL0, Activity Monitor Event Type
Register, EL0 on page 409.

|Activity<br>monitor<br>counter<br><n>|Event|Event<br>number|Description|
|---|---|---|---|
|4|High<br>activity|`0xF1`|Instructions executing through the design which act as a hint for potential high power activity.|

# 20. Embedded Trace Macrocell

## 20.1 About the ETM

## 20.2 ETM trace unit generation options and resources

This chapter describes the ETM for the Neoverse™ N1 core.

20.1 About the ETM

The ETM trace unit is a module that performs real-time instruction ﬂow tracing that is based on the
ETMv4 architecture. The ETM is a CoreSight component, and is an integral part of the Arm Real-
time Debug solution, DS-5 Development Studio.

See the  Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4  for more information.

The following table shows the trace generation options that are implemented in the Neoverse™ N1
ETM trace unit.

Table 20-1: ETM trace unit generation options implemented

|Description|Configuration|
|---|---|
|Instruction address size in bytes|8|
|Data address size in bytes|0|
|Data value size in bytes|0|
|Virtual Machine ID size in bytes|4|
|Context ID size in bytes|4|
|Support for conditional instruction tracing|Not implemented|
|Support for tracing of data|Not implemented|
|Support for tracing of load and store instructions as P0 elements|Not implemented|
|Support for cycle counting in the instruction trace|Implemented|
|Support for branch broadcast tracing|Implemented|
|Number of events supported in the trace|4|
|Return stack support|Implemented|
|Tracing of SError exception support|Implemented|
|Instruction trace cycle counting minimum threshold|4|
|Size of Trace ID|7 bits|
|Synchronization period support|read/write|
|Global timestamp size|64 bits|
|Number of cores available for tracing|1|
|ATB trigger support|Implemented|
|Low power behavior override|Not implemented|

## 20.3 ETM trace unit functional description

The following table shows the resources that are implemented in the Neoverse™ N1 ETM trace
unit.

Table 20-2: ETM trace unit resources implemented

This section describes the functionality of the ETM trace unit.

The following ﬁgure shows the main functional blocks of the ETM trace unit.

|Description|Configuration|
|---|---|
|Stall control support|Not implemented|
|Support for overﬂow avoidance|Not implemented|
|Support for using CONTEXTIDR_EL2 in VMID comparator|Implemented|

|Description|Configuration|
|---|---|
|Number of resource selection pairs implemented|8|
|Number of external input selectors implemented|4|
|Number of external inputs implemented|173, 4 CTI + 169 PMU|
|Number of counters implemented|2|
|Reduced function counter implemented|Not implemented|
|Number of sequencer states implemented|4|
|Number of Virtual Machine ID comparators implemented|1|
|Number of Context ID comparators implemented|1|
|Number of address comparator pairs implemented|4|
|Number of single-shot comparator controls|1|
|Number of core comparator inputs implemented|0|
|Data address comparisons implemented|Not implemented|
|Number of data value comparators implemented|0|

Figure 20-1: ETM functional blocks

ETM

CORECLK

ATB

Trace out

FIFO
Debug APB
Filtering and triggering resources

block
Core
interface

Trace generation
Core interface

Core interface

This block monitors the behavior of the core and generates P0 elements that are essentially
executed branches and exceptions traced in program order.

Trace generation

The trace generation block generates various trace packets based on P0 elements.

Filtering and triggering resources

You can limit the amount of trace data generated by the ETM through the process of ﬁltering.

For example, generating trace only in a certain address range. More complicated logic
analyzer style ﬁltering options are also available.

The ETM trace unit can also generate a trigger that is a signal to the Trace Capture Device to
stop capturing trace.

FIFO

The trace generated by the ETM trace unit is in a highly-compressed form.

The FIFO enables trace bursts to be ﬂattened out. When the FIFO becomes full, the FIFO
signals an overﬂow. The trace generation logic does not generate any new trace until the
FIFO is emptied. This causes a gap in the trace when viewed in the debugger.

Trace out

Trace from FIFO is output on the AMBA ATB interface.

## 20.4 Resetting the ETM

## 20.5 Programming and reading ETM trace unit registers

20.4 Resetting the ETM

The reset for the ETM trace unit is the same as a Cold reset for the core.

The ETM trace unit is not reset when Warm reset is applied to the core so that tracing through
Warm core reset is possible.

If the ETM trace unit is reset, tracing stops until the ETM trace unit is reprogrammed and re-
enabled. However, if the core is reset using Warm reset, the last few instructions that are provided
by the core before the reset might not be traced.

You program and read the ETM trace unit registers using the Debug APB interface.

The core does not have to be in debug state when you program the ETM trace unit registers.

When you are programming the ETM trace unit registers, you must enable all the changes at the
same time. Otherwise, if you program the counter, it might start to count based on incorrect events
before the correct setup is in place for the trigger condition.

To disable the ETM trace unit, use the TRCPRGCTLR.EN bit.

## 20.6 ETM trace unit register interfaces

Figure 20-2: Programming ETM trace unit registers

Start

Set main enable bit in
TRCPRGCTLR to 0b0

Read TRCSTATR

Is TRCSTATR  Idle

0b1?

Program all trace
registers required

Set main enable bit in
TRCPRGCTLR to 0b1

Read TRCSTATR

Is TRCSTATR  Idle

0b0?

End

The Neoverse™ N1 core supports only memory-mapped interface to trace registers.

See the  Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4  for information on the
behaviors on register accesses for diﬀerent trace unit states and the diﬀerent access mechanisms.

Related information
External debug interface on page 316

|N|o|
|---|---|

|Y|es|
|---|---|

|N|o|
|---|---|

|Y|es|
|---|---|

## 20.7 Interaction with the PMU and Debug

This section describes the interaction with the PMU and the eﬀect of debug double lock on trace
register access.

Interaction with the PMU

The Neoverse™ N1 core includes a PMU that enables events, such as cache misses and instructions
executed, to be counted over a period of time.

The PMU and ETM trace unit function together.

Use of PMU events by the ETM trace unit
The PMU architectural events described in 18.3 PMU events  on page 318 are available to the
ETM trace unit through the extended input facility.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information about
PMU events.

The ETM trace unit uses four extended external input selectors to access the PMU events. Each
selector can independently select one of the PMU events, that are then active for the cycles
where the relevant events occur. These selected events can then be accessed by any of the event
registers within the ETM trace unit. The PMU event table describes the PMU events.

Related information
PMU events on page 318

# 21. Statistical Profiling Extension

## 21.1 About the Statistical Profiling Extension

## 21.2 SPE functional description

21. Statistical Proﬁling Extension

This chapter describes the Statistical Proﬁling Extension (SPE) for the Neoverse™ N1 core.

21.1 About the Statistical Proﬁling Extension

The Neoverse™ N1 core supports the Statistical Proﬁling Extension (SPE), which was introduced
in Armv8.2. SPE provides a statistical view of the performance characteristics of executed
instructions, which can be used by software writers to optimize their code for better performance.

This statistical view is provided by periodically capturing proﬁles of the characteristics of micro-
operations as they are executed on the Neoverse™ N1 core, and writing those proﬁles to memory
after the corresponding instruction has retired.

This section describes the functionality of the SPE.

At a high level, SPE behavior consists of:

- Selection of the micro-operation to be proﬁled.

- Marking the selected micro-operation throughout its lifetime in the core, indicating within the
various units that it is to be proﬁled.

- Storing data about the proﬁled micro-operation in internal registers during its lifetime in the
core.

- Following retire/abort/ﬂush of the proﬁled instruction, recording the proﬁle data to memory.

While the SPE architecture allows either instructions or micro-operations to be proﬁled, the core
will proﬁle micro-operations in order to minimize the amount of logic necessary to support SPE.

Proﬁles are collected periodically, with the selection of a micro-operation to be proﬁled being
driven by a simple down-counter which counts the number of speculative micro-operations
dispatched, decremented once for each micro-operation. When the counter reaches zero, a
micro-operation is identiﬁed as being sampled and is proﬁled throughout its lifetime in the
microarchitecture.

The proﬁling activity is expected to be largely non-intrusive to the core performance, meaning the
core's performance should not be meaningfully perturbed while proﬁling is taking place. Permitted
perturbation includes using LS/L2 bandwidth to record the proﬁle data to memory.

## 21.3 IMPLEMENTATION DEFINED features of SPE

The rate of occurrence of this activity depends on the sampling rate, which is
user-speciﬁed, so it may be possible for the user to specify a sampling rate that is
meaningfully intrusive to the core's performance.

- The core's recommended minimum sampling interval is once per 1024 uops.

- This value is also communicated to software via the PMSIDR_EL1 interval bits.

Unlike trace information, SPE proﬁles are written to memory using a Virtual Address (VA), which
means that writes of proﬁles must have access to the MMU in order to translate a VA to a Physical
Address (PA), and must have a means to be written to memory.

This section describes the IMPLEMENTATION DEFINED features of SPE.

Events deﬁnition

The Neoverse™ N1 core includes a 16-bit event packet which is deﬁned in the following table.

Data source packet

The Neoverse™ N1 core provides an 8-bit data source for load and store operations as deﬁned in
the following table. All other values are reserved.

|Bit|Definition|
|---|---|
|15|Reserved|
|14|Reserved|
|13|Reserved|
|12|Late prefetch|
|11|Reserved|
|10|Remote access|
|9|Last level cache miss|
|8|Last level cache access|
|7|Branch mispredicted|
|6|Not taken|
|5|DTLB walk|
|4|TLB access|
|3|L1 data cache reﬁll|
|2|L1 data cache access|
|1|Architecturally retired|
|0|Generated exception|

|Value|Name|
|---|---|
|`0b0000`|L1 data cache|

|Value|Name|
|---|---|
|`0b1000`|L2 cache|
|`0b1001`|Peer CPU|
|`0b1010`|Local cluster|
|`0b1011`|System cache|
|`0b1100`|Peer cluster|
|`0b1101`|Remote|
|`0b1110`|DRAM|

# 22. AArch32 debug registers

## 22.1 AArch32 Debug register summary

This chapter describes the Debug registers in the AArch32 Execution state and shows examples of
how to use them.

The following table summarizes the 32-bit and 64-bit debug control registers that are accessible in
the AArch32 Execution state from the internal CP14 interface. These registers are accessed by the
MCR and MRC instructions in the order of CRn, op2, CRm, Op1, or MCRR and MRRC instructions in the
order of CRm, Op1.

For those registers that are not described in this chapter, see the Arm® Architecture Reference
Manual Arm®v8, for Arm®v8-A architecture proﬁle.

Table 22-1: AArch32 Debug register summary

|CRn|Op2|CRm|Op1|Name|Type|Reset|Description|
|---|---|---|---|---|---|---|---|
|c0|0|c1|0|DBGDSCRint|RO|`000x0000`|Debug Status and Control Register, Internal View|
|c0|0|c5|0|DBGDTRTXint|WO|-|Debug Data Transfer Register, Transmit, Internal View|
|c0|0|c5|0|DBGDTRRXint|RO|`0x00000000`|Debug Data Transfer Register, Receive, Internal View|

# 23. AArch64 Debug registers

## 23.1 AArch64 Debug register summary

This chapter describes the Debug registers in the AArch64 Execution state and shows examples of
how to use them.

These registers, listed in the following table, are accessed by the MRS and MSR instructions in the
order of Op0, CRn, Op1, CRm, Op2.

See 24.1 Memory-mapped Debug register summary on page 348 for a complete list of registers
accessible from the external debug interface. The 64-bit registers cover two addresses on the
external memory interface. For those registers that are not described in this chapter, see the Arm®
Architecture Reference Manual for A-proﬁle architecture.

Table 23-1: AArch64 debug register summary

|Name|Type|Reset|Width|Description|
|---|---|---|---|---|
|OSDTRRX_EL1|RW|`0x00000000`|32|Debug Data Transfer Register, Receive, External View|
|DBGBVR0_EL1|RW|-|64|Debug Breakpoint Value Register 0|
|DBGBCR0_EL1|RW|UNK|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|DBGWVR0_EL1|RW|-|64|Debug Watchpoint Value Register 0|
|DBGWCR0_EL1|RW|UNK|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|DBGBVR1_EL1|RW|-|64|Debug Breakpoint Value Register 1|
|DBGBCR1_EL1|RW|UNK|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|DBGWVR1_EL1|RW|-|64|Debug Watchpoint Value Register 1|
|DBGWCR1_EL1|RW|UNK|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|MDCCINT_EL1|RW|`0x00000000`|32|Monitor Debug Comms Channel Interrupt Enable Register|
|MDSCR_EL1|RW|-|32|Monitor Debug System Control Register, EL1|
|DBGBVR2_EL1|RW|-|64|Debug Breakpoint Value Register 2|
|DBGBCR2_EL1|RW|UNK|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|DBGWVR2_EL1|RW|-|64|Debug Watchpoint Value Register 2|
|DBGWCR2_EL1|RW|UNK|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|OSDTRTX_EL1|RW|-|32|Debug Data Transfer Register, Transmit, External View|
|DBGBVR3_EL1|RW|-|64|Debug Breakpoint Value Register 3|
|DBGBCR3_EL1|RW|UNK|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|DBGWVR3_EL1|RW|-|64|Debug Watchpoint Value Register 3|
|DBGWCR3_EL1|RW|UNK|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|DBGBVR4_EL1|RW|-|64|Debug Breakpoint Value Register 4|
|DBGBCR4_EL1|RW|UNK|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|DBGBVR5_EL1|RW|-|64|Debug Breakpoint Value Register 5|

## 23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1

23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers,
EL1

The DBGBCRn_EL1 registers hold control information for a breakpoint. Each DBGBVRn_EL1
is associated with a DBGBCRn_EL1 to form a Breakpoint Register Pair (BRP). The range of n for
DBGBCRn_EL1 registers is 0 to 5.

Bit ﬁeld descriptions
The DBGBCRn_EL1 registers are 32-bit registers.

Figure 23-1: DBGBCRn_EL1 bit assignments

31
24 23
20 19
16 15
13
9
8
5
4
3
2
1
0

14

12

BT
BAS
PMC E

LBN
SSC

HMC

res0

RES0, [31:24]

RES0

Reserved.

|Name|Type|Reset|Width|Description|
|---|---|---|---|---|
|DBGBCR5_EL1|RW|UNK|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|OSECCR_EL1|RW|`0x00000000`|32|Debug OS Lock Exception Catch Register|
|MDCCSR_EL0|RO|`0x00000000`|32|Monitor Debug Comms Channel Status Register|
|DBGDTR_EL0|RW|`0x00000000`|64|Debug Data Transfer Register, half-duplex|
|DBGDTRTX_EL0|WO|`0x00000000`|32|Debug Data Transfer Register, Transmit, Internal View|
|DBGDTRRX_EL0|RO|`0x00000000`|32|Debug Data Transfer Register, Receive, Internal View|
|MDRAR_EL1|RO|-|64|Debug ROM Address Register. This register is reserved,_RES0_|
|OSLAR_EL1|WO|-|32|Debug OS Lock Access Register|
|OSLSR_EL1|RO|`0x0000000A`|32|Debug OS Lock Status Register|
|OSDLR_EL1|RW|`0x00000000`|32|Debug OS Double Lock Register|
|DBGPRCR_EL1|RW|-|32|Debug Power/Reset Control Register|
|DBGCLAIMSET_EL1|RW|`0x000000FF`|32|23.3 DBGCLAIMSET_EL1, Debug Claim Tag Set Register, EL1 on page 343|
|DBGCLAIMCLR_EL1|RW|`0x00000000`|32|Debug Claim Tag Clear Register|
|DBGAUTHSTATUS_EL1|RO|`0x000000AA`|32|Debug Authentication Status Register|

BT, [23:20]

Breakpoint Type. This ﬁeld controls the behavior of Breakpoint debug event generation. This
includes the meaning of the value held in the associated DBGBVRn_EL1, indicating whether
it is an instruction address match or mismatch, or a Context match. It also controls whether
the breakpoint is linked to another breakpoint. The possible values are:

0b0000
Unlinked instruction address match.

0b0001
Linked instruction address match.

0b0010
Unlinked Context ID match.

0b0011
Linked Context ID match.

0b0100
Unlinked instruction address mismatch.

0b0101
Linked instruction address mismatch.

0b0110
Unlinked CONTEXTIDR_EL1 match.

0b0111
Linked CONTEXTIDR_EL1 match.

0b1000
Unlinked VMID match.

0b1001
Linked VMID match.

0b1010
Unlinked VMID + Conext ID match.

0b1011
Linked VMID + Context ID match.

0b1100
Unlinked CONTEXTIDR_EL2 match.

0b1101
Linked CONTEXTIDR_EL2 match.

0b1110
Unlinked Full Context ID match.

0b1111
Linked Full Context ID match.

The ﬁeld breakdown is:

- BT[3,1]: Base type. If the breakpoint is not context-aware, these bits are RES0. Otherwise,
the possible values are:

0b000
Match address. DBGBVRn_EL1 is the address of an
instruction.
0b001
Match context ID. DBGBVRn_EL1[31:0] is a context ID.
0b010
Match VMID. DBGBVRn_EL1[47:32] is a VMID.
0b011
Match VMID and CONTEXTIDR_EL1. DBGBVRn_EL1[31:0]
is a context ID, and DBGBVRn_EL1[47:32] is a VMID.

- BT[2]: Mismatch. RES0.

- BT[0]: Enable linking.

LBN, [19:16]

Linked breakpoint number. For Linked address matching breakpoints, this speciﬁes the index
of the context-matching breakpoint linked to.

SSC, [15:14]

Security State Control. Determines the Security states under which a breakpoint debug event
for breakpoint n is generated.

This ﬁeld must be interpreted with the Higher Mode Control (HMC), and Privileged Mode
Control (PMC), ﬁelds to determine the mode and Security states that can be tested.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for possible values of the
HMC and PMC ﬁelds.

HMC, [13]

Hyp Mode Control bit. Determines the debug perspective for deciding when a breakpoint
debug event for breakpoint n is generated.

This bit must be interpreted with the SSC and PMC ﬁelds to determine the mode and
Security states that can be tested.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for possible values of the
SSC and PMC ﬁelds.

RES0, [12:9]

RES0

Reserved.

BAS, [8:5]

Byte Address Select. Deﬁnes which halfwords a regular breakpoint matches, regardless of the
instruction set and Execution state. A debugger must program this ﬁeld as follows:

0x3
Match the T32 instruction at DBGBVRn_EL1.

0xC
Match the T32 instruction at DBGBVRn_EL1+2.

0xF
Match the A64 or A32 instruction at DBGBVRn_EL1, or context match.

All other values are reserved.

The Arm®v8‑A architecture does not support direct execution of Java bytecodes. BAS[3] and
BAS[1] ignore writes and on reads return the values of BAS[2] and BAS[0] respectively.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for more information on
how the BAS ﬁeld is interpreted by hardware.

RES0, [4:3]

RES0

Reserved.

PMC, [2:1]

Privileged Mode Control. Determines the Exception level or levels that a breakpoint debug
event for breakpoint n is generated.

This ﬁeld must be interpreted with the SSC and HMC ﬁelds to determine the mode and
Security states that can be tested.

See the Arm® Architecture Reference Manual for A-proﬁle architecture for possible values of the
SSC and HMC ﬁelds.

Bits[2:1] have no eﬀect for accesses made in Hyp mode.

E, [0]

Enable breakpoint. This bit enables the BRP:

0
BRP disabled.

1
BRP enabled.

A BRP never generates a breakpoint debug event when it is disabled.

The value of DBGBCRn_EL1.E is UNKNOWN on reset. A debugger must ensure that
DBGBCRn_EL1.E has a deﬁned value before it enables debug.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

## 23.3 DBGCLAIMSET_EL1, Debug Claim Tag Set Register, EL1

23.3 DBGCLAIMSET_EL1, Debug Claim Tag Set Register,
EL1

The DBGCLAIMSET_EL1 is used by software to set CLAIM bits to 1.

Bit ﬁeld descriptions
The DBGCLAIMSET_EL1 is a 32-bit register.

Figure 23-2: DBGCLAIMSET_EL1 bit assignments

31
8
7
0

CLAIM

res0

RES0, [31:8]

RES0

Reserved.

CLAIM, [7:0]

Claim set bits.

Writing a 1 to one of these bits sets the corresponding CLAIM bit to 1. This is an indirect
write to the CLAIM bits.

A single write operation can set multiple bits to 1. Writing 0 to one of these bits has no
eﬀect.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

## 23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1

23.4 DBGWCRn_EL1, Debug Watchpoint Control
Registers, EL1

The DBGWCRn_EL1 registers hold control information for a watchpoint. Each DBGWCRn_EL1
is associated with a DBGWVRn_EL1 to form a Watchpoint Register Pair (WRP). The range of n for
DBGWCRn_EL1 registers is 0 to 3.

Bit ﬁeld descriptions
The DBGWCRn_EL1 registers are 32-bit registers.

Figure 23-3: DBGWCRn_EL1 bit assignments

31
29 28
24 23
21 20 19
16 15 14 13 12
5
4
3
2
1
0

SSC
LSC
MASK
LBN

BAS

PAC
E

WT

HMC

res0

RES0, [31:29]

RES0

Reserved.

MASK, [28:24]

Address mask. Only objects up to 2GB can be watched using a single mask.

0b00000
No mask.
0b00001
Reserved.
0b00010
Reserved.

Other values mask the corresponding number of address bits, from 0b00011 masking
3 address bits (0x00000007 mask for address) to 0b11111 masking 31 address bits
(0x7FFFFFFF mask for address).

RES0, [23:21]

RES0

Reserved.

WT, [20]

Watchpoint type. Possible values are:

0b0
Unlinked data address match.

0b1
Linked data address match.

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

LBN, [19:16]

Linked breakpoint number. For Linked data address watchpoints, this speciﬁes the index of
the Context-matching breakpoint linked to.

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

SSC, [15:14]

Security state control. Determines the Security states under which a watchpoint debug event
for watchpoint n is generated. This ﬁeld must be interpreted along with the HMC and PAC
ﬁelds.

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

HMC, [13]

Higher mode control. Determines the debug perspective for deciding when a watchpoint
debug event for watchpoint n is generated. This ﬁeld must be interpreted along with the SSC
and PAC ﬁelds.

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

BAS, [12:5]

Byte address select. Each bit of this ﬁeld selects whether a byte from within the word or
double-word addressed by DBGWVRn_EL1 is being watched. See the Arm® Architecture
Reference Manual for A-proﬁle architecture for more information.

LSC, [4:3]

Load/store access control. This ﬁeld enables watchpoint matching on the type of access
being made. The possible values are:

0b01
Match instructions that load from a watchpoint address.

0b10
Match instructions that store to a watchpoint address.

0b11
Match instructions that load from or store to a watchpoint address.

All other values are reserved, but must behave as if the watchpoint is disabled. Software
must not rely on this property because the behavior of reserved values might change in a
future revision of the architecture.

IGNORED

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

PAC, [2:1]

Privilege of access control. Determines the Exception level or levels at which a watchpoint
debug event for watchpoint n is generated. This ﬁeld must be interpreted along with the SSC
and HMCﬁelds.

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

E, [0]

Enable watchpoint n. Possible values are:

0b0
Watchpoint disabled.

0b1
Watchpoint enabled.

On Cold reset, the ﬁeld reset value is architecturally UNKNOWN.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

# 24. Memory-mapped Debug registers

## 24.1 Memory-mapped Debug register summary

This chapter describes the memory-mapped Debug registers and shows examples of how to use
them.

The following table shows the oﬀset address for the registers that are accessible from the external
debug interface.

For those registers that are not described in this chapter, see the Arm® Architecture Reference
Manual for A-proﬁle architecture.

Table 24-1: Memory-mapped debug register summary

|Offset|Name|Type|Width|Description|
|---|---|---|---|---|
|`0x000-0x01C`|-|-|-|Reserved|
|`0x020`|EDESR|RW|32|External Debug Event Status Register|
|`0x024`|EDECR|RW|32|External Debug Execution Control Register|
|`0x028-0x02C`|-|-|-|Reserved|
|`0x030`|EDWAR[31:0]|RO|64|External Debug Watchpoint Address Register|
|`0x034`|EDWAR[63:32]|EDWAR[63:32]|EDWAR[63:32]|EDWAR[63:32]|
|`0x038-0x07C`|-|-|-|Reserved|
|`0x080`|DBGDTRRX_EL0|RW|32|Debug Data Transfer Register, Receive|
|`0x084`|EDITR|WO|32|External Debug Instruction Transfer Register|
|`0x088`|EDSCR|RW|32|External Debug Status and Control Register|
|`0x08C`|DBGDTRTX_EL0|WO|32|Debug Data Transfer Register, Transmit|
|`0x090`|EDRCR|WO|32|24.14 EDRCR, External Debug Reserve Control Register on page 361|
|`0x094`|-|RW|32|Reserved|
|`0x098`|EDECCR|RW|32|External Debug Exception Catch Control Register|
|`0x09C`|-|-|-|Reserved|
|`0x0A0`|-|-|-|Reserved|
|`0x0A4`|-|-|-|Reserved|
|`0x0A8`|-|-|-|Reserved|
|`0x0AC`|-|-|-|Reserved|
|`0x0B0-0x2FC`|-|-|-|Reserved|
|`0x300`|OSLAR_EL1|WO|32|OS Lock Access Register|
|`0x304-0x30C`|-|-|-|Reserved|
|`0x310`|EDPRCR|RW|32|External Debug Power/Reset Control Register|
|`0x314`|EDPRSR|RO|32|External Debug Processor Status Register|
|`0x318-0x3FC`|-|-|-|Reserved|

|Offset|Name|Type|Width|Description|
|---|---|---|---|---|
|`0x400`|DBGBVR0_EL1[31:0]|RW|64|Debug Breakpoint Value Register 0|
|`0x404`|DBGBVR0_EL1[63:32]|DBGBVR0_EL1[63:32]|DBGBVR0_EL1[63:32]|DBGBVR0_EL1[63:32]|
|`0x408`|DBGBCR0_EL1|RW|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|`0x40C`|-|-|-|Reserved|
|`0x410`|DBGBVR1_EL1[31:0]|RW|64|Debug Breakpoint Value Register 1|
|`0x414`|DBGBVR1_EL1[63:32]|DBGBVR1_EL1[63:32]|DBGBVR1_EL1[63:32]|DBGBVR1_EL1[63:32]|
|`0x418`|DBGBCR1_EL1|RW|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|`0x41C`|-|-|-|Reserved|
|`0x420`|DBGBVR2_EL1[31:0]|RW|64|Debug Breakpoint Value Register 2|
|`0x424`|DBGBVR2_EL1[63:32]|DBGBVR2_EL1[63:32]|DBGBVR2_EL1[63:32]|DBGBVR2_EL1[63:32]|
|`0x428`|DBGBCR2_EL1|RW|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|`0x42C`|-|-|-|Reserved|
|`0x430`|DBGBVR3_EL1[31:0]|RW|64|Debug Breakpoint Value Register 3|
|`0x434`|DBGBVR3_EL1[63:32]|DBGBVR3_EL1[63:32]|DBGBVR3_EL1[63:32]|DBGBVR3_EL1[63:32]|
|`0x438`|DBGBCR3_EL1|RW|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|`0x43C`|-|-|-|Reserved|
|`0x440`|DBGBVR4_EL1[31:0]|RW|64|Debug Breakpoint Value Register 4|
|`0x444`|DBGBVR4_EL1[63:32]|DBGBVR4_EL1[63:32]|DBGBVR4_EL1[63:32]|DBGBVR4_EL1[63:32]|
|`0x448`|DBGBCR4_EL1|RW|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|`0x44C`|-|-|-|Reserved|
|`0x450`|DBGBVR5_EL1[31:0]|RW|64|Debug Breakpoint Value Register 5|
|`0x454`|DBGBVR5_EL1[63:32]|RW|64|Debug Breakpoint Value Register 5|
|`0x458`|DBGBCR5_EL1|RW|32|23.2 DBGBCRn_EL1, Debug Breakpoint Control Registers, EL1 on page 340|
|`0x45C-0x7FC`|-|-|-|Reserved|
|`0x800`|DBGWVR0_EL1[31:0]|RW|64|Debug Watchpoint Value Register 0|
|`0x804`|DBGWVR0_EL1[63:32]|DBGWVR0_EL1[63:32]|DBGWVR0_EL1[63:32]|DBGWVR0_EL1[63:32]|
|`0x808`|DBGWCR0_EL1|RW|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|`0x80C`|-|-|-|Reserved|
|`0x810`|DBGWVR1_EL1[31:0]|RW|64|Debug Watchpoint Value Register 1|
|`0x814`|DBGWVR1_EL1[63:32]|DBGWVR1_EL1[63:32]|DBGWVR1_EL1[63:32]|DBGWVR1_EL1[63:32]|
|`0x818`|DBGWCR1_EL1|RW|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|`0x81C`|-|-|-|Reserved|
|`0x820`|DBGWVR2_EL1[31:0]|RW|64|Debug Watchpoint Value Register 2|
|`0x824`|DBGWVR2_EL1[63:32]|DBGWVR2_EL1[63:32]|DBGWVR2_EL1[63:32]|DBGWVR2_EL1[63:32]|
|`0x828`|DBGWCR2_EL1|RW|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|
|`0x82C`|-|-|-|Reserved|
|`0x830`|DBGWVR3_EL1[31:0]|RW|64|Debug Watchpoint Value Register 0,|
|`0x834`|DBGWVR3_EL1[63:32]|DBGWVR3_EL1[63:32]|DBGWVR3_EL1[63:32]|DBGWVR3_EL1[63:32]|
|`0x838`|DBGWCR3_EL1|RW|32|23.4 DBGWCRn_EL1, Debug Watchpoint Control Registers, EL1 on page 344|

|Offset|Name|Type|Width|Description|
|---|---|---|---|---|
|`0x83C-0xCFC`|-|-|-|Reserved|
|`0xD00`|MIDR|RO|32|13.90 MIDR_EL1, Main ID Register, EL1 on page 230|
|`0xD04-0xD1C`|-|-|-|Reserved|
|`0xD20`|EDPFR[31:0]|RO|64|13.67 ID_AA64PFR0_EL1, AArch64 Processor Feature Register 0, EL1 on page<br>190|
|`0xD24`|EDPFR[63:32]|EDPFR[63:32]|EDPFR[63:32]|EDPFR[63:32]|
|`0xD28`|EDDFR[31:0]|RO|64|13.67 ID_AA64PFR0_EL1, AArch64 Processor Feature Register 0, EL1 on page<br>190|
|`0xD2C`|EDDFR[63:32]|EDDFR[63:32]|EDDFR[63:32]|EDDFR[63:32]|
|`0xD60-0xEFC`|-|-|-|Reserved|
|`0xF00`|-|-|-|Reserved|
|`0xF04-0xF9C`|-|-|-|Reserved|
|`0xFA0`|DBGCLAIMSET_EL1|RW|32|23.3 DBGCLAIMSET_EL1, Debug Claim Tag Set Register, EL1 on page 343|
|`0xFA4`|DBGCLAIMCLR_EL1|RW|32|Debug Claim Tag Clear Register|
|`0xFA8`|EDDEVAFF0|RO|32|External Debug Device Aﬃnity Register 0|
|`0xFAC`|EDDEVAFF1|RO|32|External Debug Device Aﬃnity Register 1|
|`0xFB0`|-|-|-|Reserved|
|`0xFB4`|-|-|-|Reserved|
|`0xFB8`|DBGAUTHSTATUS_EL1|RO|32|Debug Authentication Status Register|
|`0xFBC`|EDDEVARCH|RO|32|External Debug Device Architecture Register|
|`0xFC0`|EDDEVID2|RO|32|External Debug Device ID Register 2,_res0_|
|`0xFC4`|EDDEVID1|RO|32|24.7 EDDEVID1, External Debug Device ID Register 1 on page 355|
|`0xFC8`|EDDEVID|RO|32|24.6 EDDEVID, External Debug Device ID Register 0 on page 354|
|`0xFCC`|EDDEVTYPE|RO|32|External Debug Device Type Register|
|`0xFD0`|EDPIDR4|RO|32|24.12 EDPIDR4, External Debug Peripheral Identiﬁcation Register 4 on page<br>359|
|`0xFD4-0xFDC`|EDPIDR5-7|RO|32|24.13 EDPIDRn, External Debug Peripheral Identiﬁcation Registers 5-7 on page<br>360|
|`0xFE0`|EDPIDR0|RO|32|24.8 EDPIDR0, External Debug Peripheral Identiﬁcation Register 0 on page 356|
|`0xFE4`|EDPIDR1|RO|32|24.9 EDPIDR1, External Debug Peripheral Identiﬁcation Register 1 on page 357|
|`0xFE8`|EDPIDR2|RO|32|24.10 EDPIDR2, External Debug Peripheral Identiﬁcation Register 2 on page<br>358|
|`0xFEC`|EDPIDR3|RO|32|24.11 EDPIDR3, External Debug Peripheral Identiﬁcation Register 3 on page<br>358|
|`0xFF0`|EDCIDR0|RO|32|24.2 EDCIDR0, External Debug Component Identiﬁcation Register 0 on page<br>350|
|`0xFF4`|EDCIDR1|RO|32|24.3 EDCIDR1, External Debug Component Identiﬁcation Register 1 on page<br>351|
|`0xFF8`|EDCIDR2|RO|32|24.4 EDCIDR2, External Debug Component Identiﬁcation Register 2 on page<br>352|
|`0xFFC`|EDCIDR3|RO|32|24.5 EDCIDR3, External Debug Component Identiﬁcation Register 3 on page<br>353|

## 24.2 EDCIDR0, External Debug Component Identification Register 0

## 24.3 EDCIDR1, External Debug Component Identification Register 1

24.2 EDCIDR0, External Debug Component Identiﬁcation
Register 0

The EDCIDR0 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDCIDR0 is a 32-bit register.

Figure 24-1: EDCIDR0 bit assignments

7
8

31
0

PRMBL_0

res0

RES0, [31:8]

RES0

Reserved.

PRMBL_0, [7:0]

0x0D
Preamble byte 0.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDCIDR0 can be accessed through the external debug interface, oﬀset 0xFF0.

24.3 EDCIDR1, External Debug Component Identiﬁcation
Register 1

The EDCIDR1 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDCIDR1 is a 32-bit register.

## 24.4 EDCIDR2, External Debug Component Identification Register 2

Figure 24-2: EDCIDR1 bit assignments

7
8
3
4

31
0

CLASS

PRMBL_1

res0

RES0, [31:8]

RES0

Reserved.

CLASS, [7:4]

0x9
Debug component.

PRMBL_1, [3:0]

0x0
Preamble.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDCIDR1 can be accessed through the external debug interface, oﬀset 0xFF4.

24.4 EDCIDR2, External Debug Component Identiﬁcation
Register 2

The EDCIDR2 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDCIDR2 is a 32-bit register.

## 24.5 EDCIDR3, External Debug Component Identification Register 3

Figure 24-3: EDCIDR2 bit assignments

7
8

31
0

PRMBL_2

res0

RES0, [31:8]

RES0

Reserved.

PRMBL_2, [7:0]

0x05
Preamble byte 2.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDCIDR2 can be accessed through the external debug interface, oﬀset 0xFF8.

24.5 EDCIDR3, External Debug Component Identiﬁcation
Register 3

The EDCIDR3 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDCIDR3 is a 32-bit register.

## 24.6 EDDEVID, External Debug Device ID Register 0

Figure 24-4: EDCIDR3 bit assignments

7
8

31
0

PRMBL_3

res0

RES0, [31:8]

RES0

Reserved.

PRMBL_3, [7:0]

0xB1
Preamble byte 3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDCIDR3 can be accessed through the external debug interface, oﬀset 0xFFC.

The EDDEVID provides extra information for external debuggers about features of the debug
implementation.

Bit ﬁeld descriptions
The EDDEVID is a 32-bit register.

## 24.7 EDDEVID1, External Debug Device ID Register 1

Figure 24-5: EDDEVID bit assignments

31
0
28 27
24 23

AuxRegs

res0

RES0, [31:28]

RES0

Reserved.

AuxRegs, [27:24]

Indicates support for Auxiliary registers:

0x0
None supported.

RES0, [23:0]

RES0

Reserved.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDDEVID can be accessed through the external debug interface, oﬀset 0xFC8.

The EDDEVID1 provides extra information for external debuggers about features of the debug
implementation.

Bit ﬁeld descriptions
The EDDEVID1 is a 32-bit register.

## 24.8 EDPIDR0, External Debug Peripheral Identification Register 0

Figure 24-6: EDDEVID1 bit assignments

31
0

res0

RES0, [31:0]

RES0

Reserved.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDDEVID1 can be accessed through the external debug interface, oﬀset 0xFC4.

24.8 EDPIDR0, External Debug Peripheral Identiﬁcation
Register 0

The EDPIDR0 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDPIDR0 is a 32-bit register.

Figure 24-7: EDPIDR0 bit assignments

31
0
7
8

Part_0

res0

## 24.9 EDPIDR1, External Debug Peripheral Identification Register 1

RES0, [31:8]

RES0

Reserved.

Part_0, [7:0]

0x0C
Least signiﬁcant byte of the debug part number.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDPIDR0 can be accessed through the external debug interface, oﬀset 0xFE0.

24.9 EDPIDR1, External Debug Peripheral Identiﬁcation
Register 1

The EDPIDR1 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDPIDR1 is a 32-bit register.

Figure 24-8: EDPIDR1 bit assignments

31
0
3
4

7
8

DES_0

Part_1

res0

RES0, [31:8]

RES0

Reserved.

DES_0, [7:4]

0xB
Arm Limited. This is the least signiﬁcant nibble of JEP106 ID code.

Part_1, [3:0]

0xD
Most signiﬁcant nibble of the debug part number.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

## 24.10 EDPIDR2, External Debug Peripheral Identification Register 2

The EDPIDR1 can be accessed through the external debug interface, oﬀset 0xFE4.

24.10 EDPIDR2, External Debug Peripheral Identiﬁcation
Register 2

The EDPIDR2 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDPIDR2 is a 32-bit register.

Figure 24-9: EDPIDR2 bit assignments

31
0
3
4

7
8

2

Revision

DES_1

JEDEC

RES0

RES0, [31:8]

RES0
Reserved.

Revision, [7:4]

0x5
r4p1.

JEDEC, [3]

0b1
RAO. Indicates a JEP106 identity code is used.

DES_1, [2:0]

0b011
Arm Limited. This is the most signiﬁcant nibble of JEP106 ID code.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDPIDR2 can be accessed through the external debug interface, oﬀset 0xFE8.

## 24.11 EDPIDR3, External Debug Peripheral Identification Register 3

24.11 EDPIDR3, External Debug Peripheral Identiﬁcation
Register 3

The EDPIDR3 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDPIDR3 is a 32-bit register.

Figure 24-10: EDPIDR3 bit assignments

31
0
3
4

7
8

REVAND

CMOD

res0

RES0, [31:8]

RES0

Reserved.

REVAND, [7:4]

0x0
Part minor revision.

CMOD, [3:0]

0x0
Customer modiﬁed.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDPIDR3 can be accessed through the external debug interface, oﬀset 0xFEC.

## 24.12 EDPIDR4, External Debug Peripheral Identification Register 4

24.12 EDPIDR4, External Debug Peripheral Identiﬁcation
Register 4

The EDPIDR4 provides information to identify an external debug component.

Bit ﬁeld descriptions
The EDPIDR4 is a 32-bit register.

Figure 24-11: EDPIDR4 bit assignments

31
0
3
4

7
8

SIZE

DES_2

res0

RES0, [31:8]

RES0

Reserved.

SIZE, [7:4]

0x0
Size of the component. Log2 the number of 4KB pages from the start of the
component to the end of the component ID registers.

DES_2, [3:0]

0x4
Arm Limited This is the least signiﬁcant nibble JEP106 continuation code.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The EDPIDR4 can be accessed through the external debug interface, oﬀset 0xFD0.

## 24.13 EDPIDRn, External Debug Peripheral Identification Registers 5-7

## 24.14 EDRCR, External Debug Reserve Control Register

24.13 EDPIDRn, External Debug Peripheral Identiﬁcation
Registers 5-7

No information is held in the Peripheral ID5, Peripheral ID6, and Peripheral ID7 Registers.

They are reserved for future use and are RES0.

The EDRCR is part of the Debug registers functional group.

Bit ﬁeld descriptions

Figure 24-12: EDRCR bit assignments

31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9
8
7
6
5
4
3
2
1
0

CSPA

CSE
RES0

RES0, [31:4]

RES0

Reserved.

CSPA, [3]

Clear Sticky Pipeline Advance. This bit is used to clear the EDSCR.PipeAdv bit to 0. The
actions on writing to this bit are:

0
No action.

1
Clear the EDSCR.PipeAdv bit to 0.

CSE, [2]

Clear Sticky Error. Used to clear the EDSCR cumulative error bits to 0. The actions on writing
to this bit are:

0
No action

1
Clear the EDSCR.{TXU, RXO, ERR} bits, and, if the core is in Debug state, the
EDSCR.ITO bit, to 0.

RES0, [1:0]

RES0

Reserved.

The EDRCR can be accessed through the internal memory-mapped interface and the external
debug interface, oﬀset 0x090.

Usage constraints

This register is accessible as follows:

Conﬁgurations

EDRCR is in the Core power domain.

|Off|DLK|OSLK|SLK|Default|
|---|---|---|---|---|
|Error|Error|Error|WI|WO|

# 25. AArch32 PMU registers

## 25.1 AArch32 PMU register summary

This chapter describes the AArch32 PMU registers and shows examples of how to use them.

The PMU event counters and their associated control registers are accessible in the AArch32
Execution state from the internal CP15 System register interface with MCR and MRC instructions for
32-bit registers and MCRR and MRRC for 64-bit registers.

The following table gives a summary of the Neoverse™ N1 PMU registers in the AArch32 Execution
state. For those registers that are not described in this chapter, see the Arm® Architecture Reference
Manual for A-proﬁle architecture.

Table 25-1: PMU register summary in the AArch32 Execution state

|CRn|Op1|CRm|Op2|Name|Type|Width|Reset|Description|
|---|---|---|---|---|---|---|---|---|
|c9|0|c12|0|PMCR|RW|32|`0x410C30XX`|25.5 PMCR, Performance Monitors Control Register on page<br>370|
|c9|0|c12|1|PMCNTENSET|RW|32|`0x00000000`|Performance Monitors Count Enable Set Register|
|c9|0|c12|2|PMCNTENCLR|RW|32|`0x00000000`|Performance Monitors Count Enable Clear Register|
|c9|0|c12|3|PMOVSR|RW|32|`0x00000000`|Performance Monitors Overﬂow Flag Status Register|
|c9|0|c12|4|PMSWINC|WO|32|UNK|Performance Monitors Software Increment Register|
|c9|0|c12|5|PMSELR|RW|32|UNK|Performance Monitors Event Counter Selection Register|
|c9|0|c12|6|PMCEID0|RO|32|`0x7FFF0F3F`|25.2 PMCEID0, Performance Monitors Common Event<br>Identiﬁcation Register 0 on page 364|
|c9|0|c12|7|PMCEID1|RO|32|`0x00F2AE7F`|25.3 PMCEID1, Performance Monitors Common Event<br>Identiﬁcation Register 1 on page 367|
|c9|0|c14|4|PMCEID2|RO|32|`0x0000000F`|25.4 PMCEID2, Performance Monitors Common Event<br>Identiﬁcation Register 2, (Ares Speciﬁc) on page 369|
|c9|0|c14|5|PMCEID3|RO|32|`0x00000000`|Reserved|
|c9|0|c13|0|PMCCNTR[31:0]|RW|32|UNK|Performance Monitors Cycle Count Register|
|c9|3|c13|0|PMCCNTR[63:0]|RW|64|UNK|UNK|
|c9|0|c13|1|PMXEVTYPER|RW|32|UNK|Performance Monitors Selected Event Type Register|
|c9|0|c13|2|PMXEVCNTR|RW|32|UNK|Performance Monitors Selected Event Count Register|
|c9|0|c14|0|PMUSERENR|RW|32|UNK|Performance Monitors User Enable Register|
|c9|0|c14|3|PMOVSSET|RW|32|`0x00000000`|Performance Monitor Overﬂow Flag Status Set Register|
|c14|0|c8|0|PMEVCNTR0|RW|32|UNK|Performance Monitor Event Count Registers|
|c14|0|c8|1|PMEVCNTR1|RW|32|UNK|UNK|
|c14|0|c8|2|PMEVCNTR2|RW|32|UNK|UNK|
|c14|0|c8|3|PMEVCNTR3|RW|32|UNK|UNK|
|c14|0|c8|4|PMEVCNTR4|RW|32|UNK|UNK|

## 25.2 PMCEID0, Performance Monitors Common Event Identification Register 0

25.2 PMCEID0, Performance Monitors Common Event
Identiﬁcation Register 0

The PMCEID0 deﬁnes which common architectural and common microarchitectural feature events
are implemented.

Bit ﬁeld descriptions

Figure 25-1: PMCEID0 bit assignments

ID[31:0], [31:0]

Common architectural and microarchitectural feature events that can be counted by the
PMU event counters.

The following table shows the PMCEID0 bit assignments with event implemented or not
implemented when the associated bit is set to 1 or 0. See the Arm® Architecture Reference
Manual for A-proﬁle architecture for more information about these events.

Table 25-2: PMU events

|CRn|Op1|CRm|Op2|Name|Type|Width|Reset|Description|
|---|---|---|---|---|---|---|---|---|
|c14|0|c8|5|PMEVCNTR5|RW|32|UNK||
|c14|0|c12|0|PMEVTYPER0|RW|32|UNK|Performance Monitors Event Type Registers|
|c14|0|c12|1|PMEVTYPER1|RW|32|UNK|UNK|
|c14|0|c12|2|PMEVTYPER2|RW|32|UNK|UNK|
|c14|0|c12|3|PMEVTYPER3|RW|32|UNK|UNK|
|c14|0|c12|4|PMEVTYPER4|RW|32|UNK|UNK|
|c14|0|c12|5|PMEVTYPER5|RW|32|UNK|UNK|
|c14|0|c15|7|PMCCFILTR|RW|32|UNK|Performance Monitors Cycle Count Filter Register|

|3130 29 28|27 26 25 24|23 22 21 20|19 18 1716|15141312|11 10 9 8|7 6 5 4|3 2 1 0|
|---|---|---|---|---|---|---|---|
|ID[31:0]|ID[31:0]|ID[31:0]|ID[31:0]|ID[31:0]|ID[31:0]|ID[31:0]|ID[31:0]|

|Bit|Event mnemonic|Description|
|---|---|---|
|[31]|L1D_CACHE_ALLOCATE|L1 Data cache allocate:<br>**`0`**<br>This event is not implemented.|
|[30]|CHAIN|Chain. For odd-numbered counters, counts once for each overﬂow of the preceding even-numbered<br>counter. For even-numbered counters, does not count:<br>**`1`**<br>This event is implemented.|

|Bit|Event mnemonic|Description|
|---|---|---|
|[29]|BUS_CYCLES|Bus cycle:<br>**`1`**<br>This event is implemented.|
|[28]|TTBR_WRITE_RETIRED|TTBR write, architecturally executed, condition check pass - write to translation table base:<br>**`1`**<br>This event is implemented.|
|[27]|INST_SPEC|Instruction speculatively executed:<br>**`1`**<br>This event is implemented.|
|[26]|MEMORY_ERROR|Local memory error:<br>**`1`**<br>This event is implemented.|
|[25]|BUS_ACCESS|Bus access:<br>**`1`**<br>This event is implemented.|
|[24]|L2D_CACHE_WB|L2 Data cache Write-Back:<br>**`1`**<br>This event is implemented.|
|[23]|L2D_CACHE_REFILL|L2 Data cache reﬁll:<br>**`1`**<br>This event is implemented.|
|[22]|L2D_CACHE|L2 Data cache access:<br>**`1`**<br>This event is implemented.|
|[21]|L1D_CACHE_WB|L1 Data cache Write-Back:<br>**`1`**<br>This event is implemented.|
|[20]|L1I_CACHE|L1 Instruction cache access:<br>**`1`**<br>This event is implemented.|
|[19]|MEM_ACCESS|Data memory access:<br>**`1`**<br>This event is implemented.|
|[18]|BR_PRED|Predictable branch Speculatively executed:<br>**`1`**<br>This event is implemented.|
|[17]|CPU_CYCLES|Cycle:<br>**`1`**<br>This event is implemented.|
|[16]|BR_MIS_PRED|Mispredicted or not predicted branch Speculatively executed:<br>**`1`**<br>This event is implemented.|

|Bit|Event mnemonic|Description|
|---|---|---|
|[15]|UNALIGNED_LDST_RETIRED|Instruction architecturally executed, condition check pass - unaligned load or store:<br>**`0`**<br>This event is not implemented.|
|[14]|BR_RETURN_RETIRED|Instruction architecturally executed, condition check pass - procedure return:<br>**`0`**<br>This event is not implemented.|
|[13]|BR_IMMED_RETIRED|Instruction architecturally executed - immediate branch:<br>**`0`**<br>This event is not implemented.|
|[12]|PC_WRITE_RETIRED|Instruction architecturally executed, condition check pass - software change of the PC:<br>**`0`**<br>This event is not implemented.|
|[11]|CID_WRITE_RETIRED|Instruction architecturally executed, condition check pass - write to CONTEXTIDR:<br>**`1`**<br>This event is implemented.|
|[10]|EXC_RETURN|Instruction architecturally executed, condition check pass - exception return:<br>**`1`**<br>This event is implemented.|
|[9]|EXC_TAKEN|Exception taken:<br>**`1`**<br>This event is implemented.|
|[8]|INST_RETIRED|Instruction architecturally executed:<br>**`1`**<br>This event is implemented.|
|[7]|ST_RETIRED|Instruction architecturally executed, condition check pass - store:<br>**`0`**<br>This event is not implemented.|
|[6]|LD_RETIRED|Instruction architecturally executed, condition check pass - load:<br>**`0`**<br>This event is not implemented.|
|[5]|L1D_TLB_REFILL|L1 Data TLB reﬁll:<br>**`1`**<br>This event is implemented.|
|[4]|L1D_CACHE|L1 Data cache access:<br>**`1`**<br>This event is implemented.|
|[3]|L1D_CACHE_REFILL|L1 Data cache reﬁll:<br>**`1`**<br>This event is implemented.|
|[2]|L1I_TLB_REFILL|L1 Instruction TLB reﬁll:<br>**`1`**<br>This event is implemented.|

## 25.3 PMCEID1, Performance Monitors Common Event Identification Register 1

The PMU events implemented in the above table can be found in Table 18-1: PMU
Events on page 318.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

25.3 PMCEID1, Performance Monitors Common Event
Identiﬁcation Register 1

The PMCEID1 deﬁnes which common architectural and common microarchitectural feature events
are implemented.

Bit ﬁeld descriptions

Figure 25-2: PMCEID1 bit assignments

31
0

24

23

ID[55:32]

RES0

RES0, [31:24]

RES0

Reserved.

ID[55:32], [23:0]

Common architectural and microarchitectural feature events that can be counted by the
PMU event counters.

For each bit described in the following table, the event is implemented if the bit is set to 1, or
not implemented if the bit is set to 0.

|Bit|Event mnemonic|Description|
|---|---|---|
|[1]|L1I_CACHE_REFILL|L1 Instruction cache reﬁll:<br>**`1`**<br>This event is implemented.|
|[0]|SW_INCR|Instruction architecturally executed, condition check pass - software increment:<br>**`1`**<br>This event is implemented.|

Table 25-3: PMU common events

|Bit|Event mnemonic|Description|
|---|---|---|
|[23]|LL_CACHE_MISS_RD|Attributable Last Level cache memory read<br>miss.<br>**`1`**<br>This event is implemented.|
|[22]|LL_CACHE_RD|Attributable Last Level cache memory read.<br>**`1`**<br>This event is implemented.|
|[21]|ITLB_WALK|Attributable instruction TLB access with at<br>least one translation table walk.<br>**`1`**<br>This event is implemented.|
|[20]|DTLB_WALK|Attributable data or uniﬁed TLB access with<br>at least one translation table walk.<br>**`1`**<br>This event is implemented.|
|[17]|REMOTE_ACCESS|Attributable access to another socket in a<br>multi-socket system.<br>**`1`**<br>This event is implemented.|
|[15]|L2D_TLB|Attributable Level 2 data or uniﬁed TLB<br>access.<br>**`1`**<br>This event is implemented.|
|[13]|L2D_TLB_REFILL|Attributable Level 2 data or uniﬁed TLB reﬁll.<br>**`1`**<br>This event is implemented.|
|[11]|L3D_CACHE|Attributable Level 3 data cache access.<br>**`1`**<br>This event is implemented.|
|[10]|L3D_CACHE_REFILL|Attributable Level 3 data cache reﬁll.<br>**`1`**<br>This event is implemented.|
|[9]|L3D_CACHE_ALLOCATE|Attributable Level 3 data or uniﬁed cache<br>allocation without reﬁll.<br>**`1`**<br>This event is implemented.|
|[6]|L1I_TLB|Attributable Level 1 instruction TLB access.<br>**`1`**<br>This event is implemented.|
|[5]|L1D_TLB|Attributable Level 1 data or uniﬁed TLB<br>access.<br>**`1`**<br>This event is implemented.|
|[4]|STALL_BACKEND|No operation issued due to backend.<br>**`1`**<br>This event is implemented.|

## 25.4 PMCEID2, Performance Monitors Common Event Identification Register 2

The PMU events implemented in the above table can be found in Table 18-1: PMU
Events on page 318.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

25.4 PMCEID2, Performance Monitors Common Event
Identiﬁcation Register 2

The PMCEID2 deﬁnes which common architectural and common microarchitectural feature events
are implemented.

Bit ﬁeld descriptions

Figure 25-3: PMCEID2 bit assignments

31
0
4

3

ID[3:0]

RES0

RES0, [31:4]

RES0
Reserved.

|Bit|Event mnemonic|Description|
|---|---|---|
|[3]|STALL_FRONTEND|No operation issued due to frontend.<br>**`1`**<br>This event is implemented.|
|[2]|BR_MIS_PRED_RETIRED|Instruction architecturally executed,<br>mispredicted branch.<br>**`1`**<br>This event is not implemented.|
|[1]|BR_RETIRED|Instruction architecturally executed, branch.<br>**`1`**<br>This event is implemented.|
|[0]|L2D_CACHE_ALLOCATE|Level 2 data cache allocation without reﬁll.<br>**`1`**<br>This event is implemented.|

## 25.5 PMCR, Performance Monitors Control Register

ID, [3:0]

Common architectural and microarchitectural feature events that can be counted by the
PMU event counters.

For each bit described in the following table, the event is implemented if the bit is set to 1, or
not implemented if the bit is set to 0.

Table 25-4: PMU common events

The PMCR provides details of the Performance Monitors implementation, including the number of
counters that are implemented, and conﬁgures and controls the counters.

Bit ﬁeld descriptions
PMCR is a 32-bit register, and is part of the Performance Monitors registers functional group.

Figure 25-4: PMCR bit assignments

31
0
5
6
7
1
2
10
11
15
16
23

3
4
24

IMP
X

IDCODE
N
E
P
C
D

DP

LC

res0

IMP, [31:24]

Indicates the implementer code. The value is:

0x41
ASCII character 'A' - implementer is Arm® Limited.

|Bit|Event mnemonic|Description|
|---|---|---|
|[31:4]|**RES0**|Reserved|
|[3]|SAMPLE_COLLISION|Sample collided with previous sample.<br>**`1`**<br>This event is implemented.|
|[2]|SAMPLE_FILTRATE|Sample taken and not removed by ﬁltering.<br>**`1`**<br>This event is implemented.|
|[1]|SAMPLE_FEED|Sample taken.<br>**`1`**<br>This event is implemented.|
|[0]|SAMPLE_POP|Sample population.<br>**`1`**<br>This event is implemented.|

IDCODE, [23:16]

Identiﬁcation code. The value is:

0x0C
Neoverse™ N1 core.

N, [15:11]

Identiﬁes the number of event counters implemented.

0b00110
The core implements six event counters.

RES0, [10:7]

RES0
Reserved.

LC, [6]

Long cycle count enable. Determines which PMCCNTR bit generates an overﬂow recorded in
PMOVSR[31]. The overﬂow event is generated on a 32-bit or 64-bit boundary. The possible
values are:

0b0
Overﬂow event is generated on a 32-bit boundary, when an
increment changes PMCCNTR[31] from 1 to 0. This is the reset
value.
0b1
Overﬂow event is generated on a 64-bit boundary, when an
increment changes PMCCNTR[63] from 1 to 0.

Arm deprecates use of PMCR.LC = 0b0.

DP, [5]

Disable cycle counter CCNT when event counting is prohibited. The possible values are:

0b0
Cycle counter operates regardless of the non-invasive debug
authentication settings. This is the reset value.
0b1
Cycle counter is disabled if non-invasive debug is not permitted and
enabled.

X, [4]

Export enable. This bit permits events to be exported to another debug device, such as a
trace macrocell, over an event bus. The possible values are:

0b0
Export of events is disabled. This is the reset value.
0b1
Export of events is enabled.

No events are exported when counting is prohibited.

This ﬁeld does not aﬀect the generation of Performance Monitors overﬂow interrupt
requests or signaling to a cross-trigger interface (CTI) that can be implemented as signals
exported from the PE.

When this register has an architecturally deﬁned reset value, if this ﬁeld is implemented as an
RW ﬁeld, it resets to 0.

D, [3]

Clock divider. The possible values are:

0b0
When enabled, counter CCNT counts every clock cycle. This is the
reset value.
0b1
When enabled, counter CCNT counts once every 64 clock cycles.

Arm deprecates use of PMCR.D = 0b1.

C, [2]

Cycle counter reset. This bit is WO. The eﬀects of writing to this bit are:

0b0
No action. This is the reset value.
0b1
Reset PMCCNTR to zero.

This bit is always RAZ.

Resetting PMCCNTR does not clear the PMCCNTR overﬂow bit to 0. See the Arm®
Architecture Reference Manual for A-proﬁle architecture for more information.

P, [1]

Event counter reset. This bit is WO. The eﬀects of writing to this bit are:

0b0
No action. This is the reset value.
0b1
Reset all event counters accessible in the current EL, not including
PMCCNTR, to zero.

This bit is always RAZ.

In Non-secure EL0 and EL1, a write of 1 to this bit does not reset event counters that
HDCR.HPMN or MDCR_EL2.HPMN reserves for EL2 use.

In EL2 and EL3, a write of 1 to this bit resets all the event counters.

Resetting the event counters does not clear any overﬂow bits to 0.

E, [0]

Enable. The possible values are:

0b0
All counters that are accessible at Non-secure EL1, including
PMCCNTR, are disabled. This is the reset value.
0b1
When this register has an architecturally deﬁned reset value, this ﬁeld
resets to 0.

This bit is RW.

This bit does not aﬀect the operation of event counters that HDCR.HPMN or
MDCR_EL2.HPMN reserves for EL2 use.

When this register has an architecturally deﬁned reset value, this ﬁeld resets to 0.

Conﬁgurations

AArch32 System register PMCR is architecturally mapped to AArch64 System register
PMCR_EL0. See 26.4 PMCR_EL0, Performance Monitors Control Register, EL0 on page
381.

AArch32 System register PMCR bits [6:0] are architecturally mapped to External register
PMCR_EL0[6:0].

There is one instance of this register that is used in both Secure and Non-secure states.

This register is in the Warm reset domain. Some or all RW ﬁelds of this register have deﬁned
reset values. On a Warm or Cold reset these apply only if the PE resets into an Exception
level that is using AArch32. Otherwise, on a Warm or Cold reset RW ﬁelds in this register
reset to architecturally UNKNOWN values.

# 26. AArch64 PMU registers

## 26.1 AArch64 PMU register summary

This chapter describes the AArch64 PMU registers and shows examples of how to use them.

The PMU event counters and their associated control registers are accessible in the AArch64
Execution state with MRS and MSR instructions.

The following table gives a summary of the Neoverse™ N1 PMU registers in the AArch64 Execution
state. For those registers that are not described in this chapter, see the Arm® Architecture Reference
Manual for A-proﬁle architecture.

Table 26-1: PMU register summary in the AArch64 Execution state

|Name|Type|Width|Reset|Description|
|---|---|---|---|---|
|PMCR_EL0|RW|32|`0x410C30XX`|26.4 PMCR_EL0,<br>Performance Monitors<br>Control Register, EL0 on<br>page 381|
|PMCNTENSET_EL0|RW|32|UNK|Performance Monitors<br>Count Enable Set<br>Register|
|PMCNTENCLR_EL0|RW|32|UNK|Performance Monitors<br>Count Enable Clear<br>Register|
|PMOVSCLR_EL0|RW|32|UNK|Performance Monitors<br>Overﬂow Flag Status<br>Register|
|PMSWINC_EL0|WO|32|UNK|Performance Monitors<br>Software Increment<br>Register|
|PMSELR_EL0|RW|32|UNK|Performance Monitors<br>Event Counter Selection<br>Register|
|PMCEID0_EL0|RO|64|`0x0000000F7FFF0F3F`|26.2 PMCEID0_EL0,<br>Performance Monitors<br>Common Event<br>Identiﬁcation Register 0,<br>EL0 on page 375|
|PMCEID1_EL0|RO|64|`0x0000000000F2AE7F`|26.3 PMCEID1_EL0,<br>Performance Monitors<br>Common Event<br>Identiﬁcation Register 1,<br>EL0 on page 379|
|PMCCNTR_EL0|RW|64|UNK|Performance Monitors<br>Cycle Count Register|

Related information
PMU events on page 318

|Name|Type|Width|Reset|Description|
|---|---|---|---|---|
|PMXEVTYPER_EL0|RW|32|UNK|Performance Monitors<br>Selected Event Type and<br>Filter Register|
|PMCCFILTR_EL0|RW|32|UNK|Performance Monitors<br>Cycle Count Filter<br>Register|
|PMXEVCNTR_EL0|RW|32|UNK|Performance Monitors<br>Selected Event Count<br>Register|
|PMUSERENR_EL0|RW|32|UNK|Performance Monitors<br>User Enable Register|
|PMINTENSET_EL1|RW|32|UNK|Performance Monitors<br>Interrupt Enable Set<br>Register|
|PMINTENCLR_EL1|RW|32|UNK|Performance Monitors<br>Interrupt Enable Clear<br>Register|
|PMOVSSET_EL0|RW|32|UNK|Performance Monitors<br>Overﬂow Flag Status Set<br>Register|
|PMEVCNTR0_EL0|RW|32|UNK|Performance Monitors<br>Event Count Registers|
|PMEVCNTR1_EL0|RW|32|UNK|UNK|
|PMEVCNTR2_EL0|RW|32|UNK|UNK|
|PMEVCNTR3_EL0|RW|32|UNK|UNK|
|PMEVCNTR4_EL0|RW|32|UNK|UNK|
|PMEVCNTR5_EL0|RW|32|UNK|UNK|
|PMEVTYPER0_EL0|RW|32|UNK|Performance Monitors<br>Event Type Registers|
|PMEVTYPER1_EL0|RW|32|UNK|UNK|
|PMEVTYPER2_EL0|RW|32|UNK|UNK|
|PMEVTYPER3_EL0|RW|32|UNK|UNK|
|PMEVTYPER4_EL0|RW|32|UNK|UNK|
|PMEVTYPER5_EL0|RW|32|UNK|UNK|
|PMCCFILTR_EL0|RW|32|UNK|Performance Monitors<br>Cycle Count Filter<br>Register|

## 26.2 PMCEID0_EL0, Performance Monitors Common Event Identification Register 0, EL0

26.2 PMCEID0_EL0, Performance Monitors Common
Event Identiﬁcation Register 0, EL0

The PMCEID0_EL0 deﬁnes which common architectural and common microarchitectural feature
events are implemented.

Bit ﬁeld descriptions

Figure 26-1: PMCEID0_EL0 bit assignments

63
0

36 35

CE[35:0]

RES0

RES0, [63:36]

RES0
Reserved.

CE[35:0], [35:0]

Common architectural and microarchitectural feature events that can be counted by the
PMU event counters.

For each bit described in the following table, the event is implemented if the bit is set to 1, or
not implemented if the bit is set to 0.

Table 26-2: PMU common events

|Bit|Event mnemonic|Description|
|---|---|---|
|[35]|SAMPLE_COLLISION|Sample collided with previous sample:<br>**`1`**<br>This event is implemented.|
|[34]|SAMPLE_FILTRATE|Sample taken, not removed:<br>**`1`**<br>This event is implemented.|
|[33]|SAMPLE_FEED|Sample taken:<br>**`1`**<br>This event is implemented.|
|[32]|SAMPLE_POP|Sample population:<br>**`1`**<br>This event is implemented.|
|[31]|L1D_CACHE_ALLOCATE|L1 Data cache allocate:<br>**`0`**<br>This event is not implemented.|

|Bit|Event mnemonic|Description|
|---|---|---|
|[30]|CHAIN|Chain. For odd-numbered counters, counts once for each overﬂow of the preceding even-numbered<br>counter. For even-numbered counters, does not count:<br>**`1`**<br>This event is implemented.|
|[29]|BUS_CYCLES|Bus cycle:<br>**`1`**<br>This event is implemented.|
|[28]|TTBR_WRITE_RETIRED|TTBR write, architecturally executed, condition check pass - write to translation table base:<br>**`1`**<br>This event is implemented.|
|[27]|INST_SPEC|Instruction speculatively executed:<br>**`1`**<br>This event is implemented.|
|[26]|MEMORY_ERROR|Local memory error:<br>**`1`**<br>This event is implemented.|
|[25]|BUS_ACCESS|Bus access:<br>**`1`**<br>This event is implemented.|
|[24]|L2D_CACHE_WB|L2 Data cache Write-Back:<br>**`1`**<br>This event is implemented.|
|[23]|L2D_CACHE_REFILL|L2 Data cache reﬁll:<br>**`1`**<br>This event is implemented.|
|[22]|L2D_CACHE|L2 Data cache access:<br>**`1`**<br>This event is implemented.|
|[21]|L1D_CACHE_WB|L1 Data cache Write-Back:<br>**`1`**<br>This event is implemented.|
|[20]|L1I_CACHE|L1 Instruction cache access:<br>**`1`**<br>This event is implemented.|
|[19]|MEM_ACCESS|Data memory access:<br>**`1`**<br>This event is implemented.|
|[18]|BR_PRED|Predictable branch speculatively executed:<br>**`1`**<br>This event is implemented.|
|[17]|CPU_CYCLES|Cycle:<br>**`1`**<br>This event is implemented.|
|[16]|BR_MIS_PRED|Mispredicted or not predicted branch speculatively executed:<br>**`1`**<br>This event is implemented.|

|Bit|Event mnemonic|Description|
|---|---|---|
|[15]|UNALIGNED_LDST_RETIRED|Instruction architecturally executed, condition check pass - unaligned load or store:<br>**`0`**<br>This event is not implemented.|
|[14]|BR_RETURN_RETIRED|Instruction architecturally executed, condition check pass - procedure return:<br>**`0`**<br>This event is not implemented.|
|[13]|BR_IMMED_RETIRED|Instruction architecturally executed - immediate branch:<br>**`0`**<br>This event is not implemented.|
|[12]|PC_WRITE_RETIRED|Instruction architecturally executed, condition check pass - software change of the PC:<br>**`0`**<br>This event is not implemented.|
|[11]|CID_WRITE_RETIRED|Instruction architecturally executed, condition check pass - write to CONTEXTIDR:<br>**`1`**<br>This event is implemented.|
|[10]|EXC_RETURN|Instruction architecturally executed, condition check pass - exception return:<br>**`1`**<br>This event is implemented.|
|[9]|EXC_TAKEN|Exception taken:<br>**`1`**<br>This event is implemented.|
|[8]|INST_RETIRED|Instruction architecturally executed:<br>**`1`**<br>This event is implemented.|
|[7]|ST_RETIRED|Instruction architecturally executed, condition check pass - store:<br>**`0`**<br>This event is not implemented.|
|[6]|LD_RETIRED|Instruction architecturally executed, condition check pass - load:<br>**`0`**<br>This event is not implemented.|
|[5]|L1D_TLB_REFILL|L1 Data TLB reﬁll:<br>**`1`**<br>This event is implemented.|
|[4]|L1D_CACHE|L1 Data cache access:<br>**`1`**<br>This event is implemented.|
|[3]|L1D_CACHE_REFILL|L1 Data cache reﬁll:<br>**`1`**<br>This event is implemented.|
|[2]|L1I_TLB_REFILL|L1 Instruction TLB reﬁll:<br>**`1`**<br>This event is implemented.|
|[1]|L1I_CACHE_REFILL|L1 Instruction cache reﬁll:<br>**`1`**<br>This event is implemented.|

## 26.3 PMCEID1_EL0, Performance Monitors Common Event Identification Register 1, EL0

The PMU events implemented in the above table can be found in Table 18-1: PMU
Events on page 318.

26.3 PMCEID1_EL0, Performance Monitors Common
Event Identiﬁcation Register 1, EL0

The PMCEID1_EL0 deﬁnes which common architectural and common microarchitectural feature
events are implemented.

Bit ﬁeld descriptions

Figure 26-2: PMCEID1 bit assignments

63
0

24

23

ID[55:32]

RES0

RES0, [63:24]

RES0
Reserved.

ID[55:32], [23:0]

Common architectural and microarchitectural feature events that can be counted by the
PMU event counters.

For each bit described in the following table, the event is implemented if the bit is set to 1, or
not implemented if the bit is set to 0.

Table 26-3: PMU common events

|Bit|Event mnemonic|Description|
|---|---|---|
|[0]|SW_INCR|Instruction architecturally executed, condition check pass - software increment:<br>**`1`**<br>This event is implemented.|

|Bit|Event mnemonic|Description|
|---|---|---|
|[23]|LL_CACHE_MISS_RD|Attributable Last Level cache memory read<br>miss.<br>**`1`**<br>This event is implemented.|

|Bit|Event mnemonic|Description|
|---|---|---|
|[22]|LL_CACHE_RD|Attributable Last Level cache memory read.<br>**`1`**<br>This event is implemented.|
|[21]|ITLB_WALK|Attributable instruction TLB access with at<br>least one translation table walk.<br>**`1`**<br>This event is implemented.|
|[20]|DTLB_WALK|Attributable data or uniﬁed TLB access with<br>at least one translation table walk.<br>**`1`**<br>This event is implemented.|
|[17]|REMOTE_ACCESS|Attributable access to another socket in a<br>multi-socket system.<br>**`1`**<br>This event is implemented.|
|[15]|L2D_TLB|Attributable Level 2 data or uniﬁed TLB<br>access.<br>**`1`**<br>This event is implemented.|
|[13]|L2D_TLB_REFILL|Attributable Level 2 data or uniﬁed TLB reﬁll.<br>**`1`**<br>This event is implemented.|
|[11]|L3D_CACHE|Attributable Level 3 data cache access.<br>**`1`**<br>This event is implemented.|
|[10]|L3D_CACHE_REFILL|Attributable Level 3 data cache reﬁll.<br>**`1`**<br>This event is implemented.|
|[9]|L3D_CACHE_ALLOCATE|Attributable Level 3 data or uniﬁed cache<br>allocation without reﬁll.<br>**`1`**<br>This event is implemented.|
|[6]|L1I_TLB|Attributable Level 1 instruction TLB access.<br>**`1`**<br>This event is implemented.|
|[5]|L1D_TLB|Attributable Level 1 data or uniﬁed TLB<br>access.<br>**`1`**<br>This event is implemented.|
|[4]|STALL_BACKEND|No operation issued due to backend.<br>**`1`**<br>This event is implemented.|
|[3]|STALL_FRONTEND|No operation issued due to frontend.<br>**`1`**<br>This event is implemented.|

## 26.4 PMCR_EL0, Performance Monitors Control Register, EL0

The PMU events implemented in the above table can be found in Table 18-1: PMU
Events on page 318.

26.4 PMCR_EL0, Performance Monitors Control Register,
EL0

The PMCR_EL0 provides details of the Performance Monitors implementation, including the
number of counters that are implemented, and conﬁgures and controls the counters.

Bit ﬁeld descriptions

Figure 26-3: PMCR_EL0 bit assignments

31
24 23
16 15
11 10
6
5
4
3
2
1
0

7

IMP
IDCODE
N
DP X D C P
LC

E

RES0

IMP, [31:24]

Implementer code:

0x41
Arm.

This is a read-only ﬁeld.

IDCODE, [23:16]

Identiﬁcation code:

0x0C
Neoverse™ N1.

|Bit|Event mnemonic|Description|
|---|---|---|
|[2]|BR_MIS_PRED_RETIRED|Instruction architecturally executed,<br>mispredicted branch.<br>**`1`**<br>This event is not implemented.|
|[1]|BR_RETIRED|Instruction architecturally executed, branch.<br>**`1`**<br>This event is implemented.|
|[0]|L2D_CACHE_ALLOCATE|Level 2 data cache allocation without reﬁll.<br>**`1`**<br>This event is implemented.|

This is a read-only ﬁeld.

N, [15:11]

Number of event counters.

0b00110
Six counters.

RES0, [10:7]

RES0
Reserved.

LC, [6]

Long cycle count enable. Determines which PMCCNTR_EL0 bit generates an overﬂow
recorded in PMOVSR[31]. The possible values are:

0
Overﬂow on increment that changes PMCCNTR_EL0[31] from 1 to
0.
1
Overﬂow on increment that changes PMCCNTR_EL0[63] from 1 to
0.

DP, [5]

Disable cycle counter, PMCCNTR_EL0 when event counting is prohibited:

0
Cycle counter operates regardless of the non-invasive debug
authentication settings. This is the reset value.
1
Cycle counter is disabled if non-invasive debug is not permitted and
enabled.

This bit is read/write.

X, [4]

Export enable. This bit permits events to be exported to another debug device, such as a
trace macrocell, over an event bus:

0
Export of events is disabled. This is the reset value.
1
Export of events is enabled.

This bit is read/write and does not aﬀect the generation of Performance Monitors interrupts
on the nPMUIRQ pin.

D, [3]

Clock divider:

0
When enabled, PMCCNTR_EL0 counts every clock cycle. This is the
reset value.
1
When enabled, PMCCNTR_EL0 counts every 64 clock cycles.

This bit is read/write.

C, [2]

Clock counter reset. This bit is WO. The eﬀects of writing to this bit are:

0
No action. This is the reset value.
1
Reset PMCCNTR_EL0 to 0.

This bit is always RAZ.

Resetting PMCCNTR_EL0 does not clear the PMCCNTR_EL0 overﬂow bit to 0. See the
Arm® Architecture Reference Manual for A-proﬁle architecture for more information.

P, [1]

Event counter reset. This bit is WO. The eﬀects of writing to this bit are:

0
No action. This is the reset value.
1
Reset all event counters, not including PMCCNTR_EL0, to zero.

This bit is always RAZ.

In Non-secure EL0 and EL1, a write of 1 to this bit does not reset event counters that
MDCR_EL2.HPMN reserves for EL2 use.

In EL2 and EL3, a write of 1 to this bit resets all the event counters.

Resetting the event counters does not clear any overﬂow bits to 0.

E, [0]

Enable. The possible values of this bit are:

0
All counters, including PMCCNTR_EL0, are disabled. This is the reset
value.
1
All counters are enabled.

This bit is RW.

In Non-secure EL0 and EL1, this bit does not aﬀect the operation of event counters that
MDCR_EL2.HPMN reserves for EL2 use.

On Warm reset, the ﬁeld resets to 0.

Conﬁgurations

AArch64 System register PMCR_EL0 is architecturally mapped to AArch32 System register
PMCR.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

# 27. Memory-mapped PMU registers

## 27.1 Memory-mapped PMU register summary

This chapter describes the memory-mapped PMU registers and shows examples of how to use
them.

There are PMU registers that are accessible through the external debug interface.

These registers are listed in the following table. For those registers that are not described in this
chapter, see the Arm® Architecture Reference Manual for A-proﬁle architecture.

Table 27-1: Memory-mapped PMU register summary

|Offset|Name|Type|Description|
|---|---|---|---|
|`0x000`|PMEVCNTR0_EL0|RW|Performance Monitor Event Count Register<br>0|
|`0x004`|-|-|Reserved|
|`0x008`|PMEVCNTR1_EL0|RW|Performance Monitor Event Count Register<br>1|
|`0x00C`|-|-|Reserved|
|`0x010`|PMEVCNTR2_EL0|RW|Performance Monitor Event Count Register<br>2|
|`0x014`|-|-|Reserved|
|`0x018`|PMEVCNTR3_EL0|RW|Performance Monitor Event Count Register<br>3|
|`0x01C`|-|-|Reserved|
|`0x020`|PMEVCNTR4_EL0|RW|Performance Monitor Event Count Register<br>4|
|`0x024`|-|-|Reserved|
|`0x028`|PMEVCNTR5_EL0|RW|Performance Monitor Event Count Register<br>5|
|`0x02C-0x0F4`|-|-|Reserved|
|`0x0F8`|PMCCNTR_EL0[31:0]|RW|Performance Monitor Cycle Count Register|
|`0x0FC`|PMCCNTR_EL0[63:32]|RW|RW|
|`0x200`|PMPCSR[31:0]|RO|Program Counter Sample Register|
|`0x204`|PMPCSR[63:32]|PMPCSR[63:32]|PMPCSR[63:32]|
|`0x208`|PMCID1SR|RO|CONTEXTIDR_EL1 Sample Register|
|`0x20C`|PMVIDSR|RO|VMID Sample Register|
|`0x220`|PMPCSR[31:0]|RO|Program Counter Sample Register (alias)|
|`0x224`|PMPCSR[63:32]|PMPCSR[63:32]|PMPCSR[63:32]|
|`0x228`|PMCID1SR|RO|CONTEXTIDR_EL1 Sample Register (alias)|
|`0x22C`|PMCID2SR|RO|CONTEXTIDR_EL2 Sample Register|

|Offset|Name|Type|Description|
|---|---|---|---|
|`0x230-0x3FC`|-|-|Reserved|
|`0x418-0x478`|-|-|Reserved|
|`0x47C`|PMCCFILTR_EL0|RW|Performance Monitor Cycle Count Filter<br>Register|
|`0x600`|PMPCSSR_LO|RO|28.2 PMPCSSR, PMU Snapshot Program<br>Counter Sample Register on page 396|
|`0x604`|PMPCSSR_HI|RO|RO|
|`0x608`|PMCIDSSR|RO|28.3 PMCIDSSR, PMU Snapshot<br>CONTEXTIDR_EL1 Sample Register on page<br>397|
|`0x60C`|PMCID2SSR|RO|28.4 PMCID2SSR, PMU Snapshot<br>CONTEXTIDR_EL2 Sample Register on page<br>397|
|`0x610`|PMSSSR|RO|28.5 PMSSSR, PMU Snapshot Status<br>Register on page 398|
|`0x614`|PMOVSSR|RO|28.6 PMOVSSR, PMU Snapshot Overﬂow<br>Status Register on page 399|
|`0x618`|PMCCNTSR_LO|RO|28.7 PMCCNTSR, PMU Snapshot Cycle<br>Counter Register on page 399|
|`0x61C`|PMCCNTSR_HI|RO|RO|
|`0x620`+ 4×n|PMEVCNTSRn|RO|28.8 PMEVCNTSRn, PMU Snapshot Cycle<br>Counter Registers 0-5 on page 400|
|`0x6F0`|PMSSCR|WO|28.9 PMSSCR, PMU Snapshot Capture<br>Register on page 400|
|`0xC00`|PMCNTENSET_EL0|RW|Performance Monitor Count Enable Set<br>Register|
|`0xC04-0xC1C`|-|-|Reserved|
|`0xC20`|PMCNTENCLR_EL0|RW|Performance Monitor Count Enable Clear<br>Register|
|`0xC24-0xC3C`|-|-|Reserved|
|`0xC40`|PMINTENSET_EL1|RW|Performance Monitor Interrupt Enable Set<br>Register|
|`0xC44-0xC5C`|-|-|Reserved|
|`0xC60`|PMINTENCLR_EL1|RW|Performance Monitor Interrupt Enable Clear<br>Register|
|`0xC64-0xC7C`|-|-|Reserved|
|`0xC80`|PMOVSCLR_EL0|RW|Performance Monitor Overﬂow Flag Status<br>Register|
|`0xC84-0xC9C`|-|-|Reserved|
|`0xCA0`|PMSWINC_EL0|WO|Performance Monitor Software Increment<br>Register|
|`0xCA4-0xCBC`|-|-|Reserved|
|`0xCC0`|PMOVSSET_EL0|RW|Performance Monitor Overﬂow Flag Status<br>Set Register|
|`0xCC4-0xDFC`|-|-|Reserved|

|Offset|Name|Type|Description|
|---|---|---|---|
|`0xE00`|PMCFGR|RO|27.2 PMCFGR, Performance Monitors<br>Conﬁguration Register on page 387|
|`0xE04`|PMCR_EL0|RW|Performance Monitors Control Register.<br>This register is distinct from the PMCR_EL0<br>System register. It does not have the same<br>value.|
|`0xE08-0xE1C`|-|-|Reserved|
|`0xE20`|PMCEID0|RO|25.2 PMCEID0, Performance Monitors<br>Common Event Identiﬁcation Register 0 on<br>page 364|
|`0xE24`|PMCEID1|RO|25.3 PMCEID1, Performance Monitors<br>Common Event Identiﬁcation Register 1 on<br>page 367|
|`0xE28`|PMCEID2|RO|25.4 PMCEID2, Performance Monitors<br>Common Event Identiﬁcation Register 2,<br>(Ares Speciﬁc) on page 369<br>Performance Monitors Common Event<br>Identiﬁcation register 2|
|`0xE2C`|PMCEID3|RO|Performance Monitors Common Event<br>Identiﬁcation register 3|
|`0xFA4`|-|-|Reserved|
|`0xFA8`|PMDEVAFF0|RO|13.91 MPIDR_EL1, Multiprocessor Aﬃnity<br>Register, EL1 on page 231|
|`0xFAC`|PMDEVAFF1|RO|13.91 MPIDR_EL1, Multiprocessor Aﬃnity<br>Register, EL1 on page 231|
|`0xFB8`|PMAUTHSTATUS|RO|Performance Monitor Authentication Status<br>Register|
|`0xFBC`|PMDEVARCH|RO|Performance Monitor Device Architecture<br>Register|
|`0xFC0-0xFC8`|-|-|Reserved|
|`0xFCC`|PMDEVTYPE|RO|Performance Monitor Device Type Register|
|`0xFD0`|PMPIDR4|RO|27.11 PMPIDR4, Performance Monitors<br>Peripheral Identiﬁcation Register 4 on page<br>394|
|`0xFD4`|PMPIDR5|RO|27.12 PMPIDRn, Performance Monitors<br>Peripheral Identiﬁcation Register 5-7 on<br>page 395|
|`0xFD8`|PMPIDR6|RO|RO|
|`0xFDC`|PMPIDR7|RO|RO|
|`0xFE0`|PMPIDR0|RO|27.7 PMPIDR0, Performance Monitors<br>Peripheral Identiﬁcation Register 0 on page<br>391|
|`0xFE4`|PMPIDR1|RO|27.8 PMPIDR1, Performance Monitors<br>Peripheral Identiﬁcation Register 1 on page<br>392|

## 27.2 PMCFGR, Performance Monitors Configuration Register

27.2 PMCFGR, Performance Monitors Conﬁguration
Register

The PMCFGR contains PMU speciﬁc conﬁguration data.

Bit ﬁeld descriptions
The PMCFGR is a 32-bit register.

Figure 27-1: PMCFGR bit assignments

31
17 16 15 14 13
8
7
0

Size

N

EX
CCD
CC

RES0

RES0, [31:17]

RES0
Reserved.

EX, [16]

Export supported. The value is:

1
Export is supported. PMCR_EL0.EX is read/write.

|Offset|Name|Type|Description|
|---|---|---|---|
|`0xFE8`|PMPIDR2|RO|27.9 PMPIDR2, Performance Monitors<br>Peripheral Identiﬁcation Register 2 on page<br>393|
|`0xFEC`|PMPIDR3|RO|27.10 PMPIDR3, Performance Monitors<br>Peripheral Identiﬁcation Register 3 on page<br>393|
|`0xFF0`|PMCIDR0|RO|27.3 PMCIDR0, Performance Monitors<br>Component Identiﬁcation Register 0 on page<br>388|
|`0xFF4`|PMCIDR1|RO|27.4 PMCIDR1, Performance Monitors<br>Component Identiﬁcation Register 1 on page<br>389|
|`0xFF8`|PMCIDR2|RO|27.5 PMCIDR2, Performance Monitors<br>Component Identiﬁcation Register 2 on page<br>390|
|`0xFFC`|PMCIDR3|RO|27.6 PMCIDR3, Performance Monitors<br>Component Identiﬁcation Register 3 on page<br>390|

## 27.3 PMCIDR0, Performance Monitors Component Identification Register 0

CCD, [15]

Cycle counter has pre-scale. The value is:

1
PMCR_EL0.D is read/write.

CC, [14]

Dedicated cycle counter supported. The value is:

1
Dedicated cycle counter is supported.

Size, [13:8]

Counter size. The value is:

0b111111
64-bit counters.

N, [7:0]

Number of event counters. The value is:

0x06
Six counters.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMCFGR can be accessed through the external debug interface, oﬀset 0xE00.

27.3 PMCIDR0, Performance Monitors Component
Identiﬁcation Register 0

The PMCIDR0 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMCIDR0 is a 32-bit register.

Figure 27-2: PMCIDR0 bit assignments

7
8

31
0

PRMBL_0

RES0

RES0, [31:8]

RES0

Reserved.

## 27.4 PMCIDR1, Performance Monitors Component Identification Register 1

PRMBL_0, [7:0]

0x0D
Preamble byte 0.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMCIDR0 can be accessed through the external debug interface, oﬀset 0xFF0.

27.4 PMCIDR1, Performance Monitors Component
Identiﬁcation Register 1

The PMCIDR1 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMCIDR1 is a 32-bit register.

Figure 27-3: PMCIDR1 bit assignments

7
8
3
4

31
0

CLASS

PRMBL_1

RES0

RES0, [31:8]

RES0

Reserved.

CLASS, [7:4]

0x9
Debug component.

PRMBL_1, [3:0]

0x0
Preamble byte 1.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMCIDR1 can be accessed through the external debug interface, oﬀset 0xFF4.

## 27.5 PMCIDR2, Performance Monitors Component Identification Register 2

## 27.6 PMCIDR3, Performance Monitors Component Identification Register 3

27.5 PMCIDR2, Performance Monitors Component
Identiﬁcation Register 2

The PMCIDR2 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMCIDR2 is a 32-bit register.

Figure 27-4: PMCIDR2 bit assignments

7
8

31
0

PRMBL_2

RES0

RES0, [31:8]

RES0

Reserved.

PRMBL_2, [7:0]

0x05
Preamble byte 2.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMCIDR2 can be accessed through the external debug interface, oﬀset 0xFF8.

27.6 PMCIDR3, Performance Monitors Component
Identiﬁcation Register 3

The PMCIDR3 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMCIDR3 is a 32-bit register.

## 27.7 PMPIDR0, Performance Monitors Peripheral Identification Register 0

Figure 27-5: PMCIDR3 bit assignments

7
8

31
0

PRMBL_3

RES0

RES0, [31:8]

RES0

Reserved.

PRMBL_3, [7:0]

0xB1
Preamble byte 3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMCIDR3 can be accessed through the external debug interface, oﬀset 0xFFC.

27.7 PMPIDR0, Performance Monitors Peripheral
Identiﬁcation Register 0

The PMPIDR0 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMPIDR0 is a 32-bit register.

Figure 27-6: PMPIDR0 bit assignments

31
0
7
8

Part_0

RES0

RES0, [31:8]

RES0

Reserved.

## 27.8 PMPIDR1, Performance Monitors Peripheral Identification Register 1

Part_0, [7:0]

0x0C
Least signiﬁcant byte of the performance monitor part number.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMPIDR0 can be accessed through the external debug interface, oﬀset 0xFE0.

27.8 PMPIDR1, Performance Monitors Peripheral
Identiﬁcation Register 1

The PMPIDR1 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMPIDR1 is a 32-bit register.

Figure 27-7: PMPIDR1 bit assignments

31
0
3
4

7
8

DES_0

Part_1

RES0

RES0, [31:8]

RES0

Reserved.

DES_0, [7:4]

0xB
Arm Limited. This is the least signiﬁcant nibble of JEP106 ID code.

Part_1, [3:0]

0xD
Most signiﬁcant nibble of the performance monitor part number.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMPIDR1 can be accessed through the external debug interface, oﬀset 0xFE4.

## 27.9 PMPIDR2, Performance Monitors Peripheral Identification Register 2

27.9 PMPIDR2, Performance Monitors Peripheral
Identiﬁcation Register 2

The PMPIDR2 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMPIDR2 is a 32-bit register.

Figure 27-8: PMPIDR2 bit assignments

31
0
3
4

7
8

2

Revision

DES_1

JEDEC

RES0

RES0, [31:8]

RES0

Reserved.

Revision, [7:4]

0x5
r4p1.

JEDEC, [3]

0b1
RAO. Indicates a JEP106 identity code is used.

DES_1, [2:0]

0b011
Arm Limited. This is the most signiﬁcant nibble of JEP106 ID code.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMPIDR2 can be accessed through the external debug interface, oﬀset 0xFE8.

## 27.10 PMPIDR3, Performance Monitors Peripheral Identification Register 3

## 27.11 PMPIDR4, Performance Monitors Peripheral Identification Register 4

27.10 PMPIDR3, Performance Monitors Peripheral
Identiﬁcation Register 3

The PMPIDR3 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMPIDR3 is a 32-bit register.

Figure 27-9: PMPIDR3 bit assignments

31
0
3
4

7
8

REVAND

CMOD

RES0

RES0, [31:8]

RES0

Reserved.

REVAND, [7:4]

0x0
Part minor revision.

CMOD, [3:0]

0x0
Customer modiﬁed.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMPIDR3 can be accessed through the external debug interface, oﬀset 0xFEC.

27.11 PMPIDR4, Performance Monitors Peripheral
Identiﬁcation Register 4

The PMPIDR4 provides information to identify a Performance Monitor component.

Bit ﬁeld descriptions
The PMPIDR4 is a 32-bit register.

## 27.12 PMPIDRn, Performance Monitors Peripheral Identification Register 5-7

Figure 27-10: PMPIDR4 bit assignments

31
0
3
4

7
8

Size

DES_2

RES0

RES0, [31:8]

RES0

Reserved.

Size, [7:4]

0x0
Size of the component. Log2 the number of 4KB pages from the start of the
component to the end of the component ID registers.

DES_2, [3:0]

0x4
Arm Limited. This is the least signiﬁcant nibble JEP106 continuation code.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Architecture Reference Manual for A-proﬁle architecture.

The PMPIDR4 can be accessed through the external debug interface, oﬀset 0xFD0.

27.12 PMPIDRn, Performance Monitors Peripheral
Identiﬁcation Register 5-7

No information is held in the Peripheral ID5, Peripheral ID6, and Peripheral ID7 Registers.

They are reserved for future use and are RES0.

# 28. PMU snapshot registers

## 28.1 PMU snapshot register summary

## 28.2 PMPCSSR, PMU Snapshot Program Counter Sample Register

PMU snapshot registers are an IMPLEMENTATION DEFINED extension to an Arm®v8‑A compliant
PMU to support an external core monitor that connects to a system proﬁler.

28.1 PMU snapshot register summary

The snapshot registers are visible in an IMPLEMENTATION DEFINED region of the PMU external debug
interface. Each time the debugger sends a snapshot request, information is collected to see how
the code is executed in the diﬀerent cores.

The following table describes the PMU snapshot registers implemented in the core.

Table 28-1: PMU snapshot register summary

28.2 PMPCSSR, PMU Snapshot Program Counter Sample
Register

The PMPCSSR holds the same value as the PMPCSR register at the time of the snapshot.

However, unlike the other view of PMPCSR, it is not sensitive to reads. That is, reads of PMPCSSR
through the PMU snapshot view do not cause a new sample capture and do not change
PMCID1SR, PMCID2SR, or PMVIDSR.

Bit ﬁeld descriptions
The PMPCSSR is a 64-bit read-only register.

|Offset|Name|Type|Width|Description|
|---|---|---|---|---|
|`0x600`|PMPCSSR_LO|RO|32|28.2 PMPCSSR, PMU Snapshot Program Counter Sample Register on page 396|
|`0x604`|PMPCSSR_HI|RO|32|32|
|`0x608`|PMCIDSSR|RO|32|28.3 PMCIDSSR, PMU Snapshot CONTEXTIDR_EL1 Sample Register on page 397|
|`0x60C`|PMCID2SSR|RO|32|28.4 PMCID2SSR, PMU Snapshot CONTEXTIDR_EL2 Sample Register on page 397|
|`0x610`|PMSSSR|RO|32|28.5 PMSSSR, PMU Snapshot Status Register on page 398|
|`0x614`|PMOVSSR|RO|32|28.6 PMOVSSR, PMU Snapshot Overﬂow Status Register on page 399|
|`0x618`|PMCCNTSR_LO|RO|32|28.7 PMCCNTSR, PMU Snapshot Cycle Counter Register on page 399|
|`0x61C`|PMCCNTSR_HI|RO|32|32|
|`0x620` + 4×n|PMEVCNTSRn|RO|32|28.8 PMEVCNTSRn, PMU Snapshot Cycle Counter Registers 0-5 on page 400|
|`0x6F0`|PMSSCR|WO|32|28.9 PMSSCR, PMU Snapshot Capture Register on page 400|

## 28.3 PMCIDSSR, PMU Snapshot CONTEXTIDR_EL1 Sample Register

Figure 28-1: PMPCSSR bit assignments

63
0

56 55
60
61
62

PC
EL

NS

RES0

NS, [63]

Non-secure sample.

EL, [62:61]

Exception level sample.

RES0, [60:56]

Reserved, RES0.

PC, [55:0]

Sampled PC.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints
Any access to PMPCSSR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

28.3 PMCIDSSR, PMU Snapshot CONTEXTIDR_EL1
Sample Register

The PMCIDSSR holds the same value as the PMCID1SR register at the time of the snapshot.

Conﬁgurations
There are no conﬁguration notes.

Usage constraints
Any access to PMCIDSSR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

## 28.4 PMCID2SSR, PMU Snapshot CONTEXTIDR_EL2 Sample Register

## 28.5 PMSSSR, PMU Snapshot Status Register

28.4 PMCID2SSR, PMU Snapshot CONTEXTIDR_EL2
Sample Register

The PMCID2SSR holds the same value as the PMCID2SR register at the time of the snapshot.

Conﬁgurations
There are no conﬁguration notes.

Usage constraints
Any access to PMCID2SSR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

The PMSSSR holds status information about the captured counters.

Bit ﬁeld descriptions
The PMSSSR is a 32-bit read-only register.

Figure 28-2: PMSSSR bit assignments

31
0
1

NC

RES0

RES0, [31:1]

Reserved, RES0.

NC, [0]

No capture. This bit indicates whether the PMU event counters have been captured. The
possible values are:

0
PMU event counters are captured.
1
PMU event counters are not captured.

If there is a security violation, the core does not capture the event counters. The external
monitor is responsible for keeping track of whether it managed to capture the snapshot
registers from the core.

## 28.6 PMOVSSR, PMU Snapshot Overflow Status Register

## 28.7 PMCCNTSR, PMU Snapshot Cycle Counter Register

This bit does not reﬂect the status of the captured Program Counter Sample registers.

The core resets this bit to 1 by a Warm reset but MPSSSR.NC is overwritten at the ﬁrst
capture.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints
Any access to PMSSSR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

28.6 PMOVSSR, PMU Snapshot Overﬂow Status Register

The PMOVSSR is a captured copy of PMOVSR.

Once it is captured, the value in PMOVSSR is unaﬀected by writes to PMOVSSET_EL0 and
PMOVSCLR_EL0.

Conﬁgurations
There are no conﬁguration notes.

Usage constraints
Any access to PMOVSSR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

The PMCCNTSR is a captured copy of PMCCNTR_EL0.

Once it is captured, the value in PMCCNTSR is unaﬀected by writes to PMCCNTR_EL0 and
PMCR_EL0.C.

Conﬁgurations
There are no conﬁguration notes.

Usage constraints
Any access to PMCCNTSR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

## 28.8 PMEVCNTSRn, PMU Snapshot Cycle Counter Registers 0-5

## 28.9 PMSSCR, PMU Snapshot Capture Register

- DoubleLockStatus() == TRUE.

28.8 PMEVCNTSRn, PMU Snapshot Cycle Counter
Registers 0-5

The PMEVCNTSRn, are captured copies of PMEVCNTRn_EL0, n is 0-5.

When they are captured, the value in PMSSEVCNTRn is unaﬀected by writes to
PMSSEVCNTRn_EL0 and PMCR_EL0.P.

Conﬁgurations
There are no conﬁguration notes.

Usage constraints
Any access to PMSSEVCNTRn returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

The PMSSCR provides a mechanism for software to initiate a sample.

Bit ﬁeld descriptions
The PMSSCR is a 32-bit write-only register.

Figure 28-3: PMSSCR bit assignments

31
0
1

SS

RES0

RES0, [31:1]

Reserved, RES0.

SS, [0]

Capture now. The possible values are:

0
IGNORED.
1
Initiate a capture immediately.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints
Any access to PMSSCR returns an error if any of the following occurs:

- The Core power domain is oﬀ.

- DoubleLockStatus() == TRUE.

# 29. AArch64 AMU registers

## 29.1 AArch64 AMU register summary

## 29.2 AMCNTENCLR_EL0, Activity Monitors Count Enable Clear Register, EL0

This chapter describes the AArch64 AMU registers and shows examples of how to use them.

29.1 AArch64 AMU register summary

The following table gives a summary of the Neoverse™ N1 AMU registers in the AArch64
Execution state.

Table 29-1: AArch64 AMU registers

29.2 AMCNTENCLR_EL0, Activity Monitors Count Enable
Clear Register, EL0

The AMCNTENCLR_EL0 disables the activity monitor counters that are implemented,
AMEVCNTR<0-4>_EL0.

Bit ﬁeld descriptions
The AMCNTENCLR_EL0 is a 32-bit register.

|Name|Width|Reset|Description|
|---|---|---|---|
|AMCNTENCLR_EL0|32|`0x00000000`|29.2 AMCNTENCLR_EL0, Activity Monitors Count Enable Clear Register,<br>EL0 on page 402|
|AMCNTENSET_EL0|32|`0x00000000`|29.3 AMCNTENSET_EL0, Activity Monitors Count Enable Set Register,<br>EL0 on page 404|
|AMCFGR_EL0|32|`0x00003F04`|29.4 AMCFGR_EL0, Activity Monitors Conﬁguration Register, EL0 on page<br>405|
|AMUSERENR_EL0|32|`0x00000000`|29.5 AMUSERENR_EL0, Activity Monitor EL0 Enable access, EL0 on page<br>406|
|AMEVCNTRn_EL0|64|`0x0000000000000000`|29.6 AMEVCNTRn_EL0, Activity Monitor Event Counter Register, EL0 on<br>page 408|
|AMEVTYPERn_EL0|32|The reset value depends on the<br>register:<br>•<br>AMEVTYPER0_EL0 =<br>`0x00000011`.<br>•<br>AMEVTYPER1_EL0 =<br>`0x000000EF`.<br>•<br>AMEVTYPER2_EL0 =<br>`0x00000008`.<br>•<br>AMEVTYPER3_EL0 =<br>`0x000000F0`.<br>•<br>AMEVTYPER4_EL0 =<br>`0x000000F1`.|29.7 AMEVTYPERn_EL0, Activity Monitor Event Type Register, EL0 on<br>page 409|

Figure 29-1: AMCNTENCLR_EL0 bit assignments

31
4
3
0

5

P<n>
RAZ/WI

P<n>, bit[n]

AMEVCNTRn disable bit for n=0-4. The possible values are:

0
When this bit is read, the activity counter n is disabled. When it is
written, it has no eﬀect.
1
When this bit is read, the activity counter n is enabled. When it is
written, it disables the activity counter n.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the AMCNTENCLR_EL0

To access the AMCNTENCLR_EL0:

MRS <Xt>, AMCNTENCLR_EL0 ; Read AMCNTENCLR_EL0 into Xt
MSR AMCNTENCLR_EL0, <Xt> ; Write <Xt> to AMCNTENCLR_EL0

Register access is encoded as follows:

Table 29-2: AMCNTENCLR_EL0 encoding

The AMCNTENCLR_EL0 can be accessed through the external debug interface, oﬀset
0xC20. In this case, it is read-only.

This register is accessible as follows:

Traps and enables

If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are
trapped to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped
to EL3.

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|1111|1001|111|

|EL0|EL1|EL2|EL3|
|---|---|---|---|
|RO|RO|RO|RW|

## 29.3 AMCNTENSET_EL0, Activity Monitors Count Enable Set Register, EL0

If AMUSERENR_EL0.EN is 0, then accesses to this register from EL0 are trapped to EL1.

29.3 AMCNTENSET_EL0, Activity Monitors Count Enable
Set Register, EL0

The AMCNTENSET_EL0 enables the activity monitor counters that are implemented,
AMEVCNTRn (n is 0-4).

Bit ﬁeld descriptions
The AMCNTENSET_EL0 is a 32-bit register.

Figure 29-2: AMCNTENSET_EL0 bit assignments

31
4
3
0

5

P<n>
RAZ/WI

P<n>, bit[n]

AMEVCNTRn enable bit for n=0-4. The possible values are:

0
When this bit is read, the activity counter n is disabled. When it is
written, it has no eﬀect.
1
When this bit is read, the activity counter n is enabled. When it is
written, it enables the activity counter n.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the AMCNTENSET_EL0

To access the AMCNTENSET_EL0:

MRS <Xt>, AMCNTENSET_EL0 ; Read AMCNTENSET_EL0 into Xt
MSR AMCNTENSET_EL0, <Xt> ; Write <Xt> to AMCNTENSET_EL0

Register access is encoded as follows:

Table 29-4: AMCNTENSET_EL0 encoding

The AMCNTENSET_EL0 can be accessed through the external debug interface, oﬀset
0xC00. In this case, it is read-only.

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|1111|1001|110|

## 29.4 AMCFGR_EL0, Activity Monitors Configuration Register, EL0

This register is accessible as follows:

Traps and enables

If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are
trapped to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped
to EL3.

If AMUSERENR_EL0.EN is 0, then accesses to this register from EL0 are trapped to EL1.

29.4 AMCFGR_EL0, Activity Monitors Conﬁguration
Register, EL0

The AMCFGR_EL0 provides information on the number of activity counters that are implemented
and their size.

Bit ﬁeld descriptions
The AMCFGR_EL0 is a 32-bit register.

Figure 29-3: AMCFGR_EL0 bit assignments

31
0

14
8
7

13

N

SIZE

res0

RES0, [31:14]

Reserved, RES0.

SIZE, [13:8]

Size of counters, minus one.

This ﬁeld deﬁnes the size of the largest counter that is implemented by the activity monitors.
In the Armv8-A architecture, the largest counter has 64 bits, therefore the value of this ﬁeld
is 0b111111.

N, [7:0]

Number of activity counters that are implemented, where the number of counters is N+1.
The Neoverse™ N1 core implements ﬁve counters, therefore the value is 0x04.

|EL0|EL1|EL2|EL3|
|---|---|---|---|
|RO|RO|RO|RW|

## 29.5 AMUSERENR_EL0, Activity Monitor EL0 Enable access, EL0

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the AMCFGR_EL0

To access the AMCFGR_EL0:

MRS <Xt>, AMCFGR_EL0 ; Read AMCFGR_EL0 into Xt

Register access is encoded as follows:

Table 29-6: AMCFGR_EL0 encoding

The AMCFGR_EL0 can be accessed through the external debug interface, oﬀset 0xE00. In
this case, it is read-only.

This register is accessible as follows:

Traps and enables

If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are
trapped to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped
to EL3.

If AMUSERENR_EL0.EN is 0, then accesses to this register from EL0 are trapped to EL1.

29.5 AMUSERENR_EL0, Activity Monitor EL0 Enable
access, EL0

The AMUSERENR_EL0 enables or disables EL0 access to the activity monitors.

Bit ﬁeld descriptions
The AMUSERENR_EL0 is a 32-bit register.

This register resets to value 0x00000000.

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|1111|1010|110|

|EL0|EL1|EL2|EL3|
|---|---|---|---|
|RO|RO|RO|RO|

Figure 29-4: AMUSERENR_EL0 bit assignments

31
0
1

EN

RES0

RES0, [31:1]

Reserved, RES0.

EN, [0]

Traps EL0 accesses to the activity monitor registers to EL1. The possible values are:

0
EL0 accesses to the activity monitor registers are trapped to EL1.
This is the reset value.
1
EL0 accesses to the activity monitor registers are not trapped to EL1.
Software can access all activity monitor registers at EL0.

Conﬁgurations

There are no conﬁguration notes.

Usage constraints

Accessing the AMUSERENR_EL0

To access the AMUSERENR_EL0:

MRS <Xt>, AMUSERENR_EL0 ; Read AMUSERENR_EL0 into Xt
MSR AMUSERENR_EL0, <Xt> ; Write Xt to AMUSERENR_EL0

Register access is encoded as follows:

Table 29-8: AMUSERENR_EL0 encoding

This register is accessible as follows:

AMUSERENR_EL0 is always RO at EL0 and not trapped by the EN bit.

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|1111|1010|111|

|EL0|EL1|EL2|EL3|
|---|---|---|---|
|RO|RW|RW|RW|

## 29.6 AMEVCNTRn_EL0, Activity Monitor Event Counter Register, EL0

Traps and enables

If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are
trapped to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped
to EL3.

29.6 AMEVCNTRn_EL0, Activity Monitor Event Counter
Register, EL0

The activity counters AMEVCNTRn_EL0 are directly accessible in the memory mapped-view. n is
0-4.

Bit ﬁeld descriptions
The AMEVCNTRn_EL0 is a 64-bit register.

Figure 29-5: AMEVCNTRn_EL0 bit assignments

63
0

ACNT

ACNT, [63:0]

Value of the activity counter AMEVCNTRn_EL0.

This bit ﬁeld resets to zero and the counters monitoring cycle events do not increment when
the core is in WFI or WFE.

Conﬁgurations

Counters might have ﬁxed event allocation.

Usage constraints

Accessing the AMEVCNTRn_EL0

To access the AMEVCNTRn_EL0:

MRS <Xt>, AMEVCNTRn_EL0 ; Read AMEVCNTRn_EL0 into Xt
MSR AMEVCNTRn_EL0, <Xt> ; Write Xt to AMEVCNTRn_EL0

Register access is encoded as follows:

Table 29-10: AMEVCNTRn_EL0 encoding

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|1111|1001|<0-4>|

## 29.7 AMEVTYPERn_EL0, Activity Monitor Event Type Register, EL0

The AMEVCNTRn_EL0[63:32] can also be accessed through the external memory-mapped
interface, oﬀset 0x004+8n. In this case, it is read-only.

The AMEVCNTRn_EL0[31:0] can also be accessed through the external memory-mapped
interface, oﬀset 0x000+8n. In this case, it is read-only.

This register is accessible as follows:

Traps and enables

If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are
trapped to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped
to EL3.

If AMUSERENR_EL0.EN is 0, then accesses to this register from EL0 are trapped to EL1.

29.7 AMEVTYPERn_EL0, Activity Monitor Event Type
Register, EL0

The activity counters AMEVTYPERn_EL0 are directly accessible in the memory mapped view,
where n is 0-4.

Bit ﬁeld descriptions
The AMEVTYPERn_EL0 is a 32-bit register.

Figure 29-6: AMEVTYPERn_EL0 bit assignments

31
0

10

9

evtCount

RES0

RES0, [31:10]

Reserved, RES0.

evtCount, bits[9:0]

The event the counter monitors might be ﬁxed at implementation. In this case, the ﬁeld is
read-only. See 19.4 AMU events on page 327.

|EL0|EL1|EL2|EL3|
|---|---|---|---|
|RO|RO|RO|RW|

Conﬁgurations

Counters might have ﬁxed event allocation.

Traps and enables
If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are trapped
to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped to
EL3.

If AMUSERENR_EL0.EN is 0, then accesses to this register from EL0 are trapped to EL1.

Usage constraints

Accessing the AMEVTYPERn_EL0

To access the AMEVTYPERn_EL0:

MRS <Xt>, AMEVTYPERn_EL0 ; Read AMEVTYPERn_EL0 into Xt
MSR AMEVTYPERn_EL0, <Xt> ; Write Xt to AMEVTYPERn_EL0

Register access is encoded as follows:

Table 29-12: AMEVTYPER_EL0 encoding

This register can also be accessed through the external memory-mapped interface, oﬀset
0x400+4n. In this case, it is read-only.

This register is accessible as follows:

Traps and enables

If ACTLR_EL2.AMEN is 0, then Non-secure accesses to this register from EL0 and EL1 are
trapped to EL2.

If ACTLR_EL3.AMEN is 0, then accesses to this register from EL0, EL1, and EL2 are trapped
to EL3.

If AMUSERENR_EL0.EN is 0, then accesses to this register from EL0 are trapped to EL1.

|op0|op1|CRn|CRm|op2|
|---|---|---|---|---|
|11|011|1111|1010|<0-4>|

|EL0|EL1|EL2|EL3|
|---|---|---|---|
|RO|RO|RO|RO|

# 30. Memory-mapped AMU registers

## 30.1 Memory-mapped AMU register summary

This chapter describes the memory-mapped AMU registers. The memory-mapped interface
provides read-only access to the AMU registers via the external debug interface.

There are AMU registers that are accessible through the external debug interface.

These registers are listed in the following table. For those registers not described in this chapter,
see the Arm® Architecture Reference Manual for A-proﬁle architecture.

Table 30-1: Memory-mapped AMU register summary

|Offset|Name|Type|Description|
|---|---|---|---|
|`0xC20`|AMCNTENCLR|RO|29.2 AMCNTENCLR_EL0, Activity Monitors<br>Count Enable Clear Register, EL0 on page<br>402|
|`0xC00`|AMCNTENSET|RO|29.3 AMCNTENSET_EL0, Activity Monitors<br>Count Enable Set Register, EL0 on page 404|
|`0xE00`|AMCFGR|RO|29.4 AMCFGR_EL0, Activity Monitors<br>Conﬁguration Register, EL0 on page 405|
|`0x000+8n`|AMEVCNTRn [31:0]|RO|29.6 AMEVCNTRn_EL0, Activity Monitor<br>Event Counter Register, EL0 on page 408|
|`0x004+8n`|AMEVCNTRn [63:32]|RO|29.6 AMEVCNTRn_EL0, Activity Monitor<br>Event Counter Register, EL0 on page 408|
|`0x400+4n`|AMEVTYPERn|RO|29.7 AMEVTYPERn_EL0, Activity Monitor<br>Event Type Register, EL0 on page 409|

# 31. ETM registers

## 31.1 ETM register summary

This chapter describes the ETM registers.

This section summarizes the ETM trace unit registers.

All ETM trace unit registers are 32-bit wide. The description of each register includes its oﬀset
from a base address. The base address is deﬁned by the system integrator when placing the ETM
trace unit in the Debug-APB memory map.

The following table lists all of the ETM trace unit registers.

Table 31-1: ETM trace unit register summary

|Offset|Name|Type|Reset|Description|
|---|---|---|---|---|
|`0x004`|TRCPRGCTLR|RW|`0x00000000`|31.61 TRCPRGCTLR, Programming Control Register on page 470|
|`0x00C`|TRCSTATR|RO|`0x00000003`|31.68 TRCSTATR, Status Register on page 477|
|`0x010`|TRCCONFIGR|RW|UNK|31.20 TRCCONFIGR, Trace Conﬁguration Register on page 433|
|`0x018`|TRCAUXCTLR|RW|`0x00000000`|31.5 TRCAUXCTLR, Auxiliary Control Register on page 418|
|`0x020`|TRCEVENTCTL0R|RW|UNK|31.26 TRCEVENTCTL0R, Event Control 0 Register on page 438|
|`0x024`|TRCEVENTCTL1R|RW|UNK|31.27 TRCEVENTCTL1R, Event Control 1 Register on page 440|
|`0x030`|TRCTSCTLR|RW|UNK|31.71 TRCTSCTLR, Global Timestamp Control Register on page 480|
|`0x034`|TRCSYNCPR|RW|UNK|31.69 TRCSYNCPR, Synchronization Period Register on page 478|
|`0x038`|TRCCCCTLR|RW|UNK|31.7 TRCCCCTLR, Cycle Count Control Register on page 421|
|`0x03C`|TRCBBCTLR|RW|UNK|31.6 TRCBBCTLR, Branch Broadcast Control Register on page 420|
|`0x040`|TRCTRACEIDR|RW|UNK|31.70 TRCTRACEIDR, Trace ID Register on page 479|
|`0x080`|TRCVICTLR|RW|UNK|31.72 TRCVICTLR, ViewInst Main Control Register on page 481|
|`0x084`|TRCVIIECTLR|RW|UNK|31.73 TRCVIIECTLR, ViewInst Include-Exclude Control Register on page 483|
|`0x088`|TRCVISSCTLR|RW|UNK|31.74 TRCVISSCTLR, ViewInst Start-Stop Control Register on page 484|
|`0x100`|TRCSEQEVR0|RW|UNK|31.63 TRCSEQEVRn, Sequencer State Transition Control Registers 0-2 on page<br>472|
|`0x104`|TRCSEQEVR1|RW|UNK|31.63 TRCSEQEVRn, Sequencer State Transition Control Registers 0-2 on page<br>472|
|`0x108`|TRCSEQEVR2|RW|UNK|31.63 TRCSEQEVRn, Sequencer State Transition Control Registers 0-2 on page<br>472|
|`0x118`|TRCSEQRSTEVR|RW|UNK|31.64 TRCSEQRSTEVR, Sequencer Reset Control Register on page 473|
|`0x11C`|TRCSEQSTR|RW|UNK|31.65 TRCSEQSTR, Sequencer State Register on page 474|
|`0x120`|TRCEXTINSELR|RW|UNK|31.28 TRCEXTINSELR, External Input Select Register on page 441|
|`0x140`|TRCCNTRLDVR0|RW|UNK|31.18 TRCCNTRLDVRn, Counter Reload Value Registers 0-1 on page 431|
|`0x144`|TRCCNTRLDVR1|RW|UNK|31.18 TRCCNTRLDVRn, Counter Reload Value Registers 0-1 on page 431|
|`0x150`|TRCCNTCTLR0|RW|UNK|31.16 TRCCNTCTLR0, Counter Control Register 0 on page 428|

|Offset|Name|Type|Reset|Description|
|---|---|---|---|---|
|`0x154`|TRCCNTCTLR1|RW|UNK|31.17 TRCCNTCTLR1, Counter Control Register 1 on page 429|
|`0x160`|TRCCNTVR0|RW|UNK|31.19 TRCCNTVRn, Counter Value Registers 0-1 on page 432|
|`0x164`|TRCCNTVR1|RW|UNK|31.19 TRCCNTVRn, Counter Value Registers 0-1 on page 432|
|`0x180`|TRCIDR8|RO|`0x00000000`|31.35 TRCIDR8, ID Register 8 on page 452|
|`0x184`|TRCIDR9|RO|`0x00000000`|31.36 TRCIDR9, ID Register 9 on page 452|
|`0x188`|TRCIDR10|RO|`0x00000000`|31.37 TRCIDR10, ID Register 10 on page 453|
|`0x18C`|TRCIDR11|RO|`0x00000000`|31.38 TRCIDR11, ID Register 11 on page 453|
|`0x190`|TRCIDR12|RO|`0x00000000`|31.39 TRCIDR12, ID Register 12 on page 454|
|`0x194`|TRCIDR13|RO|`0x00000000`|31.40 TRCIDR13, ID Register 13 on page 454|
|`0x1C0`|TRCIMSPEC0|RW|`0x00000000`|31.41 TRCIMSPEC0, IMPLEMENTATION SPECIFIC Register 0 on page 455|
|`0x1E0`|TRCIDR0|RO|`0x28000EA1`|31.29 TRCIDR0, ID Register 0 on page 442|
|`0x1E4`|TRCIDR1|RO|`0x4100F425`|31.30 TRCIDR1, ID Register 1 on page 444|
|`0x1E8`|TRCIDR2|RO|`0x20001088`|31.31 TRCIDR2, ID Register 2 on page 445|
|`0x1EC`|TRCIDR3|RO|`0x017B0004`|31.32 TRCIDR3, ID Register 3 on page 447|
|`0x1F0`|TRCIDR4|RO|`0x11170004`|31.33 TRCIDR4, ID Register 4 on page 449|
|`0x1F4`|TRCIDR5|RO|`0x284708AD`|31.34 TRCIDR5, ID Register 5 on page 450|
|`0x200`|TRCRSCTLRn|RW|UNK|31.62 TRCRSCTLRn, Resource Selection Control Registers 2-15 on page 471,<br>n is 2, 15|
|`0x280`|TRCSSCCR0|RW|UNK|31.66 TRCSSCCR0, Single-Shot Comparator Control Register 0 on page 475|
|`0x2A0`|TRCSSCSR0|RW|UNK|31.67 TRCSSCSR0, Single-Shot Comparator Status Register 0 on page 476|
|`0x300`|TRCOSLAR|WO|`0x00000001`|31.51 TRCOSLAR, OS Lock Access Register on page 462|
|`0x304`|TRCOSLSR|RO|`0x0000000A`|31.52 TRCOSLSR, OS Lock Status Register on page 463|
|`0x310`|TRCPDCR|RW|`0x00000000`|31.53 TRCPDCR, Power Down Control Register on page 464|
|`0x314`|TRCPDSR|RO|`0x00000023`|31.54 TRCPDSR, Power Down Status Register on page 465|
|`0x400`|TRCACVRn|RW|UNK|31.3 TRCACVRn, Address Comparator Value Registers 0-7 on page 416|
|`0x480`|TRCACATRn|RW|UNK|31.2 TRCACATRn, Address Comparator Access Type Registers 0-7 on page<br>414|
|`0x600`|TRCCIDCVR0|RW|UNK|31.9 TRCCIDCVR0, Context ID Comparator Value Register 0 on page 422|
|`0x640`|TRCVMIDCVR0|RW|UNK|31.75 TRCVMIDCVR0, VMID Comparator Value Register 0 on page 485|
|`0x680`|TRCCIDCCTLR0|RW|UNK|31.8 TRCCIDCCTLR0, Context ID Comparator Control Register 0 on page 422|
|`0x688`|TRCVMIDCCTLR0|RW|UNK|31.76 TRCVMIDCCTLR0, Virtual context identiﬁer Comparator Control Register<br>0 on page 485|
|`0xEDC`|TRCITMISCOUT|WO|UNK|31.48 TRCITMISCOUT, Trace Integration Miscellaneous Outputs Register on<br>page 460|
|`0xEE0`|TRCITMISCIN|RO|UNK|31.47 TRCITMISCIN, Trace Integration Miscellaneous Input Register on page<br>460|
|`0xEEC`|TRCITATBDATA0|WO|UNK|31.45 TRCITATBDATA0, Trace Integration Test ATB Data Register 0 on page<br>458|
|`0xEF0`|TRCITATBCTR2|RO|UNK|31.44 TRCITATBCTR2, Trace Integration Test ATB Control Register 2 on page<br>457|
|`0xEF4`|TRCITATBCTR1|WO|UNK|31.43 TRCITATBCTR1, Trace Integration Test ATB Control Register 1 on page<br>456|

## 31.2 TRCACATRn, Address Comparator Access Type Registers 0-7

31.2 TRCACATRn, Address Comparator Access Type
Registers 0-7

The TRCACATRn registers control the access for the corresponding address comparators.

Bit ﬁeld descriptions
The TRCACATRn registers are 64-bit registers.

|Offset|Name|Type|Reset|Description|
|---|---|---|---|---|
|`0xEF8`|TRCITATBCTR0|WO|UNK|31.42 TRCITATBCTR0, Trace Integration Test ATB Control Register 0 on page<br>456|
|`0xF00`|TRCITCTRL|RW|`0x00000000`|31.46 TRCITCTRL, Trace Integration Mode Control register on page 459|
|`0xFA0`|TRCCLAIMSET|RW|UNK|31.15 TRCCLAIMSET, Claim Tag Set Register on page 427|
|`0xFA4`|TRCCLAIMCLR|RW|`0x00000000`|31.14 TRCCLAIMCLR, Claim Tag Clear Register on page 426|
|`0xFA8`|TRCDEVAFF0|RO|UNK|31.21 TRCDEVAFF0, Device Aﬃnity Register 0 on page 435|
|`0xFAC`|TRCDEVAFF1|RO|UNK|31.22 TRCDEVAFF1, Device Aﬃnity Register 1 on page 436|
|`0xFB0`|TRCLAR|WO|UNK|31.49 TRCLAR, Software Lock Access Register on page 461|
|`0xFB4`|TRCLSR|RO|`0x00000000`|31.50 TRCLSR, Software Lock Status Register on page 461|
|`0xFB8`|TRCAUTHSTATUS|RO|UNK|31.4 TRCAUTHSTATUS, Authentication Status Register on page 417|
|`0xFBC`|TRCDEVARCH|RO|`0x47724A13`|31.23 TRCDEVARCH, Device Architecture Register on page 436|
|`0xFC8`|TRCDEVID|RO|`0x00000000`|31.24 TRCDEVID, Device ID Register on page 437|
|`0xFCC`|TRCDEVTYPE|RO|`0x00000013`|31.25 TRCDEVTYPE, Device Type Register on page 437|
|`0xFE0`|TRCPIDR0|RO|`0x0000000C`|31.55 TRCPIDR0, ETM Peripheral Identiﬁcation Register 0 on page 466|
|`0xFE4`|TRCPIDR1|RO|`0x000000BD`|31.56 TRCPIDR1, ETM Peripheral Identiﬁcation Register 1 on page 467|
|`0xFE8`|TRCPIDR2|RO|`0x0000005B`|31.57 TRCPIDR2, ETM Peripheral Identiﬁcation Register 2 on page 467|
|`0xFEC`|TRCPIDR3|RO|`0x00000000`|31.58 TRCPIDR3, ETM Peripheral Identiﬁcation Register 3 on page 468|
|`0xFD0`|TRCPIDR4|RO|`0x00000004`|31.59 TRCPIDR4, ETM Peripheral Identiﬁcation Register 4 on page 469|
|`0xFD4` -<br>`0xFDC`|TRCPIDRn|RO|`0x00000000`|31.60 TRCPIDRn, ETM Peripheral Identiﬁcation Registers 5-7 on page 470|
|`0xFF0`|TRCCIDR0|RO|`0x0000000D`|31.10 TRCCIDR0, ETM Component Identiﬁcation Register 0 on page 423|
|`0xFF4`|TRCCIDR1|RO|`0x00000090`|31.11 TRCCIDR1, ETM Component Identiﬁcation Register 1 on page 424|
|`0xFF8`|TRCCIDR2|RO|`0x00000005`|31.12 TRCCIDR2, ETM Component Identiﬁcation Register 2 on page 425|
|`0xFFC`|TRCCIDR3|RO|`0x000000B1`|31.13 TRCCIDR3, ETM Component Identiﬁcation Register 3 on page 425|

Figure 31-1: TRCACATRn bit assignments

63
16 15
12 11
8
7
3
2
1
0

TYPE

CONTEXTTYPE
EXLEVEL_NS
RES0

EXLEVEL_S

RES0, [63:16]

RES0
Reserved

EXLEVEL_NS, [15:12]

Each bit controls whether a comparison can occur in Non-secure state for the corresponding
Exception level. The possible values are:

0
The trace unit can perform a comparison, in Non-secure state, for
Exception level n.
1
The trace unit does not perform a comparison, in Non-secure state,
for Exception level n.

The Exception levels are:

Bit[12]
Exception level 0
Bit[13]
Exception level 1
Bit[14]
Exception level 2
Bit[15]
Always RES0

EXLEVEL_S, [11:8]

Each bit controls whether a comparison can occur in Secure state for the corresponding
Exception level. The possible values are:

0
The trace unit can perform a comparison, in Secure state, for
Exception level n.
1
The trace unit does not perform a comparison, in Secure state, for
Exception level n.

The Exception levels are:

Bit[8]
Exception level 0
Bit[9]
Exception level 1
Bit[10]
Always RES0
Bit[11]
Exception level 3

RES0, [7:4]

RES0
Reserved

## 31.3 TRCACVRn, Address Comparator Value Registers 0-7

CONTEXT TYPE, [3:2]

Controls whether the trace unit performs a Context ID comparison, a VMID comparison, or
both comparisons:

0b00
The trace unit does not perform a Context ID comparison.
0b01
The trace unit performs a Context ID comparison using the Context
ID comparator that the CONTEXT ﬁeld speciﬁes, and signals a
match if both the Context ID comparator matches and the address
comparator match.
0b10
The trace unit performs a VMID comparison using the VMID
comparator that the CONTEXT ﬁeld speciﬁes, and signals a match if
both the VMID comparator and the address comparator match.
0b11
The trace unit performs a Context ID comparison and a VMID
comparison using the comparators that the CONTEXT ﬁeld speciﬁes,
and signals a match if the Context ID comparator matches, the VMID
comparator matches, and the address comparator matches.

TYPE, [1:0]

Type of comparison:

0b00
Instruction address, RES0

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCACATRn registers can be accessed through the external debug interface, oﬀset
0x480-0x4B8.

The TRCACVRn registers indicate the address for the address comparators.

Bit ﬁeld descriptions
The TRCACVRn registers are 64-bit registers.

Figure 31-2: TRCACVRn bit assignments

63
0

ADDRESS

ADDRESS, [63:0]

The address value to compare against

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

## 31.4 TRCAUTHSTATUS, Authentication Status Register

The TRCACVRn can be accessed through the external debug interface, oﬀset 0x400-0x43C.

The TRCAUTHSTATUS indicates the current level of tracing permitted by the system.

Bit ﬁeld descriptions
The TRCAUTHSTATUS is a 32-bit register.

Figure 31-3: TRCAUTHSTATUS bit assignments

31
1
0

4
3
5
7
6
8

2

SNID

RES0

SID

NSNID

NSID

RES0, [31:8]

RES0
Reserved.

SNID, [7:6]

Secure Non-invasive Debug:

0b10
Secure Non-invasive Debug implemented but disabled.
0b11
Secure Non-invasive Debug implemented and enabled.

SID, [5:4]

Secure Invasive Debug:

0b00
Secure Invasive Debug is not implemented.

NSNID, [3:2]

Non-secure Non-invasive Debug:

0b10
Non-secure Non-invasive Debug implemented but disabled,
NIDEN=0.
0b11
Non-secure Non-invasive Debug implemented and enabled,
NIDEN=1.

NSID, [1:0]

Non-secure Invasive Debug:

## 31.5 TRCAUXCTLR, Auxiliary Control Register

0b00
Non-secure Invasive Debug is not implemented.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCAUTHSTATUS can be accessed through the external debug interface, oﬀset 0xFB8.

The TRCAUXCTLR provides IMPLEMENTATION DEFINED conﬁguration and control options.

Bit ﬁeld descriptions

Figure 31-4: TRCAUXCTLR bit assignments

9

31
0
8
7

6
5
4
3
2
1

DBGFLUSHOVERRIDE

RES0

CIFOVERRIDE

INOVFLOWEN

FLUSHOVERRIDE

TSIOVERRIDE

SYNCOVERRIDE
FRSYNCOVFLOW
IDLEACKOVERRIDE
AFREADYOVERRIDE

RES0, [31:9]

RES0
Reserved.

DBGFLUSHOVERRIDE, [8]

Override trace ﬂush on debug state entry. The possible values are:

0
Trace ﬂush on debug state entry is enabled.
1
Trace ﬂush on debug state entry is disabled.

CIFOVERRIDE, [7]

Override core interface register repeater clock enable. The possible values are:

0
Core interface clock gate is enabled.
1
Core interface clock gate is disabled.

INOVFLOWEN, [6]

Allow overﬂows of the core interface buﬀer, removing any rare impact that the trace unit
might have on the core's speculation when enabled. The possible values are:

0
Core interface buﬀer overﬂows are disabled.
1
Core interface buﬀer overﬂows are enabled.

When this bit is set to 1, the trace start/stop logic might deviate from architecturally-
speciﬁed behavior.

FLUSHOVERRIDE, [5]

Override ETM ﬂush behavior. The possible values are:

0
ETM trace unit FIFO is ﬂushed and ETM trace unit enters idle state
when DBGEN or NIDEN is LOW.
1
ETM trace unit FIFO is not ﬂushed and ETM trace unit does not
enter idle state when DBGEN or NIDEN is LOW.

When this bit is set to 1, the trace unit behavior deviates from architecturally-speciﬁed
behavior.

TSIOVERRIDE, [4]

Override TS packet insertion behavior. The possible values are:

0
Timestamp packets are inserted into FIFO only when trace activity is
LOW.
1
Timestamp packets are inserted into FIFO irrespective of trace
activity.

SYNCOVERRIDE, [3]

Override SYNC packet insertion behavior. The possible values are:

0
SYNC packets are inserted into FIFO only when trace activity is low.
1
SYNC packets are inserted into FIFO irrespective of trace activity.

FRSYNCOVFLOW, [2]

Force overﬂows to output synchronization packets. The possible values are:

0
No FIFO overﬂow when SYNC packets are delayed.
1
Forces FIFO overﬂow when SYNC packets are delayed.

When this bit is set to 1, the trace unit behavior deviates from architecturally-speciﬁed
behavior.

IDLEACKOVERRIDE, [1]

Force ETM idle acknowledge. The possible values are:

0
ETM trace unit idle acknowledge is asserted only when the ETM
trace unit is in idle state.
1
ETM trace unit idle acknowledge is asserted irrespective of the ETM
trace unit idle state.

When this bit is set to 1, trace unit behavior deviates from architecturally-speciﬁed behavior.

## 31.6 TRCBBCTLR, Branch Broadcast Control Register

AFREADYOVERRIDE, [0]

Force assertion of AFREADYM output. The possible values are:

0
ETM trace unit AFREADYM output is asserted only when the ETM
trace unit is in idle state or when all the trace bytes in FIFO before a
ﬂush request are output.
1
ETM trace unit AFREADYM output is always asserted HIGH.

When this bit is set to 1, trace unit behavior deviates from architecturally-speciﬁed behavior.

The TRCAUXCTLR can be accessed through the internal memory-mapped interface and the
external debug interface, oﬀset 0x018.

Conﬁgurations

Available in all conﬁgurations.

The TRCBBCTLR controls how branch broadcasting behaves, and allows branch broadcasting to be
enabled for certain memory regions.

Bit ﬁeld descriptions
The TRCBBCTLR is a 32-bit register.

Figure 31-5: TRCBBCTLR bit assignments

31
0
8
7

9

RANGE

MODE
RES0

RES0, [31:9]

RES0
Reserved

MODE, [8]

Mode bit:

0
Exclude mode. Branch broadcasting is not enabled in the address
range that RANGE deﬁnes.

If RANGE==0 then branch broadcasting is enabled for the entire
memory map.

## 31.7 TRCCCCTLR, Cycle Count Control Register

1
Include mode. Branch broadcasting is enabled in the address range
that RANGE deﬁnes.

If RANGE==0 then the behavior of the trace unit is CONSTRAINED
UNPREDICTABLE. That is, the trace unit might or might not consider
any instructions to be in a branch broadcast region.

RANGE, [7:0]

Address range ﬁeld.

Selects which address range comparator pairs are in use with branch broadcasting. Each bit
represents an address range comparator pair, so bit[n] controls the selection of address range
comparator pair n. If bit[n] is:

0
The address range that address range comparator pair n deﬁnes, is
not selected.
1
The address range that address range comparator pair n deﬁnes, is
selected.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCBBCTLR can be accessed through the external debug interface, oﬀset 0x03C.

The TRCCCCTLR sets the threshold value for cycle counting.

Bit ﬁeld descriptions
The TRCCCCTLR is a 32-bit register.

Figure 31-6: TRCCCCTLR bit assignments

31
0

12 11

THRESHOLD

RES0

RES0, [31:12]

RES0
Reserved.

THRESHOLD, [11:0]

Instruction trace cycle count threshold.

## 31.8 TRCCIDCCTLR0, Context ID Comparator Control Register 0

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCCCTLR can be accessed through the external debug interface, oﬀset 0x038.

31.8 TRCCIDCCTLR0, Context ID Comparator Control
Register 0

The TRCCIDCCTLR0 controls the mask value for the context ID comparators.

Bit ﬁeld descriptions
The TRCCIDCCTLR0 is a 32-bit register.

Figure 31-7: TRCCIDCCTLR0 bit assignments

31
0
4

3

COMP0

RES0

RES0, [31:4]

RES0
Reserved.

COMP0, [3:0]

Controls the mask value that the trace unit applies to TRCCIDCVR0. Each bit in this ﬁeld
corresponds to a byte in TRCCIDCVR0. When a bit is:

0
The trace unit includes the relevant byte in TRCCIDCVR0 when it
performs the Context ID comparison.
1
The trace unit ignores the relevant byte in TRCCIDCVR0 when it
performs the Context ID comparison.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCIDCCTLR0 can be accessed through the external debug interface, oﬀset 0x680.

## 31.9 TRCCIDCVR0, Context ID Comparator Value Register 0

## 31.10 TRCCIDR0, ETM Component Identification Register 0

31.9 TRCCIDCVR0, Context ID Comparator Value Register
0

The TRCCIDCVR0 contains a Context ID value.

Bit ﬁeld descriptions
The TRCCIDCVR0 is a 64-bit register.

Figure 31-8: TRCCIDCVR0 bit assignments

63
0

31
32

Value

RES0

RES0, [63:32]

RES0
Reserved.

VALUE, [31:0]

The data value to compare against.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCIDCVR0 can be accessed through the external debug interface, oﬀset 0x600.

31.10 TRCCIDR0, ETM Component Identiﬁcation Register
0

The TRCCIDR0 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCCIDR0 is a 32-bit register.

## 31.11 TRCCIDR1, ETM Component Identification Register 1

Figure 31-9: TRCCIDR0 bit assignments

7
8

31
0

PRMBL_0

RES0

RES0, [31:8]

RES0
Reserved.

PRMBL_0, [7:0]

0x0D
Preamble byte 0.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCIDR0 can be accessed through the external debug interface, oﬀset 0xFF0.

31.11 TRCCIDR1, ETM Component Identiﬁcation Register
1

The TRCCIDR1 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCCIDR1 is a 32-bit register.

Figure 31-10: TRCCIDR1 bit assignments

31
0

7
8
3
4

CLASS

PRMBL_1

RES0

RES0, [31:8]

RES0
Reserved.

## 31.12 TRCCIDR2, ETM Component Identification Register 2

CLASS, [7:4]

0x9
Debug component.

PRMBL_1, [3:0]

0x0
Preamble byte 1.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCIDR1 can be accessed through the external debug interface, oﬀset 0xFF4.

31.12 TRCCIDR2, ETM Component Identiﬁcation Register
2

The TRCCIDR2 provides information to identify a CTI component.

Bit ﬁeld descriptions
The TRCCIDR2 is a 32-bit register.

Figure 31-11: TRCCIDR2 bit assignments

7
8

31
0

PRMBL_2

RES0

RES0, [31:8]

RES0
Reserved.

PRMBL_2, [7:0]

0x05
Preamble byte 2.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCIDR2 can be accessed through the external debug interface, oﬀset 0xFF8.

## 31.13 TRCCIDR3, ETM Component Identification Register 3

## 31.14 TRCCLAIMCLR, Claim Tag Clear Register

31.13 TRCCIDR3, ETM Component Identiﬁcation Register
3

The TRCCIDR3 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCCIDR3 is a 32-bit register.

Figure 31-12: TRCCIDR3 bit assignments

7
8

31
0

PRMBL_3

RES0

RES0, [31:8]

RES0
Reserved.

PRMBL_3, [7:0]

0xB1
Preamble byte 3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCIDR3 can be accessed through the external debug interface, oﬀset 0xFFC.

The TRCCLAIMCLR clears bits in the claim tag and determines the current value of the claim tag.

Bit ﬁeld descriptions
The TRCCLAIMCLR is a 32-bit register.

## 31.15 TRCCLAIMSET, Claim Tag Set Register

Figure 31-13: TRCCLAIMCLR bit assignments

31
4
3
0

CLR

RES0

RES0, [31:4]

RES0
Reserved.

CLR, [3:0]

On reads, for each bit:

0
Claim tag bit is not set.
1
Claim tag bit is set.

On writes, for each bit:

0
Has no eﬀect.
1
Clears the relevant bit of the claim tag.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCLAIMCLR can be accessed through the external debug interface, oﬀset 0xFA4.

The TRCCLAIMSET sets bits in the claim tag and determines the number of claim tag bits
implemented.

Bit ﬁeld descriptions
The TRCCLAIMSET is a 32-bit register.

Figure 31-14: TRCCLAIMSET bit assignments

31
4
3
0

SET

RES0

## 31.16 TRCCNTCTLR0, Counter Control Register 0

RES0, [31:4]

RES0
Reserved.

SET, [3:0]

On reads, for each bit:

0
Claim tag bit is not implemented.
1
Claim tag bit is implemented.

On writes, for each bit:

0
Has no eﬀect.
1
Sets the relevant bit of the claim tag.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCLAIMSET can be accessed through the external debug interface, oﬀset 0xFA0.

The TRCCNTCTLR0 controls the counter.

Bit ﬁeld descriptions
The TRCCNTCTLR0 is a 32-bit register.

Figure 31-15: TRCCNTCTLR0 bit assignments

31
16 15 14
12 11
8
7
6
4
3
0

17

RLDSEL
CNTSEL

RLDSELF
CNTTYPE

RLDTYPE
RES0

RES0, [31:17]

RES0
Reserved.

RLDSELF, [16]

Deﬁnes whether the counter reloads when it reaches zero:

0
The counter does not reload when it reaches zero. The counter only
reloads based on RLDTYPE and RLDSEL.

1
The counter reloads when it reaches zero and the resource selected
by CNTTYPE and CNTSEL is also active. The counter also reloads
based on RLDTYPE and RLDSEL.

RLDTYPE, [15]

Selects the resource type for the reload:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [14:12]

RES0
Reserved.

RLDSEL, [11:8]

Selects the resource number, based on the value of RLDTYPE:

When RLDTYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When RLDTYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

CNTTYPE, [7]

Selects the resource type for the counter:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [6:4]

RES0
Reserved.

CNTSEL, [3:0]

Selects the resource number, based on the value of CNTTYPE:

When CNTTYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When CNTTYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by
bits[2:0].

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCNTCTLR0 can be accessed through the external debug interface, oﬀset 0x150.

## 31.17 TRCCNTCTLR1, Counter Control Register 1

The TRCCNTCTLR1 controls the counter.

Bit ﬁeld descriptions
The TRCCNTCTLR1 is a 32-bit register.

Figure 31-16: TRCCNTCTLR1 bit assignments

31
16 15 14
12 11
8
7
6
4
3
0

17
18

RLDSEL
CNTSEL

CNTCHAIN

CNTTYPE

RES0

RLDSELF

RLDTYPE

RES0, [31:18]

RES0
Reserved.

CNTCHAIN, [17]

Deﬁnes whether the counter decrements when the counter reloads. This enables two
counters to be used in combination to provide a larger counter:

0
The counter operates independently from the counter. The counter
only decrements based on CNTTYPE and CNTSEL.
1
The counter decrements when the counter reloads. The counter also
decrements when the resource selected by CNTTYPE and CNTSEL is
active.

RLDSELF, [16]

Deﬁnes whether the counter reloads when it reaches zero:

0
The counter does not reload when it reaches zero. The counter only
reloads based on RLDTYPE and RLDSEL.
1
The counter reloads when it is zero and the resource selected by
CNTTYPE and CNTSEL is also active. The counter also reloads based
on RLDTYPE and RLDSEL.

RLDTYPE, [15]

Selects the resource type for the reload:

0
Single selected resource.
1
Boolean combined resource pair.

## 31.18 TRCCNTRLDVRn, Counter Reload Value Registers 0-1

RES0, [14:12]

RES0
Reserved.

RLDSEL, [11:8]

Selects the resource number, based on the value of RLDTYPE:

When RLDTYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When RLDTYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

CNTTYPE, [7]

Selects the resource type for the counter:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [6:4]

RES0
Reserved.

CNTSEL, [3:0]

Selects the resource number, based on the value of CNTTYPE:

When CNTTYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When CNTTYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by
bits[2:0].

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCNTCTLR1 can be accessed through the external debug interface, oﬀset 0x154.

31.18 TRCCNTRLDVRn, Counter Reload Value Registers
0-1

The TRCCNTRLDVRn registers deﬁne the reload value for the counter.

Bit ﬁeld descriptions
The TRCCNTRLDVRn registers are 32-bit registers.

## 31.19 TRCCNTVRn, Counter Value Registers 0-1

Figure 31-17: TRCCNTRLDVRn bit assignments

31
16 15
0

VALUE

RES0

RES0, [31:16]

RES0
Reserved

VALUE, [15:0]

Deﬁnes the reload value for the counter. This value is loaded into the counter each time the
reload event occurs.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCNTRLDVRn registers can be accessed through the external debug interface, oﬀsets:

TRCCNTRLDVR0

0x140

TRCCNTRLDVR1

0x144

The TRCCNTVRn registers contain the current counter value.

Bit ﬁeld descriptions
The TRCCNTVRn registers are 32-bit registers.

Figure 31-18: TRCCNTVRn bit assignments

31
16 15
0

VALUE

RES0

## 31.20 TRCCONFIGR, Trace Configuration Register

RES0, [31:16]

RES0
Reserved

VALUE, [15:0]

Contains the current counter value.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCNTVRn registers can be accessed through the external debug interface, oﬀsets:

TRCCNTVR0

0x160

TRCCNTVR1

0x164

31.20 TRCCONFIGR, Trace Conﬁguration Register

The TRCCONFIGR controls the tracing options.

Bit ﬁeld descriptions
The TRCCONFIGR is a 32-bit register.

Figure 31-19: TRCCONFIGR bit assignments

31
0
12 11 10
8
6
7
5
4
3
2

13
1

14
15
16
17
18

QE
COND

VMID

DV
DA
VMIDOPT

RES1

CID
CCI

RES0

BB

RS

TS
INSTP0

RES0, [31:18]

RES0
Reserved.

DV, [17]

Enables data value tracing. The possible values are:

0
Disables data value tracing.
1
Enables data value tracing.

DA, [16]

Enables data address tracing. The possible values are:

0
Disables data address tracing.
1
Enables data address tracing.

VMIDOPT, [15]

Conﬁgures the Virtual context identiﬁer value that is used by the trace unit, both for trace
generation and in the Virtual context identiﬁer comparators. The possible values are:

0b0
VTTBR_EL2.VMID is used. If the trace unit supports a Virtual context
identiﬁer larger than the VTTBR_EL2.VMID, the upper unused bits
are always zero. If the trace unit supports a Virtual context identiﬁer
larger than 8 bits and if the VTCR_EL2.VS bit forces use of an 8-bit
Virtual context identiﬁer, bits [15:8] of the trace unit Virtual context
identiﬁer are always zero.
0b1
CONTEXTIDR_EL2 is used. TRCIDR2.VMIDOPT indicates whether
this ﬁeld is implemented.

QE, [14:13]

Enables Q element. The possible values are:

0b00
Q elements are disabled.
0b01
Q elements with instruction counts are disabled. Q elements without
instruction counts are disabled.
0b10
Reserved.
0b11
Q elements with and without instruction counts are enabled.

RS, [12]

Enables the return stack. The possible values are:

0
Disables the return stack.
1
Enables the return stack.

TS, [11]

Enables global timestamp tracing. The possible values are:

0
Disables global timestamp tracing.
1
Enables global timestamp tracing.

COND, [10:8]

Enables conditional instruction tracing. The possible values are:

0b000
Conditional instruction tracing is disabled.
0b001
Conditional load instructions are traced.
0b010
Conditional store instructions are traced.
0b011
Conditional load and store instructions are traced.
0b111
All conditional instructions are traced.

VMID, [7]

Enables VMID tracing. The possible values are:

0
Disables VMID tracing.
1
Enables VMID tracing.

CID, [6]

Enables context ID tracing. The possible values are:

0
Disables context ID tracing.
1
Enables context ID tracing.

RES0, [5]

RES0
Reserved.

CCI, [4]

Enables cycle counting instruction trace. The possible values are:

0
Disables cycle counting instruction trace.
1
Enables cycle counting instruction trace.

BB, [3]

Enables branch broadcast mode. The possible values are:

0
Disables branch broadcast mode.
1
Enables branch broadcast mode.

INSTP0, [2:1]

Controls whether load and store instructions are traced as P0 instructions. The possible
values are:

0b00
Load and store instructions are not traced as P0 instructions.
0b01
Load instructions are traced as P0 instructions.
0b10
Store instructions are traced as P0 instructions.
0b11
Load and store instructions are traced as P0 instructions.

RES1, [0]

RES1
Reserved.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCCONFIGR can be accessed through the external debug interface, oﬀset 0x010.

## 31.21 TRCDEVAFF0, Device Affinity Register 0

## 31.22 TRCDEVAFF1, Device Affinity Register 1

## 31.23 TRCDEVARCH, Device Architecture Register

31.21 TRCDEVAFF0, Device Aﬃnity Register 0

The TRCDEVAFF0 provides an additional core identiﬁcation mechanism for scheduling purposes
in a cluster. TRCDEVAFF0 is a read-only copy of MPIDR_EL1[31:0] accessible from the external
debug interface.

Bit ﬁeld descriptions
The TRCDEVAFF0 is a 32-bit register and is a copy of MPIDR_EL1[31:0]. See 13.91 MPIDR_EL1,
Multiprocessor Aﬃnity Register, EL1 on page 231 for full bit ﬁeld descriptions.

31.22 TRCDEVAFF1, Device Aﬃnity Register 1

The TRCDEVAFF1 provides an additional core identiﬁcation mechanism for scheduling purposes
in a cluster. TRCDEVAFF1 is a read-only copy of MPIDR_EL1[63:32] accessible from the external
debug interface.

Bit ﬁeld descriptions
The TRCDEVAFF1 is a 32-bit register and is a copy of MPIDR_EL1[63:32]. See 13.91 MPIDR_EL1,
Multiprocessor Aﬃnity Register, EL1 on page 231 for full bit ﬁeld descriptions.

The TRCDEVARCH identiﬁes the ETM trace unit as an ETMv4 component.

Bit ﬁeld descriptions
The TRCDEVARCH is a 32-bit register.

Figure 31-20: TRCDEVARCH bit assignments

31
21 20 19
16 15
0

ARCHITECT

REVISION
ARCHID

PRESENT

ARCHITECT, [31:21]

Deﬁnes the architect of the component:

0x4
Arm JEP continuation.
0x3B
Arm JEP 106 code.

PRESENT, [20]

Indicates the presence of this register:

## 31.24 TRCDEVID, Device ID Register

## 31.25 TRCDEVTYPE, Device Type Register

0b1
Register is present.

REVISION, [19:16]

Architecture revision:

0x02
Architecture revision 2.

ARCHID, [15:0]

Architecture ID:

0x4A13
ETMv4 component.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCDEVARCH can be accessed through the external debug interface, oﬀset 0xFBC.

31.24 TRCDEVID, Device ID Register

The TRCDEVID indicates the capabilities of the ETM trace unit.

Bit ﬁeld descriptions
The TRCDEVID is a 32-bit register.

Figure 31-21: TRCDEVID bit assignments

31
0

DEVID

DEVID, [31:0]

RAZ. There are no component-deﬁned capabilities.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCDEVID can be accessed through the external debug interface, oﬀset 0xFC8.

The TRCDEVTYPE indicates the type of the component.

Bit ﬁeld descriptions
The TRCDEVTYPE is a 32-bit register.

## 31.26 TRCEVENTCTL0R, Event Control 0 Register

Figure 31-22: TRCDEVTYPE bit assignments

31
0
4
3
7
8

SUB
MAJOR

RES0

RES0, [31:8]

RES0
Reserved.

SUB, [7:4]

The sub-type of the component:

0b0001
Core trace.

MAJOR, [3:0]

The main type of the component:

0b0011
Trace source.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCDEVTYPE can be accessed through the external debug interface, oﬀset 0xFCC.

The TRCEVENTCTL0R controls the tracing of events in the trace stream. The events also drive the
external outputs from the ETM trace unit. The events are selected from the Resource Selectors.

Bit ﬁeld descriptions
The TRCEVENTCTL0R is a 32-bit register.

Figure 31-23: TRCEVENTCTL0R bit assignments

31
16 15 14
12 11
8
7
4
3
0

30
28 27
24 23 22
20 19

6

SEL3
SEL2

SEL1

SEL0

TYPE3

TYPE2
TYPE0
TYPE1

RES0

TYPE3, [31]

Selects the resource type for trace event 3:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [30:28]

RES0
Reserved.

SEL3, [27:24]

Selects the resource number, based on the value of TYPE3:

When TYPE3 is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When TYPE3 is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

TYPE2, [23]

Selects the resource type for trace event 2:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [22:20]

RES0
Reserved.

SEL2, [19:16]

Selects the resource number, based on the value of TYPE2:

When TYPE2 is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When TYPE2 is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

TYPE1, [15]

Selects the resource type for trace event 1:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [14:12]

RES0
Reserved.

SEL1, [11:8]

Selects the resource number, based on the value of TYPE1:

When TYPE1 is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When TYPE1 is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

## 31.27 TRCEVENTCTL1R, Event Control 1 Register

TYPE0, [7]

Selects the resource type for trace event 0:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [6:4]

RES0
Reserved.

SEL0, [3:0]

Selects the resource number, based on the value of TYPE0:

When TYPE0 is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When TYPE0 is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCEVENTCTL0R can be accessed through the external debug interface, oﬀset 0x020.

The TRCEVENTCTL1R controls the behavior of the events that TRCEVENTCTL0R selects.

Bit ﬁeld descriptions
The TRCEVENTCTL1R is a 32-bit register.

Figure 31-24: TRCEVENTCTL1R bit assignments

31
0
4
3
5
8
7

13

12 11 10

EN

ATB

LPOVERRIDE
RES0

RES0, [31:13]

RES0
Reserved.

LPOVERRIDE, [12]

Low-power state behavior override:

0
Low-power state behavior unaﬀected.

## 31.28 TRCEXTINSELR, External Input Select Register

1
Low-power state behavior overridden. The resources and Event trace
generation are unaﬀected by entry to a low-power state.

ATB, [11]

ATB trigger enable:

0
ATB trigger disabled.
1
ATB trigger enabled.

RES0, [10:4]

RES0
Reserved.

EN, [3:0]

One bit per event, to enable generation of an event element in the instruction trace stream
when the selected event occurs:

0
Event does not cause an event element.
1
Event causes an event element.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCEVENTCTL1R can be accessed through the external debug interface, oﬀset 0x024.

The TRCEXTINSELR controls the selectors that choose an external input as a resource in the ETM
trace unit. You can use the Resource Selectors to access these external input resources.

Bit ﬁeld descriptions

Figure 31-25: TRCEXTINSELR bit assignments

SEL3, [31:24]

Selects an event from the external input bus for External Input Resource 3.

|24 31|23 16|15 8|7 0|
|---|---|---|---|
|24<br>31|23<br>16|15<br>8|7<br>0|
|SEL3|SEL2|SEL1|SEL0|

## 31.29 TRCIDR0, ID Register 0

SEL2, [23:16]

Selects an event from the external input bus for External Input Resource 2.

SEL1, [15:8]

Selects an event from the external input bus for External Input Resource 1.

SEL0, [7:0]

Selects an event from the external input bus for External Input Resource 0.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCEXTINSELR can be accessed through the external debug interface, oﬀset 0x120.

The TRCIDR0 returns the tracing capabilities of the ETM trace unit.

Bit ﬁeld descriptions
The TRCIDR0 is a 32-bit register.

Figure 31-26: TRCIDR0 bit assignments

31
0

5
4
3
2
1
10
9
8
7
6
13 12 11
14
15
16
17
24 23
30 29 28

TSSIZE

COMMOPT

QSUPP

INSTP0

QFILT

TRCDATA

CONDTYPE

TRCBB

res1

NUMEVENT

TRCCOND

TRCCCI
res0

RETSTACK

RES0, [31:30]

RES0
Reserved.

COMMOPT, [29]

Indicates the meaning of the commit ﬁeld in some packets:

1
Commit mode 1.

TSSIZE, [28:24]

Global timestamp size ﬁeld:

0b01000
Implementation supports a maximum global timestamp of 64 bits.

RES0, [23:17]

RES0
Reserved.

QSUPP, [16:15]

Indicates Q element support:

0b00
Q elements not supported.

QFILT, [14]

Indicates Q element ﬁltering support:

0b0
Q element ﬁltering not supported.

CONDTYPE, [13:12]

Indicates how conditional results are traced:

0b00
Conditional trace not supported.

NUMEVENT, [11:10]

Number of events supported in the trace, minus 1:

0b11
Four events supported.

RETSTACK, [9]

Return stack support:

1
Return stack implemented.

RES0, [8]

RES0
Reserved.

TRCCCI, [7]

Support for cycle counting in the instruction trace:

1
Cycle counting in the instruction trace is implemented.

TRCCOND, [6]

Support for conditional instruction tracing:

0
Conditional instruction tracing is not supported.

TRCBB, [5]

Support for branch broadcast tracing:

1
Branch broadcast tracing is implemented.

## 31.30 TRCIDR1, ID Register 1

TRCDATA, [4:3]

Conditional tracing ﬁeld:

0b00
Tracing of data addresses and data values is not implemented.

INSTP0, [2:1]

P0 tracing support ﬁeld:

0b00
Tracing of load and store instructions as P0 elements is not
supported.

RES1, [0]

RES1
Reserved.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR0 can be accessed through the external debug interface, oﬀset 0x1E0.

The TRCIDR1 returns the base architecture of the trace unit.

Bit ﬁeld descriptions
The TRCIDR1 is a 32-bit register.

Figure 31-27: TRCIDR1 bit assignments

DESIGNER, [31:24]

Indicates which company designed the trace unit:

0x41
Arm.

RES0, [23:16]

RES0
Reserved.

|31|24|23|16|15 12|11 8|7 4|3 0|
|---|---|---|---|---|---|---|---|
|DESIGNER|DESIGNER||||||REVISION|
|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|TRCARCHMAJ<br>TRCARCHMIN<br>RES0<br>RES1|
|||||||||
|||||||||

## 31.31 TRCIDR2, ID Register 2

RES1, [15:12]

RES1
Reserved.

TRCARCHMAJ, [11:8]

Major trace unit architecture version number:

0x4
ETMv4.

TRCARCHMIN, [7:4]

Minor trace unit architecture version number:

0x2
ETMv4.2

REVISION, [3:0]

Trace unit implementation revision number:

0x5
ETM revision for r4p1

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR1 can be accessed through the external debug interface, oﬀset 0x1E4.

The TRCIDR2 returns the maximum size of six parameters in the trace unit.

The parameters are:

- Cycle counter.

- Data value.

- Data address.

- VMID.

- Context ID.

- Instruction address.

Bit ﬁeld descriptions
The TRCIDR2 is a 32-bit register.

Figure 31-28: TRCIDR2 bit assignments

31
0
25 24
14
15
10 9
5
4

29 28
20 19

CIDSIZE
VMIDSIZE
DASIZE
DVSIZE
CCSIZE

IASIZE

VMIDOPT

RES0

RES0, [31]

RES0

Reserved.

VMIDOPT, [30:29]

Indicates the options for observing the Virtual context identiﬁer:

0x1
VMIDOPT is implemented.

CCSIZE, [28:25]

Size of the cycle counter in bits minus 12:

0x0
The cycle counter is 12 bits in length.

DVSIZE, [24:20]

Data value size in bytes:

0x00
Data value tracing is not implemented.

DASIZE, [19:15]

Data address size in bytes:

0x00
Data address tracing is not implemented.

VMIDSIZE, [14:10]

Virtual Machine ID size:

0x4
Maximum of 32-bit Virtual Machine ID size.

CIDSIZE, [9:5]

Context ID size in bytes:

0x4
Maximum of 32-bit Context ID size.

## 31.32 TRCIDR3, ID Register 3

IASIZE, [4:0]

Instruction address size in bytes:

0x8
Maximum of 64-bit address size.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR2 can be accessed through the external debug interface, oﬀset 0x1E8.

The TRCIDR3 indicates:

- Whether TRCVICTLR is supported.

- The number of cores available for tracing.

- If an Exception level supports instruction tracing.

- The minimum threshold value for instruction trace cycle counting.

- Whether the synchronization period is ﬁxed.

Bit ﬁeld descriptions
The TRCIDR3 is a 32-bit register.

Figure 31-29: TRCIDR3 bit assignments

31
0
25 24
16 15
11
12
5
4

30
28
20 19
23
27 26

CCITMIN

EXLEVEL_S
EXLEVEL_NS

NOOVERFLOW
NUMPROC
SYSSTALL
STALLCTL
SYNCPR
TRCERR

RES0

NOOVERFLOW, [31]

Indicates whether TRCSTALLCTLR.NOOVERFLOW is implemented:

0
TRCSTALLCTLR.NOOVERFLOW is not implemented.

NUMPROC, [30:28]

Indicates the number of cores available for tracing:

0b000
The trace unit can trace one core, ETM trace unit sharing not
supported.

SYSSTALL, [27]

Indicates whether stall control is implemented:

0
The system does not support core stall control.

STALLCTL, [26]

Indicates whether TRCSTALLCTLR is implemented:

0
TRCSTALLCTLR is not implemented.

This ﬁeld is used in conjunction with SYSSTALL.

SYNCPR, [25]

Indicates whether there is a ﬁxed synchronization period:

0
TRCSYNCPR is read/write so software can change the
synchronization period.

TRCERR, [24]

Indicates whether TRCVICTLR.TRCERR is implemented:

1
TRCVICTLR.TRCERR is implemented.

EXLEVEL_NS, [23:20]

Each bit controls whether instruction tracing in Non-secure state is implemented for the
corresponding Exception level:

0b0111
Instruction tracing is implemented for Non-secure EL0, EL1, and EL2
Exception levels.

EXLEVEL_S, [19:16]

Each bit controls whether instruction tracing in Secure state is implemented for the
corresponding Exception level:

0b1011
Instruction tracing is implemented for Secure EL0, EL1, and EL3
Exception levels.

RES0, [15:12]

RES0
Reserved.

CCITMIN, [11:0]

The minimum value that can be programmed in TRCCCCTLR.THRESHOLD:

## 31.33 TRCIDR4, ID Register 4

0x004
Instruction trace cycle counting minimum threshold is 4.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR3 can be accessed through the external debug interface, oﬀset 0x1EC.

The TRCIDR4 indicates the resources available in the ETM trace unit.

Bit ﬁeld descriptions
The TRCIDR4 is a 32-bit register.

Figure 31-30: TRCIDR4 bit assignments

31
0
23
24
16 15
8
7
3
4
27
28
20 19

9
11
12

NUMVMIDC

NUMDVC
NUMPC
NUMSSCC
NUMCIDC

SUPPDAC
RES0

NUMRSPAIRS

NUMACPAIRS

NUMVMIDC, [31:28]

Indicates the number of VMID comparators available for tracing:

0x1
One VMID comparator is available.

NUMCIDC, [27:24]

Indicates the number of CID comparators available for tracing:

0x1
One Context ID comparator is available.

NUMSSCC, [23:20]

Indicates the number of single-shot comparator controls available for tracing:

0x1
One single-shot comparator control is available.

NUMRSPAIRS, [19:16]

Indicates the number of resource selection pairs available for tracing:

0x7
Eight resource selection pairs are available.

NUMPC, [15:12]

Indicates the number of core comparator inputs available for tracing:

## 31.34 TRCIDR5, ID Register 5

0x0
Core comparator inputs are not implemented.

RES0, [11:9]

RES0
Reserved.

SUPPDAC, [8]

Indicates whether the implementation supports data address comparisons: This value is:

0
Data address comparisons are not implemented.

NUMDVC, [7:4]

Indicates the number of data value comparators available for tracing:

0x0
Data value comparators are not implemented.

NUMACPAIRS, [3:0]

Indicates the number of address comparator pairs available for tracing:

0x4
Four address comparator pairs are implemented.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR4 can be accessed through the external debug interface, oﬀset 0x1F0.

The TRCIDR5 returns how many resources the trace unit supports.

Bit ﬁeld descriptions

Figure 31-31: TRCIDR5 bit assignments

31
0
25 24
16 15
8
27
28

30
23 22 21

9
11
12

NUMEXTIN
TRACEIDSIZE

ATBTRIG

NUMEXTINSEL

LPOVERRIDE
NUMSEQSTATE
NUMCNTR
REDFUNCNTR

RES0

REDFUNCNTR, [31]

Reduced Function Counter implemented:

0
Reduced Function Counter not implemented.

NUMCNTR, [30:28]

Number of counters implemented:

0b010
Two counters implemented.

NUMSEQSTATE, [27:25]

Number of sequencer states implemented:

0b100
Four sequencer states implemented.

RES0, [24]

RES0
Reserved.

LPOVERRIDE, [23]

Low-power state override support:

0
Low-power state override support is not implemented.

ATBTRIG, [22]

ATB trigger support:

1
ATB trigger support implemented.

TRACEIDSIZE, [21:16]

Number of bits of trace ID:

0x07
Seven-bit trace ID implemented.

RES0, [15:12]

RES0
Reserved.

NUMEXTINSEL, [11:9]

Number of external input selectors implemented:

0b100
Four external input selectors implemented.

NUMEXTIN, [8:0]

Number of external inputs implemented:

0xAD
173 external inputs implemented.

## 31.35 TRCIDR8, ID Register 8

## 31.36 TRCIDR9, ID Register 9

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR5 can be accessed through the external debug interface, oﬀset 0x1F4.

31.35 TRCIDR8, ID Register 8

The TRCIDR8 returns the maximum speculation depth of the instruction trace stream.

Bit ﬁeld descriptions
The TRCIDR8 is a 32-bit register.

Figure 31-32: TRCIDR8 bit assignments

31
0

MAXSPEC

MAXSPEC, [31:0]

The maximum number of P0 elements in the trace stream that can be Speculative at any
time.

0
Maximum speculation depth of the instruction trace stream.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR8 can be accessed through the external debug interface, oﬀset 0x180.

The TRCIDR9 returns the number of P0 right-hand keys that the trace unit can use.

Bit ﬁeld descriptions
The TRCIDR9 is a 32-bit register.

Figure 31-33: TRCIDR9 bit assignments

31
0

NUMP0KEY

NUMP0KEY, [31:0]

The number of P0 right-hand keys that the trace unit can use.

## 31.37 TRCIDR10, ID Register 10

## 31.38 TRCIDR11, ID Register 11

0
Number of P0 right-hand keys.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR9 can be accessed through the external debug interface, oﬀset 0x184.

31.37 TRCIDR10, ID Register 10

The TRCIDR10 returns the number of P1 right-hand keys that the trace unit can use.

Bit ﬁeld descriptions
The TRCIDR10 is a 32-bit register.

Figure 31-34: TRCIDR10 bit assignments

31
0

NUMP1KEY

NUMP1KEY, [31:0]

The number of P1 right-hand keys that the trace unit can use.

0
Number of P1 right-hand keys.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR10 can be accessed through the external debug interface, oﬀset 0x188.

The TRCIDR11 returns the number of special P1 right-hand keys that the trace unit can use.

Bit ﬁeld descriptions
The TRCIDR11 is a 32-bit register.

Figure 31-35: TRCIDR11 bit assignments

31
0

NUMP1SPC

## 31.39 TRCIDR12, ID Register 12

## 31.40 TRCIDR13, ID Register 13

NUMP1SPC, [31:0]

The number of special P1 right-hand keys that the trace unit can use.

0
Number of special P1 right-hand keys.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR11 can be accessed through the external debug interface, oﬀset 0x18C.

31.39 TRCIDR12, ID Register 12

The TRCIDR12 returns the number of conditional instruction right-hand keys that the trace unit
can use.

Bit ﬁeld descriptions
The TRCIDR12 is a 32-bit register.

Figure 31-36: TRCIDR12 bit assignments

31
0

NUMCONDKEY

NUMCONDKEY, [31:0]

The number of conditional instruction right-hand keys that the trace unit can use, including
normal and special keys.

0
Number of conditional instruction right-hand keys.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR12 can be accessed through the external debug interface, oﬀset 0x190.

The TRCIDR13 returns the number of special conditional instruction right-hand keys that the trace
unit can use.

Bit ﬁeld descriptions
The TRCIDR13 is a 32-bit register.

## 31.41 TRCIMSPEC0, IMPLEMENTATION SPECIFIC Register 0

Figure 31-37: TRCIDR13 bit assignments

31
0

NUMCONDSPC

NUMCONDSPC, [31:0]

The number of special conditional instruction right-hand keys that the trace unit can use,
including normal and special keys.

0
Number of special conditional instruction right-hand keys.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIDR13 can be accessed through the external debug interface, oﬀset 0x194.

31.41 TRCIMSPEC0, IMPLEMENTATION SPECIFIC
Register 0

The TRCIMSPEC0 shows the presence of any IMPLEMENTATION SPECIFIC features, and enables any
features that are provided.

Bit ﬁeld descriptions
The TRCIMSPEC0 is a 32-bit register.

Figure 31-38: TRCIMSPEC0 bit assignments

31
0
4

3

RES0

SUPPORT

RES0, [31:4]

RES0

Reserved.

SUPPORT, [3:0]

0
No IMPLEMENTATION SPECIFIC extensions are supported.

## 31.42 TRCITATBCTR0, Trace Integration Test ATB Control Register 0

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCIMSPEC0 can be accessed through the external debug interface, oﬀset 0x1C0.

System register accesses to the TRCIMSPEC0R will result in an UNDEF exception.

31.42 TRCITATBCTR0, Trace Integration Test ATB Control
Register 0

TRCITATBCTR0 controls signal outputs when TRCITCTRL.IME is set.

Bit ﬁeld descriptions
The TRCITATBCTR0 is a 32-bit register.

Figure 31-39: TRCITATBCTR0 bit assignments

31
0
1
2
7
8
9
10

ATBYTESM[1:0]
AFREADYM

ATVALIDM
RES0

ATBYTESM[1:0], [9:8]

Drives the ATBYTESM outputs.

AFREADYM, [1]

Drives the AFREADYM output.

ATVALIDM, [0]

Drives the ATVALIDM output.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITATBCTR0 register can be accessed through the internal memory-mapped interface and
the external debug interface, oﬀset 0xEF8.

## 31.43 TRCITATBCTR1, Trace Integration Test ATB Control Register 1

## 31.44 TRCITATBCTR2, Trace Integration Test ATB Control Register 2

31.43 TRCITATBCTR1, Trace Integration Test ATB Control
Register 1

TRCITATBCTR1 controls the ATIDM[6:0] signals when TRCITCTRL.IME is set.

Bit ﬁeld descriptions
The TRCITATBCTR1 is a 32-bit register.

Figure 31-40: TRCITATBCTR1 bit assignments

31
0
7
6

ATIDM[6:0]

RES0

ATIDM[6:0], [6:0]

Drives the ATIDM[6:0] outputs.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITATBCTR1 register can be accessed through the internal memory-mapped interface and
the external debug interface, oﬀset 0xEF4.

31.44 TRCITATBCTR2, Trace Integration Test ATB Control
Register 2

TRCITATBCTR2 enables the values of signal inputs to be read when bit[0] of the Integration Mode
Control Register is set.

Bit ﬁeld descriptions
The TRCITATBCTR2 is a 32-bit register.

## 31.45 TRCITATBDATA0, Trace Integration Test ATB Data Register 0

Figure 31-41: TRCITATBCTR2 bit assignments

31
0

1
2

AFVALIDM
ATREADYM

RES0

AFVALIDM, [1]

Returns the value of AFVALIDM input.

ATREADYM, [0]

Returns the value of ATREADYM input. To sample ATREADYM correctly from the processor
signals, ATVALIDM must be asserted.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITATBCTR2 register can be accessed through the internal memory-mapped interface and
the external debug interface, oﬀset 0xEF0.

31.45 TRCITATBDATA0, Trace Integration Test ATB Data
Register 0

TRCITATBDATA0 controls signal outputs when TRCITCTRL.IME is set.

Bit ﬁeld descriptions
The TRCITATBDATA0 is a 32-bit register.

Figure 31-42: TRCITATBDATA0 bit assignments

31
1
0
2
3
4
5

ATDATAM[31]
ATDATAM[23]
ATDATAM[15]

RES0

ATDATAM[7]
ATDATAM[0]

## 31.46 TRCITCTRL, Trace Integration Mode Control register

ATDATAM[31],
[4]

Drives the ATDATAM[31] output.

ATDATAM[23],
[3]

Drives the ATDATAM[23] output.

ATDATAM[15],
[2]

Drives the ATDATAM[15] output.

ATDATAM[7],
[1]

Drives the ATDATAM[7] output.

ATDATAM[0],
[0]

Drives the ATDATAM[0] output.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITATBDATA0 register can be accessed through the internal memory-mapped interface and
the external debug interface, oﬀset 0xEEC.

31.46 TRCITCTRL, Trace Integration Mode Control
register

TRCITCTRL controls whether the trace unit is in integration mode.

Bit ﬁeld descriptions
The TRCITCTRL is a 32-bit RW management register that is reset to zero.

Figure 31-43: TRCITCTRL bit assignments

IME
res0

IME, [0]

Integration mode enable bit. The possible values are:

0b0
The trace unit is not in integration mode.
0b1
The trace unit is in integration mode. This mode enables:

- A debug agent to perform topology detection.

- SoC test software to perform integration testing.

Usage constraints

- Accessible only from the memory-mapped interface or from an external agent such as a
debugger.

## 31.47 TRCITMISCIN, Trace Integration Miscellaneous Input Register

## 31.48 TRCITMISCOUT, Trace Integration Miscellaneous Outputs Register

- If the IME bit changes from one to zero then Arm recommends that the trace unit is
reset. Otherwise the trace unit might generate incorrect or corrupt trace and the trace
unit resources might behave unexpectedly.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITCTRL register can be accessed through the internal memory-mapped interface and the
external debug interface, oﬀset 0xF00.

31.47 TRCITMISCIN, Trace Integration Miscellaneous
Input Register

TRCITMISCIN enables the values of signal inputs to be read when TRCITCTRL.IME is set.

Bit ﬁeld descriptions
The TRCITMISCIN is a 32-bit register.

Figure 31-44: TRCITMISCIN bit assignments

31
0

4
3

ETMEXTIN[3:0]

RES0

ETMEXTIN[3:0], [3:0]

Returns the value of the ETMEXTIN[3:0] inputs.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITMISCIN register can be accessed through the internal memory-mapped interface and
the external debug interface, oﬀset 0xEE0.

31.48 TRCITMISCOUT, Trace Integration Miscellaneous
Outputs Register

TRCITMISCOUT controls signal outputs when TRCITCTRL.IME is set.

Bit ﬁeld descriptions
The TRCITMISCOUT is a 32-bit register.

## 31.49 TRCLAR, Software Lock Access Register

Figure 31-45: TRCITMISCOUT bit assignments

31
0

8
7
12
11

ETMEXTOUT[3:0]

res0

ETMEXTOUT[3:0], [11:8]

Drives the EXTOUT[3:0] outputs.

Bit ﬁelds and details not provided in this description are architecturally deﬁned. See the  Arm®
Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCITMISCOUT register can be accessed through the internal memory-mapped interface and
the external debug interface, oﬀset 0xEDC.

The TRCLAR controls access to registers using the memory-mapped interface, when
PADDRDBG31 is LOW.

Bit ﬁeld descriptions
The TRCLAR is a 32-bit register.

Figure 31-46: TRCLAR bit assignments

31
0

RAZ/WI

RAZ/WI, [31:0]

Read-As-Zero, write ignore.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCLAR can be accessed through the external debug interface, oﬀset 0xFB0.

## 31.50 TRCLSR, Software Lock Status Register

## 31.51 TRCOSLAR, OS Lock Access Register

31.50 TRCLSR, Software Lock Status Register

The TRCLSR determines whether the software lock is implemented, and indicates the current
status of the software lock.

Bit ﬁeld descriptions
The TRCLSR is a 32-bit register.

Figure 31-47: TRCLSR bit assignments

31
0

RAZ/WI

RAZ/WI, [31:0]

Read-As-Zero, write ignore.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCLSR can be accessed through the external debug interface, oﬀset 0xFB4.

The TRCOSLAR sets and clears the OS Lock, to lock out external debugger accesses to the ETM
trace unit registers.

Bit ﬁeld descriptions
The TRCOSLAR is a 32-bit register.

Figure 31-48: TRCOSLAR bit assignments

31
1
0

OSLK
RES0

RES0, [31:1]

RES0
Reserved.

## 31.52 TRCOSLSR, OS Lock Status Register

OSLK, [0]

OS Lock key value:

0
Unlock the OS Lock.
1
Lock the OS Lock.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCOSLAR can be accessed through the external debug interface, oﬀset 0x300.

The TRCOSLSR returns the status of the OS Lock.

Bit ﬁeld descriptions
The TRCOSLSR is a 32-bit register.

Figure 31-49: TRCOSLSR bit assignments

31
1
0

3
2
4

OSLM[1]

RES0

nTT
OSLK
OSLM[0]

RES0, [31:4]

RES0
Reserved.

OSLM[1], [3]

OS Lock model [1] bit. This bit is combined with OSLM[0] to form a two-bit ﬁeld that
indicates the OS Lock model is implemented.

The value of this ﬁeld is always 0b10, indicating that the OS Lock is implemented.

nTT, [2]

This bit is RAZ, that indicates that software must perform a 32-bit write to update the
TRCOSLAR.

OSLK, [1]

OS Lock status bit:

## 31.53 TRCPDCR, Power Down Control Register

0
OS Lock is unlocked.
1
OS Lock is locked.

OSLM[0], [0]

OS Lock model [0] bit. This bit is combined with OSLM[1] to form a two-bit ﬁeld that
indicates the OS Lock model is implemented.

The value of this ﬁeld is always 0b10, indicating that the OS Lock is implemented.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCOSLSR can be accessed through the external debug interface, oﬀset 0x304.

The TRCPDCR request to the system power controller to keep the ETM trace unit powered up.

Bit ﬁeld descriptions
The TRCPDCR is a 32-bit register.

Figure 31-50: TRCPDCR bit assignments

31
4
3
2
0

PU
RES0

RES0, [31:4]

RES0
Reserved.

PU, [3]

Powerup request, to request that power to the ETM trace unit and access to the trace
registers is maintained:

0
Power not requested.
1
Power requested.

This bit is reset to 0 on a trace unit reset.

RES0, [2:0]

RES0
Reserved.

## 31.54 TRCPDSR, Power Down Status Register

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPDCR can be accessed through the external debug interface, oﬀset 0x310.

The TRCPDSR indicates the power down status of the ETM trace unit.

Bit ﬁeld descriptions
The TRCPDSR is a 32-bit register.

Figure 31-51: TRCPDSR bit assignments

31
6
5
4
2
1
0

OSLK
STICKYPD

RES0

POWER

RES0, [31:6]

RES0
Reserved.

OSLK, [5]

OS lock status.

0
The OS Lock is unlocked.
1
The OS Lock is locked.

RES0, [4:2]

RES0
Reserved.

STICKYPD, [1]

Sticky power down state.

0
Trace register power has not been removed since the TRCPDSR was
last read.
1
Trace register power has been removed since the TRCPDSR was last
read.

This bit is set to 1 when power to the ETM trace unit registers is removed, to indicate that
programming state has been lost. It is cleared after a read of the TRCPDSR.

## 31.55 TRCPIDR0, ETM Peripheral Identification Register 0

POWER, [0]

Indicates the ETM trace unit is powered:

0
ETM trace unit is not powered. The trace registers are not accessible
and they all return an error response.
1
ETM trace unit is powered. All registers are accessible.

If a system implementation allows the ETM trace unit to be powered oﬀ independently
of the Debug power domain, the system must handle accesses to the ETM trace unit
appropriately.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPDSR can be accessed through the external debug interface, oﬀset 0x314.

31.55 TRCPIDR0, ETM Peripheral Identiﬁcation Register 0

The TRCPIDR0 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCPIDR0 is a 32-bit register.

Figure 31-52: TRCPIDR0 bit assignments

31
0
7
8

Part_0

RES0

RES0, [31:8]

RES0
Reserved.

Part_0, [7:0]

0x0C

Least signiﬁcant byte of the ETM trace unit part number.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPIDR0 can be accessed through the external debug interface, oﬀset 0xFE0.

## 31.56 TRCPIDR1, ETM Peripheral Identification Register 1

## 31.57 TRCPIDR2, ETM Peripheral Identification Register 2

31.56 TRCPIDR1, ETM Peripheral Identiﬁcation Register 1

The TRCPIDR1 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCPIDR1 is a 32-bit register.

Figure 31-53: TRCPIDR1 bit assignments

31
0
3
4

7
8

DES_0

Part_1

RES0

RES0, [31:8]

RES0
Reserved.

DES_0, [7:4]

0xB
Arm Limited. This is bits[3:0] of JEP106 ID code.

Part_1, [3:0]

0xD
Most signiﬁcant four bits of the ETM trace unit part number.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPIDR1 can be accessed through the external debug interface, oﬀset 0xFE4.

31.57 TRCPIDR2, ETM Peripheral Identiﬁcation Register 2

The TRCPIDR2 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCPIDR2 is a 32-bit register.

## 31.58 TRCPIDR3, ETM Peripheral Identification Register 3

Figure 31-54: TRCPIDR2 bit assignments

31
0
3
4

7
8

2

Revision

DES_1

JEDEC

RES0

RES0, [31:8]

RES0
Reserved.

Revision, [7:4]

0x5
r4p1.

JEDEC, [3]

0b1
RES1. Indicates a JEP106 identity code is used.

DES_1, [2:0]

0b011
Arm Limited. This is bits[6:4] of JEP106 ID code.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPIDR2 can be accessed through the external debug interface, oﬀset 0xFE8.

31.58 TRCPIDR3, ETM Peripheral Identiﬁcation Register 3

The TRCPIDR3 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCPIDR3 is a 32-bit register.

Figure 31-55: TRCPIDR3 bit assignments

31
0
3
4

7
8

REVAND

CMOD

RES0

## 31.59 TRCPIDR4, ETM Peripheral Identification Register 4

RES0, [31:8]

RES0
Reserved.

REVAND, [7:4]

0x0
Part minor revision.

CMOD, [3:0]

0x0
Not customer modiﬁed.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPIDR3 can be accessed through the external debug interface, oﬀset 0xFEC.

31.59 TRCPIDR4, ETM Peripheral Identiﬁcation Register 4

The TRCPIDR4 provides information to identify a trace component.

Bit ﬁeld descriptions
The TRCPIDR4 is a 32-bit register.

Figure 31-56: TRCPIDR4 bit assignments

31
0
3
4

7
8

Size

DES_2

RES0

RES0, [31:8]

RES0
Reserved.

Size, [7:4]

0x0
Size of the component. Log2 the number of 4KB pages from the start
of the component to the end of the component ID registers.

DES_2, [3:0]

0x4
Arm Limited. This is bits[3:0] of the JEP106 continuation code.

## 31.60 TRCPIDRn, ETM Peripheral Identification Registers 5-7

## 31.61 TRCPRGCTLR, Programming Control Register

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCPIDR4 can be accessed through the external debug interface, oﬀset 0xFD0.

31.60 TRCPIDRn, ETM Peripheral Identiﬁcation Registers
5-7

No information is held in the Peripheral ID5, Peripheral ID6, and Peripheral ID7 Registers.

They are reserved for future use and are RES0.

The TRCPRGCTLR enables the ETM trace unit.

Bit ﬁeld descriptions
The TRCPRGCTLR is a 32-bit register.

Figure 31-57: TRCPRGCTLR bit assignments

31
1
0

EN
RES0

RES0, [31:1]

RES0
Reserved.

EN, [0]

Trace program enable:

0
The ETM trace unit interface in the core is disabled, and clocks are
enabled only when necessary to process APB accesses, or drain any
already generated trace. This is the reset value.
1
The ETM trace unit interface in the core is enabled, and clocks are
enabled. Writes to most trace registers are IGNORED.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

## 31.62 TRCRSCTLRn, Resource Selection Control Registers 2-15

The TRCPRGCTLR can be accessed through the external debug interface, oﬀset 0x004.

31.62 TRCRSCTLRn, Resource Selection Control Registers
2-15

The TRCRSCTLRn registers control the trace resources. There are eight resource pairs, the ﬁrst
pair is predeﬁned as {0,1,pair=0} and having reserved select registers. This leaves seven pairs to be
implemented as programmable selectors.

Bit ﬁeld descriptions
The TRCRSCTLRn registers are 32-bit registers.

Figure 31-58: TRCRSCTLRn bit assignments

31
0
16 15
20 19

22 21

18
8
7

SELECT
GROUP

INV
PAIRINV

RES0

RES0, [31:22]

RES0
Reserved

PAIRINV, [21]

Inverts the result of a combined pair of resources.

This bit is implemented only on the lower register for a pair of resource selectors.

INV, [20]

Inverts the selected resources:

0
Resource is not inverted.
1
Resource is inverted.

RES0, [19]

RES0
Reserved

GROUP, [18:16]

Selects a group of resources. See the  Arm® ETM Architecture Speciﬁcation, ETMv4 for more
information.

## 31.63 TRCSEQEVRn, Sequencer State Transition Control Registers 0-2

RES0, [15:8]

RES0
Reserved

SELECT, [7:0]

Selects one or more resources from the required group. One bit is provided for each resource
from the group.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCRSCTLRn registers can be accessed through the external debug interface, oﬀset
0x208-0x023C.

31.63 TRCSEQEVRn, Sequencer State Transition Control
Registers 0-2

The TRCSEQEVRn registers deﬁne the sequencer transitions that progress to the next state or
backwards to the previous state. The ETM trace unit implements a sequencer state machine with
up to four states.

Bit ﬁeld descriptions
The TRCSEQEVRn registers are 32-bit registers.

Figure 31-59: TRCSEQEVRn bit assignments

31
0
16

15
8
7
11
12
14
4
3
6

B SEL
F SEL

B TYPE
F TYPE
RES0

RES0, [31:16]

RES0
Reserved

B TYPE, [15]

Selects the resource type to move backwards to this state from the next state:

0
Single selected resource
1
Boolean combined resource pair

## 31.64 TRCSEQRSTEVR, Sequencer Reset Control Register

RES0, [14:12]

RES0
Reserved

B SEL, [11:8]

Selects the resource number, based on the value of B TYPE:

When B TYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When B TYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

F TYPE, [7]

Selects the resource type to move forwards from this state to the next state:

0
Single selected resource
1
Boolean combined resource pair

RES0, [6:4]

RES0
Reserved

F SEL, [3:0]

Selects the resource number, based on the value of F TYPE:

When F TYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When F TYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSEQEVRn registers can be accessed through the external debug interface, oﬀsets:

TRCSEQEVR0

0x100

TRCSEQEVR1

0x104

TRCSEQEVR2

0x108

The TRCSEQRSTEVR resets the sequencer to state 0.

Bit ﬁeld descriptions
The TRCSEQRSTEVR is a 32-bit register.

## 31.65 TRCSEQSTR, Sequencer State Register

Figure 31-60: TRCSEQRSTEVR bit assignments

31
0

8
7
4
3
6

RESETSEL

RESETTYPE
RES0

RES0, [31:8]

RES0
Reserved.

RESETTYPE, [7]

Selects the resource type to move back to state 0:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [6:4]

RES0
Reserved.

RESETSEL, [3:0]

Selects the resource number, based on the value of RESETTYPE:

When RESETTYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When RESETTYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by
bits[2:0].

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSEQRSTEVR can be accessed through the external debug interface, oﬀset 0x118.

The TRCSEQSTR holds the value of the current state of the sequencer.

Bit ﬁeld descriptions
The TRCSEQSTR is a 32-bit register.

## 31.66 TRCSSCCR0, Single-Shot Comparator Control Register 0

Figure 31-61: TRCSEQSTR bit assignments

31
1
0
2

STATE
RES0

RES0, [31:2]

RES0
Reserved.

STATE, [1:0]

Current sequencer state:

0b00
State 0.
0b01
State 1.
0b10
State 2.
0b11
State 3.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSEQSTR can be accessed through the external debug interface, oﬀset 0x11C.

31.66 TRCSSCCR0, Single-Shot Comparator Control
Register 0

The TRCSSCCR0 controls the single-shot comparator.

Bit ﬁeld descriptions
The TRCSSCCR0 is a 32-bit register.

Figure 31-62: TRCSSCCR0 bit assignments

31
20 19
16 15
8
7
0

24 23
25

ARC
SAC

RST
RES0

## 31.67 TRCSSCSR0, Single-Shot Comparator Status Register 0

RES0, [31:25]

RES0
Reserved.

RST, [24]

Enables the single-shot comparator resource to be reset when it occurs, to enable another
comparator match to be detected:

1
Reset enabled. Multiple matches can occur.

RES0, [23:20]

RES0
Reserved.

ARC, [19:16]

Selects one or more address range comparators for single-shot control.

One bit is provided for each implemented address range comparator.

RES0, [15:8]

RES0
Reserved.

SAC, [7:0]

Selects one or more single address comparators for single-shot control.

One bit is provided for each implemented single address comparator.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSSCCR0 can be accessed through the external debug interface, oﬀset 0x280.

31.67 TRCSSCSR0, Single-Shot Comparator Status
Register 0

The TRCSSCSR0 indicates the status of the single-shot comparator. TRCSSCSR0 is sensitive to
instruction addresses.

Bit ﬁeld descriptions
The TRCSSCSR0 is a 32-bit register.

Figure 31-63: TRCSSCSR0 bit assignments

31 30
3
2
1
0

STATUS
DV
DA
INST
RES0

STATUS, [31]

Single-shot status. This indicates whether any of the selected comparators have matched:

0
Match has not occurred.
1
Match has occurred at least once.

When programming the ETM trace unit, if TRCSSCCRn.RST is b0, the STATUS bit must be
explicitly written to 0 to enable this single-shot comparator control.

RES0, [30:3]

RES0
Reserved.

DV, [2]

Data value comparator support:

0
Single-shot data value comparisons not supported.

DA, [1]

Data address comparator support:

0
Single-shot data address comparisons not supported.

INST, [0]

Instruction address comparator support:

1
Single-shot instruction address comparisons supported.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSSCSR0 can be accessed through the external debug interface, oﬀset 0x2A0.

## 31.68 TRCSTATR, Status Register

## 31.69 TRCSYNCPR, Synchronization Period Register

31.68 TRCSTATR, Status Register

The TRCSTATR indicates the ETM trace unit status.

Bit ﬁeld descriptions
The TRCSTATR is a 32-bit register.

Figure 31-64: TRCSTATR bit assignments

31
1
0

2

PMSTABLE
RES0

IDLE

RES0, [31:2]

RES0
Reserved.

PMSTABLE, [1]

Indicates whether the ETM trace unit registers are stable and can be read:

0
The programmers model is not stable.
1
The programmers model is stable.

IDLE, [0]

Idle status:

0
The ETM trace unit is not idle.
1
The ETM trace unit is idle.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSTATR can be accessed through the external debug interface, oﬀset 0x00C.

The TRCSYNCPR controls how often periodic trace synchronization requests occur.

Bit ﬁeld descriptions
The TRCSYNCPR is a 32-bit register.

## 31.70 TRCTRACEIDR, Trace ID Register

Figure 31-65: TRCSYNCPR bit assignments

31
0
4
5

PERIOD

RES0

RES0, [31:5]

RES0
Reserved.

PERIOD, [4:0]

Deﬁnes the number of bytes of trace between synchronization requests as a total of the
number of bytes generated by both the instruction and data streams. The number of bytes is
2N where N is the value of this ﬁeld:

- A value of zero disables these periodic synchronization requests, but does not disable
other synchronization requests.

- The minimum value that can be programmed, other than zero, is 8, providing a minimum
synchronization period of 256 bytes.

- The maximum value is 20, providing a maximum synchronization period of 220 bytes.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCSYNCPR can be accessed through the external debug interface, oﬀset 0x034.

The TRCTRACEIDR sets the trace ID for instruction trace.

Bit ﬁeld descriptions
The TRCTRACEIDR is a 32-bit register.

Figure 31-66: TRCTRACEIDR bit Assignments

31
0

6
7

TRACEID

RES0

## 31.71 TRCTSCTLR, Global Timestamp Control Register

RES0, [31:7]

RES0
Reserved.

TRACEID, [6:0]

Trace ID value. When only instruction tracing is enabled, this provides the trace ID.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCTRACEIDR can be accessed through the external debug interface, oﬀset 0x040.

The TRCTSCTLR controls the insertion of global timestamps in the trace streams. When the
selected event is triggered, the trace unit inserts a global timestamp into the trace streams. The
event is selected from one of the Resource Selectors.

Bit ﬁeld descriptions
The TRCTSCTLR is a 32-bit register.

Figure 31-67: TRCTSCTLR bit assignments

31
0
8
7

3
6
4

SEL

TYPE
RES0

RES0, [31:8]

RES0
Reserved

TYPE, [7]

Single or combined resource selector.

RES0, [6:4]

RES0
Reserved

SEL, [3:0]

Identiﬁes the resource selector to use.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

## 31.72 TRCVICTLR, ViewInst Main Control Register

The TRCTSCTLR can be accessed through the external debug interface, oﬀset 0x030.

The TRCVICTLR controls instruction trace ﬁltering.

Bit ﬁeld descriptions
The TRCVICTLR is a 32-bit register.

Figure 31-68: TRCVICTLR bit assignments

31
0
7
8
9
10
11
15
12
16
19
20
23
24

6
3
4

SEL

TYPE
RES0

EXLEVEL_S
EXLEVEL_NS

SSSTATUS
TRCRESET
TRCERR

RES0, [31:24]

RES0
Reserved.

EXLEVEL_NS, [23:20]

In Non-secure state, each bit controls whether instruction tracing is enabled for the
corresponding Exception level:

0
Trace unit generates instruction trace, in Non-secure state, for
Exception level n.
1
Trace unit does not generate instruction trace, in Non-secure state,
for Exception level n.

The Exception levels are:

Bit[20]
Exception level 0.
Bit[21]
Exception level 1.
Bit[22]
Exception level 2.
Bit[23]
RAZ/WI. Instruction tracing is not implemented for Exception level 3.

EXLEVEL_S, [19:16]

In Secure state, each bit controls whether instruction tracing is enabled for the corresponding
Exception level:

0
Trace unit generates instruction trace, in Secure state, for Exception
level n.

1
Trace unit does not generate instruction trace, in Secure state, for
Exception level n.

The Exception levels are:

Bit[16]
Exception level 0.
Bit[17]
Exception level 1.
Bit[18]
RAZ/WI. Instruction tracing is not implemented for Exception level 2.
Bit[19]
Exception level 3.

RES0, [15:12]

RES0
Reserved.

TRCERR, [11]

Selects whether a system error exception must always be traced:

0
System error exception is traced only if the instruction or exception
immediately before the system error exception is traced.
1
System error exception is always traced regardless of the value of
ViewInst.

TRCRESET, [10]

Selects whether a reset exception must always be traced:

0
Reset exception is traced only if the instruction or exception
immediately before the reset exception is traced.
1
Reset exception is always traced regardless of the value of ViewInst.

SSSTATUS, [9]

Indicates the current status of the start/stop logic:

0
Start/stop logic is in the stopped state.
1
Start/stop logic is in the started state.

RES0, [8]

RES0
Reserved.

TYPE, [7]

Selects the resource type for the viewinst event:

0
Single selected resource.
1
Boolean combined resource pair.

RES0, [6:4]

RES0
Reserved.

## 31.73 TRCVIIECTLR, ViewInst Include-Exclude Control Register

SEL, [3:0]

Selects the resource number to use for the viewinst event, based on the value of TYPE:

When TYPE is 0, selects a single selected resource from 0-15 deﬁned by bits[3:0].

When TYPE is 1, selects a Boolean combined resource pair from 0-7 deﬁned by bits[2:0].

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCVICTLR can be accessed through the external debug interface, oﬀset 0x080.

31.73 TRCVIIECTLR, ViewInst Include-Exclude Control
Register

The TRCVIIECTLR deﬁnes the address range comparators that control the ViewInst include/
exclude control.

Bit ﬁeld descriptions
The TRCVIIECTLR is a 32-bit register.

Figure 31-69: TRCVIIECTLR bit assignments

31
0

19
20
3
4

16 15

EXCLUDE

INCLUDE

RES0

RES0, [31:20]

RES0
Reserved.

EXCLUDE, [19:16]

Deﬁnes the address range comparators for ViewInst exclude control. One bit is provided for
each implemented Address Range Comparator.

RES0, [15:4]

RES0
Reserved.

INCLUDE, [3:0]

Deﬁnes the address range comparators for ViewInst include control.

## 31.74 TRCVISSCTLR, ViewInst Start-Stop Control Register

Selecting no include comparators indicates that all instructions must be included. The exclude
control indicates which ranges must be excluded.

One bit is provided for each implemented Address Range Comparator.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCVIIECTLR can be accessed through the external debug interface, oﬀset 0x084.

The TRCVISSCTLR deﬁnes the single address comparators that control the ViewInst Start/Stop
logic.

Bit ﬁeld descriptions
The TRCVISSCTLR is a 32-bit register.

Figure 31-70: TRCVISSCTLR bit assignments

RES0

RES0, [31:24]

RES0
Reserved.

STOP, [23:16]

Deﬁnes the single address comparators to stop trace with the ViewInst Start/Stop control.

One bit is provided for each implemented single address comparator.

RES0, [15:8]

RES0
Reserved.

START, [7:0]

Deﬁnes the single address comparators to start trace with the ViewInst Start/Stop control.

One bit is provided for each implemented single address comparator.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

|31|24|23|16|15 8|7|0|
|---|---|---|---|---|---|---|
|||STOP|STOP||START|START|

## 31.75 TRCVMIDCVR0, VMID Comparator Value Register 0

## 31.76 TRCVMIDCCTLR0, Virtual context identifier Comparator Control Register 0

The TRCVISSCTLR can be accessed through the external debug interface, oﬀset 0x088.

31.75 TRCVMIDCVR0, VMID Comparator Value Register
0

The TRCVMIDCVR0 contains a VMID value.

Bit ﬁeld descriptions

Figure 31-71: TRCVMIDCVR0 bit assignments

63
32 31
0

VALUE

RES0

RES0, [63:32]

RES0
Reserved.

VALUE, [31:0]

The VMID value.

The TRCVMIDCVR0 can be accessed through the internal memory-mapped interface and the
external debug interface, oﬀset 0x640.

Usage constraints

Accepts writes only when the trace unit is disabled.

Conﬁgurations

Available in all conﬁgurations.

31.76 TRCVMIDCCTLR0, Virtual context identiﬁer
Comparator Control Register 0

The TRCVMIDCCTLR0 contains the Virtual Machine Identiﬁer mask value for the TRCVMIDCVR0
register.

Bit ﬁeld descriptions
The TRCVMIDCCTLR0 is a 32-bit register.

Figure 31-72: TRCVMIDCCTLR0 bit assignments

31
0
4

3

COMP0

RES0

RES0, [31:4]

RES0
Reserved.

COMP0, [3:0]

Controls the mask value that the trace unit applies to TRCVMIDCVR0. Each bit in this ﬁeld
corresponds to a byte in TRCVMIDCVR0. When a bit is:

0
The trace unit includes the relevant byte in TRCVMIDCVR0 when it
performs the Virtual context ID comparison.
1
The trace unit ignores the relevant byte in TRCVMIDCVR0 when it
performs the Virtual context ID comparison.

Bit ﬁelds and details that are not provided in this description are architecturally deﬁned. See the
Arm® Embedded Trace Macrocell Architecture Speciﬁcation ETMv4 .

The TRCVMIDCCTLR0 can be accessed through the external debug interface, oﬀset 0x688.

# 32. SPE registers

## 32.1 SPE register summary

This chapter describes the SPE registers.

This section summarizes the SPE registers.

The following table lists all of the SPE registers included in the SPE architecture.

Table 32-1: AArch64 debug register summary

|Op0|Op1|CRn|CRm|Op2|Name|Type|Reset|Description|
|---|---|---|---|---|---|---|---|---|
|3|0|c9|c9|0|PMSCR_EL1|RW|UNK|Statistical Proﬁling Control Register EL1|
|3|4|c9|c9|0|PMSCR_EL2|RW|UNK|Statistical Proﬁling Control Register EL2|
|3|5|c9|c9|0|PMSCR_EL12|RW|UNK|Alias of the PMSCR_EL1 register, available in EL2|
|3|0|c9|c9|2|PMSICR_EL1|RW|UNK|Sampling Interval Counter Register|
|3|0|c9|c9|3|PMSIRR_EL1|RW|UNK|Sampling Interval Reload Register|
|3|0|c9|c9|5|PMSEVFR_EL1|RW|UNK|Sampling Event Filter Register|
|3|0|c9|c9|6|PMSLATFR_EL1|RW|UNK|Sampling Latency Filter Register|
|3|0|c9|c10|1|PMBPTR_EL1|RW|UNK|Proﬁling Buﬀer Write Pointer Register|
|3|0|c9|c10|0|PMBLIMITR_EL1|RW|`00000000`|Proﬁling Buﬀer Limit Address Register|
|3|0|c9|c10|3|PMBSR_EL1|RW|UNK|Proﬁling Buﬀer Status/syndrome Register|
|3|0|c9|c9|4|PMSFCR_EL1|RW|UNK|Sampling Filter Control Register|
|3|0|c9|c10|7|PMBIDR_EL1|RO|`00000026`|Proﬁling Buﬀer ID Register|
|3|0|c9|c9|7|PMSIDR_EL1|RO|`00026497`|Sampling Proﬁling ID Register|

# B Neoverse™ N1 Core AArch32 unpredictable behaviors

## B.1 Use of R15 by Instruction

## B.2 Load/Store accesses crossing page boundaries

Appendix B Core AArch32 unpredictable

behaviors

This appendix describes the cases in which the Neoverse™ N1 core implementation diverges from
the preferred behavior that is described in Armv8 AArch32 UNPREDICTABLE behaviors.

B.1 Use of R15 by Instruction

If the use of R15 as a base register for a load or store is UNPREDICTABLE, the value that is used
by the load or store using R15 as a base register is the Program Counter (PC) with its usual oﬀset
and, in the case of T32 instructions, with the forced word alignment. In this case, if the instruction
speciﬁes write-back, then the load or store is performed without write-back.

The Neoverse™ N1 core does not implement a Read 0 or Ignore Write policy on UNPREDICTABLE use
of R15 by instruction. Instead, the Neoverse™ N1 core takes an UNDEFINED exception trap.

The Neoverse™ N1 core implements a set of behaviors for load or store accesses that cross page
boundaries.

Crossing a page boundary with diﬀerent memory types or Shareability attributes

The Arm® Architecture Reference Manual for A-proﬁle architecture, states that a memory access
from a load or store instruction that crosses a page boundary to a memory location that
has a diﬀerent memory type or Shareability attribute results in CONSTRAINED UNPREDICTABLE
behavior.

Crossing a 4KB boundary with a Device access

The Arm® Architecture Reference Manual for A-proﬁle architecture, states that a memory access
from a load or store instruction to Device memory that crosses a 4KB boundary results in

CONSTRAINED UNPREDICTABLE behavior.

Implementation (for both page boundary speciﬁcations)

For an access that crosses a page boundary, the Neoverse™ N1 core implements the
following behaviors:

- Store crossing a page boundary:

- No alignment fault.

- The access is split into two stores.

- Each store uses the memory type and Shareability attributes that are associated with
its own address.

- Load crossing a page boundary (Device to Device and Normal to Normal):

## B.3 Armv8 Debug unpredictable behaviors

- No alignment fault.

- The access is split into two loads.

- Each load uses the memory type and Shareability attributes that are associated with
its own address.

- Load crossing a page boundary (Device to Normal and Normal to Device):

- The instruction will generate an alignment fault.

This section describes the behavior that the Neoverse™ N1 core implements when:

- A topic has multiple options.

- The behavior diﬀers from either or both of the Options and Preferences behaviors.

This section does not describe the behavior when a topic only has a single option
and the core implements the preferred behavior.

Table B-1: Armv8 Debug unpredictable behaviors

|Scenario|Behavior|
|---|---|
|A32 BKPT instruction with condition code not AL|The core implements the following preferred option:<br>•<br>Executed unconditionally.|
|Address match breakpoint match only on second halfword<br>of an instruction|The core generates a breakpoint on the instruction if CPSR.IL=0. In the case<br>of CPSR.IL=1, the core does not generate a breakpoint exception.|
|Address matching breakpoint on A32 instruction with<br>DBGBCRn.BAS=1100|The core implements the following option:<br>•<br>Does match if CPSR.IL=0.|
|Address match breakpoint match on T32 instruction at<br>DBGBCRn+2 with DBGBCRn.BAS=1111|The core implements the following option:<br>•<br>Does match.|
|Link to non-existent breakpoint or breakpoint that is not<br>context-aware|The core implements the following option:<br>•<br>No Breakpoint or Watchpoint debug event is generated, and the LBN<br>ﬁeld of the_linker_ reads**UNKNOWN**.|
|DBGWCRn_EL1.MASK!=00000 and<br>DBGWCRn_EL1.BAS!=11111111|The core behaves as indicated in the sole Preference:<br>•<br>DBGWCRn_EL1.BAS is**IGNORED** and treated as if`0x11111111`.|
|Address match breakpoint with<br>DBGBCRn_EL1.BAS=0000|The core implements the following option:<br>•<br>As if disabled.|
|DBGWCRn_EL1.BAS speciﬁes a non-contiguous set of<br>bytes within a double-word|The core implements the following option:<br>•<br>A Watchpoint debug event is generated for each byte.|
|A32 HLT instruction with condition code not AL|The core implements the following option:<br>•<br>Executed unconditionally.|

|Scenario|Behavior|
|---|---|
|Execute instruction at a given EL when the corresponding<br>EDECCR bit is 1 and Halting is allowed|The core behaves as follows:<br>•<br>Generates debug event and Halt no later than the instruction following<br>the next_Context Synchronization operation_ (CSO) excluding ISB instruction.|
|H > N or H = 0 at Non-secure EL1 and EL0, including<br>value read from PMCR_EL0.N|The core implements:<br>•<br>A simple implementation where all of HPMN[4:0] are implemented, and<br>In Non-secure EL1 and EL0:<br>◦<br>If H > N then M = N.<br>◦<br>If H = 0 then M = 0.|
|H > N or H = 0: value read back in MDCR_EL2.HPMN|The core implements:<br>•<br>A simple implementation where all of HPMN[4:0] are implemented and<br>for reads of MDCR_EL2.HPMN, return H.|
|P ≥ M and P ≠ 31: reads and writes of PM<br>XEVTYPER_EL0 and PMXEVCNTR_EL0|The core implements:<br>•<br>A simple implementation where all of SEL[4:0] are implemented, and if P<br>≥ M and P ≠ 31 then the register is**RES0**.|
|P ≥ M and P ≠ 31: value read in PMSELR_EL0.SEL|The core implements:<br>•<br>A simple implementation where all of SEL[4:0] are implemented, and if P<br>≥ M and P ≠ 31 then the register is**RES0**.|
|P = 31: reads and writes of PMXEVCNTR_EL0|The core implements:<br>•<br>**RES0**.|
|n ≥ M: Direct access to PMEVCNTRn_EL0 and<br>PMEVTYPERn_EL0|The core implements:<br>•<br>If n ≥ N, then the instruction is_unallocated_.<br>•<br>Otherwise if n ≥ M, then the register is**RES0**.|
|Exiting Debug state while instruction issued through<br>EDITR is in ﬂight|The core implements the following option:<br>•<br>The instruction completes in Debug state before executing the restart.|
|Using memory-access mode with a non-word-aligned<br>address|The core behaves as indicated in the sole Preference:<br>•<br>Does unaligned accesses, faulting if these are not permitted for the<br>memory type.|
|Access to memory-mapped registers mapped to Normal<br>memory|The core behaves as indicated in the sole Preference:<br>•<br>The access is generated, and accesses might be repeated, gathered, split<br>or resized, in accordance with the rules for Normal memory, meaning the<br>eﬀect is**UNPREDICTABLE**.|
|Not word-sized accesses or (AArch64 only) doubleword-<br>sized accesses<br>>|The core behaves as indicated in the sole Preference:<br>•<br>Reads occur and return**UNKNOWN** data.<br>•<br>Writes set the accessed register(s) to**UNKNOWN**.|
|External debug write to register that is being reset|The core behaves as indicated in the sole Preference:<br>•<br>Takes reset value.|

## B.4 Other UNPREDICTABLE behaviors

This section describes other UNPREDICTABLE behaviors.

Table B-2: Other UNPREDICTABLE behaviors

|Scenario|Behavior|
|---|---|
|Accessing reserved Debug registers|The core deviates from preferred behavior because the hardware cost to<br>decode some of these addresses in Debug power domain is signiﬁcantly high.<br>The actual behavior is:<br>1.<br>For reserved Debug registers in the address range`0x000`-`0xCFC`<br>and Performance Monitors registers in the address range`0x000`, the<br>response is either**CONSTRAINED UNPREDICTABLE** Error or_res0_ when any of<br>the following errors occurs:<br>**Oﬀ**<br>The Core power domain is either completely oﬀ or in a low-power<br>state where the Core power domain registers cannot be accessed.<br>**DLK**<br>`DoubleLockStatus()` is TRUE and OS double-lock is locked<br>(EDPRSR.DLK is 1).<br>**OSLK**<br>OS lock is locked (OSLSR_EL1.OSLK is 1).<br>2.<br>For reserved Debug registers in the address ranges`0x400`-`0x4FC` and<br>`0x800`-`0x8FC`, the response is**CONSTRAINED UNPREDICTABLE** Error or**RES0**<br>when the conditions in1 on page 492 do not apply and the following<br>error occurs:<br>**EDAD**<br>`AllowExternalDebugAccess()` is FALSE. External debug<br>access is disabled.<br>3.<br>For reserved Performance Monitor registers in the address ranges<br>`0x000`-`0x0FC` and`0x400`-`0x47C`, the response is either**CONSTRAINED**<br>**UNPREDICTABLE** Error, or**RES0** when the conditions in1 on page 492<br>and2 on page 492 do not apply, and the following error occurs:<br>**EPMAD**<br>`AllowExternalPMUAccess()` is FALSE. External Performance<br>Monitors access is disabled.|
|Clearing the_clear-after-read_ EDPRSR bits when Core<br>power domain is on, and`DoubleLockStatus()` is<br>TRUE|The core behaves as indicated in the sole Preference:<br>•<br>Bits are not cleared to zero.|

|Scenario|Description|
|---|---|
|CSSELR indicates a cache that is not<br>implemented.|If CSSELR indicates a cache that is not implemented, then on a read of the CCSIDR the behavior<br>is**CONSTRAINED UNPREDICTABLE**, and can be one of the following:<br>•<br>The CCSIDR read is treated as`NOP`.<br>•<br>The CCSIDR read is**UNDEFINED**.<br>•<br>The CCSIDR read returns an**UNKNOWN** value (preferred).|

|Scenario|Description|
|---|---|
|HDCR.HPMN is set to 0, or to a value<br>larger than PMCR.N.|If HDCR.HPMN is set to 0, or to a value larger than PMCR.N, then the behavior in Non-secure<br>EL0 and EL1 is**CONSTRAINED UNPREDICTABLE**, and one of the following must happen:<br>•<br>The number of counters accessible is an**UNKNOWN** nonzero value less than PMCR.N.<br>•<br>There is no access to any counters.<br>For reads of HDCR.HPMN by EL2 or higher, if this ﬁeld is set to 0 or to a value larger than<br>PMCR.N, the core must return a**CONSTRAINED UNPREDICTABLE** value that is one of:<br>•<br>PMCR.N.<br>•<br>The value that was written to HDCR.HPMN.<br>•<br>(The value that was written to HDCR.HPMN) modulo 2h, where h is the smallest number of<br>bits required for a value in the range 0 to PMCR.N.|
|CRC32 or CRC32C instruction with<br>`size==64`.|On read of the instruction, the behavior is**CONSTRAINED UNPREDICTABLE**, and the instruction<br>executes with the additional decode:`size==32`.|
|CRC32 or CRC32C instruction with<br>`cond!=1110` in the A1 encoding.|The core implements the following option:<br>•<br>Executed unconditionally.|
