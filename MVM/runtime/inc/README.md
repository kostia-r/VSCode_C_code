# Runtime Internal Headers

Runtime declarations belong here when a syscall domain needs private shared
types. Current syscall implementations are split by domain under `runtime/src`:
dispatch, streams, caps, decompression, heap, strings, timing/random, and misc.
