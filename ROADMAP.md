# OpenMophun Roadmap

This file tracks current project status, open defects, and the remaining work.
Completed implementation history belongs in Git, not in this document.

## Project Direction

- Execute decrypted Mophun VMGP games through a portable C library.
- Keep the library independent from operating systems and concrete devices.
- Support source inclusion and precompiled static-library integration.
- Use parent-owned memory and explicit callbacks with no hidden allocation.
- Preserve deterministic bounded execution and fixed-date support.
- Target STM32F407VET6 for the first real embedded bring-up.

Original `.mpn` decryption is separate research. Real STM32 BSP integration is
reserved for Phase 14.

## Current Architecture

The library is located in `OpenMophun/` and contains:

```text
inc/
  MVM.h
  MVM_Types.h
src/
  private headers and implementation
OpenMophun.mk
README.md
LICENSE
```

Current integration contract:

- application code includes only `MVM.h`;
- one `MVM_InitConfig_t` configures one isolated `MVM_Instance_t`;
- the application owns instance storage and the runtime pool;
- image, sidecar, and save access uses one `MVM_FileApi_t`;
- display, input, audio, time, random, log, event, and message handling use
  `MVM_HostServices_t`;
- callbacks and configuration are per instance, with no global mutable config;
- execution is bounded through `MVM_RunStep()`, `MVM_RunSteps()`,
  `MVM_RunFrame()`, or `MVM_RunForTime()`;
- interactive hosts use `MVM_RunFrame()` and impose cadence through display
  synchronization, a timer, or an RTOS delay;
- the application integration file explicitly registers every filesystem and
  host-service callback; platform backends do not mutate `MVM_InitConfig_t`;
- diagnostic lines are assembled inside the library with per-instance runtime
  levels and relative timestamps, while the platform logger only prints the
  completed string;
- guest memory uses checked segmented backing while preserving legacy logical
  addresses;
- the renderer produces an RGB565 framebuffer and exposes only
  `display_flush`;
- audio is delivered as borrowed encoded beep/MIDI/AMR data;
- the library contains no SDL, Win32, stdio filesystem, or desktop path policy.

The desktop application in `Src/` is the current reference integration.

## Phase Status

| Phase | Status | Result |
| --- | --- | --- |
| 1. Naming and style | Complete | Stable `MVM_` C API and project conventions |
| 2. Execution model | Complete | Bounded execution and explicit VM states |
| 3. Memory model | Complete | Parent-owned instance/runtime memory |
| 4. Image/resources | Complete | Source-backed VMGP loading and streaming |
| 5. Logging/events | Complete | Callback logging and structured events |
| 6. Import boundary | Complete | Guest imports remain internal to the library |
| 7. SDK coverage | Ongoing | Required corpus imports implemented; gaps tracked below |
| 8. Device profiles | Complete | Per-instance profile-driven capabilities |
| 9. Desktop host | Complete | SDL/file/audio/input adapters outside the library |
| 10. Persistence model | Partial | Named files work; exact persistent-resource policy remains |
| 11. Corpus infrastructure | Complete | T310/T610 manifests, recordings, classifications |
| 12. External integration | Functionally complete | Only standalone-repository/submodule mechanics remain |
| 13. Decryption research | Planned | Original encrypted image support |
| 14. Minimal MCU port | Planned | Real STM32F407VET6 BSP and hardware execution |
| 15. Optimization/review | Planned | Performance and final portability review |

## Phase 12: Functional Closure

The API, local packaging, callback model, documentation, desktop integration,
Cortex-M4 cross-build, focused gameplay checks, and complete corpus regression
are accepted.

Latest complete checkpoint after runtime logging, explicit platform
registration, and frame-cadence API cleanup:

- `Runs/Phase12PostLoggingFullT310/20260727_002307` — 21/21;
- `Runs/Phase12PostLoggingFullT610/20260727_002307` — 13/13;
- zero process failures and timeouts;
- zero VM errors, OOB faults, invalid opcodes, and missing syscalls;
- zero allocation failures and tracker overflows;
- all 34 runs produced logs, recordings, audio, frames, and videos;
- maximum observed heap high-water: 87,508 bytes T310 and 45,128 bytes T610;
- terminal VM state and error match
  `Runs/Phase12FinalFullT310/20260726_190949` and
  `Runs/Phase12FinalFullT610/20260726_190949` for all 34 runs.

Wall-clock-bounded instruction counts remain diagnostic and may change with
host cadence or logging overhead. Terminal state, structured failures, memory
outcomes, and generated artifacts are the regression gates.

Compared with `Runs/Segmented96T310/20260719_195347` and
`Runs/Segmented96T610/20260719_195347`, three games reached guest
`vTerminateVMGP` within the time window instead of remaining active:

- Bouncy demo T310;
- HoneyCave2 T310;
- HoneyCave2 T610.

All three are normal `state=4,error=0` exits with process code zero. They are
treated as timing/cadence coverage differences, not VM failures. No
classification change is required.

Manual Phase 12 acceptance already covered Prehistorik Man, Alien Scum, Deep
Abyss, VRally2, snowboardx, Iceblox, and the known certificate-sensitive paths.
Existing visual/timing/certificate differences remain explicit backlog items
and are not regressions introduced by the integration work.

## Phase 12: Remaining Closure Work

Repository mechanics are deliberately the last Phase 12 implementation step:

- move `OpenMophun/` into the already-created standalone OpenMophun repository;
- reconnect it here as a Git submodule;
- require zero library-local patches from the parent repository;
- rebuild source-included and `libOpenMophun.a` variants;
- repeat Cortex-M4 compilation with `-Werror`;
- repeat focused smoke and complete T310/T610 regression gates.

The functional implementation is frozen. Phase 12 closes administratively when
the same source state is reintegrated as a submodule and all build/regression
gates remain clean.

## Accepted Phase 12 Decisions

- Public headers are limited to `MVM.h` and `MVM_Types.h`.
- All private `.c` and `.h` files are flat under `OpenMophun/src`.
- `OpenMophun.mk` is the only library build fragment.
- The public C prefix remains `MVM_`; OpenMophun is the project/library name.
- Historical Mophun, VMGP, SDK, and PIP terminology remains where technically
  accurate.
- One configuration structure and one instance descriptor are sufficient.
- The application `main.c` is the composition root and visibly registers every
  platform callback, including unsupported optional services as `NULL`.
- Multi-instance execution is supported when storage and callback contexts are
  separate; shared-device arbitration belongs to the application.
- Filesystem integration is a small callback table, not a VFS module.
- Raw flash image access is out of scope; the target uses a filesystem on an
  external flash card.
- Renderer command replay is private; applications receive RGB565 frames.
- Input is callback-only.
- Audio is callback-only and supplies borrowed encoded data.
- The 96 KiB physical guest heap with segmented high-memory backing is accepted
  for the current corpus.
- Real linker scripts, FatFS/BSP wiring, DMA placement, and execution on the
  STM32F407VET6 belong to Phase 14.

## Current Resource Baseline

Arm GNU Toolchain 15.3.1, Cortex-M4 Thumb, `-Os`, section splitting, and
`-Werror`:

| Resource | Measurement |
| --- | ---: |
| Library code and constants | 48,857 bytes |
| Initialized data | 0 bytes |
| Object BSS | 32 bytes |
| Opaque VM instance | 6,740 bytes |
| Largest static function frame | 592 bytes |
| Estimated T310 working RAM | 55.2–105.6 KiB |
| Estimated T610 working RAM | 98.5–123.6 KiB |

This fits the STM32F407VET6 total 512 KiB Flash and 192 KiB SRAM/CCM budget.
Final SRAM-bank, DMA-buffer, BSP, filesystem, interrupt-stack, and linker-map
validation remains a Phase 14 requirement.

## Open Defect Backlog

Machine-readable per-game outcomes remain in
`Tools/corpus/classifications.json`.

### Renderer and visual parity

- Deep Abyss system-font metrics and appearance differ from reference captures.
- VRally/VRally2 road, transition, HUD, text, and layer composition require
  parity work.
- HoneyCave/HoneyCave2 show vertical stripe or layer corruption.
- FiveStones retains visual parity defects.
- SpaceExplorer and 4in1 can lose background/playfield layers.
- snowboardx has black mask/text-like blit defects.
- SynergenixRally shows right-edge artifacts.
- CMRally4 shows black text/background rectangles.

### Timing and input cadence

- Several games run faster than reference devices.
- Tick cadence and scripted-input progression do not always match reference
  captures even when execution is deterministic.

### Certificates and licensing

- Exact certificate/date policy is incomplete.
- `vCheckDataCert` has multiple observed result conventions. The current
  argument-shape compatibility rule must eventually be replaced by verified
  signatures and certificate parsing.
- CMRally4 T610, VRally T610, BombJack T610, 1849 Gold Rush T610, and Lunar T610
  retain certificate/demo-policy gaps.
- Embedded validity tags such as `M001YYYYMMDD` and `M002YYYYMMDD` are not yet
  parsed automatically.

### VM task and termination semantics

- `vTerminateVMGP` currently terminates the complete VM.
- Reference VRally T610 behavior indicates task-local termination followed by
  another task.
- Investigate the minimal deterministic model for `vCreateTask`, `vThisTask`,
  `vKillTask`, `vTaskAlive`, and task-local termination.

### Storage and persistence

- Save/sidecar files use the external file API, but exact persistent writable
  resource policy remains incomplete.
- Corpus persistence should use disposable per-run storage instead of modifying
  source game images.

### SDK/import coverage

- Known zero stubs and partially modeled imports must remain visible in logs and
  structured events.
- Add an import to the implemented catalog only after its ABI and return
  semantics are understood.
- Verify that VRally2 Help and other guest system UI requests reach
  `system_message` with correct bounded text and acknowledgement results.

### Corpus/reference coverage

- Some games still lack reference video, so visual parity cannot be judged
  conclusively.
- `prhstrkmn_T610` remains an expected unsupported-device/profile outcome.

### Test coverage

Add focused automated tests for:

- VMGP header, pool, string, and resource parsing;
- decompression;
- representative PIP instructions and bounds failures;
- file stream open/read/write/seek/close behavior;
- allocator reuse, invalid free, and double free;
- memory query and instance alignment;
- callback validation and multi-instance isolation.

## Phase 13: Decryption Research

- Compare encrypted and decrypted image pairs.
- Document relevant headers, transformations, and validation.
- Keep decryption separate from VM execution correctness.
- Add an input stage only after the format is understood and legally suitable.

## Phase 14: STM32F407VET6 Bring-up

- Integrate the existing BSP and linker script.
- Implement external-card filesystem callbacks.
- Place DMA-visible framebuffer/buffers in SRAM1/SRAM2.
- Place suitable CPU-only state in CCM.
- Run the bounded loop from bare metal or an RTOS task.
- Measure final Flash, runtime pool, stack high-water, interrupt margin, and CPU
  time from the linker map and hardware.
- Run at least one real game through input, display, storage, and normal exit.

## Phase 15: Optimization and Final Review

- Profile opcode, import, renderer, and memory mapping hot paths.
- Consider jump-table or indexed dispatch where measurement justifies it.
- Remove release-inactive diagnostics through compile-time policy.
- Review integer overflow, alignment, endian handling, bounds checks, and
  callback contracts.
- Re-run complete regression gates after every behavior-affecting optimization.

### Final provenance, licensing, and acknowledgements audit

Perform this only at the end of the project, before public release:

- freeze the audited OpenMophun, MoRePhun, and nofun revisions;
- inventory the official documentation and community references used during
  development, together with their licenses and copyright notices;
- run exact, normalized, and token-based source similarity checks;
- manually review high-risk VMGP, PIP, decompression, import, renderer,
  certificate, allocator, font, and constant-table matches;
- classify each material match as specification-derived, common idiom,
  independently derived, licensed adaptation, or unresolved;
- remove, independently reimplement, or license and attribute any material
  adaptation as required;
- record the procedure, findings, decisions, and remaining uncertainty in a
  code-provenance audit report;
- only after the audit, revise README wording about implementation origin, add
  acknowledgements for verified reference projects and authors, and add any
  required license or NOTICE material;
- do not claim strict clean-room development or absence of copied code unless
  the completed audit supports that statement.
