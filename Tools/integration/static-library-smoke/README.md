# Static Library Integration Smoke

This check archives `MVM_LIBRARY_SRC` into `Runs/StaticLibrarySmoke/libmvm.a`.
It then compiles a parent executable from only:

- public headers under `MVM/inc`;
- the parent-owned `MVM_Cfg.h` and `MVM_Lcfg.c` from the external-config smoke;
- the completed `libmvm.a` artifact.

No VM implementation `.c` file is compiled directly into the parent
executable. Run from the repository root:

```bat
C:\mingw64\bin\mingw32-make.exe -f Tools\integration\static-library-smoke\Makefile run
```

The executable performs memory and source queries, initializes a real decrypted
MPN through the external config, runs bounded VM steps, and verifies framebuffer
flush callbacks.
