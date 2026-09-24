# The Road to the Register File: Forensics and Structural Fault Injection for a Production ARM64 Mercurial Core

*Draft 1.0, September 2026. Target: MICRO / HPCA. Data: 17 production crashes on one Kunpeng-920 machine, 2026-08-14 to 09-04.*


## Abstract

A production ARM64 server (Kunpeng-920, 192 cores) crashed seventeen times over twenty-two days. Every crash, and more than 160 precursor warnings, trace to a single physical core; the other 191 cores recorded nothing. The machine's RAS infrastructure (RAS Extension, GHES, ghes_edac, BERT) recorded nothing either, for any core. This paper is a post-mortem of that machine and an argument about what such post-mortems can and cannot see.

Our forensic method pairs two sources that fleet-scale SDC studies cannot obtain: the register state at the moment of each crash, and the memory contents those registers were loaded from. For thirteen of the seventeen crashes the faulting instruction sequence is identical (the per-CPU offset load in the CFS load balancer) and register arithmetic closes exactly (x27 = x1 + x20; FAR = x27 + 0x120) while the memory source reads back correct in the dump. The delivered value, not the stored value, is wrong. Across the seventeen crashes the wrong values fall into four families: collapses to zero, torn values that match a byte-shifted window elsewhere in the source array (verified by unique-match search and direct re-read), broadband bit flips of Hamming weight 31–35, and whole-word substitutions of one valid pointer for another, including one case that persisted in memory and drew 458 subsequent warnings on five other cores.

We argue these four families are one defect: a marginal timing condition on the load/store return path of one core, whose manifestation depends on which downstream logic each unlucky cycle catches. This reading contradicts the single-bit-flip fault model that most fault-injection studies still use, and it lands in a gap the fleet literature has not covered, the LSU data path, which instruction-level screening tests cannot observe because they compare outputs, not deliveries.

To test the reading we built structural fault injection into gem5: byte-lane skew, stale-line replay, and all-zero corruption applied to the store-to-load forwarding path, plus corruption hooks on the address path and page-table walker. Injected skew reproduces the production Oops chain end to end (30 injections, 28 detected pointer corruptions in the target kernel; golden run clean). The same hooks exposed a methodological trap: in syscall-emulation mode the address and PTW hooks never fire, because gem5's SE mode disables the MMU; results that would have silently validated the fault model are in fact artifacts of the simulation mode.

We close with what the machine taught us that fleets cannot: spurious translation faults, currently discarded by the kernel after an AT S1E1R replay succeeds, are the only systematic leak this defect had, and counting them per core would have identified the bad core years before it killed anything.


## 1. Introduction

The cloud has learned to distrust its silicon. Google named the problem in 2021 [1]: some small fraction of cores compute wrongly, silently, and indefinitely: mercurial cores. Meta measured it at scale [2,3]: roughly one machine in 2,850 will fail an SDC test at some point in its life, and more than 60% of failures sit on a single physical core. Alibaba confirmed the magnitudes on a million-processor fleet and added the trigger physics: error rates rise exponentially with temperature, below thresholds that idle machines never reach [4]. Veritas traced the vulnerability to unprotected arithmetic units, vector floating point above all [5]; SEVI found that 92% of a hyperscale fleet's SDC events involved FMA instructions [6].

All of these studies share one property: they find what their instruments can see. A functional test applies known inputs to an instruction and compares the output against a golden result. That instrument is sharp for arithmetic, where the output is the result. It is blind to the delivery path (the store-to-load forwarding network, fill buffers, alignment muxes, the write port of the register file) because a defect there does not change what the instruction computes; it changes what the instruction receives. The most recent systematic evaluation of test suites against the load/store queue put detection rates at 0–20%, the lowest of any structure examined [7]. What we do not know is whether that blind spot is empty or merely dark.

This paper reports what came out of it on one machine.

Between August 14 and September 4, 2026, a 4-socket Kunpeng-920 server (192 Taishan-v110 cores, 8 NUMA nodes, 768 GB, openEuler 6.6) panicked seventeen times. The crashes shared almost nothing at the software level: the victims included the CFS load balancer, the RCU state machine, an unbound kworker, two sftp sessions, NetworkManager, and systemd-coredump. They shared two things at the hardware level. Sixteen of seventeen executed on CPU 179, and the seventeenth, a list-corruption storm on five cores, traces to a word that was written wrong once, most plausibly by CPU 179. Over the same window the machine logged more than 160 kernel warnings, every one of them on CPU 179. The other 191 cores were silent. So was the RAS stack.

Each crash left a dump. That is the resource fleet studies do not have: not a test failure, but the register file, the stack, and the memory image at the moment of death. The difference matters. A test failure says an output was wrong. A dump lets you ask the sharper question: was the input wrong when it arrived?

The paper makes four contributions.

Method. A forensic protocol that closes register arithmetic modulo 2⁶⁴ at three levels (Section 3), reads back the memory source of each faulting load from the dump, and uses the ARMv8 requirement that FAR_EL1 equal the address the MMU was asked to translate as an invariant on the address path. The protocol needs only kdump and crash; any operator can run it.

Evidence. A taxonomy of delivered-wrong values, four families strong, with the torn-value family verified to the byte: the delivered value is exactly the eight bytes at a mis-phased offset elsewhere in the source array, a match unique across 1,536 candidates and confirmed by re-reading the address (Section 4). One case gives two loads of the same address, one turn apart, returning different values: inconsistent execution in the wild, so far only demonstrated in RTL [8].

*Interpretation.* The argument that four families require one defect rather than four: the cost of independent causes all landing on one core and one path is (1/192)^(k−1); the families share five invariants; and the machine's decay curve is single, not braided (Section 5).

Validation. A structural fault-injection facility in gem5 whose skew mode reproduces the production crash chain end to end, and whose address/PTW modes exposed a simulation-mode artifact that would otherwise have laundered a null result into a finding (Section 6).

We write this as a case study with a general method, in that order. The machine is gone (the core has been offlined and the sockets are being replaced), so the dataset is closed and finite: seventeen crashes, each with a full forensic report, every claim traceable to a dump offset or a dmesg line number [9].


## 2. Background and What the Literature Already Settled

Three lines of work bear on this paper, and it is worth being precise about what each has established, because our contribution is defined against them.

Fleet epidemiology. The Google account [1] established that mercurial cores exist at roughly one per few thousand CPUs and argued from economics that screening must move into the fleet. Meta built the screening: Fleetscanner reuses maintenance windows for out-of-production tests, Ripple co-locates millisecond test slices with production traffic [2]; PinDrop, fifty months of continuous measurement, fixed the lifetime failure rate at 0.035% of machines and the single-physical-core share above 60% [3]. Alibaba's study of a million processors [4] contributed the physics: exponential temperature dependence (r > 0.75 on six of 27 defective machines), minimum trigger temperatures, and the observation that bit-flip positions repeat for a given defect, a fixed mask, which they read as a stable defect signature. PinDrop explicitly rejects particle-induced transients as the cause of new-onset SDC and attributes production SDC to marginal defects that behave as small-delay faults, sensitive to combinations of temperature, voltage, frequency, and data.

Vulnerability modeling. The AVF line, from Mukherjee et al.'s ACE analysis [10] through mechanistic models [11] and accelerated injection [12], ranked structures under the single-event-upset model: storage arrays dominate (DTLB data 36%, writeback L1D 25%, issue queues 28%), the datapath is cheap (execution units 9%, after masking and idleness derates). Two later results bend that ranking. Under permanent stuck-at faults, combinational logic (an adder, H-AVF 0.149) becomes more vulnerable than the array protecting it (L1D, 0.005) [13]. Under small-delay faults, ALU vulnerability exceeds the register file by 5×, and about half of fault activations produce multi-bit errors [14]. The fault model decides the ranking; this is the point DelayAVF was made to press.

Unit attribution in the field. Veritas [5] paired gate-level injection of arithmetic units in gem5 with six years of Meta telemetry: vector units outrank scalar by two to three orders of magnitude, multipliers outrank adders by two, and scalar integer adders rarely cause SDC at all because their results feed addresses and control flow, so errors crash rather than corrupt. SEVI [6] put the fleet number on it: 92% of SDC events were FMA. For memory-side SDC, SEVI's two observed modes are worth quoting for later: wrong-offset reads (76%) and data corruption (24%). No fleet study attributes SDC to the LSU return path. The closest is PinDrop's "Data Move" test family, about which its authors say the underlying hardware cause "is unknown and beyond the scope of this paper" [3].

Detection. The tools divide by vantage point. SiliFuzz fuzzes instruction sequences against a simulator and replays them fleet-wide [15]. Harpocrates generates tests against a microarchitectural model and reaches near-perfect detection on functional units, while measuring 0–20% on the load-store queue [7]. Vega derives tests bottom-up from aging analysis of RTL it owns [16]. ITHICA instruments the program itself, duplicating instructions and comparing, and found the deepest lever: the most dangerous defects produce *inconsistent* errors: the same instruction, same architectural inputs, different results depending on execution context [8]. Hardware Sentinel works purely from telemetry: crashes concentrated on one core, rare kernel exceptions enriched 20–59× on defective CPUs, and silence in the hardware event log as a necessary condition [17].

Three gaps stand where this paper's machine happens to sit. No fleet instrument observes the delivery path. No injection study models byte-phased misrouting rather than bit flips. And no public account, to our knowledge, has paired a production SDC crash with the memory truth of the very load that delivered the wrong value.


## 3. The Forensic Protocol

### 3.1 What the crash scene gives you

An ARM64 kernel oops prints the full register file, the fault address (FAR_EL1), the exception syndrome (ESR_EL1), and the code window around the faulting PC. A kdump preserves memory. The protocol is a fixed sequence of checks run against each of the seventeen crashes; nothing in it requires the machine to be alive.

Three registers anchor the analysis. In the CFS load balancer's `update_sg_lb_stats`, the per-CPU iteration computes the runqueue address of every CPU in the scheduling group:

```
ldr  x20, [x0, w25, sxtw #3]   ; x20 ← __per_cpu_offset[i]
add  x27, x1, x20              ; x27 = &runqueues + offset
ldr  x23, [x27, #0x120]        ; x23 ← rq->cfs.load_avg
```

`__per_cpu_offset` is a 192-entry array of per-CPU base offsets, written once at boot and read-only thereafter. In this kernel it is a strict arithmetic sequence with stride 0x22000, a property we use as a consistency check on every read.

### 3.2 The three closures

For each crash we verify, in fixed order, three identities modulo 2⁶⁴. The verification is scripted; no value in this paper was checked by hand.

Closure 1, the adder. x1 + x20_observed = x27_observed, bit for bit. The base address and the addition are correct; whatever is wrong arrived before the adder, in x20.

Closure 2, the offset. x27_observed + 0x120 = FAR. The immediate decodes from the faulting instruction word (0xf9409377: LDR x23, [x27, #0x120]); the address that reached the MMU is exactly the address the (already poisoned) x27 implies. This closure does more than check arithmetic. ARMv8 requires that on a translation fault, FAR_EL1[63:0] equal the virtual address the MMU attempted to translate. FAR matching x27 + 0x120 therefore certifies that the *address path* (from the register file through the AGU to the MMU) delivered correctly, even while the *data path* did not. The defect has a direction.

Closure 3, the counterfactual. x1 + __per_cpu_offset[i]_true, where the true value is read from the dump, should equal the runqueue of CPU i. We check it three ways: the computed address matches the self-pointer embedded in the runqueue instance (`nohz_csd.info`), vtop walks it VALID, and the instance is sane (cpu = i, plausible load_avg). Had the load delivered the truth, the instruction would have read its value and moved on. The one necessary condition for the crash is a corrupted delivery of x20.

Two properties of the dataset sharpen this. First, the iteration index i (x25) is 97 in one crash and 149 in another, never 179, the executing core. The corruption is bound to *which core executes the load*, not to *which address is read*. Second, the whole array reads back as the intact arithmetic sequence in every dump where the dump is complete enough to read it. The memory was right every time we could check. What the register received was not.

### 3.3 Honesty rules

Two of the seventeen dumps are incomplete (kdump died while writing, because kdump also runs on the defective machine). For those, memory-truth comparison is impossible; the classification of those crashes rests on register algebra alone and is marked accordingly in Section 4, at lower confidence. This is a real forensic boundary, not a formality: a defect that can kill a kernel can also kill its crash dumper.


## 4. Four Families of Wrong Values

Table 1 collects the delivered values. Reading the seventeen cases side by side, the wrong values sort into four families, and the sorting is not imposed: the families are separated by properties that admit no intermediate cases.

Family Z, zero collapse (six crashes, five verified). A non-zero kernel pointer arrives as 0x0. Sixty-four bits do not flip to zero together; a bit flip model has no way to express this. The natural reading is a dropped beat: the return bus undriven for that cycle, or a fill-buffer entry replaced with a zero-filled one. Zero collapse has the cleanest system-level signature too: with x20 = 0, x27 falls back to the `.data..percpu` template address, which lies in the free_initmem unmapped region, so the fault surfaces as an L2 or L3 translation fault with an unmapped page-table entry. One of these crashes came 24 minutes after boot with zero warnings and 1,365 seconds of dmesg silence before it. There was no precursor at all.

Family T, torn delivery (seven crashes, six verified to the byte). The delivered value is neither the true value nor noise. It is the true content of the source array at the wrong phase. The method: lay the 192-entry array out as a little-endian byte stream, take the eight delivered bytes, and search. In the 08-31 crash the delivered 0xa000ffffbe56fb25 matches exactly one location in the whole array (slot 125, offset +2 bytes, a window that straddles the 125/126 boundary), and re-reading that address from the dump returns the delivered value bit for bit. The instruction wanted slot 60. The 09-03 crash matches slot 123 at +1 byte; the 09-04 morning crash matches slot 9 at +5 bytes, which in this array's geometry is simultaneously a 3-byte rotation of slot 10 itself. Three distinct phases, one common shape: the address was right, the data was real, the byte alignment was wrong.

This family carries the paper's central physical claim, so its evidentiary weight matters. The match is unique over 1,536 candidate alignments (192 slots × 8 rotations). It is not a near match; it is equality, re-verified by a second read of the matched address. A defect in the SRAM array cannot produce another location's true contents. A defect in the byte-steering (the alignment mux network between the fill buffer and the load's destination register) produces exactly this.

Family B, broadband flips (two crashes). Delivered and true differ by Hamming weight 31 and 35, with the flipped bits spread uniformly across all 64 bits: no byte structure, no nibble structure, no runs. Every structured digital failure is excluded by that shape: byte enables fail by bytes, wordline and column decode fail in rows and columns, bridging faults stick to columns. What remains is the analog front end: sense amplifiers sampling bitlines before the differential has developed, in a window where the margin was not there. Two properties distinguish these flips from single-event effects: the weights are an order of magnitude beyond a typical particle strike, and they fit the small-delay prediction that marginal timing activates multi-bit errors about half the time [14].

Family S, substitution, including the write path (one crash). The 09-04 22:09 machine died differently, and the difference is diagnostic. A per-CPU pageset list head, whose `prev` field should point at itself (empty list), instead held a pointer into the vmemmap page structure array: a *valid* pointer, to an object whose `lru.next` pointed back at the corrupted head. XOR distance 27 bits: whole-word replacement, not flips. Five other cores then hit that word 458 times in the page allocator. This is the write side of the same defect: a store that delivered the wrong data, persistently, for other cores to trip on. And within the same boot, CPU 179 itself delivered a zero-load of `__per_cpu_offset[174]` (truth non-zero, read back from the dump): Family Z, one machine, one hour.

The same crash holds the paper's most striking single observation. In the list-corruption warning path, one load of the poisoned address returned the good value, and the *next* load, one subroutine call later, returned the bad value. Same address, same core, consecutive loads, different results. ITHICA demonstrated inconsistent execution by injecting a stuck-at fault in RTL [8]; this is that phenomenon, unbidden, in production silicon.

The precursor channel. Not a family of values but a family of signals: throughout the dataset, the warnings that precede the crashes are "spurious kernel translation faults": hardware reporting a translation fault for an address whose page-table walk succeeds when the kernel replays it with AT S1E1R, whose mapping vtop confirms as valid, sometimes as a 1 GB block. The page-table walker is a client of the same data path as ordinary loads. When its reads go wrong transiently, the fault is *detected* (and discarded as spurious); when an ordinary load's delivery goes wrong, nothing is detected. Two faces of one defect. In one crash the precursor warning and the fatal oops are 38 seconds apart, and the register state of both events points at the same `sched_group` object; the machine touched the same object wrongly twice within a minute.


## 5. One Defect or Four?

The families look mechanistically unrelated. Zero collapse is a lost beat; tearing is a phase error; broadband flips are analog; substitution is a misdirected source. The proposition that these are four independent defects, however, fails three independent tests.

Statistics. k independent defects, each free to land on any of 192 cores, all land on core 179 with probability (1/192)^(k−1). Our sample is not one event per defect: it is seventeen boots, 160+ warnings, every one on that core, the other 191 cores at zero. For k = 2 the joint probability is 2.7 × 10⁻⁵ before conditioning on anything else, and the posterior here is not close.

Shared invariants. All four families pass the same five-row table: memory truth intact (verified wherever the dump allows); address computation correct; arithmetic self-consistent; the only anomaly is the delivered value of a load or store; and attribution to CPU 179 (the substitution case attributes the *read* half to CPU 179 directly, and the *write* half only to "one core's LSU data path," a boundary we keep explicit). Independent defects that agree on all five rows are not independent in any useful sense; they have merely been given separate names.

One decay curve. Survival time across the seventeen boots: 110 h, 66 h, 89 h, 14 h, 66 min, 24 min... down to 9 minutes between reboots in the final evening, with the last few crashes clustering in bursts seconds apart and, in one case, two touches of the same object inside 38 seconds. Four independent defects decaying in synchrony would need a fourth coincidence to explain the synchrony itself.

What unifies them mechanistically is a property of marginal timing faults that deterministic-fault intuition does not prepare you for: a path with insufficient margin does not fail the same way every time. Whether a given cycle violates setup depends on the data value's transition direction (through its capacitive load and coupling to neighbors), on the switching activity of adjacent wires, and on where the voltage and frequency currently sit. The same marginal condition, striking different downstream logic on different cycles, yields a lost beat here, a mis-steered byte phase there, an underdeveloped sense-amplifier read yonder. The manifestation is a function of where the glitch lands. The number of faces is not evidence of the number of causes.

An honest boundary: software forensics cannot distinguish "one degraded via/transition causing fan-out failures" from "one aged local power domain stressing several critical paths together." The second explains the breadth more economically and matches the burst-and-decay statistics, but settling it requires the vendor's shmoo plots of core 179 against a golden core. That is the ceiling of this method, stated as such in every one of the seventeen case reports [9].


## 6. Structural Fault Injection in gem5

Reading a defect off a corpse is one thing; showing the reading *generates* the observed pathology is another. The standard tool for the second half is fault injection, and the standard fault model (flip a bit, or hold a node stuck) cannot express what Section 4 documents. A byte-phased misroute has Hamming distance zero from the truth it displaced: it is *somebody else's truth*, re-steered. No number of bit flips on the true value produces it except by coincidence at negligible probability.

We therefore extended CHAOS [18] with three structural fault models on the store-to-load forwarding path, plus corruption hooks on the effective address and the page-table walk:

* byte_lane_skew: the forwarded data word is rotated by k byte positions before delivery to the destination register;
* stale_line_replay: the delivery is replaced with the oldest recent fill-buffer entry;
* all_zero: the delivery is zeroed.

End-to-end reproduction. The probe is a user-space kernel that walks a pointer table through the forwarding path and validates each load. Golden run: zero failures. With byte_lane_skew at probability 0.05: 30 structural injections, 28 pointer corruptions detected (93%), and, in the kernel-mode configuration, the full production chain: poisoned pointer arithmetic, non-canonical address, translation fault, oops. The chain the machine walked on its own is walked on demand, reproducibly, across seeds.

The simulation-mode trap. We initially measured the address-path (D2) and PTW (D3) hooks in gem5's syscall-emulation mode and observed zero effect. The zero was an artifact. In SE mode gem5 sets SCTLR.M = 0 and translates addresses arithmetically, bypassing the page-table walker entirely; the D3 hook sits on code that never executes. In full-system mode the same hooks fire freely: address-path corruption at probability 0.5 injects 20 faults whose register signature matches the production D2 signature (a canonical kernel address with byte 7 cleared becomes non-canonical); PTW corruption at probability 0.5 injects 7,963 faults, of which 7,727 (97%) produce invalid PTEs that surface as translation faults. That is the spurious-fault precursor channel, synthesized. Had we stopped at the SE-mode zero, we would have concluded the address path is insensitive to corruption and written that up. Null results deserve the same scrutiny as positive ones; sometimes the null is the simulator's, not the machine's.

The full quantitative campaign, separating the D1/D2/D3 signature spectra under matched full-system workloads, remains open, and we flag rather than bury the two confounds already identified in interim runs: D1's forwarding hook is not exercised during early FS boot (store-to-load forwarding needs a warmed pipeline), and mid-run instruction stalls in the simulator are not guest crashes. The reproduction claim we make here is deliberately narrower than the campaign: the *chain* is reproducible, the *rates* are not yet attributed.


## 7. What This Case Adds to the Fleet Picture

The blind spot has residents. The fleet literature's unit attribution stops at arithmetic units and coherence logic, with the delivery path explicitly out of scope [3,5]. One machine proves nothing about rates, but it proves the class is not empty: a delivery-path defect, invisible to output-comparison testing, ran for twenty-two days in a production kernel, was systematic enough to kill the machine seventeen times, and would have kept going. The 0–20% LSQ detection rates measured by Harpocrates [7] are not measuring an absence of defects; they are measuring an absence of instruments.

A mechanism anchor for wrong-offset reads. SEVI's most common memory-side SDC mode is the wrong-offset read (76% of memory-side events) [6], reported without unit attribution. Torn delivery is that phenomenon with the covers off: the offset is a byte-phase error in the steering network, and the delivered value is traceable to its true source address in the array. Fleet-scale symptom, microarchitecture-scale cause, one dataset joining them.

A production confirmation of inconsistent execution. ITHICA's central finding, that the worst defects produce different results for the same instruction and inputs depending on context, was established by RTL injection and fleet testing [8]. The consecutive-loads observation in the substitution case is the same phenomenon in the field: same address, adjacent loads, different deliveries. For detection design, the implication is direct: single-shot self-checking (execute twice, compare) has exactly the blind spot ITHICA described, and our field case shows the blind spot is populated.

Form stability as defect class signature. Alibaba reported that bit-flip masks repeat for a given defect [4], a stable fingerprint, consistent with a stuck gate in an arithmetic unit. Our machine shows the opposite: every episode a different shape, which is what a timing/phase defect predicts. Form stability, in other words, separates resident defects (stable mask, unit-localized) from path defects (unstable form, context-dependent). Neither fleet telemetry nor single-machine forensics can see this distinction alone; together they can, and nobody has yet built the taxonomy.

The precursor channel is free and it is enough. Every warning this machine ever gave was discarded by the kernel as spurious, after a software replay proved the address valid. Counted per core, those discards were a 100%-specific signature: 160+ events on one core, zero elsewhere, months before the first fatal crash. This is Hardware Sentinel's heuristic [17] (rare exceptions enriched and concentrated on one core, hardware event log silent) available at zero marginal cost, because the kernel already computes the discriminating statistic and throws it away. Exposing a per-core spurious-fault counter is a one-line change to a diagnosis that currently does not exist.


## 8. Design Implications

We keep these brief; the case reports carry the detail [9].

Protect the road, not just the warehouse. ECC on L1D catches a bit that flips *while stored*. It cannot catch a line that is correct in the array and wrong at the register-file write port, because the check happens before the journey, not after. A parity bit carried end-to-end on the load return path (fill buffer through alignment network to the destination register, with replay on mismatch) covers the gap at a fraction of the cost of array ECC, and the substitution case argues for the symmetric store path.

Give the walker a voice. The PTW is a load client. When its reads go bad transiently, today's outcome is either a spurious fault (discarded) or, in the worst case, a PTE read wrong but plausible: a silently wrong translation, which is SDC in its purest form (no crash, no log, systematically wrong data). A parity check on walker reads is cheap insurance against the one failure mode in this whole story that we never observed but cannot rule out.

Test the steering, not only the cells. March algorithms test arrays; a byte-skew in the steering network passes them. The test pattern this case implies is simple: fill consecutive lines with known phase-encoded data, read back through every byte-offset alignment the ISA permits, compare. Our skew injector provides the oracle side of exactly this test, and the positional-parity checking scheme we prototyped on top of it (golden run zero false positives; 100% detection on bit flips, zero collapse, and the pointer-chain deliveries; 96% on random skew, with all 18 escapes accounted for as low-entropy loader words) shows the detection layer is tractable [19].

For operators. Offline the core at the first sign of single-core exception concentration; the machine's own decay curve shows the interval between "first warning" and "first fatality" shrinking from 29 hours to none. Do not spend effort on cache-disabling mitigations; three l1d_disable trials on this machine changed nothing, as the defect model predicts: the flaw was never in the cells.


## 9. Limitations

This is one machine. It establishes that delivery-path SDC exists in production and is diagnosable with public tools; it says nothing about population rates, which only fleets can measure. Two of seventeen dumps are incomplete, and their classifications carry correspondingly lower confidence. The single-defect argument in Section 5 is strong on rejection of independence but cannot discriminate between the two single-defect models without vendor DFT access. The injection campaign of Section 6 reproduces chains, not rates; the rate attribution campaign is unfinished and confounded as described. And the machine is gone, so the dataset will not grow.


## 10. Conclusion

A core delivered wrong data for twenty-two days. The memory was right, the addresses were right, the arithmetic was right; only the delivery was wrong, in four different shapes, all of them shapes that a marginal timing condition produces when different cycles catch different downstream logic. Everything that was supposed to catch this (array ECC, RAS telemetry, functional screening tests) was watching somewhere else: the arrays, the event logs, the outputs. The one instrument that worked was the one nobody built on purpose: a kernel that happens to print its register file when it dies, and a scheduler hot enough to make the defective path execute thousands of times a day.

The general lesson is not about this machine. It is that the delivery path, the road between the cache and the register file, is observable from software, systematically, with tools every operator already has, and that the signals it leaks are being discarded today as noise. Seventeen crashes bought that observation. It seems wasteful to keep throwing it away.


## References

[1] P. Hochschild et al. "Cores that don't count." HotOS'21.
[2] Meta. "Fleetscanner and Ripple: Detecting silent data corruptions in the wild." arXiv:2203.08989.
[3] PinDrop authors. "Breaking the silence on SDCs in a large-scale fleet." HPCA'26.
[4] Alibaba Cloud and Tsinghua. "Understanding silent data corruptions in a large production CPU population." SOSP'23.
[5] Veritas authors (Univ. of Athens and Meta). "Demystifying silent data corruptions: µarch-level modeling and fleet data of modern x86 CPUs." HPCA'25.
[6] SEVI authors (CMU and Meta). "Silent data corruption of vector instructions in hyperscale datacenters." ASPLOS'26.
[7] Harpocrates authors (Univ. of Athens and AMD). "Automated functional program generation against CPU faults and silent data corruptions." ISCA'24 / IEEE Micro'26.
[8] ITHICA authors (Stanford and Google). "Intra-thread instruction checking approach for defect-induced silent data corruptions." arXiv.
[9] The seventeen case reports, with per-case command logs, register algebra scripts, and dump citations: docs/cases/vmcore-diagnosis-report-127.0.0.1-2026-*, this repository.
[10] S. Mukherjee et al. "Measuring architectural vulnerability factors." IEEE Micro (MICRO Top Picks), 2003.
[11] P. Nair et al. "A first-order mechanistic model for architectural vulnerability factor." ISCA'12.
[12] MeRLiN authors. "Exploiting dynamic instruction behavior for fast and accurate microarchitecture level reliability assessment." ISCA'17.
[13] Sridharan et al. "Applying architectural vulnerability analysis to hard faults in the microprocessor." SIGMETRICS'06.
[14] DelayAVF authors (MIT and AMD). "Calculating architectural vulnerability factors for delay faults." MICRO'24.
[15] SiliFuzz authors (Google). "Fuzzing CPUs before shipping them." White paper.
[16] Vega authors (Umich, Technion, UW). "Proactive runtime detection of aging-related silent data corruptions: a bottom-up approach." ASPLOS'24.
[17] Hardware Sentinel authors (Meta). "Protecting software applications from hardware silent data corruptions." ASPLOS'25.
[18] CHAOS authors (Univ. of Catania). "Controlled hardware fault injector system for gem5." arXiv:2602.02119.
[19] Positional parity research, with detection matrix and escape analysis: POSITIONAL_PARITY_RESEARCH.md, this repository.

*Artifacts: all seventeen forensic reports, the algebra scripts (algebra.py per case, outputs archived), the gem5 injection configuration, and the probe kernels are in this repository. Every quantitative claim in Sections 3–5 traces to a dump offset or dmesg line number in a case report; every claim in Section 6 traces to a logged gem5 run.*
