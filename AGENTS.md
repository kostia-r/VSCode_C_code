# MVM Workspace Instructions

This file is the persistent, machine-portable project context for coding agents.
Treat paths as relative to the repository root. Do not depend on the original
checkout location or on files outside this repository.

## Project identity

MVM is a C implementation of a Mophun VMGP virtual machine. Its immediate host
is a Windows/SDL2 diagnostic runner; the library is being shaped into a
deterministic, allocation-free component suitable for firmware and RTOS hosts.
The runner consumes **decrypted** `.mpn` images. Original-image decryption is a
separate future research phase.

The source of truth for scope and current work is `ROADMAP.md`. Phase 11
(corpus runner infrastructure) is closed. Phase 12 (external configuration and
submodule-friendly integration) is the next planned architecture milestone.
Do not silently implement later roadmap phases while fixing a focused defect.

## Repository map

- `OpenMophun/` - portable VM component.
  - `inc/` - complete public API (`MVM.h`, `MVM_Types.h`).
  - `src/` - flat private implementation and internal headers.
  - `OpenMophun.mk` - reusable source/include list for embedding builds.
- `Src/main.c` - the single bare-metal-style integration example and desktop entry point.
- `Src/` - Windows/SDL adapters, input scripts, recording, audio, and host loop.
- `Tools/corpus/` - manifests, classifications, and corpus documentation.
- `Tools/corpus-run.ps1` - batch corpus orchestrator; `corpus-run.bat` is its entry point.
- `Tools/ref_runner/` - automation for the legacy reference emulator.
- `mpn/` - local game corpus and associated sidecars.
- `SDL2-2.32.6/` - vendored SDL source plus the Windows MinGW distribution used by the runner.
- `Runs/` and `Build/` - generated/ignored artifacts; never treat them as source.

More detailed durable notes are in `.agents/PROJECT_KNOWLEDGE.md`.
The surrounding SDK, emulator, reference implementations, and recovery archives
are inventoried in `.agents/EXTERNAL_TOOLCHAINS.md`.

## Architecture invariants

- `MVM_Instance_t` is opaque. Hosts allocate aligned storage using
  `MVM_GetInstanceStorageSize()` / `MVM_GetInstanceStorageAlign()` and receive
  the descriptor from `MVM_Init()`.
- Guest pointers are offsets in guest RAM, never host pointers. Host callbacks
  must not retain translated guest buffers after a call returns.
- VM execution is bounded (`MVM_RunStep`, `MVM_RunSteps`, `MVM_RunForTime`) so a
  host task can yield.
- The VM library performs no hidden dynamic allocation. Guest RAM and decoded
  metadata are carved from the host/integration-owned runtime pool.
- Image access is source-backed. Resources should be streamed instead of copied
  wholesale into guest RAM.
- VM-owned imports stay in the runtime layer; hardware-facing behavior crosses
  the configured platform boundary.
- Platform callbacks should be non-blocking where documented. Asynchronous work
  such as audio should be queued or signalled.
- Important outcomes use structured events; do not make automation parse prose
  logs when an event can represent the state.
- Persistent writable resources use a dirty overlay and optional image-write
  callback. Full VM snapshots are not implemented.
- Preserve deterministic fixed-date support because old game certificates are
  date-sensitive.

## Build and run

The current desktop build expects Windows, GNU Make/MinGW GCC, and the bundled
SDL2 MinGW package. `Makefile` currently defaults to `C:/mingw64/bin`; this is a
known host-toolchain assumption, not a library requirement.

The legacy Mophun/PIP SDK toolchain is installed outside this repository under
`C:/mophun`. It is a research/reference input, not the compiler used to build
MVM itself. Consult `.agents/EXTERNAL_TOOLCHAINS.md` before using or relocating
it; the SDK license restricts redistribution.

Common commands from the repository root:

```bat
build.bat
run.bat path\to\game.mpn SE_T610 100000000 0 --fixed-date-time 2003-11-04T12:00:00
rebuild.bat
clean.bat
corpus-run.bat -Manifest Tools\corpus\smoke-manifest.json -OutRoot Runs\Smoke
```

Direct runner syntax:

```text
MVM.exe <decrypted.mpn> [profile_name] [max_steps] [max_logged_calls]
  [--duration-ms N] [--input-script PATH] [--record-dir DIR]
  [--fixed-date-time YYYY-MM-DDTHH:MM:SS]
```

Built-in profiles are `SE_T310` and `SE_T610`. The interactive key map and
manifest input format are documented in `Tools/corpus/README.md`.

## Validation expectations

- After a C change, build at minimum with `build.bat` (or the equivalent make
  command available on the host).
- For runner/runtime behavior, use the smallest relevant manifest or a focused
  direct run before considering a full corpus.
- A successful process exit alone is insufficient: inspect terminal VM state,
  structured events, missing syscalls, invalid opcodes, and memory faults.
- Do not reclassify known guest-controlled exits as VM crashes. Consult
  `Tools/corpus/classifications.json` and the latest baseline named in
  `ROADMAP.md`.
- Preserve before/after artifacts when they are needed to demonstrate a visual,
  audio, timing, or compatibility change.

## Coding and change policy

- Follow the component rules recorded in this file. In particular: two-space
  indentation, 120-column limit, braces on their own line, declarations at the
  start of a function, `MVM_` naming, prescribed file sections, and closing
  comments for functions/control blocks.
- Keep the parent-owned bounded execution loop explicit in `Src/main.c`, SDL
  behavior in `SdlBackend`, and VM semantics in the library.
- Avoid mixing broad formatting migrations with functional changes.
- Keep the portable library free of SDL, Win32, filesystem, and desktop-only
  assumptions.
- Preserve unrelated working-tree changes and generated local logs.
- Never edit game images as a side effect of a test unless persistence is the
  behavior under test. The desktop backend may open writable `.mpn` files and
  flush resource changes in place, so use a disposable copy when necessary.
- Update `ROADMAP.md` when a milestone or defect status materially changes;
  update this file only when durable architecture/workflow facts change.

## Known backlog themes

The current high-level backlog includes external config/static-library support,
an MCU-oriented driver contract, reference-matched system fonts, renderer parity
for several corpus titles, VM timing/cadence parity, exact certificate/date
policy, external persistent storage, original-image decryption, and a minimal
MCU port. See the roadmap for the authoritative details and acceptance gates.
