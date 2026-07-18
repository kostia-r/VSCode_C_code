# MVM Project Knowledge

This note complements the root `AGENTS.md`. It records durable project context,
not session history. Keep it usable after copying or cloning this directory to
another machine.

## Mental model

```text
decrypted .mpn / image source
            |
            v
       VMGP loader
            |
            v
 guest RAM + pool/resource metadata
            |
            v
       PIP executor
            |
       import dispatch
        /          \
 VM-owned runtime  platform-facing request
                         |
                         v
                 configured host backend
```

A VMGP image contains guest code/data, a constant pool (including import names),
and resources. PIP/PIP2 is the guest instruction set. Calls from guest code are
resolved by import name and handled either by the VM runtime or the host-facing
platform path.

## Public integration path

1. Allocate storage of `MVM_GetStorageSize()` with alignment from
   `MVM_GetStorageAlign()`.
2. Obtain the opaque VM object with `MVM_GetVmFromStorage()`.
3. Optionally query image memory requirements.
4. Initialize from a memory buffer or `MpnImageSource_t`; source-backed access is
   preferred for constrained systems.
5. Select a built-in/profile-catalog entry such as `SE_T310` or `SE_T610`.
6. Drive bounded execution from the host loop/task.
7. Consume display, input, audio, log, and structured-event activity through
   configured callbacks.
8. Free/close cleanly so dirty writable resource overlays can flush.

Today, bundled bindings live in `MVM/Config/MVM_Lcfg.c`. Phase 12 is intended to
let a parent project supply external config without modifying the component and
to support static-library packaging.

## Runtime and diagnostics

The runner is deliberately deterministic where practical:

- `--duration-ms` caps wall-clock execution.
- `--input-script` injects timed Mophun button operations.
- `--record-dir` captures diagnostic media/artifacts.
- `--fixed-date-time` stabilizes certificate-sensitive games.
- `max_steps` and `max_logged_calls` are host caps; hitting one does not prove
  that the guest exited.

Prefer structured terminal events and the final VM state over video duration or
process exit code. Useful failure classes include missing import, invalid opcode,
guest-memory out of bounds, fatal VM state, unsupported device/profile, license
or certificate outcome, and missing sidecar.

## Corpus baseline

The corpus manifest and repeatable runner are established. The 2026-07-18 T310
and T610 reference-input batches completed without runner timeout, invalid
opcode, missing syscall, or memory-OOB failures. Machine-readable exceptional
outcomes live in `Tools/corpus/classifications.json`.

Known classification facts:

- Missing `.mpc` sidecars are informational by themselves; the reference
  emulator also tolerates several absent sidecars.
- `IcebloxPlus610`, `lunar`, and `Huntsman` are classified as incomplete
  artifact sets in the current corpus.
- `prhstrkmn_T610` follows an unsupported-device/profile guest path.
- Accepted data-certificate calls use SDK-style `0` success semantics.

Known parity work remains in system fonts, renderer composition for selected
games, rally visuals, and timing/cadence. These are not automatically VM-core
failures.

## Portability boundary

Portable and intended to embed:

- everything under `MVM/` except the bundled config's concrete host choices;
- public headers and `vm.mk`;
- bounded execution, static storage, image-source and platform abstractions.

Desktop/test-only:

- `Src/` SDL runner;
- corpus/reference automation;
- recording and media muxing;
- MinGW and Windows batch wrappers.

Host-local assumptions that a new machine may need to adapt:

- MinGW is currently expected at `C:\mingw64` by the root Makefile/wrappers;
- optional corpus video encoding may require `ffmpeg` on `PATH`;
- legacy reference-runner automation has its own external emulator/runtime
  prerequisites;
- paths under historical roadmap entries may point to old external evidence and
  are not required for building the repository.

Do not encode a checkout's absolute path into new source, manifests, or docs.
Generated `Build/` and `Runs/` content is intentionally ignored and need not be
copied to reproduce the source workspace.

## Where to look first

- Project/API overview: `MVM/README.md`
- Current plan, defects, and baselines: `MVM/ROADMAP.md`
- C conventions: `MVM/STYLE_GUIDE.md`
- Public API: `MVM/inc/MVM.h`
- Source list for integrators: `MVM/vm.mk`
- Import coverage/contracts: `MVM/runtime/src/MVM_Imports.c`
- Bundled integration: `MVM/Config/MVM_Cfg.h`, `MVM/Config/MVM_Lcfg.c`
- Desktop CLI: `Src/main.c`
- Host execution loop: `Src/VmRunner.c`
- SDL backend: `Src/SdlBackend.c`
- Corpus operation: `Tools/corpus/README.md`
- External SDK/reference inventory: `.agents/EXTERNAL_TOOLCHAINS.md`

