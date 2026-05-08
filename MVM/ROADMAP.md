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

Status: done. The VM now exposes a public import/syscall contract model through
`MVM_Imports.h`, categorizes known imports by subsystem and guest-visible call
model, documents implementation status and default stub return values, and
keeps the replacement boundary explicit between VM-owned runtime imports and
platform-owned imports bound in `Config/MVM_Lcfg.c`.

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
- Keep host/platform import replacement at the integration-config boundary.
- Add default SDK stubs with documented return values.

Done when:

- Every known SDK syscall has an entry and status.
- Missing syscalls are reported deterministically.
- Host can replace any platform-owned import implementation through the
  integration config.

## Phase 7: SDK Coverage And Import Documentation

Status: done. The official SDK documentation remains the canonical API source,
while the runtime import layer now carries one handler per import together with
short SDK-based contract comments and explicit implementation status in the
source file.

Purpose: make runtime completeness measurable without maintaining one duplicated
catalog separate from the SDK.

Tasks:

- Extract import names, signatures, and intent from official SDK docs.
- Keep SDK-based contract comments above import handlers.
- Mark each import in code as:
  - implemented;
  - partial;
  - stub.
- Keep the import binding layer centralized so implementation coverage is
  visible from one place.
- Use game import dumps against `MVM_Imports.c` to identify missing APIs.

Done when:

- We can answer "which SDK APIs are implemented?" from the repo.
- New games can report missing APIs against the import binding layer.

## Phase 8: Device Profiles

Status: done. The VM now exposes a stable device-profile model, ships with
SonyEricsson T310 and T610 profiles, selects profiles through the normal init
path, and resolves supported `vGetCaps` queries from profile data instead of
hardcoded device constants in runtime logic.

Purpose: remove hardcoded T310 capability values from runtime logic and make
profile selection a stable part of integration.

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
- Implement SonyEricsson T310 and T610 profiles first.
- Make `vGetCaps` profile-driven.
- Keep profile identity stable so later save/snapshot data can be tied to the
  active profile.

Done when:

- T310/T610 values are not hardcoded in syscall handlers.
- Adding another SonyEricsson profile does not touch PIP/core.
- Host can select one profile without patching runtime logic.

## Phase 9: Windows Platform Backend

Purpose: run games interactively on desktop, not only trace them, and push at
least one target game to a genuinely playable state.

Status: functionally complete pending final cleanup commit.

Tasks:

- Done: create the Windows desktop backend in the current `Src` runner layout.
- Done: use SDL2 for:
  - window creation;
  - framebuffer presentation;
  - keyboard input;
  - audio output.
- Done: build one host event loop around the bounded VM execution API.
- Done: connect keyboard input to `vGetButtonData` / `vTestKey`.
- Done: implement framebuffer present through `vFlipScreen`.
- Done: implement basic 2D drawing/import behavior needed for menu/UI flow:
  - `vClearScreen`;
  - `vFillRect`;
  - `vDrawLine`;
  - palette operations;
  - clip window and transfer mode handling.
- Done: implement the object/sprite drawing path needed by the current game:
  - `vDrawObject`;
  - visible map/sprite update flow;
  - palette-aware rendering.
- Done: implement enough text/font behavior for menu and HUD readability:
  - `vSetActiveFont`;
  - `vPrint`.
- Done: add audio handling for `vPlayResource`:
  - done: queue SDK-shaped `vPlayResource(data, length, flags)` requests and
    expose safe guest-memory reads for platform backends;
  - done: handle `SOUND_FLAG_STOP`, non-stream BEEP sequences, and MIDI/SMF
    request detection in the SDL backend;
  - done: render MIDI through the embedded TinySoundFont/TinyMidiLoader path
    using the default nofun soundfont; Windows MCI and the diagnostic marker
    tone remain fallback paths only.
- Done: run one playability pass against the target game:
  - start screen;
  - menu flow;
  - gameplay input;
  - visible rendering correctness;
  - sound playback.
- Done: track and fix target-game graphics regressions found during Phase 9:
  - keep the cleared MPN persistent/second-run flag as the root fix for the
    full-screen lava corruption;
  - revert memory-model experiments that switched desktop runs back to host
    heap allocation while the static allocation path is the target baseline;
  - revert deferred SDL/rendering changes that were added only to chase the
    full-screen lava symptom;
  - done: fix loading-screen level text so level 1 is shown as `LEVEL 01`;
  - done: fix loading-screen text artifacts in `LEVEL`, `REQUIRED`,
    `PERFECT`, and `START`;
  - done: restore a visually close loading-screen reveal effect without
    blocking Phase 9 on exact dirty-tile mechanics;
  - known difference: the loading-screen reveal is now visually close enough
    for Phase 9 playability, but its mechanics still differ from the reference
    emulator. The reference appears to use a dirty/intermediate tile update
    path; the current backend keeps full tilemap emission to avoid gameplay
    regressions such as stale edge pixels and sprite/map lag. Keep this tracked
    for a later renderer-accuracy pass instead of blocking Phase 9 progress;
  - done: restore the bottom lava bubbling/animation; keep sprite/map layer
    order under comparison, but bubbles now emerge from the lava as expected;
  - done: keep a clean comparison set: reference video/log, current video/log,
    and fixed input scripts; latest artifacts live in
    `C:\mophun\MY\videos&logs`;
  - done: final playability recording
    `C:\mophun\MY\videos&logs\Recording 2026-05-08 235410.mp4` reaches
    gameplay, death/restart flow, level complete, and level 2 without VM
    errors; latest log ends with `state=4 error=0`.
- Done: keep `Src/main.c` as a thin runner and move platform/VM/render logic
  into `VmRunner`, `SdlBackend`, and library-side render replay helpers.
- Done: keep real error/fallback messages while routing diagnostics through
  level-gated debug logging before the Phase 9 close commit.

Done when:

- The target game reaches visible/menu state through the backend.
- Input works through the real keyboard path.
- Graphics are rendered through the backend instead of log-only fallbacks.
- At least one target game can be played at a basic level on desktop.
- The target game level intro and first gameplay screen match the reference
  for level text, font pixels, lava animation, and map/sprite layer order,
  except for the documented loading-screen reveal mechanics difference.

## Phase 10: Persistent Data And Snapshot Support

Purpose: support game progress persistence and, later, optional full VM
suspend/resume.

Status: done for persistent game-owned data. Writable VMGP resource streams use
a dirty overlay and flush it back through an optional image-write backend on
close/free. The default desktop file-backed integration opens `.mpn` files
read/write when possible, so modified resource payload bytes persist in-place.
Full VM snapshots are explicitly deferred as a separate optional layer.

Tasks:

- Done: add an optional image-source write callback for persistent resource data.
- Done: mark modified resource overlays dirty and flush them on stream close and
  VM free.
- Done: keep persistence at game-owned resource data level, without register or
  VM-state snapshots.
- Deferred: define a stable game identifier for host-managed persistent records:
  - content hash;
  - image metadata fingerprint;
  - avoid relying on file name alone.
- Deferred: add persistent-data export/import APIs for host-managed NvM storage
  when a target needs storage outside the writable image backend.
- Deferred: define record metadata:
  - game id;
  - profile id/name;
  - format version;
  - payload size;
  - integrity check.
- Done: decide what belongs to the persistent payload:
  - game-owned save data;
  - selected VM-owned metadata only when required;
  - no register snapshots.
- Done for current model: keep host-side storage ownership outside the VM core:
  the VM only calls the configured image-write backend.
- Deferred for non-image storage:
  - export one record/blob;
  - import one previously stored record/blob.
- Deferred: explore optional full snapshot APIs for exact suspend/resume:
  - registers and execution state;
  - guest RAM;
  - allocator metadata;
  - stream/resource runtime state;
  - random/timing state where needed.
- Deferred: define snapshot compatibility rules:
  - same game image;
  - same device profile;
  - same snapshot format version.

Done when:

- Game-owned resource data persists across VM runs through the configured image
  backend.
- Desktop runner can preserve progress in-place for writable `.mpn` files.
- Snapshot support is documented as a separate optional layer.

## Game Corpus And Regression Runner

Purpose: use multiple games to discover missing opcodes and APIs.

Prerequisite infrastructure before starting broad corpus work:

1. Define the run data model and directory layout:
  - choose the manifest format;
  - choose the input-scenario format;
  - choose the output layout under `Runs/Corpus/`;
  - define stable run ids from date/time, game file, profile, and scenario.
2. Define a corpus manifest as the source of truth for automated runs:
  - explicit `.mpn` game file path;
  - explicit target device profiles per game, starting with `SE_T310` and
    `SE_T610`;
  - per-run duration or step/log limits;
  - per-game JSON input scenarios, including a no-input scenario for first-load
    smoke runs.
3. Add the no-input smoke path first:
  - run each manifest entry for a fixed duration without synthetic key input;
  - prove that logs and process timeouts work before adding scripted input.
4. Add a batch runner/orchestrator that reads the manifest and runs every
  `game + profile + input scenario` combination sequentially.
5. Store every run under a timestamped output directory with stable names derived
  from date/time, game file, profile, and scenario.
6. Write one log file per run, not one shared overwritten log:
  - include game path;
  - selected profile;
  - selected input scenario;
  - start/end wall-clock time;
  - exit code;
  - executed steps;
  - final VM state/error;
  - missing imports/unhandled opcodes when present.
7. Produce a summary file for the whole batch, preferably CSV or JSONL, so the
  corpus can be sorted by failures and missing APIs.
8. Add scripted input scenarios:
  - support a `none` scenario that sends no synthetic key input for initial
    boot/load diagnostics;
  - keep game-specific button sequences in the corpus JSON manifest so each
    game can define its own start/menu/gameplay probes;
  - support timed waits and key presses/releases with durations;
  - support Mophun button names rather than desktop key names, for example
    `UP`, `DOWN`, `LEFT`, `RIGHT`, `FIRE`, `FIRE2`, and `SELECT`;
  - document the mapping between manifest button names and desktop keyboard
    keys used by the SDL runner;
  - allow game-specific scenarios such as start-menu navigation, first gameplay
    movement, death/restart, and level-transition probes.
9. Add synthetic input support in the desktop runner/backend:
  - keep physical keyboard input working;
  - combine physical input with the scripted synthetic button mask;
  - drive scripted input from monotonic run time, independent of host frame
    rate where practical.
10. Add built-in recording support in the C runner/backend:
  - capture rendered frames from the SDL logical framebuffer or render target;
  - mirror queued PCM audio into a recording stream at the backend audio sample
    rate;
  - write deterministic intermediate artifacts such as raw frames and WAV audio
    directly from C;
  - optionally use a post-run tool such as `ffmpeg` only to mux/encode the final
    `.mp4`, keeping the captured video/audio source inside the runner.
11. Keep recording optional through runner arguments so normal interactive runs
  remain lightweight.
12. Document the one-command entry point for corpus preparation, for example a
  script that builds the runner and executes the manifest into `Runs/Corpus/`.
13. Run a small pre-Phase-11 validation set before the real corpus:
  - one known-good game;
  - both `SE_T310` and `SE_T610`;
  - `none` input;
  - at least one scripted input scenario;
  - logs, summary, raw recording artifacts, and final video present for every
    run.

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
- Use corpus runs to prioritize missing SDK imports and incomplete backend
  behaviors.
- Triage each corpus batch into concrete fix tickets or checklist items:
  - loader/format issues;
  - missing or incorrect opcodes;
  - missing SDK imports;
  - runtime/platform behavior gaps;
  - renderer, input, timing, persistence, or audio regressions.
- Fix the highest-priority failures found by corpus runs inside the same Phase
  11 feedback loop.
- Re-run the affected games and profiles after each fix and keep before/after
  logs or videos when they explain the regression.
- Promote stable scenarios to regression checks so fixed failures do not return.

Done when:

- Running a batch of games is one command.
- Each run has its own log and video/audio diagnostic artifact.
- The manifest explicitly records which profiles and input scenarios were used.
- API gaps are visible as a prioritized list.
- Corpus findings have an active fix-and-rerun loop, not just archived logs.
- A fixed corpus failure can be verified by re-running the same manifest entry.

## Phase 12: External Config And Submodule-Friendly Integration

Purpose: make the VM easy to consume as a git submodule without modifying files
inside the library tree, after the integration boundaries have been validated
by a real host backend. The target integration experience should be close to
LVGL-style porting: the VM is added as a library, while the host provides a
small, explicit platform/driver API instead of editing VM internals.

Tasks:

- Separate bundled default config from project-owned integration config.
- Support external `MVM_Cfg.h` / `MVM_Lcfg.c` supplied by the parent project.
- Keep bundled config as a fallback for local smoke tests and examples.
- Add build hooks so the parent project can point the VM build at an external
  config directory.
- Minimize assumptions in `vm.mk` about runner layout and top-level project
  structure.
- Write a minimal integration guide for parent projects.
- Document platform callback interfaces and backend responsibilities.
- Shape the host-facing port layer around simple driver-style APIs:
  - display/framebuffer present or draw callbacks;
  - input state polling or event injection;
  - audio sample queue/playback callbacks;
  - time/tick provider;
  - random provider;
  - logging/event sink;
  - storage/image read and optional write callbacks;
  - static memory/runtime-pool ownership.
- Keep the minimal embedded integration path obvious:
  - add VM sources/include paths;
  - provide one config object;
  - allocate VM storage and runtime pool;
  - initialize from an image source;
  - call bounded VM execution from the host loop/task;
  - flush display/audio/input through the platform driver callbacks.
- Provide a tiny reference port template similar in spirit to an LVGL display
  and input driver skeleton, without SDL or desktop-only dependencies.
- Add one small static architecture/data-flow overview for integrators.
- Add compile-time validation for invalid external-config combinations where
  possible.
- Document the expected parent-project responsibilities:
  - provide config files;
  - provide platform backend callbacks;
  - provide image source setup;
  - allocate VM storage.
- Verify that the library can be dropped in as a git submodule without editing
  library-owned files.

Done when:

- A parent project can integrate the VM as a submodule using external config
  files.
- Platform-specific configuration no longer requires patching the library tree.
- A new platform port can start from a small driver-template file rather than
  copying the desktop runner.
- The mandatory host API surface is small enough to document on one integration
  checklist.

## Phase 13: Decryption Research

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

## Phase 14: Minimal MCU Port

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

## Phase 15: Optimization And Final Review

Purpose: improve performance only after behavior and boundaries are stable.

Tasks:

- Profile opcode dispatch and syscall dispatch.
- Consider jump tables for opcodes.
- Consider import index/hash dispatch instead of repeated string compare.
- Consider X-macro/codegen support for opcode metadata or dispatch tables.
- Inline hot memory helpers where useful.
- Remove unused debug code from release builds.
- Review endian, alignment, integer overflow, and bounds checks.
- Expand compile-time asserts/preprocessor guards for invalid configuration.
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

Also add higher-level coverage over time:

- integration tests for import/platform/backend boundaries;
- smoke/system tests for representative games and host backends.

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
6. Finish device profiles and make `vGetCaps` fully profile-driven.
7. Build Windows platform backend and get at least one game running with real graphics/input/audio flow.
8. Add persistent game-owned resource data support and define snapshot boundaries.
9. Prepare the corpus manifest runner, scripted input scenarios, per-run
   logs, and C-side recording pipeline.
10. Run game corpus and fill missing APIs.
11. Make config externalizable for submodule-style integration.
12. Start minimal MCU port.
