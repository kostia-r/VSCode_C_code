# Mophun VM Component

This directory is intended to be embedded into a host firmware or desktop
runner as a standalone VM component.

Public integration headers live in `inc/`. Internal headers and sources are
grouped by subsystem:

- `core/` - VM state, guest memory, stack, and execution control.
- `debug/` - trace hooks and VMGP inspection helpers.
- `loader/` - VMGP image, pool, and resource loading.
- `pip/` - PIP2 decode and opcode execution.
- `runtime/` - Mophun syscalls, streams, caps, decompression, timing.

Include `vm.mk` from the parent build and add `$(MOPHUN_VM_SRC)` plus
`$(MOPHUN_VM_INC)` to the target:

```make
include Components/VM/vm.mk

C_SOURCES += $(MOPHUN_VM_SRC)
C_INCLUDES += $(addprefix -I,$(MOPHUN_VM_INC))
```

The current implementation is being split along these boundaries without
changing the external integration contract.

Host-specific runners and backend examples live outside the library tree under
`Examples/` so embedding projects can choose their own platform glue.

The VM can run with built-in desktop defaults, but embedded targets should pass
a `MophunPlatform` to `mophun_vm_init_with_platform()`. The platform owns:

- allocation and free callbacks;
- log output;
- optional tick and random providers.

For stricter firmware builds, define `MOPHUN_VM_ENABLE_DEFAULT_ALLOCATOR=0` or
`MOPHUN_VM_ENABLE_DEFAULT_LOGGER=0` from the parent build and provide the
corresponding callbacks.

`MophunVM` is opaque to host code. Allocate storage using
`mophun_vm_storage_size()` and align it to `mophun_vm_storage_align()`, then pass
it through `mophun_vm_from_storage()` before initialization. Desktop runners may
use `malloc`; firmware can place the storage in a static or task-owned buffer.
