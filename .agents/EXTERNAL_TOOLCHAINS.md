# External Mophun Toolchains and References

Inventory date: 2026-07-19. This document describes the environment surrounding
the repository on the original host. These files are not repository
dependencies unless a specific investigation says otherwise.

## Portable layout convention

On the inventoried host, the MVM checkout is `C:\mophun\MY\MVM` and the legacy
SDK is installed at `C:\mophun`. When restoring on another machine, preserve the
logical roles rather than these drive-letter paths:

```text
<mophun-root>/
  bin/                         official SDK tools and emulator
  doc/                         official SDK/emulator documentation
  lib/, pip/, share/           PIP compiler runtime/toolchain
  tutorials/                   official example source and built .mpn files
  MY/
    MVM/                       this repository
    Install/                   recovery archives and community collection
    mophun_2.5.4_tuxality_A2/  modified reference emulator
    Refs/
      MoRePhun/                early C++ emulator reference
      nofun/                   C#/Unity emulator reference
    fonts/, fontsPic/          font research artifacts
    videos&logs/               historical comparison evidence
```

Use environment/configurable paths in new automation. Do not add a dependency
on `C:\mophun` or another absolute checkout path.

## Official Mophun SDK 2.5

Installed root: `C:\mophun`. The SDK readme identifies it as Synergenix
Interactive AB's Mophun SDK, copyright 2004. The emulator release notes identify
the bundled official emulator as version 2.5.4.

Primary tools under `bin/`:

- `pip-gcc.exe`, `pip-g++.exe` - legacy GCC drivers targeting the PIP VM.
- `pip-as.exe`, `pip-ld.exe`, `pip-ar.exe`, `pip-objdump.exe`,
  `pip-readelf.exe`, `pip-gdb.exe` - matching binutils/debugger suite.
- `morc.exe` - Mophun resource compiler; tutorial resource descriptions are
  compiled to linkable objects with it.
- `mophun.exe` - official PC SDK emulator 2.5.4.
- `CertTest.exe` and `DataCertificateTestSuite.cmd` - data-certificate tests.
- `cmdMopack.exe` - Mopack/package-related command-line utility.

The target headers live under `pip/include/`. Important canonical contract
sources are `vmgp.h`, `vmgp3d.h`, `vmgpcaps.h`, `vmgpsonyericsson.h`,
`vstream.h`, `vsound.h`, `vhttp.h`, `datacert.h`, and `datacertlib.h`.
Libraries/import descriptions include `pip/lib/libc.a`, `libmocca.a`,
`libmophun.exp`, and `libdatacert.exp`.

Official documentation under `doc/sdk/`:

- `MophunAPIReferenceGuide.pdf` / `.chm` - canonical guest API semantics.
- `MophunAsmRef.pdf` - PIP assembly/instruction reference.
- `MophunProgrammingGuidelines.pdf` - official programming guidance.
- `morc.pdf` - resource compiler documentation.
- `gdb.pdf` - debugger documentation.
- `RELEASE_NOTES.txt` - SDK history and compatibility notes.

`doc/emulator/emulator.chm` and `doc/emulator/RELEASE_NOTES.txt` document the
official emulator and profile history.

The official `tutorials/Source/` tree is especially valuable as executable API
contract evidence. It covers 2D graphics, sprites/tilemaps, fonts, sound,
streams/files, compression, timing, capabilities, tasks, network/HTTP,
Bluetooth/IrDA, certificates, and 3D. Matching built modules are in
`tutorials/RuntimeModules/`. Typical official build flow is:

```text
morc resource-description
pip-gcc -c ... source.c
pip-gcc -o game.mpn ... -mstack=<bytes> -mdata=<bytes> -ldatacert -s
```

For MVM behavior questions, prefer evidence in this order: official API/ASM
documentation, official tutorial source, observed official emulator behavior,
then community implementations. Old emulator behavior is still not proof of
real handset timing or system-font pixels; the SDK release notes explicitly
state that some profiles have no timing.

### License boundary

The SDK readme says the SDK may not be redistributed except for its GPL-covered
GCC/binutils/GDB portions. Do not copy the installed SDK, PDFs, binaries, or
headers into this git repository. Keep only this inventory and independently
written findings here.

Local recovery archive (not tracked by MVM):

```text
MY/Install/mophunSDK2.5.7z
size:   21,983,459 bytes
SHA256: D96CDF11AA45F0AD58411AB22F121E8DEB1919FF3BB239FC280DD875E9EEAA37
```

## Tuxality-modified reference emulator

Installed at `C:\mophun\MY\mophun_2.5.4_tuxality_A2`. The community collection
describes this as Mophun-mod 2.5.4 Alpha 2: an unofficial modification of the
official 2.5.4 PC emulator by Tuxality. It adds on-the-fly decryption and
drag-and-drop handling; compressed MPN support remains incomplete.

Key files are `mophun.exe`, `motuxality.dll`, `mocomm.dll`, `moutil.dll`, and
`plugins/`. `moapi.dll.txt` is a local text/disassembly-style reference used to
research import behavior. Do not confuse the modified emulator with the
unmodified SDK copy at `C:\mophun\bin\mophun.exe`; their hashes differ.

```text
modified mophun.exe SHA256:
ADFA7FEA8057112B9CA0745D39F979B7CC7E5E1FF0496268CB0C375082DDE9D2

official mophun.exe SHA256:
0A15DE055E8C0296B8C5E4F87EA6021B3AB9AEE06E2DA04FED75674D0E96452F
```

The MVM repository's `Tools/ref_runner/` automation and reference manifests are
the repeatable bridge to this legacy emulator. Treat its video/log output as a
behavioral comparison baseline, not as canonical source semantics.

Local recovery archive:

```text
MY/Install/mophun_2.5.4_tuxality_A2.zip
size:   3,499,164 bytes
SHA256: 3836EAB5EE455003B152EF201F7B1EC6860B8859F477859D342F85BD8E797D6F
```

## MoRePhun reference project

Path: `C:\mophun\MY\Refs\MoRePhun`

```text
upstream: https://github.com/Luca1991/MoRePhun.git
commit:   4740eb3421c130d0c71078dd535a08a970969730
date:     2021-07-12
```

MoRePhun is a C++14 proof-of-concept emulator with VMGP loader, pool decoder,
registers, interpreter/opcodes, and a limited set of SDL-backed syscalls. Its
README explicitly says opcode/API coverage is small and the heap is missing.
Build metadata uses CMake >= 3.18, Conan, and SDL2 2.0.14 from the historical
`bincrafters/stable` package. Use it for hypotheses and naming cross-checks,
never as the primary correctness oracle.

The local checkout contains untracked CMake/.vscode artifacts; those are not
part of the recorded upstream revision.

## nofun reference project

Path: `C:\mophun\MY\Refs\nofun`

```text
local upstream: https://github.com/jagotu/nofun.git
commit:         ba74485764781889b5282c3f2b856bbcb4e25508
date:           2024-01-28
license:        Apache-2.0
```

The project README describes nofun as a C# Mophun emulator running in Unity. The
local checkout targets Unity `2023.3.0a18` and URP 17.0.1. It includes VM/loader
and memory code, module implementations for graphics, sprites, tilemaps, input,
sound, strings, system, tasks, text, and time, plus native `llvm-pip2` plugins
for Windows and Android. It also includes a module-call binding generator.

The README warns that 3D support is immature and encrypted/compressed games are
unsupported. The code is useful for syscall, loader, rendering, certificate,
and PIP implementation comparisons, but remains a community reference rather
than the official contract. Its README branding refers to RadratSoftworks while
this local checkout's git remote is the `jagotu/nofun` repository; record both
facts rather than assuming they are identical upstream histories.

The Unity package manifest contains several git-based dependencies, so a clean
build requires network/package restoration unless their caches are preserved.
The local checkout has a modified `.vscode/settings.json` unrelated to MVM.

`Assets/Resources/games.db.bytes` is an actual SQLite 3 database, not an opaque
Unity-only blob. It may contain useful game identity/configuration metadata and
can be queried with ordinary SQLite tooling during future corpus provenance or
profile research. Do not assume its records are canonical without inspecting
the schema and source that consumes them.

## Decryption and format research tools

Path: `C:\mophun\MY\Install\Decrypt tools`

- `MophunDecrypt.exe` / `MophunDecrypt_2.zip` - JaGoTu community decryptor;
  version 2 is described as adding decompression.
- `DateMophun-eng.exe` - certificate/game-date inspection helper.
- `decomp.py` - Python implementation of the observed `LZ` stream decoder.
- `src/` - C++ decryption research source. `Source.cpp` contains a tentative
  VMGP header, key loading, block decryption, and OpenSSL BIGNUM usage;
  `xtea.cpp` contains a 32-round 16-bit-adapted XTEA decrypt routine derived
  from AVR-Crypto-Lib.
- `MophunDecryptGUI.pyw` - community GUI wrapper.

The checked-in-local `MophunDecryptGUI.bat` contains a different developer's
absolute desktop paths and is not portable. Do not copy that pattern into MVM.
Decryption research is Phase 13 and should remain separate from VM correctness.

```text
MY/Install/Decrypt tools/MophunDecrypt_2.zip
size:   774,914 bytes
SHA256: 407A5F65C8FB83C32ED64899E9DBBDC32C9F4FDF5A5171774F34DFBD0CE8CFC1
```

## Community archive and other evidence

`C:\mophun\MY\Install` is a local checkout of the community preservation repo:

```text
upstream: https://github.com/ptnnx/Mophun.git
commit:   cfe8622b4ab403a79d72a3c279ed34bf869fbd75
date:     2022-09-11
```

It has many local/untracked additions: SDK/emulator/SDL archives, decryptors,
games, resources, screenshots, phone lists, and video tutorials. Therefore the
commit alone does not reconstruct this host's collection; the hashes above
identify the most important recovery artifacts.

Other local evidence:

- `MY/fonts/` contains Andale fonts and a Sony Ericsson J220i TTF.
- `MY/fontsPic/` contains a candidate Sony Ericsson T230 bitmap atlas/header.
  It is research material, not yet a reference-matched normal system font.
- `MY/videos&logs/` contains historical visual/reference evidence. Current
  reproducible run artifacts should instead be produced under MVM's `Runs/`.
- `MY/Refs/T300input_limitations.doc` is historical device/input material.

## Legacy research bundle under MY/etc

`C:\mophun\MY\etc\Mophun` was discovered in the broader `MY` inventory. It is
an ordinary directory created on 2026-07-19, not a junction. Its files match the
corresponding tree at `MY/Install/Games/1/Mophun` by hash, so it is a convenient
working copy rather than an independent provenance source.

Despite being duplicated, the contents are valuable:

- `instructionset.txt` contains early reverse-engineered PIP validation flags,
  pool-instantiation notes, and selected opcode layouts.
- `opcodes2.txt` enumerates opcodes `0x00` through `0x73`, including the five
  syscall forms at `0x67` through `0x6B`.
- `vmgp.txt` records an early 40-byte VMGP header interpretation and tentative
  module layout.
- `API Reference/` preserves API guides 1.52 and 1.72, programming guidelines
  1.51, the ASM reference, resource-compiler guide, certification-process
  documents 1.22/1.28, game requirements/submission documents, a white paper,
  and T300 input limitations.
- `pcemu/1.4.3` and `pcemu/2.0.2` preserve earlier official emulator binaries,
  profile DLLs, skins, release notes, and the 2.0.2 emulator help file.
- `codebase/` contains historical CAB/ActiveX emulator delivery artifacts.
- `ppcemu/` and `symbemu/` preserve Windows Mobile and Symbian runtime/launcher
  artifacts.

The early official emulator versions are useful for differential testing when a
behavior changed between profiles/releases. They should not replace 2.5.4 as
the normal reference baseline. The unofficial text files are leads to verify
against `MophunAsmRef.pdf`, SDK headers, binaries, and observed behavior.

## External game and sidecar reservoir

The collection outside MVM contains 1,160 `.mpn` files and 12 `.mpc` files.
Content hashing gives 567 unique MPN images and 12 unique MPC sidecars. The MVM
repository currently contains 41 unique MPN/MPC hashes; only 10 unique external
items overlap it. Therefore the external collection contains 569 unique items
not present in MVM, including 557 MPN images and all 12 counted external
sidecars.

This is a large future compatibility reservoir, not a ready-to-run corpus:

- 894 physical files begin with the plain `VMGP` magic, but this count includes
  duplicates from scraped mirrors.
- Many non-VMGP `.mpn` files are encrypted payloads, extracted resource chunks,
  or incorrectly saved HTML pages; for example, 60 files beginning with
  `<!doctype` are scrape artifacts and must be rejected during corpus import.
- Several of the largest absent images are 3D/UIQ/Symbian titles such as
  Martial Arts 3D, The Da Vinci Code, Exile, Carmageddon 3D, Golf Pro Contest,
  Lock'n'Load, Rally Pro Contest, and Joe's Treasure Quest. These exercise a
  substantially broader target than the current T310/T610 2D baseline.
- The archive contains alternate versions/variants of existing 2D titles and
  many games absent from the present corpus, useful for loader/import/opcode
  discovery after deduplication and profile classification.
- The 12 external `.mpc` files include two distinct T300/T610-style sets of
  `VRally_multipack` plus `VRally_extrapack1` through `4`, and `Chars.mpc` /
  `Quest1.mpc`. They are valuable for sidecar-name mapping and external-resource
  behavior.

Before promoting any item into `MVM/mpn` or a manifest:

1. Deduplicate by SHA256, not by filename.
2. Validate the magic/container and distinguish full images from extracted
   chunks and HTML errors.
3. Record original mirror/archive provenance and whether the image is encrypted,
   compressed, decrypted, or modified.
4. Determine device family/profile and 2D versus 3D expectations.
5. Preserve matching `.mpc` and other sidecars beside the selected image.
6. Use a disposable copy for runs because writable resource persistence may
   modify an image in place.

`MY/Install/Games resources/README.md` is a provenance index for preserved and
archived download sites. `MY/Install/List of Mophun supported phones/README.md`
is a useful community device catalog, but profile claims should be checked
against SDK capability headers and official emulator profile DLLs. The small
`Mophun 3D database/README.md` provides historical context, not a machine-readable
compatibility database.

## Desktop MVM build dependency

MVM itself is built with a modern host MinGW toolchain, currently assumed at
`C:\mingw64`, not with `pip-gcc`. The vendored SDL tree inside MVM is version
2.32.6. A matching recovery archive exists outside the repo:

```text
MY/Install/SDL2-devel-2.32.6-mingw.zip
size:   14,266,273 bytes
SHA256: 5A58D900C3A8312EA8266230DBF44DF58364538A1FC1709479ABF0A32C54B36C
```

Keep these compiler roles distinct:

- host MinGW GCC builds `MVM.exe`;
- legacy `pip-gcc` builds guest `.mpn` programs for the Mophun VM;
- Unity/C# and native `llvm-pip2` belong to the nofun reference project;
- CMake/Conan/C++14 belong to the MoRePhun reference project.

## Restoration checklist

1. Clone/copy the MVM repository; its own SDL2 dependency is already present.
2. Install a modern MinGW toolchain and make the MVM build path configurable or
   recreate the current `C:\mingw64` convention.
3. If official SDK research is needed, restore `mophunSDK2.5.7z` outside the git
   repository and verify its SHA256. Respect the SDK license.
4. Restore the Tuxality emulator archive outside the repository only when
   reference-runner comparisons are needed.
5. Clone reference repos at the recorded commits; do not rely on their local
   generated files.
6. Preserve decrypt/font/video artifacts separately if they are relevant to the
   investigation; they are evidence sets, not ordinary build dependencies.
7. Update configured paths rather than embedding the original drive layout in
   source or manifests.
