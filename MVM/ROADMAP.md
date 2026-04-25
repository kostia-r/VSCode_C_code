# Mophun VM Roadmap

This document fixes the current technical plan for turning the VM PoC into a
portable embeddable component. It should be updated after each milestone, not
treated as a frozen specification.

## Goals

- Run decrypted Mophun VMGP games on desktop for debugging.
- Keep the VM core platform-independent.
- Make the same VM component portable to STM32/FreeRTOS, Infineon TriCore, and
  other embedded targets.
- Provide deterministic memory and timing options for firmware builds.
- Keep host integration small: include `MVM/vm.mk`, allocate VM storage, provide
  platform callbacks, and run the VM from a task/thread/main loop.

## Current Baseline

- VM code is split into `core`, `loader`, `pip`, `runtime`, and `debug`.
- `MpnVM_t` is opaque to host code.
- Component style rules are captured in `STYLE_GUIDE.md`.
- VM source and header files use the `MVM_` prefix.
- Public VM APIs use the `MVM_` function prefix.
- Public storage API exists:
  - `MVM_GetStorageSize()`
  - `MVM_GetStorageAlign()`
  - `MVM_GetVmFromStorage()`
- Platform callbacks exist for logging, ticks, and random.
- Runtime syscalls are split by domain.
- Host/platform bindings are now centralized in `Config/` instead of being overridden through a public syscall registration API.
- Trace and VMGP debug APIs are separate from the core API.

## Phase 1: Naming And Code Style

Status: baseline pass done. Further module-by-module formatting can continue
when files are touched for functional work.

Purpose: make the component match the target embedded codebase style before the
API surface grows further.

Tasks:

- Read and summarize `2.+Naming+conventions.docx`.
- Read and summarize `3.+Styling.docx`.
- Decide final component prefix: `MVM_`.
- Define naming rules for:
  - public APIs;
  - internal APIs;
  - translation-unit functions;
  - static/local functions and variables;
  - types, enums, constants, macros.
- Rename files and symbols consistently.
- Keep a compatibility note if old names are temporarily kept.

Local rules are captured in `STYLE_GUIDE.md`.

Done when:

- New code follows one naming convention.
- Public headers expose one stable style.
- Build and smoke test still pass.

## Phase 2: Execution Model

Status: done. Public bounded execution APIs are in place, the desktop runner
drives the VM from a non-blocking host loop, and the VM state model now covers
ready/running/paused/waiting/exited/error transitions.

Purpose: avoid a blocking VM loop that can starve an MCU scheduler.

Tasks:

- Split execution API into:
  - init/free;
  - single-step;
  - run for N steps;
  - run for time/cycle budget.
- Move trace-only execution behind debug/trace API.
- Add VM state model:
  - ready;
  - running;
  - paused;
  - waiting;
  - exited;
  - error.
- Add APIs:
  - pause;
  - resume;
  - request exit;
  - get state;
  - get last error.

Done when:

- A FreeRTOS task can call the VM for a bounded amount of work and yield.
- Desktop runner can drive the VM from its own non-blocking host loop.

## Phase 3: Static Memory Model

Status: done. The VM can now report memory requirements before init, use one
host-owned runtime pool supplied through the integration config, and fail
deterministically with a memory error when that pool is undersized or missing.

Purpose: make RAM usage deterministic and remove hidden allocation from firmware
builds.

Tasks:

- Define the runtime-pool-backed integration memory model.
- Separate memory areas:
  - VM context storage;
  - guest RAM;
  - stack;
  - heap;
  - pool/resource tables;
  - stream buffers;
  - decompression scratch;
  - optional MPN cache.
- Support static memory arenas supplied by the host.
- Add compile-time options to forbid default allocator/logger.
- Make memory sizing inspectable before init.
- Add runtime asserts/errors for insufficient memory.

Done when:

- VM can initialize without `malloc/calloc/free`.
- RAM requirements for the current game can be printed or queried.
- Firmware builds can fail at compile/init time if buffers are undersized.

## Phase 4: Image And Resource Provider

Status: done. The VM now accepts source-backed images, fetches code/pool/string
table data through compile-time configured image-backend callbacks, and reads
resource streams directly from the backing image without mirroring the full
resource section into guest RAM.

Purpose: avoid requiring the entire `.mpn` file in RAM.

Tasks:

- Define image source/backend callbacks:
  - read file/image range;
  - read resource range;
  - optional map/unmap range;
  - optional cache hints.
- Refactor loader/runtime streams to use image-backend callbacks.
- Support full-image RAM mode for desktop.
- Support small window/cache mode for MCU.
- Make cache buffer size configurable.

Done when:

- Desktop can still load from a full memory buffer.
- Embedded target can stream from flash/FS/external storage.
- Resource reads do not require all resources to be copied into guest RAM.

## Phase 5: Logger And Events

Status: done. The VM now uses level-gated logging macros with a compile-time
max log level, the platform logger receives level/module/event/message
metadata, and structured VM events are emitted for lifecycle transitions,
import dispatch, missing syscalls, invalid opcodes, memory faults, resource
open/read activity, frame-ready notifications, and sound requests.

Purpose: make diagnostics usable without paying runtime cost when disabled.

Tasks:

- Replace direct formatted logging with level-gated macros.
- Add log levels:
  - error;
  - warning;
  - info;
  - debug;
  - trace.
- Add compile-time max log level so disabled messages compile out.
- Pass level/module/event metadata to the logger callback.
- Review `logger_example/SysLog` and align adapter shape.
- Add VM event callbacks:
  - import called;
  - missing syscall;
  - invalid opcode;
  - memory out of bounds;
  - resource opened/read;
  - frame ready;
  - sound requested;
  - VM paused/resumed/exited/error.

Done when:

- Disabled logs do not format strings.
- Host can filter logs by level.
- Important VM events can be consumed without parsing text logs.

## Phase 6: Syscall And Platform Model

Purpose: define a clean contract between game imports, VM runtime, and host
platform backends.

Tasks:

- Document what VMGP imports are.
- Define syscall categories:
  - graphics;
  - audio;
  - input;
  - streams/resources;
  - memory;
  - strings;
  - timing/random;
  - system/control;
  - debug.
- Mark each syscall as:
  - synchronous with result;
  - synchronous fire-and-forget;
  - asynchronous request;
  - polling;
  - unsupported/stub.
- Define callback blocking rules.
- Define ownership rules for buffers and pointers.
- Keep host syscall override registration.
- Add default SDK stubs with documented return values.

Done when:

- Every known SDK syscall has an entry and status.
- Missing syscalls are reported deterministically.
- Host can replace any syscall implementation.

## Phase 7: SDK Catalog

Purpose: make runtime completeness measurable.

Tasks:

- Extract syscall names, signatures, and descriptions from official SDK docs.
- Create a machine-readable syscall catalog.
- Add status per syscall:
  - implemented;
  - partial;
  - stub;
  - unknown;
  - not applicable.
- Link runtime implementation files to catalog entries.
- Generate or maintain a human-readable table.

Done when:

- We can answer "which SDK APIs are implemented?" from the repo.
- New games can report missing APIs against the catalog.

## Phase 8: Device Profiles

Purpose: remove hardcoded T310 capability values from runtime logic.

Tasks:

- Define `MVM_DeviceProfile`.
- Add profile fields:
  - screen size;
  - color depth/palette mode;
  - key layout;
  - timing;
  - memory limits;
  - audio capabilities;
  - supported caps.
- Implement SonyEricsson T310 profile first.
- Make `vGetCaps` profile-driven.

Done when:

- T310 values are not hardcoded in syscall handlers.
- Adding another SonyEricsson profile does not touch PIP/core.

## Phase 9: Windows Platform Backend

Purpose: run games interactively on desktop, not only trace them.

Tasks:

- Create Windows backend under `Examples/platform/win32`.
- Implement:
  - logging;
  - ticks/random;
  - input;
  - screen/framebuffer;
  - basic palette/graphics stubs;
  - stream/resource access;
  - audio stub.
- Keep `Src/main.c` as a thin runner or move it to examples.

Done when:

- The target game reaches visible/menu state through our backend.
- Input can be injected or read from keyboard.

## Phase 10: Game Corpus And Regression Runner

Purpose: use multiple games to discover missing opcodes and APIs.

Tasks:

- Add a local corpus runner script.
- For each game collect:
  - load result;
  - steps executed;
  - missing imports;
  - unhandled opcodes;
  - crash/error reason;
  - first N MVM/syscall logs.
- Produce summary output, preferably CSV or JSON.
- Add regression comparison for known-good traces.

Done when:

- Running a batch of games is one command.
- API gaps are visible as a prioritized list.

## Phase 11: Decryption Research

Purpose: eventually load original encrypted `.mpn` files, not only decrypted
ones.

Tasks:

- Keep this separate from VM correctness work.
- Compare decrypted/original samples.
- Inspect official simulator/emulator behavior.
- Document file formats and keys/headers when understood.
- Add decrypt stage before VMGP loader only when stable.

Done when:

- Loader can accept original files or a documented decrypted image source.

## Phase 12: Minimal MCU Port

Purpose: prove portability on a constrained embedded target.

Tasks:

- Choose first target board/toolchain.
- Build VM without desktop default allocator/logger.
- Provide static storage and platform callbacks.
- Run bounded VM steps from a task or main loop.
- Measure RAM, stack, and CPU time.

Done when:

- VM builds and runs a simple trace or smoke case on target hardware/simulator.
- RAM/stack numbers are documented.

## Phase 13: Optimization And Final Review

Purpose: improve performance only after behavior and boundaries are stable.

Tasks:

- Profile opcode dispatch and syscall dispatch.
- Consider jump tables for opcodes.
- Consider import index/hash dispatch instead of repeated string compare.
- Inline hot memory helpers where useful.
- Remove unused debug code from release builds.
- Review endian, alignment, integer overflow, and bounds checks.
- Review integration for STM32, TriCore, Arduino-class targets.

Done when:

- Release build has minimal platform assumptions.
- Resource usage is known and acceptable for target class.

## Unit Tests

Add focused tests as modules stabilize:

- VMGP header parsing.
- Pool parsing.
- Resource table parsing.
- LZ decompression.
- Selected PIP opcodes.
- Syscall dispatch and host override.
- Static storage size/alignment.
- Stream open/read/seek/close.
- Logger compile-time filtering.

## Resource Estimate Tracking

Keep this section updated with real measurements.

Current rough model:

- VM context: several KB.
- Pool/resources: depends on game.
- Guest memory currently includes data, bss, resources, heap extra, stack extra.
- Current source-backed build keeps guest RAM focused on `.data`, `.bss`, heap,
  and stack, while code/resources are read from the image source on demand.

Measurements to add:

- `sizeof(MpnVM_t)`.
- Guest memory size for target games.
- Max host stack depth during VM run.
- Per-frame or per-step CPU time on desktop and MCU.

## Near-Term Order

1. Apply naming/style conventions.
2. Add bounded execution API and VM state model.
3. Replace dynamic allocation path with full static memory config.
4. Add level-gated logger and event callbacks.
5. Build SDK syscall catalog and default stubs.
6. Add T310 device profile.
7. Build Windows platform backend.
8. Run game corpus and fill missing APIs.
9. Start minimal MCU port.
