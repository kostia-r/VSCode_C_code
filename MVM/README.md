# MVM Component

This directory is intended to be embedded into a host runner or firmware image
as a standalone Mophun VM component.

Development roadmap: see `ROADMAP.md`.

Public host-facing headers are:

- `inc/MVM.h` - primary VM API.
- `inc/MVM_Types.h` - shared public types.

Internal headers and sources are grouped by subsystem:

- `core/` - VM state, guest memory, stack, and execution control.
- `debug/` - trace hooks and VMGP inspection helpers.
- `loader/` - VMGP image, pool, and resource loading.
- `pip/` - PIP2 decode and opcode execution.
- `runtime/` - Mophun syscalls, streams, caps, decompression, timing.

## Terms

Short glossary for the main Mophun VM terms used in this component:

- `VMGP` - the Mophun game image format. A `.mpn` game ultimately contains one
  VMGP image with code, data, resources, and the import list used by the game.
- `PIP` / `PIP2` - the guest instruction set executed by the VM. The `pip/`
  subsystem decodes these opcodes and updates the guest registers, stack, and
  memory state.
- `import` - one guest-visible API name such as `vStreamRead` or
  `vFlipScreen`. From the guest point of view this is a callable SDK function;
  from the VM point of view it is a name looked up in the import binding layer.
- `pool` / `constant pool` - the VMGP table that stores import names, constant
  values, and section-relative references used by the guest code.
- `guest RAM` - the memory owned by the VM for the running game: `.data`,
  `.bss`, heap, stack, and other guest-visible runtime state. Guest pointers
  are offsets into this memory, not host pointers.
- `resource` - one asset entry stored in the VMGP resource section, for example
  compressed graphics, maps, or other game data.
- `image source` / `provider` - the host-backed object that lets the VM read
  parts of the game image on demand. This may be a RAM buffer, file, flash
  window, or another storage backend.
- `device profile` - the selected target-device description returned to the
  guest through capability queries such as `vGetCaps`.
- `platform backend` - the host-side implementation for graphics, input,
  timing, random, audio, and other environment-facing behavior that the core VM
  itself does not own.
- `runtime pool` - the host-provided memory arena used by the VM integration
  layer to hold guest RAM plus decoded metadata such as pool entries and
  resource tables.

## Data Flow

```text
game.mpn / image source
          |
          v
     VMGP loader
          |
          v
   guest RAM + metadata
          |
          v
      PIP executor
          |
          +--> VM-owned imports
          |    (memory, strings, streams, decompress)
          |
          '--> platform-facing imports
               (graphics, input, audio, UI, timing, system)
                       |
                       v
                 platform backend
```

## How It Works

At a high level the component works like this:

1. Host code allocates opaque VM storage and supplies platform/config glue from
   `Config/`.
2. The loader reads the VMGP header, pool, string table, and resource metadata
   from the selected image source.
3. The VM initializes guest memory and enters the bounded execution loop.
4. The `pip/` subsystem decodes one PIP instruction at a time.
5. When guest code calls an SDK function, the VM resolves the import name from
   the pool and dispatches it through the runtime import layer.
6. VM-owned imports are handled inside the library, while platform-facing
   imports eventually reach the configured platform backend.

Include `vm.mk` from the parent build and add `$(MVM_SRC)` plus
`$(MVM_INC)` to the target:

```make
include Components/MVM/vm.mk

C_SOURCES += $(MVM_SRC)
C_INCLUDES += $(addprefix -I,$(MVM_INC))
```

Host-specific runners and backend examples live outside the library tree under
`Examples/` so embedding projects can choose their own platform glue.

Current Windows/SDL runner notes:
- primary logging stays in the console;
- the SDL window is used only for graphics/input;
- desktop keys follow the official Mophun SDK emulator mapping:
  - `Up/Down/Left/Right` -> guest direction keys;
  - `Shift` -> `KEY_FIRE`;
  - `Backspace` -> `KEY_SELECT`;
  - `Enter` -> `KEY_FIRE2`.

Integration-time glue lives under `Config/`:

- `Config/MVM_Cfg.h` - build-time pool sizes, device-profile parameters, and
  thin `static inline` callback adapters.
- `Config/MVM_BuildCfg.h` - internal build switches used by the VM sources.
- `Config/MVM_Lcfg.c` - one global integration object with runtime pool,
  platform callbacks, device profile, and image-backend bindings.

Every platform integration point is expected to live in these config files:

- host callbacks such as logging, tick source, and random source;
- device-profile values returned through `vGetCaps`;
- device-profile catalog entries and the selected profile used for one VM run;
- platform callback wiring used by platform-facing VM imports;
- thin adapters that bridge foreign platform callback signatures to the stable
  `MpnPlatform_t` callback shape expected by the VM.

The default integration path is:

- size the runtime pool with `MVM_CFG_RUNTIME_POOL_SIZE`;
- optionally inspect one image with `MVM_QueryMemReqs()` or
  `MVM_QueryMemReqsFromSource()`;
- initialize the VM with `MVM_Init()` or `MVM_InitFromSource()`.
- optionally validate or enumerate built-in device profiles through the public
  profile query helpers before calling `MVM_Init()`.
- replace the default callback bindings in `Config/MVM_Lcfg.c` when connecting
  the VM to a real platform backend.

The VM can read code, resources, pool entries, and string-table data directly
from a host-backed image source such as a file, flash window, or external
storage driver. Image-backend callbacks are configured in `Config/`, while host
code selects only the concrete image instance at runtime. Full-memory buffer
mode is still supported through the simpler `MVM_Init()` and
`MVM_QueryMemReqs()` wrappers.

The platform owns:

- log output;
- optional structured VM event consumption;
- optional tick and random providers.

`Config/MVM_Cfg.h` already includes reusable callback adapters for common
signature mismatches, for example:

- host tick APIs that return raw ticks instead of milliseconds;
- host tick/random APIs that do not accept the VM `user` pointer;
- host loggers that return `void` instead of an integer status.

Logger and event plumbing is available through `MpnPlatform_t`:

- `log(user, level, module, event, message)` receives structured metadata for
  each emitted message;
- `event(user, event_id, arg0, arg1)` receives lightweight VM events without
  having to parse text logs;
- VM logging sites are routed through level-gated macros, and messages above
  `MVM_MAX_LOG_LEVEL` compile out completely.

The first structured events currently cover:

- VM paused, resumed, waiting, exited, and fatal-error transitions;
- import dispatch start and missing-syscall fallback;
- invalid opcode and guest memory out-of-bounds faults;
- resource open and resource read activity;
- frame-ready notifications from `vFlipScreen`;
- sound requests from `vPlayResource`.

The built-in platform wrappers in `Config/MVM_Lcfg.c` also emit trace/debug
logs for callback traffic such as `vGetCaps`, `vGetTickCount`, random services,
generic platform stubs, frame presentation, and sound requests. This makes it
possible to trace VM-to-platform interaction through the same logger path
instead of mixing in ad-hoc prints.

Import/platform modeling lives in the internal runtime import layer. The repo
keeps one static handler per known import together with SDK-based contract
comments and implementation status directly in
`runtime/src/MVM_Imports.c`. The matching internal header lives in
`runtime/inc/MVM_Imports.h`.

The current contract is:

- imports dispatched through `MVM_HandleRuntimeImportCall()` are guest requests
  identified by VMGP import name, not direct host function pointers;
- VM-owned imports stay inside the library (`streams`, `decompress`, `memory`,
  `strings`);
- platform-facing imports are implemented in the runtime import layer and call
  into the configured platform backend where real host interaction is needed;
- guest pointers are VM-memory offsets, not host pointers, and must not be
  retained by host callbacks after the call returns;
- callbacks marked non-blocking should not wait on slow host work;
- asynchronous requests such as `vPlayResource` should enqueue or signal work
  instead of blocking the caller.

The default integration currently keeps host replacement at compile/integration
time through `Config/MVM_Lcfg.c` rather than through a public runtime
registration API. Missing imports are still reported deterministically through
`MVM_EVENT_MISSING_SYSCALL` and warning logs.

For stricter firmware builds, define `MVM_ENABLE_DEFAULT_LOGGER=0` from the
parent build. The VM library no longer allocates dynamic memory internally.
Instead, it carves guest RAM, pool metadata, and resource metadata from the
runtime pool owned by the built-in integration config. Host code must still
supply opaque VM storage.

Resource payloads are no longer mirrored into guest RAM by default. Runtime
stream reads and LZ decompression now fetch resource data through the configured
image backend, which keeps RAM usage lower on embedded targets.

In other words, the host does not call VM imports directly. The host provides
storage, one image source, and one platform backend. The VM loads VMGP data,
executes PIP instructions, resolves imports by name, and then either handles
them internally or forwards the request to the configured backend path.

`MpnVM_t` is opaque to host code. Allocate storage using
`MVM_GetStorageSize()` and align it to `MVM_GetStorageAlign()`, then pass
it through `MVM_GetVmFromStorage()` before initialization. Host code may
use `malloc`, a static buffer, or task-owned storage depending on the target.
