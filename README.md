# MVM - Mophun Virtual Machine

MVM is a portable C implementation of the Mophun VMGP runtime. The repository
contains the VM component, a Windows/SDL2 diagnostic runner, decrypted-game
corpus tooling, and platform integration examples.

The VM is designed around opaque host-owned storage, bounded execution, a
static runtime pool, source-backed game images, and explicit platform callbacks.
The current desktop runner supports the Sony Ericsson T310 and T610 profiles.

## Start here

- Component architecture and integration: [`OpenMophun/README.md`](OpenMophun/README.md)
- Current roadmap and known defects: [`ROADMAP.md`](ROADMAP.md)
- Corpus runner and scripted input: [`Tools/corpus/README.md`](Tools/corpus/README.md)
- Persistent agent/workspace context: [`AGENTS.md`](AGENTS.md)
- Surrounding SDK and reference inventory:
  [`.agents/EXTERNAL_TOOLCHAINS.md`](.agents/EXTERNAL_TOOLCHAINS.md)

OpenMophun is tracked as a Git submodule. Clone this repository with:

```bat
git clone --recurse-submodules <repository-url>
```

For an existing clone, initialize or update the library with:

```bat
git submodule update --init --recursive
```

## Desktop build

The current wrapper targets Windows with MinGW GCC/GNU Make and the bundled
SDL2 MinGW package. It presently expects MinGW under `C:\mingw64`.

```bat
build.bat
run.bat path\to\decrypted_game.mpn SE_T610
```

The direct runner accepts execution limits, scripted input, recording, and a
fixed date:

```text
MVM.exe <decrypted.mpn> [profile_name] [max_steps] [max_logged_calls]
  [--duration-ms N] [--input-script PATH] [--record-dir DIR]
  [--fixed-date-time YYYY-MM-DDTHH:MM:SS]
```

Run a small automated corpus probe with:

```bat
corpus-run.bat -Manifest Tools\corpus\smoke-manifest.json -OutRoot Runs\Smoke
```

`Build/` and `Runs/` are generated artifacts. The core VM does not depend on
SDL or Windows; those dependencies belong to the bundled desktop host.

## STM32F407VET6 resource estimate

The current OpenMophun library was compiled with Arm GNU Toolchain 15.3.1 for
Cortex-M4 Thumb using `-Os`, function/data sections, stack-usage reporting, and
`-Werror`.

| Resource | Measurement |
| --- | ---: |
| Library code and constants | 48,857 bytes |
| Library initialized data | 0 bytes |
| Library object BSS | 32 bytes |
| Opaque VM instance | 6,740 bytes |
| Largest static function frame | 592 bytes |
| Estimated T310 working RAM | 55.2-105.6 KiB |
| Estimated T610 working RAM | 98.5-123.6 KiB |

The measured library fits the STM32F407VET6's 512 KiB Flash and 192 KiB
SRAM/CCM budget. The worst projected game leaves approximately 68 KiB for the
BSP, stacks, filesystem state, and device buffers.

The final firmware must account for memory-bank placement: DMA-visible buffers
cannot reside in CCM, while the opaque VM instance and CPU-only state can.
Linker-map validation, interrupt-stack measurements, and real BSP integration
are reserved for the hardware bring-up phase.
