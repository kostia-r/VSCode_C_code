# MVM Platform Driver Contract

`MVM_Drivers_t` is the small hardware-facing boundary for MCU and parent-project
ports. Drivers deal in framebuffer pixels, button masks, PCM samples, byte
ranges, time, diagnostics, and normalized system text. They do not interpret
VMGP sprites, maps, fonts, palettes, resource tables, sound resources, guest
pointers, or game save policy.

Zero-initializing `MVM_Drivers_t` is valid. Callbacks execute synchronously in
the VM caller's context and must be non-blocking. A driver must not retain any
pointer received through a callback after that callback returns.

## Callback Rules

| Driver callback | Requirement and fallback |
| --- | --- |
| `image_read` | Required for source-backed initialization only when `MVM_Config_t.image_read` is `NULL`. |
| `image_write` | Optional. `NULL` makes the selected image read-only. |
| `get_ticks_ms` | Optional. The VM uses its deterministic/profile timing fallback. |
| `get_random` | Optional. The VM uses its deterministic internal PRNG. |
| `input_get_buttons` | Optional. The VM retains the state supplied through `MVM_SetButtonState()`, initially zero. |
| `log` | Optional. The legacy platform logger or compiled default logger is used. |
| `event` | Optional. Structured events are discarded when no legacy or driver sink exists. |
| `system_message` | Optional. The VM logs/emits the event and returns the default acknowledgement result. |
| `audio_queue_pcm`, `audio_stop` | Optional. `NULL` is the supported no-audio configuration. |
| `persistent_read`, `persistent_write` | Optional. Both `NULL` disable external persistent records; write-only absence is read-only mode. |
| `display_flush` | Optional. `NULL` keeps the primitive replay path; a callback enables the VM-owned RGB565 framebuffer. |

Explicit fields in `MVM_Config_t` and the existing `MpnPlatform_t` callbacks
take precedence where an older integration supplies them. This preserves the
desktop runner while allowing new ports to use one `MVM_Drivers_t` table.

## Data Shapes

- Input uses public `MVM_BUTTON_*_MASK` values, independent of physical key or
  GPIO numbering.
- Display receives a VM-owned framebuffer view and dirty rectangle. A zero-size
  dirty rectangle means the full frame.
- Audio receives interleaved signed 16-bit PCM with explicit rate and channel
  count. Resource decoding remains inside MVM/host adaptation, not the hardware
  driver.
- System messages receive bounded, null-terminated text copies. They never
  receive guest addresses or pointers into guest RAM.
- Image and persistent-storage callbacks return `0` on success and non-zero on
  failure. Image offsets are relative to the selected `MpnImageSource_t`.

Runtime-pool and opaque VM storage remain parent-owned and are configured
separately through `MVM_Config_t` and the public storage-size/alignment APIs.
When `display_flush` is present, memory queries include one RGB565 framebuffer
matching the selected device profile. MVM allocates it from the runtime pool,
replays VMGP drawing internally, and calls the driver from `vFlipScreen`. Clear
operations dirty the full frame; other operations accumulate the bounding dirty
rectangle of pixels whose RGB565 value actually changed.
