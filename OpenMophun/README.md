# OpenMophun

An open-source emulator/runtime for classic Mophun games from Sony Ericsson
phones, targeting embedded systems.

> **Disclaimer**
>
> This project is an independent open-source implementation and is **not
> affiliated with, endorsed by, or sponsored by Sony, Sony Ericsson, Mophun, or
> any of their respective owners.**
>
> All trademarks are the property of their respective owners.

## Project Goals

- Preserve classic Mophun games
- Run games on modern embedded hardware
- Provide a clean-room implementation of the Mophun runtime

OpenMophun is a portable C library that executes decrypted VMGP `.mpn` images.
It includes the loader, PIP interpreter, guest imports, software renderer,
streams, and deterministic execution control.

## Public API

Application code includes only:

```c
#include "MVM.h"
```

`MVM.h` contains the complete function API and includes `MVM_Types.h`, which
defines all public types. Everything under `src/` is private.

One `MVM_Instance_t *` represents one isolated VM. Separate instances use
separate storage, runtime pools, profiles, and callback contexts. The same
instance must not be entered concurrently.

## Complete Integration Example

This example assumes that the `Platform_*` functions have signatures matching
the corresponding OpenMophun callbacks. Treat the application integration file
as the composition root: assign every platform callback there explicitly.
Platform backends should implement individual operations, not mutate
`MVM_InitConfig_t` or hide callback registration in a helper.

```c
#include "MVM.h"

int main(void)
{
  PlatformContext_t platform = {0};
  MVM_FileApi_t files = {0};
  MVM_InitConfig_t config = {0};
  MVM_MemReqs_t requirements = {0};
  MVM_Instance_t *vm = NULL;
  void *instance_storage;
  void *runtime_pool;
  size_t instance_size;
  uint32_t executed;
  int result;

  Platform_Init(&platform);

  /* Register positioned file operations for images, sidecars, and saves. */
  files.context = &platform;
  files.open = Platform_FileOpen;
  files.read = Platform_FileRead;
  files.write = Platform_FileWrite;
  files.resize = Platform_FileResize;
  files.close = Platform_FileClose;
  files.remove = Platform_FileRemove;

  /*
   * Describe one independent VM instance and register every platform
   * operation explicitly in this integration file.
   */
  config.image_path = Platform_GetGamePath(&platform);
  config.file_api = &files;
  config.profile = Platform_GetDeviceProfile(&platform);
  config.watchdog_limit = 0U;
  config.log_level = MVM_LOG_LEVEL_WARNING;

  config.services.context = &platform;
  config.services.display_flush = Platform_DisplayFlush;
  config.services.input_get_buttons = Platform_InputGetButtons;
  config.services.audio_play = Platform_AudioPlay;
  config.services.audio_stop = Platform_AudioStop;
  config.services.get_ticks_ms = Platform_GetTicksMs;
  config.services.get_random = Platform_GetRandom;
  config.services.log = Platform_Log;
  config.services.event = Platform_HandleEvent;
  config.services.system_message = Platform_ShowMessage;

  /* Query image-dependent memory before constructing the VM. */
  if (MVM_QueryMemory(&config, &requirements) != MVM_OK)
  {
    Platform_Deinit(&platform);
    return 1;
  }

  /* Allocate the opaque instance and the separately reported runtime pool. */
  instance_size = MVM_GetInstanceStorageSize();
  instance_storage = Platform_AllocateAligned(instance_size,
                                              MVM_GetInstanceStorageAlign());
  runtime_pool = Platform_Allocate(requirements.runtime_pool_bytes);

  if (!instance_storage || !runtime_pool)
  {
    Platform_Free(runtime_pool);
    Platform_FreeAligned(instance_storage);
    Platform_Deinit(&platform);
    return 1;
  }

  /* Complete configuration and initialize the instance. */
  config.runtime_pool = runtime_pool;
  config.runtime_pool_size = requirements.runtime_pool_bytes;

  if (MVM_Init(instance_storage, instance_size, &config, &vm) != MVM_OK)
  {
    Platform_Free(runtime_pool);
    Platform_FreeAligned(instance_storage);
    Platform_Deinit(&platform);
    return 1;
  }

  /*
   * Run one bounded guest frame at a time. The platform controls cadence;
   * display_flush only delivers pixels and does not delay the VM.
   */
  while (MVM_GetState(vm) == MVM_STATE_READY ||
         MVM_GetState(vm) == MVM_STATE_RUNNING)
  {
    executed = 0U;

    Platform_ServiceDevices(&platform);

    if (MVM_RunFrame(vm, 50000U, &executed) != MVM_OK || executed == 0U)
    {
      break;
    }

    Platform_PresentFrame(&platform);
    Platform_WaitForNextFrame(&platform);
  }

  /* Read the final state before invalidating the instance. */
  result = MVM_GetState(vm) == MVM_STATE_ERROR ? 1 : 0;
  MVM_Deinit(vm);
  Platform_Free(runtime_pool);
  Platform_FreeAligned(instance_storage);
  Platform_Deinit(&platform);
  return result;
}
```

Optional services that the platform does not implement must remain visibly
unregistered in the same block:

```c
config.services.get_random = NULL;
config.services.event = NULL;
config.services.system_message = NULL;
```

### Execution and frame pacing

`MVM_RunFrame()` executes until the guest calls `vFlipScreen`, the VM stops, or
the supplied instruction limit is reached. Use it in an interactive host loop
so input, display presentation, audio, and frame pacing happen once per guest
frame. The instruction limit is a safety bound, not a timing mechanism.

`MVM_RunFrame()` does not sleep. The platform must impose the desired cadence,
for example with display synchronization, a timer, or an RTOS delay. Running
unbounded `MVM_RunStep()` or `MVM_RunSteps()` loops without platform pacing
makes games run faster than intended.

The other bounded execution APIs remain available for specialized hosts:

| API | Stops after |
| --- | --- |
| `MVM_RunStep` | One guest instruction |
| `MVM_RunSteps` | An instruction count |
| `MVM_RunFrame` | A completed guest frame or instruction limit |
| `MVM_RunForTime` | A host-time budget |

### Logging

Logging is disabled by default in a zero-initialized configuration. Set
`config.log_level` from `MVM_LOG_LEVEL_ERROR` through
`MVM_LOG_LEVEL_TRACE` to select the per-instance runtime threshold:

```c
int Platform_Log(void *context, const char *message);
```

OpenMophun assembles the complete line before calling the platform logger,
including the timestamp relative to `MVM_Init()`, severity, module, message,
and trailing newline:

```text
[    0.003] [I][MVM] VM initialized
```

The callback only writes the supplied string to the platform output. It may
send it to stdout, UART, SWO, RTT, or a logging queue. `context` is the same
`services.context` pointer registered by the application and may identify the
specific UART, logger, or platform instance. A platform that uses global output
may ignore it.

Firmware builds may define `MVM_COMPILED_LOG_LEVEL` from `0` (no compiled
logging) through `5` (trace). Runtime configuration cannot enable levels
removed by this compile-time ceiling.

Required `MVM_FileApi_t` operations are `open`, `read`, and `close`. `write`,
`resize`, and `remove` may be `NULL` for read-only use. The same API handles the
selected image and named sidecar/save files.

Host services are optional unless required by the application:

| Callback | Purpose |
| --- | --- |
| `display_flush` | Receives an RGB565 framebuffer and dirty rectangle |
| `input_get_buttons` | Returns the current `MVM_BUTTON_*` mask |
| `audio_play` / `audio_stop` | Handles borrowed beep, MIDI, or AMR data |
| `get_ticks_ms` / `get_random` | Supplies time and random values |
| `log` / `event` | Receives diagnostics and structured events |
| `system_message` | Presents normalized guest messages |

Callbacks run in the caller's context and should not block. Callback buffers
are borrowed and must not be retained; asynchronous work must copy or enqueue
data before returning.

The application owns instance storage, the runtime pool, and callback contexts
until `MVM_Deinit()`. OpenMophun performs no hidden allocation. Guest addresses
are offsets in guest memory and are never host pointers.

## Build

To compile sources directly into a make-based project:

```make
include third_party/OpenMophun/OpenMophun.mk

C_SOURCES += $(OPENMOPHUN_SRC)
C_INCLUDES += $(addprefix -I,$(OPENMOPHUN_INC))
```

The same source list can be archived into a static library. A precompiled
library requires only `inc/` in the consumer's include path. Configuration and
callbacks are supplied independently for every instance at runtime.

## Legal

This project does **not** include:

- proprietary Sony Ericsson firmware
- proprietary Mophun runtime
- copyrighted game files

Users are responsible for obtaining game files legally.

## License

This project is licensed under the MIT License.
