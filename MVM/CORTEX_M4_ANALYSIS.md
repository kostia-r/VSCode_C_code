# Cortex-M4 / STM32F407VET6 Footprint Analysis

This document records the Phase 12 analysis and first footprint implementation.
Measurements use Arm GNU Toolchain 15.3.1, Cortex-M4 Thumb builds at `-Os`, and
the 2026-07-19 full T310/T610 corpus checkpoints.

## Target Memory

STM32F407VET6 provides 512 KiB Flash and 192 KiB normal/CCM SRAM:

- 112 KiB SRAM1 plus 16 KiB SRAM2 on the bus matrix;
- 64 KiB CCM data RAM accessible only by the CPU;
- DMA and other bus masters cannot access CCM;
- FSMC can attach external SRAM/PSRAM, but the first bring-up should determine
  what can run from internal memory alone.

Official references:

- STM32F407VE product page:
  <https://www.st.com/en/microcontrollers-microprocessors/stm32f407ve.html>
- RM0090 memory and bus architecture:
  <https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf>

## Current Measurements

### Opaque VM Context

Before the framebuffer refactor, Cortex-M4 `sizeof(MpnVM_t)` was 473,344 bytes.
Two desktop-oriented deferred rendering fields accounted for 466,944 bytes
(98.65%):

| Field | Bytes |
| --- | ---: |
| 2,048 deferred draw commands | 335,872 |
| 128 complete 256-entry palette snapshots | 131,072 |
| All remaining VM state | 6,400 |

One draw command is 164 bytes because every command embeds text, map, clip,
style, and palette-related snapshot state regardless of command type.

Desktop and MCU now use the same VM-owned RGB565 framebuffer path. Each complete
draw operation is applied immediately through a one-command/one-palette scratch
area; `vFlipScreen` submits the accumulated dirty rectangle to `display_flush`.
The measured Cortex-M4 `sizeof(MpnVM_t)` is now **7,600 bytes**, a reduction of
465,744 bytes (98.39%). SDL is only the desktop implementation of the platform
flush callback and is not linked into the VM library.

The later compatibility allocator experiment added a fixed 512-entry tracking
table and guest-memory diagnostics. With the segmented-address fields included,
the current Cortex-M4 context measures **13,824 bytes**. Most of the increase is
temporary allocation tracking and should be reduced or moved to optional
parent-owned diagnostic workspace before firmware release.

### Runtime Pool

Memory planning previously reserved the same guest budgets for every image:

- 256 KiB heap;
- 64 KiB guest stack;
- 4 KiB stack guard.

That fixed 324 KiB dominated the old runtime-pool requirement. Measured old
Cortex-M4 estimates, including decoded metadata and RGB565 framebuffer, were:

| Corpus/profile | Minimum | Maximum | Average |
| --- | ---: | ---: | ---: |
| T310 | 344.0 KiB | 400.2 KiB | 364.6 KiB |
| T610 | 377.0 KiB | 424.5 KiB | 392.6 KiB |

The VMGP header already contains a requested guest stack size. Across the
current 34-entry corpus it ranges from 256 to 3,000 bytes, not 64 KiB.

The VMGP header also contains the requested additional data heap. The 16-bit
field at offset `0x04` stores `-mdata` bytes divided by four; the stack field at
offset `0x08` stores `-mstack` bytes divided by four. This was verified by
building minimal images with the official `pip-gcc`/`pip-ld` toolchain and
varying only those two options. `pip-ld --help` describes `--data=SIZE` as the
additional data-heap size and `--stack=SIZE` as the default stack size.

Across the current corpus, declared additional heap is 0-59,000 bytes for T310
and 0-52,180 bytes for T610. Replacing both fixed budgets with the image fields
and retaining a conservative 1 KiB MVM guard gives these target Cortex-M4
estimates. The current code still uses the pre-existing 4 KiB guard, so add
3 KiB to these runtime-pool figures:

| Corpus/profile | Minimum | Maximum | Average |
| --- | ---: | ---: | ---: |
| T310 | 39.6 KiB | 90.0 KiB | 67.8 KiB |
| T610 | 82.9 KiB | 108.0 KiB | 100.5 KiB |

Adding the current 7.4 KiB opaque context gives an approximate current
library-owned working-RAM range of **50.0-100.4 KiB for T310** and
**93.3-118.4 KiB for T610**, before the parent stack and hardware-driver/DMA
buffers. This fits the STM32F407VET6 internal RAM budget for the measured corpus,
although final SRAM/CCM placement and host-stack high-water measurement remain.

The image-provided heap value should be authoritative rather than replaced by a
parent guess. However, the current heap allocator is monotonic:
`vDisposePtr` validates a pointer but does not reclaim its block. The official
heap requirement may assume normal reuse of freed blocks, so the declared
capacity still needs high-water/allocation-failure validation and the allocator
may require correctness work rather than extra RAM.

### Regression Gate After Immediate Rendering

- focused Phase 12 smoke: 2/2 completed with valid recordings;
- full T310: `Runs/Phase12ImmediateFullT310/20260719_162502`, 21/21;
- full T610: `Runs/Phase12ImmediateFullT610/20260719_162502`, 13/13;
- no timeout, invalid opcode, missing syscall, memory fault, display failure, or
  fatal VM event was introduced;
- structured event totals and recording/audio/video coverage match the previous
  full checkpoint;
- four wall-clock-bounded runs reached normal `state=4,error=0` termination
  instead of remaining active at timeout; this is a timing outcome, not a VM
  error, and the same normal terminal state exists in earlier corpus baselines.

### Host Stack

`MVM_QueryMemReqsFromSourceWithConfig()` previously created a complete
473,344-byte `VMGPContext` local variable. It now uses a small query-only header
and reader state; Arm stack-usage output fell from 473,400 to 160 bytes.

After excluding that function, the largest current compiler-reported static
frame is 608 bytes (`MVM_lOpenSidecarFileStream`). Normal firmware still needs
measured call-chain, interrupt, and RTOS stack margin.

### Flash

The VM implementation objects contain approximately 46.8 KiB of code before
the final C library and math dependencies are linked.

After routing all guest-memory access through the checked segmented backend,
the Cortex-M4 object total is **51,515 bytes** of code/read-only data plus 32
bytes of object BSS. This is roughly a 4.7 KiB code cost for address translation
and bounds enforcement before final link-time garbage collection.

A complete generic bare-metal loop linked with newlib-nano and `--gc-sections`
currently measures:

- 71,736 bytes code/read-only data;
- 204 bytes initialized data.

The non-nano newlib probe was approximately 136 KiB of Flash. The current link
still includes file-oriented syscalls, formatted scanning/printing, time-zone
code, and substantial double-precision trigonometry. Flash is therefore not
the first F407 blocker, but feature separation can leave more space for BSP,
assets, and application code.

## Recommended Architecture Direction

### 1. Remove The Query-Time Full Context

Parse requirements into a small loader-query structure or caller-provided
scratch object. Do not temporarily construct `MpnVM_t`. This should remove the
462 KiB host-stack blocker without changing runtime behavior.

### 2. Separate Renderer State From Core VM State

The core context should keep registers, loader/runtime state, palette state,
stream tables, driver/config references, and small renderer state only.

For the MCU framebuffer mode, render operations should update the VM-owned
RGB565 framebuffer directly and accumulate a dirty rectangle. It should not
retain 2,048 desktop replay commands or 128 full palette snapshots.

The desktop backend can either use the same immediate framebuffer path or own
an optional external deferred-render workspace. This is also the strongest
reason to make the desktop runner exercise the same minimal public lifecycle as
the bare-metal port.

Expected context result before smaller secondary tables: approximately 6.25
KiB instead of 462.25 KiB.

### 3. Preserve Logical Addresses While Segmenting Physical Backing

The Prehistorik 96 KiB experiment disproved the assumption that heap and stack
budgets affect only capacity. The game stores the exact logical base `0x40000`
and reads byte `0x40030`; the legacy stack also lives above `0x41000`. These
addresses are part of the observable guest ABI even though most of the logical
256 KiB heap span does not need physical SRAM.

The current experiment therefore retains the legacy logical heap and stack
addresses while physically backing a 96 KiB low segment and the high region
from `0x40000` through the stack/guard. All guest access goes through a checked
mapping backend. Unmapped or overflowing requests emit `MVM_EVENT_MEMORY_OOB`
and stop the VM with an execution error instead of exposing an invalid host
pointer.

Manual gameplay subsequently completed the entire Prehistorik T310 demo level
and exited normally with the 96 KiB physical heap (`state=4,error=0`). The run
reached 83,352 bytes heap high-water and 75,248 bytes peak-live with no
allocation failure. Together with the 34/34 automated T310/T610 checkpoint,
this removes the previous contiguous-256-KiB guest-RAM blocker for the
STM32F407VET6 feasibility assessment.

The VMGP image fields should still drive the default soft requirement and work
with a precompiled library. Parent-owned runtime policy may add a margin or
impose a maximum, but must not silently change the logical ABI. Proposed inputs
remain:

- policy for accepting, capping, or adding margin to the declared guest heap;
- minimum guest stack and stack guard;
- policy to honor or reject the VMGP header stack request;
- optional framebuffer pointer/capacity supplied separately from guest memory.

The memory-query result must expose each contribution and reject unsupported
images deterministically.

### 4. Measure Before Reducing Heap

Add diagnostic-only high-water counters for guest heap, guest stack, runtime
pool, draw-command count, and palette snapshots. Run the focused gate, then the
full corpus. Validate that image-declared heap/stack values are sufficient and
detect where the current non-reclaiming heap diverges from expected behavior.

### 5. Support Explicit Memory Regions

A single contiguous runtime arena makes F407 bank placement unnecessarily
difficult. The integration descriptor should allow at least:

- guest RAM/decoded metadata workspace;
- display framebuffer;
- optional renderer workspace;
- opaque VM storage.

This allows CPU-only core state and stacks in 64 KiB CCM while keeping an LCD
DMA framebuffer in SRAM1/SRAM2. It also permits a small internal-RAM target now
and external FSMC RAM later without changing VM semantics.

### 6. Gate Desktop-Oriented Runtime Features

Separate embedded image/persistent callbacks from C `FILE` sidecar support and
avoid formatted I/O/time-zone dependencies in the minimal build. Keep math
imports required by a selected game, but allow unused import groups to be
excluded in documented precompiled-library variants if dispatch correctness is
preserved.

## Feasibility Assessment

The first internal-RAM STM32F407 bring-up is feasible if it uses the immediate
RGB565 renderer path. The image-declared budgets now indicate that the complete
current T310/T610 corpus may fit, although this is not proven until allocator
and bank-placement validation. A realistic preliminary budget after the
structural changes is:

- about 6-8 KiB core VM context;
- 39.6-90.0 KiB runtime pool across the measured T310 corpus;
- 82.9-108.0 KiB runtime pool across the measured T610 corpus;
- 15.8 KiB T310 RGB565 framebuffer, already included in those pool estimates;
- BSP, host/interrupt stack, DMA queues, and peripheral buffers in the remaining
  memory;
- approximately 72 KiB current newlib-nano VM integration Flash, before BSP.

Running every corpus title from internal RAM is not yet proven. The projected
worst current pool is 108.0 KiB including the T610 framebuffer, before adding
the approximately 6.25 KiB optimized core context and BSP. This is compatible
with the F407 total SRAM budget, but bank placement, DMA access, allocator
reuse, host stacks, and BSP buffers must still be validated on the real target.

## Safe Implementation Order

1. Add resource high-water measurements without changing capacities.
2. Run focused and representative/full corpus gates and record the results.
3. Replace the query-time full context with a small query structure.
4. Run regression gates and repeat the Cortex-M4 stack probe.
5. Split immediate framebuffer rendering from optional deferred desktop replay.
6. Run focused and full renderer regression gates.
7. Decode and honor VMGP heap/stack fields, add parent safety policy, and
   separate memory regions.
8. Select one small T310 bring-up image from measured data.
9. Integrate the real STM32F407VET6 BSP and produce a linker-map-backed RAM,
   Flash, stack, and frame-time report.
