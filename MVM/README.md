# MVM Component

This directory is intended to be embedded into a host runner or firmware image
as a standalone Mophun VM component.

Development roadmap: see `ROADMAP.md`.

Public integration headers live in `inc/`. Internal headers and sources are
grouped by subsystem:

- `core/` - VM state, guest memory, stack, and execution control.
- `debug/` - trace hooks and VMGP inspection helpers.
- `loader/` - VMGP image, pool, and resource loading.
- `pip/` - PIP2 decode and opcode execution.
- `runtime/` - Mophun syscalls, streams, caps, decompression, timing.

Include `vm.mk` from the parent build and add `$(MVM_SRC)` plus
`$(MVM_INC)` to the target:

```make
include Components/MVM/vm.mk

C_SOURCES += $(MVM_SRC)
C_INCLUDES += $(addprefix -I,$(MVM_INC))
```

The current implementation is being split along these boundaries without
growing the host-facing integration surface.

Host-specific runners and backend examples live outside the library tree under
`Examples/` so embedding projects can choose their own platform glue.

Integration-time glue now lives under `Config/` in AUTOSAR-like form:

- `Config/MVM_Cfg.h` - build-time pool sizes, device-profile parameters, and
  thin `static inline` callback adapters.
- `Config/MVM_BuildCfg.h` - internal build switches used by the VM sources.
- `Config/MVM_Lcfg.c` - one global integration object with runtime pool,
  platform callbacks, device profile, and import/syscall bindings.

The design intent is that every platform integration point lives in these
config files:

- host callbacks such as logging, tick source, and random source;
- device-profile values returned through `vGetCaps`;
- device-profile catalog entries and the selected profile used for one VM run;
- platform-owned VM imports such as graphics, input, UI, audio, and system
  control;
- thin adapters that bridge foreign platform callback signatures to the stable
  `MpnPlatform_t` callback shape expected by the VM.

The default integration path is now:

- size the runtime pool with `MVM_CFG_RUNTIME_POOL_SIZE`;
- optionally inspect one image with `MVM_QueryMemReqs()`;
- initialize the VM with `MVM_Init()`.
- optionally validate or enumerate built-in device profiles through the public
  profile query helpers before calling `MVM_Init()`.
- replace the default callback bindings or import stubs in `Config/MVM_Lcfg.c`
  when porting to a real platform backend.

The platform owns:

- log output;
- optional tick and random providers.

`Config/MVM_Cfg.h` already includes reusable callback adapters for common
signature mismatches, for example:

- host tick APIs that return raw ticks instead of milliseconds;
- host tick/random APIs that do not accept the VM `user` pointer;
- host loggers that return `void` instead of an integer status.

For stricter firmware builds, define `MVM_ENABLE_DEFAULT_LOGGER=0` from the
parent build. The VM library no longer allocates dynamic memory internally.
Instead, it carves guest RAM, pool metadata, and resource metadata from the
runtime pool owned by the built-in integration config. Host code must still supply opaque VM
storage.

`MpnVM_t` is opaque to host code. Allocate storage using
`MVM_GetStorageSize()` and align it to `MVM_GetStorageAlign()`, then pass
it through `MVM_GetVmFromStorage()` before initialization. Host code may
use `malloc`, a static buffer, or task-owned storage depending on the target.
