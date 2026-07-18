# MVM - Mophun Virtual Machine

MVM is a portable C implementation of the Mophun VMGP runtime. The repository
contains the VM component, a Windows/SDL2 diagnostic runner, decrypted-game
corpus tooling, and platform integration examples.

The VM is designed around opaque host-owned storage, bounded execution, a
static runtime pool, source-backed game images, and explicit platform callbacks.
The current desktop runner supports the Sony Ericsson T310 and T610 profiles.

## Start here

- Component architecture and integration: [`MVM/README.md`](MVM/README.md)
- Current roadmap and known defects: [`MVM/ROADMAP.md`](MVM/ROADMAP.md)
- Component coding rules: [`MVM/STYLE_GUIDE.md`](MVM/STYLE_GUIDE.md)
- Corpus runner and scripted input: [`Tools/corpus/README.md`](Tools/corpus/README.md)
- Persistent agent/workspace context: [`AGENTS.md`](AGENTS.md)
- Surrounding SDK and reference inventory:
  [`.agents/EXTERNAL_TOOLCHAINS.md`](.agents/EXTERNAL_TOOLCHAINS.md)

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
