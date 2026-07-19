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
- Track SDL system-font accuracy as a Phase 11 renderer follow-up:
  - replace the current host debug-text fallback with SDK/reference-shaped
    system font rendering and metrics for `vTextOut`/`vTextOutU`;
  - verify DeepAbyss 1.1/1.4 menu, loading, and death-screen text against the
    reference emulator after corpus stability issues are under control.
- Current system-font findings:
  - `DeepAbyss 1.4` menu text uses the SDK system-font path through
    `vSelectFont(size=0, flags=0)` plus `vTextOutU`, i.e. the `Normal` face;
  - `VRally` menu text uses `vSelectFont(size=1, flags=0x10000002)` plus
    `vTextOutU`, i.e. `Small + Bold + ShadowLowerRight`;
  - the screenshot-derived `fontsPic` atlas is useful, but current comparison
    against real-phone `DeepAbyss 1.4` screenshots shows it is closer to a
    small-face candidate than to the final normal face;
  - the runtime now keeps explicit system-font face slots, but `Normal` is
    still intentionally wired to a placeholder and `Small` to a candidate
    atlas until reference-matched faces are reconstructed;
  - the old desktop emulator is not a reliable source of authentic device
    bitmap glyphs: inspection shows a GDI-backed font path with configurable
    `SysFont` / `Normal` / `Small` / `Large` names rather than embedded Sony
    Ericsson bitmap tables.
- Track MVM-level platform events for globally important runtime outcomes:
  - device/profile not supported by the guest or corpus classification;
  - data certificate accepted/rejected/expired;
  - license or sidecar resource-pack mismatch;
  - keep these as structured events so hosts do not need to parse guest
    message-box text such as "Terminal not supported".
- Treat data certificates as first-class corpus metadata:
  - scan embedded certificate resources in `.mpn` images first;
  - scan sidecar `.mpc` files as an additional path; at the current checkpoint
    only `VRally` sidecars are actually present in the corpus, while
    `IcebloxPlus610`, `lunar`, `Huntsman`, and `prhstrkmn` reference their own
    sidecar names from inside the `.mpn` but the matching files are absent;
  - parse observed `M001YYYYMMDD`/`M002YYYYMMDD` validity tags and log the
  selected fixed date;
  - allow per-game fixed-date overrides until automatic certificate-date
    selection is implemented;
  - investigate the SDK `vlDataCert*` functions and map them to MVM imports if
    a title calls them directly.
- Implemented first platform-outcome event layer:
  - `MVM_EVENT_DATA_CERT_CHECKED` is now emitted by the existing
    `vCheckDataCert` / `vCheckDataCertFile` import paths;
  - `MVM_EVENT_LICENSE_EXPIRED` and `MVM_EVENT_DEVICE_UNSUPPORTED` are now
    emitted for the currently observed guest `vMsgBox()` outcomes so corpus
    consumers no longer need to parse those message strings directly;
  - `MVM_EVENT_SIDECAR_MISSING` is now emitted when a guest requests a
    read-only sidecar-like file that is absent beside the active image;
  - expiry screens rendered fully by the guest, such as `Huntsman`, still need
    real certificate/date interpretation before they can be classified without
    visual inspection.
- Current corpus checkpoint:
  - 2026-07-18 reference-input corpus reruns after Phase 11 close-out fixes:
    - T610: `Runs/RefInputT610/20260718_232534`;
    - T310: `Runs/RefInputT310/20260718_233046`;
    - both batches completed without runner timeouts; the saved T310 summary
      has one `snowboardx_T310` process exit 2 with `LDBU addr OOB`, while all
      other entries have process exit code 0;
    - no `invalid-opcode` or `missing-syscall` failures were observed in these
      rerun logs;
    - C-side recording audio is now trimmed to the recorded video interval at
      WAV finalization, fixing long-audio MP4s such as `4in1`,
      `SpaceExplorer`, and `SnowBob`;
    - `vCheckDataCert` / `vCheckDataCertFile` now return SDK-style zero for an
      accepted certificate and non-zero only for invalid input/handles; this
      removes the false `license-expired` path for the observed
      `IcebloxPlus610` and `Huntsman` sidecar-missing cases;
    - exact certificate/date policy remains incomplete and is moved to backlog
      for titles that still diverge through guest-controlled flow after the
      accepted-cert result, including `lunar`, `CMRally4`, `VRally`,
      `1849GoldRush`, and `bombjack`;
  - 2026-07-18 reference-input comparison baseline:
    `Runs/RefCompare_20260718/analysis.md`;
  - 2026-05-15 full-manifest early-exit recheck:
    `Runs/CorpusEarlyExitRecheck/20260515_204820`;
  - this batch ran all 32 manifest entries with a 30-second duration override
    specifically to revalidate startup/early-exit behavior after later fixes;
  - the fresh short-exit classification is now:
    - `IcebloxPlus610_T610`: missing `IcebloxPlus610.mpc`, then
      `vMsgBox("The game has expired")` and `vTerminateVMGP()`; classify as
      `incomplete_artifact_set`;
    - `lunar_T610`: missing `lunar.mpc`, then `vMsgBox("Game Expired")` and
      `vTerminateVMGP()`; classify as `incomplete_artifact_set`;
    - `Huntsman_T610`: missing `Huntsman.mpc`, then renders its own
      `GAME EXPIRED` screen and calls `vTerminateVMGP()` after about 6 seconds;
      classify as `incomplete_artifact_set`;
    - `prhstrkmn_T610`: `vMsgBox("Terminal not supported!")`, then
      `vTerminateVMGP()`;
  - `snowboardx_T310` no longer reproduces the old short-exit symptom in the
    current batch and runs through the full 30-second probe window;
  - several apparent early stops in the 2026-05-15 batch are not guest exits
    at all: they end with VM `state=0` after hitting the manifest's
    `defaultMaxLoggedCalls=1000000`, so startup analysis must look at terminal
    events rather than duration alone;
  - 2026-05-13 full corpus run:
    `Runs/CorpusFullAfterSoundFix/20260513_135812`;
  - 32/32 manifest entries completed with process exit code 0 and no runner
    timeout;
  - no `invalid-opcode`, `memory-oob`, or VM fatal error was observed in the
    batch logs;
  - DeepAbyss 1.1/1.4 on T310/T610 now runs long enough for visual and audio
    inspection after the call/stack, heap, font, date/certificate, persistent
    stream, and sound capability fixes;
  - the corpus runner infrastructure is usable as a repeatable Phase 11
    diagnostic loop.
- Open defects from the latest corpus checkpoints:
  - sidecar/resource-pack lookup is partially fixed for read-only files next to
    the active `.mpn`: `VRally`/`VRally2` now resolve `multipack` and
    `extrapack*` to `VRally_*.mpc`; remaining work is to classify missing
    sidecars that are not present in the corpus and decide whether writable
    host sidecars are required;
  - VRally T310/T610 no longer stalls on stale frames after the SDL renderer
    consumes deferred draw commands after each presented VM frame; focused
    Shift/FIRE probes in `Runs/VRallyProbeAfterConsume/20260513_162618` show
    zero `emitted=0` map updates and both profiles reaching gameplay;
  - VRally sprite positioning improved after treating `vDrawObject` coordinates
    as sprite anchor/center coordinates and allowing validated legacy sprite
    layouts to render instead of falling back to rectangles; transfer mode is
    now captured by draw commands and used for zero-pixel transparency in
    direct raw-tile rendering and sprite rendering. Invalid `vFillRect` calls
    are skipped with reference-compatible `x0 >= x1 || y0 >= y1` semantics,
    `vUpdateSprite` slot replay preserves the captured transfer mode, and
    `vUpdateMap` tilemap draws no longer inherit the global transfer mode.
    `vSetClipWindow` now treats arguments as signed 16-bit coordinates and
    clamps them to the active screen, fixing VRally T310/T610 negative-clip
    road/menu artifacts such as `y0=65509`. Focused probes:
    `Runs/VRallyProbeSpriteZero/20260513_172216`,
    `Runs/VRallyProbeSpriteSlotsMode/20260513_172754`, and
    `Runs/VRallyProbeClipFix/20260513_215229`. Follow-up remains for residual
    VRally visual parity issues in road perspective composition, menu
    transitions, and HUD text/time rendering;
  - persistent-file logging now separates missing read-only sidecars from
    missing first-run persistent files; remaining work is to add an external
    per-run persistent storage backend so save reset does not rely on restoring
    writable `.mpn` images after each corpus run;
  - the current corpus now has an explicit machine-readable classification list
    in `Tools/corpus/classifications.json`; `IcebloxPlus610`, `lunar`, and
    `Huntsman` are fixed there as `incomplete_artifact_set`, not unresolved VM
    failures;
  - short-exit handling still needs explicit runtime/platform work:
    - embedded-certificate interpretation and automatic fixed-date selection are
      still not implemented for titles whose required cert artifacts are
      actually present;
  - missing syscall IDs from the 2026-05-13 corpus batch were all the same
    missing import under different pool indices: `vutoa` (`0x28` in `4in1`,
    `0x32` in `HoneyCave2`, `0x25` in `jbubble`, and `0x23` in
    `Bouncy_demo`); focused probes now run without `missing-syscall` after
    adding `vutoa`;
  - several games terminate through normal VM exit state instead of VM error,
    including very short exits in `prhstrkmn`, `lunar`, and `IcebloxPlus610`,
    plus certificate/resource-pack-looking exits in `Huntsman` and
    `snowboardx`; classify these as unsupported profile, certificate/license,
    missing pack, or valid guest-controlled exit;
  - SDL system-font visual parity remains open for DeepAbyss menu/loading/death
    text despite current readability improvements;
  - many games appear to run noticeably faster in MVM than in the original
    emulator/reference videos; keep this as a separate timing/cadence defect,
    not just a scripted-input mismatch, and compare video pacing against the
    reference captures when prioritizing timer/frame fixes;
  - video review of the same corpus run shows additional renderer defects:
    `HoneyCave` has vertical stripe/layer corruption, `snowboardx` has
    broken black mask/text-like blits, `SynergenixRally` has right-edge
    rectangular artifacts, `CMRally4` shows black text/background rectangles,
    and `SpaceExplorer`/`4in1` lose large background or playfield layers to
    black frames;
  - unsupported-profile and missing-sidecar outcomes are now promoted to
    structured MVM-level events; certificate/license classification still needs
    real certificate interpretation instead of relying on observed guest text.
- Phase 11 close-out gate:
  - done: run final reference-input T310/T610 corpus batches after the current
    Phase 11 infrastructure fixes;
  - done: review logs and videos enough to freeze the residual defect classes
    in `Runs/RefCompare_20260718/analysis.md` and
    `Tools/corpus/classifications.json`;
  - done: fix the recording audio-duration blocker so encoded MP4 video/audio
    stream durations stay aligned for long MIDI/looping audio cases;
  - accepted follow-up: renderer visual parity defects in `HoneyCave`,
    `HoneyCave2`, `FiveStones`, `SpaceExplorer`, DeepAbyss system-font text,
    and rally road/menu rendering are backlog items, not corpus-runner blockers;
  - accepted follow-up: finish SE T230-style system-font integration and metrics
    so SDK text rendering matches real-phone captures rather than the current
    candidate/placeholder faces;
  - accepted follow-up: exact VM tick/input-flow parity is backlog as long as
    the runner produces repeatable logs/videos and no VM fatal error is present;
  - accepted follow-up: add image/certificate scanning that parses embedded
    `M001YYYYMMDD` / `M002YYYYMMDD` tags and suggests or applies a run date that
    falls inside the game's validity window;
  - accepted follow-up: full certificate/date/policy interpretation is backlog;
    Phase 11 keeps structured events and machine-readable classification for
    short exits instead of blocking on complete certificate emulation.
  - accepted follow-up: route guest help/system-message requests through the
    platform layer. In particular, when `VRally2` invokes Help, MVM should emit
    a system event and let the platform show a system `MsgBox`/printable help
    text instead of treating it as ordinary in-game rendering only.
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

The MCU integration goal is that platform drivers stay simple and do not need
to understand Mophun/VMGP internals. Display, input, audio, storage, timing,
logging, and system-message drivers should expose small hardware-oriented
callbacks, while the VM keeps ownership of guest API semantics such as sprites,
maps, fonts, resources, sound requests, help/message-box requests, certificate
events, and persistent-data behavior.

Tasks:

- Separate bundled default config from project-owned integration config.
- Support external `MVM_Cfg.h` / `MVM_Lcfg.c` supplied by the parent project.
- Keep bundled config as a fallback for local smoke tests and examples.
- Add build hooks so the parent project can point the VM build at an external
  config directory.
- Minimize assumptions in `vm.mk` about runner layout and top-level project
  structure.
- Optionally support building MVM as a standalone static library artifact
  (`libmvm.a` / `mvm.lib`) and linking that artifact back into the desktop
  runner or a tiny parent-project smoke test.
- Keep configuration hybrid rather than purely runtime:
  - compile-time config for static limits, feature flags, log level, default
    profile catalog, framebuffer format, and runtime-pool sizing;
  - runtime/init-time config for selected image source, active profile, callback
    user pointers, fixed date, platform logger/event sinks, and driver context.
- Add public `WithConfig` initialization/query APIs where needed, so a parent
  project can pass its own `MVM_Config_t` without relying on the built-in
  `MVM_Config` object.
- Write a minimal integration guide for parent projects.
- Document platform callback interfaces and backend responsibilities.
- Shape the host-facing port layer around simple driver-style APIs:
  - display framebuffer flush/present callbacks, preferably allowing a simple
    "VM renders to a buffer, platform flushes dirty rects" MCU path;
  - optional primitive/draw replay callbacks for desktop/debug backends;
  - input state polling or event injection using Mophun button masks, not
    desktop key names;
  - audio sample queue/playback callbacks, with a valid no-op path for boards
    without audio;
  - time/tick provider;
  - random provider;
  - logging/event sink;
  - storage/image read and optional write callbacks;
  - persistent-data read/write callbacks that do not require rewriting the
    original `.mpn` image;
  - an optional named-file/VFS service for guest sidecars and save files, with
    explicit open/read/write/seek/size/close/delete capabilities or an
    equivalently small bounded contract; MCU parents can adapt FatFS, LittleFS,
    raw flash records, or another filesystem without linking those components
    into MVM;
  - platform system-message callback or event path for guest requests such as
    `vMsgBox` / Help text;
  - static memory/runtime-pool ownership.
- Keep VMGP-specific behavior inside MVM instead of pushing it into drivers:
  - display drivers should not know about VMGP sprites, maps, fonts, palettes,
    or transfer modes;
  - audio drivers should not know about VMGP resource tables;
  - storage drivers should not know game-level save-file policy;
  - platform callbacks should receive normalized requests/events from MVM.
- Keep host filesystem policy outside the portable library:
  - no `FILE`, `fopen`, host path construction, current-directory lookup, or
    desktop sidecar naming heuristics inside `MVM/`;
  - MVM owns guest `vStream*` semantics and translates them into registered
    host file/VFS services;
  - the desktop runner provides the stdio/filesystem adapter and path policy;
  - a bare-metal parent explicitly registers a FatFS/LittleFS/raw-storage
    adapter when named files are supported, or leaves the service absent for a
    deterministic no-filesystem configuration.
- Keep the minimal embedded integration path obvious:
  - add VM sources/include paths;
  - provide one config object;
  - allocate VM storage and runtime pool;
  - initialize from an image source;
  - call bounded VM execution from the host loop/task;
  - flush display/audio/input through the platform driver callbacks.
- Make callback direction explicit and precompiled-library-safe:
  - parent-to-MVM calls initialize, feed input/events, and run bounded steps;
  - MVM-to-parent callbacks request display, audio, storage/VFS, time, logging,
    events, and system messages;
  - host services are supplied through one init descriptor or explicit
    per-instance runtime registration, never by editing library config or
    relying on weak libc/syscall symbols;
  - guest Mophun imports remain internal dispatch entries and are not exposed as
    callbacks that the parent must register one by one.
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
- Verify that the same sources can be compiled into a static library and then
  consumed by a separate test executable using only public headers and external
  config.
- Preserve the Phase 11 behavioral baseline while changing integration shape:
  - keep the current corpus manifests, input sequences, classifications, and
    representative videos/logs as regression reference artifacts;
  - run the focused smoke corpus after every major external-config,
    static-library, public-API, render-port, storage-port, or platform-callback
    refactor, so regressions are found near the change that introduced them;
  - run the full corpus at milestone/freeze points, especially before declaring
    Phase 12 closed;
  - compare high-level VM events, exit states, missing-import/opcode counters,
    frame/audio artifact generation, and known short-exit classifications;
  - treat renderer/timing differences as known backlog only when they match the
    documented baseline and do not introduce new VM fatal errors.

Execution plan:

1. Done: freeze the current Phase 11 baseline:
   - focused gate: `Tools/corpus/phase12-smoke-manifest.json`;
   - baseline paths, acceptance criteria, classifications, known renderer and
     timing defects, and accepted short-exit outcomes:
     `Tools/corpus/PHASE12_BASELINE.md`.
2. Done: split public integration contract from bundled defaults:
   - `MVM_Config_t`, device profiles, and capability flags live in public
     `inc/MVM_Config.h` and `inc/MVM_Device.h`;
   - bundled `Config/` retains default macros, adapters, profiles, storage, and
     the built-in `MVM_Config` object for desktop/local use.
3. Done: add external-config build support:
   - `vm.mk` accepts parent-owned `MVM_CONFIG_DIR` and optional
     `MVM_CONFIG_SOURCE` overrides;
   - the external config include directory has priority, with bundled build
     switches remaining available as fallback;
   - a no-SDL build smoke under `Tools/integration/external-config-smoke/`
     verifies external profile/config selection while the desktop build keeps
     using bundled defaults.
4. Done: add public `WithConfig` APIs:
   - init, memory-query, and profile-query entry points accept a parent-owned
     `MVM_Config_t` without modifying it;
   - built-in-config wrappers preserve local smoke tests and desktop runner
     compatibility.
5. Done: define the MCU-facing driver contract:
   - public `MVM_Drivers_t` keeps display, button input, normalized PCM audio,
     image/persistent storage, time, random, log, event, and system-message
     callbacks hardware-oriented and small;
   - VMGP-specific behavior and guest pointers stay inside MVM;
   - zero initialization, no-audio/no-persistence/no-system-message operation,
     and read-only image storage are valid documented paths in
     `MVM/DRIVER_CONTRACT.md`.
6. Done: make the display path MCU-friendly:
   - `drivers.display_flush` enables a runtime-pool-backed RGB565 framebuffer
     with changed-pixel dirty-rectangle accumulation for bare-metal LCD drivers;
   - desktop and MCU use the same immediate RGB565 framebuffer path; SDL remains
     only the desktop platform implementation of `display_flush`;
   - the former 2,048-command/128-palette deferred storage is reduced to one
     scratch command and palette, cutting Cortex-M4 `MpnVM_t` from 473,344 to
     7,600 bytes before later heap-diagnostics metadata.
7. Done: add a no-SDL reference port template:
   - `Examples/MVM_BareMetalPort/` demonstrates aligned static VM storage,
     runtime pool, flash image reads, fixed date, button polling, RGB565 flush,
     audio no-op, event/log, and system-message hooks;
   - its bare-metal-style loop runs bounded `MVM_RunSteps()` slices and yields
     through a replaceable idle hook.
8. Done: add static-library packaging:
   - `MVM_LIBRARY_SRC` and the root `static-lib` target build `libmvm.a` without
     embedding a bundled or parent-owned config object;
   - `Tools/integration/static-library-smoke/` links the archive into a real-MPN
     parent executable using only public headers and external config sources.
9. Done: document the integration path:
    - write a short checklist for source-included and static-library modes;
    - document compile-time vs runtime/init-time responsibilities;
    - document required, optional, and no-op platform callbacks.
    - `MVM/INTEGRATION.md` is the parent-project entry point and links the
      public driver contract, no-SDL reference port, and integration smokes.
10. Optimize the integration footprint for the first Cortex-M4 bring-up target:
    - use `STM32F407VET6` and its existing parent-owned BSP as the initial
      constrained integration target;
    - initial measured footprint breakdown, feasibility model, and safe
      implementation order are recorded in `MVM/CORTEX_M4_ANALYSIS.md`;
    - confirmed from official `pip-gcc`/`pip-ld` output probes: VMGP header
      offsets `0x04` and `0x08` store the game-authored additional data-heap
      and stack requirements in 32-bit words; replace the current fixed guest
      reservations with these decoded requirements during the implementation;
    - remove the approximately 462 KiB loader-query context from the host stack;
    - make the large VM tables/limits configurable and reduce `MpnVM_t` from its
      current approximately 462 KiB footprint without changing guest behavior;
    - measure the real runtime-pool, framebuffer, VM-storage, host-stack, and
      Flash requirements for at least one small decrypted image;
    - avoid pulling desktop-oriented newlib/file I/O paths into the minimal MCU
      link when their features are disabled;
    - current heap experiments use compatibility-oriented oldest-quarantine
      reuse with a parent-visible soft limit, statistics, and physical fallback;
      the full 34-game automated corpus has no fatal/allocation failures, while
      manual gameplay measured 83,352 bytes for Prehistorik and the automated
      maximum is 87,508 bytes for snowboardx; validate the experimental 96 KiB
      physical heap on the focused heavy-game set before another full corpus;
    - rejected experiment: reducing the contiguous guest heap/address span to
      96 KiB makes Prehistorik access `0x40030` during level load despite zero
      allocation failures; the fixed 256 KiB span currently affects guest stack
      placement/address ABI, not only allocator capacity. Restore 256 KiB until
      logical guest addresses can be decoupled safely from physical backing;
    - keep this footprint work after the integration contract/documentation is
      stable and validate behavior before attempting hardware bring-up.
    - before changing the public contract, evaluate the precompiled-library
      configuration model explicitly: compile-time options are fixed when the
      archive is built, so parent-selectable limits/features must either move
      into a runtime descriptor or be supplied as documented library variants;
    - evaluate replacing the current split legacy/config/driver setup with one
      parent-owned integration descriptor covering platform context, storage,
      profiles, limits, and callbacks, including runtime registration where it
      genuinely simplifies use without adding hidden allocation or mutable
      global state;
    - evaluate a desktop backend built through the same minimal bare-metal-style
      integration path, so desktop and STM32 exercise one public lifecycle and
      driver contract while SDL remains only a platform implementation;
    - complete that evaluation by migrating the desktop runner fully onto the
      same public bare-metal-style lifecycle and integration descriptor used by
      `Examples/MVM_BareMetalPort`; remove the compatibility-only split between
      legacy `MpnPlatform_t`, direct config callbacks, and `MVM_Drivers_t` once
      equivalent behavior is proven;
    - add the missing named-file/VFS service contract and migrate `.mpc`
      sidecar/save-file access out of `MVM/runtime`; provide a desktop stdio
      adapter and a filesystem-free no-op/reference adapter, document how a
      future MCU parent registers FatFS or LittleFS, and keep actual filesystem
      component selection/integration in Phase 14;
    - verify the static/precompiled library path can select all per-instance
      callbacks and storage services at init/runtime without recompiling MVM or
      depending on library-owned global configuration;
    - add the still-missing compact architecture/data-flow overview showing
      parent-to-MVM control calls, MVM-to-parent services, guest import
      ownership, memory ownership, and desktop/MCU adapter placement;
    - finish compile-time and init-time validation for contradictory feature,
      limit, framebuffer, storage/VFS, and callback configurations, returning
      deterministic errors rather than relying on a later null callback or
      linker failure;
    - judge all configuration/API simplifications against the Cortex-M4 RAM,
      Flash, stack, determinism, and non-blocking requirements rather than only
      desktop convenience;
    - after the architecture decisions and MCU footprint work stabilize, run a
      clean consumer-repository rehearsal:
      - create the proposed standalone library repository named `OpenMophun`;
      - decide the safe rename scope before changing terminology: project-owned
        names may become OpenMophun, while historical VMGP/Mophun format and API
        terms may need to remain for technical accuracy and compatibility;
      - integrate that repository back into this project as a fresh git
        submodule without library-local patches;
      - build and run the documented desktop and STM32-oriented integration
        paths from the clean checkout;
      - repeat the focused and full regression gates after reintegration.
    - Phase 12 stops at architecture, cross-compilation, footprint analysis,
      integration cleanup, and clean-consumer rehearsal. Real STM32F407VET6
      linker-script/BSP integration and execution on target hardware belong to
      Phase 14 and are not a Phase 12 closure gate.
11. Consolidate the public per-instance integration contract:
    - replace the transitional split between `MVM_Config_t`, legacy
      `MpnPlatform_t`, direct image callbacks, and `MVM_Drivers_t` with one
      clearly documented parent-owned integration descriptor;
    - define which services are fixed at initialization and whether any service
      may be registered or replaced at runtime;
    - make callback direction explicit: parent-to-MVM lifecycle/input/execution
      calls versus MVM-to-parent display, keyboard/button input polling,
      audio, storage/VFS, time, log, event, and message requests;
    - keep guest Mophun imports internal to MVM rather than requiring the parent
      to register guest syscalls individually;
    - verify that every per-instance service and limit needed by a parent can be
      supplied to a precompiled `libmvm.a` without editing library files,
      rebuilding the archive, or mutating library-owned global configuration.
12. Add the portable named-file/VFS service boundary:
    - define a small bounded host-file contract covering the operations required
      by current guest `vStream*` behavior: open, read, write, seek/tell or
      positioned I/O, size, close, and delete/truncate where required;
    - keep guest handles, access-mode interpretation, certificate behavior,
      sidecar/save classification, and `vStream*` semantics inside MVM;
    - support explicit registration of a parent VFS adapter and a deterministic
      no-filesystem configuration;
    - document how Phase 14 ports can adapt FatFS, LittleFS, raw-flash records,
      or another storage stack without linking any of them into MVM itself.
13. Remove host filesystem and path policy from the portable library:
    - remove `FILE`, `fopen`, `fseek`, `ftell`, `fread`, `fclose`, host path
      construction, current-directory assumptions, and desktop `.mpc` filename
      probing from `MVM/` library sources;
    - move desktop sidecar discovery/naming and stdio access into a desktop-owned
      VFS adapter under `Src/`;
    - provide a filesystem-free reference adapter for the bare-metal example;
    - preserve existing sidecar/certificate/save behavior through focused tests
      before proceeding to the desktop lifecycle conversion.
14. Convert the complete desktop sample to the bare-metal-style lifecycle:
    - make the desktop runner use the same public storage allocation, integration
      descriptor, initialization, bounded execution loop, and host-service
      registration demonstrated by `Examples/MVM_BareMetalPort`;
    - keep SDL video, keyboard/input mapping, audio, stdio/filesystem, input
      scripting, recording, and desktop timing exclusively in desktop-owned
      adapters;
    - remove compatibility-only legacy initialization/callback paths after the
      unified path reproduces the current corpus behavior;
    - keep `Src/main.c` thin and make the desktop application the executable
      reference integration for the future MCU port.
15. Finalize Phase 12 configuration, footprint, and documentation cleanup:
    - move parent-selectable memory limits and feature policy out of hidden
      library-only macros where a precompiled archive must support them;
    - remove or make optional the fixed heap/address diagnostic probes and the
      large allocation-tracker workspace; eliminate obsolete allocator helpers
      and relevant compiler warnings;
    - ensure memory queries report logical guest layout and actual physical
      backing requirements accurately;
    - add compile-time and init-time checks for contradictory limits, features,
      framebuffer, VFS, storage, and callback combinations;
    - add one compact architecture/data-flow overview showing callback
      direction, ownership, guest-import boundaries, and the video/display,
      keyboard/button input, audio, storage/VFS, time, log/event, and
      system-message adapters for desktop and MCU parents;
    - refresh the Cortex-M4 RAM/Flash/context estimates after cleanup, without
      performing the real hardware bring-up reserved for Phase 14.
16. Rehearse the final standalone-library/submodule integration:
    - create the proposed standalone repository named `OpenMophun`;
    - define and apply the safe rename scope: rename project-owned branding while
      retaining historical Mophun/VMGP/API terminology where technically
      required;
    - integrate the new repository back into this project as a clean submodule;
    - build source-included, precompiled-static-library, desktop bare-metal-style,
      and MCU-oriented cross-compile paths without library-local patches.
17. Run the Phase 12 closure regression gates:
    - run the focused smoke corpus after every major step above;
    - run the full T310 and T610 corpus after the unified descriptor/VFS/desktop
      conversion and again after OpenMophun submodule reintegration;
    - repeat manual focused gameplay/visual checks for Prehistorik and the known
      heavy/certificate-sensitive games;
    - compare terminal states, events, missing imports/opcodes, allocation/OOB
      diagnostics, recording/audio/video coverage, and accepted classifications;
    - update classifications only for deliberate behavior changes, then mark
      Phase 12 closed. Real STM32F407VET6 BSP/linker/hardware execution starts in
      Phase 14.

Done when:

- A parent project can integrate the VM as a submodule using external config
  files.
- MVM can be built either as source-included component or as a static library
  linked by a parent project.
- Platform-specific configuration no longer requires patching the library tree.
- A new platform port can start from a small driver-template file rather than
  copying the desktop runner.
- The mandatory host API surface is small enough to document on one integration
  checklist.
- The reference port template builds without SDL, Windows, or desktop runner
  dependencies.
- The MCU-facing driver contract is hardware-oriented and small: framebuffer
  flush/present, button mask, audio queue/no-op, ticks, random, storage,
  persistent data, log/event, and system-message hooks.
- Ordinary MCU integration does not require runtime syscall registration or
  Mophun-aware display/audio/storage drivers. It does require explicit
  registration of the host services the application chooses to support,
  including an optional named-file/VFS adapter; guest syscalls/imports remain
  implemented inside MVM.
- The desktop runner uses the same public bare-metal-style initialization,
  execution, driver, and host-service contract as the reference MCU template;
  SDL and stdio/filesystem code exist only in desktop-owned adapters.
- The portable `MVM/` library has no direct `FILE`/stdio filesystem access or
  host path-resolution policy, and a no-filesystem configuration is valid.
- A precompiled static library accepts per-instance platform/VFS callbacks
  without requiring library recompilation or mutation of global config.
- Phase 11 corpus behavior remains stable after the packaging/config changes:
  no new fatal VM errors, no lost logs/videos, no new missing opcode/syscall
  regressions, and known short exits remain classified consistently.

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

- Use STM32F407VET6, its existing parent-owned BSP, and Arm GNU Toolchain as the
  first target board/toolchain path.
- Build VM without desktop default allocator/logger.
- Provide static storage and platform callbacks.
- Run bounded VM steps from a task or main loop.
- Provide the real linker script and place VM context, guest segments, stack,
  framebuffer, and DMA-visible buffers across SRAM1/SRAM2/CCM as appropriate.
- Measure RAM, stack, Flash, and CPU time on the actual target.

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

## Open Defect Backlog

This section is the single human-readable backlog for known open defects and
accepted follow-ups. Keep machine-readable per-game corpus classifications in
`Tools/corpus/classifications.json`, and keep this section updated when corpus
runs change the defect picture.

### Renderer / Visual Parity

- DeepAbyss 1.4 system-font text still does not match real-phone/reference
  captures closely enough; the SE T230-style system font integration and
  metrics remain open.
- VRally / VRally2 road, menu, transition, HUD text/time, and layer composition
  still have visual parity defects against phone/reference captures.
- HoneyCave and HoneyCave2 have vertical stripe/layer corruption in corpus
  videos.
- FiveStones has visual parity defects that remain accepted Phase 11 follow-up
  work.
- SpaceExplorer and 4in1 can lose large background/playfield layers to black
  frames.
- snowboardx has broken black mask/text-like blits in video review.
- SynergenixRally shows right-edge rectangular artifacts.
- CMRally4 shows black text/background rectangles.

### Timing / Cadence / Input Flow

- Many games appear to run noticeably faster in MVM than in the original
  emulator/reference videos; track this as a timing/cadence defect separate
  from scripted-input mismatch.
- Exact VM tick/input-flow parity remains open; scripted input is repeatable,
  but game progression can diverge from reference captures under the same input
  JSON.
- Phase 12 integration refactors must run focused smoke corpus checks after
  major changes and preserve the Phase 11 behavioral baseline.

### Certificate / Date / Licensing

- Full certificate/date/policy interpretation remains incomplete.
- Add image/certificate scanning that parses embedded `M001YYYYMMDD` /
  `M002YYYYMMDD` tags and suggests or applies a fixed run date inside the
  game's validity window.
- `CMRally4_T610` and `VRally_T610` still classify as
  `certificate_date_enforcement_mismatch` after the accepted-cert stub fix.
- `lunar_T610` still exits through guest-controlled termination after
  certificate acceptance.
- `1849GoldRush_T610` and `bombjack_T610` remain certificate/demo policy gaps.
- Missing `.mpc` sidecars are classified, but full sidecar/certificate semantics
  still need real policy interpretation.
- `vCheckDataCert` has at least two observed ABI/result conventions: pointer-only
  callers such as VRally2 require boolean success `1`, while sized-certificate
  callers such as Prehistorik require status success `0`; replace the current
  argument-shape compatibility rule with verified SDK signatures and real
  certificate parsing.

### Guest Heap / Allocation Compatibility

- Immediate first-fit reuse regresses Prehistorik because legacy guests can
  double-free, free unknown pointers, or retain data after `vDisposePtr`.
- Monotonic allocation preserves compatibility but exhausted a 256 KiB heap in
  Prehistorik despite measured peak-live data below 76 KiB.
- Finish and validate deterministic oldest-quarantine reuse without guest-RAM
  block headers; keep double-free/unknown-free handling non-fatal and expose
  high-water, peak-live, reuse, fallback, and failure statistics.
- VMGP `data_heap_bytes` is a useful soft requirement, not yet a proven hard
  capacity: Alien Scum and Prehistorik required physical fallback above their
  declared values. Establish the safe runtime/configuration policy before MCU
  bring-up and remove temporary diagnostic metadata or move it to configured
  parent-owned workspace.
- A 96 KiB physical/contiguous heap experiment regressed Prehistorik with an
  access near the former 256 KiB boundary while allocator statistics remained
  below the new capacity. Investigate fixed guest address-space/stack-top
  expectations and segmented or sparse backing before claiming the live-heap
  reduction as physical SRAM savings.
- First instruction-level finding for that regression: Prehistorik `pc=0x0F14`
  executes `LDBU r12, [r30 + 0x30]`; the preceding `LDWd` loads `r30/R0` as the
  exact logical base `0x40000` from a structure field at offset `0x84`, producing
  failed address `0x40030`. A successful 256 KiB gameplay trace confirmed that
  the byte read there is zero, while a separate access at `0x411D4` belongs to
  the guest stack below `stack_top=0x4121C`. This supports preserving the logical
  address map with sparse/segmented physical backing rather than reserving the
  full span as ordinary heap SRAM; trace all reads/writes in the high region and
  validate a low heap plus explicit zero/system page and stack segment.
- Implemented the complete segmented guest-memory experiment after rejecting a
  partial PIP-only prototype that caused broad graphical/runtime regressions:
  all core, loader, PIP, import, renderer, debug/public guest accesses now cross
  one checked mapping API; no direct `ctx->mem[address]` or guest-derived
  `ctx->mem + address` remains outside the backend. The physical layout uses a
  96 KiB low heap segment plus backing from logical `0x40000` through the legacy
  stack/guard, while preserving the 256 KiB logical heap ABI and stack address.
- Full segmented-memory regression checkpoint:
  `Runs/Segmented96T310/20260719_195347` (21/21) and
  `Runs/Segmented96T610/20260719_195347` (13/13); zero timeout, process failure,
  memory OOB, invalid opcode, allocation failure, or fatal VM event. An injected
  first-instruction `LDBU 0x007FFFFF` probe terminated deterministically as
  `state=5,error=4` without a host crash.
- Manual acceptance completed: Prehistorik T310 loaded and completed its full
  demo level, returned to the application flow, and terminated normally as
  `state=4,error=0` with a 96 KiB physical heap. Final statistics were 83,352
  bytes high-water, 75,248 bytes peak-live, 172 allocations, 197 frees, 76
  quarantined-block reuses, 70 soft-limit fallbacks, and zero allocation
  failures. Additional manual spot checks of multiple games reported no visual
  or runtime regression. Treat segmented guest memory as accepted for the
  current corpus; retain broader hardware validation as the next gate.

### VM Tasks / Termination Semantics

- `vTerminateVMGP` currently terminates the whole VM, but the reference VRally
  T610 trace terminates one VMGP task and subsequently starts another task.
- Investigate and implement the minimum task model required by `vCreateTask`,
  `vThisTask`, `vKillTask`, `vTaskAlive`, and task-local termination; the VM must
  only enter its final terminal state when the application/task model actually
  finishes.
- Do not special-case VRally by image name or simply ignore termination; preserve
  bounded execution and deterministic task scheduling suitable for bare metal
  and RTOS hosts.

### Platform Events / System UI

- Route guest help/system-message requests through the platform layer. In
  particular, when `VRally2` invokes Help, MVM should emit a system event and
  let the platform show a system `MsgBox`/printable help text.
- Extend structured events where needed for globally important outcomes such as
  unsupported profile/device, license/certificate decisions, missing sidecars,
  and system messages.

### Persistent Data / Storage

- Persistent-file handling should move toward an external per-run persistent
  storage backend so corpus save reset does not rely on restoring writable
  `.mpn` images.
- MCU-facing persistence must use simple read/write callbacks and must not
  require rewriting the original game image.

### Corpus / Reference Artifacts

- Keep `Tools/corpus/classifications.json` as the machine-readable index for
  per-game outcomes such as unsupported device/profile, incomplete artifact set,
  certificate/date gaps, and reference artifact gaps.
- `prhstrkmn_T610` remains classified as unsupported device/profile.
- Some entries still lack reference video artifacts, so visual parity cannot be
  judged for those games until reference captures are available.
