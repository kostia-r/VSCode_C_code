# MVM Component

This directory is intended to be embedded into a host firmware or desktop
runner as a standalone Mophun VM component.

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
changing the external integration contract.

Host-specific runners and backend examples live outside the library tree under
`Examples/` so embedding projects can choose their own platform glue.

The VM can run with built-in desktop defaults, but embedded targets should pass
a `MophunPlatform` to `MVM_bInitWithPlatform()`. The platform owns:

- allocation and free callbacks;
- log output;
- optional tick and random providers.

For stricter firmware builds, define `MVM_ENABLE_DEFAULT_ALLOCATOR=0` or
`MVM_ENABLE_DEFAULT_LOGGER=0` from the parent build and provide the
corresponding callbacks.

`MophunVM` is opaque to host code. Allocate storage using
`MVM_udtGetStorageSize()` and align it to `MVM_udtGetStorageAlign()`, then pass
it through `MVM_pudtGetVmFromStorage()` before initialization. Desktop runners may
use `malloc`; firmware can place the storage in a static or task-owned buffer.
