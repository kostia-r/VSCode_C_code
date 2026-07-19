# MVM Bare-Metal Port Template

This no-SDL template is a starting point for MCU, RTOS, and simulator parent
projects. Copy the directory into the parent project, replace the stubs in
`MVM_Port.c`, and point `MVM_CONFIG_DIR` at it before including `MVM/vm.mk`.

The template demonstrates:

- aligned static opaque VM storage;
- a parent-owned static runtime pool;
- a flash/memory-range image reader;
- one external device profile and `MVM_Config`;
- RGB565 framebuffer flush with dirty rectangles;
- Mophun button-mask polling;
- valid no-op audio, log, event, and system-message hooks;
- deterministic fixed date setup;
- a bounded `MVM_RunSteps()` loop with an idle/yield point.

`main.c` contains a 64-byte placeholder image so the template can be linked
without redistributing a game. Replace `MVM_lImageBytes` with a decrypted MPN
stored in flash and remove `MVM_PORT_BUILD_CHECK` for a real port.

From the repository root, the host-side dependency check is:

```bat
C:\mingw64\bin\mingw32-make.exe -f Examples\MVM_BareMetalPort\Makefile run
```

The command builds and starts the template without SDL, Win32, filesystem, or
desktop runner sources. It validates linkage and static-storage assumptions;
it also builds the complete non-build-check execution loop but does not run it
against the placeholder image.
