# MVM Integration Guide

MVM can be consumed either by compiling its sources into a parent target or by
linking a static library. In both modes the parent owns configuration, platform
drivers, opaque VM storage, the runtime pool, and the concrete decrypted MPN
image source. No library-owned file needs to be modified.

## Integration Checklist

1. Add MVM as a submodule or copy it under the parent project.
2. Provide a config directory containing `MVM_Cfg.h` and, by convention,
   `MVM_Lcfg.c`.
3. Define one or more `MpnDevProfile_t` entries and an `MVM_Config_t` object.
4. Provide an image range reader for a flash-, file-, or storage-backed image,
   or use the memory-image API.
5. Implement the required drivers and select supported optional callbacks.
6. Allocate aligned opaque VM storage using `MVM_GetStorageSize()` and
   `MVM_GetStorageAlign()`.
7. Allocate a parent-owned runtime pool and expose it through `MVM_Config_t`.
8. Query the image requirements before initialization and reject an undersized
   runtime pool.
9. Initialize with an explicit `WithConfig` API.
10. Execute bounded slices with `MVM_RunSteps()` or `MVM_RunForTime()` and yield
    to the parent scheduler between slices.

`Examples/MVM_BareMetalPort/` is the complete no-SDL template for these steps.

## Source-Included Build

Set the external config directory before including `vm.mk`:

```make
MVM_CONFIG_DIR := $(PROJECT_ROOT)/Config/MVM
include third_party/MVM/MVM/vm.mk

C_SOURCES += $(MVM_SRC)
C_INCLUDES += $(addprefix -I,$(MVM_INC))
```

`MVM_SRC` contains the VM implementation and the selected config source. The
default config source is `$(MVM_CONFIG_DIR)/MVM_Lcfg.c`; set
`MVM_CONFIG_SOURCE` before including `vm.mk` if the parent uses another path.
If `MVM_CONFIG_DIR` is omitted, the bundled `MVM/Config` integration is used.

## Static-Library Build

Compile and archive `MVM_LIBRARY_SRC`, not `MVM_SRC`:

```make
MVM_CONFIG_DIR := $(PROJECT_ROOT)/Config/MVM
include third_party/MVM/MVM/vm.mk

LIB_OBJECTS := $(MVM_LIBRARY_SRC:.c=.o)
```

Compile the library sources with `MVM_INC`, then archive the objects. The
archive intentionally contains no `MVM_Config` object. Compile the parent-owned
`MVM_Lcfg.c` into the final executable and link it with the archive.

The final parent target needs only these include directories:

```make
PARENT_INCLUDES := $(MVM_CONFIG_DIR) $(MVM_PUBLIC_INC)
```

The working end-to-end example is
`Tools/integration/static-library-smoke/`. The repository root `static-lib`
target builds the bundled `libmvm.a` artifact.

## Compile-Time And Init-Time Responsibilities

| Compile/integration time | Initialization/run time |
| --- | --- |
| Select `MVM_CONFIG_DIR` and config source. | Select the concrete decrypted image source. |
| Compile `MVM_LIBRARY_SRC` or `MVM_SRC`. | Select an active device-profile name. |
| Set feature and logging macros, such as `MVM_ENABLE_DEFAULT_LOGGER=0`. | Supply the config instance to a `WithConfig` API. |
| Define static limits and allocate fixed backing arrays. | Query and validate image-specific memory requirements. |
| Include or exclude parent driver implementations. | Supply driver context and consume events. |

`MVM_Config_t` is parent-owned and is not modified by the explicit APIs. A
parent may keep several config objects and choose one for each VM instance.
The non-`WithConfig` wrappers use the linked global `MVM_Config` and are kept
for bundled/desktop compatibility.

## Config Object

At minimum, an external integration supplies:

- a non-empty device-profile catalog and active profile;
- a suitably aligned parent-owned runtime pool;
- an image reader, either in `MVM_Config_t.image_read` or
  `MVM_Config_t.drivers.image_read`, when using source-backed initialization.

Zero-initialize fields that are not used. `watchdog_limit == 0` selects the
library default behavior. Legacy `MpnPlatform_t` fields remain available for
the desktop backend; new MCU integrations should prefer `MVM_Drivers_t`.

## Driver Requirements And Fallbacks

| Function | Status |
| --- | --- |
| Image range read | Required for source-backed mode; not needed for a complete memory image. |
| Display flush | Optional; enables the VM-owned RGB565 framebuffer path when present. |
| Button polling | Optional; use `MVM_SetButtonState()` when absent. |
| PCM queue and stop | Optional; both may be `NULL` for a no-audio target. |
| Monotonic milliseconds | Optional; deterministic/profile timing fallback is used when absent. |
| Random provider | Optional; deterministic internal PRNG is used when absent. |
| Log and structured event sinks | Optional; events may be discarded and the default logger may be compiled out. |
| Image write | Optional; `NULL` makes the original image read-only. |
| Persistent record read/write | Optional; both `NULL` disable external persistence. |
| System message | Optional; the VM emits diagnostics and returns its default acknowledgement. |

Callbacks run synchronously in the VM caller's context and should not block.
Pointers supplied to callbacks are borrowed only for that call and must not be
retained. Full callback shapes and precedence rules are in
`DRIVER_CONTRACT.md`.

## Storage And Initialization

Opaque VM storage and the runtime pool are separate parent-owned allocations.
Do not use `sizeof(MpnVM_t)` in parent code.

```c
static _Alignas(64) uint8_t vm_storage[VM_STORAGE_BYTES];
static uint8_t runtime_pool[RUNTIME_POOL_BYTES];

MpnVM_t *vm = MVM_GetVmFromStorage(vm_storage, sizeof(vm_storage));
```

Before initialization, verify that the selected alignment is at least
`MVM_GetStorageAlign()`, the storage array is at least `MVM_GetStorageSize()`,
and `requirements.runtime_pool_bytes` fits the configured pool:

```c
MVM_MemReqs_t requirements;

result = MVM_QueryMemReqsFromSourceWithConfig(&image, &config, &requirements);
if (result != MVM_OK ||
    requirements.runtime_pool_bytes > config.runtime_pool_size)
{
  /* Reject the image or select a larger pool. */
}

result = MVM_InitFromSourceWithConfig(vm, &image, "BOARD_PROFILE", &config);
```

For a complete image already resident in memory, use
`MVM_QueryMemReqsWithConfig()` and `MVM_InitWithConfig()` instead.

The runtime pool contains guest RAM and decoded metadata. When
`drivers.display_flush` is present, the memory query also includes a
profile-sized RGB565 framebuffer. Resource payloads remain source-backed until
an operation needs them; writable resource overlays consume additional pool
space.

## Bounded Host Loop

```c
for (;;)
{
  uint32_t executed_steps = 0U;
  MVM_RetCode_t result = MVM_RunSteps(vm, 1000U, &executed_steps);
  MVM_State_t state = MVM_GetState(vm);

  if (result != MVM_OK || state == MVM_STATE_EXITED || state == MVM_STATE_ERROR)
  {
    break;
  }

  Platform_IdleOrYield();
}

MVM_Free(vm);
```

Use a deterministic fixed date through `MVM_SetFixedDateTime()` when testing
older certificate-sensitive images. `MVM_Free()` releases VM-owned state but
does not free parent-owned storage.

## Validation

From the repository root, the integration checks are:

```bat
C:\mingw64\bin\mingw32-make.exe -f Tools\integration\external-config-smoke\Makefile run
C:\mingw64\bin\mingw32-make.exe -f Tools\integration\static-library-smoke\Makefile run
C:\mingw64\bin\mingw32-make.exe -f Examples\MVM_BareMetalPort\Makefile run
```

The first two use a real decrypted local image. The bare-metal check links the
complete execution loop without SDL, Win32, or runner sources and runs a safe
placeholder-image build check.
